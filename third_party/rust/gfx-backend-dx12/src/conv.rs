use crate::validate_line_width;

use spirv_cross::spirv;
use std::mem;

use winapi::{
    shared::{
        basetsd::UINT8,
        dxgiformat::*,
        minwindef::{FALSE, INT, TRUE, UINT},
    },
    um::{d3d12::*, d3dcommon::*},
};

use auxil::ShaderStage;
use hal::{
    buffer,
    format::{Format, ImageFeature, SurfaceType, Swizzle},
    image, pso,
};

use native::ShaderVisibility;

fn is_little_endinan() -> bool {
    unsafe { 1 == *(&1u32 as *const _ as *const u8) }
}

pub fn map_format(format: Format) -> Option<DXGI_FORMAT> {
    use hal::format::Format::*;

    // Handling packed formats according to the platform endianness.
    let reverse = is_little_endinan();
    let format = match format {
        Bgra4Unorm if !reverse => DXGI_FORMAT_B4G4R4A4_UNORM,
        R5g6b5Unorm if reverse => DXGI_FORMAT_B5G6R5_UNORM,
        B5g6r5Unorm if !reverse => DXGI_FORMAT_B5G6R5_UNORM,
        B5g5r5a1Unorm if !reverse => DXGI_FORMAT_B5G5R5A1_UNORM,
        A1r5g5b5Unorm if reverse => DXGI_FORMAT_B5G5R5A1_UNORM,
        R8Unorm => DXGI_FORMAT_R8_UNORM,
        R8Snorm => DXGI_FORMAT_R8_SNORM,
        R8Uint => DXGI_FORMAT_R8_UINT,
        R8Sint => DXGI_FORMAT_R8_SINT,
        Rg8Unorm => DXGI_FORMAT_R8G8_UNORM,
        Rg8Snorm => DXGI_FORMAT_R8G8_SNORM,
        Rg8Uint => DXGI_FORMAT_R8G8_UINT,
        Rg8Sint => DXGI_FORMAT_R8G8_SINT,
        Rgba8Unorm => DXGI_FORMAT_R8G8B8A8_UNORM,
        Rgba8Snorm => DXGI_FORMAT_R8G8B8A8_SNORM,
        Rgba8Uint => DXGI_FORMAT_R8G8B8A8_UINT,
        Rgba8Sint => DXGI_FORMAT_R8G8B8A8_SINT,
        Rgba8Srgb => DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        Bgra8Unorm => DXGI_FORMAT_B8G8R8A8_UNORM,
        Bgra8Srgb => DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        Abgr8Unorm if reverse => DXGI_FORMAT_R8G8B8A8_UNORM,
        Abgr8Snorm if reverse => DXGI_FORMAT_R8G8B8A8_SNORM,
        Abgr8Uint if reverse => DXGI_FORMAT_R8G8B8A8_UINT,
        Abgr8Sint if reverse => DXGI_FORMAT_R8G8B8A8_SINT,
        Abgr8Srgb if reverse => DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        A2b10g10r10Unorm if reverse => DXGI_FORMAT_R10G10B10A2_UNORM,
        A2b10g10r10Uint if reverse => DXGI_FORMAT_R10G10B10A2_UINT,
        R16Unorm => DXGI_FORMAT_R16_UNORM,
        R16Snorm => DXGI_FORMAT_R16_SNORM,
        R16Uint => DXGI_FORMAT_R16_UINT,
        R16Sint => DXGI_FORMAT_R16_SINT,
        R16Sfloat => DXGI_FORMAT_R16_FLOAT,
        Rg16Unorm => DXGI_FORMAT_R16G16_UNORM,
        Rg16Snorm => DXGI_FORMAT_R16G16_SNORM,
        Rg16Uint => DXGI_FORMAT_R16G16_UINT,
        Rg16Sint => DXGI_FORMAT_R16G16_SINT,
        Rg16Sfloat => DXGI_FORMAT_R16G16_FLOAT,
        Rgba16Unorm => DXGI_FORMAT_R16G16B16A16_UNORM,
        Rgba16Snorm => DXGI_FORMAT_R16G16B16A16_SNORM,
        Rgba16Uint => DXGI_FORMAT_R16G16B16A16_UINT,
        Rgba16Sint => DXGI_FORMAT_R16G16B16A16_SINT,
        Rgba16Sfloat => DXGI_FORMAT_R16G16B16A16_FLOAT,
        R32Uint => DXGI_FORMAT_R32_UINT,
        R32Sint => DXGI_FORMAT_R32_SINT,
        R32Sfloat => DXGI_FORMAT_R32_FLOAT,
        Rg32Uint => DXGI_FORMAT_R32G32_UINT,
        Rg32Sint => DXGI_FORMAT_R32G32_SINT,
        Rg32Sfloat => DXGI_FORMAT_R32G32_FLOAT,
        Rgb32Uint => DXGI_FORMAT_R32G32B32_UINT,
        Rgb32Sint => DXGI_FORMAT_R32G32B32_SINT,
        Rgb32Sfloat => DXGI_FORMAT_R32G32B32_FLOAT,
        Rgba32Uint => DXGI_FORMAT_R32G32B32A32_UINT,
        Rgba32Sint => DXGI_FORMAT_R32G32B32A32_SINT,
        Rgba32Sfloat => DXGI_FORMAT_R32G32B32A32_FLOAT,
        B10g11r11Ufloat if reverse => DXGI_FORMAT_R11G11B10_FLOAT,
        E5b9g9r9Ufloat if reverse => DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
        D16Unorm => DXGI_FORMAT_D16_UNORM,
        D24UnormS8Uint => DXGI_FORMAT_D24_UNORM_S8_UINT,
        X8D24Unorm if reverse => DXGI_FORMAT_D24_UNORM_S8_UINT,
        D32Sfloat => DXGI_FORMAT_D32_FLOAT,
        D32SfloatS8Uint => DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
        Bc1RgbUnorm | Bc1RgbaUnorm => DXGI_FORMAT_BC1_UNORM,
        Bc1RgbSrgb | Bc1RgbaSrgb => DXGI_FORMAT_BC1_UNORM_SRGB,
        Bc2Unorm => DXGI_FORMAT_BC2_UNORM,
        Bc2Srgb => DXGI_FORMAT_BC2_UNORM_SRGB,
        Bc3Unorm => DXGI_FORMAT_BC3_UNORM,
        Bc3Srgb => DXGI_FORMAT_BC3_UNORM_SRGB,
        Bc4Unorm => DXGI_FORMAT_BC4_UNORM,
        Bc4Snorm => DXGI_FORMAT_BC4_SNORM,
        Bc5Unorm => DXGI_FORMAT_BC5_UNORM,
        Bc5Snorm => DXGI_FORMAT_BC5_SNORM,
        Bc6hUfloat => DXGI_FORMAT_BC6H_UF16,
        Bc6hSfloat => DXGI_FORMAT_BC6H_SF16,
        Bc7Unorm => DXGI_FORMAT_BC7_UNORM,
        Bc7Srgb => DXGI_FORMAT_BC7_UNORM_SRGB,

        _ => return None,
    };

    Some(format)
}

