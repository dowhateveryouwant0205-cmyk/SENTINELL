// SimulationEngine.cpp — Facade orchestrating map, fleet, dispatcher, signals
#include "sentinel/SimulationEngine.h"
#include "nlohmann/json.hpp"
#include <random>
#include <cmath>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

namespace sentinel {

SimulationEngine& SimulationEngine::instance() {
    static SimulationEngine engine;
    return engine;
}

bool SimulationEngine::loadMapJson(const std::string& jsonText) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    try {
        auto map = std::make_shared<CityMap>();
        map->loadFromJsonString(jsonText);
        city_ = map;
        graph_ = map->graph();
        signals_.buildFromGraph(graph_);
        signals_.attach(this); // Observer registration
        dispatcher_.setStrategy(RouterFactory::create(routerKind_));
        buildFleet();
        incidents_.clear();
        simMinute_ = 0;
        nextIncidentId_ = 1;
        return true;
    } catch (const std::exception&) {
        return false; // invalid map file -> graceful failure to Kotlin layer
    }
}

void SimulationEngine::buildFleet() {
    fleet_.clear();
    if (!graph_) return;
    auto ids = graph_->allNodeIds();
    if (ids.empty()) return;

    // Depot nodes: spread around the map perimeter of node ordering.
    const char* types[] = {"Ambulance", "Ambulance", "FireTruck", "PoliceCar", "PoliceCar", "HazmatUnit"};
    int vid = 1;
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        int depot = ids[(i * ids.size()) / 6 % ids.size()];
        auto v = VehicleFactory::create(types[i]); // Factory Pattern
        v->id = vid++;
        v->currentNode = depot;
        v->pos = graph_->position(depot);
        v->status = VehicleStatus::IDLE;
        fleet_.emplace_back(std::move(v));
    }
}

void SimulationEngine::start() { running_ = true; }
void SimulationEngine::pause() { running_ = false; }

void SimulationEngine::resetRun() {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    incidents_.clear();
    simMinute_ = 0;
    nextIncidentId_ = 1;
    buildFleet();
    signals_.buildFromGraph(graph_);
}

void SimulationEngine::setMode(SimMode m) { mode_ = static_cast<int>(m); }

void SimulationEngine::setRouter(RouterKind k) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    routerKind_ = k;
    dispatcher_.setStrategy(RouterFactory::create(k)); // Strategy swap at runtime
}

Metrics SimulationEngine::metrics() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Metrics m;
    m.simMinute = simMinute_;
    m.mode = mode();
    m.routerName = dispatcher_.strategy() ? dispatcher_.strategy()->name() : "None";
    m.signalChanges = signals_.changeCount();
    double sum = 0; int done = 0;
    for (auto& inc : incidents_) {
        if (inc->active) m.activeIncidents++; else m.resolvedIncidents++;
        if (inc->arrivalTime >= 0 && inc->dispatchTime >= 0) { sum += inc->arrivalTime - inc->dispatchTime; done++; }
    }
    if (done) m.avgResponseMin = sum / done;
    for (auto& v : fleet_) {
        if (v->status == VehicleStatus::EN_ROUTE) m.unitsDeployed++; else m.unitsAvailable++;
    }
    if (!incidents_.empty()) {
        auto& last = incidents_.back();
        if (last->arrivalTime >= 0 && last->dispatchTime >= 0)
            m.lastResponseMin = last->arrivalTime - last->dispatchTime;
    }
    return m;
}

