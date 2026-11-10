// ConfigManager.cpp — Meyers singleton
#include "sentinel/ConfigManager.h"

namespace sentinel {

ConfigManager& ConfigManager::instance() {
    static ConfigManager cfg;
    return cfg;
}

void ConfigManager::reset() {
    mapPath = "sample_city.json";
    simSpeedMultiplier = 30.0;
    timeScaleMinutesPerSec = 0.25;
    corridorLookahead = 4;
    baseCongestion = 0.45;
}

} // namespace sentinel
