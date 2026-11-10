// Graph.h — Weighted, dynamic city graph (adjacency list + coordinates)
#pragma once
#include "CoreTypes.h"
#include <unordered_map>
#include <mutex>

namespace sentinel {

class Graph {
public:
    int addNode(const std::string& name, float x, float y);
    void addEdge(int a, int b, double distance, const std::string& roadName,
                 const std::string& roadType, double traffic);

    int nodeCount() const;
    int edgeCount() const;

    bool hasNode(int id) const;
    NodeInfo node(int id) const;
    Vec2 position(int id) const;
    std::string nodeName(int id) const;

    // Neighbors with current edge data
    std::vector<EdgeInfo> neighbors(int id) const;
    std::vector<int> allNodeIds() const;
    std::vector<EdgeInfo> allEdges() const;

    // Dynamic traffic
    void setTraffic(int a, int b, double t);
    double getTraffic(int a, int b) const;
    EdgeInfo* findEdgeMutable(int a, int b);

    double euclidean(int a, int b) const;      // straight-line distance helper
    double travelTimeMin(int a, int b, double speedKmh) const; // edge traversal time

private:
    struct EdgeRec { EdgeInfo info; };
    std::vector<NodeInfo> nodes_;
    std::vector<EdgeRec> edges_;
    std::unordered_map<int, std::vector<int>> adj_; // nodeId -> indices into edges_
    mutable std::recursive_mutex mu_;
};

} // namespace sentinel
