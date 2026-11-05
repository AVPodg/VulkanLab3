#include "lab3_application.hpp"
#include "textured_renderer.hpp"
#include <imgui.h>
#include <iostream>

namespace lab3 {
namespace {
texturing::State state;
double lastTime = -1;
float panelWidth = 390.f;
void textureControls() {
    auto& t = state.texture;
    ImGui::Combo("Texture", &t.material, texturing::materialNames, texturing::materialCount);
    ImGui::Checkbox("Animate texture effects", &state.animateTextures);
    if (ImGui::Button("Restart texture effects")) state.textureTime = 0.0;
    ImGui::Checkbox("Animated sampling", &t.effect);
    ImGui::SliderFloat("Effect amount", &t.effectAmount, 0.f, 1.f);
    ImGui::SliderFloat("Effect speed", &t.effectSpeed, 0.f, 3.f);
    ImGui::Checkbox("Specular map", &t.specularMap);
    ImGui::Checkbox("Emissive map", &t.emissiveMap);
    ImGui::SliderFloat("Emission", &t.emission, 0.f, 5.f);
}
void orbitCamera(float yaw, float elevation, float distance) {
    scene::Settings orbit;
    orbit.cameraYaw = yaw;
    orbit.cameraElevation = elevation;
    orbit.cameraDistance = distance;
    state.camera.position = glm::vec3(glm::inverse(scene::view(orbit))[3]);
    state.camera.yaw = std::remainder(-90.f - yaw, 360.f);
    state.camera.pitch = -elevation;
}
void orbitControls() {
    auto& c = state.camera;
    float distance = std::max(glm::length(c.position), 0.001f);
    float yaw = glm::degrees(std::atan2(c.position.x, c.position.z));
    float elevation = glm::degrees(std::asin(std::clamp(c.position.y / distance, -1.f, 1.f)));
    bool changed = ImGui::SliderFloat("Orbit yaw", &yaw, -180.f, 180.f, "%.1f deg");
    changed |= ImGui::SliderFloat("Orbit elevation", &elevation, -80.f, 80.f, "%.1f deg");
    changed |= ImGui::SliderFloat("Distance", &distance, 3.f, 25.f);
    if (changed) orbitCamera(yaw, elevation, distance);
    ImGui::TextDisabled("Orbit around the scene origin: Alt + RMB");
}
void materialControls(const char* name, lighting::Material& m) {
    ImGui::PushID(name);
    ImGui::ColorEdit3("Albedo", &m.albedo.x);
    ImGui::ColorEdit3("Specular color", &m.specular.x);
    ImGui::SliderFloat("Shininess", &m.shininess, 1.f, 256.f, "%.0f");
    ImGui::Checkbox("Procedural vertex colors", &m.vertexColors);
    ImGui::PopID();
}
void cameraControls() {
    auto& c = state.camera;
    ImGui::DragFloat3("Camera position", &c.position.x, 0.05f);
    ImGui::SliderFloat("Yaw", &c.yaw, -180.f, 180.f, "%.1f deg");
    ImGui::SliderFloat("Pitch", &c.pitch, -89.f, 89.f, "%.1f deg");
    ImGui::SliderFloat("Roll", &c.roll, -180.f, 180.f, "%.1f deg");
    ImGui::Checkbox("Perspective", &c.perspective);
    ImGui::SliderFloat("FOV", &c.fov, 15.f, 110.f, "%.0f deg");
    if (!c.perspective) ImGui::SliderFloat("Ortho height", &c.orthoHeight, 1.f, 30.f);
    ImGui::SliderFloat("Near", &c.nearPlane, 0.01f, 2.f);
    ImGui::SliderFloat("Far", &c.farPlane, 10.f, 300.f);
    ImGui::SliderFloat("Move speed", &c.speed, 0.5f, 15.f);
    orbitControls();
    if (ImGui::Button("Reset camera")) c = {};
}
void objectControls() {
    auto& s = state.object;
    ImGui::DragFloat3("Position", &s.position.x, 0.03f);
    ImGui::DragFloat3("Rotation", &s.rotation.x, 0.5f);
    ImGui::DragFloat3("Scale", &s.scale.x, 0.01f, 0.1f, 3.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::ColorEdit3("Tint", &s.tint.x);
    if (ImGui::SmallButton("White tint")) s.tint = glm::vec3(1.f);
    ImGui::TextWrapped("Vertex RGB comes from local XYZ; tint and albedo multiply these colors before lighting.");
    materialControls("pyramid",state.material);
    ImGui::Checkbox("Enable trajectory", &s.animate);
    if (ImGui::Button(s.playing ? "Pause" : "Play")) s.playing = !s.playing;
    ImGui::SameLine();
    if (ImGui::Button("Restart")) s.animationTime = 0;
    ImGui::SliderFloat("Speed", &s.speed, 0.1f, 3.f);
    ImGui::SliderFloat("Radius", &s.radius, 0.f, 3.f);
    ImGui::SliderFloat("Height", &s.height, 0.f, 2.f);
    ImGui::SliderFloat("Spin", &s.spin, -180.f, 180.f);
    ImGui::TextDisabled("3D figure-eight | time %.2f s", s.animationTime);
}
void pointControls() {
    if (ImGui::Button("Add point light")) state.points.push_back({});
    for (size_t i = 0; i < state.points.size();) {
        ImGui::PushID(static_cast<int>(i));
        bool remove = false;
        if (ImGui::TreeNode("Point", "Point %d", static_cast<int>(i+1))) {
            auto& p = state.points[i];
            ImGui::Checkbox("Enabled", &p.enabled);
            ImGui::DragFloat3("Position", &p.position.x, 0.05f);
            ImGui::ColorEdit3("Color", &p.color.x);
            ImGui::SliderFloat("Intensity", &p.intensity, 0.f, 150.f);
            remove = ImGui::Button("Delete point");
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (remove) state.points.erase(state.points.begin()+i); else ++i;
    }
}
void spotControls() {
    if (ImGui::Button("Add spotlight")) state.spots.push_back({});
    for (size_t i = 0; i < state.spots.size();) {
        ImGui::PushID(static_cast<int>(i));
        bool remove = false;
        if (ImGui::TreeNode("Spot", "Spot %d", static_cast<int>(i+1))) {
            auto& p = state.spots[i];
            ImGui::Checkbox("Enabled", &p.enabled);
            ImGui::DragFloat3("Position", &p.position.x, 0.05f);
            ImGui::DragFloat3("Direction", &p.direction.x, 0.01f, -1.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::TextDisabled("Direction points FROM the light. Zero = down.");
            ImGui::ColorEdit3("Color", &p.color.x);
            ImGui::SliderFloat("Intensity", &p.intensity, 0.f, 200.f);
            ImGui::SliderFloat("Inner angle", &p.innerAngle, 0.f, 80.f, "%.1f deg");
            ImGui::SliderFloat("Outer angle", &p.outerAngle, p.innerAngle, 80.f, "%.1f deg");
            p.outerAngle = std::max(p.innerAngle,p.outerAngle);
            ImGui::TextDisabled("Half angles; equal = hard edge");
            remove = ImGui::Button("Delete spotlight");
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (remove) state.spots.erase(state.spots.begin()+i); else ++i;
    }
}
void movement(float dt) {
    const auto& io = ImGui::GetIO();
    auto& c = state.camera;
    if (!io.WantCaptureMouse && io.MousePos.x > panelWidth) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            if (io.KeyAlt) {
                float distance = std::max(glm::length(c.position), 0.001f);
                float yaw = glm::degrees(std::atan2(c.position.x, c.position.z));
                float elevation = glm::degrees(std::asin(std::clamp(c.position.y / distance, -1.f, 1.f)));
                orbitCamera(yaw + io.MouseDelta.x * 0.35f,
                    std::clamp(elevation + io.MouseDelta.y * 0.35f, -80.f, 80.f), distance);
            } else {
                c.yaw += io.MouseDelta.x * 0.25f;
                c.yaw = std::remainder(c.yaw,360.f);
                c.pitch = std::clamp(c.pitch-io.MouseDelta.y*0.25f,-89.f,89.f);
            }
        }
        if (io.KeyAlt && c.perspective) {
            float distance = glm::length(c.position);
            if (distance > 0.001f)
                c.position *= std::clamp(distance-io.MouseWheel*0.5f,3.f,25.f) / distance;
        }
        else if (c.perspective) c.fov = std::clamp(c.fov-io.MouseWheel*2.f,15.f,110.f);
        else c.orthoHeight = std::clamp(c.orthoHeight-io.MouseWheel*0.5f,1.f,30.f);
    }
    if (io.WantCaptureKeyboard) return;
    glm::vec3 f = lighting::forward(c), right = glm::normalize(glm::cross(f,glm::vec3(0,1,0)));
    float step = c.speed * dt * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.f : 1.f);
    if (ImGui::IsKeyDown(ImGuiKey_W)) c.position += f*step;
    if (ImGui::IsKeyDown(ImGuiKey_S)) c.position -= f*step;
    if (ImGui::IsKeyDown(ImGuiKey_D)) c.position += right*step;
    if (ImGui::IsKeyDown(ImGuiKey_A)) c.position -= right*step;
    if (ImGui::IsKeyDown(ImGuiKey_E)) c.position.y += step;
    if (ImGui::IsKeyDown(ImGuiKey_Q)) c.position.y -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_Space,false)) state.object.playing = !state.object.playing;
    if (ImGui::IsKeyPressed(ImGuiKey_R,false)) state = {};
}
}
bool initialize(const std::filesystem::path& directory) {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowPadding = {16.f, 14.f};
    style.FramePadding = {7.f, 4.f};
    style.ItemSpacing = {7.f, 7.f};
    style.FrameRounding = style.GrabRounding = 4.f;
    style.Colors[ImGuiCol_WindowBg] = {0.055f, 0.065f, 0.09f, 1.f};
    style.Colors[ImGuiCol_Header] = {0.12f, 0.20f, 0.30f, 1.f};
    style.Colors[ImGuiCol_Button] = {0.12f, 0.30f, 0.48f, 1.f};
    try { texturing::initializeRenderer(directory); return true; }
    catch (const std::exception& e) {
        std::cerr << "Lighting initialization: " << e.what() << '\n';
        texturing::shutdownRenderer();
        return false;
    }
}
void shutdown() {
    texturing::shutdownRenderer();
}
void update(double time) {
    float dt = lastTime < 0 ? 0.f : static_cast<float>(std::clamp(time-lastTime,0.0,0.1));
    lastTime = time;
    if (state.animateTextures) state.textureTime += dt;
    scene::advance(state.object,dt);
    if (state.animateLights) {
        float angle = dt*0.5f;
        for (auto& p : state.points) {
            float x = p.position.x, z = p.position.z;
            p.position.x = x*std::cos(angle)+z*std::sin(angle);
            p.position.z = -x*std::sin(angle)+z*std::cos(angle);
        }
    }
    const auto& io = ImGui::GetIO();
    panelWidth = std::min(390.f,io.DisplaySize.x*0.47f);
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({panelWidth,io.DisplaySize.y});
    ImGui::Begin("Labs 1 + 2 + 3",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({0.4f,0.75f,1.f,1.f},"COMPUTER GRAPHICS / LABS 01 + 02 + 03");
    ImGui::TextDisabled("Pyramid / variant 6 / Blinn-Phong");
    ImGui::TextWrapped("Esc: exit | WASD + Q/E: fly | Shift: faster\nRMB: look | Alt + RMB: orbit\nWheel: FOV / ortho size | R: reset\nSpace: pause object animation");
    ImGui::Separator();
    if (ImGui::Button("Reset scene")) state = {};
    if (ImGui::Button("Base task: directional light only")) {
        state = {}; state.points.clear(); state.spots.clear();
    }
    ImGui::PushItemWidth(-115.f);
    if (ImGui::CollapsingHeader("Textures / Lab 3", ImGuiTreeNodeFlags_DefaultOpen)) textureControls();
    if (ImGui::CollapsingHeader("1. Camera")) cameraControls();
    if (ImGui::CollapsingHeader("2. Pyramid / material / animation")) objectControls();
    if (ImGui::CollapsingHeader("3. Ambient + directional",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Ambient color", &state.ambient.x);
        ImGui::SliderFloat("Ambient power", &state.ambientIntensity, 0.f, 1.f);
        ImGui::Checkbox("Directional enabled", &state.directionalEnabled);
        ImGui::DragFloat3("Ray direction", &state.direction.x, 0.01f, -1.f, 1.f, "%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("Sun color", &state.directionalColor.x);
        ImGui::SliderFloat("Sun intensity", &state.directionalIntensity, 0.f, 4.f);
    }
    if (ImGui::CollapsingHeader("4. Point lights",ImGuiTreeNodeFlags_DefaultOpen)) pointControls();
    if (ImGui::CollapsingHeader("5. Spotlights",ImGuiTreeNodeFlags_DefaultOpen)) spotControls();
    if (ImGui::CollapsingHeader("6. Visualization",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Light cubes", &state.showMarkers);
        ImGui::Checkbox("Spot cones + directions", &state.showCones);
        ImGui::SliderFloat("Cone length", &state.coneLength, 0.2f, 6.f);
        ImGui::Checkbox("Animate point lights", &state.animateLights);
    }
    ImGui::PopItemWidth();
    ImGui::TextDisabled("%d point / %d spot | %.0f FPS",static_cast<int>(state.points.size()),static_cast<int>(state.spots.size()),io.Framerate);
    ImGui::End();
    movement(dt);
}
void render(const graphics::internal::FrameData& frame) {
    texturing::render(frame,state,panelWidth);
}
}
