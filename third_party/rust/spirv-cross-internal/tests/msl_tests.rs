use spirv_cross_internal::{msl, spirv};

use std::collections::BTreeMap;

mod common;
use crate::common::words_from_bytes;

#[test]
fn msl_compiler_options_has_default() {
    let compiler_options = msl::CompilerOptions::default();
    assert_eq!(compiler_options.vertex.invert_y, false);
    assert_eq!(compiler_options.vertex.transform_clip_space, false);
    assert!(compiler_options.resource_binding_overrides.is_empty());
    assert!(compiler_options.vertex_attribute_overrides.is_empty());
}

#[test]
fn is_rasterization_enabled() {
    let modules = [
        (
            true,
            spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv"))),
        ),
        (
            false,
            spirv::Module::from_words(words_from_bytes(include_bytes!(
                "shaders/rasterize_disabled.vert.spv"
            ))),
        ),
    ];
    for (expected, module) in &modules {
        let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();
        ast.compile().unwrap();
        assert_eq!(*expected, ast.is_rasterization_enabled().unwrap());
    }
}

#[test]
fn ast_compiles_to_msl() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();

    let mut compiler_options = msl::CompilerOptions::default();

    compiler_options.resource_binding_overrides.insert(
        msl::ResourceBindingLocation {
            stage: spirv::ExecutionModel::Vertex,
            desc_set: 0,
            binding: 0,
        },
        msl::ResourceBinding {
            buffer_id: 5,
            texture_id: 6,
            sampler_id: 7,
            count: 0,
        },
    );

    ast.set_compiler_options(&compiler_options).unwrap();
    assert_eq!(
        ast.compile().unwrap(),
        "\
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct uniform_buffer_object
{
    float4x4 u_model_view_projection;
    float u_scale;
};

struct main0_out
{
    float3 v_normal [[user(locn0)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float4 a_position [[attribute(0)]];
    float3 a_normal [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniform_buffer_object& _22 [[buffer(5)]])
{
    main0_out out = {};
    out.v_normal = in.a_normal;
    out.gl_Position = (_22.u_model_view_projection * in.a_position) * _22.u_scale;
    return out;
}

"
    );
    assert_eq!(
        ast.get_cleansed_entry_point_name("main", spirv::ExecutionModel::Vertex)
            .unwrap(),
        "main0"
    );
}

#[test]
fn captures_output_to_buffer() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();
    let mut compiler_options = msl::CompilerOptions::default();
    compiler_options.capture_output_to_buffer = true;
    compiler_options.output_buffer_index = 456;
    ast.set_compiler_options(&compiler_options).unwrap();
    assert_eq!(
        ast.compile().unwrap(),
        "\
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct uniform_buffer_object
{
    float4x4 u_model_view_projection;
    float u_scale;
};

struct main0_out
{
    float3 v_normal [[user(locn0)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float4 a_position [[attribute(0)]];
    float3 a_normal [[attribute(1)]];
};

vertex void main0(main0_in in [[stage_in]], constant uniform_buffer_object& _22 [[buffer(0)]], uint gl_VertexIndex [[vertex_id]], uint gl_BaseVertex [[base_vertex]], uint gl_InstanceIndex [[instance_id]], uint gl_BaseInstance [[base_instance]], device main0_out* spvOut [[buffer(456)]], device uint* spvIndirectParams [[buffer(29)]])
{
    device main0_out& out = spvOut[(gl_InstanceIndex - gl_BaseInstance) * spvIndirectParams[0] + gl_VertexIndex - gl_BaseVertex];
    out.v_normal = in.a_normal;
    out.gl_Position = (_22.u_model_view_projection * in.a_position) * _22.u_scale;
}

"
    );
}

#[test]
fn swizzles_texture_samples() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/sampler.frag.spv")));
    let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();
    let mut compiler_options = msl::CompilerOptions::default();
    compiler_options.swizzle_texture_samples = true;
    compiler_options.swizzle_buffer_index = 123;
    ast.set_compiler_options(&compiler_options).unwrap();
    assert_eq!(
        ast.compile().unwrap(),
        "\
#pragma clang diagnostic ignored \"-Wmissing-prototypes\"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 target0 [[color(0)]];
};

struct main0_in
{
    float2 v_uv [[user(locn0)]];
};

template<typename T> struct spvRemoveReference { typedef T type; };
template<typename T> struct spvRemoveReference<thread T&> { typedef T type; };
template<typename T> struct spvRemoveReference<thread T&&> { typedef T type; };
template<typename T> inline constexpr thread T&& spvForward(thread typename spvRemoveReference<T>::type& x)
{
    return static_cast<thread T&&>(x);
}
template<typename T> inline constexpr thread T&& spvForward(thread typename spvRemoveReference<T>::type&& x)
{
    return static_cast<thread T&&>(x);
}

enum class spvSwizzle : uint
{
    none = 0,
    zero,
    one,
    red,
    green,
    blue,
    alpha
};

template<typename T>
inline T spvGetSwizzle(vec<T, 4> x, T c, spvSwizzle s)
{
    switch (s)
    {
        case spvSwizzle::none:
            return c;
        case spvSwizzle::zero:
            return 0;
        case spvSwizzle::one:
            return 1;
        case spvSwizzle::red:
            return x.r;
        case spvSwizzle::green:
            return x.g;
        case spvSwizzle::blue:
            return x.b;
        case spvSwizzle::alpha:
            return x.a;
    }
}

// Wrapper function that swizzles texture samples and fetches.
template<typename T>
inline vec<T, 4> spvTextureSwizzle(vec<T, 4> x, uint s)
{
    if (!s)
        return x;
    return vec<T, 4>(spvGetSwizzle(x, x.r, spvSwizzle((s >> 0) & 0xFF)), spvGetSwizzle(x, x.g, spvSwizzle((s >> 8) & 0xFF)), spvGetSwizzle(x, x.b, spvSwizzle((s >> 16) & 0xFF)), spvGetSwizzle(x, x.a, spvSwizzle((s >> 24) & 0xFF)));
}

template<typename T>
inline T spvTextureSwizzle(T x, uint s)
{
    return spvTextureSwizzle(vec<T, 4>(x, 0, 0, 1), s).x;
}

fragment main0_out main0(main0_in in [[stage_in]], constant uint* spvSwizzleConstants [[buffer(123)]], texture2d<float> u_texture [[texture(0)]], sampler u_sampler [[sampler(0)]])
{
    main0_out out = {};
    constant uint& u_textureSwzl = spvSwizzleConstants[0];
    out.target0 = spvTextureSwizzle(u_texture.sample(u_sampler, in.v_uv), u_textureSwzl);
    return out;
}

"
    );
}

#[test]
fn sets_argument_buffer_index() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/sampler.frag.spv")));
    let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();
    let mut resource_binding_overrides = BTreeMap::new();
    resource_binding_overrides.insert(
        msl::ResourceBindingLocation {
            stage: spirv::ExecutionModel::Fragment,
            desc_set: 0,
            binding: msl::ARGUMENT_BUFFER_BINDING,
        },
        msl::ResourceBinding {
            buffer_id: 2,
            texture_id: 0,
            sampler_id: 0,
            count: 0,
        },
    );
    let mut compiler_options = msl::CompilerOptions::default();
    compiler_options.resource_binding_overrides = resource_binding_overrides;
    compiler_options.version = msl::Version::V2_0;
    compiler_options.enable_argument_buffers = true;
    ast.set_compiler_options(&compiler_options).unwrap();
    assert_eq!(
        ast.compile().unwrap(),
        "\
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct spvDescriptorSetBuffer0
{
    texture2d<float> u_texture [[id(0)]];
    sampler u_sampler [[id(1)]];
};

struct main0_out
{
    float4 target0 [[color(0)]];
};

struct main0_in
{
    float2 v_uv [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(2)]])
{
    main0_out out = {};
    out.target0 = spvDescriptorSet0.u_texture.sample(spvDescriptorSet0.u_sampler, in.v_uv);
    return out;
}

",
    );
}

