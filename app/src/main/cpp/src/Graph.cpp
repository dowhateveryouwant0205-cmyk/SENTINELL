// Graph.cpp
#include "sentinel/Graph.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace sentinel {

int Graph::addNode(const std::string& name, float x, float y) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    int id = static_cast<int>(nodes_.size());
    NodeInfo n;
    n.id = id; n.name = name; n.pos = {x, y};
    nodes_.push_back(n);
    adj_[id];
    return id;
}

void Graph::addEdge(int a, int b, double distance, const std::string& roadName,
                    const std::string& roadType, double traffic) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (a < 0 || b < 0 || a >= (int)nodes_.size() || b >= (int)nodes_.size())
        throw std::runtime_error("addEdge: invalid node id");
    EdgeInfo e;
    e.from = a; e.to = b; e.distance = std::max(0.05, distance);
    e.roadName = roadName; e.roadType = roadType;
    e.traffic = std::clamp(traffic, 0.0, 1.0);
    int idx = static_cast<int>(edges_.size());
    edges_.push_back({e});
    adj_[a].push_back(idx);
    adj_[b].push_back(idx); // undirected city streets
}

int Graph::nodeCount() const { std::lock_guard<std::recursive_mutex> lk(mu_); return (int)nodes_.size(); }
int Graph::edgeCount() const { std::lock_guard<std::recursive_mutex> lk(mu_); return (int)edges_.size(); }

bool Graph::hasNode(int id) const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    return id >= 0 && id < (int)nodes_.size();
}

NodeInfo Graph::node(int id) const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (!hasNode(id)) throw std::runtime_error("node(): invalid id");
    return nodes_[id];
}

Vec2 Graph::position(int id) const { return node(id).pos; }
std::string Graph::nodeName(int id) const { return node(id).name; }

std::vector<EdgeInfo> Graph::neighbors(int id) const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<EdgeInfo> out;
    auto it = adj_.find(id);
    if (it == adj_.end()) return out;
    for (int ei : it->second) {
        EdgeInfo e = edges_[ei].info;
        if (e.from != id) std::swap(e.from, e.to); // orient from=id
        out.push_back(e);
    }
    return out;
}

std::vector<int> Graph::allNodeIds() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<int> ids;
    for (auto& n : nodes_) ids.push_back(n.id);
    return ids;
}

std::vector<EdgeInfo> Graph::allEdges() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<EdgeInfo> out;
    for (auto& e : edges_) out.push_back(e.info);
    return out;
}

void Graph::setTraffic(int a, int b, double t) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (auto* e = findEdgeMutable(a, b)) e->traffic = std::clamp(t, 0.0, 1.0);
}

double Graph::getTraffic(int a, int b) const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    auto& self = const_cast<Graph&>(*this);
    if (auto* e = self.findEdgeMutable(a, b)) return e->traffic;
    return 0.0;
}

EdgeInfo* Graph::findEdgeMutable(int a, int b) {
    auto it = adj_.find(a);
    if (it == adj_.end()) return nullptr;
    for (int ei : it->second) {
        auto& e = edges_[ei].info;
        if ((e.from == a && e.to == b) || (e.from == b && e.to == a)) return &e;
    }
    return nullptr;
}

double Graph::euclidean(int a, int b) const {
    auto pa = position(a), pb = position(b);
    double dx = pa.x - pb.x, dy = pa.y - pb.y;
    return std::sqrt(dx * dx + dy * dy);
}

double Graph::travelTimeMin(int a, int b, double speedKmh) const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    const EdgeInfo* e = const_cast<Graph*>(this)->findEdgeMutable(a, b);
    double dist = e ? e->distance : euclidean(a, b);
    double traffic = e ? e->traffic : 0.3;
    // Congestion slows effective speed: free flow at 0, ~35% at full gridlock.
    double eff = std::max(8.0, speedKmh * (1.0 - 0.65 * traffic));
    return (dist / eff) * 60.0;
}

} // namespace sentinel
