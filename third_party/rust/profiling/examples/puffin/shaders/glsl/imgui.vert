#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "imgui.glsl"

// @[semantic("POSITION")]
layout(location = 0) in vec2 pos;

// @[semantic("TEXCOORD")]
layout(location = 1) in vec2 in_uv;

// @[semantic("COLOR")]
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 color;

void main() {
    uv = in_uv;
    color = in_color;
    gl_Position = uniform_buffer.mvp * vec4(pos.x, pos.y, 0.0, 1.0);
}