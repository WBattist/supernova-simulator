#pragma once
#include <vector>
#include "Object.h"

extern float simulationSpeedFactor;

Object CreateWhiteDwarf(Vector3 position, Vector3 velocity, float mass, Vector4 color, bool glow = false, float density = WHITE_DWARF_DENSITY);

Vector3 CalculateBarycenter(const std::vector<Object>& bodies);
void ResetToStableDoubleDegenerate(std::vector<Object>& objs, std::vector<DebrisParticle>& ejecta, std::vector<AccretionParticle>& accretionFlow);
void UpdatePhysics(std::vector<Object>& objs, std::vector<AccretionParticle>& accretionFlow, std::vector<DebrisParticle>& ejecta, float deltaTime, bool& triggerExplosion, Vector3& explosionPosition);