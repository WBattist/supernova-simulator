// Taken from kavan, translated from opengl to raylib, and heavily modified to be more physically accurate and visually appealing. 
#include "Simulation.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

extern bool pause;
extern float simulationSpeedFactor;

float separation = 12000.0f;
float phase = 0.0f;
float eccentricity = 0.25f;
float inclination = 0.0f;
float longitude = 0.0f;
// straight from web sim ...
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

    // Place the stars on the x-axis and give them the Newtonian circular-orbit velocity.
    float r = separation * (1.0f - eccentricity);
    float r1 = r * (m_WD2 / (m_WD1 + m_WD2));
    float r2 = r * (m_WD1 / (m_WD1 + m_WD2));
    float orbitalOmega = sqrtf((float)(G * (m_WD1 + m_WD2) / (r * r * r)));

    Vector3 pos1 = systemCenter + RotateOrbitPoint(Vector3{ -r1, 0.0f, 0.0f }, inclination, longitude);
    Vector3 pos2 = systemCenter + RotateOrbitPoint(Vector3{ r2, 0.0f, 0.0f }, inclination, longitude);
    Vector3 vel1 = RotateOrbitPoint(Vector3{ 0.0f, 0.0f, -orbitalOmega * r1 }, inclination, longitude);
    Vector3 vel2 = RotateOrbitPoint(Vector3{ 0.0f, 0.0f, orbitalOmega * r2 }, inclination, longitude);

// Convert custom macro color to normalized floats for shader rendering
    Vector4 normalizedColor = {
        (float)babyboybuttermybunsblue.r / 255.0f,
        (float)babyboybuttermybunsblue.g / 255.0f,
        (float)babyboybuttermybunsblue.b / 255.0f,
        (float)babyboybuttermybunsblue.a / 255.0f
    };
// HANDLES WHITE STAR CREATION WITH HIGHER DENSITY TO PREVENT UNSTABLE INITIAL CONDITIONS
    objs.push_back(CreateWhiteDwarf(pos1, vel1, m_WD1, normalizedColor, false, 5.0e8f));
    objs.push_back(CreateWhiteDwarf(pos2, vel2, m_WD2, normalizedColor));
}

