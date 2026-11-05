#pragma once
#include "scene.hpp"
#include <vector>

namespace lighting {
struct Vertex { glm::vec3 position, color, normal; };
struct Material {
    glm::vec3 albedo{1.f};
    glm::vec3 specular{0.8f};
    float shininess = 64.f;
    bool vertexColors = true;
};
struct PointLight {
    glm::vec3 position{2.f, 2.f, 2.f};
    glm::vec3 color{1.f, 0.45f, 0.15f};
    float intensity = 18.f;
    bool enabled = true;
};
struct SpotLight {
    glm::vec3 position{-3.f, 4.f, 2.f};
    glm::vec3 direction{0.5f, -1.f, -0.4f};
    glm::vec3 color{0.3f, 0.6f, 1.f};
    float intensity = 55.f;
    float innerAngle = 15.f, outerAngle = 25.f;
    bool enabled = true;
};
struct Camera {
    glm::vec3 position{5.f, 3.f, 7.f};
    float yaw = -125.5f, pitch = -19.2f, roll = 0.f;
    float fov = 45.f, orthoHeight = 8.f, speed = 4.f;
    float nearPlane = 0.1f, farPlane = 150.f;
    bool perspective = true;
};
struct State {
    scene::Settings object;
    Camera camera;
    Material material;
    glm::vec3 ambient{1.f}, direction{-0.6f, -1.f, -0.5f}, directionalColor{1.f, 0.95f, 0.85f};
    float ambientIntensity = 0.08f, directionalIntensity = 0.7f;
    bool directionalEnabled = true, showMarkers = true, showCones = true;
    bool animateLights = false;
    float coneLength = 3.f;
    std::vector<PointLight> points{PointLight{}};
    std::vector<SpotLight> spots{SpotLight{}};
};
glm::vec3 safeDirection(glm::vec3 value);
glm::vec3 forward(const Camera& camera);
glm::mat4 view(const Camera& camera);
glm::mat4 projection(const Camera& camera, float aspect);
std::vector<Vertex> pyramid();
void markers(const State& state, std::vector<Vertex>& cubes, std::vector<Vertex>& lines);
}
