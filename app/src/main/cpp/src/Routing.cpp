// Routing.cpp — Dijkstra, A*, Priority-Weighted (Strategy Pattern)
#include "sentinel/Routing.h"
#include "sentinel/Graph.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <cmath>

namespace sentinel {

namespace {
// Edge cost for plain shortest-path: travel time at nominal 60 km/h.
double baseCost(const Graph& g, const EdgeInfo& e) {
    return g.travelTimeMin(e.from, e.to, 60.0);
}
} // namespace

std::vector<int> DijkstraRouter::findPath(const Graph& g, int start, int goal) const {
    if (start == goal) return {start};
    if (!g.hasNode(start) || !g.hasNode(goal)) return {};

    std::unordered_map<int, double> dist;
    std::unordered_map<int, int> prev;
    using Q = std::pair<double, int>;
    std::priority_queue<Q, std::vector<Q>, std::greater<Q>> pq;
    dist[start] = 0.0;
    pq.push({0.0, start});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (u == goal) break;
        if (d > dist[u]) continue; // stale entry
        for (auto& e : g.neighbors(u)) {
            double w = baseCost(g, e);
            double nd = d + w;
            if (!dist.count(e.to) || nd < dist[e.to]) {
                dist[e.to] = nd;
                prev[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }
    if (!dist.count(goal)) return {}; // unreachable -> exception handled upstream
    std::vector<int> path;
    for (int c = goal; c != start; c = prev[c]) {
        path.push_back(c);
        if (!prev.count(c)) return {};
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<int> AStarRouter::findPath(const Graph& g, int start, int goal) const {
    if (start == goal) return {start};
    if (!g.hasNode(start) || !g.hasNode(goal)) return {};

    auto heuristic = [&](int n) {
        // Euclidean distance scaled to minutes at best-case speed.
        return g.euclidean(n, goal) / 60.0 * 60.0 / 70.0;
    };

    std::unordered_map<int, double> gScore;
    std::unordered_map<int, int> prev;
    std::unordered_set<int> closed;
    using Q = std::pair<double, int>;
    std::priority_queue<Q, std::vector<Q>, std::greater<Q>> open;

    gScore[start] = 0.0;
    open.push({heuristic(start), start});

    while (!open.empty()) {
        auto [f, u] = open.top(); open.pop();
        if (u == goal) {
            std::vector<int> path;
            for (int c = goal; c != start; c = prev[c]) { path.push_back(c); if (!prev.count(c)) return {}; }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }
        if (closed.count(u)) continue;
        closed.insert(u);
        for (auto& e : g.neighbors(u)) {
            if (closed.count(e.to)) continue;
            double tentative = gScore[u] + baseCost(g, e);
            if (!gScore.count(e.to) || tentative < gScore[e.to]) {
                gScore[e.to] = tentative;
                prev[e.to] = u;
                open.push({tentative + heuristic(e.to), e.to});
            }
        }
    }
    return {};
}

std::vector<int> PriorityRouter::findPath(const Graph& g, int start, int goal) const {
    if (start == goal) return {start};
    if (!g.hasNode(start) || !g.hasNode(goal)) return {};

    // Cost = travel time * (1 + trafficW_*density) * road-type penalty, A*-guided.
    auto roadPenalty = [](const std::string& t) {
        if (t == "highway") return 0.85;   // faster class of road preferred
        if (t == "collector") return 1.10;
        return 1.0;                          // arterial
    };
    auto cost = [&](const EdgeInfo& e) {
        double tt = g.travelTimeMin(e.from, e.to, 60.0);
        return tt * (1.0 + trafficW_ * e.traffic) * roadPenalty(e.roadType);
    };
    auto heuristic = [&](int n) {
        return g.euclidean(n, goal) / 70.0 * 60.0 * 0.5; // optimistic
    };

    std::unordered_map<int, double> gScore;
    std::unordered_map<int, int> prev;
    std::unordered_set<int> closed;
    using Q = std::pair<double, int>;
    std::priority_queue<Q, std::vector<Q>, std::greater<Q>> open;
    gScore[start] = 0.0;
    open.push({heuristic(start), start});

    while (!open.empty()) {
        auto [f, u] = open.top(); open.pop();
        if (u == goal) {
            std::vector<int> path;
            for (int c = goal; c != start; c = prev[c]) { path.push_back(c); if (!prev.count(c)) return {}; }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }
        if (closed.count(u)) continue;
        closed.insert(u);
        for (auto& e : g.neighbors(u)) {
            if (closed.count(e.to)) continue;
            double tentative = gScore[u] + cost(e);
            if (!gScore.count(e.to) || tentative < gScore[e.to]) {
                gScore[e.to] = tentative;
                prev[e.to] = u;
                open.push({tentative + heuristic(e.to), e.to});
            }
        }
    }
    return {};
}

std::shared_ptr<RoutingStrategy> RouterFactory::create(RouterKind kind) {
    switch (kind) {
        case RouterKind::DIJKSTRA: return std::make_shared<DijkstraRouter>();
        case RouterKind::ASTAR:    return std::make_shared<AStarRouter>();
        case RouterKind::PRIORITY: return std::make_shared<PriorityRouter>();
    }
    return std::make_shared<PriorityRouter>();
}

} // namespace sentinel
