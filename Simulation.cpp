// Taken from kavan, translated from opengl to raylib, and heavily modified to be more physically accurate and visually appealing. 
#include "Simulation.h"
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

    // Start from a physically plausible close white-dwarf binary in SI units.
    separation = 1.4e9f;
    phase = 0.0f;
    eccentricity = 0.0f;
    inclination = 0.0f;
    longitude = 0.0f;

    double m_WD1 = 1.90f * M_SUN;
    double m_WD2 = 1.05f * M_SUN;

    Vector3 systemCenter = { 0.0f, 0.0f, 0.0f };

    // Set up on flat plane (Z is handled through rendering transformations)
    float r = separation * (1.0f - eccentricity);
    float r1 = r * (m_WD2 / (m_WD1 + m_WD2));
    float r2 = r * (m_WD1 / (m_WD1 + m_WD2));
    float orbitalOmega = sqrtf((float)(G * (m_WD1 + m_WD2) / (r * r * r)));

    // Physics coordinates initialized on a pure flat flat plane (X, Y)
    Vector3 pos1 = Vector3{ -r1, 0.0f, 0.0f };
    Vector3 pos2 = Vector3{ r2, 0.0f, 0.0f };
    Vector3 vel1 = Vector3{ 0.0f, -orbitalOmega * r1, 0.0f };
    Vector3 vel2 = Vector3{ 0.0f, orbitalOmega * r2, 0.0f };

    Vector4 normalizedColor = {
        (float)babyboybuttermybunsblue.r / 255.0f,
        (float)babyboybuttermybunsblue.g / 255.0f,
        (float)babyboybuttermybunsblue.b / 255.0f,
        (float)babyboybuttermybunsblue.a / 255.0f
    };

    objs.push_back(CreateWhiteDwarf(pos1, vel1, m_WD1, normalizedColor, false, 5.0e8f));
    objs.push_back(CreateWhiteDwarf(pos2, vel2, m_WD2, normalizedColor));
}

