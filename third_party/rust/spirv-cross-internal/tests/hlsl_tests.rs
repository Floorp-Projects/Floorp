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
    let mut options = hlsl::CompilerOptions::default();
    options.shader_model = hlsl::ShaderModel::V6_0;
    ast.set_compiler_options(&options).unwrap();

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
        let mut options = hlsl::CompilerOptions::default();
        options.shader_model = shader_model;
        assert!(ast.set_compiler_options(&options).is_ok());
    }
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
uniform float4 gl_HalfPixel;

static float4 gl_Position;
static float rand;

struct SPIRV_Cross_Input
{
    float rand : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 gl_Position : POSITION;
};

void vert_main()
{
    float4 pos;
    if (rand > 0.5f)
    {
        pos = 1.0f.xxxx;
    }
    gl_Position = pos;
    gl_Position.x = gl_Position.x - gl_HalfPixel.x * gl_Position.w;
    gl_Position.y = gl_Position.y + gl_HalfPixel.y * gl_Position.w;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    rand = stage_input.rand;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
",
        ),
        (
            true,
            "\
uniform float4 gl_HalfPixel;

static float4 gl_Position;
static float rand;

struct SPIRV_Cross_Input
{
    float rand : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 gl_Position : POSITION;
};

void vert_main()
{
    float4 pos = 0.0f.xxxx;
    if (rand > 0.5f)
    {
        pos = 1.0f.xxxx;
    }
    gl_Position = pos;
    gl_Position.x = gl_Position.x - gl_HalfPixel.x * gl_Position.w;
    gl_Position.y = gl_Position.y + gl_HalfPixel.y * gl_Position.w;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    rand = stage_input.rand;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
",
        ),
    ];
    for (force_zero_initialized_variables, expected_result) in cases.iter() {
        let mut ast = spirv::Ast::<hlsl::Target>::parse(&module).unwrap();
        let mut compiler_options = hlsl::CompilerOptions::default();
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
uniform float4 gl_HalfPixel;

static float4 gl_Position;
struct SPIRV_Cross_Output
{
    float4 gl_Position : POSITION;
};

void vert_main()
{
    gl_Position = 1.0f.xxxx;
    gl_Position.x = gl_Position.x - gl_HalfPixel.x * gl_Position.w;
    gl_Position.y = gl_Position.y + gl_HalfPixel.y * gl_Position.w;
}

SPIRV_Cross_Output main()
{
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
",
        ),
        (
            Some((String::from("main_vs"), spirv::ExecutionModel::Vertex)),
            "\
uniform float4 gl_HalfPixel;

static float4 gl_Position;
struct SPIRV_Cross_Output
{
    float4 gl_Position : POSITION;
};

void vert_main()
{
    gl_Position = 1.0f.xxxx;
    gl_Position.x = gl_Position.x - gl_HalfPixel.x * gl_Position.w;
    gl_Position.y = gl_Position.y + gl_HalfPixel.y * gl_Position.w;
}

SPIRV_Cross_Output main()
{
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
",
        ),
        (
            Some((String::from("main_fs"), spirv::ExecutionModel::Fragment)),
            "\
static float4 color;

struct SPIRV_Cross_Output
{
    float4 color : COLOR0;
};

void frag_main()
{
    color = 1.0f.xxxx;
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.color = float4(color);
    return stage_output;
}
",
        ),
    ];

    for (entry_point, expected_result) in cases.drain(..) {
        let mut ast = spirv::Ast::<hlsl::Target>::parse(&module).unwrap();
        let mut compiler_options = hlsl::CompilerOptions::default();
        compiler_options.entry_point = entry_point;
        ast.set_compiler_options(&compiler_options).unwrap();
        assert_eq!(&ast.compile().unwrap(), expected_result);
    }
}
