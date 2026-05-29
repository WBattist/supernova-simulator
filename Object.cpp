#include "Object.h"
#include <cmath>

Object::Object(Vector3 pos, Vector3 vel, float mass, float density, Vector4 col, bool glow, ObjectType type)
    : position(pos), velocity(vel), mass(mass), density(density), color(col), glow(glow), type(type) {
    radius = cbrtf((3.0f * mass) / (4.0f * 3.14159265f * density));
}

void Object::UpdatePos(float deltaTime) {
    position += velocity * deltaTime;
    radius = cbrtf((3.0f * mass) / (4.0f * 3.14159265f * density));
}

// Added missing accelerate implementation
void Object::accelerate(float x, float y, float z) {
    velocity.x += (x / 96.0f) * dt;
    velocity.y += (y / 96.0f) * dt;
    velocity.z += (z / 96.0f) * dt;
}
