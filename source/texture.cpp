#include "texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <memory>
#include <stdexcept>

namespace graphics {
void transfer(const std::function<void(VkCommandBuffer)>& record) {
    const auto& c = internal::context;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    try {
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        info.queueFamilyIndex = c.graphics_queue_index;
        check(vkCreateCommandPool(c.device, &info, nullptr, &pool), "upload pool");
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.commandPool = pool; alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount = 1;
        VkCommandBuffer cmd;
        check(vkAllocateCommandBuffers(c.device, &alloc, &cmd), "upload command buffer");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(cmd, &begin), "begin upload");
        record(cmd);
        check(vkEndCommandBuffer(cmd), "end upload");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        check(vkCreateFence(c.device, &fi, nullptr, &fence), "upload fence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
        check(vkQueueSubmit(c.graphics_queue, 1, &submit, fence), "submit upload");
        check(vkWaitForFences(c.device, 1, &fence, VK_TRUE, UINT64_MAX), "wait upload");
    } catch (...) {
        vkDeviceWaitIdle(c.device);
        vkDestroyFence(c.device, fence, nullptr);
        vkDestroyCommandPool(c.device, pool, nullptr);
        throw;
    }
    vkDestroyFence(c.device, fence, nullptr);
    vkDestroyCommandPool(c.device, pool, nullptr);
}
Buffer createDeviceBuffer(const void* data, size_t size, VkBufferUsageFlags usage) {
    Buffer staging = createBuffer(data, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), result;
    const auto& c = internal::context;
    try {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size; info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo alloc{};
        alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        check(vmaCreateBuffer(c.allocator, &info, &alloc, &result.handle, &result.allocation, nullptr), "device buffer");
        transfer([&](VkCommandBuffer cmd) {
            VkBufferCopy copy{0, 0, size};
            vkCmdCopyBuffer(cmd, staging.handle, result.handle, 1, &copy);
            VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = result.handle; barrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);
        });
    } catch (...) { destroyBuffer(staging); destroyBuffer(result); throw; }
    destroyBuffer(staging);
    return result;
}
Texture loadTexture(const std::filesystem::path& path, bool srgb) {
    int width = 0, height = 0, channels = 0;
    // stb decodes files on the CPU; a staging buffer transfers RGBA pixels to VRAM.
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free);
    if (!pixels) throw std::runtime_error("Cannot decode texture: " + path.string());
    const auto& c = internal::context;
    Texture t;
    Buffer staging;
    try {
        staging = createBuffer(pixels.get(), size_t(width) * height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.imageType = VK_IMAGE_TYPE_2D;
        image.format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        image.extent = {uint32_t(width), uint32_t(height), 1};
        image.mipLevels = image.arrayLayers = 1;
        image.samples = VK_SAMPLE_COUNT_1_BIT; image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo alloc{};
        alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        check(vmaCreateImage(c.allocator, &image, &alloc, &t.image, &t.allocation, nullptr), "texture image");
        transfer([&](VkCommandBuffer cmd) {
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = t.image; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &b);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = image.extent;
            vkCmdCopyBufferToImage(cmd, staging.handle, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            b.oldLayout = b.newLayout; b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &b);
        });
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = t.image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = image.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(c.device, &view, nullptr, &t.view), "texture view");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(vkCreateSampler(c.device, &sampler, nullptr, &t.sampler), "texture sampler");
    } catch (...) { destroyBuffer(staging); destroyTexture(t); throw; }
    destroyBuffer(staging);
    return t;
}
void destroyTexture(Texture& t) {
    const auto& c = internal::context;
    vkDestroySampler(c.device, t.sampler, nullptr);
    vkDestroyImageView(c.device, t.view, nullptr);
    if (t.image) vmaDestroyImage(c.allocator, t.image, t.allocation);
    t = {};
}
}
