#pragma once
#include "graphics_internal.hpp"
#include <filesystem>
namespace lab3 {
bool initialize(const std::filesystem::path& directory);
void shutdown();
void update(double time);
void render(const graphics::internal::FrameData& frame);
}
