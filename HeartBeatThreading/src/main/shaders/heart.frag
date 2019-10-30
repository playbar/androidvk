#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D colorMap;
layout (binding = 2) uniform sampler2D normalMap;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inLightVec;
layout (location = 2) in vec3 inLightVecB;
layout (location = 3) in vec3 inViewVec;

layout (location = 0) out vec4 outFragColor;

void main()
{
    float lightRadius = 45.0;
    float invRadius = 1.0/lightRadius;
    float ambient = 0.25;
    vec3 specularColor = vec3(1.0, 0.8, 0.8);

	vec3 color = texture(colorMap, inUV).rgb;
	vec3 normal = normalize((texture(normalMap, inUV).rgb - 0.5) * 2.0);

    float distSqr = dot(inLightVecB, inLightVecB);
    vec3 lVec = inLightVecB * inversesqrt(distSqr);

    float atten = max(clamp(1.0 - invRadius * sqrt(distSqr), 0.0, 1.0), ambient);
    float diffuse = clamp(dot(lVec, normal), 0.0, 1.0);

    vec3 light = normalize(-inLightVec);
    vec3 view = normalize(inViewVec);
    vec3 reflectDir = reflect(-light, normal);

    float specular = pow(max(dot(view, reflectDir), 0.0), 4.0);

    outFragColor = vec4((color * atten + (diffuse * color + 0.5 * specular * specularColor)) * atten, 1.0);
}