#version 450

layout(location = 0) in vec4 input_buffer;

layout(binding = 0, std140) uniform iu
{
	uvec4 m[2];
} input_uniform;

layout(binding = 1, std430) writeonly buffer ob
{
	uvec4 m[2];
} output_buffer;

void main()
{
	gl_Position = input_buffer;
	for (int i = 0; i < 2; i++)
	{
		output_buffer.m[i] = input_uniform.m[i];
	}
}
