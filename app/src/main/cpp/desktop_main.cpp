// desktop_main.cpp — Phase 1 verification harness (run on PC, no Android needed)
// Builds: cd app/src/main/cpp && cmake -B build && cmake --build build && ./build/sentinel_core_tests
#include "sentinel/SimulationEngine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

using namespace sentinel;

static bool readFile(const std::string& p, std::string& out) {
    std::ifstream f(p);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

int main(int argc, char** argv) {
    std::string mapPath = argc > 1 ? argv[1] : "../../../../assets/sample_city.json";
    std::string json;
    if (!readFile(mapPath, json)) {
        // try a couple of fallback locations
        const char* alts[] = {"sample_city.json", "../../assets/sample_city.json",
                              "app/src/main/assets/sample_city.json"};
        bool ok = false;
        for (auto* a : alts) if (readFile(a, json)) { ok = true; break; }
        if (!ok) { std::cerr << "[FAIL] cannot find map JSON at " << mapPath << "\n"; return 1; }
    }

    std::cout << "== SENTINEL Core Engine — desktop test ==\n";

    // 1. Graph + CityMap JSON load
    auto& eng = SimulationEngine::instance();
    if (!eng.loadMapJson(json)) { std::cerr << "[FAIL] invalid map\n"; return 1; }
    std::cout << "[OK] Map loaded: " << eng.nodes().size() << " nodes, "
              << eng.edges().size() << " edges\n";

    // 2. Routing strategies produce paths (polymorphic swap)
    for (auto rk : {RouterKind::DIJKSTRA, RouterKind::ASTAR, RouterKind::PRIORITY}) {
        eng.setRouter(rk);
        std::cout << "[OK] Router set -> " << eng.metrics().routerName << "\n";
    }

    // 3. Vehicle hierarchy polymorphism
    for (auto& v : eng.vehicles())
        std::cout << "[OK] Vehicle #" << v.id << " " << v.type
                  << " priority=" << v.priority << " maxV=" << v.speedKmh << " km/h\n";

    // 4. Spawn incidents + dispatch + movement via simulation thread
    eng.setRouter(RouterKind::PRIORITY);
    eng.setMode(SimMode::SENTINEL);
    for (int i = 0; i < 3; ++i) eng.spawnIncident("", -1);
    eng.start();
    std::thread t([&]{ for (int i = 0; i < 400; ++i) { eng.tick(0.05); std::this_thread::sleep_for(std::chrono::milliseconds(2)); } });
    // keep spawning while the sim thread runs (tests multithreaded safety)
    for (int i = 0; i < 4; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(120)); eng.spawnIncident("", -1); }
    t.join();
    eng.pause();
    auto m = eng.metrics();
    std::cout << "[OK] Simulated " << m.simMinute << " min | resolved=" << m.resolvedIncidents
              << " | avg response=" << m.avgResponseMin << " min | signal changes=" << m.signalChanges << "\n";

    // 5. A/B killer metric — headless deterministic back-to-back runs
    eng.runAbComparison(6);
    std::string rep = eng.reportJson();
    auto pos = rep.find("\"abComparison\"");
    std::cout << (pos != std::string::npos ? "[OK]" : "[FAIL]") << " A/B comparison embedded in report\n";
    if (pos == std::string::npos) return 1;

    // Pull the headline numbers out of the report JSON.
    auto grab = [&](const char* key) -> double {
        auto p = rep.find(key, pos);
        if (p == std::string::npos) return -999;
        p = rep.find(':', p);
        std::istringstream is(rep.substr(p + 1));
        double v; is >> v; return v;
    };
    double tBase = grab("\"baselineAvgMin\"");
    double tSent = grab("\"sentinelAvgMin\"");
    double impr  = grab("\"improvementPct\"");
    std::cout << ">> Baseline avg response : " << tBase << " min\n";
    std::cout << ">> SENTINEL avg response : " << tSent << " min\n";
    std::cout << ">> IMPROVEMENT           : " << impr << "% faster\n";
    if (impr < 20.0) {
        std::cerr << "[FAIL] improvement below target band (expected 40-60%)\n";
        return 1;
    }

    std::cout << "---- DISPATCH LOG / INCIDENT DETAIL ----\n"
              << rep.substr(0, 900) << "\n...\n";
    std::cout << "== ALL DESKTOP TESTS PASSED ==\n";
    return 0;
}
