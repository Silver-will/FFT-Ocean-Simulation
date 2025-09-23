#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outFragPos;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;


layout (set = 0, binding = 0) uniform sampler2D displacement_map;

struct Vertex{
	vec4 position;
	vec2 uv;
	vec2 pad;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};


layout( push_constant ) uniform constants
{
	mat4 mvp;
	VertexBuffer vertexBuffer;
	uint ocean_size;
} PushConstants;


vec3 CalcSlopeNormal(vec2 texCoord)
{   
	float textureDelta = 1/512.0;
	
	float left = texture(displacement_map, texCoord + vec2(-textureDelta,0)).r;
	float right = texture(displacement_map, texCoord + vec2(textureDelta,0)).r;
	float up = texture(displacement_map, texCoord + vec2(0,textureDelta)).r;
	float down = texture(displacement_map, texCoord + vec2(0,-textureDelta)).r;
	
	vec3 normal = normalize(vec3(left - right,1.0f, up - down));
	return normalize(normal);
}

void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 disp = texture(displacement_map, v.uv).rgba;
	//disp = disp / PushConstants.ocean_size;
	vec3 position = v.position.xyz;/* + texture(displacement_map, v.uv).rgb;*/
	
	vec4 displaced_pos = vec4(position + disp.rgb,1.f);
	gl_Position = PushConstants.mvp * displaced_pos;
	
	outNormal = CalcSlopeNormal(v.uv);
	outFragPos = displaced_pos.rgb;
	outUV = v.uv;

}