#version 310 es
precision mediump float;

struct W
{
    vec4 e;
    vec4 f;
    vec4 g;
    vec4 h;
};

layout(location = 0) in W w;

layout(location = 0) out vec4 color;

void main() {
    color = vec4(w.e.x, w.f.y, w.g.z, w.h.w);
}
