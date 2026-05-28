#include "Object.h"
#include <cmath>

Object::Object(Vector3 pos, Vector3 vel, float mass, float density, Vector4 col, bool glow, ObjectType type)
    : position(pos), velocity(vel), mass(mass), density(density), color(col), glow(glow), type(type) {
    radius = powf(((3.0f * mass / density) / (4.0f * 3.14159265f)), 1.0f / 3.0f) / 30000.0f;
}

void Object::UpdatePos(float deltaTime) {
    position.x += velocity.x / 94.0f;
    position.y += velocity.y / 94.0f;
    position.z += velocity.z / 94.0f;
    radius = powf(((3.0f * mass / density) / (4.0f * 3.14159265f)), 1.0f / 3.0f) / 30000.0f;
}

// Added missing accelerate implementation
void Object::accelerate(float x, float y, float z) {
    velocity.x += x / 96.0f;
    velocity.y += y / 96.0f;
    velocity.z += z / 96.0f;
}