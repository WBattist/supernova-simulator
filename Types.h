#pragma once
#define RAYMATH_DISABLE_CPP_OPERATORS // Prevent duplicate C++ operator overrides
#include "raylib.h"
#include "raymath.h"

// Custom light blue stellar color definition (Alpha set to 255 to ensure full visibility)
#define babyboybuttermybunsblue CLITERAL(Color){ 107, 214, 250, 255 }

// Original scale constants
const double G = 6.6743e-11;
const float c = 299792458.0f;
const float M_SUN = 1.989e30f; 
const float CHANDRASEKHAR_LIMIT = 3.5e25f; // combined limit within original coordinates scale

enum ObjectType {
    OBJ_GENERIC,
    OBJ_WHITE_DWARF
};

struct DebrisParticle {
    Vector3 position;
    Vector3 velocity;
    Vector4 color;
    float radius;
    float lifeTime;
    float maxLifeTime;
};

struct AccretionParticle {
    Vector3 position;
    float progress;
};

// Vector3 Math Operator Overloads
inline Vector3 operator+(Vector3 a, Vector3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vector3 operator-(Vector3 a, Vector3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vector3 operator*(Vector3 a, float b) { return { a.x * b, a.y * b, a.z * b }; }
inline Vector3 operator/(Vector3 a, float b) { return { a.x / b, a.y / b, a.z / b }; }
inline Vector3& operator+=(Vector3& a, Vector3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline Vector3& operator-=(Vector3& a, Vector3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline Vector3& operator*=(Vector3& a, float b) { a.x *= b; a.y *= b; a.z *= b; return a; }