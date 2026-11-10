// CityMap.cpp — JSON loading/saving + living traffic evolution
#include "sentinel/CityMap.h"
#include "nlohmann/json.hpp"
#include <random>
#include <stdexcept>

using json = nlohmann::json;

namespace sentinel {

CityMap::CityMap() : graph_(std::make_shared<Graph>()) {}
CityMap::CityMap(std::shared_ptr<Graph> g) : graph_(std::move(g)) {}

void CityMap::loadFromJsonString(const std::string& text) {
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("Invalid map JSON: ") + e.what());
    }
    if (!j.contains("nodes") || !j["nodes"].is_array())
        throw std::runtime_error("Map JSON missing 'nodes' array");
    if (!j.contains("edges") || !j["edges"].is_array())
        throw std::runtime_error("Map JSON missing 'edges' array");

    auto g = std::make_shared<Graph>();
    for (auto& n : j["nodes"]) {
        std::string name = n.value("name", "Intersection");
        float x = n.value("x", 0.f), y = n.value("y", 0.f);
        g->addNode(name, x, y);
    }
    for (auto& e : j["edges"]) {
        int a = e.value("from", -1), b = e.value("to", -1);
        if (!g->hasNode(a) || !g->hasNode(b))
            throw std::runtime_error("Edge references unknown node id");
        double dist = e.value("distance", 1.0);
        std::string road = e.value("road", "Main St");
        std::string type = e.value("type", "arterial");
        double traffic = e.value("traffic", 0.3);
        g->addEdge(a, b, dist, road, type, traffic);
    }
    graph_ = g;
}

std::string CityMap::saveToJsonString() const {
    json j;
    j["nodes"] = json::array();
    for (int id : graph_->allNodeIds()) {
        auto n = graph_->node(id);
        j["nodes"].push_back({{"id", n.id}, {"name", n.name}, {"x", n.pos.x}, {"y", n.pos.y}});
    }
    j["edges"] = json::array();
    for (auto& e : graph_->allEdges()) {
        j["edges"].push_back({
            {"from", e.from}, {"to", e.to}, {"distance", e.distance},
            {"road", e.roadName}, {"type", e.roadType}, {"traffic", e.traffic}});
    }
    return j.dump(2);
}

void CityMap::evolveTraffic(double dtSec, unsigned seed) {
    // Gentle random walk of congestion so routes recalculate dynamically.
    static thread_local std::mt19937 rng(seed ^ 0x9E3779B9u);
    std::uniform_real_distribution<double> d(-1.0, 1.0);
    auto edges = graph_->allEdges();
    for (auto& e : edges) {
        double delta = d(rng) * 0.04 * dtSec;
        double t = e.traffic + delta;
        if (t < 0.05) t = 0.05;
        if (t > 0.98) t = 0.98;
        graph_->setTraffic(e.from, e.to, t);
    }
}

int CityMap::nodeCount() const { return graph_->nodeCount(); }
int CityMap::edgeCount() const { return graph_->edgeCount(); }

std::vector<NodeInfo> CityMap::nodes() const {
    std::vector<NodeInfo> out;
    for (int id : graph_->allNodeIds()) out.push_back(graph_->node(id));
    return out;
}
std::vector<EdgeInfo> CityMap::edges() const { return graph_->allEdges(); }

} // namespace sentinel
