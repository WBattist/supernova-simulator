#include "Simulation.h"
#include "RenderHelpers.h"
#include "DebugLogger.h" // Needed to check ENABLE_LOGGING
#include "rlgl.h"
#include <cstdio>

std::vector<Object> objs;
std::vector<DebrisParticle> ejecta;
std::vector<AccretionParticle> accretionFlow;
bool pause = true;
float simulationSpeedFactor = 150.0f;

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
        finalColor = vec4(objectColor.rgb * 1.5, objectColor.a);
    } else {
        float fade = smoothstep(0.0, 10.0, lightIntensity * 10.0);
        finalColor = vec4(objectColor.rgb * fade, objectColor.a);
    }
})glsl";

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

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "dr brown sim");
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
        if (IsKeyPressed(KEY_EQUAL)) simulationSpeedFactor = fminf(simulationSpeedFactor * 1.25f, 200.0f);
        if (IsKeyPressed(KEY_MINUS)) simulationSpeedFactor = fmaxf(simulationSpeedFactor / 1.25f, 0.01f);
        if (IsKeyPressed(KEY_R)) ResetToStableDoubleDegenerate(objs, ejecta, accretionFlow);
        if (IsKeyPressed(KEY_Q)) break;

        bool triggerExplosion = false;
        Vector3 explosionPosition = { 0.0f, 0.0f, 0.0f };
        UpdatePhysics(objs, accretionFlow, ejecta, deltaTime, triggerExplosion, explosionPosition);

        if (triggerExplosion) {
            objs.clear();
            accretionFlow.clear();

            Vector2 flatExplosion = Vector2{ explosionPosition.x, explosionPosition.z };
            Vector3 tiltedExplosion = OrbitLocalToWorld(flatExplosion, Vector3{0,0,0}, inclination, longitude);

            int ejectaShellCount = 1200;
            for (int k = 0; k < ejectaShellCount; k++) {
                float theta = ((float)rand() / RAND_MAX) * PI;
                float phi = ((float)rand() / RAND_MAX) * 2.0f * PI;
                Vector3 direction = { sinf(theta) * cosf(phi), sinf(theta) * sinf(phi), cosf(theta) };

                float blastSpeed = 500.0f + ((float)rand() / RAND_MAX) * 6000.0f;
                DebrisParticle p;
                p.position = tiltedExplosion;
                p.velocity = direction * blastSpeed;

                float rngColor = (float)rand() / RAND_MAX;
                if (rngColor < 0.15f) p.color = Vector4{ 0.45f, 0.70f, 1.00f, 1.0f };
                else if (rngColor < 0.50f) p.color = Vector4{ 1.00f, 0.50f, 0.05f, 1.0f };
                else if (rngColor < 0.80f) p.color = Vector4{ 1.00f, 0.85f, 0.15f, 1.0f };
                else p.color = Vector4{ 0.85f, 0.10f, 0.10f, 1.0f };

                p.radius = 35.0f + ((float)rand() / RAND_MAX) * 90.0f; 
                p.maxLifeTime = 2.5f + ((float)rand() / RAND_MAX) * 4.0f;
                p.lifeTime = p.maxLifeTime;

                ejecta.push_back(p);
            }
        }

        gridVertices = UpdateGridVertices(baseGridVertices, objs);
        Vector3 currentCOM = ToRenderPosition(CalculateBarycenter(objs));

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);
            rlSetClipPlanes(0.1, 750000.0);

            for (size_t i = 0; i < gridVertices.size(); i += 2) {
                DrawLine3D(gridVertices[i], gridVertices[i+1], ColorAlpha(WHITE, 0.25f));
            }

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

            if (!accretionFlow.empty()) {
                int isGridValObj = 0;
                SetShaderValue(shader, isGridLoc, &isGridValObj, SHADER_UNIFORM_INT);
                int glowValObj = 1;
                SetShaderValue(shader, glowLoc, &glowValObj, SHADER_UNIFORM_INT);

                for (const auto& ap : accretionFlow) {
                    float gasColor[4] = { 1.0f, 0.7f, 0.3f, 0.9f };
                    SetShaderValue(shader, objectColorLoc, gasColor, SHADER_UNIFORM_VEC4);

                    DrawModel(sphereModel, ToRenderPosition(ap.position), 0.05f, babyboybuttermybunsblue);
                }
            }

            for (const auto& obj : objs) {
                int isGridValObj = 0;
                SetShaderValue(shader, isGridLoc, &isGridValObj, SHADER_UNIFORM_INT);
                int glowValObj = obj.glow ? 1 : 0;
                SetShaderValue(shader, glowLoc, &glowValObj, SHADER_UNIFORM_INT);
                float colorArr[4] = { obj.color.x, obj.color.y, obj.color.z, obj.color.w };
                SetShaderValue(shader, objectColorLoc, colorArr, SHADER_UNIFORM_VEC4);

                DrawModel(sphereModel, ToRenderPosition(obj.position), ToRenderLength(obj.radius), babyboybuttermybunsblue);
            }

            if (!ejecta.empty()) {
                int isGridValObj = 0;
                SetShaderValue(shader, isGridLoc, &isGridValObj, SHADER_UNIFORM_INT);
                int glowValObj = 1;
                SetShaderValue(shader, glowLoc, &glowValObj, SHADER_UNIFORM_INT);

                for (const auto& p : ejecta) {
                    float colorArr[4] = { p.color.x, p.color.y, p.color.z, p.color.w };
                    SetShaderValue(shader, objectColorLoc, colorArr, SHADER_UNIFORM_VEC4);
                    DrawModel(sphereModel, ToRenderPosition(p.position), fmaxf(ToRenderLength(p.radius), 1.0f), babyboybuttermybunsblue);
                }
            }

            rlDisableDepthTest();
            rlDisableDepthMask();
            DrawOrbitalPaths(objs, eccentricity, inclination, longitude);
            rlEnableDepthMask();
            rlEnableDepthTest();
        EndMode3D();

        DrawFPS(10, 10);
        DrawText(TextFormat("Sim speed x%.2f", simulationSpeedFactor), 10, 30, 20, RAYWHITE);
        DrawText("K pause | R reset | +/- sim speed", 10, 52, 18, RAYWHITE);

        EndDrawing();
    }

    UnloadModel(sphereModel);
    UnloadShader(shader);
    CloseWindow();
    return 0;
}