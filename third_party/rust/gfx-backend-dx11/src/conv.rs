use hal::format::Format;
use hal::image::{Anisotropic, Filter, WrapMode};
use hal::pso::{
    BlendDesc,
    BlendOp,
    ColorBlendDesc,
    Comparison,
    DepthBias,
    DepthStencilDesc,
    Face,
    Factor,
    FrontFace,
    InputAssemblerDesc,
    PolygonMode,
    Rasterizer,
    Rect,
    Sided,
    Stage,
    State,
    StencilFace,
    StencilOp,
    StencilValue,
    Viewport,
};
use hal::IndexType;

use spirv_cross::spirv;

use winapi::shared::dxgiformat::*;
use winapi::shared::minwindef::{FALSE, INT, TRUE};

use winapi::um::d3d11::*;
use winapi::um::d3dcommon::*;

use std::mem;

pub fn map_index_type(ty: IndexType) -> DXGI_FORMAT {
    match ty {
        IndexType::U16 => DXGI_FORMAT_R16_UINT,
        IndexType::U32 => DXGI_FORMAT_R32_UINT,
    }
}

// TODO: add aspect parameter
pub fn viewable_format(format: DXGI_FORMAT) -> DXGI_FORMAT {
    match format {
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT => DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        DXGI_FORMAT_D32_FLOAT => DXGI_FORMAT_R32_FLOAT,
        DXGI_FORMAT_D16_UNORM => DXGI_FORMAT_R16_UNORM,
        _ => format,
    }
}