pub fn map_format_shader_depth(surface: SurfaceType) -> Option<DXGI_FORMAT> {
    match surface {
        SurfaceType::D16 => Some(DXGI_FORMAT_R16_UNORM),
        SurfaceType::X8D24 | SurfaceType::D24_S8 => Some(DXGI_FORMAT_R24_UNORM_X8_TYPELESS),
        SurfaceType::D32 => Some(DXGI_FORMAT_R32_FLOAT),
        SurfaceType::D32_S8 => Some(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS),
        _ => return None,
    }
}

pub fn map_format_shader_stencil(surface: SurfaceType) -> Option<DXGI_FORMAT> {
    match surface {
        SurfaceType::D24_S8 => Some(DXGI_FORMAT_X24_TYPELESS_G8_UINT),
        SurfaceType::D32_S8 => Some(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT),
        _ => None,
    }
}

pub fn map_format_dsv(surface: SurfaceType) -> Option<DXGI_FORMAT> {
    match surface {
        SurfaceType::D16 => Some(DXGI_FORMAT_D16_UNORM),
        SurfaceType::X8D24 | SurfaceType::D24_S8 => Some(DXGI_FORMAT_D24_UNORM_S8_UINT),
        SurfaceType::D32 => Some(DXGI_FORMAT_D32_FLOAT),
        SurfaceType::D32_S8 => Some(DXGI_FORMAT_D32_FLOAT_S8X24_UINT),
        _ => None,
    }
}

