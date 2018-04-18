#version 450
#extension GL_ARB_separate_shader_objects : enable
//above is required for Vulkan shaders to work

//since there is no specified output variable we have to declare our own
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(binding = 1) uniform sampler2D texSampler;

void main()
{
	outColor = texture(texSampler, fragTexCoord);
}