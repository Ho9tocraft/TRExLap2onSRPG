#version 460

layout(set = 0, binding = 0) uniform sampler2D currentColorSampler;
layout(set = 0, binding = 1) uniform sampler2D historyColorSampler;

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
	vec4 currentColor = texture(currentColorSampler, inUv);
	vec4 historyColor = texture(historyColorSampler, inUv);
	outColor = mix(currentColor, historyColor, drawConstants.historyWeight);
}