pub fn map_format_nosrgb(format: Format) -> Option<DXGI_FORMAT> {
    // NOTE: DXGI doesn't allow sRGB format on the swapchain, but
    //       creating RTV of swapchain buffers with sRGB works
    match format {
        Format::Bgra8Srgb => Some(DXGI_FORMAT_B8G8R8A8_UNORM),
        Format::Rgba8Srgb => Some(DXGI_FORMAT_R8G8B8A8_UNORM),
        _ => map_format(format),
    }
}

pub fn map_swizzle(swizzle: Swizzle) -> UINT {
    use hal::format::Component::*;

    [swizzle.0, swizzle.1, swizzle.2, swizzle.3]
        .iter()
        .enumerate()
        .fold(
            D3D12_SHADER_COMPONENT_MAPPING_ALWAYS_SET_BIT_AVOIDING_ZEROMEM_MISTAKES,
            |mapping, (i, &component)| {
                let value = match component {
                    R => D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
                    G => D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
                    B => D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
                    A => D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3,
                    Zero => D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0,
                    One => D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
                };
                mapping | (value << D3D12_SHADER_COMPONENT_MAPPING_SHIFT as usize * i)
            },
        )
}

pub fn swizzle_rg(swizzle: Swizzle) -> Swizzle {
    use hal::format::Component as C;
    fn map_component(c: C) -> C {
        match c {
            C::R => C::G,
            C::G => C::R,
            x => x,
        }
    }
    Swizzle(
        map_component(swizzle.0),
        map_component(swizzle.1),
        map_component(swizzle.2),
        map_component(swizzle.3),
    )
}

pub fn map_surface_type(st: SurfaceType) -> Option<DXGI_FORMAT> {
    use hal::format::SurfaceType::*;

    assert_eq!(1, unsafe { *(&1u32 as *const _ as *const u8) });
    Some(match st {
        R5_G6_B5 => DXGI_FORMAT_B5G6R5_UNORM,
        A1_R5_G5_B5 => DXGI_FORMAT_B5G5R5A1_UNORM,
        R8 => DXGI_FORMAT_R8_TYPELESS,
        R8_G8 => DXGI_FORMAT_R8G8_TYPELESS,
        R8_G8_B8_A8 => DXGI_FORMAT_R8G8B8A8_TYPELESS,
        B8_G8_R8_A8 => DXGI_FORMAT_B8G8R8A8_TYPELESS,
        A8_B8_G8_R8 => DXGI_FORMAT_R8G8B8A8_TYPELESS,
        A2_B10_G10_R10 => DXGI_FORMAT_R10G10B10A2_TYPELESS,
        R16 => DXGI_FORMAT_R16_TYPELESS,
        R16_G16 => DXGI_FORMAT_R16G16_TYPELESS,
        R16_G16_B16_A16 => DXGI_FORMAT_R16G16B16A16_TYPELESS,
        R32 => DXGI_FORMAT_R32_TYPELESS,
        R32_G32 => DXGI_FORMAT_R32G32_TYPELESS,
        R32_G32_B32 => DXGI_FORMAT_R32G32B32_TYPELESS,
        R32_G32_B32_A32 => DXGI_FORMAT_R32G32B32A32_TYPELESS,
        B10_G11_R11 => DXGI_FORMAT_R11G11B10_FLOAT,
        E5_B9_G9_R9 => DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
        D16 => DXGI_FORMAT_R16_TYPELESS,
        X8D24 => DXGI_FORMAT_R24G8_TYPELESS,
        D32 => DXGI_FORMAT_R32_TYPELESS,
        D24_S8 => DXGI_FORMAT_R24G8_TYPELESS,
        D32_S8 => DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        _ => return None,
    })
}

