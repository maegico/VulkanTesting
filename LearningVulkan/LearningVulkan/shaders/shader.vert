#version 450
#extension GL_ARB_separate_shader_objects : enable
//above is required for Vulkan shaders to work

layout(binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

//certain values like dvec3 (double vec3) require multiple slots
//example:
	/*layout(location = 0) in dvec3 inPosition;
	layout(location = 2) in vec3 inColor;*/

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	fragColor = inColor;
	fragTexCoord = inTexCoord;
}