std::vector<NodeInfo> SimulationEngine::nodes() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (!city_) return {};
    auto ns = city_->nodes();
    for (auto& n : ns) n.signal = signals_.stateOf(n.id);
    return ns;
}
std::vector<EdgeInfo> SimulationEngine::edges() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    return city_ ? city_->edges() : std::vector<EdgeInfo>{};
}
std::vector<VehicleInfo> SimulationEngine::vehicles() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<VehicleInfo> out;
    if (!graph_) return out;
    for (auto& v : fleet_) {
        VehicleInfo vi;
        vi.id = v->id; vi.type = v->typeName(); vi.priority = v->priorityLevel();
        vi.pos = v->interpolatedPosition(*graph_);
        vi.currentNode = v->currentNode;
        vi.targetNode = v->targetNode();
        vi.status = v->status;
        vi.speedKmh = v->maxSpeedKmh();
        vi.path = v->path;
        vi.incidentId = v->assignedIncident;
        vi.progress = v->progress;
        out.push_back(vi);
    }
    return out;
}
std::vector<IncidentInfo> SimulationEngine::incidents() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<IncidentInfo> out;
    for (auto& i : incidents_) {
        IncidentInfo ii;
        ii.id = i->id; ii.type = i->typeName(); ii.node = i->node; ii.severity = i->severity;
        ii.active = i->active; ii.assignedVehicle = i->assignedVehicle;
        if (i->arrivalTime >= 0 && i->dispatchTime >= 0) ii.responseTimeMin = i->arrivalTime - i->dispatchTime;
        out.push_back(ii);
    }
    return out;
}

double SimulationEngine::spawnIncident(const std::string& type, int node) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (!graph_) return -1;
    std::unique_ptr<Incident> inc;
    std::uniform_int_distribution<unsigned> raw(1, 0xFFFFFFFFu);
    unsigned draw = raw(rng_);
    if (type.empty()) {
        IncidentSpawner sp;
        inc = sp.spawn(*graph_, nextIncidentId_, draw);
    } else {
        if (type == "medical") inc = std::make_unique<MedicalIncident>();
        else if (type == "fire") inc = std::make_unique<FireIncident>();
        else if (type == "accident") inc = std::make_unique<AccidentIncident>();
        else inc = std::make_unique<HazmatIncident>();
        if (node < 0 || !graph_->hasNode(node)) {
            IncidentSpawner sp; auto probe = sp.spawn(*graph_, 0, draw);
            node = probe ? probe->node : graph_->allNodeIds().front();
        }
        inc->node = node;
        inc->id = nextIncidentId_;
        std::uniform_int_distribution<int> sev(1, 5);
        inc->severity = sev(rng_);
    }
    if (!inc) return -1;
    double id = inc->id;
    nextIncidentId_++;
    incidents_.push_back(std::move(inc));
    dispatchPendingLocked();
    return id;
}

void SimulationEngine::dispatchPendingLocked() {
    for (auto& inc : incidents_) {
        if (inc->active && inc->assignedVehicle < 0) {
            dispatcher_.dispatch(inc, fleet_, *graph_, simMinute_);
        }
    }
}

double SimulationEngine::effectiveSpeedKmh(const Vehicle& v, int a, int b) const {
    double base = v.maxSpeedKmh();
    auto neighbors = graph_->neighbors(a);
    for (auto& n : neighbors) if (n.to == b) { base *= (1.0 - 0.65 * n.traffic); break; }
    // Baseline mode: obey red lights at the upcoming intersection.
    bool sent = mode() == SimMode::SENTINEL;
    if (!sent && signals_.stateOf(b) == SignalState::RED && !v.stoppedAtRed) {
        base /= v.slowdownAtRed(); // full stop + restart penalty
    }
    return std::max(6.0, base);
}

void SimulationEngine::stepLocked(double dtSec) {
    if (!graph_) return;
    auto& cfg = ConfigManager::instance();
    double simDt = dtSec * cfg.simSpeedMultiplier;           // seconds in sim time
    simMinute_ += simDt / 60.0;

    city_->evolveTraffic(simDt, 0xC17Cu);                        // living congestion
    bool sent = mode() == SimMode::SENTINEL;

    // Signals: SENTINEL preempts corridors; baseline just cycles normally.
    if (!sent) signals_.releasePreempts();
    signals_.tick(simDt);

    // Vehicles move along paths.
    for (auto& v : fleet_) {
        if (v->status != VehicleStatus::EN_ROUTE || !v->hasRoute()) continue;
        double spd = effectiveSpeedKmh(*v, v->currentNode, v->nextNode);
        bool halted = (!sent && signals_.stateOf(v->nextNode) == SignalState::RED && v->progress > 0.85);
        double distKm = halted ? 0.0 : spd * (simDt / 3600.0);
        if (sent && v->canPreemptSignals()) {
            int idx = 0;
            for (size_t i = 0; i < v->path.size(); ++i) if (v->path[i] == v->currentNode) { idx = (int)i; break; }
            signals_.preemptCorridor(v->path, idx, cfg.corridorLookahead, v->priorityLevel());
        }
        bool arrived = v->advance(distKm, *graph_);
        if (arrived) {
            v->arrivalSimTime = simMinute_;
            for (auto& inc : incidents_) {
                if (inc->id == v->assignedIncident) {
                    inc->arrivalTime = simMinute_;
                    inc->active = false;
                }
            }
            v->status = VehicleStatus::ARRIVED;
            // After a short on-scene dwell handled implicitly: go idle at scene.
            v->status = VehicleStatus::IDLE;
            v->path.clear();
            v->assignedIncident = -1;
        }
    }

    dispatchPendingLocked();
}

