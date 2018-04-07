#version 450
#extension GL_ARB_separate_shader_objects : enable
//above is required for Vulkan shaders to work

//since there is no specified output variable we have to declare our own
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;

void main()
{
	outColor = vec4(fragColor, 1.0);
}