// Vehicle.h — Abstract base + concrete emergency vehicles (OOP showcase)
#pragma once
#include "CoreTypes.h"
#include <memory>

namespace sentinel {

class Vehicle {
public:
    virtual ~Vehicle() = default;

    // ---- Pure virtual (abstraction) ----
    virtual int priorityLevel() const = 0;
    virtual std::string typeName() const = 0;

    // ---- Virtual with defaults (polymorphism) ----
    virtual double maxSpeedKmh() const { return 60.0; }
    virtual double maneuverability() const { return 1.0; }   // 0..1, affects turns
    virtual bool canPreemptSignals() const { return true; }
    virtual double slowdownAtRed() const { return 3.2; }      // time multiplier when forced to stop

    // ---- Common state / behavior ----
    int id = -1;
    Vec2 pos;
    int currentNode = -1;
    int nextNode = -1;
    double progress = 0.0;                 // 0..1 along current edge
    VehicleStatus status = VehicleStatus::IDLE;
    std::vector<int> path;                 // planned node route
    int assignedIncident = -1;
    double dispatchSimTime = -1.0;         // sim-clock minute at dispatch
    double arrivalSimTime = -1.0;
    bool stoppedAtRed = false;             // baseline mode red-light delay flag

    void setPath(const std::vector<int>& p);
    bool hasRoute() const { return path.size() >= 2; }
    int targetNode() const { return path.empty() ? -1 : path.back(); }

    // Advance along the path by `distanceKm`. Returns true if reached final node.
    bool advance(double distanceKm, const class Graph& g);

    Vec2 interpolatedPosition(const class Graph& g) const;
};

class Ambulance : public Vehicle {
public:
    int priorityLevel() const override { return 4; }
    std::string typeName() const override { return "Ambulance"; }
    double maxSpeedKmh() const override { return 78; }
    double maneuverability() const override { return 0.9; }
};

class FireTruck : public Vehicle {
public:
    int priorityLevel() const override { return 3; }
    std::string typeName() const override { return "FireTruck"; }
    double maxSpeedKmh() const override { return 55; }
    double maneuverability() const override { return 0.6; } // slow, wide turns
};

class PoliceCar : public Vehicle {
public:
    int priorityLevel() const override { return 2; }
    std::string typeName() const override { return "PoliceCar"; }
    double maxSpeedKmh() const override { return 92; }
    double maneuverability() const override { return 1.0; }
};

class HazmatUnit : public Vehicle {
public:
    int priorityLevel() const override { return 5; }
    std::string typeName() const override { return "HazmatUnit"; }
    double maxSpeedKmh() const override { return 62; }
    double maneuverability() const override { return 0.7; }
    bool canPreemptSignals() const override { return true; }
};

// Factory Pattern: create vehicles by type string.
class VehicleFactory {
public:
    static std::unique_ptr<Vehicle> create(const std::string& type);
};

} // namespace sentinel
