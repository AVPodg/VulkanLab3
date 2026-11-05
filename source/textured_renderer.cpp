#include "textured_renderer.hpp"
#include "graphics.hpp"
#include "texture.hpp"
#include <imgui.h>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace texturing {
namespace {
struct SceneUniform {
    glm::mat4 viewProjection;
    glm::vec4 camera, ambient, direction, directionalColor;
    glm::ivec4 counts;
};
struct ModelUniform {
    glm::mat4 transform, normalMatrix;
    glm::vec4 albedo, specular, parameters, maps;
};
struct PointData { glm::vec4 position, color; };
struct SpotData { glm::vec4 position, direction, color, angles; };
static_assert(sizeof(SceneUniform) == 144 && offsetof(SceneUniform, counts) == 128);
static_assert(sizeof(ModelUniform) == 192 && offsetof(ModelUniform, parameters) == 160);
static_assert(sizeof(PointData) == 32 && sizeof(SpotData) == 64);
struct DynamicBuffer { graphics::Buffer buffer; size_t capacity = 0; std::vector<unsigned char> cachedGeometry; };
DynamicBuffer sceneBuffer, pointBuffer, spotBuffer, modelBuffer, cubeBuffer, lineBuffer;
graphics::Buffer pyramidBuffer, pyramidIndices;
std::array<graphics::Texture, materialCount> albedoTextures, specularTextures, emissiveTextures;
std::array<VkDescriptorSet, materialCount> materialSets{};
VkDeviceSize modelStride = 0;
struct SamplingPush { float time, amount, speed; int enabled; };
static_assert(sizeof(SamplingPush) == 16);
uint32_t pyramidCount = 0;
VkDescriptorSetLayout sceneLayout = VK_NULL_HANDLE, modelLayout = VK_NULL_HANDLE;
VkPipelineLayout lightingLayout = VK_NULL_HANDLE, markerLayout = VK_NULL_HANDLE;
VkPipeline litPipeline = VK_NULL_HANDLE, basePipeline = VK_NULL_HANDLE, cubePipeline = VK_NULL_HANDLE, linePipeline = VK_NULL_HANDLE;
VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
VkDescriptorSet sceneSet = VK_NULL_HANDLE;

void upload(DynamicBuffer& b, const void* data, size_t size, VkBufferUsageFlags usage) {
    size_t needed = std::max(size, size_t(16));
    if (needed > b.capacity) {
        graphics::destroyBuffer(b.buffer);
        b.capacity = std::max(needed, b.capacity * 2);
        std::vector<unsigned char> zero(b.capacity, 0);
        b.buffer = graphics::createBuffer(zero.data(), zero.size(), usage);
    }
    if (size == 0) return;
    void* mapped = nullptr;
    auto allocator = graphics::internal::context.allocator;
    graphics::check(vmaMapMemory(allocator, b.buffer.allocation, &mapped), "vmaMapMemory");
    std::memcpy(mapped, data, size);
    VkResult result = vmaFlushAllocation(allocator, b.buffer.allocation, 0, size);
    vmaUnmapMemory(allocator, b.buffer.allocation);
    graphics::check(result, "vmaFlushAllocation");
}
void bindBuffer(VkDescriptorSet set, uint32_t binding, const DynamicBuffer& buffer, VkDescriptorType type, VkDeviceSize range = 0) {
    VkDescriptorBufferInfo info{buffer.buffer.handle, 0, range ? range : buffer.capacity};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set; write.dstBinding = binding;
    write.descriptorCount = 1; write.descriptorType = type; write.pBufferInfo = &info;
    vkUpdateDescriptorSets(graphics::internal::context.device, 1, &write, 0, nullptr);
}

VkPipeline createPipeline(const std::filesystem::path& directory, bool marker, bool lines, bool basic = false) {
    const auto device = graphics::internal::context.device;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    try {
        vert = graphics::loadShader(directory / (marker ? "light_marker.vert.spv" : "textured.vert.spv"));
        frag = graphics::loadShader(directory / (marker ? "marker.frag.spv" : basic ? "textured_base.frag.spv" : "textured.frag.spv"));

        const VkPipelineShaderStageCreateInfo stages[]{
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"}
        };

        const VkVertexInputBindingDescription binding{0, uint32_t(marker ? sizeof(lighting::Vertex) : sizeof(Vertex)), VK_VERTEX_INPUT_RATE_VERTEX};
        const VkVertexInputAttributeDescription attributes[]{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
            {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)}
        };
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = 1;
        input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = marker ? 2 : 4;
        input.pVertexAttributeDescriptions = attributes;

        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = marker ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.f;

        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = 0xf;

        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;

        const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = states;

        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &input;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &samples;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = marker ? markerLayout : lightingLayout;
        info.renderPass = graphics::internal::context.render_pass;

        graphics::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline),
                        "vkCreateGraphicsPipelines");
    } catch (...) {
        vkDestroyShaderModule(device, frag, nullptr);
        vkDestroyShaderModule(device, vert, nullptr);
        throw;
    }
    vkDestroyShaderModule(device, frag, nullptr);
    vkDestroyShaderModule(device, vert, nullptr);
    return pipeline;
}

