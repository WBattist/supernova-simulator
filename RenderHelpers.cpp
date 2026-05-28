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
// straight from web sim ...

static Vector3 OrbitLocalToWorld(Vector2 local, Vector3 center, float inclinationDegrees, float longitudeDegrees) {
    float incRad = inclinationDegrees * (PI / 180.0f);
    float lonRad = longitudeDegrees * (PI / 180.0f);

    float cosInc = cosf(incRad);
    float sinInc = sinf(incRad);
    float cosLon = cosf(lonRad);
    float sinLon = sinf(lonRad);

    Vector3 point = { local.x, 0.0f, local.y };
    return center + Vector3{
        point.x * cosLon - point.z * sinLon * cosInc,
        point.z * sinInc,
        point.x * sinLon + point.z * cosLon * cosInc
    };
}
// from web sim
static void DrawQuadraticOrbitSegment(Vector2 start, Vector2 control, Vector2 end, Vector3 center, float inclinationDegrees, float longitudeDegrees, Color color) {
    const int subdivisions = 24;
    Vector2 previous = start;
    const float pathRadius = 0.0f;

    for (int i = 1; i <= subdivisions; ++i) {
        float t = (float)i / (float)subdivisions;
        float invT = 1.0f - t;

        Vector2 current = {
            invT * invT * start.x + 2.0f * invT * t * control.x + t * t * end.x,
            invT * invT * start.y + 2.0f * invT * t * control.y + t * t * end.y
        };

        DrawCylinderEx(
            OrbitLocalToWorld(previous, center, inclinationDegrees, longitudeDegrees),
            OrbitLocalToWorld(current, center, inclinationDegrees, longitudeDegrees),
            pathRadius,
            pathRadius,
            6,
            color
        );

        previous = current;
    }
}

void DrawOrbitalPaths(const std::vector<Object>& objs, float eccentricity, float inclination, float longitude) {
    if (objs.size() < 2) {
        return;
    }

    float totalMass = objs[0].mass + objs[1].mass;
    if (totalMass <= 0.0f) {
        return;
    }

    Vector3 center = ToRenderPosition(CalculateBarycenter(objs));
    float separationMeters = Vector3Distance(objs[0].position, objs[1].position);

    float a1 = separationMeters * (objs[1].mass / totalMass);
    float a2 = separationMeters * (objs[0].mass / totalMass);

    float e = fminf(fmaxf(eccentricity, 0.0f), 0.999f);
    float s = 1.0f / METERS_PER_RENDER_UNIT;

    float k = sqrtf(1.0f - e * e);
    float b1 = a1 * k;
    float b2 = a2 * k;

    float aa1 = s * a1;
    float ab1 = s * b1;
    float aa2 = s * a2;
    float ab2 = s * b2;

    const int n = 12;
    const float step = (2.0f * PI) / (float)n;

    float caScale = 1.0f / cosf(step / 2.0f);
    float ca1 = aa1 * caScale;
    float cb1 = ab1 * caScale;
    float ca2 = aa2 * caScale;
    float cb2 = ab2 * caScale;

    float dx1 = aa1 * e;
    float dx2 = -aa2 * e;

    float aAngle = 0.0f;
    float cAngle = -step / 2.0f;

    Vector2 path1Start = { -aa1 * cosf(aAngle) + dx1, ab1 * sinf(aAngle) };
    Vector2 path2Start = { aa2 * cosf(aAngle) + dx2, ab2 * sinf(aAngle) };

    Vector2 previous1 = path1Start;
    Vector2 previous2 = path2Start;

    Color pathColor = ColorAlpha(SKYBLUE, 0.95f);

    for (int i = 0; i < n; ++i) {
        aAngle += step;
        cAngle += step;

        float ccA = cosf(cAngle);
        float scA = sinf(cAngle);
        float caA = cosf(aAngle);
        float saA = sinf(aAngle);

        Vector2 control1 = { -ca1 * ccA + dx1, cb1 * scA };
        Vector2 end1 = { -aa1 * caA + dx1, ab1 * saA };

        Vector2 control2 = { ca2 * ccA + dx2, cb2 * scA };
        Vector2 end2 = { aa2 * caA + dx2, ab2 * saA };

        DrawQuadraticOrbitSegment(previous1, control1, end1, center, inclination, longitude, pathColor);
        DrawQuadraticOrbitSegment(previous2, control2, end2, center, inclination, longitude, pathColor);

        previous1 = end1;
        previous2 = end2;
    }
}
