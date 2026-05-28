#pragma once
#include "Types.h"

class Object {
public:
    Vector3 position;
    Vector3 velocity;
    Vector4 color;
    ObjectType type;
    float mass;
    float density;
    float radius;
    bool glow;

    Object(Vector3 pos, Vector3 vel, float mass, float density = 5515.0f, Vector4 col = { 1.0f, 0.0f, 0.0f, 1.0f }, bool glow = false, ObjectType type = OBJ_GENERIC);
    void UpdatePos(float deltaTime);
    void accelerate(float x, float y, float z); // Added missing declaration
};