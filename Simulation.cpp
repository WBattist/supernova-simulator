// Taken from kavan, translated from opengl to raylib, and heavily modified to be more physically accurate and visually appealing. 
#include "Simulation.h"
#include "DebugLogger.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

extern bool pause;
extern float simulationSpeedFactor;

float separation = 12000.0f;
float phase = 0.0f;
float eccentricity = 0.0f; // Defaulting to 0.0f for stable initialization
float inclination = 0.0f;
float longitude = 0.0f;

static Vector3 RotateOrbitPoint(Vector3 point, float inclinationDegrees, float longitudeDegrees) {
    float incRad = inclinationDegrees * (PI / 180.0f);
    float lonRad = longitudeDegrees * (PI / 180.0f);

    float cosInc = cosf(incRad);
    float sinInc = sinf(incRad);
    float cosLon = cosf(lonRad);
    float sinLon = sinf(lonRad);

    return Vector3{
        point.x * cosLon - point.z * sinLon * cosInc,
        point.z * sinInc,
        point.x * sinLon + point.z * cosLon * cosInc
    };
}

Object CreateWhiteDwarf(Vector3 position, Vector3 velocity, float mass, Vector4 color, bool glow, float density) {
    return Object(position, velocity, mass, density, color, glow, OBJ_WHITE_DWARF);
}

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

    // Bound initial separation in meters (~60,000 km) for strong gravity and rapid GW decay
    separation = 45.0e8f; 
    phase = 0.0f;
    eccentricity = 0.0f; // Injecting slight starting eccentricity for clear elliptical path profiles!
    inclination = 0.0f;
    longitude = 0.0f;

    // Masses in kg (using standard Solar Mass conversion constants)
    double m_WD1 = 1.20f * M_SUN; 
    double m_WD2 = 0.85f * M_SUN; 
    double totalMass = m_WD1 + m_WD2;

    // Calculate stable circular orbit radii around barycenter
    float r1 = separation * (float)(m_WD2 / totalMass);
    float r2 = separation * (float)(m_WD1 / totalMass);

    // Exact Keplerian orbital velocities required for stability (Accounting for starting eccentricity)
    float v1 = sqrtf((float)(G * m_WD2 * m_WD2 / (totalMass * separation))) * (1.0f + eccentricity);
    float v2 = sqrtf((float)(G * m_WD1 * m_WD1 / (totalMass * separation))) * (1.0f + eccentricity);

    // Pure flat 2D physics coordinates on the X/Z plane
    Vector3 pos1 = Vector3{ -r1, 0.0f, 0.0f };
    Vector3 pos2 = Vector3{  r2, 0.0f, 0.0f };
    Vector3 vel1 = Vector3{ 0.0f, 0.0f, -v1 };
    Vector3 vel2 = Vector3{ 0.0f, 0.0f,  v2 };

    Vector4 normalizedColor = {
        (float)babyboybuttermybunsblue.r / 255.0f,
        (float)babyboybuttermybunsblue.g / 255.0f,
        (float)babyboybuttermybunsblue.b / 255.0f,
        (float)babyboybuttermybunsblue.a / 255.0f
    };

    Object wd1 = CreateWhiteDwarf(pos1, vel1, m_WD1, normalizedColor, true, 1.0e9f);
    wd1.radius = 8.29e6f; // Metric meters allocation guard (~8,290 km)
    
    Object wd2 = CreateWhiteDwarf(pos2, vel2, m_WD2, normalizedColor, true, 2.0e9f);
    wd2.radius = 5.86e6f; // (~5,860 km)

    objs.push_back(wd1);
    objs.push_back(wd2);
}