#[test]
fn forces_native_array() {
    let module = spirv::Module::from_words(words_from_bytes(include_bytes!(
        "shaders/const_array.vert.spv"
    )));

    let cases = [
        (
            false,
            "\
#pragma clang diagnostic ignored \"-Wmissing-prototypes\"
#pragma clang diagnostic ignored \"-Wmissing-braces\"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

constant spvUnsafeArray<float2, 3> _23 = spvUnsafeArray<float2, 3>({ float2(0.0, -0.5), float2(0.5), float2(-0.5, 0.5) });

struct main0_out
{
    float4 gl_Position [[position]];
};

vertex main0_out main0(uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    out.gl_Position = float4(_23[int(gl_VertexIndex)], 0.0, 1.0);
    return out;
}

"
        ),
        (
            true,
            "\
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant float2 _23[3] = { float2(0.0, -0.5), float2(0.5), float2(-0.5, 0.5) };

struct main0_out
{
    float4 gl_Position [[position]];
};

vertex main0_out main0(uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    out.gl_Position = float4(_23[int(gl_VertexIndex)], 0.0, 1.0);
    return out;
}

"
        )
    ];
    for (force_native_arrays, expected_result) in cases.iter() {
        let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();
        let mut compiler_options = msl::CompilerOptions::default();
        compiler_options.force_native_arrays = *force_native_arrays;
        ast.set_compiler_options(&compiler_options).unwrap();
        assert_eq!(&ast.compile().unwrap(), expected_result);
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
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 gl_Position [[position]];
};

struct main0_in
{
    float rand [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    float4 pos;
    if (in.rand > 0.5)
    {
        pos = float4(1.0);
    }
    out.gl_Position = pos;
    return out;
}

",
        ),
        (
            true,
            "\
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 gl_Position [[position]];
};

struct main0_in
{
    float rand [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    float4 pos = {};
    if (in.rand > 0.5)
    {
        pos = float4(1.0);
    }
    out.gl_Position = pos;
    return out;
}

",
        ),
    ];
    for (force_zero_initialized_variables, expected_result) in cases.iter() {
        let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();
        let mut compiler_options = msl::CompilerOptions::default();
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
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main_vs_out
{
    float4 gl_Position [[position]];
};

vertex main_vs_out main_vs()
{
    main_vs_out out = {};
    out.gl_Position = float4(1.0);
    return out;
}

",
        ),
        (
            Some((String::from("main_vs"), spirv::ExecutionModel::Vertex)),
            "\
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main_vs_out
{
    float4 gl_Position [[position]];
};

vertex main_vs_out main_vs()
{
    main_vs_out out = {};
    out.gl_Position = float4(1.0);
    return out;
}

",
        ),
        (
            Some((String::from("main_fs"), spirv::ExecutionModel::Fragment)),
            "\
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main_fs_out
{
    float4 color [[color(0)]];
};

fragment main_fs_out main_fs()
{
    main_fs_out out = {};
    out.color = float4(1.0);
    return out;
}

",
        ),
    ];

    for (entry_point, expected_result) in cases.drain(..) {
        let mut ast = spirv::Ast::<msl::Target>::parse(&module).unwrap();
        let mut compiler_options = msl::CompilerOptions::default();
        compiler_options.entry_point = entry_point;
        ast.set_compiler_options(&compiler_options).unwrap();
        assert_eq!(&ast.compile().unwrap(), expected_result);
    }
}
