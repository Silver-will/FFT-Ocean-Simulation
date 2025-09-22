#version 460 core

layout (location = 0) in vec3 inFragPos;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;


layout (set = 0, binding = 1) uniform sampler2D normal_map;

layout(set = 0, binding = 2) uniform  SceneData{   
    vec3 world_camera_pos;
    int show_wireframe;
    vec3 sun_direction;    
    int padding;
} sceneData;

layout(set=0,binding=3) uniform samplerCube environmentMap;

vec3 HDR(vec3 color, float exposure)
{
    return 1.0 - exp(-color * exposure);
}

void main()
{
   

    vec3 normal = texture(normal_map, inUV).xyz;
    normal = normalize(vec3(-normal.x,1,-normal.y));
	vec3 view_dir = normalize(sceneData.world_camera_pos - inFragPos);
    float fresnel = 0.02f + 0.98f * pow(1.f - dot(normal, view_dir), 5.f);
    
    vec3 sky_color = vec3(3.2f, 9.6f, 12.8f);
    vec3 ocean_color = vec3(0.004f, 0.016f, 0.047f);
    float exposure = 0.35f;
    
    vec3 sky = fresnel * sky_color;
    float diffuse = clamp(dot(normal, normalize(-sceneData.sun_direction)), 0.f, 1.f);
    vec3 water = (1.f - fresnel) * ocean_color * sky_color * diffuse;
    
    vec3 color = sky + water;
    
    vec3 envColor = texture(environmentMap, vec3(0.5)).rgb;

    color += (envColor * 0.01);
    outFragColor = vec4(HDR(color,exposure), 0.f);
    /*
    if (sceneData.show_wireframe == 1)
    {
        outFragColor = vec4(0.f, 0.f, 0.f, 1.f);
        return;
    }
    vec3 normal = texture(normal_map, inUV).xyz;
    
	vec3 view_dir = normalize(sceneData.world_camera_pos - inFragPos);
    float fresnel = 0.02f + 0.98f * pow(1.f - dot(normal, view_dir), 5.f);
    
    vec3 sky_color = vec3(3.2f, 9.6f, 12.8f);
    vec3 ocean_color = vec3(0.004f, 0.016f, 0.047f);
    float exposure = 0.35f;
    
    vec3 sky = fresnel * sky_color;
    float diffuse = clamp(dot(normal, normalize(-sceneData.sun_direction)), 0.f, 1.f);
    vec3 water = (1.f - fresnel) * ocean_color * sky_color * diffuse;
    
    vec3 color = sky + water;
    */
    //outFragColor = vec4(1.0f,0.0f,0.0f,0.0f);
}