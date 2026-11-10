// Signals.cpp — TrafficSignal cycles + SignalController (Observer Pattern)
#include "sentinel/Signals.h"
#include "sentinel/Graph.h"
#include <algorithm>

namespace sentinel {

bool TrafficSignal::tick(double dtSec) {
    if (preempted) return false; // held GREEN for emergency corridor
    elapsed += dtSec;
    SignalState old = state;
    switch (state) {
        case SignalState::GREEN:
            if (elapsed >= greenDur) { state = SignalState::YELLOW; elapsed = 0; }
            break;
        case SignalState::YELLOW:
            if (elapsed >= yellowDur) { state = SignalState::RED; elapsed = 0; }
            break;
        case SignalState::RED:
            if (elapsed >= redDur) { state = SignalState::GREEN; elapsed = 0; }
            break;
    }
    return state != old;
}

void TrafficSignal::setPreempt(bool on) {
    preempted = on;
    if (on) { state = SignalState::GREEN; elapsed = 0; preemptHolds = 3; }
}

void SignalController::buildFromGraph(const std::shared_ptr<Graph>& g) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    signals_.clear();
    for (int id : g->allNodeIds()) {
        TrafficSignal s;
        s.nodeId = id;
        // Stagger phases so the city doesn't pulse in unison.
        double phase = (id % 5) * 6.0;
        s.state = (id % 3 == 0) ? SignalState::RED : (id % 3 == 1 ? SignalState::GREEN : SignalState::YELLOW);
        s.elapsed = phase;
        signals_[id] = s;
    }
}

void SignalController::attach(SignalObserver* obs) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (obs && std::find(observers_.begin(), observers_.end(), obs) == observers_.end())
        observers_.push_back(obs);
}

void SignalController::detach(SignalObserver* obs) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    observers_.erase(std::remove(observers_.begin(), observers_.end(), obs), observers_.end());
}

void SignalController::notify(int nodeId, SignalState oldS, SignalState newS) {
    // Called with lock held; copy observers to avoid deadlock in callbacks.
    auto obs = observers_;
    changeCount_++;
    for (auto* o : obs) o->onSignalChanged(nodeId, oldS, newS);
}

void SignalController::tick(double dtSec) {
    std::vector<std::tuple<int, SignalState, SignalState>> changes;
    {
        std::lock_guard<std::recursive_mutex> lk(mu_);
        for (auto& [id, s] : signals_) {
            SignalState before = s.state;
            if (s.tick(dtSec)) changes.emplace_back(id, before, s.state);
            if (s.preempted && --s.preemptHolds <= 0) {
                s.preempted = false; // corridor released after vehicle passed
            }
        }
    }
    for (auto& [id, a, b] : changes) notify(id, a, b);
}

int SignalController::preemptCorridor(const std::vector<int>& path, int currentIndex,
                                      int lookahead, int priority) {
    std::vector<std::tuple<int, SignalState, SignalState>> changes;
    int flipped = 0;
    {
        std::lock_guard<std::recursive_mutex> lk(mu_);
        int end = std::min((int)path.size() - 1, currentIndex + lookahead);
        // Higher-priority vehicles get longer hold times (in signal-tick units).
        int holds = 6 + priority;
        for (int i = std::max(1, currentIndex + 1); i <= end; ++i) {
            auto it = signals_.find(path[i]);
            if (it == signals_.end()) continue;
            auto& s = it->second;
            if (!s.preempted || s.state != SignalState::GREEN) {
                SignalState before = s.state;
                s.setPreempt(true);
                s.preemptHolds = holds;
                if (before != s.state) { changes.emplace_back(s.nodeId, before, s.state); flipped++; }
            } else {
                s.preemptHolds = std::max(s.preemptHolds, holds);
            }
        }
    }
    for (auto& [id, a, b] : changes) notify(id, a, b);
    return flipped;
}

void SignalController::releasePreempts() {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    for (auto& [id, s] : signals_) s.preempted = false;
}

SignalState SignalController::stateOf(int nodeId) const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    auto it = signals_.find(nodeId);
    return it == signals_.end() ? SignalState::GREEN : it->second.state;
}

bool SignalController::isPreempted(int nodeId) const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    auto it = signals_.find(nodeId);
    return it != signals_.end() && it->second.preempted;
}

std::vector<std::pair<int, SignalState>> SignalController::snapshot() const {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<std::pair<int, SignalState>> out;
    for (auto& [id, s] : signals_) out.emplace_back(id, s.state);
    return out;
}

} // namespace sentinel
