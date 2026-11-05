layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec3 vertexColor;
layout(location=3) in vec2 textureUV;
layout(location=0) out vec4 outColor;
layout(set=0,binding=0,std140) uniform Scene {
    mat4 viewProjection;
    vec4 camera;
    vec4 ambient;
    vec4 direction;
    vec4 directionalColor;
    ivec4 counts;
} scene;
struct Point { vec4 position; vec4 color; };
struct Spot { vec4 position; vec4 direction; vec4 color; vec4 angles; };
// Runtime arrays: no fixed maximum number of lights in the shader.
layout(set=0,binding=1,std430) readonly buffer Points { Point values[]; } points;
layout(set=0,binding=2,std430) readonly buffer Spots { Spot values[]; } spots;
layout(set=1,binding=0,std140) uniform Model {
    mat4 transform;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specular;
    vec4 parameters;
    vec4 maps; // textured, specular enabled, emissive enabled, emission strength
} model;
layout(set=1,binding=1) uniform sampler2D albedoMap;
layout(set=1,binding=2) uniform sampler2D specularMap;
layout(set=1,binding=3) uniform sampler2D emissiveMap;
#ifndef BASIC_SAMPLING
// Nontrivial sampling alone uses push constants; model properties stay in UBOs.
layout(push_constant) uniform Sampling {
    float time;
    float amount;
    float speed;
    int enabled;
} sampling;
#endif
vec3 specularColor;
vec3 sampleAlbedo(vec2 uv) {
    vec3 first = texture(albedoMap,uv).rgb;
#ifndef BASIC_SAMPLING
    if (sampling.enabled == 0) return first;
    float t = sampling.time * sampling.speed;
    float amount = sampling.amount;
    // Bounded coordinates remain inside the image with CLAMP_TO_EDGE.
    vec2 warped = clamp(uv + amount * 0.08 * vec2(sin(uv.y*12.0+t),cos(uv.x*10.0-t)),0.0,1.0);
    float angle = 0.35*sin(t*0.4);
    mat2 rotation = mat2(cos(angle),sin(angle),-sin(angle),cos(angle));
    vec2 secondUV = 0.5 + rotation*(uv-0.5)*0.65 + 0.06*vec2(sin(t*0.3),cos(t*0.4));
    vec3 second = texture(albedoMap,secondUV).rgb;
    vec3 modulation = 0.75 + 0.25*cos(t + vec3(0.0,2.094,4.189) + uv.x*3.0);
    return mix(first, mix(texture(albedoMap,warped).rgb,second,0.35)*modulation,amount);
#else
    return first;
#endif
}
vec3 unitVector(vec3 v) { return v * inversesqrt(max(dot(v,v), 0.000001)); }
vec3 blinnPhong(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo) {
    float diffuse = max(dot(N,L),0);
    vec3 H = unitVector(L+V);
    float highlight = diffuse > 0 ? pow(max(dot(N,H),0),model.parameters.x) : 0;
    return radiance * (albedo * diffuse + specularColor * highlight);
}
void main() {
    vec3 N = unitVector(worldNormal);
    vec3 V = unitVector(scene.camera.xyz-worldPosition);
    vec3 albedo = model.albedo.rgb;
    vec2 uv = textureUV;
    specularColor = model.specular.rgb;
    if (model.maps.x > 0.5) {
        albedo *= sampleAlbedo(uv);
        // Missing maps bind a black pixel, removing the corresponding contribution.
        specularColor *= model.maps.y > 0.5 ? texture(specularMap,uv).rgb : vec3(0.0);
    } else if (model.parameters.y > 0.5) albedo *= vertexColor;
    vec3 result = scene.ambient.rgb * scene.ambient.a * albedo;
    result += blinnPhong(-scene.direction.xyz, scene.directionalColor.rgb * scene.directionalColor.a,N,V,albedo);
    for (int i=0; i<scene.counts.x; ++i) {
        vec3 delta = points.values[i].position.xyz-worldPosition;
        float distanceSquared = max(dot(delta,delta),0.0001);
        vec3 radiance = points.values[i].color.rgb * points.values[i].color.a / distanceSquared;
        result += blinnPhong(unitVector(delta),radiance,N,V,albedo);
    }
    for (int i=0; i<scene.counts.y; ++i) {
        Spot light = spots.values[i];
        vec3 delta = light.position.xyz-worldPosition;
        float distanceSquared = max(dot(delta,delta),0.0001);
        vec3 L = unitVector(delta);
        float cosine = dot(-L,light.direction.xyz);
        float inner = light.angles.x, outer = light.angles.y;
        // Equal angles give a hard boundary, otherwise a smooth transition.
        float cone = inner > outer + 0.000001 ? smoothstep(outer,inner,cosine) : step(outer,cosine);
        vec3 radiance = light.color.rgb * light.color.a * cone / distanceSquared;
        result += blinnPhong(L,radiance,N,V,albedo);
    }
    if (model.maps.x > 0.5 && model.maps.z > 0.5)
        result += texture(emissiveMap,uv).rgb * model.maps.w;
    outColor = vec4(result,1);
}