void UpdatePhysics(std::vector<Object>& objs, std::vector<AccretionParticle>& accretionFlow, std::vector<DebrisParticle>& ejecta, float deltaTime, bool& triggerExplosion, Vector3& explosionPosition) {
    float physicsDeltaTime = deltaTime * SIMULATION_TIME_SCALE * simulationSpeedFactor;

    if (objs.empty()) {
        for (auto it = ejecta.begin(); it != ejecta.end();) {
            it->position += it->velocity * physicsDeltaTime;
            it->lifeTime -= physicsDeltaTime;
            it->color.w = it->lifeTime / it->maxLifeTime;
            it->radius += (280.0f + (fabsf(it->velocity.x) * 0.015f)) * physicsDeltaTime; 
            
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
        Vector3 separationVector = wd2.position - wd1.position;
        
        // Softening reduced to allow complete inner merger contact without gravity clamping
        float softening = 10.0f; 
        float distanceSquared = separationVector.x * separationVector.x + separationVector.y * separationVector.y + separationVector.z * separationVector.z + softening * softening;
        float distance = sqrtf(distanceSquared);
        float invDistanceCubed = 1.0f / (distanceSquared * distance);

        Vector3 accelOnWd1 = separationVector * ((float)(G * wd2.mass) * invDistanceCubed);
        Vector3 accelOnWd2 = separationVector * (-(float)(G * wd1.mass) * invDistanceCubed);

        wd1.velocity += accelOnWd1 * physicsDeltaTime;
        wd2.velocity += accelOnWd2 * physicsDeltaTime;

        // --- Gravitational-wave driven energy loss (Peters, 1964) ---
        {
            double m1 = wd1.mass;
            double m2 = wd2.mass;
            double M = m1 + m2;

            Vector3 relPos = wd2.position - wd1.position;
            Vector3 relVel = wd2.velocity - wd1.velocity;
            double r = sqrt((double)(relPos.x * relPos.x + relPos.y * relPos.y + relPos.z * relPos.z));

            if (r > 1e-3) {
                double P = (32.0 / 5.0) * (G * G * G * G) / (pow((double)c, 5.0)) * (m1 * m1 * m2 * m2 * M) / pow(r, 5.0);
                double dE = P * (double)physicsDeltaTime;

                double Eold = -G * m1 * m2 / (2.0 * r);
                double Enew = Eold - dE;

                if (Enew < -1e-200) {
                    double anew = -G * m1 * m2 / (2.0 * Enew);
                    if (anew < r) {
                        double vrel_new_mag = sqrt(G * M / anew);

                        Vector3 rhat = relPos / (float)r;
                        double vdotr = relVel.x * rhat.x + relVel.y * rhat.y + relVel.z * rhat.z;
                        Vector3 vt = relVel - rhat * (float)vdotr;
                        double vtmag = sqrt((double)(vt.x * vt.x + vt.y * vt.y + vt.z * vt.z));

                        Vector3 tangent = (vtmag > 1e-9) ? (vt / (float)vtmag) : Vector3Normalize(Vector3{ -rhat.y, rhat.x, 0.0f });
                        Vector3 vcm = (wd1.velocity * (m1 / (float)M)) + (wd2.velocity * (m2 / (float)M));

                        // Retain the actual radial velocity component so they can plunge inward!
                        Vector3 v_radial = rhat * (float)vdotr;
                        Vector3 v_tangent_corrected = tangent * (float)vrel_new_mag;
                        Vector3 vrel_combined = v_radial + v_tangent_corrected;

                        wd1.velocity = vcm - vrel_combined * (float)(m2 / M);
                        wd2.velocity = vcm + vrel_combined * (float)(m1 / M);
                    }
                }
            }
        }

        // --- Core-Isolating Tangential Velocity Damping Loop ---
        {
            double m1 = wd1.mass;
            double m2 = wd2.mass;
            double M = m1 + m2;

            Vector3 relPosDamp = wd2.position - wd1.position;
            double rDamp = sqrt((double)(relPosDamp.x * relPosDamp.x + relPosDamp.y * relPosDamp.y + relPosDamp.z * relPosDamp.z));

            const double baseK = 3e-3; 
            double scale = (rDamp > 1e-6) ? ((double)separation / rDamp) : 1.0;
            if (scale < 1.0) scale = 1.0;
            double dampingK = baseK * scale * scale;

            Vector3 vcm = (wd1.velocity * (m1 / (float)M)) + (wd2.velocity * (m2 / (float)M));
            Vector3 relVelDamp = wd2.velocity - wd1.velocity;

            Vector3 rhat = relPosDamp / (float)rDamp;
            float v_radial_mag = relVelDamp.x * rhat.x + relVelDamp.y * rhat.y + relVelDamp.z * rhat.z;
            
            // Separate components cleanly
            Vector3 v_radial = rhat * v_radial_mag;
            Vector3 v_tangent = relVelDamp - v_radial;

            // DAMP ONLY THE TANGENT PLANE FORCE (Drops orbital angular momentum, frees gravity to pull them inward)
            float dampFactor = 1.0f - (float)(dampingK * physicsDeltaTime);
            if (dampFactor < 0.0f) dampFactor = 0.0f;
            v_tangent *= dampFactor;

            Vector3 vrel_decayed = v_radial + v_tangent;

            wd1.velocity = vcm - vrel_decayed * (float)(m2 / M);
            wd2.velocity = vcm + vrel_decayed * (float)(m1 / M);
        }

        wd1.position += wd1.velocity * physicsDeltaTime;
        wd2.position += wd2.velocity * physicsDeltaTime;

        // Hard Lock to 2D Physics Plane
        wd1.position.z = 0.0f;
        wd2.position.z = 0.0f;
        wd1.velocity.z = 0.0f;
        wd2.velocity.z = 0.0f;

        // Mass Transfer evaluation
        float currentDist = Vector3Distance(wd1.position, wd2.position);
        if (currentDist < (wd1.radius + wd2.radius) * 1.3f) {
            float massTransfer = 3.0e19f * physicsDeltaTime;
            if (wd2.mass > massTransfer) {
                wd2.mass -= massTransfer;
                wd1.mass += massTransfer;
            }
            if (GetRandomValue(0, 100) < 40) {
                accretionFlow.push_back({ wd2.position, 0.0f });
            }
        }
    }

    // Process gas particles
    for (auto it = accretionFlow.begin(); it != accretionFlow.end();) {
        it->progress += 4.0f * deltaTime;
        if (it->progress >= 1.0f || objs.size() < 2) {
            it = accretionFlow.erase(it);
        } else {
            it->position = Vector3Lerp(objs[1].position, objs[0].position, it->progress);
            ++it;
        }
    }

    // Merger evaluation
    if (!pause) {
        float collisionDist = (wd1.radius + wd2.radius) * 1.02f;
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
