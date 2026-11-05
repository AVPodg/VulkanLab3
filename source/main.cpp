#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include "graphics_internal.hpp"
#include "lab3_application.hpp"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
std::filesystem::path executableDirectory(const char* arg) {
#ifdef _WIN32
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    if (length && length < 32768) return std::filesystem::path(path).parent_path();
#elif defined(__linux__)
    std::error_code error;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return path.parent_path();
#endif
    return std::filesystem::absolute(arg).parent_path();
}
}

int main(int, char** argv) {
    glfwSetErrorCallback([](int code, const char* message) {
        std::cerr << "GLFW " << code << ": " << message << '\n';
    });
    if (!glfwInit()) return EXIT_FAILURE;
    if (!glfwVulkanSupported()) {
        std::cerr << "Vulkan driver not available. Install the AMD graphics driver.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1440, 900, "CG Labs 1 + 2 + 3 - Pyramid, Lighting and Textures - Vulkan", nullptr, nullptr);
    if (!window) { glfwTerminate(); return EXIT_FAILURE; }
    glfwSetWindowSizeLimits(window, 760, 540, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
        if (width > 0 && height > 0) graphics::internal::resize(width, height);
    });
    bool imgui = false, backend = false, graphics = false, app = false;
    int result = EXIT_SUCCESS;
    try {
        if (!ImGui::CreateContext()) throw std::runtime_error("Cannot create ImGui context");
        imgui = true;
        ImGui::GetIO().IniFilename = nullptr;
        if (!ImGui_ImplGlfw_InitForVulkan(window, true)) throw std::runtime_error("Cannot initialize ImGui GLFW backend");
        backend = true;
        if (!graphics::internal::initialize(window)) throw std::runtime_error("Cannot initialize Vulkan");
        graphics = true;
        if (!lab3::initialize(executableDirectory(argv[0]) / "shaders"))
            throw std::runtime_error("Cannot initialize the pyramid");
        app = true;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (width == 0 || height == 0) { glfwWaitEvents(); continue; }
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
            // Wait for the previous submission before touching ImGui GPU resources.
            const auto fd = graphics::internal::prepare();
            if (!fd.command_buffer) throw std::runtime_error("Cannot acquire a frame");
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            lab3::update(glfwGetTime());
            ImGui::Render();
            lab3::render(fd);
            graphics::internal::submitAndPresent();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        result = EXIT_FAILURE;
    }
    if (app) lab3::shutdown();
    if (graphics) graphics::internal::shutdown();
    if (backend) ImGui_ImplGlfw_Shutdown();
    if (imgui) ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
