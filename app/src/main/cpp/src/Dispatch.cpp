// Dispatch.cpp — IncidentSpawner, DispatchCommand, Dispatcher
#include "sentinel/Dispatch.h"
#include "sentinel/Graph.h"
#include <random>
#include <algorithm>
#include <sstream>

namespace sentinel {

std::unique_ptr<Incident> IncidentSpawner::spawn(const Graph& g, int nextId, unsigned seed) {
    static thread_local std::mt19937 rng(seed);
    if (seed != 0) rng.seed(seed);
    auto ids = g.allNodeIds();
    if (ids.empty()) return nullptr;

    // Weighted pick: prefer congested intersections (busier roads => more incidents).
    std::uniform_real_distribution<double> u(0.0, 1.0);
    std::vector<std::pair<double, int>> weights;
    double total = 0;
    for (int id : ids) {
        double congestion = 0;
        int deg = 0;
        for (auto& e : g.neighbors(id)) { congestion += e.traffic; deg++; }
        double w = 0.2 + (deg ? congestion / deg : 0.3);
        total += w;
        weights.emplace_back(total, id);
    }
    double r = u(rng) * total;
    int node = weights.back().second;
    for (auto& [acc, id] : weights) if (r <= acc) { node = id; break; }

    static const char* types[] = {"medical", "fire", "accident", "hazmat"};
    std::uniform_int_distribution<int> t(0, 3);
    std::uniform_int_distribution<int> sev(1, 5);
    std::string type = types[t(rng)];

    std::unique_ptr<Incident> inc;
    if (type == "medical") inc = std::make_unique<MedicalIncident>();
    else if (type == "fire") inc = std::make_unique<FireIncident>();
    else if (type == "accident") inc = std::make_unique<AccidentIncident>();
    else inc = std::make_unique<HazmatIncident>();

    inc->id = nextId;
    inc->node = node;
    inc->severity = sev(rng);
    return inc;
}

// ---------- DispatchCommand ----------
DispatchCommand::DispatchCommand(std::shared_ptr<Vehicle> v, std::shared_ptr<Incident> inc,
                                 std::vector<int> path, VehicleStatus prevStatus,
                                 std::vector<int> prevPath, int prevIncident)
    : vehicle(std::move(v)), incident(std::move(inc)), newPath(std::move(path)),
      oldStatus(prevStatus), oldPath(std::move(prevPath)), oldIncident(prevIncident) {}

void DispatchCommand::execute() {
    vehicle->setPath(newPath);
    vehicle->status = VehicleStatus::EN_ROUTE;
    vehicle->assignedIncident = incident->id;
    incident->assignedVehicle = vehicle->id;
}

void DispatchCommand::undo() {
    vehicle->setPath(oldPath);
    if (oldPath.empty()) { vehicle->currentNode = vehicle->pos.x >= 0 ? vehicle->currentNode : vehicle->currentNode; }
    vehicle->status = oldStatus;
    vehicle->assignedIncident = oldIncident;
    incident->assignedVehicle = -1;
    incident->active = true;
}

std::string DispatchCommand::describe() const {
    std::ostringstream os;
    os << "Dispatch " << vehicle->typeName() << "#" << vehicle->id
       << " -> " << incident->typeName() << " #" << incident->id
       << " (" << newPath.size() << " nodes)";
    return os.str();
}

// ---------- Dispatcher ----------
std::shared_ptr<DispatchCommand> Dispatcher::dispatch(
    const std::shared_ptr<Incident>& inc,
    const std::vector<std::shared_ptr<Vehicle>>& fleet,
    const Graph& g, double simMinute) {

    if (!strategy_ || !inc || !inc->active) return nullptr;

    struct Candidate {
        std::shared_ptr<Vehicle> v;
        std::vector<int> path;
        double etaMin;
        bool preferred;
    };
    std::vector<Candidate> cands;

    for (auto& v : fleet) {
        if (!v || v->status == VehicleStatus::EN_ROUTE) continue;
        if (v->currentNode < 0 || !g.hasNode(v->currentNode)) continue;
        auto path = strategy_->findPath(g, v->currentNode, inc->node);
        if (path.empty()) continue; // unreachable node -> skip (exception-safe)
        double eta = 0;
        for (size_t i = 0; i + 1 < path.size(); ++i)
            eta += g.travelTimeMin(path[i], path[i + 1], v->maxSpeedKmh());
        bool pref = (v->typeName() == inc->preferredVehicle());
        cands.push_back({v, std::move(path), eta, pref});
    }
    if (cands.empty()) return nullptr;

    // Best-suited first, then nearest by ETA; higher-priority vehicles break ties.
    std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) {
        if (a.preferred != b.preferred) return a.preferred;
        if (std::abs(a.etaMin - b.etaMin) > 0.4) return a.etaMin < b.etaMin;
        return a.v->priorityLevel() > b.v->priorityLevel();
    });

    auto& best = cands.front();
    auto cmd = std::make_shared<DispatchCommand>(best.v, inc, best.path,
                                                 best.v->status, best.v->path, best.v->assignedIncident);
    cmd->execute();
    inc->dispatchTime = simMinute;
    history_.push_back(cmd);
    if (history_.size() > 64) history_.pop_front();
    return cmd;
}

bool Dispatcher::undoLast() {
    if (history_.empty()) return false;
    history_.back()->undo();
    history_.pop_back();
    return true;
}

} // namespace sentinel
