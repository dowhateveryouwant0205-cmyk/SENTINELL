// Signals.h — TrafficSignal + SignalController (Observer Pattern)
#pragma once
#include "CoreTypes.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace sentinel {

class Graph;

// ---- Observer interface: notified when a signal changes state ----
class SignalObserver {
public:
    virtual ~SignalObserver() = default;
    virtual void onSignalChanged(int nodeId, SignalState oldState, SignalState newState) = 0;
};

class TrafficSignal {
public:
    int nodeId = -1;
    SignalState state = SignalState::GREEN;
    double elapsed = 0.0;      // seconds in current phase
    double greenDur = 25.0;
    double yellowDur = 4.0;
    double redDur = 30.0;
    bool preempted = false;    // held GREEN by SENTINEL
    int preemptHolds = 0;

    // Advance cycle; returns new state if changed.
    bool tick(double dtSec);
    void setPreempt(bool on);
};

// Subject of the Observer pattern; manages all signals in the city.
class SignalController {
public:
    void buildFromGraph(const std::shared_ptr<Graph>& g);   // one signal per node
    void attach(SignalObserver* obs);
    void detach(SignalObserver* obs);

    void tick(double dtSec);                                // advance normal cycles

    // Green corridor: preempt signals within `lookahead` nodes ahead of path.
    int preemptCorridor(const std::vector<int>& path, int currentIndex, int lookahead, int priority);
    void releasePreempts();                                 // decay holds each tick

    SignalState stateOf(int nodeId) const;
    bool isPreempted(int nodeId) const;
    std::vector<std::pair<int, SignalState>> snapshot() const;
    long changeCount() const { return changeCount_; }

private:
    void notify(int nodeId, SignalState oldS, SignalState newS);
    std::unordered_map<int, TrafficSignal> signals_;
    std::vector<SignalObserver*> observers_;
    mutable std::recursive_mutex mu_;
    long changeCount_ = 0;
};

} // namespace sentinel
