#include "Simulation.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

extern bool pause;
extern float separation;
extern float phase;
extern float eccentricity;
extern float inclination;
extern float longitude;

Vector3 CalculateBarycenter(const std::vector<Object>& bodies) {
    Vector3 com = { 0.0f, 0.0f, 0.0f };
    float totalMass = 0.0f;
    for (const auto& obj : bodies) {
        com += obj.position * obj.mass;
        totalMass += obj.mass;
    }
    if (totalMass > 0.0f) {
        com = com / totalMass;
    }
    return com;
}

void ResetToStableDoubleDegenerate(std::vector<Object>& objs, std::vector<DebrisParticle>& ejecta, std::vector<AccretionParticle>& accretionFlow) {
    objs.clear();
    ejecta.clear();
    accretionFlow.clear();

    // Start even further apart for a highly dramatic, sweeping orbital sequence
    separation = 12000.0f; 
    phase = 0.0f;
    eccentricity = 0.25f;  // Elliptic Keplerian eccentricity
    inclination = 0.0f;    // Flat orbit in X-Z plane
    longitude = 0.0f;

    double m_WD1 = 2.8e25; // Massive Primary 
    double m_WD2 = 1.0e25; // Lighter Secondary

    Vector3 systemCenter = { 0.0f, 0.0f, -350.0f }; 

    // Pre-calculate initial positions at phase = 0.0f based on mass ratio and barycenter
    float r = separation * (1.0f - eccentricity);
    float r1 = r * (m_WD2 / (m_WD1 + m_WD2));
    float r2 = r * (m_WD1 / (m_WD1 + m_WD2));

    float inc_rad = inclination * (PI / 180.0f);
    float omega_rad = longitude * (PI / 180.0f);

    float cosInc = cosf(inc_rad);
    float sinInc = sinf(inc_rad);
    float cosOmega = cosf(omega_rad);
    float sinOmega = sinf(omega_rad);

    float oX1 = -r1; // cos(0) = 1, sin(0) = 0
    float oZ1 = 0.0f;
    Vector3 pos1 = systemCenter + Vector3{
        oX1 * cosOmega - oZ1 * sinOmega * cosInc,
        oZ1 * sinInc,
        oX1 * sinOmega + oZ1 * cosOmega * cosInc
    };

    float oX2 = r2;
    float oZ2 = 0.0f;
    Vector3 pos2 = systemCenter + Vector3{
        oX2 * cosOmega - oZ2 * sinOmega * cosInc,
        oZ2 * sinInc,
        oX2 * sinOmega + oZ2 * cosOmega * cosInc
    };

// Convert custom macro color to normalized floats for shader rendering
    Vector4 normalizedColor = {
        (float)babyboybuttermybunsblue.r / 255.0f,
        (float)babyboybuttermybunsblue.g / 255.0f,
        (float)babyboybuttermybunsblue.b / 255.0f,
        (float)babyboybuttermybunsblue.a / 255.0f
    };

    // Changed 6th parameter (glow) from true to false to enable rich 3D shading
    objs.push_back(Object(pos1, Vector3{ 0.0f, 0.0f, 0.0f }, m_WD1, 5515.0f, normalizedColor, false, OBJ_WHITE_DWARF));
    objs.push_back(Object(pos2, Vector3{ 0.0f, 0.0f, 0.0f }, m_WD2, 5515.0f, normalizedColor, false, OBJ_WHITE_DWARF));
}