void SimulationEngine::tick(double realDtSec) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (!running_.load() || !graph_) return;
    stepLocked(realDtSec);
}

void SimulationEngine::onSignalChanged(int, SignalState, SignalState) {
    // Observer hook: UI reads snapshots each frame; metrics counts changes centrally.
}

std::string SimulationEngine::mapJson() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    return city_ ? city_->saveToJsonString() : "{}";
}

// ---------- Headless deterministic run used by A/B comparison ----------
static double headlessRun(const std::string& mapJson, SimMode mode, RouterKind rk,
                          int incidentCount, unsigned seed, json& detail) {
    // Fresh subsystems per run (no singleton state).
    auto city = std::make_shared<CityMap>();
    city->loadFromJsonString(mapJson);
    auto g = city->graph();
    SignalController sig;
    sig.buildFromGraph(g);
    Dispatcher disp;
    disp.setStrategy(RouterFactory::create(rk));

    // Same fleet layout as engine.
    std::vector<std::shared_ptr<Vehicle>> fleet;
    auto ids = g->allNodeIds();
    const char* types[] = {"Ambulance", "Ambulance", "FireTruck", "PoliceCar", "PoliceCar", "HazmatUnit"};
    int vid = 1;
    for (size_t i = 0; i < 6 && i < ids.size(); ++i) {
        int depot = ids[(i * ids.size()) / 6 % ids.size()];
        auto v = VehicleFactory::create(types[i]);
        v->id = vid++;
        v->currentNode = depot;
        v->pos = g->position(depot);
        fleet.push_back(std::move(v));
    }

    auto& cfg = ConfigManager::instance();
    unsigned rng = seed;
    double simMinute = 0;
    double totalResp = 0;
    int resolved = 0;
    const double dt = 0.2; // seconds per step (scaled below)

    std::vector<std::shared_ptr<Incident>> pending;
    IncidentSpawner spawner;
    for (int k = 0; k < incidentCount; ++k) {
        rng ^= (rng << 13); rng ^= (rng >> 17); rng ^= (rng << 5);
        auto inc = spawner.spawn(*g, k + 1, rng);
        if (inc) { inc->dispatchTime = -1; pending.push_back(std::move(inc)); }
    }
    size_t spawned = 0;
    double nextSpawnMin = 0;

    int maxSteps = 200000;
    for (int step = 0; step < maxSteps && resolved < incidentCount; ++step) {
        double simDt = dt * cfg.simSpeedMultiplier;
        simMinute += simDt / 60.0;
        city->evolveTraffic(simDt, rng);
        sig.tick(simDt);

        // spawn incidents spaced over time, deterministically
        if (spawned < pending.size() && simMinute >= nextSpawnMin) {
            pending[spawned]->dispatchTime = -1; // set at assignment
            nextSpawnMin += 1.2;
            spawned++;
        }
        // dispatch
        for (size_t i = 0; i < spawned; ++i) {
            auto& inc = pending[i];
            if (inc->active && inc->assignedVehicle < 0) {
                auto cmd = disp.dispatch(inc, fleet, *g, simMinute);
                if (cmd) inc->dispatchTime = simMinute;
            }
        }
        // move
        bool sent = mode == SimMode::SENTINEL;
        for (auto& v : fleet) {
            if (v->status != VehicleStatus::EN_ROUTE || !v->hasRoute()) continue;
            double spd = v->maxSpeedKmh();
            auto nbrs = g->neighbors(v->currentNode);
            for (auto& n : nbrs) if (n.to == v->nextNode) { spd *= (1.0 - 0.65 * n.traffic); break; }
            bool halted = (!sent && sig.stateOf(v->nextNode) == SignalState::RED && v->progress > 0.85);
            if (sent && v->canPreemptSignals()) {
                int idx = 0;
                for (size_t j = 0; j < v->path.size(); ++j) if (v->path[j] == v->currentNode) { idx = (int)j; break; }
                sig.preemptCorridor(v->path, idx, cfg.corridorLookahead, v->priorityLevel());
            }
            if (halted) continue; // full stop until the light turns green
            if (v->advance(spd * (simDt / 3600.0), *g)) {
                for (size_t i = 0; i < spawned; ++i) {
                    auto& inc = pending[i];
                    if (inc->id == v->assignedIncident && inc->active) {
                        inc->arrivalTime = simMinute;
                        inc->active = false;
                        totalResp += inc->arrivalTime - inc->dispatchTime;
                        resolved++;
                        json rj;
                        rj["incident"] = inc->id;
                        rj["type"] = inc->typeName();
                        rj["vehicle"] = v->typeName();
                        rj["responseMin"] = std::round((inc->arrivalTime - inc->dispatchTime) * 100.0) / 100.0;
                        rj["routeLen"] = (int)v->path.size();
                        detail["runs"].push_back(rj);
                    }
                }
                v->status = VehicleStatus::IDLE;
                v->path.clear();
                v->assignedIncident = -1;
            }
        }
    }
    return resolved ? totalResp / resolved : -1.0;
}