void UpdatePhysics(std::vector<Object>& objs, std::vector<AccretionParticle>& accretionFlow, std::vector<DebrisParticle>& ejecta, float deltaTime, bool& triggerExplosion, Vector3& explosionPosition) {
    float physicsDeltaTime = deltaTime * SIMULATION_TIME_SCALE * simulationSpeedFactor;

    // Process ejecta particle movement if stars have already detonated
    if (objs.empty()) {
        for (auto it = ejecta.begin(); it != ejecta.end();) {
            it->position += it->velocity * physicsDeltaTime;
            it->lifeTime -= physicsDeltaTime;
            it->color.w = it->lifeTime / it->maxLifeTime;
            
            // Dynamic expansion: faster particles expand slightly quicker
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
        float softening = 1.0e6f;
        float distanceSquared = separationVector.x * separationVector.x + separationVector.y * separationVector.y + separationVector.z * separationVector.z + softening * softening;
        float distance = sqrtf(distanceSquared);
        float invDistanceCubed = 1.0f / (distanceSquared * distance);

        Vector3 accelOnWd1 = separationVector * ((float)(G * wd2.mass) * invDistanceCubed);
        Vector3 accelOnWd2 = separationVector * (-(float)(G * wd1.mass) * invDistanceCubed);

        wd1.velocity += accelOnWd1 * physicsDeltaTime;
        wd2.velocity += accelOnWd2 * physicsDeltaTime;

        // --- Gravitational-wave driven inspiral (energy loss) ---
        // Approximate orbital energy loss for near-circular binaries using Peters (1964) formula
        // Power radiated (circular orbit): P = (32/5) * G^4/(c^5) * (m1^2 * m2^2 * (m1 + m2)) / r^5
        // We convert the loss in orbital energy to a reduction in the relative velocity that
        // preserves the center-of-mass motion but decreases the orbital separation.
        {
            double m1 = wd1.mass;
            double m2 = wd2.mass;
            double M = m1 + m2;

            Vector3 relPos = wd2.position - wd1.position;
            Vector3 relVel = wd2.velocity - wd1.velocity;
            double r = sqrt((double)(relPos.x * relPos.x + relPos.y * relPos.y + relPos.z * relPos.z));

            if (r > 1e-3) {
                // circular-orbit power (SI units)
                double P = (32.0 / 5.0) * (G * G * G * G) / ( (double)c * (double)c * (double)c * (double)c * (double)c )
                           * (m1 * m1 * m2 * m2 * M) / (r * r * r * r * r);

                double dE = P * (double)physicsDeltaTime;

                // current orbital energy (negative for bound orbit): E = - G m1 m2 / (2 a) ; for circular a ~= r
                double Eold = -G * m1 * m2 / (2.0 * r);
                double Enew = Eold - dE; // energy becomes more negative

                // protect against non-physical values
                if (Enew < -1e-200) {
                    double anew = -G * m1 * m2 / (2.0 * Enew);
                    if (anew < r) {
                        // desired new relative circular speed
                        double vrel_new_mag = sqrt(G * M / anew);

                        // compute tangent direction (component of relVel perpendicular to relPos)
                        Vector3 rhat = relPos / (float)r;
                        double vdotr = relVel.x * rhat.x + relVel.y * rhat.y + relVel.z * rhat.z;
                        Vector3 vt = relVel - rhat * (float)vdotr;
                        double vtmag = sqrt((double)(vt.x * vt.x + vt.y * vt.y + vt.z * vt.z));

                        Vector3 tangent;
                        if (vtmag > 1e-9) {
                            tangent = vt / (float)vtmag;
                        } else {
                            // fallback: pick an arbitrary perpendicular vector
                            tangent = Vector3{ -rhat.z, 0.0f, rhat.x };
                            double tam = sqrt((double)(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z));
                            if (tam > 0.0) tangent = tangent / (float)tam;
                        }

                        Vector3 vrel_new = tangent * (float)vrel_new_mag;

                        // preserve center of mass velocity
                        Vector3 vcm = (wd1.velocity * (m1 / (float)M)) + (wd2.velocity * (m2 / (float)M));

                        // set new velocities such that v2 - v1 = vrel_new and m1*v1 + m2*v2 = M*vcm
                        wd1.velocity = vcm - vrel_new * (float)(m2 / M);
                        wd2.velocity = vcm + vrel_new * (float)(m1 / M);
                    }
                }
            }
        }

        // Adaptive relative-velocity damping to drive inspiral while preserving COM motion.
        // This simple approach increases damping as the separation decreases.
        {
            double m1 = wd1.mass;
            double m2 = wd2.mass;
            double M = m1 + m2;

            Vector3 relPos = wd2.position - wd1.position;
            double r = sqrt((double)(relPos.x * relPos.x + relPos.y * relPos.y + relPos.z * relPos.z));

            // base damping coefficient (tune this if it's too slow/fast)
            const double baseK = 5e-2f; // try 1e-4..1e-2
            double scale = 1.0;
            if (r > 1e-6) scale = (double)separation / r; // stronger damping as r shrinks
            if (scale < 1.0) scale = 1.0;
            double dampingK = baseK * scale * scale;

            Vector3 vcm = (wd1.velocity * (m1 / (float)M)) + (wd2.velocity * (m2 / (float)M));
            Vector3 vrel = wd2.velocity - wd1.velocity;

            float dampFactor = 1.0f - (float)(dampingK * physicsDeltaTime);
            if (dampFactor < 0.0f) dampFactor = 0.0f;
            vrel *= dampFactor;

            wd1.velocity = vcm - vrel * (float)(m2 / M);
            wd2.velocity = vcm + vrel * (float)(m1 / M);
        }

        wd1.position += wd1.velocity * physicsDeltaTime;
        wd2.position += wd2.velocity * physicsDeltaTime;

        float currentDist = Vector3Distance(wd1.position, wd2.position);
        if (currentDist < (wd1.radius + wd2.radius) * 1.2f) {
            float massTransfer = 1.0e18f * physicsDeltaTime;
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
