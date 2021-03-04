use spirv_cross_internal::{glsl, spirv};

mod common;
use crate::common::words_from_bytes;

#[test]
fn glsl_compiler_options_has_default() {
    let compiler_options = glsl::CompilerOptions::default();
    assert_eq!(compiler_options.vertex.invert_y, false);
    assert_eq!(compiler_options.vertex.transform_clip_space, false);
}

#[test]
fn ast_compiles_to_glsl() {
    let mut ast = spirv::Ast::<glsl::Target>::parse(&spirv::Module::from_words(words_from_bytes(
        include_bytes!("shaders/simple.vert.spv"),
    )))
    .unwrap();
    let mut options = glsl::CompilerOptions::default();
    options.version = glsl::Version::V4_60;
    options.enable_420_pack_extension = true;
    ast.set_compiler_options(&options).unwrap();

    assert_eq!(
        ast.compile().unwrap(),
        "\
#version 460

layout(std140) uniform uniform_buffer_object
{
    mat4 u_model_view_projection;
    float u_scale;
} _22;

layout(location = 0) out vec3 v_normal;
layout(location = 1) in vec3 a_normal;
layout(location = 0) in vec4 a_position;

void main()
{
    v_normal = a_normal;
    gl_Position = (_22.u_model_view_projection * a_position) * _22.u_scale;
}

"
    );
}

#[test]
fn ast_compiles_all_versions_to_glsl() {
    use spirv_cross_internal::glsl::Version::*;

    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let mut ast = spirv::Ast::<glsl::Target>::parse(&module).unwrap();

    let versions = [
        V1_10, V1_20, V1_30, V1_40, V1_50, V3_30, V4_00, V4_10, V4_20, V4_30, V4_40, V4_50, V4_60,
        V1_00Es, V3_00Es,
    ];
    for &version in versions.iter() {
        let mut options = glsl::CompilerOptions::default();
        options.version = version;
        options.enable_420_pack_extension = true;
        if ast.set_compiler_options(&options).is_err() {
            panic!("Did not compile");
        }
    }
}

#[test]
fn ast_renames_interface_variables() {
    let vert =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/struct.vert.spv")));
    let mut vert_ast = spirv::Ast::<glsl::Target>::parse(&vert).unwrap();
    let mut vert_options = glsl::CompilerOptions::default();
    vert_options.version = glsl::Version::V1_00Es;
    vert_options.enable_420_pack_extension = true;
    vert_ast.set_compiler_options(&vert_options).unwrap();
    let vert_stage_outputs = vert_ast.get_shader_resources().unwrap().stage_outputs;
    vert_ast
        .rename_interface_variable(&vert_stage_outputs, 0, "renamed")
        .unwrap();

    let vert_output = vert_ast.compile().unwrap();

    let frag =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/struct.frag.spv")));
    let mut frag_ast = spirv::Ast::<glsl::Target>::parse(&frag).unwrap();
    let mut frag_options = glsl::CompilerOptions::default();
    frag_options.version = glsl::Version::V1_00Es;
    frag_options.enable_420_pack_extension = true;
    frag_ast.set_compiler_options(&frag_options).unwrap();
    let frag_stage_inputs = frag_ast.get_shader_resources().unwrap().stage_inputs;
    frag_ast
        .rename_interface_variable(&frag_stage_inputs, 0, "renamed")
        .unwrap();
    let frag_output = frag_ast.compile().unwrap();

    assert_eq!(
        vert_output,
        "\
#version 100

struct SPIRV_Cross_Interface_Location0
{
    vec4 InterfaceMember0;
    vec4 InterfaceMember1;
    vec4 InterfaceMember2;
    vec4 InterfaceMember3;
};

varying vec4 renamed_InterfaceMember0;
varying vec4 renamed_InterfaceMember1;
varying vec4 renamed_InterfaceMember2;
varying vec4 renamed_InterfaceMember3;
attribute vec4 a;
attribute vec4 b;
attribute vec4 c;
attribute vec4 d;

void main()
{
    SPIRV_Cross_Interface_Location0 _20 = SPIRV_Cross_Interface_Location0(a, b, c, d);
    renamed_InterfaceMember0 = _20.InterfaceMember0;
    renamed_InterfaceMember1 = _20.InterfaceMember1;
    renamed_InterfaceMember2 = _20.InterfaceMember2;
    renamed_InterfaceMember3 = _20.InterfaceMember3;
}

"
    );

    assert_eq!(
        frag_output,
        "\
#version 100
precision mediump float;
precision highp int;

struct SPIRV_Cross_Interface_Location0
{
    vec4 InterfaceMember0;
    vec4 InterfaceMember1;
    vec4 InterfaceMember2;
    vec4 InterfaceMember3;
};

varying vec4 renamed_InterfaceMember0;
varying vec4 renamed_InterfaceMember1;
varying vec4 renamed_InterfaceMember2;
varying vec4 renamed_InterfaceMember3;

void main()
{
    gl_FragData[0] = vec4(renamed_InterfaceMember0.x, renamed_InterfaceMember1.y, renamed_InterfaceMember2.z, renamed_InterfaceMember3.w);
}

"
    );
}

