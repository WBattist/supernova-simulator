#include "RenderHelpers.h"

#include <cmath>

Vector3 ToRenderPosition(Vector3 meters) {
    return meters / METERS_PER_RENDER_UNIT;
}

float ToRenderLength(float meters) {
    return (meters / METERS_PER_RENDER_UNIT) * STAR_RENDER_SCALE;
}

std::vector<Vector3> CreateGridVertices(float size, int divisions) {
    std::vector<Vector3> vertices;
    float step = size / divisions;
    float halfSize = size / 2.0f;

    for (int yStep = 3; yStep <= 3; ++yStep) {
        float y = -halfSize * 0.3f + yStep * step;
        for (int zStep = 0; zStep <= divisions; ++zStep) {
            float z = -halfSize + zStep * step;
            for (int xStep = 0; xStep < divisions; ++xStep) {
                float xStart = -halfSize + xStep * step;
                float xEnd = xStart + step;
                vertices.push_back(Vector3{ xStart, y, z });
                vertices.push_back(Vector3{ xEnd, y, z });
            }
        }
    }
    for (int xStep = 0; xStep <= divisions; ++xStep) {
        float x = -halfSize + xStep * step;
        for (int yStep = 3; yStep <= 3; ++yStep) {
            float y = -halfSize * 0.3f + yStep * step;
            for (int zStep = 0; zStep < divisions; ++zStep) {
                float zStart = -halfSize + zStep * step;
                float zEnd = zStart + step;
                vertices.push_back(Vector3{ x, y, zStart });
                vertices.push_back(Vector3{ x, y, zEnd });
            }
        }
    }
    return vertices;
}

std::vector<Vector3> UpdateGridVertices(const std::vector<Vector3>& base, const std::vector<Object>& objs) {
    std::vector<Vector3> deformed = base;

    for (size_t i = 0; i < deformed.size(); ++i) {
        float totalDisplacementY = 0.0f;
        float gridX = base[i].x * METERS_PER_RENDER_UNIT;
        float gridZ = base[i].z * METERS_PER_RENDER_UNIT;

        for (const auto& obj : objs) {
            float dx = obj.position.x - gridX;
            float dz = obj.position.z - gridZ;
            float distance = sqrtf(dx * dx + dz * dz);

            float schwarzschildRadius = (2.0f * (float)G * obj.mass) / (c * c);
            float softening = fmaxf(obj.radius, schwarzschildRadius);
            float displacementMeters = GRID_WARP_SCALE * schwarzschildRadius * (softening / (distance + softening));
            float displacementY = displacementMeters / METERS_PER_RENDER_UNIT;

            totalDisplacementY += displacementY;
        }

        float maxDisplacement = 1800.0f;
        if (totalDisplacementY > maxDisplacement) totalDisplacementY = maxDisplacement;

        deformed[i].y = base[i].y - totalDisplacementY;
    }

    return deformed;
}