pub fn map_topology_type(primitive: pso::Primitive) -> D3D12_PRIMITIVE_TOPOLOGY_TYPE {
    use hal::pso::Primitive::*;
    match primitive {
        PointList => D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        LineList | LineStrip => D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
        TriangleList | TriangleStrip => D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        PatchList(_) => D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
    }
}

pub fn map_topology(ia: &pso::InputAssemblerDesc) -> D3D12_PRIMITIVE_TOPOLOGY {
    use hal::pso::Primitive::*;
    match (ia.primitive, ia.with_adjacency) {
        (PointList, false) => D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
        (PointList, true) => panic!("Points can't have adjacency info"),
        (LineList, false) => D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        (LineList, true) => D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
        (LineStrip, false) => D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
        (LineStrip, true) => D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
        (TriangleList, false) => D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        (TriangleList, true) => D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
        (TriangleStrip, false) => D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        (TriangleStrip, true) => D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,
        (PatchList(num), false) => {
            assert!(num != 0);
            D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (num as u32) - 1
        }
        (_, true) => panic!("Patches can't have adjacency info"),
    }
}

pub fn map_rasterizer(rasterizer: &pso::Rasterizer, multisample: bool) -> D3D12_RASTERIZER_DESC {
    use hal::pso::FrontFace::*;
    use hal::pso::PolygonMode::*;

    let bias = match rasterizer.depth_bias {
        //TODO: support dynamic depth bias
        Some(pso::State::Static(db)) => db,
        Some(_) | None => pso::DepthBias::default(),
    };

    if let pso::State::Static(w) = rasterizer.line_width {
        validate_line_width(w);
    }

    D3D12_RASTERIZER_DESC {
        FillMode: match rasterizer.polygon_mode {
            Point => {
                error!("Point rasterization is not supported");
                D3D12_FILL_MODE_WIREFRAME
            }
            Line => D3D12_FILL_MODE_WIREFRAME,
            Fill => D3D12_FILL_MODE_SOLID,
        },
        CullMode: match rasterizer.cull_face {
            pso::Face::NONE => D3D12_CULL_MODE_NONE,
            pso::Face::FRONT => D3D12_CULL_MODE_FRONT,
            pso::Face::BACK => D3D12_CULL_MODE_BACK,
            _ => panic!("Culling both front and back faces is not supported"),
        },
        FrontCounterClockwise: match rasterizer.front_face {
            Clockwise => FALSE,
            CounterClockwise => TRUE,
        },
        DepthBias: bias.const_factor as INT,
        DepthBiasClamp: bias.clamp,
        SlopeScaledDepthBias: bias.slope_factor,
        DepthClipEnable: !rasterizer.depth_clamping as _,
        MultisampleEnable: if multisample { TRUE } else { FALSE },
        ForcedSampleCount: 0,         // TODO: currently not supported
        AntialiasedLineEnable: FALSE, // TODO: currently not supported
        ConservativeRaster: if rasterizer.conservative {
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON
        } else {
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
        },
    }
}

