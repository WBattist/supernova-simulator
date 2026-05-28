#include "Simulation.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>


#define babyboybuttermybunsblue CLITERAL(Color){ 107, 214, 250, 1 }

std::vector<Object> objs;
std::vector<DebrisParticle> ejecta;
std::vector<AccretionParticle> accretionFlow;
bool pause = true;

// Shared orbital mechanics state values
float separation = 12000.0f;
float phase = 0.0f;
float eccentricity = 0.25f;
float inclination = 0.0f; // Flattens orbit on horizontal plane
float longitude = 0.0f;   // Aligns orbit cleanly on X-axis

const char* vertexShaderSource = R"glsl(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
uniform mat4 matModel;
out float lightIntensity;
void main() {
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    vec3 worldPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
    vec3 normal = normalize(vertexPosition);
    vec3 dirToCenter = normalize(-worldPos);
    lightIntensity = max(dot(normal, dirToCenter), 0.15);
})glsl";

const char* fragmentShaderSource = R"glsl(
#version 330
in float lightIntensity;
out vec4 finalColor;
uniform vec4 objectColor;
uniform int isGrid; 
uniform int GLOW;
void main() {
    if (isGrid != 0) {
        finalColor = objectColor;
    } else if (GLOW != 0) {
        // Changed from 100000.0 to 1.5 to prevent washing out colors to pure white
        finalColor = vec4(objectColor.rgb * 1.5, objectColor.a);
    } else {
        float fade = smoothstep(0.0, 10.0, lightIntensity * 10.0);
        finalColor = vec4(objectColor.rgb * fade, objectColor.a);
    }
})glsl";

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

