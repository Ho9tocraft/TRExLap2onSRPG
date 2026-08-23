#version 460

layout(location = 0) out vec2 outUv;

layout(push_constant) uniform TextureDrawConstants
{
	vec2 halfExtentNdc;
	float encodeToUnorm;
	float historyWeight;
} drawConstants;

const vec2 kPositions[6] = vec2[](
	vec2(-1.0, -1.0),
	vec2(1.0, -1.0),
	vec2(1.0, 1.0),
	vec2(-1.0, -1.0),
	vec2(1.0, 1.0),
	vec2(-1.0, 1.0));

const vec2 kUvs[6] = vec2[](
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(1.0, 1.0),
	vec2(0.0, 0.0),
	vec2(1.0, 1.0),
	vec2(0.0, 1.0));

void main()
{
	gl_Position = vec4(kPositions[gl_VertexIndex] * drawConstants.halfExtentNdc, 0.0, 1.0);
	outUv = kUvs[gl_VertexIndex];
}
