#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

#include <vector>
#include "Object.h"

// Shared flag visible to your main setup pass
extern const bool ENABLE_LOGGING;

class DebugLogger {
private:
    static float logTimer;
    static const float LOG_INTERVAL;

public:
    static void LogSystemState(const std::vector<Object>& objs, float deltaTime);
};

#endif // DEBUG_LOGGER_H