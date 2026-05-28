#pragma once
#include <vector>
#include "Object.h"

Vector3 CalculateBarycenter(const std::vector<Object>& bodies);
void ResetToStableDoubleDegenerate(std::vector<Object>& objs, std::vector<DebrisParticle>& ejecta, std::vector<AccretionParticle>& accretionFlow);
void UpdatePhysics(std::vector<Object>& objs, std::vector<AccretionParticle>& accretionFlow, std::vector<DebrisParticle>& ejecta, float deltaTime, bool& triggerExplosion, Vector3& explosionPosition);