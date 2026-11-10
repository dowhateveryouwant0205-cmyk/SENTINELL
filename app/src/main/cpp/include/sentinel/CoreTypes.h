// CoreTypes.h — Shared enums, structs and small value types for SENTINEL
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace sentinel {

enum class SignalState { RED, YELLOW, GREEN };
enum class VehicleStatus { IDLE, EN_ROUTE, ARRIVED, RETURNING };
enum class SimMode { SENTINEL, BASELINE };
enum class RouterKind { DIJKSTRA, ASTAR, PRIORITY };

struct Vec2 {
    float x = 0.f, y = 0.f;
};

struct NodeInfo {
    int id = -1;
    std::string name;
    Vec2 pos;
    SignalState signal = SignalState::GREEN;
    bool hasSignal = true;
};

struct EdgeInfo {
    int from = -1, to = -1;
    double distance = 1.0;
    std::string roadName;
    std::string roadType = "arterial"; // arterial | collector | highway
    double traffic = 0.3;              // dynamic density 0..1
};

struct VehicleInfo {
    int id = -1;
    std::string type;        // Ambulance / FireTruck / PoliceCar / HazmatUnit
    int priority = 0;
    Vec2 pos;
    int currentNode = -1;
    int targetNode = -1;
    VehicleStatus status = VehicleStatus::IDLE;
    double speedKmh = 60;
    std::vector<int> path;   // planned route (node ids)
    int incidentId = -1;
    double progress = 0.0;   // 0..1 between currentNode and next node
};

struct IncidentInfo {
    int id = -1;
    std::string type;        // medical | fire | accident | hazmat
    int node = -1;
    int severity = 1;
    bool active = true;
    int assignedVehicle = -1;
    double responseTimeMin = -1.0;
};

} // namespace sentinel
