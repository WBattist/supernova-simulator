#include "DebugLogger.h"
#include <iostream>
#include <iomanip>
#include <cmath>

// =========================================================================
// TOGGLE SWITCH: Set to true for absolute headless console logs.
//                Set to false to use normal 3D visual windows on desktop monitors.
// =========================================================================
const bool ENABLE_LOGGING = false; 

const float DebugLogger::LOG_INTERVAL = 0.5f; 
float DebugLogger::logTimer = 0.0f;

void DebugLogger::LogSystemState(const std::vector<Object>& objs, float deltaTime) {
    if (!ENABLE_LOGGING) return;

    logTimer += deltaTime;
    if (logTimer < LOG_INTERVAL) return;
    logTimer = 0.0f; 

    if (objs.empty()) return;

    if (objs.size() < 2) {
        std::cout << "[SIMULATION] System has merged or is empty.\n";
        return;
    }

    const auto& wd1 = objs[0];
    const auto& wd2 = objs[1];

    float dx = wd2.position.x - wd1.position.x;
    float dy = wd2.position.y - wd1.position.y;
    float distance = std::sqrt(dx*dx + dy*dy);

    float rvx = wd2.velocity.x - wd1.velocity.x;
    float rvy = wd2.velocity.y - wd1.velocity.y;
    float relSpeed = std::sqrt(rvx*rvx + rvy*rvy);

    std::cout << "\n================= DIAGNOSTIC DASHBOARD =================" << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Distance Between Stars : " << (distance / 1000.0f) << " km" << std::endl;
    std::cout << "Relative Orbital Speed : " << (relSpeed / 1000.0f) << " km/s" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "White Dwarf 1 (Primary):" << std::endl;
    std::cout << "  Mass                 : " << (wd1.mass / 1.989e30f) << " Solar Masses" << std::endl;
    std::cout << "  Physical Radius      : " << (wd1.radius / 1000.0f) << " km" << std::endl;
    std::cout << "White Dwarf 2 (Donor)  :" << std::endl;
    std::cout << "  Mass                 : " << (wd2.mass / 1.989e30f) << " Solar Masses" << std::endl;
    std::cout << "  Physical Radius      : " << (wd2.radius / 1000.0f) << " km" << std::endl;
    std::cout << "=======================================================" << std::endl;
}