// Vehicle.cpp — movement along path + factory
#include "sentinel/Vehicle.h"
#include "sentinel/Graph.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace sentinel {

void Vehicle::setPath(const std::vector<int>& p) {
    path = p;
    if (p.size() >= 2) {
        currentNode = p[0];
        nextNode = p[1];
        progress = 0.0;
    } else if (p.size() == 1) {
        currentNode = p[0];
        nextNode = -1;
        progress = 0.0;
    }
}

bool Vehicle::advance(double distanceKm, const Graph& g) {
    if (!hasRoute()) return status == VehicleStatus::ARRIVED;
    double remaining = distanceKm;
    int guard = 0;
    while (remaining > 1e-9 && guard++ < 64) {
        double edgeLen = g.euclidean(currentNode, nextNode);
        if (edgeLen <= 1e-6) edgeLen = 0.05;
        double need = (1.0 - progress) * edgeLen;
        if (remaining >= need) {
            remaining -= need;
            // step forward one node
            size_t idx = 0;
            for (size_t i = 0; i < path.size(); ++i)
                if (path[i] == currentNode && (int)i + 1 < (int)path.size() && path[i + 1] == nextNode) { idx = i; break; }
            if ((int)idx + 2 < (int)path.size()) {
                currentNode = nextNode;
                nextNode = path[idx + 2];
                progress = 0.0;
            } else {
                currentNode = nextNode;
                nextNode = -1;
                progress = 0.0;
                status = VehicleStatus::ARRIVED;
                pos = g.position(currentNode);
                return true;
            }
        } else {
            progress += remaining / edgeLen;
            remaining = 0.0;
        }
    }
    pos = interpolatedPosition(g);
    return false;
}

Vec2 Vehicle::interpolatedPosition(const Graph& g) const {
    if (nextNode < 0 || currentNode < 0) return g.position(currentNode < 0 ? 0 : currentNode);
    auto a = g.position(currentNode);
    auto b = g.position(nextNode);
    float t = (float)std::clamp(progress, 0.0, 1.0);
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

std::unique_ptr<Vehicle> VehicleFactory::create(const std::string& type) {
    if (type == "Ambulance")  return std::make_unique<Ambulance>();
    if (type == "FireTruck")  return std::make_unique<FireTruck>();
    if (type == "PoliceCar")  return std::make_unique<PoliceCar>();
    if (type == "HazmatUnit") return std::make_unique<HazmatUnit>();
    throw std::runtime_error("VehicleFactory: unknown vehicle type '" + type + "'");
}

} // namespace sentinel