fn map_factor(factor: pso::Factor, is_alpha: bool) -> D3D12_BLEND {
    use hal::pso::Factor::*;
    match factor {
        Zero => D3D12_BLEND_ZERO,
        One => D3D12_BLEND_ONE,
        SrcColor if is_alpha => D3D12_BLEND_SRC_ALPHA,
        SrcColor => D3D12_BLEND_SRC_COLOR,
        OneMinusSrcColor if is_alpha => D3D12_BLEND_INV_SRC_ALPHA,
        OneMinusSrcColor => D3D12_BLEND_INV_SRC_COLOR,
        DstColor if is_alpha => D3D12_BLEND_DEST_ALPHA,
        DstColor => D3D12_BLEND_DEST_COLOR,
        OneMinusDstColor if is_alpha => D3D12_BLEND_INV_DEST_ALPHA,
        OneMinusDstColor => D3D12_BLEND_INV_DEST_COLOR,
        SrcAlpha => D3D12_BLEND_SRC_ALPHA,
        OneMinusSrcAlpha => D3D12_BLEND_INV_SRC_ALPHA,
        DstAlpha => D3D12_BLEND_DEST_ALPHA,
        OneMinusDstAlpha => D3D12_BLEND_INV_DEST_ALPHA,
        ConstColor | ConstAlpha => D3D12_BLEND_BLEND_FACTOR,
        OneMinusConstColor | OneMinusConstAlpha => D3D12_BLEND_INV_BLEND_FACTOR,
        SrcAlphaSaturate => D3D12_BLEND_SRC_ALPHA_SAT,
        Src1Color if is_alpha => D3D12_BLEND_SRC1_ALPHA,
        Src1Color => D3D12_BLEND_SRC1_COLOR,
        OneMinusSrc1Color if is_alpha => D3D12_BLEND_INV_SRC1_ALPHA,
        OneMinusSrc1Color => D3D12_BLEND_INV_SRC1_COLOR,
        Src1Alpha => D3D12_BLEND_SRC1_ALPHA,
        OneMinusSrc1Alpha => D3D12_BLEND_INV_SRC1_ALPHA,
    }
}

fn map_blend_op(
    operation: pso::BlendOp,
    is_alpha: bool,
) -> (D3D12_BLEND_OP, D3D12_BLEND, D3D12_BLEND) {
    use hal::pso::BlendOp::*;
    match operation {
        Add { src, dst } => (
            D3D12_BLEND_OP_ADD,
            map_factor(src, is_alpha),
            map_factor(dst, is_alpha),
        ),
        Sub { src, dst } => (
            D3D12_BLEND_OP_SUBTRACT,
            map_factor(src, is_alpha),
            map_factor(dst, is_alpha),
        ),
        RevSub { src, dst } => (
            D3D12_BLEND_OP_REV_SUBTRACT,
            map_factor(src, is_alpha),
            map_factor(dst, is_alpha),
        ),
        Min => (D3D12_BLEND_OP_MIN, D3D12_BLEND_ZERO, D3D12_BLEND_ZERO),
        Max => (D3D12_BLEND_OP_MAX, D3D12_BLEND_ZERO, D3D12_BLEND_ZERO),
    }
}

pub fn map_render_targets(
    color_targets: &[pso::ColorBlendDesc],
) -> [D3D12_RENDER_TARGET_BLEND_DESC; D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT as usize] {
    let dummy_target = D3D12_RENDER_TARGET_BLEND_DESC {
        BlendEnable: FALSE,
        LogicOpEnable: FALSE,
        SrcBlend: D3D12_BLEND_ZERO,
        DestBlend: D3D12_BLEND_ZERO,
        BlendOp: D3D12_BLEND_OP_ADD,
        SrcBlendAlpha: D3D12_BLEND_ZERO,
        DestBlendAlpha: D3D12_BLEND_ZERO,
        BlendOpAlpha: D3D12_BLEND_OP_ADD,
        LogicOp: D3D12_LOGIC_OP_CLEAR,
        RenderTargetWriteMask: 0,
    };
    let mut targets = [dummy_target; D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT as usize];

    for (target, color_desc) in targets.iter_mut().zip(color_targets.iter()) {
        target.RenderTargetWriteMask = color_desc.mask.bits() as UINT8;
        if let Some(ref blend) = color_desc.blend {
            let (color_op, color_src, color_dst) = map_blend_op(blend.color, false);
            let (alpha_op, alpha_src, alpha_dst) = map_blend_op(blend.alpha, true);
            target.BlendEnable = TRUE;
            target.BlendOp = color_op;
            target.SrcBlend = color_src;
            target.DestBlend = color_dst;
            target.BlendOpAlpha = alpha_op;
            target.SrcBlendAlpha = alpha_src;
            target.DestBlendAlpha = alpha_dst;
        }
    }

    targets
}

