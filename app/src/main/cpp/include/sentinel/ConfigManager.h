// ConfigManager.h — Singleton global configuration
#pragma once
#include "CoreTypes.h"
#include <string>

namespace sentinel {

class ConfigManager {
public:
    static ConfigManager& instance();

    std::string mapPath = "sample_city.json";
    double simSpeedMultiplier = 30.0;   // sim seconds per real second
    double timeScaleMinutesPerSec = 0.25; // sim minutes advanced per real second at 1x
    int corridorLookahead = 4;          // nodes ahead to turn green
    double baseCongestion = 0.45;

    void reset();

private:
    ConfigManager() = default;
};

} // namespace sentinel
