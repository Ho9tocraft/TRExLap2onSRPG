#version 460

layout(set = 0, binding = 0) uniform sampler2D textureSampler;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform TextureDrawConstants
{
	vec2 halfExtentNdc;
	float encodeToUnorm;
	float historyWeight;
} drawConstants;

void main()
{
	vec4 sampled = texture(textureSampler, inUv);
	vec3 color = sampled.rgb;
	if (drawConstants.encodeToUnorm > 0.5)
		color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
	outColor = vec4(color, sampled.a);
}