// TODO: stolen from d3d12 backend, maybe share function somehow?
pub fn map_format(format: Format) -> Option<DXGI_FORMAT> {
    use hal::format::Format::*;

    let format = match format {
        R5g6b5Unorm => DXGI_FORMAT_B5G6R5_UNORM,
        R5g5b5a1Unorm => DXGI_FORMAT_B5G5R5A1_UNORM,
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
        A2b10g10r10Unorm => DXGI_FORMAT_R10G10B10A2_UNORM,
        A2b10g10r10Uint => DXGI_FORMAT_R10G10B10A2_UINT,
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
        B10g11r11Ufloat => DXGI_FORMAT_R11G11B10_FLOAT,
        E5b9g9r9Ufloat => DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
        D16Unorm => DXGI_FORMAT_D16_UNORM,
        D32Sfloat => DXGI_FORMAT_D32_FLOAT,
        D32SfloatS8Uint => DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
        Bc1RgbUnorm => DXGI_FORMAT_BC1_UNORM,
        Bc1RgbSrgb => DXGI_FORMAT_BC1_UNORM_SRGB,
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

pub fn map_format_nosrgb(format: Format) -> Option<DXGI_FORMAT> {
    // NOTE: DXGI doesn't allow sRGB format on the swapchain, but
    //       creating RTV of swapchain buffers with sRGB works
    match format {
        Format::Bgra8Srgb => Some(DXGI_FORMAT_B8G8R8A8_UNORM),
        Format::Rgba8Srgb => Some(DXGI_FORMAT_R8G8B8A8_UNORM),
        _ => map_format(format),
    }
}

#[derive(Debug, Clone)]
pub struct DecomposedDxgiFormat {
    pub typeless: DXGI_FORMAT,
    pub srv: Option<DXGI_FORMAT>,
    pub rtv: Option<DXGI_FORMAT>,
    pub uav: Option<DXGI_FORMAT>,
    pub dsv: Option<DXGI_FORMAT>,
    // the format we use internally for operating on textures (eg. Rgba8 uses R32 internally for
    // copies)
    pub copy_uav: Option<DXGI_FORMAT>,
    pub copy_srv: Option<DXGI_FORMAT>,
}

impl DecomposedDxgiFormat {
    pub const UNKNOWN: DecomposedDxgiFormat = DecomposedDxgiFormat {
        typeless: DXGI_FORMAT_UNKNOWN,
        srv: None,
        rtv: None,
        uav: None,
        dsv: None,
        copy_uav: None,
        copy_srv: None,
    };

    // TODO: we probably want to pass in usage flags or similar to allow for our `typeless_format`
    //       field to only contain the input format (eg. depth only rather than typeless likely
    //       improves perf since the driver doesn't need to expose internals)
    //
    // TODO: we also want aspect for determining depth/stencil
    pub fn from_dxgi_format(format: DXGI_FORMAT) -> DecomposedDxgiFormat {
        match format {
            DXGI_FORMAT_R8G8B8A8_UNORM
            | DXGI_FORMAT_R8G8B8A8_SNORM
            | DXGI_FORMAT_R8G8B8A8_UINT
            | DXGI_FORMAT_R8G8B8A8_SINT
            | DXGI_FORMAT_R8G8B8A8_UNORM_SRGB => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R8G8B8A8_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R32_UINT),
                copy_srv: Some(DXGI_FORMAT_R8G8B8A8_UINT),
            },

            DXGI_FORMAT_B8G8R8A8_UNORM | DXGI_FORMAT_B8G8R8A8_UNORM_SRGB => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_B8G8R8A8_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(DXGI_FORMAT_B8G8R8A8_UNORM),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R32_UINT),
                copy_srv: Some(DXGI_FORMAT_B8G8R8A8_UNORM),
            },

            DXGI_FORMAT_A8_UNORM => DecomposedDxgiFormat {
                typeless: format,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(format),
                copy_srv: Some(format),
            },

            DXGI_FORMAT_R8_UNORM | DXGI_FORMAT_R8_SNORM | DXGI_FORMAT_R8_UINT
            | DXGI_FORMAT_R8_SINT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R8_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R8_UINT),
                copy_srv: Some(DXGI_FORMAT_R8_UINT),
            },

            DXGI_FORMAT_R8G8_UNORM
            | DXGI_FORMAT_R8G8_SNORM
            | DXGI_FORMAT_R8G8_UINT
            | DXGI_FORMAT_R8G8_SINT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R8G8_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R8G8_UINT),
                copy_srv: Some(DXGI_FORMAT_R8G8_UINT),
            },

            DXGI_FORMAT_D16_UNORM => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R16_TYPELESS,
                srv: Some(DXGI_FORMAT_R16_FLOAT),
                rtv: Some(DXGI_FORMAT_R16_FLOAT),
                uav: Some(DXGI_FORMAT_R16_FLOAT),
                dsv: Some(format),
                copy_uav: Some(DXGI_FORMAT_R16_UINT),
                copy_srv: Some(DXGI_FORMAT_R16_UINT),
            },

            DXGI_FORMAT_R16_UNORM
            | DXGI_FORMAT_R16_SNORM
            | DXGI_FORMAT_R16_UINT
            | DXGI_FORMAT_R16_SINT
            | DXGI_FORMAT_R16_FLOAT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R16_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: Some(DXGI_FORMAT_D16_UNORM),
                copy_uav: Some(DXGI_FORMAT_R16_UINT),
                copy_srv: Some(DXGI_FORMAT_R16_UINT),
            },

            DXGI_FORMAT_R16G16_UNORM
            | DXGI_FORMAT_R16G16_SNORM
            | DXGI_FORMAT_R16G16_UINT
            | DXGI_FORMAT_R16G16_SINT
            | DXGI_FORMAT_R16G16_FLOAT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R16G16_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R32_UINT),
                copy_srv: Some(DXGI_FORMAT_R16G16_UINT),
            },

            DXGI_FORMAT_R16G16B16A16_UNORM
            | DXGI_FORMAT_R16G16B16A16_SNORM
            | DXGI_FORMAT_R16G16B16A16_UINT
            | DXGI_FORMAT_R16G16B16A16_SINT
            | DXGI_FORMAT_R16G16B16A16_FLOAT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R16G16B16A16_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R16G16B16A16_UINT),
                copy_srv: Some(DXGI_FORMAT_R16G16B16A16_UINT),
            },

            DXGI_FORMAT_D32_FLOAT_S8X24_UINT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R32G8X24_TYPELESS,
                // TODO: depth or stencil?
                srv: Some(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS),
                rtv: None,
                uav: None,
                dsv: Some(format),
                copy_uav: None,
                copy_srv: Some(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS),
            },

            DXGI_FORMAT_D32_FLOAT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R32_TYPELESS,
                srv: Some(DXGI_FORMAT_R32_FLOAT),
                rtv: None,
                uav: None,
                dsv: Some(format),
                copy_uav: Some(DXGI_FORMAT_R32_UINT),
                copy_srv: Some(DXGI_FORMAT_R32_UINT),
            },

            DXGI_FORMAT_R32_UINT | DXGI_FORMAT_R32_SINT | DXGI_FORMAT_R32_FLOAT => {
                DecomposedDxgiFormat {
                    typeless: DXGI_FORMAT_R32_TYPELESS,
                    srv: Some(format),
                    rtv: Some(format),
                    uav: Some(format),
                    dsv: Some(DXGI_FORMAT_D32_FLOAT),
                    copy_uav: Some(DXGI_FORMAT_R32_UINT),
                    copy_srv: Some(DXGI_FORMAT_R32_UINT),
                }
            }

            DXGI_FORMAT_R32G32_UINT | DXGI_FORMAT_R32G32_SINT | DXGI_FORMAT_R32G32_FLOAT => {
                DecomposedDxgiFormat {
                    typeless: DXGI_FORMAT_R32G32_TYPELESS,
                    srv: Some(format),
                    rtv: Some(format),
                    uav: Some(format),
                    dsv: None,
                    copy_uav: Some(DXGI_FORMAT_R32G32_UINT),
                    copy_srv: Some(DXGI_FORMAT_R32G32_UINT),
                }
            }

            // TODO: should we just convert to Rgba32 internally?
            DXGI_FORMAT_R32G32B32_UINT
            | DXGI_FORMAT_R32G32B32_SINT
            | DXGI_FORMAT_R32G32B32_FLOAT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R32G32_TYPELESS,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R32G32B32_UINT),
                copy_srv: Some(DXGI_FORMAT_R32G32B32_UINT),
            },

            DXGI_FORMAT_R32G32B32A32_UINT
            | DXGI_FORMAT_R32G32B32A32_SINT
            | DXGI_FORMAT_R32G32B32A32_FLOAT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R32G32B32A32_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R32G32B32A32_UINT),
                copy_srv: Some(DXGI_FORMAT_R32G32B32A32_UINT),
            },

            DXGI_FORMAT_R10G10B10A2_UNORM | DXGI_FORMAT_R10G10B10A2_UINT => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_R10G10B10A2_TYPELESS,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(DXGI_FORMAT_R32_UINT),
                copy_srv: Some(DXGI_FORMAT_R10G10B10A2_UINT),
            },

            DXGI_FORMAT_R11G11B10_FLOAT => DecomposedDxgiFormat {
                typeless: format,
                srv: Some(format),
                rtv: Some(format),
                uav: Some(format),
                dsv: None,
                copy_uav: Some(format),
                copy_srv: Some(format),
            },

            DXGI_FORMAT_R9G9B9E5_SHAREDEXP => DecomposedDxgiFormat {
                typeless: format,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            DXGI_FORMAT_BC1_UNORM | DXGI_FORMAT_BC1_UNORM_SRGB => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_BC1_TYPELESS,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            DXGI_FORMAT_BC2_UNORM | DXGI_FORMAT_BC2_UNORM_SRGB => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_BC2_TYPELESS,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            DXGI_FORMAT_BC3_UNORM | DXGI_FORMAT_BC3_UNORM_SRGB => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_BC3_TYPELESS,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            DXGI_FORMAT_BC4_UNORM | DXGI_FORMAT_BC4_SNORM => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_BC4_TYPELESS,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            DXGI_FORMAT_BC5_UNORM | DXGI_FORMAT_BC5_SNORM => DecomposedDxgiFormat {
                typeless: format,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            DXGI_FORMAT_BC6H_UF16 | DXGI_FORMAT_BC6H_SF16 => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_BC6H_TYPELESS,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            // TODO: srgb craziness
            DXGI_FORMAT_BC7_UNORM | DXGI_FORMAT_BC7_UNORM_SRGB => DecomposedDxgiFormat {
                typeless: DXGI_FORMAT_BC7_TYPELESS,
                srv: Some(format),
                rtv: None,
                uav: None,
                dsv: None,
                // NOTE: read only
                copy_uav: None,
                copy_srv: Some(format),
            },

            _ => unimplemented!(),
        }
    }
}

