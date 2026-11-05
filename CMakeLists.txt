cmake_minimum_required(VERSION 3.24)
project(camera-lighting-lab VERSION 3.0 LANGUAGES C CXX)
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_program(LAB_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT LAB_GLSLC)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK and restart the terminal, or set LAB_GLSLC.")
endif()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE STATIC)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.5.1 GIT_SHALLOW TRUE)
FetchContent_Declare(vk-bootstrap GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git GIT_TAG v1.4.321 GIT_SHALLOW TRUE)
FetchContent_Declare(VulkanMemoryAllocator GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git GIT_TAG v3.4.0 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9 GIT_SHALLOW TRUE)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glfw vk-bootstrap VulkanMemoryAllocator imgui glm)

add_executable(camera-lighting-lab source/main.cpp source/graphics_internal.cpp source/graphics.cpp source/texture.cpp source/lighting_scene.cpp source/textured_renderer.cpp source/lab3_application.cpp)
target_link_libraries(camera-lighting-lab PRIVATE glfw Vulkan::Vulkan vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator glm::glm)
target_include_directories(camera-lighting-lab PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends third_party)
target_compile_definitions(camera-lighting-lab PRIVATE GLFW_INCLUDE_NONE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_sources(camera-lighting-lab PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
if(MSVC)
    target_compile_options(camera-lighting-lab PRIVATE /W4 /wd4201 /utf-8)
else()
    target_compile_options(camera-lighting-lab PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter)
endif()
if(MINGW)
    target_link_options(camera-lighting-lab PRIVATE -static-libgcc -static-libstdc++)
endif()
set_target_properties(camera-lighting-lab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:camera-lighting-lab>")

foreach(shader textured.vert textured.frag textured_base.frag light_marker.vert marker.frag)
    set(binary "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader}.spv")
    add_custom_command(OUTPUT "${binary}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${LAB_GLSLC}" --target-env=vulkan1.1 "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${binary}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_common.glsl" VERBATIM)
    list(APPEND SHADER_BINARIES "${binary}")
endforeach()
add_custom_target(lab-shaders DEPENDS ${SHADER_BINARIES})
add_dependencies(camera-lighting-lab lab-shaders)
# Run on every build so editing only a shader also updates the runnable directory.
add_custom_target(lab-resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/textures" "$<TARGET_FILE_DIR:camera-lighting-lab>/textures"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:camera-lighting-lab>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_BINARIES} "$<TARGET_FILE_DIR:camera-lighting-lab>/shaders"
    DEPENDS camera-lighting-lab lab-shaders VERBATIM)
if(MINGW)
    get_filename_component(COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(WINPTHREAD_DLL libwinpthread-1.dll HINTS "${COMPILER_BIN}")
    if(WINPTHREAD_DLL)
        add_custom_command(TARGET lab-resources POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WINPTHREAD_DLL}" "$<TARGET_FILE_DIR:camera-lighting-lab>"
            VERBATIM)
        install(FILES "${WINPTHREAD_DLL}" DESTINATION .)
    endif()
endif()

install(TARGETS camera-lighting-lab RUNTIME DESTINATION .)
install(FILES ${SHADER_BINARIES} DESTINATION shaders)
install(DIRECTORY textures DESTINATION .)
install(FILES LICENSE NOTICE DESTINATION .)
