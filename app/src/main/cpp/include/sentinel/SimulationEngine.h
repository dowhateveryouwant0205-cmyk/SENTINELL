// SimulationEngine.h — Facade: single entry point for JNI / desktop tests
#pragma once
#include "CityMap.h"
#include "Vehicle.h"
#include "Dispatch.h"
#include "Signals.h"
#include "Routing.h"
#include "ConfigManager.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <random>

namespace sentinel {

struct Metrics {
    double simMinute = 0;
    int activeIncidents = 0;
    int resolvedIncidents = 0;
    int unitsDeployed = 0;
    int unitsAvailable = 0;
    double avgResponseMin = -1.0;      // live average (all completed)
    double lastResponseMin = -1.0;
    long signalChanges = 0;
    SimMode mode = SimMode::SENTINEL;
    std::string routerName = "Priority-Weighted";
};

class SimulationEngine : public SignalObserver {
public:
    static SimulationEngine& instance();

    // ---- lifecycle ----
    bool loadMapJson(const std::string& json);           // parse + build fleet/signals
    void start();
    void pause();
    void resetRun();                                     // clear incidents/vehicles, keep map
    bool isRunning() const { return running_.load(); }

    // ---- controls ----
    void setMode(SimMode m);
    SimMode mode() const { return static_cast<SimMode>(mode_.load()); }
    void setRouter(RouterKind k);
    RouterKind routerKind() const { return routerKind_; }
    double spawnIncident(const std::string& type /* "" = random */, int node /* -1 = random */);
    void tick(double realDtSec);                         // manual step (used by UI loop)

    // ---- queries (snapshot copies, thread-safe) ----
    std::vector<NodeInfo> nodes() const;
    std::vector<EdgeInfo> edges() const;
    std::vector<VehicleInfo> vehicles() const;
    std::vector<IncidentInfo> incidents() const;
    Metrics metrics() const;
    std::string reportJson() const;                      // full run report incl. A/B
    std::string mapJson() const;

    // ---- A/B killer metric ----
    void runAbComparison(int incidentCount = 4);         // headless deterministic dual-run
    bool hasAbResult() const { return abDone_; }

    // Observer hook (green corridor events feed metrics)
    void onSignalChanged(int nodeId, SignalState oldS, SignalState newS) override;

private:
    SimulationEngine() = default;
    void buildFleet();
    void stepLocked(double dtSec);                       // one simulation step (map+fleet+signals)
    void dispatchPendingLocked();
    double effectiveSpeedKmh(const Vehicle& v, int a, int b) const;

    std::shared_ptr<CityMap> city_;
    std::shared_ptr<Graph> graph_;
    SignalController signals_;
    Dispatcher dispatcher_;
    std::vector<std::shared_ptr<Vehicle>> fleet_;
    std::vector<std::shared_ptr<Incident>> incidents_;
    RouterKind routerKind_ = RouterKind::PRIORITY;

    mutable std::recursive_mutex mu_;
    std::atomic<bool> running_{false};
    std::atomic<int> mode_{static_cast<int>(SimMode::SENTINEL)};
    double simMinute_ = 0;
    std::mt19937 rng_{12345u};
    int nextIncidentId_ = 1;
    bool abDone_ = false;
    std::string abReport_;
};

} // namespace sentinel