pub fn map_depth_stencil(dsi: &pso::DepthStencilDesc) -> D3D12_DEPTH_STENCIL_DESC {
    let (depth_on, depth_write, depth_func) = match dsi.depth {
        Some(ref depth) => (TRUE, depth.write, map_comparison(depth.fun)),
        None => unsafe { mem::zeroed() },
    };

    let (stencil_on, front, back, read_mask, write_mask) = match dsi.stencil {
        Some(ref stencil) => {
            let read_masks = stencil.read_masks.static_or(pso::Sided::new(!0));
            let write_masks = stencil.write_masks.static_or(pso::Sided::new(!0));
            if read_masks.front != read_masks.back || write_masks.front != write_masks.back {
                error!(
                    "Different sides are specified for read ({:?} and write ({:?}) stencil masks",
                    read_masks, write_masks
                );
            }
            (
                TRUE,
                map_stencil_side(&stencil.faces.front),
                map_stencil_side(&stencil.faces.back),
                read_masks.front,
                write_masks.front,
            )
        }
        None => unsafe { mem::zeroed() },
    };

    D3D12_DEPTH_STENCIL_DESC {
        DepthEnable: depth_on,
        DepthWriteMask: if depth_write {
            D3D12_DEPTH_WRITE_MASK_ALL
        } else {
            D3D12_DEPTH_WRITE_MASK_ZERO
        },
        DepthFunc: depth_func,
        StencilEnable: stencil_on,
        StencilReadMask: read_mask as _,
        StencilWriteMask: write_mask as _,
        FrontFace: front,
        BackFace: back,
    }
}

pub fn map_comparison(func: pso::Comparison) -> D3D12_COMPARISON_FUNC {
    use hal::pso::Comparison::*;
    match func {
        Never => D3D12_COMPARISON_FUNC_NEVER,
        Less => D3D12_COMPARISON_FUNC_LESS,
        LessEqual => D3D12_COMPARISON_FUNC_LESS_EQUAL,
        Equal => D3D12_COMPARISON_FUNC_EQUAL,
        GreaterEqual => D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        Greater => D3D12_COMPARISON_FUNC_GREATER,
        NotEqual => D3D12_COMPARISON_FUNC_NOT_EQUAL,
        Always => D3D12_COMPARISON_FUNC_ALWAYS,
    }
}

fn map_stencil_op(op: pso::StencilOp) -> D3D12_STENCIL_OP {
    use hal::pso::StencilOp::*;
    match op {
        Keep => D3D12_STENCIL_OP_KEEP,
        Zero => D3D12_STENCIL_OP_ZERO,
        Replace => D3D12_STENCIL_OP_REPLACE,
        IncrementClamp => D3D12_STENCIL_OP_INCR_SAT,
        IncrementWrap => D3D12_STENCIL_OP_INCR,
        DecrementClamp => D3D12_STENCIL_OP_DECR_SAT,
        DecrementWrap => D3D12_STENCIL_OP_DECR,
        Invert => D3D12_STENCIL_OP_INVERT,
    }
}

fn map_stencil_side(side: &pso::StencilFace) -> D3D12_DEPTH_STENCILOP_DESC {
    D3D12_DEPTH_STENCILOP_DESC {
        StencilFailOp: map_stencil_op(side.op_fail),
        StencilDepthFailOp: map_stencil_op(side.op_depth_fail),
        StencilPassOp: map_stencil_op(side.op_pass),
        StencilFunc: map_comparison(side.fun),
    }
}

