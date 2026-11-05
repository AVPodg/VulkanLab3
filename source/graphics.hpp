#pragma once
#include "graphics_internal.hpp"
#include <cstddef>
#include <filesystem>

namespace graphics {
struct Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};
void check(VkResult result, const char* operation);
Buffer createBuffer(const void* data, size_t size, VkBufferUsageFlags usage);
void destroyBuffer(Buffer& buffer);
VkShaderModule loadShader(const std::filesystem::path& path);
} // namespace graphics
