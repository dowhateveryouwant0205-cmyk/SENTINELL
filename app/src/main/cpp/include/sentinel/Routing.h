// Routing.h — Strategy Pattern: interchangeable routing algorithms
#pragma once
#include "CoreTypes.h"
#include <memory>
#include <functional>

namespace sentinel {

class Graph;

// Abstract routing interface
class RoutingStrategy {
public:
    virtual ~RoutingStrategy() = default;
    // Returns node-id path from start to goal, or empty if unreachable.
    virtual std::vector<int> findPath(const Graph& g, int start, int goal) const = 0;
    virtual std::string name() const = 0;
};

class DijkstraRouter : public RoutingStrategy {
public:
    std::vector<int> findPath(const Graph& g, int start, int goal) const override;
    std::string name() const override { return "Dijkstra"; }
};

class AStarRouter : public RoutingStrategy {
public:
    std::vector<int> findPath(const Graph& g, int start, int goal) const override;
    std::string name() const override { return "A*"; }
};

// Factors traffic density, road type and (optionally) signal preemption benefit.
class PriorityRouter : public RoutingStrategy {
public:
    explicit PriorityRouter(double trafficWeight = 1.6, double roadTypeWeight = 0.4)
        : trafficW_(trafficWeight), roadW_(roadTypeWeight) {}
    std::vector<int> findPath(const Graph& g, int start, int goal) const override;
    std::string name() const override { return "Priority-Weighted"; }
private:
    double trafficW_, roadW_;
};

class RouterFactory {
public:
    static std::shared_ptr<RoutingStrategy> create(RouterKind kind);
};

} // namespace sentinel