#[test]
fn ast_can_rename_combined_image_samplers() {
    let mut ast = spirv::Ast::<glsl::Target>::parse(&spirv::Module::from_words(words_from_bytes(
        include_bytes!("shaders/sampler.frag.spv"),
    )))
    .unwrap();
    let mut options = glsl::CompilerOptions::default();
    options.version = glsl::Version::V4_10;
    options.enable_420_pack_extension = true;
    ast.set_compiler_options(&options).unwrap();
    for cis in ast.get_combined_image_samplers().unwrap() {
        let new_name = format!(
            "combined_sampler_{}_{}_{}",
            cis.sampler_id, cis.image_id, cis.combined_id
        );
        ast.set_name(cis.combined_id, &new_name).unwrap();
        assert_eq!(new_name, ast.get_name(cis.combined_id).unwrap());
    }

    assert_eq!(
        ast.compile().unwrap(),
        "\
#version 410
#ifdef GL_ARB_shading_language_420pack
#extension GL_ARB_shading_language_420pack : require
#endif

uniform sampler2D combined_sampler_16_12_26;

layout(location = 0) out vec4 target0;
layout(location = 0) in vec2 v_uv;

void main()
{
    target0 = texture(combined_sampler_16_12_26, v_uv);
}

"
    );
}

#[test]
fn flatten_uniform_buffers() {
    let mut ast = spirv::Ast::<glsl::Target>::parse(&spirv::Module::from_words(words_from_bytes(
        include_bytes!("shaders/two_ubo.vert.spv"),
    )))
    .unwrap();
    let mut options = glsl::CompilerOptions::default();
    options.version = glsl::Version::V3_30;
    options.emit_uniform_buffer_as_plain_uniforms = true;
    options.enable_420_pack_extension = false;

    ast.set_compiler_options(&options).unwrap();

    for uniform_buffer in &ast.get_shader_resources().unwrap().uniform_buffers {
        ast.flatten_buffer_block(uniform_buffer.id).unwrap();
    }

    assert_eq!(
        ast.compile().unwrap(),
        "\
#version 330

uniform vec4 ubo1[7];
uniform vec4 ubo2[3];
void main()
{
    gl_Position = vec4(((((ubo1[1].z + ubo1[4].x) + ubo1[6].y) + ubo2[0].x) + ubo2[1].x) + ubo2[2].z);
}

"
    );
}

#[test]
fn add_header_line() {
    let mut ast = spirv::Ast::<glsl::Target>::parse(&spirv::Module::from_words(words_from_bytes(
        include_bytes!("shaders/simple.vert.spv"),
    )))
    .unwrap();
    ast.add_header_line("// Comment").unwrap();

    assert_eq!(Some("// Comment"), ast.compile().unwrap().lines().nth(1));
}

#[test]
fn low_precision() {
    let mut ast = spirv::Ast::<glsl::Target>::parse(&spirv::Module::from_words(words_from_bytes(
        include_bytes!("shaders/sampler.frag.spv"),
    )))
    .unwrap();
    let mut options = glsl::CompilerOptions::default();
    options.version = glsl::Version::V3_00Es;
    options.fragment = glsl::CompilerFragmentOptions {
        default_float_precision: glsl::Precision::Low,
        default_int_precision: glsl::Precision::Low,
    };
    ast.set_compiler_options(&options).unwrap();

    assert_eq!(
        ast.compile().unwrap(),
        "\
#version 300 es
precision lowp float;
precision lowp int;

uniform highp sampler2D _26;

layout(location = 0) out highp vec4 target0;
in highp vec2 v_uv;

void main()
{
    target0 = texture(_26, v_uv);
}

"
    );
}

#[test]
fn forces_zero_initialization() {
    let module = spirv::Module::from_words(words_from_bytes(include_bytes!(
        "shaders/initialization.vert.spv"
    )));

    let cases = [
        (
            false,
            "\
#version 450

layout(location = 0) in float rand;

void main()
{
    vec4 pos;
    if (rand > 0.5)
    {
        pos = vec4(1.0);
    }
    gl_Position = pos;
}

",
        ),
        (
            true,
            "\
#version 450

layout(location = 0) in float rand;

void main()
{
    vec4 pos = vec4(0.0);
    if (rand > 0.5)
    {
        pos = vec4(1.0);
    }
    gl_Position = pos;
}

",
        ),
    ];
    for (force_zero_initialized_variables, expected_result) in cases.iter() {
        let mut ast = spirv::Ast::<glsl::Target>::parse(&module).unwrap();
        let mut compiler_options = glsl::CompilerOptions::default();
        compiler_options.force_zero_initialized_variables = *force_zero_initialized_variables;
        ast.set_compiler_options(&compiler_options).unwrap();
        assert_eq!(&ast.compile().unwrap(), expected_result);
    }
}

#[test]
fn ast_sets_entry_point() {
    let module = spirv::Module::from_words(words_from_bytes(include_bytes!(
        "shaders/vs_and_fs.asm.spv"
    )));

    let mut cases = vec![
        (
            None,
            "\
#version 450

void main()
{
    gl_Position = vec4(1.0);
}

",
        ),
        (
            Some((String::from("main_vs"), spirv::ExecutionModel::Vertex)),
            "\
#version 450

void main()
{
    gl_Position = vec4(1.0);
}

",
        ),
        (
            Some((String::from("main_fs"), spirv::ExecutionModel::Fragment)),
            "\
#version 450

layout(location = 0) out vec4 color;

void main()
{
    color = vec4(1.0);
}

",
        ),
    ];

    for (entry_point, expected_result) in cases.drain(..) {
        let mut ast = spirv::Ast::<glsl::Target>::parse(&module).unwrap();
        let mut compiler_options = glsl::CompilerOptions::default();
        compiler_options.entry_point = entry_point;
        ast.set_compiler_options(&compiler_options).unwrap();
        assert_eq!(&ast.compile().unwrap(), expected_result);
    }
}