void SimulationEngine::runAbComparison(int incidentCount) {
    std::string mapStr;
    RouterKind rk;
    {
        std::lock_guard<std::recursive_mutex> lk(mu_);
        if (!city_) return;
        mapStr = city_->saveToJsonString();
        rk = routerKind_;
    }
    json base = json::object(), sent = json::object();
    double tBase = headlessRun(mapStr, SimMode::BASELINE, rk, incidentCount, 987654321u, base);
    double tSent = headlessRun(mapStr, SimMode::SENTINEL, rk, incidentCount, 987654321u, sent);
    double improvement = (tBase > 0 && tSent > 0) ? (tBase - tSent) / tBase * 100.0 : 0.0;

    json rep;
    rep["baselineAvgMin"] = std::round(tBase * 100.0) / 100.0;
    rep["sentinelAvgMin"] = std::round(tSent * 100.0) / 100.0;
    rep["improvementPct"] = std::round(improvement * 10.0) / 10.0;
    rep["incidents"] = incidentCount;
    rep["baselineDetail"] = base;
    rep["sentinelDetail"] = sent;
    {
        std::lock_guard<std::recursive_mutex> lk(mu_);
        abReport_ = rep.dump();
        abDone_ = true;
    }
}

std::string SimulationEngine::reportJson() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    json rep;
    rep["simMinute"] = std::round(simMinute_ * 10.0) / 10.0;
    rep["mode"] = mode() == SimMode::SENTINEL ? "SENTINEL" : "BASELINE";
    rep["router"] = dispatcher_.strategy() ? dispatcher_.strategy()->name() : "None";
    rep["signalChanges"] = signals_.changeCount();
    json incArr = json::array();
    for (auto& i : incidents_) {
        json j;
        j["id"] = i->id; j["type"] = i->typeName(); j["node"] = i->node;
        j["severity"] = i->severity; j["active"] = i->active;
        if (i->arrivalTime >= 0 && i->dispatchTime >= 0)
            j["responseMin"] = std::round((i->arrivalTime - i->dispatchTime) * 100.0) / 100.0;
        incArr.push_back(j);
    }
    rep["incidents"] = incArr;
    json cmds = json::array();
    for (auto& c : dispatcher_.history()) cmds.push_back(c->describe());
    rep["dispatchLog"] = cmds;
    if (!abReport_.empty()) rep["abComparison"] = json::parse(abReport_);
    return rep.dump(2);
}

} // namespace sentinel
