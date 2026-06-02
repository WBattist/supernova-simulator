#include "Simulation.h"
#include "RenderHelpers.h"
#include "DebugLogger.h" // Needed to check ENABLE_LOGGING
#include "rlgl.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <cstdlib>

std::vector<Object> objs;
std::vector<DebrisParticle> ejecta;
std::vector<AccretionParticle> accretionFlow;
bool pause = true;
float simulationSpeedFactor = 150.0f;

struct Explosion {
    Vector3 position;
    float lifeRemaining;
    float maxLifeTime;
    float maxRadius;
    Vector4 color;
    float flashDuration;
    float shellThickness;
};

std::vector<Explosion> explosions;

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

  Texture2D flashbangPic = LoadTexture("eric.png");
    
    // Check if the texture successfully bound to a valid ID and has dimensions
    bool isImgValid = (flashbangPic.id > 0 && flashbangPic.width > 0);
    
    if (!isImgValid) {
        printf("[WARNING] eric.jpg could not be loaded cleanly! Check if it is in your project build output folder.\n");
    }

    bool flashActive = false;
    float flashAlpha = 0.0f;       
    bool showPicture = false;
    float pictureAlpha = 0.0f;     
    float flashbangTimer = 0.0f;   

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
        
        if (IsKeyPressed(KEY_R)) {
            ResetToStableDoubleDegenerate(objs, ejecta, accretionFlow);
            explosions.clear();
            flashActive = false;
            showPicture = false;
            flashAlpha = 0.0f;
            pictureAlpha = 0.0f;
            flashbangTimer = 0.0f;
        }
        if (IsKeyPressed(KEY_Q)) break;
        
        // --- MANUAL DEBUG TEST TRIGGER ---
        if (IsKeyPressed(KEY_E)) {
            Vector3 com = CalculateBarycenter(objs);
            Explosion ex;
            ex.position = com;
            ex.maxLifeTime = 1.6f;
            ex.lifeRemaining = ex.maxLifeTime;
            ex.maxRadius = 2.0e8f;
            ex.color = Vector4{ 1.0f, 0.85f, 0.35f, 1.0f };
            ex.flashDuration = 0.25f;
            ex.shellThickness = 0.12f;
            explosions.push_back(ex);

            // Trigger flashbang overlays
            flashActive = true;
            flashAlpha = 1.0f;
            flashbangTimer = 0.0f;
            showPicture = false;
            pictureAlpha = 0.0f;
        }

        bool triggerExplosion = false;
        Vector3 explosionPosition = { 0.0f, 0.0f, 0.0f };
        UpdatePhysics(objs, accretionFlow, ejecta, deltaTime, triggerExplosion, explosionPosition);

        if (triggerExplosion) {
            objs.clear();
            accretionFlow.clear();

            Vector2 flatExplosion = Vector2{ explosionPosition.x, explosionPosition.z };
            Vector3 tiltedExplosion = OrbitLocalToWorld(flatExplosion, Vector3{0,0,0}, inclination, longitude);

            Explosion ex;
            ex.position = tiltedExplosion;
            ex.maxLifeTime = 4.0f; 
            ex.lifeRemaining = ex.maxLifeTime;
            ex.maxRadius = 5.0e9f; 
            ex.color = Vector4{ 1.0f, 0.95f, 0.9f, 1.0f };
            ex.flashDuration = 0.25f; 
            ex.shellThickness = 0.12f; 
            explosions.push_back(ex);

            int ejectaShellCount = 1200;
            for (int k = 0; k < ejectaShellCount; k++) {
                float theta = ((float)rand() / RAND_MAX) * PI;
                float phi = ((float)rand() / RAND_MAX) * 2.0f * PI;
                Vector3 direction = { sinf(theta) * cosf(phi), sinf(theta) * sinf(phi), cosf(theta) };

                float blastSpeed = 1.0e7f + ((float)rand() / RAND_MAX) * 1.0e7f;
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

        // --- AUTOMATIC SCALE INTERSECTION DETECTION ---
        if (!explosions.empty() && !flashActive && !showPicture) {
            for (const auto& ex : explosions) {
                float elapsed = ex.maxLifeTime - ex.lifeRemaining;
                float progress = elapsed / ex.maxLifeTime;
                float currentProgress = fminf(fmaxf(progress, 0.0f), 1.0f);
                
                float currentRadiusRender = ToRenderLength(ex.maxRadius * currentProgress);
                float distToCam = Vector3Distance(camera.position, ToRenderPosition(ex.position));

                if (distToCam <= (currentRadiusRender * 10.0f)) { 
                    flashActive = true;
                    flashAlpha = 1.0f; 
                    flashbangTimer = 0.0f;
                    showPicture = false;
                    pictureAlpha = 0.0f;
                    break;
                }
            }
        }

        // --- STATE CONTROLLER ENGINE ---
        if (flashActive) {
            flashbangTimer += deltaTime;
            if (flashbangTimer < 0.60f) {
                flashAlpha = 1.0f; 
            } else if (flashbangTimer < 1.60f) {
                float blendRatio = (flashbangTimer - 0.60f) / 1.0f;
                flashAlpha = 1.0f - blendRatio;
                showPicture = true;
                pictureAlpha = blendRatio; 
            } else {
                flashActive = false;
                flashAlpha = 0.0f;
                showPicture = true;
                pictureAlpha = 1.0f;
            }
        } else if (showPicture) {
            flashbangTimer += deltaTime;
            if (flashbangTimer > 4.5f) { 
                pictureAlpha -= deltaTime * 1.2f; 
                if (pictureAlpha <= 0.0f) {
                    pictureAlpha = 0.0f;
                    showPicture = false; 
                }
            }
        }

        // Update explosions
        for (auto it = explosions.begin(); it != explosions.end(); ) {
            it->lifeRemaining -= deltaTime;
            if (it->lifeRemaining <= 0.0f) it = explosions.erase(it);
            else ++it;
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

            if (!explosions.empty()) {
                rlDisableDepthTest();
                rlDisableDepthMask();
                BeginBlendMode(BLEND_ADDITIVE);

                int isGridValExpl = 0;
                SetShaderValue(shader, isGridLoc, &isGridValExpl, SHADER_UNIFORM_INT);
                int glowValExpl = 1;
                SetShaderValue(shader, glowLoc, &glowValExpl, SHADER_UNIFORM_INT);

                for (const auto& ex : explosions) {
                    float elapsed = ex.maxLifeTime - ex.lifeRemaining;
                    float progress = elapsed / ex.maxLifeTime;

                    if (elapsed <= ex.flashDuration) {
                        float flashT = elapsed / ex.flashDuration;
                        float flashAlphaVal = 1.0f - flashT;
                        float flashScale = ToRenderLength(ex.maxRadius * 0.05f) * (1.0f + 8.0f * flashT);
                        float flashColor[4] = { 1.0f, 1.0f, 1.05f, flashAlphaVal };
                        SetShaderValue(shader, objectColorLoc, flashColor, SHADER_UNIFORM_VEC4);
                        DrawModel(sphereModel, ToRenderPosition(ex.position), fmaxf(flashScale, 0.5f), babyboybuttermybunsblue);
                    }

                    float shellProgress = fminf(fmaxf(progress, 0.0f), 1.0f);
                    float radius = ex.maxRadius * shellProgress;
                    float innerRadius = radius * (1.0f - ex.shellThickness);
                    float shellAlpha = fmaxf(0.0f, 1.0f - shellProgress);

                    Vector4 col;
                    if (shellProgress < 0.25f) {
                        float t = shellProgress / 0.25f;
                        col = Vector4{ 1.0f, 1.0f - 0.1f * t, 1.0f, shellAlpha };
                    } else if (shellProgress < 0.6f) {
                        float t = (shellProgress - 0.25f) / (0.35f);
                        col = Vector4{ 1.0f, 0.85f - 0.35f * t, 0.4f - 0.15f * t, shellAlpha };
                    } else {
                        float t = (shellProgress - 0.6f) / 0.4f;
                        col = Vector4{ 1.0f - 0.2f * t, 0.5f - 0.5f * t, 0.25f - 0.25f * t, shellAlpha * (1.0f - 0.2f * t) };
                    }

                    float colorArr[4] = { col.x, col.y, col.z, col.w };
                    SetShaderValue(shader, objectColorLoc, colorArr, SHADER_UNIFORM_VEC4);

                    float outerScale = fmaxf(ToRenderLength(radius), 0.5f);
                    float innerScale = fmaxf(ToRenderLength(innerRadius), 0.4f);
                    
                    DrawModel(sphereModel, ToRenderPosition(ex.position), outerScale, babyboybuttermybunsblue);
                    
                    float innerColor[4] = { col.x * 0.6f, col.y * 0.6f, col.z * 0.6f, col.w * 0.6f };
                    SetShaderValue(shader, objectColorLoc, innerColor, SHADER_UNIFORM_VEC4);
                    DrawModel(sphereModel, ToRenderPosition(ex.position), innerScale, babyboybuttermybunsblue);
                }

                EndBlendMode();
                rlEnableDepthMask();
                rlEnableDepthTest();
            }

            rlDisableDepthTest();
            rlDisableDepthMask();
            DrawOrbitalPaths(objs, eccentricity, inclination, longitude);
            rlEnableDepthMask();
            rlEnableDepthTest();
        EndMode3D();

        DrawFPS(10, 10);
        DrawText(TextFormat("Sim speed: x%.2f", simulationSpeedFactor), 10, 30, 20, RAYWHITE);

        // --- 2D CANVAS DRAW OVERLAYS ---
        if (showPicture) {
            Color picColor = ColorAlpha(WHITE, pictureAlpha);
            if (isImgValid) {
                DrawTexturePro(
                    flashbangPic, 
                    Rectangle{ 0.0f, 0.0f, (float)flashbangPic.width, (float)flashbangPic.height },
                    Rectangle{ 0.0f, 0.0f, (float)screenWidth, (float)screenHeight },
                    Vector2{ 0.0f, 0.0f }, 0.0f, picColor
                );
            } else {
                // Purple fallback so you can check if it's hitting this code path
                DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(PURPLE, pictureAlpha));
                DrawText("IMAGE 'eric.jpg' NOT FOUND", screenWidth/2 - 120, screenHeight/2 - 10, 20, WHITE);
                DrawText("Make sure it's in the directory where you run the executable!", screenWidth/2 - 200, screenHeight/2 + 20, 14, LIGHTGRAY);
            }
        }

        if (flashActive) {
            DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(WHITE, flashAlpha));
        }

        EndDrawing();
    }

    if (isImgValid) UnloadTexture(flashbangPic); 
    UnloadModel(sphereModel);
    UnloadShader(shader);
    CloseWindow();
    return 0;
}