pub fn map_wrap(wrap: image::WrapMode) -> D3D12_TEXTURE_ADDRESS_MODE {
    use hal::image::WrapMode::*;
    match wrap {
        Tile => D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        Mirror => D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        Clamp => D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        Border => D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        MirrorClamp => D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE,
    }
}

fn map_filter_type(filter: image::Filter) -> D3D12_FILTER_TYPE {
    match filter {
        image::Filter::Nearest => D3D12_FILTER_TYPE_POINT,
        image::Filter::Linear => D3D12_FILTER_TYPE_LINEAR,
    }
}

pub fn map_filter(
    mag_filter: image::Filter,
    min_filter: image::Filter,
    mip_filter: image::Filter,
    reduction: D3D12_FILTER_REDUCTION_TYPE,
    anisotropy_clamp: Option<u8>,
) -> D3D12_FILTER {
    let mag = map_filter_type(mag_filter);
    let min = map_filter_type(min_filter);
    let mip = map_filter_type(mip_filter);

    (min & D3D12_FILTER_TYPE_MASK) << D3D12_MIN_FILTER_SHIFT
        | (mag & D3D12_FILTER_TYPE_MASK) << D3D12_MAG_FILTER_SHIFT
        | (mip & D3D12_FILTER_TYPE_MASK) << D3D12_MIP_FILTER_SHIFT
        | (reduction & D3D12_FILTER_REDUCTION_TYPE_MASK) << D3D12_FILTER_REDUCTION_TYPE_SHIFT
        | anisotropy_clamp
            .map(|_| D3D12_FILTER_ANISOTROPIC)
            .unwrap_or(0)
}

