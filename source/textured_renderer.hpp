#pragma once
#include "graphics_internal.hpp"
#include "lighting_scene.hpp"
#include <filesystem>

namespace texturing {
inline constexpr int materialCount = 4;
inline constexpr const char* materialNames[] = {"Vertex / solid color (Labs 1-2)", "Cat", "Sunflower", "Brick"};
struct TextureSettings {
    int material = 1;
    bool effect = false, specularMap = true, emissiveMap = true;
    float effectAmount = 0.35f, effectSpeed = 1.f, emission = 1.f;
};
struct Vertex { glm::vec3 position, color, normal; glm::vec2 uv{0.f}; };
struct State : lighting::State {
    TextureSettings texture;
    bool animateTextures = true;
    double textureTime = 0.0;
};
std::vector<Vertex> pyramid();
void initializeRenderer(const std::filesystem::path& directory);
void shutdownRenderer();
void render(const graphics::internal::FrameData& frame, const State& state, float panelWidth);
}