pub fn map_viewport(viewport: &Viewport) -> D3D11_VIEWPORT {
    D3D11_VIEWPORT {
        TopLeftX: viewport.rect.x as _,
        TopLeftY: viewport.rect.y as _,
        Width: viewport.rect.w as _,
        Height: viewport.rect.h as _,
        MinDepth: viewport.depth.start,
        MaxDepth: viewport.depth.end,
    }
}

pub fn map_rect(rect: &Rect) -> D3D11_RECT {
    D3D11_RECT {
        left: rect.x as _,
        top: rect.y as _,
        right: (rect.x + rect.w) as _,
        bottom: (rect.y + rect.h) as _,
    }
}

pub fn map_topology(ia: &InputAssemblerDesc) -> D3D11_PRIMITIVE_TOPOLOGY {
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

fn map_fill_mode(mode: PolygonMode) -> D3D11_FILL_MODE {
    match mode {
        PolygonMode::Fill => D3D11_FILL_SOLID,
        PolygonMode::Line(_) => D3D11_FILL_WIREFRAME,
        // TODO: return error
        _ => unimplemented!(),
    }
}

fn map_cull_mode(mode: Face) -> D3D11_CULL_MODE {
    match mode {
        Face::NONE => D3D11_CULL_NONE,
        Face::FRONT => D3D11_CULL_FRONT,
        Face::BACK => D3D11_CULL_BACK,
        _ => panic!("Culling both front and back faces is not supported"),
    }
}

pub(crate) fn map_rasterizer_desc(desc: &Rasterizer) -> D3D11_RASTERIZER_DESC {
    let bias = match desc.depth_bias {
        //TODO: support dynamic depth bias
        Some(State::Static(db)) => db,
        Some(_) | None => DepthBias::default(),
    };
    D3D11_RASTERIZER_DESC {
        FillMode: map_fill_mode(desc.polygon_mode),
        CullMode: map_cull_mode(desc.cull_face),
        FrontCounterClockwise: match desc.front_face {
            FrontFace::Clockwise => FALSE,
            FrontFace::CounterClockwise => TRUE,
        },
        DepthBias: bias.const_factor as INT,
        DepthBiasClamp: bias.clamp,
        SlopeScaledDepthBias: bias.slope_factor,
        DepthClipEnable: !desc.depth_clamping as _,
        // TODO:
        ScissorEnable: TRUE,
        // TODO: msaa
        MultisampleEnable: FALSE,
        // TODO: line aa?
        AntialiasedLineEnable: FALSE,
        // TODO: conservative raster in >=11.x
    }
}

fn map_blend_factor(factor: Factor) -> D3D11_BLEND {
    match factor {
        Factor::Zero => D3D11_BLEND_ZERO,
        Factor::One => D3D11_BLEND_ONE,
        Factor::SrcColor => D3D11_BLEND_SRC_COLOR,
        Factor::OneMinusSrcColor => D3D11_BLEND_INV_SRC_COLOR,
        Factor::DstColor => D3D11_BLEND_DEST_COLOR,
        Factor::OneMinusDstColor => D3D11_BLEND_INV_DEST_COLOR,
        Factor::SrcAlpha => D3D11_BLEND_SRC_ALPHA,
        Factor::OneMinusSrcAlpha => D3D11_BLEND_INV_SRC_ALPHA,
        Factor::DstAlpha => D3D11_BLEND_DEST_ALPHA,
        Factor::OneMinusDstAlpha => D3D11_BLEND_INV_DEST_ALPHA,
        Factor::ConstColor | Factor::ConstAlpha => D3D11_BLEND_BLEND_FACTOR,
        Factor::OneMinusConstColor | Factor::OneMinusConstAlpha => D3D11_BLEND_INV_BLEND_FACTOR,
        Factor::SrcAlphaSaturate => D3D11_BLEND_SRC_ALPHA_SAT,
        Factor::Src1Color => D3D11_BLEND_SRC1_COLOR,
        Factor::OneMinusSrc1Color => D3D11_BLEND_INV_SRC1_COLOR,
        Factor::Src1Alpha => D3D11_BLEND_SRC1_ALPHA,
        Factor::OneMinusSrc1Alpha => D3D11_BLEND_INV_SRC1_ALPHA,
    }
}

fn map_alpha_blend_factor(factor: Factor) -> D3D11_BLEND {
    match factor {
        Factor::Zero => D3D11_BLEND_ZERO,
        Factor::One => D3D11_BLEND_ONE,
        Factor::SrcColor | Factor::SrcAlpha => D3D11_BLEND_SRC_ALPHA,
        Factor::DstColor | Factor::DstAlpha => D3D11_BLEND_DEST_ALPHA,
        Factor::OneMinusSrcColor | Factor::OneMinusSrcAlpha => D3D11_BLEND_INV_SRC_ALPHA,
        Factor::OneMinusDstColor | Factor::OneMinusDstAlpha => D3D11_BLEND_INV_DEST_ALPHA,
        Factor::ConstColor | Factor::ConstAlpha => D3D11_BLEND_BLEND_FACTOR,
        Factor::OneMinusConstColor | Factor::OneMinusConstAlpha => D3D11_BLEND_INV_BLEND_FACTOR,
        Factor::SrcAlphaSaturate => D3D11_BLEND_SRC_ALPHA_SAT,
        Factor::Src1Color | Factor::Src1Alpha => D3D11_BLEND_SRC1_ALPHA,
        Factor::OneMinusSrc1Color | Factor::OneMinusSrc1Alpha => D3D11_BLEND_INV_SRC1_ALPHA,
    }
}

fn map_blend_op(operation: BlendOp) -> (D3D11_BLEND_OP, D3D11_BLEND, D3D11_BLEND) {
    match operation {
        BlendOp::Add { src, dst } => (
            D3D11_BLEND_OP_ADD,
            map_blend_factor(src),
            map_blend_factor(dst),
        ),
        BlendOp::Sub { src, dst } => (
            D3D11_BLEND_OP_SUBTRACT,
            map_blend_factor(src),
            map_blend_factor(dst),
        ),
        BlendOp::RevSub { src, dst } => (
            D3D11_BLEND_OP_REV_SUBTRACT,
            map_blend_factor(src),
            map_blend_factor(dst),
        ),
        BlendOp::Min => (D3D11_BLEND_OP_MIN, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO),
        BlendOp::Max => (D3D11_BLEND_OP_MAX, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO),
    }
}

fn map_alpha_blend_op(operation: BlendOp) -> (D3D11_BLEND_OP, D3D11_BLEND, D3D11_BLEND) {
    match operation {
        BlendOp::Add { src, dst } => (
            D3D11_BLEND_OP_ADD,
            map_alpha_blend_factor(src),
            map_alpha_blend_factor(dst),
        ),
        BlendOp::Sub { src, dst } => (
            D3D11_BLEND_OP_SUBTRACT,
            map_alpha_blend_factor(src),
            map_alpha_blend_factor(dst),
        ),
        BlendOp::RevSub { src, dst } => (
            D3D11_BLEND_OP_REV_SUBTRACT,
            map_alpha_blend_factor(src),
            map_alpha_blend_factor(dst),
        ),
        BlendOp::Min => (D3D11_BLEND_OP_MIN, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO),
        BlendOp::Max => (D3D11_BLEND_OP_MAX, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO),
    }
}

fn map_blend_targets(
    render_target_blends: &[ColorBlendDesc],
) -> [D3D11_RENDER_TARGET_BLEND_DESC; 8] {
    let mut targets: [D3D11_RENDER_TARGET_BLEND_DESC; 8] = [unsafe { mem::zeroed() }; 8];

    for (mut target, color_desc) in targets.iter_mut().zip(render_target_blends.iter()) {
        target.RenderTargetWriteMask = color_desc.mask.bits() as _;
        if let Some(ref blend) = color_desc.blend {
            let (color_op, color_src, color_dst) = map_blend_op(blend.color);
            let (alpha_op, alpha_src, alpha_dst) = map_alpha_blend_op(blend.alpha);
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

pub(crate) fn map_blend_desc(desc: &BlendDesc) -> D3D11_BLEND_DESC {
    D3D11_BLEND_DESC {
        // TODO: msaa
        AlphaToCoverageEnable: FALSE,
        IndependentBlendEnable: TRUE,
        RenderTarget: map_blend_targets(&desc.targets),
    }
}

pub fn map_comparison(func: Comparison) -> D3D11_COMPARISON_FUNC {
    match func {
        Comparison::Never => D3D11_COMPARISON_NEVER,
        Comparison::Less => D3D11_COMPARISON_LESS,
        Comparison::LessEqual => D3D11_COMPARISON_LESS_EQUAL,
        Comparison::Equal => D3D11_COMPARISON_EQUAL,
        Comparison::GreaterEqual => D3D11_COMPARISON_GREATER_EQUAL,
        Comparison::Greater => D3D11_COMPARISON_GREATER,
        Comparison::NotEqual => D3D11_COMPARISON_NOT_EQUAL,
        Comparison::Always => D3D11_COMPARISON_ALWAYS,
    }
}

fn map_stencil_op(op: StencilOp) -> D3D11_STENCIL_OP {
    match op {
        StencilOp::Keep => D3D11_STENCIL_OP_KEEP,
        StencilOp::Zero => D3D11_STENCIL_OP_ZERO,
        StencilOp::Replace => D3D11_STENCIL_OP_REPLACE,
        StencilOp::IncrementClamp => D3D11_STENCIL_OP_INCR_SAT,
        StencilOp::IncrementWrap => D3D11_STENCIL_OP_INCR,
        StencilOp::DecrementClamp => D3D11_STENCIL_OP_DECR_SAT,
        StencilOp::DecrementWrap => D3D11_STENCIL_OP_DECR,
        StencilOp::Invert => D3D11_STENCIL_OP_INVERT,
    }
}

fn map_stencil_side(side: &StencilFace) -> D3D11_DEPTH_STENCILOP_DESC {
    D3D11_DEPTH_STENCILOP_DESC {
        StencilFailOp: map_stencil_op(side.op_fail),
        StencilDepthFailOp: map_stencil_op(side.op_depth_fail),
        StencilPassOp: map_stencil_op(side.op_pass),
        StencilFunc: map_comparison(side.fun),
    }
}

pub(crate) fn map_depth_stencil_desc(
    desc: &DepthStencilDesc,
) -> (D3D11_DEPTH_STENCIL_DESC, State<StencilValue>) {
    let (depth_on, depth_write, depth_func) = match desc.depth {
        Some(ref depth) => (TRUE, depth.write, map_comparison(depth.fun)),
        None => unsafe { mem::zeroed() },
    };

    let (stencil_on, front, back, read_mask, write_mask, stencil_ref) = match desc.stencil {
        Some(ref stencil) => {
            let read_masks = stencil.read_masks.static_or(Sided::new(!0));
            let write_masks = stencil.read_masks.static_or(Sided::new(!0));
            let reference_value = match stencil.reference_values {
                State::Static(ref values) => {
                    if values.front != values.back {
                        error!("Different reference values for front ({}) and back ({}) of the stencil",
                            values.front, values.back);
                    }
                    State::Static(values.front)
                }
                State::Dynamic => State::Dynamic,
            };
            // TODO: cascade to create_pipeline
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
                reference_value,
            )
        }
        None => unsafe { mem::zeroed() },
    };

    (
        D3D11_DEPTH_STENCIL_DESC {
            DepthEnable: depth_on,
            DepthWriteMask: if depth_write {
                D3D11_DEPTH_WRITE_MASK_ALL
            } else {
                D3D11_DEPTH_WRITE_MASK_ZERO
            },
            DepthFunc: depth_func,
            StencilEnable: stencil_on,
            StencilReadMask: read_mask as _,
            StencilWriteMask: write_mask as _,
            FrontFace: front,
            BackFace: back,
        },
        stencil_ref,
    )
}

pub fn map_execution_model(model: spirv::ExecutionModel) -> Stage {
    match model {
        spirv::ExecutionModel::Vertex => Stage::Vertex,
        spirv::ExecutionModel::Fragment => Stage::Fragment,
        spirv::ExecutionModel::Geometry => Stage::Geometry,
        spirv::ExecutionModel::GlCompute => Stage::Compute,
        spirv::ExecutionModel::TessellationControl => Stage::Hull,
        spirv::ExecutionModel::TessellationEvaluation => Stage::Domain,
        spirv::ExecutionModel::Kernel => panic!("Kernel is not a valid execution model."),
    }
}

pub fn map_stage(stage: Stage) -> spirv::ExecutionModel {
    match stage {
        Stage::Vertex => spirv::ExecutionModel::Vertex,
        Stage::Fragment => spirv::ExecutionModel::Fragment,
        Stage::Geometry => spirv::ExecutionModel::Geometry,
        Stage::Compute => spirv::ExecutionModel::GlCompute,
        Stage::Hull => spirv::ExecutionModel::TessellationControl,
        Stage::Domain => spirv::ExecutionModel::TessellationEvaluation,
    }
}

pub fn map_wrapping(wrap: WrapMode) -> D3D11_TEXTURE_ADDRESS_MODE {
    match wrap {
        WrapMode::Tile => D3D11_TEXTURE_ADDRESS_WRAP,
        WrapMode::Mirror => D3D11_TEXTURE_ADDRESS_MIRROR,
        WrapMode::Clamp => D3D11_TEXTURE_ADDRESS_CLAMP,
        WrapMode::Border => D3D11_TEXTURE_ADDRESS_BORDER,
    }
}

pub fn map_anisotropic(anisotropic: Anisotropic) -> D3D11_FILTER {
    match anisotropic {
        Anisotropic::On(_) => D3D11_FILTER_ANISOTROPIC,
        Anisotropic::Off => 0,
    }
}

fn map_filter_type(filter: Filter) -> D3D11_FILTER_TYPE {
    match filter {
        Filter::Nearest => D3D11_FILTER_TYPE_POINT,
        Filter::Linear => D3D11_FILTER_TYPE_LINEAR,
    }
}

// Hopefully works just as well in d3d11 :)
pub fn map_filter(
    mag_filter: Filter,
    min_filter: Filter,
    mip_filter: Filter,
    reduction: D3D11_FILTER_REDUCTION_TYPE,
    anisotropic: Anisotropic,
) -> D3D11_FILTER {
    let mag = map_filter_type(mag_filter);
    let min = map_filter_type(min_filter);
    let mip = map_filter_type(mip_filter);

    (min & D3D11_FILTER_TYPE_MASK) << D3D11_MIN_FILTER_SHIFT
        | (mag & D3D11_FILTER_TYPE_MASK) << D3D11_MAG_FILTER_SHIFT
        | (mip & D3D11_FILTER_TYPE_MASK) << D3D11_MIP_FILTER_SHIFT
        | (reduction & D3D11_FILTER_REDUCTION_TYPE_MASK) << D3D11_FILTER_REDUCTION_TYPE_SHIFT
        | map_anisotropic(anisotropic)
}