pub fn map_buffer_resource_state(access: buffer::Access) -> D3D12_RESOURCE_STATES {
    use self::buffer::Access;
    // Mutable states
    if access.contains(Access::SHADER_WRITE) {
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if access.contains(Access::TRANSFER_WRITE) {
        // Resolve not relevant for buffers.
        return D3D12_RESOURCE_STATE_COPY_DEST;
    }

    // Read-only states
    let mut state = D3D12_RESOURCE_STATE_COMMON;

    if access.contains(Access::TRANSFER_READ) {
        state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if access.contains(Access::INDEX_BUFFER_READ) {
        state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    }
    if access.contains(Access::VERTEX_BUFFER_READ) || access.contains(Access::UNIFORM_READ) {
        state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    if access.contains(Access::INDIRECT_COMMAND_READ) {
        state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    if access.contains(Access::SHADER_READ) {
        // SHADER_READ only allows SRV access
        state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    state
}

/// Derive the combination of read-only states from the access flags.
fn derive_immutable_image_states(access: image::Access) -> D3D12_RESOURCE_STATES {
    let mut state = D3D12_RESOURCE_STATE_COMMON;

    if access.contains(image::Access::TRANSFER_READ) {
        state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if access.contains(image::Access::INPUT_ATTACHMENT_READ) {
        state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    if access.contains(image::Access::DEPTH_STENCIL_ATTACHMENT_READ) {
        state |= D3D12_RESOURCE_STATE_DEPTH_READ;
    }
    if access.contains(image::Access::SHADER_READ) {
        state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    state
}

/// Derive the mutable or read-only states from the access flags.
fn derive_mutable_image_states(access: image::Access) -> D3D12_RESOURCE_STATES {
    if access.contains(image::Access::SHADER_WRITE) {
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if access.contains(image::Access::COLOR_ATTACHMENT_WRITE) {
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if access.contains(image::Access::DEPTH_STENCIL_ATTACHMENT_WRITE) {
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if access.contains(image::Access::TRANSFER_WRITE) {
        return D3D12_RESOURCE_STATE_COPY_DEST;
    }

    derive_immutable_image_states(access)
}

pub fn map_image_resource_state(
    access: image::Access,
    layout: image::Layout,
) -> D3D12_RESOURCE_STATES {
    match layout {
        // the same as COMMON (general state)
        image::Layout::Present => D3D12_RESOURCE_STATE_PRESENT,
        image::Layout::ColorAttachmentOptimal => D3D12_RESOURCE_STATE_RENDER_TARGET,
        image::Layout::DepthStencilAttachmentOptimal => D3D12_RESOURCE_STATE_DEPTH_WRITE,
        // `TRANSFER_WRITE` requires special handling as it requires RESOLVE_DEST | COPY_DEST
        // but only 1 write-only allowed. We do the required translation before the commands.
        // We currently assume that `COPY_DEST` is more common state than out of renderpass resolves.
        // Resolve operations need to insert a barrier before and after the command to transition
        // from and into `COPY_DEST` to have a consistent state for srcAccess.
        image::Layout::TransferDstOptimal => D3D12_RESOURCE_STATE_COPY_DEST,
        image::Layout::TransferSrcOptimal => D3D12_RESOURCE_STATE_COPY_SOURCE,
        image::Layout::General => derive_mutable_image_states(access),
        image::Layout::ShaderReadOnlyOptimal | image::Layout::DepthStencilReadOnlyOptimal => {
            derive_immutable_image_states(access)
        }
        image::Layout::Undefined | image::Layout::Preinitialized => D3D12_RESOURCE_STATE_COMMON,
    }
}

pub fn map_shader_visibility(flags: pso::ShaderStageFlags) -> ShaderVisibility {
    use hal::pso::ShaderStageFlags as Ssf;

    match flags {
        Ssf::VERTEX => ShaderVisibility::VS,
        Ssf::GEOMETRY => ShaderVisibility::GS,
        Ssf::HULL => ShaderVisibility::HS,
        Ssf::DOMAIN => ShaderVisibility::DS,
        Ssf::FRAGMENT => ShaderVisibility::PS,
        _ => ShaderVisibility::All,
    }
}

pub fn map_buffer_flags(usage: buffer::Usage) -> D3D12_RESOURCE_FLAGS {
    let mut flags = D3D12_RESOURCE_FLAG_NONE;

    // TRANSFER_DST also used for clearing buffers, which is implemented via UAV clears.
    if usage.contains(buffer::Usage::STORAGE) || usage.contains(buffer::Usage::TRANSFER_DST) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    flags
}

pub fn map_image_flags(usage: image::Usage, features: ImageFeature) -> D3D12_RESOURCE_FLAGS {
    use self::image::Usage;
    let mut flags = D3D12_RESOURCE_FLAG_NONE;

    // Blit operations implemented via a graphics pipeline
    if usage.contains(Usage::COLOR_ATTACHMENT) {
        debug_assert!(features.contains(ImageFeature::COLOR_ATTACHMENT));
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if usage.contains(Usage::DEPTH_STENCIL_ATTACHMENT) {
        debug_assert!(features.contains(ImageFeature::DEPTH_STENCIL_ATTACHMENT));
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    if usage.contains(Usage::TRANSFER_DST) {
        if features.contains(ImageFeature::COLOR_ATTACHMENT) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        };
        if features.contains(ImageFeature::DEPTH_STENCIL_ATTACHMENT) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        };
    }
    if usage.contains(Usage::STORAGE) {
        debug_assert!(features.contains(ImageFeature::STORAGE));
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if !features.contains(ImageFeature::SAMPLED) {
        flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    flags
}

pub fn map_stage(stage: ShaderStage) -> spirv::ExecutionModel {
    match stage {
        ShaderStage::Vertex => spirv::ExecutionModel::Vertex,
        ShaderStage::Fragment => spirv::ExecutionModel::Fragment,
        ShaderStage::Geometry => spirv::ExecutionModel::Geometry,
        ShaderStage::Compute => spirv::ExecutionModel::GlCompute,
        ShaderStage::Hull => spirv::ExecutionModel::TessellationControl,
        ShaderStage::Domain => spirv::ExecutionModel::TessellationEvaluation,
        ShaderStage::Task | ShaderStage::Mesh => {
            panic!("{:?} shader is not yet implemented in SPIRV-Cross", stage)
        }
    }
}
