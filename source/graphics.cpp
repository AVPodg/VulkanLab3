#include "graphics.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace graphics {
void check(VkResult result, const char* operation) { 
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult=" + std::to_string(result)); // Выброс исключения
}

Buffer createBuffer(const void* data, size_t size, VkBufferUsageFlags usage) {
    const auto& c = internal::context; // объект, содержащий ресурсы Vulkan
    
    // Настройка вулкан буффера
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // ресурс использует режим исключительного владения.
    
    // управление памятью Vulkan
    VmaAllocationCreateInfo alloc{}; // какую память необходимо получить.
    alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                  VMA_ALLOCATION_CREATE_MAPPED_BIT;
    alloc.usage = VMA_MEMORY_USAGE_AUTO; 
    
    // Подготовка результата
    VmaAllocationInfo mapped{}; 
    Buffer result;
    check(vmaCreateBuffer(c.allocator, &info, &alloc, &result.handle, &result.allocation, &mapped),
          "vmaCreateBuffer");
    std::memcpy(mapped.pMappedData, data, size); // копируем данные в созданный буфер
    const auto flush = vmaFlushAllocation(c.allocator, result.allocation, 0, size); 
    if (flush != VK_SUCCESS) {
        destroyBuffer(result);
        check(flush, "vmaFlushAllocation");
    }
    return result;
}

void destroyBuffer(Buffer& buffer) {
    if (buffer.handle) // Если дескриптор не пустой, значит, есть буфер, который необходимо уничтожить.
        vmaDestroyBuffer(internal::context.allocator, buffer.handle, buffer.allocation);
    buffer = {};
}

VkShaderModule loadShader(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate); 
    if (!file) throw std::runtime_error("Cannot open shader: " + path.string());
    const auto size = file.tellg(); 
    if (size <= 0 || static_cast<size_t>(size) % sizeof(uint32_t) != 0)
        throw std::runtime_error("Invalid SPIR-V size: " + path.string());
    std::vector<uint32_t> code(static_cast<size_t>(size) / sizeof(uint32_t));
    file.seekg(0); // возвращаемся в начало
    if (!file.read(reinterpret_cast<char*>(code.data()), size)) 
        throw std::runtime_error("Cannot read shader: " + path.string());
    
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; 
    info.codeSize = static_cast<size_t>(size);  
    info.pCode = code.data(); 
    VkShaderModule shader = VK_NULL_HANDLE; 
    check(vkCreateShaderModule(internal::context.device, &info, nullptr, &shader), "vkCreateShaderModule"); 
    return shader;
}
} 