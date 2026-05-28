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
const float CHANDRASEKHAR_LIMIT = 1.44f * M_SUN;
const float METERS_PER_RENDER_UNIT = 1.0e6f;
const float SIMULATION_TIME_SCALE = 250.0f;
const float GRID_WARP_SCALE = 5.0e7f;
const float WHITE_DWARF_DENSITY = 1.0e9f;
const float STAR_RENDER_SCALE = 20.0f;

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