void UpdatePhysics(std::vector<Object>& objs, std::vector<AccretionParticle>& accretionFlow, std::vector<DebrisParticle>& ejecta, float deltaTime, bool& triggerExplosion, Vector3& explosionPosition) {
    // Calculate total simulated time chunk assigned to this frame loop
    float totalTimeBill = deltaTime * SIMULATION_TIME_SCALE * simulationSpeedFactor;

    // Strict sub-step ceiling (in seconds) to prevent numerical overshooting/teleportation
    const float maxSubStep = 1.0f; 
    float timeAccumulator = 0.0f;

    if (objs.empty()) {
        for (auto it = ejecta.begin(); it != ejecta.end();) {
            it->position += it->velocity * totalTimeBill;
            it->lifeTime -= totalTimeBill;
            it->color.w = it->lifeTime / it->maxLifeTime;
            it->radius += (280.0f + (fabsf(it->velocity.x) * 0.015f)) * totalTimeBill; 
            
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

    // Loop and execute the physics equations in stable, manageable steps
    while (timeAccumulator < totalTimeBill) {
        float physicsDeltaTime = fminf(maxSubStep, totalTimeBill - timeAccumulator);
        timeAccumulator += physicsDeltaTime;

        if (!pause) {
            Vector3 separationVector = wd2.position - wd1.position;
            
            float softening = 1000.0f; // Scaled up to cleanly match raw meter dimensions
            float distanceSquared = separationVector.x * separationVector.x + separationVector.y * separationVector.y + separationVector.z * separationVector.z + softening * softening;
            float distance = sqrtf(distanceSquared);
            float invDistanceCubed = 1.0f / (distanceSquared * distance);

            Vector3 accelOnWd1 = separationVector * ((float)(G * wd2.mass) * invDistanceCubed);
            Vector3 accelOnWd2 = separationVector * (-(float)(G * wd1.mass) * invDistanceCubed);

            wd1.velocity += accelOnWd1 * physicsDeltaTime;
            wd2.velocity += accelOnWd2 * physicsDeltaTime;

            // --- Consolidated Smooth Inspiral Tangential Velocity Damping ---
            {
                double m1 = wd1.mass;
                double m2 = wd2.mass;
                double M = m1 + m2;

                Vector3 relPosDamp = wd2.position - wd1.position;
                double rDamp = sqrt((double)(relPosDamp.x * relPosDamp.x + relPosDamp.y * relPosDamp.y + relPosDamp.z * relPosDamp.z));

                // TUNED FLUID SCALE: Reduced factor smoothly drains velocity over orbits 
                // without collapsing the centrifugal loop into a straight drop line instantly.
                const double baseK = 5.5e-2;
                
                double scale = (rDamp > 1e-6) ? (6.0e7 / rDamp) : 1.0;
                double dampingK = baseK * (scale * scale * scale); 

                Vector3 vcm = (wd1.velocity * (m1 / (float)M)) + (wd2.velocity * (m2 / (float)M));
                Vector3 relVelDamp = wd2.velocity - wd1.velocity;

                Vector3 rhat = relPosDamp / (float)rDamp;
                float v_radial_mag = relVelDamp.x * rhat.x + relVelDamp.y * rhat.y + relVelDamp.z * rhat.z;
                
                Vector3 v_radial = rhat * v_radial_mag;
                Vector3 v_tangent = relVelDamp - v_radial;

                // Bleed off tangential velocity smoothly to display orbital tightening
                float dampFactor = 1.0f - (float)(dampingK * physicsDeltaTime);
                if (dampFactor < 0.1f) dampFactor = 0.1f; // Prevent full sideways velocity zeroing
                v_tangent *= dampFactor;

                Vector3 vrel_decayed = v_radial + v_tangent;

                wd1.velocity = vcm - vrel_decayed * (float)(m2 / M);
                wd2.velocity = vcm + vrel_decayed * (float)(m1 / M);
            }

            wd1.position += wd1.velocity * physicsDeltaTime;
            wd2.position += wd2.velocity * physicsDeltaTime;

            // Strict 2D Local Planar Lock
            wd1.position.y = 0.0f;
            wd2.position.y = 0.0f;
            wd1.velocity.y = 0.0f;
            wd2.velocity.y = 0.0f;

            // Roche-Lobe Overlap Mass Transfer evaluation
            float currentDist = Vector3Distance(wd1.position, wd2.position);
            if (currentDist < (wd1.radius + wd2.radius) * 1.5f) {
                float massTransfer = 8.0e19f * physicsDeltaTime;
                if (wd2.mass > massTransfer) {
                    wd2.mass -= massTransfer;
                    wd1.mass += massTransfer;
                }
                if (GetRandomValue(0, 100) < 40) {
                    accretionFlow.push_back({ wd2.position, 0.0f });
                }
            }
        }

        // Physical Merger Intersection Threshold
        if (!pause) {
            float collisionDist = (wd1.radius + wd2.radius) * 1.02f;
            float currentDist = Vector3Distance(wd1.position, wd2.position);
            if (currentDist <= collisionDist) {
                float combinedMass = wd1.mass + wd2.mass;
                if (combinedMass >= CHANDRASEKHAR_LIMIT) {
                    triggerExplosion = true;
                    explosionPosition = (wd1.position + wd2.position) * 0.5f;
                    break; 
                }
            }
        }
    }

    // Process gas particles (Outside sub-step loop for render sync precision)
    for (auto it = accretionFlow.begin(); it != accretionFlow.end();) {
        it->progress += 4.0f * deltaTime;
        if (it->progress >= 1.0f || objs.size() < 2) {
            it = accretionFlow.erase(it);
        } else {
            it->position = Vector3Lerp(objs[1].position, objs[0].position, it->progress);
            ++it;
        }
    }

    // Process diagnostic logging output
    DebugLogger::LogSystemState(objs, deltaTime);
}