// Inverted gravity funnel warped on top of base coordinate plane
std::vector<Vector3> UpdateGridVertices(const std::vector<Vector3>& base, const std::vector<Object>& objs) {
    std::vector<Vector3> deformed = base;

    for (size_t i = 0; i < deformed.size(); ++i) {
        float totalDisplacementY = 0.0f;
        for (const auto& obj : objs) {
            float dx = obj.position.x - deformed[i].x;
            float dy = obj.position.y - deformed[i].y;
            float dz = obj.position.z - deformed[i].z;
            float distance = sqrtf(dx * dx + dy * dy + dz * dz);
            
            float distance_m = distance * 1000.0f; 
            float rs = (2.0f * (float)G * obj.mass) / (c * c);

            float displacementY = 0.0f;
            float diff = distance_m - rs;
            
            if (diff > 0.0f) {
                displacementY = 2.0f * sqrtf(rs * diff);
            }
            totalDisplacementY += displacementY * 2.5f; // Enhanced scale depth
        }
        
        float maxDisplacement = 1800.0f;
        if (totalDisplacementY > maxDisplacement) totalDisplacementY = maxDisplacement;

        // Subtracting displacement bends the grid downwards under each star (gravity well funnel)
        deformed[i].y = base[i].y - totalDisplacementY;
    }
    return deformed;
}

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Type Ia Supernova Binary Merger");
    SetTargetFPS(60);
    DisableCursor();

    Shader shader = LoadShaderFromMemory(vertexShaderSource, fragmentShaderSource);
    int objectColorLoc = GetShaderLocation(shader, "objectColor");
    int isGridLoc = GetShaderLocation(shader, "isGrid");
    int glowLoc = GetShaderLocation(shader, "GLOW");

    Camera3D camera = { 0 };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };

    // Restore exact original camera view
    Vector3 cameraPos = { 0.0f, 1000.0f, 5000.0f };
    float yaw = -90.0f;
    float pitch = 0.0;
    Vector3 cameraFront = { 0.0f, 0.0f, -1.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };

    Mesh sphereMesh = GenMeshSphere(1.0f, 16, 16);
    Model sphereModel = LoadModelFromMesh(sphereMesh);
    sphereModel.materials[0].shader = shader;

    const std::vector<Vector3> baseGridVertices = CreateGridVertices(20000.0f, 25);
    std::vector<Vector3> gridVertices = baseGridVertices;

    ResetToStableDoubleDegenerate(objs, ejecta, accretionFlow);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Mouse look
        Vector2 mouseDelta = GetMouseDelta();
        yaw += mouseDelta.x * 0.1f;
        pitch -= mouseDelta.y * 0.1f;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        cameraFront.x = cosf(DEG2RAD * yaw) * cosf(DEG2RAD * pitch);
        cameraFront.y = sinf(DEG2RAD * pitch);
        cameraFront.z = sinf(DEG2RAD * yaw) * cosf(DEG2RAD * pitch);
        cameraFront = Vector3Normalize(cameraFront);

        float cameraSpeed = 10000.0f * deltaTime;
        if (IsKeyDown(KEY_W)) cameraPos += cameraFront * cameraSpeed;
        if (IsKeyDown(KEY_S)) cameraPos -= cameraFront * cameraSpeed;
        Vector3 right = Vector3Normalize(Vector3CrossProduct(cameraFront, cameraUp));
        if (IsKeyDown(KEY_A)) cameraPos -= right * cameraSpeed;
        if (IsKeyDown(KEY_D)) cameraPos += right * cameraSpeed;
        if (IsKeyDown(KEY_SPACE)) cameraPos += cameraUp * cameraSpeed;
        if (IsKeyDown(KEY_LEFT_SHIFT)) cameraPos -= cameraUp * cameraSpeed;

        float scroll = GetMouseWheelMove();
        if (scroll != 0) {
            cameraPos += cameraFront * (scroll * 250000.0f * deltaTime);
        }

        camera.position = cameraPos;
        camera.target = cameraPos + cameraFront;
        camera.up = cameraUp;

        if (IsKeyPressed(KEY_K)) pause = !pause;
        if (IsKeyPressed(KEY_R)) ResetToStableDoubleDegenerate(objs, ejecta, accretionFlow);
        if (IsKeyPressed(KEY_Q)) break;

        // Physics update
        bool triggerExplosion = false;
        Vector3 explosionPosition = { 0.0f, 0.0f, 0.0f };
        UpdatePhysics(objs, accretionFlow, ejecta, deltaTime, triggerExplosion, explosionPosition);

        // Process Super-Chandrasekhar Merger Detonation [2.3, 3.1]
        if (triggerExplosion) {
            objs.clear();
            accretionFlow.clear();

            // Spawn massive, cinematic multi-spectrum shell of ejecta (1200 particles) [3.1]
            int ejectaShellCount = 1200;
            for (int k = 0; k < ejectaShellCount; k++) {
                float theta = ((float)rand() / RAND_MAX) * PI;
                float phi = ((float)rand() / RAND_MAX) * 2.0f * PI;
                Vector3 direction = { sinf(theta) * cosf(phi), sinf(theta) * sinf(phi), cosf(theta) };

                // Highly energetic velocity dispersion shell [3.1]
                float blastSpeed = 500.0f + ((float)rand() / RAND_MAX) * 6000.0f;
                DebrisParticle p;
                p.position = explosionPosition;
                p.velocity = direction * blastSpeed;

                // Color mapping: hotter blue cores transitioning to yellow, orange, and cooling red [3.1]
                float rngColor = (float)rand() / RAND_MAX;
                if (rngColor < 0.15f) {
                    p.color = Vector4{ 0.45f, 0.70f, 1.00f, 1.0f }; // Ultra-hot ionized core (Ice Blue)
                } else if (rngColor < 0.50f) {
                    p.color = Vector4{ 1.00f, 0.50f, 0.05f, 1.0f }; // Thermal shockwave (Bright Orange)
                } else if (rngColor < 0.80f) {
                    p.color = Vector4{ 1.00f, 0.85f, 0.15f, 1.0f }; // Hot plasma (Solar Yellow)
                } else {
                    p.color = Vector4{ 0.85f, 0.10f, 0.10f, 1.0f }; // Cooling expanding shells (Deep Red)
                }

                p.radius = 35.0f + ((float)rand() / RAND_MAX) * 90.0f; 
                p.maxLifeTime = 2.5f + ((float)rand() / RAND_MAX) * 4.0f; // Varied fading lifespans
                p.lifeTime = p.maxLifeTime;

                ejecta.push_back(p);
            }
        }

        gridVertices = UpdateGridVertices(baseGridVertices, objs);
        Vector3 currentCOM = CalculateBarycenter(objs);

        // Render pass
        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);
            rlSetClipPlanes(0.1, 750000.0);

            // A. Draw Grid 
            for (size_t i = 0; i < gridVertices.size(); i += 2) {
                DrawLine3D(gridVertices[i], gridVertices[i+1], ColorAlpha(WHITE, 0.25f));
            }

            // B. Draw Center of Mass Marker
            if (!objs.empty()) {
                rlBegin(RL_LINES);
                float markerSize = 250.0f;
                rlColor4f(0.2f, 1.0f, 0.2f, 0.8f);
                rlVertex3f(currentCOM.x - markerSize, currentCOM.y, currentCOM.z);
                rlVertex3f(currentCOM.x + markerSize, currentCOM.y, currentCOM.z);
                rlVertex3f(currentCOM.x, currentCOM.y - markerSize, currentCOM.z);
                rlVertex3f(currentCOM.x, currentCOM.y + markerSize, currentCOM.z);
                rlVertex3f(currentCOM.x, currentCOM.y, currentCOM.z - markerSize);
                rlVertex3f(currentCOM.x, currentCOM.y, currentCOM.z + markerSize);
                rlEnd();
            }

            // C. Draw Accretion Streams
            if (!accretionFlow.empty()) {
                int isGridValObj = 0;
                SetShaderValue(shader, isGridLoc, &isGridValObj, SHADER_UNIFORM_INT);
                int glowValObj = 1;
                SetShaderValue(shader, glowLoc, &glowValObj, SHADER_UNIFORM_INT);

                for (const auto& ap : accretionFlow) {
                    float gasColor[4] = { 1.0f, 0.7f, 0.3f, 0.9f };
                    SetShaderValue(shader, objectColorLoc, gasColor, SHADER_UNIFORM_VEC4);
                    DrawModel(sphereModel, ap.position, 15.0f, babyboybuttermybunsblue);
                }
            }

            // D. Draw Progenitors (Ice Blue and Soft Blue hot White Dwarfs)
            for (const auto& obj : objs) {
                int isGridValObj = 0;
                SetShaderValue(shader, isGridLoc, &isGridValObj, SHADER_UNIFORM_INT);
                int glowValObj = obj.glow ? 1 : 0;
                SetShaderValue(shader, glowLoc, &glowValObj, SHADER_UNIFORM_INT);
                float colorArr[4] = { obj.color.x, obj.color.y, obj.color.z, obj.color.w };
                SetShaderValue(shader, objectColorLoc, colorArr, SHADER_UNIFORM_VEC4);

                DrawModel(sphereModel, obj.position, obj.radius, babyboybuttermybunsblue);
            }

            // E. Draw Supernova Gas Shell
            if (!ejecta.empty()) {
                int isGridValObj = 0;
                SetShaderValue(shader, isGridLoc, &isGridValObj, SHADER_UNIFORM_INT);
                int glowValObj = 1;
                SetShaderValue(shader, glowLoc, &glowValObj, SHADER_UNIFORM_INT);

                for (const auto& p : ejecta) {
                    float colorArr[4] = { p.color.x, p.color.y, p.color.z, p.color.w };
                    SetShaderValue(shader, objectColorLoc, colorArr, SHADER_UNIFORM_VEC4);
                    DrawModel(sphereModel, p.position, p.radius, babyboybuttermybunsblue);
                }
            }
        EndMode3D();

        DrawFPS(10, 10);

        EndDrawing();
    }

    UnloadModel(sphereModel);
    UnloadShader(shader);
    CloseWindow();
    return 0;
}