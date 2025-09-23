#version 460 core

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec4 outFragColor;


const float PI = 3.14159265359;

layout (set = 0, binding = 1) uniform sampler2D normal_map;

layout(set = 0, binding = 2) uniform  SceneData{   
    vec3 world_camera_pos;
    int show_wireframe;
    vec3 sun_direction;    
    int padding;
    vec4 sun_color;
    vec4 diffuse_reflectance;
    float fresnel_normal_strength;
    float fresnel_shininess;
    float fresnel_bias;
    float specular_reflectance;
    float specular_normal_strength;
    float shininess;
    float fresnel_strength;
    float foam;
    vec4 ambient;
    vec4 fresnel_color;
} sceneData;

layout(set=0,binding=3) uniform samplerCube environmentMap;

vec3 HDR(vec3 color, float exposure)
{
    return 1.0 - exp(-color * exposure);
}

float DotClamped(vec3 a, vec3 b)
{
    return max(dot(a,b),0.0f);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0, float power)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, power);
}

float saturate(float a)
{
    return max(a,0.0f);
}
void main()
{
     
    vec3 normal = texture(normal_map, inUV).xyz;
    normal *= sceneData.fresnel_normal_strength;
    normal = normalize(vec3(-normal.x,1.0f,-normal.y));
	vec3 view_dir = normalize(inFragPos - sceneData.world_camera_pos);
    vec3 light_dir = normalize(-sceneData.sun_direction);
    vec3 halfwayDir = normalize(light_dir + view_dir);
    float ndotl = max(0.0f,dot(light_dir, normal));
    vec3 diffuse_color = sceneData.diffuse_reflectance.rgb;

    /*
    vec3 R = reflect(view_dir, normal);
    vec3 sky_col =  texture(environmentMap, R).rgb;
    vec3 diffuse_color = sceneData.diffuse_reflectance.rgb;

    float cosTheta = saturate(dot(normal, view_dir));
    vec3 F0 = vec3(sceneData.specular_reflectance);
    vec3 F = FresnelSchlick(cosTheta, F0, sceneData.fresnel_strength);

    // --- Final shading
    vec3 color = mix(diffuse_color, sky_col, F);
    */
    vec3 view_ray_refracted = refract(view_dir, normal, 0.75);
    vec3 view_ray_reflected = reflect(view_dir, normal);

    float fresnel = 0.02 + 0.98 * pow(1.0 - dot(-view_dir, normal), 5.0);

    vec3 reflectedColor = texture(environmentMap, view_ray_reflected).rgb;

    float specular = pow(max(0.0, dot(reflect(light_dir, normal), view_dir)), 720.0) * 210.0;

    vec3 color = mix(diffuse_color * ndotl, reflectedColor + specular, fresnel);
    
    //color = reflectedColor;
    
    outFragColor = vec4(color, 0.f);
}