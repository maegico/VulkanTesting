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
	//basic usage of textures
	//outColor = texture(texSampler, fragTexCoord);

	//showing what happens with repeat mode when UVWs pass 1
	//outColor = texture(texSampler, fragTexCoord*2.0);

	//combining color with texture color
	outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);

	//combining color with texture color with repeat mode when UVWs pass 1
	outColor = vec4(fragColor * texture(texSampler, fragTexCoord*2.0).rgb, 1.0);
}
