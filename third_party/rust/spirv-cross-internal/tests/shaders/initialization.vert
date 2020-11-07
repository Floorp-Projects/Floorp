#version 310 es

layout(location = 0) in float rand;

void main()
{
    vec4 pos;

    if (rand > 0.5) {
        pos = vec4(1.0);
    }

    gl_Position = pos;
}