void createDescriptors() {
    auto device = graphics::internal::context.device;
    const VkShaderStageFlags both = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, both, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 3; layout.pBindings = bindings;
    graphics::check(vkCreateDescriptorSetLayout(device, &layout, nullptr, &sceneLayout), "scene layout");
    const VkDescriptorSetLayoutBinding modelBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, both, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    layout.bindingCount = 4; layout.pBindings = modelBindings;
    graphics::check(vkCreateDescriptorSetLayout(device, &layout, nullptr, &modelLayout), "model layout");
    VkDescriptorSetLayout sets[] = {sceneLayout, modelLayout};
    VkPipelineLayoutCreateInfo pipelineLayout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayout.setLayoutCount = 2; pipelineLayout.pSetLayouts = sets;
    VkPushConstantRange samplingPush{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SamplingPush)};
    pipelineLayout.pushConstantRangeCount = 1; pipelineLayout.pPushConstantRanges = &samplingPush;
    graphics::check(vkCreatePipelineLayout(device, &pipelineLayout, nullptr, &lightingLayout), "lighting layout");
    pipelineLayout.setLayoutCount = 1; pipelineLayout.pSetLayouts = &sceneLayout;
    pipelineLayout.pushConstantRangeCount = 0; pipelineLayout.pPushConstantRanges = nullptr;
    graphics::check(vkCreatePipelineLayout(device, &pipelineLayout, nullptr, &markerLayout), "marker layout");
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, materialCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialCount * 3}
    };
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets = 1 + materialCount; pool.poolSizeCount = 4; pool.pPoolSizes = sizes;
    graphics::check(vkCreateDescriptorPool(device, &pool, nullptr, &descriptorPool), "descriptor pool");
    std::array<VkDescriptorSetLayout, 1 + materialCount> layouts;
    layouts.fill(modelLayout); layouts[0] = sceneLayout;
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = descriptorPool; alloc.descriptorSetCount = uint32_t(layouts.size()); alloc.pSetLayouts = layouts.data();
    std::array<VkDescriptorSet, 1 + materialCount> allocated{};
    graphics::check(vkAllocateDescriptorSets(device, &alloc, allocated.data()), "descriptor sets");
    sceneSet = allocated[0];
    for (int i = 0; i < materialCount; ++i) materialSets[i] = allocated[i+1];
}
ModelUniform modelData(const glm::mat4& transform, const lighting::Material& material, const TextureSettings& texture) {
    return {transform, glm::transpose(glm::inverse(transform)),
        glm::vec4(material.albedo, 1), glm::vec4(material.specular, 1),
        {material.shininess, material.vertexColors ? 1.f : 0.f, 0, 0},
        {texture.material != 0 ? 1.f : 0.f, texture.specularMap ? 1.f : 0.f, texture.emissiveMap ? 1.f : 0.f, texture.emission}};
}
void bindMaterials() {
    for (int i = 0; i < materialCount; ++i) {
        bindBuffer(materialSets[i], 0, modelBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, sizeof(ModelUniform));
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = materialSets[i]; write.descriptorCount = 1;
        const graphics::Texture* maps[] = {&albedoTextures[i], &specularTextures[i], &emissiveTextures[i]};
        for (uint32_t j = 0; j < 3; ++j) {
            VkDescriptorImageInfo image{maps[j]->sampler, maps[j]->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            write.dstBinding = j+1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pBufferInfo = nullptr; write.pImageInfo = &image;
            vkUpdateDescriptorSets(graphics::internal::context.device, 1, &write, 0, nullptr);
        }
    }
}
void uploadGeometry(DynamicBuffer& b, const void* data, size_t size) {
    if (!size) return;
    if (b.cachedGeometry.size() == size && std::memcmp(b.cachedGeometry.data(), data, size) == 0) return;
    graphics::destroyBuffer(b.buffer);
    b.buffer = graphics::createDeviceBuffer(data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    b.capacity = size;
    const auto* bytes = static_cast<const unsigned char*>(data);
    b.cachedGeometry.assign(bytes, bytes + size);
}
void drawMesh(VkCommandBuffer cmd, const graphics::Buffer& buffer, uint32_t count) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer.handle, &offset);
    vkCmdDraw(cmd, count, 1, 0, 0);
}
}
std::vector<Vertex> pyramid() {
    std::vector<Vertex> result;
    for (const auto& v : lighting::pyramid()) {
        const auto& p = v.position;
        const glm::vec2 uv = v.normal.y < -0.99f
            ? glm::vec2((p.x + 1.f) * 0.5f, (p.z + 1.f) * 0.5f)
            : glm::vec2((p.x - p.z + 2.f) * 0.25f, (1.f - p.y) * 0.5f);
        result.push_back({p, v.color, v.normal, uv});
    }
    return result;
}
void initializeRenderer(const std::filesystem::path& directory) {
    auto p = pyramid();
    pyramidCount = static_cast<uint32_t>(p.size());
    pyramidBuffer = graphics::createDeviceBuffer(p.data(), p.size()*sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    std::vector<uint16_t> indices(p.size());
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = uint16_t(i);
    pyramidIndices = graphics::createDeviceBuffer(indices.data(), indices.size()*sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(graphics::internal::context.physical_device, &properties);
    const VkDeviceSize alignment = std::max(VkDeviceSize(1), properties.limits.minUniformBufferOffsetAlignment);
    modelStride = (sizeof(ModelUniform)+alignment-1)/alignment*alignment;
    const auto textures = directory.parent_path()/"textures";
    const char* names[] = {"white.tga", "cat.png", "sunflower.png", "brick.png"};
    for (int i = 0; i < materialCount; ++i) {
        albedoTextures[i] = graphics::loadTexture(textures/names[i], true);
        const char* spec = i == 2 ? "sunflower_specular.tga" : i == 3 ? "brick_specular.tga" : "black.tga";
        const char* emission = i == 2 ? "sunflower_emissive.tga" : i == 3 ? "brick_emissive.tga" : "black.tga";
        specularTextures[i] = graphics::loadTexture(textures/spec, false);
        emissiveTextures[i] = graphics::loadTexture(textures/emission, true);
    }
    createDescriptors();
    litPipeline = createPipeline(directory, false, false);
    basePipeline = createPipeline(directory, false, false, true);
    cubePipeline = createPipeline(directory, true, false);
    linePipeline = createPipeline(directory, true, true);
}
void shutdownRenderer() {
    auto device = graphics::internal::context.device;
    vkDeviceWaitIdle(device);
    for (auto p : {litPipeline, basePipeline, cubePipeline, linePipeline}) vkDestroyPipeline(device, p, nullptr);
    vkDestroyPipelineLayout(device, lightingLayout, nullptr);
    vkDestroyPipelineLayout(device, markerLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, sceneLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, modelLayout, nullptr);
    for (auto b : {&sceneBuffer, &pointBuffer, &spotBuffer, &modelBuffer, &cubeBuffer, &lineBuffer}) {
        graphics::destroyBuffer(b->buffer); b->capacity = 0;
    }
    graphics::destroyBuffer(pyramidBuffer);
    graphics::destroyBuffer(pyramidIndices);
    for (auto& t : albedoTextures) graphics::destroyTexture(t);
    for (auto& t : specularTextures) graphics::destroyTexture(t);
    for (auto& t : emissiveTextures) graphics::destroyTexture(t);
}
void render(const graphics::internal::FrameData& frame, const State& s, float panelWidth) {
    const auto& c = graphics::internal::context;
    const auto& io = ImGui::GetIO();
    uint32_t left = std::min(c.swapchain_extent.width-1, static_cast<uint32_t>(panelWidth*io.DisplayFramebufferScale.x));
    uint32_t width = c.swapchain_extent.width-left;
    glm::mat4 vp = lighting::projection(s.camera, float(width)/float(c.swapchain_extent.height))*lighting::view(s.camera);
    std::vector<PointData> points;
    std::vector<SpotData> spots;
    for (const auto& p : s.points) if (p.enabled)
        points.push_back({glm::vec4(p.position,1), glm::vec4(p.color,p.intensity)});
    for (const auto& p : s.spots) if (p.enabled)
        spots.push_back({glm::vec4(p.position,1), glm::vec4(lighting::safeDirection(p.direction),0), glm::vec4(p.color,p.intensity),
            {std::cos(glm::radians(p.innerAngle)),std::cos(glm::radians(p.outerAngle)),0,0}});
    SceneUniform scene{vp, glm::vec4(s.camera.position,1), glm::vec4(s.ambient,s.ambientIntensity),
        glm::vec4(lighting::safeDirection(s.direction),0), glm::vec4(s.directionalColor,s.directionalEnabled ? s.directionalIntensity : 0.f),
        {static_cast<int>(points.size()),static_cast<int>(spots.size()),0,0}};
    upload(sceneBuffer, &scene, sizeof(scene), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    upload(pointBuffer, points.data(), points.size()*sizeof(PointData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    upload(spotBuffer, spots.data(), spots.size()*sizeof(SpotData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    bindBuffer(sceneSet,0,sceneBuffer,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    bindBuffer(sceneSet,1,pointBuffer,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    bindBuffer(sceneSet,2,spotBuffer,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    lighting::Material material = s.material; material.albedo *= s.object.tint;
    const ModelUniform model = modelData(scene::model(s.object), material, s.texture);
    std::vector<unsigned char> modelBytes(size_t(modelStride), 0);
    std::memcpy(modelBytes.data(), &model, sizeof(model));
    upload(modelBuffer, modelBytes.data(), modelBytes.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    bindMaterials();
    std::vector<lighting::Vertex> cubes, lines;
    lighting::markers(s,cubes,lines);
    uploadGeometry(cubeBuffer,cubes.data(),cubes.size()*sizeof(lighting::Vertex));
    uploadGeometry(lineBuffer,lines.data(),lines.size()*sizeof(lighting::Vertex));

    auto cmd = frame.command_buffer;
    graphics::check(vkResetCommandBuffer(cmd,0), "reset frame");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    graphics::check(vkBeginCommandBuffer(cmd,&begin), "begin frame");
    VkClearValue clear[2]{};
    clear[0].color = {{0.025f,0.035f,0.055f,1.f}}; clear[1].depthStencil = {1.f,0};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = c.render_pass; pass.framebuffer = frame.framebuffer; pass.renderArea.extent = c.swapchain_extent;
    pass.clearValueCount = 2; pass.pClearValues = clear;
    vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{float(left),0,float(width),float(c.swapchain_extent.height),0,1};
    VkRect2D scissor{{static_cast<int>(left),0},{width,c.swapchain_extent.height}};
    vkCmdSetViewport(cmd,0,1,&viewport); vkCmdSetScissor(cmd,0,1,&scissor);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,s.texture.effect ? litPipeline : basePipeline);
    const auto& t = s.texture;
    VkDescriptorSet sets[] = {sceneSet, materialSets[std::clamp(t.material, 0, materialCount-1)]};
    const uint32_t dynamicOffset = 0;
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,lightingLayout,0,2,sets,1,&dynamicOffset);
    if (t.effect) {
        const SamplingPush push{float(std::fmod(s.textureTime, 3600.0)), t.effectAmount, t.effectSpeed, t.effect};
        vkCmdPushConstants(cmd,lightingLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
    }
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd,0,1,&pyramidBuffer.handle,&offset);
    vkCmdBindIndexBuffer(cmd,pyramidIndices.handle,0,VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd,pyramidCount,1,0,0,0);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,markerLayout,0,1,&sceneSet,0,nullptr);
    if (!cubes.empty()) {
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,cubePipeline);
        drawMesh(cmd,cubeBuffer.buffer,static_cast<uint32_t>(cubes.size()));
    }
    if (!lines.empty()) {
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,linePipeline);
        drawMesh(cmd,lineBuffer.buffer,static_cast<uint32_t>(lines.size()));
    }
    vkCmdEndRenderPass(cmd);
    graphics::check(vkEndCommandBuffer(cmd), "end frame");
}
}
