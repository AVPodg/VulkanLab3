#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene {

struct Vertex { glm::vec3 position; glm::vec3 color; }; 

inline const std::array<glm::vec3, 5> positions{{
    {-1.f, -1.f, -1.f}, {1.f, -1.f, -1.f},
    {1.f, -1.f, 1.f}, {-1.f, -1.f, 1.f}, {0.f, 1.f, 0.f}
}};
inline constexpr std::array<uint16_t, 18> indices{
    0, 4, 1, 1, 4, 2, 2, 4, 3, 3, 4, 0, 
    0, 1, 2, 0, 2, 3   
};

inline std::array<Vertex, 5> makeVertices() { 
    std::array<Vertex, 5> result{};
    for (size_t i = 0; i < positions.size(); ++i) {
        const glm::vec3 color = glm::vec3(0.2f) + 0.8f * (positions[i] + 1.f) * 0.5f; // Она преобразует координаты вершины из диапазона от -1 до 1 в компоненты цвета от 0.2 до 1.
        result[i] = {positions[i], color};
    }
    return result;
}

struct Settings { // пользовательские настройки и время анимации
    glm::vec3 position{0.f}; 
    glm::vec3 rotation{0.f}; 
    glm::vec3 scale{1.f}; 
    glm::vec3 tint{1.f};
    bool perspective = true; 
    bool animate = false; 
    bool playing = true; 
    float speed = 1.f; 
    float radius = 1.8f; 
    float height = 0.7f; 
    float spin = 50.f; 
    float fov = 45.f;
    float orthoHeight = 7.f; 
    float cameraYaw = 35.f; 
    float cameraElevation = 22.f; 
    float cameraDistance = 9.f; 
    double animationTime = 0.0; 
};

inline void advance(Settings& s, double delta) { 
    if (s.animate && s.playing)
        s.animationTime += std::clamp(delta, 0.0, 0.1) * s.speed; 
}

inline glm::vec3 trajectory(const Settings& s) { 
    if (!s.animate) return glm::vec3(0.f); 
    const float t = static_cast<float>(std::fmod(s.animationTime, 6.283185307179586)); 
    return {s.radius * std::sin(t), s.height * std::sin(3.f * t), 
            0.5f * s.radius * std::sin(2.f * t)}; 
}

inline glm::mat4 model(const Settings& s) { 
    const double angle = s.animate ? s.animationTime * s.spin : 0.0; 
    const float spinY = static_cast<float>(std::fmod(angle, 360.0));
    const float spinX = static_cast<float>(std::fmod(angle * 0.5, 360.0));  
    glm::mat4 m = glm::translate(glm::mat4(1.f), s.position + trajectory(s));
    m = glm::rotate(m, glm::radians(s.rotation.y + spinY), {0.f, 1.f, 0.f}); 
    m = glm::rotate(m, glm::radians(s.rotation.x + spinX), {1.f, 0.f, 0.f}); 
    m = glm::rotate(m, glm::radians(s.rotation.z), {0.f, 0.f, 1.f});
    return glm::scale(m, s.scale);
}

inline glm::mat4 view(const Settings& s) { 
    const float yaw = glm::radians(s.cameraYaw); 
    const float elevation = glm::radians(s.cameraElevation); 
    const glm::vec3 eye = s.cameraDistance * glm::vec3(  // вычисление позиции камеры
        std::cos(elevation) * std::sin(yaw), std::sin(elevation), 
        std::cos(elevation) * std::cos(yaw)); 
    return glm::lookAtRH(eye, glm::vec3(0.f), {0.f, 1.f, 0.f}); 
}

inline glm::mat4 projection(const Settings& s, float aspect) { // преобразование 3D в изображение
    const float h = s.orthoHeight * 0.5f; 
    glm::mat4 p = s.perspective 
        ? glm::perspectiveRH_ZO(glm::radians(s.fov), aspect, 0.1f, 100.f) 
        : glm::orthoRH_ZO(-h * aspect, h * aspect, -h, h, 0.1f, 100.f); 
    p[1][1] *= -1.f; 
    return p; 
}

} 