void UpdatePhysics(std::vector<Object>& objs, std::vector<AccretionParticle>& accretionFlow, std::vector<DebrisParticle>& ejecta, float deltaTime, bool& triggerExplosion, Vector3& explosionPosition) {
    // Process ejecta particle movement if stars have already detonated
    if (objs.empty()) {
        for (auto it = ejecta.begin(); it != ejecta.end();) {
            it->position.x += it->velocity.x / 94.0f;
            it->position.y += it->velocity.y / 94.0f;
            it->position.z += it->velocity.z / 94.0f;
            it->lifeTime -= deltaTime;
            it->color.w = it->lifeTime / it->maxLifeTime;
            
            // Dynamic expansion: faster particles expand slightly quicker
            it->radius += (280.0f + (fabsf(it->velocity.x) * 0.015f)) * deltaTime; 
            
            if (it->lifeTime <= 0.0f) {
                it = ejecta.erase(it);
            } else {
                ++it;
            }
        }
        return;
    }

    if (objs.size() < 2) return;

    auto& wd1 = objs[0];
    auto& wd2 = objs[1];

    if (!pause) {
        // 1. Orbital decay (separation shrinks over time)
        separation -= 150.0f * deltaTime;
        if (separation < 100.0f) separation = 100.0f;

        // 2. Compute dynamic orbital period based on Kepler's 3rd Law
        float period = 0.000004f * sqrtf(powf(separation, 3.0f));
        if (period < 0.1f) period = 0.1f;

        // 3. Advance orbital phase progress
        phase += deltaTime / period;
        if (phase > 1.0f) phase -= 1.0f;

        // 4. Kepler Solver for Eccentric Anomaly (ea)
        float ma = phase * 2.0f * PI;
        float ea = ma;
        for (int i = 0; i < 15; ++i) {
            ea = ma + eccentricity * sinf(ea);
        }

        // 5. True Anomaly (ta) and relative distance (r)
        float cosE = cosf(ea);
        float sinE = sinf(ea);
        float cosTa = (cosE - eccentricity) / (1.0f - eccentricity * cosE);
        float sinTa = (sqrtf(1.0f - eccentricity * eccentricity) * sinE) / (1.0f - eccentricity * cosE);

        float r = separation * (1.0f - eccentricity * cosE);

        // Orbital spacing balances strictly based on the mass ratio of the bodies
        float r1 = r * (wd2.mass / (wd1.mass + wd2.mass));
        float r2 = r * (wd1.mass / (wd1.mass + wd2.mass));

        Vector3 systemCenter = { 0.0f, 0.0f, -350.0f };

        // 6. Project flat orbit on horizontal X-Z plane (strictly constant Y value)
        wd1.position = systemCenter + Vector3{ -r1 * cosTa, 0.0f, -r1 * sinTa };
        wd2.position = systemCenter + Vector3{ r2 * cosTa, 0.0f, r2 * sinTa };

        // Roche lobe mass siphoning
        float currentDist = Vector3Distance(wd1.position, wd2.position);
        if (currentDist < wd2.radius * 3.5f) {
            float massTransfer = 0.045e25f * deltaTime;
            if (wd2.mass > massTransfer) {
                wd2.mass -= massTransfer;
                wd1.mass += massTransfer;
            }
            if (GetRandomValue(0, 100) < 30) {
                accretionFlow.push_back({ wd2.position, 0.0f });
            }
        }
    }

    // Accretion Flow visual siphoning along flat horizontal plane
    for (auto it = accretionFlow.begin(); it != accretionFlow.end();) {
        it->progress += 3.0f * deltaTime;
        if (it->progress >= 1.0f || objs.size() < 2) {
            it = accretionFlow.erase(it);
        } else {
            it->position = Vector3Lerp(objs[1].position, objs[0].position, it->progress);
            ++it;
        }
    }

    // Merger contact detection (only evaluated while the simulation is unpaused)
    if (!pause) {
        float collisionDist = (wd1.radius + wd2.radius) * 1.05f;
        float currentDist = Vector3Distance(wd1.position, wd2.position);
        if (currentDist <= collisionDist) {
            float combinedMass = wd1.mass + wd2.mass;
            if (combinedMass >= CHANDRASEKHAR_LIMIT) {
                triggerExplosion = true;
                explosionPosition = (wd1.position + wd2.position) * 0.5f;
            }
        }
    }
}