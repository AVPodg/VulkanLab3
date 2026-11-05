#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 uv;
layout(set=0,binding=0,std140) uniform Scene {
    mat4 viewProjection;
    vec4 camera;
    vec4 ambient;
    vec4 direction;
    vec4 directionalColor;
    ivec4 counts;
} scene;
layout(set=1,binding=0,std140) uniform Model {
    mat4 transform;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specular;
    vec4 parameters;
    vec4 maps;
} model;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec3 vertexColor;
layout(location=3) out vec2 textureUV;
void main() {
    vec4 world = model.transform * vec4(position,1);
    worldPosition = world.xyz;
    worldNormal = mat3(model.normalMatrix) * normal;
    vertexColor = color;
    textureUV = uv;
    gl_Position = scene.viewProjection * world;
}
