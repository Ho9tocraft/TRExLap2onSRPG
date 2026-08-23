#version 460

layout(location = 0) out vec3 outColor;

const vec2 kPositions[3] = vec2[](
	vec2(0.0, -0.65),
	vec2(0.65, 0.65),
	vec2(-0.65, 0.65));

const vec3 kColors[3] = vec3[](
	vec3(1.0, 0.25, 0.20),
	vec3(0.20, 1.0, 0.45),
	vec3(0.20, 0.55, 1.0));

void main()
{
	gl_Position = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
	outColor = kColors[gl_VertexIndex];
}
