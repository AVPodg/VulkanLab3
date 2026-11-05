#include "lighting_scene.hpp"

namespace lighting {
glm::vec3 safeDirection(glm::vec3 v) {
    if (glm::dot(v, v) < 0.000001f) return {0.f, -1.f, 0.f};
    return glm::normalize(v);
}
glm::vec3 forward(const Camera& c) {
    float y = glm::radians(c.yaw), p = glm::radians(c.pitch);
    return {std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y)};
}
glm::mat4 view(const Camera& c) {
    glm::vec3 f = forward(c);
    glm::vec3 right = glm::normalize(glm::cross(f, glm::vec3(0.f, 1.f, 0.f)));
    glm::vec3 up = glm::cross(right, f);
    up = up * std::cos(glm::radians(c.roll)) + right * std::sin(glm::radians(c.roll));
    return glm::lookAtRH(c.position, c.position + f, up);
}
glm::mat4 projection(const Camera& c, float aspect) {
    float h = c.orthoHeight * 0.5f;
    glm::mat4 p = c.perspective
        ? glm::perspectiveRH_ZO(glm::radians(c.fov), aspect, c.nearPlane, c.farPlane)
        : glm::orthoRH_ZO(-h * aspect, h * aspect, -h, h, c.nearPlane, c.farPlane);
    p[1][1] *= -1.f;
    return p;
}
// Each face has its own vertices: the pyramid must have sharp edges.
void triangle(std::vector<Vertex>& result, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
    glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    for (glm::vec3 p : {a, b, c}) {
        glm::vec3 color = glm::vec3(0.2f) + 0.8f * (p + 1.f) * 0.5f;
        result.push_back({p, color, n});
    }
}
std::vector<Vertex> pyramid() {
    std::vector<Vertex> result;
    for (size_t i = 0; i < scene::indices.size(); i += 3)
        triangle(result, scene::positions[scene::indices[i]],
                 scene::positions[scene::indices[i+1]], scene::positions[scene::indices[i+2]]);
    return result;
}
void cube(std::vector<Vertex>& result, glm::vec3 center, glm::vec3 color) {
    const glm::vec3 p[] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
                           {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    const int ids[] = {0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,
                       3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
    for (int i : ids) result.push_back({center + p[i] * 0.12f, color, {0,1,0}});
}
void line(std::vector<Vertex>& result, glm::vec3 a, glm::vec3 b) {
    result.push_back({a, {1,1,1}, {0,1,0}});
    result.push_back({b, {1,1,1}, {0,1,0}});
}
void markers(const State& s, std::vector<Vertex>& cubes, std::vector<Vertex>& lines) {
    cubes.clear(); lines.clear();
    for (const auto& p : s.points)
        if (s.showMarkers && p.enabled) cube(cubes, p.position, p.color);
    for (const auto& p : s.spots) {
        if (!p.enabled) continue;
        if (s.showMarkers) cube(cubes, p.position, p.color);
        if (!s.showCones) continue;
        glm::vec3 d = safeDirection(p.direction);
        glm::vec3 helper = std::abs(d.y) > 0.99f ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
        glm::vec3 u = glm::normalize(glm::cross(d, helper));
        glm::vec3 v = glm::cross(d, u);
        glm::vec3 center = p.position + d * s.coneLength;
        line(lines, p.position, center);
        // Two rings show the inner and outer half angles of the spotlight.
        for (float angle : {p.innerAngle, p.outerAngle}) {
            float radius = s.coneLength * std::tan(glm::radians(angle));
            for (int i = 0; i < 32; ++i) {
                float a = i * 6.2831853f / 32.f, b = (i + 1) * 6.2831853f / 32.f;
                glm::vec3 first = center + radius * (u * std::cos(a) + v * std::sin(a));
                glm::vec3 next = center + radius * (u * std::cos(b) + v * std::sin(b));
                line(lines, first, next);
                if (i % 8 == 0) line(lines, p.position, first);
            }
        }
    }
}
}
