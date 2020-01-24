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
    ast.set_compiler_options(&glsl::CompilerOptions {
        version: glsl::Version::V4_60,
        vertex: glsl::CompilerVertexOptions::default(),
    })
    .unwrap();

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
        if ast
            .set_compiler_options(&glsl::CompilerOptions {
                version,
                vertex: glsl::CompilerVertexOptions::default(),
            })
            .is_err()
        {
            panic!("Did not compile");
        }
    }
}

#[test]
fn ast_renames_interface_variables() {
    let vert =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/struct.vert.spv")));
    let mut vert_ast = spirv::Ast::<glsl::Target>::parse(&vert).unwrap();
    vert_ast
        .set_compiler_options(&glsl::CompilerOptions {
            version: glsl::Version::V1_00Es,
            vertex: glsl::CompilerVertexOptions::default(),
        })
        .unwrap();
    let vert_stage_outputs = vert_ast.get_shader_resources().unwrap().stage_outputs;
    vert_ast
        .rename_interface_variable(&vert_stage_outputs, 0, "renamed")
        .unwrap();

    let vert_output = vert_ast.compile().unwrap();

    let frag =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/struct.frag.spv")));
    let mut frag_ast = spirv::Ast::<glsl::Target>::parse(&frag).unwrap();
    frag_ast
        .set_compiler_options(&glsl::CompilerOptions {
            version: glsl::Version::V1_00Es,
            vertex: glsl::CompilerVertexOptions::default(),
        })
        .unwrap();
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
    {
        SPIRV_Cross_Interface_Location0 renamed = SPIRV_Cross_Interface_Location0(a, b, c, d);
        renamed_InterfaceMember0 = renamed.InterfaceMember0;
        renamed_InterfaceMember1 = renamed.InterfaceMember1;
        renamed_InterfaceMember2 = renamed.InterfaceMember2;
        renamed_InterfaceMember3 = renamed.InterfaceMember3;
    }
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
    ast.set_compiler_options(&glsl::CompilerOptions {
        version: glsl::Version::V4_10,
        vertex: glsl::CompilerVertexOptions::default(),
    })
    .unwrap();
    for cis in ast.get_combined_image_samplers().unwrap() {
        let new_name = "combined_sampler".to_string()
            + "_"
            + &cis.sampler_id.to_string()
            + "_"
            + &cis.image_id.to_string()
            + "_"
            + &cis.combined_id.to_string();
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
