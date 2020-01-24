use spirv_cross_internal::{hlsl, spirv};

mod common;
use crate::common::words_from_bytes;

#[test]
fn hlsl_compiler_options_has_default() {
    let compiler_options = hlsl::CompilerOptions::default();
    assert_eq!(compiler_options.shader_model, hlsl::ShaderModel::V3_0);
    assert_eq!(compiler_options.point_size_compat, false);
    assert_eq!(compiler_options.point_coord_compat, false);
    assert_eq!(compiler_options.vertex.invert_y, false);
    assert_eq!(compiler_options.vertex.transform_clip_space, false);
}

#[test]
fn ast_compiles_to_hlsl() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let mut ast = spirv::Ast::<hlsl::Target>::parse(&module).unwrap();
    ast.set_compiler_options(&hlsl::CompilerOptions {
        shader_model: hlsl::ShaderModel::V6_0,
        point_size_compat: false,
        point_coord_compat: false,
        vertex: hlsl::CompilerVertexOptions::default(),
    })
    .unwrap();

    assert_eq!(
        ast.compile().unwrap(),
        "\
cbuffer uniform_buffer_object
{
    row_major float4x4 _22_u_model_view_projection : packoffset(c0);
    float _22_u_scale : packoffset(c4);
};


static float4 gl_Position;
static float3 v_normal;
static float3 a_normal;
static float4 a_position;

struct SPIRV_Cross_Input
{
    float4 a_position : TEXCOORD0;
    float3 a_normal : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float3 v_normal : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    v_normal = a_normal;
    gl_Position = mul(a_position, _22_u_model_view_projection) * _22_u_scale;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    a_normal = stage_input.a_normal;
    a_position = stage_input.a_position;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.v_normal = v_normal;
    return stage_output;
}
"
    );
}

#[test]
fn ast_compiles_all_shader_models_to_hlsl() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let mut ast = spirv::Ast::<hlsl::Target>::parse(&module).unwrap();

    let shader_models = [
        hlsl::ShaderModel::V3_0,
        hlsl::ShaderModel::V4_0,
        hlsl::ShaderModel::V4_0L9_0,
        hlsl::ShaderModel::V4_0L9_1,
        hlsl::ShaderModel::V4_0L9_3,
        hlsl::ShaderModel::V4_1,
        hlsl::ShaderModel::V5_0,
        hlsl::ShaderModel::V5_1,
        hlsl::ShaderModel::V6_0,
    ];
    for &shader_model in shader_models.iter() {
        if ast
            .set_compiler_options(&hlsl::CompilerOptions {
                shader_model,
                point_size_compat: false,
                point_coord_compat: false,
                vertex: hlsl::CompilerVertexOptions::default(),
            })
            .is_err()
        {
            panic!("Did not compile");
        }
    }
}
