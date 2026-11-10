// Dispatch.h — Incident, Dispatcher, DispatchCommand (Command Pattern)
#pragma once
#include "CoreTypes.h"
#include "Vehicle.h"
#include "Routing.h"
#include "Signals.h"
#include <memory>
#include <vector>
#include <deque>

namespace sentinel {

class Graph;

// ---- Incident (abstract base + concrete incident kinds) ----
class Incident {
public:
    virtual ~Incident() = default;
    int id = -1;
    int node = -1;
    int severity = 1;              // 1..5
    bool active = true;
    int assignedVehicle = -1;
    double dispatchTime = -1.0;    // sim minutes
    double arrivalTime = -1.0;

    virtual std::string typeName() const = 0;
    virtual std::string preferredVehicle() const = 0; // vehicle type string
};

class MedicalIncident : public Incident {
public: std::string typeName() const override { return "medical"; }
         std::string preferredVehicle() const override { return "Ambulance"; }
};
class FireIncident : public Incident {
public: std::string typeName() const override { return "fire"; }
        std::string preferredVehicle() const override { return "FireTruck"; }
};
class AccidentIncident : public Incident {
public: std::string typeName() const override { return "accident"; }
        std::string preferredVehicle() const override { return "PoliceCar"; }
};
class HazmatIncident : public Incident {
public: std::string typeName() const override { return "hazmat"; }
        std::string preferredVehicle() const override { return "HazmatUnit"; }
};

class IncidentSpawner {
public:
    // Randomly pick a congested node and an incident type.
    std::unique_ptr<Incident> spawn(const Graph& g, int nextId, unsigned seed);
};

// ---- Command Pattern: every dispatch is an undoable/loggable command ----
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string describe() const = 0;
};

class DispatchCommand : public Command {
public:
    DispatchCommand(std::shared_ptr<Vehicle> v, std::shared_ptr<Incident> inc,
                    std::vector<int> path, VehicleStatus prevStatus,
                    std::vector<int> prevPath, int prevIncident);
    void execute() override;
    void undo() override;
    std::string describe() const override;

    std::shared_ptr<Vehicle> vehicle;
    std::shared_ptr<Incident> incident;
    std::vector<int> newPath;
    VehicleStatus oldStatus;
    std::vector<int> oldPath;
    int oldIncident;
};

// ---- Dispatcher: assigns the best-suited nearest available vehicle ----
class Dispatcher {
public:
    void setStrategy(std::shared_ptr<RoutingStrategy> s) { strategy_ = std::move(s); }
    std::shared_ptr<RoutingStrategy> strategy() const { return strategy_; }

    // Returns the command executed (and recorded in history), or nullptr if no unit found.
    std::shared_ptr<DispatchCommand> dispatch(
        const std::shared_ptr<Incident>& inc,
        const std::vector<std::shared_ptr<Vehicle>>& fleet,
        const Graph& g, double simMinute);

    const std::deque<std::shared_ptr<DispatchCommand>>& history() const { return history_; }
    bool undoLast();

private:
    std::shared_ptr<RoutingStrategy> strategy_;
    std::deque<std::shared_ptr<DispatchCommand>> history_;
};

} // namespace sentinel
