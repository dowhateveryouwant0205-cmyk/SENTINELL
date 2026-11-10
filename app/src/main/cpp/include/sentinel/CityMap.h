// CityMap.h — Encapsulated facade over Graph with JSON load/save
#pragma once
#include "Graph.h"
#include <memory>

namespace sentinel {

class CityMap {
public:
    CityMap();
    explicit CityMap(std::shared_ptr<Graph> g);

    // Load from a JSON string (nlohmann). Throws std::runtime_error on bad input.
    void loadFromJsonString(const std::string& json);
    std::string saveToJsonString() const;

    std::shared_ptr<Graph> graph() const { return graph_; }

    int nodeCount() const;
    int edgeCount() const;
    std::vector<NodeInfo> nodes() const;
    std::vector<EdgeInfo> edges() const;

    // Randomly perturb traffic to simulate a living city.
    void evolveTraffic(double dtSec, unsigned seed);

private:
    std::shared_ptr<Graph> graph_;
};

} // namespace sentinel
