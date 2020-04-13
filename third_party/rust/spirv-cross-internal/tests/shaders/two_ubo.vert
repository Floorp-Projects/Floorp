#version 310 es

layout(std140) uniform ubo1
{
    mat4 a;
    float b;
    vec4 c[2];
};

layout(std140) uniform ubo2
{
    float d;
    vec3 e;
    vec3 f;
};

void main()
{
    gl_Position = vec4(a[1][2] + b + c[1].y + d + e.x + f.z);
}
