#version 460 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outFragPos;
layout (location = 1) out vec2 outUV;

layout (set = 0, binding = 0) uniform sampler2D displacement_map;

struct Vertex{
	vec3 position;
	vec2 uv;
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


void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec3 position = v.position + texture(displacement_map, v.uv).rgb * (512.f/PushConstants.ocean_size);
	gl_Position = PushConstants.mvp * vec4(position, 1.f);

	outFragPos = position;
	outUV = v.uv;

}