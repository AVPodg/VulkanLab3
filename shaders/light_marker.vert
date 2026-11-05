#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(set=0,binding=0,std140) uniform Scene {
    mat4 viewProjection;
    vec4 camera, ambient, direction, directionalColor;
    ivec4 counts;
} scene;
layout(location=0) out vec3 markerColor;
void main() {
    markerColor=color;
    gl_Position=scene.viewProjection*vec4(position,1);
}
