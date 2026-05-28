#pragma once

#include <vector>
#include "Simulation.h"

Vector3 ToRenderPosition(Vector3 meters);
float ToRenderLength(float meters);

std::vector<Vector3> CreateGridVertices(float size, int divisions);
std::vector<Vector3> UpdateGridVertices(const std::vector<Vector3>& base, const std::vector<Object>& objs);
void DrawOrbitalPaths(const std::vector<Object>& objs, float eccentricity, float inclination, float longitude);
