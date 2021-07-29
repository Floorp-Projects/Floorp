use auxil::{FastHashMap, FastHashSet, ShaderStage};
use hal::{command, image, pso};

use winapi::{
    shared::minwindef::UINT,
    shared::{
        dxgiformat,
        minwindef::{FALSE, TRUE},
        winerror,
    },
    um::{d3d11, d3dcommon},
};

use wio::com::ComPtr;

use std::{mem, ptr};

use parking_lot::Mutex;
use smallvec::SmallVec;
use spirv_cross::{self, hlsl::ShaderModel};

use crate::{conv, shader, Buffer, Image, RenderPassCache};

#[repr(C)]
struct BufferCopy {
    src: u32,
    dst: u32,
    _padding: [u32; 2],
}

#[repr(C)]
struct ImageCopy {
    src: [u32; 4],
    dst: [u32; 4],
}

#[repr(C)]
struct BufferImageCopy {
    buffer_offset: u32,
    buffer_size: [u32; 2],
    _padding: u32,
    image_offset: [u32; 4],
    image_extent: [u32; 4],
    // actual size of the target image
    image_size: [u32; 4],
}

#[repr(C)]
struct BufferImageCopyInfo {
    buffer: BufferCopy,
    image: ImageCopy,
    buffer_image: BufferImageCopy,
}

#[repr(C)]
struct BlitInfo {
    offset: [f32; 2],
    extent: [f32; 2],
    z: f32,
    level: f32,
}

#[repr(C)]
struct PartialClearInfo {
    // transmute between the types, easier than juggling all different kinds of fields..
    data: [u32; 4],
}

// the threadgroup count we use in our copy shaders
const COPY_THREAD_GROUP_X: u32 = 8;
const COPY_THREAD_GROUP_Y: u32 = 8;

#[derive(Clone, Debug)]
struct ComputeCopyBuffer {
    d1_from_buffer: Option<ComPtr<d3d11::ID3D11ComputeShader>>,
    // Buffer -> Image2D
    d2_from_buffer: Option<ComPtr<d3d11::ID3D11ComputeShader>>,
    // Image2D -> Buffer
    d2_into_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    scale: (u32, u32),
}

#[derive(Debug)]
struct ConstantBuffer {
    buffer: ComPtr<d3d11::ID3D11Buffer>,
}

impl ConstantBuffer {
    unsafe fn update<T>(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>, data: T) {
        let mut mapped = mem::zeroed::<d3d11::D3D11_MAPPED_SUBRESOURCE>();
        let hr = context.Map(
            self.buffer.as_raw() as _,
            0,
            d3d11::D3D11_MAP_WRITE_DISCARD,
            0,
            &mut mapped,
        );
        assert_eq!(winerror::S_OK, hr);

        ptr::copy(&data, mapped.pData as _, 1);

        context.Unmap(self.buffer.as_raw() as _, 0);
    }
}

#[derive(Debug)]
struct MissingComputeInternal {
    // Image<->Image not covered by `CopySubresourceRegion`
    cs_copy_image_shaders: FastHashSet<(dxgiformat::DXGI_FORMAT, dxgiformat::DXGI_FORMAT)>,
    // Image -> Buffer and Buffer -> Image shaders
    cs_copy_buffer_shaders: FastHashSet<dxgiformat::DXGI_FORMAT>,
}

#[derive(Debug)]
struct ComputeInternal {
    // Image<->Image not covered by `CopySubresourceRegion`
    cs_copy_image_shaders: FastHashMap<
        (dxgiformat::DXGI_FORMAT, dxgiformat::DXGI_FORMAT),
        ComPtr<d3d11::ID3D11ComputeShader>,
    >,
    // Image -> Buffer and Buffer -> Image shaders
    cs_copy_buffer_shaders: FastHashMap<dxgiformat::DXGI_FORMAT, ComputeCopyBuffer>,
}

#[derive(Debug)]
enum PossibleComputeInternal {
    Available(ComputeInternal),
    Missing(MissingComputeInternal),
}

// Holds everything we need for fallback implementations of features that are not in DX.
//
// TODO: make struct fields more modular and group them up in structs depending on if it is a
//       fallback version or not (eg. Option<PartialClear>), should make struct definition and
//       `new` function smaller
#[derive(Debug)]
pub struct Internal {
    // partial clearing
    vs_partial_clear: ComPtr<d3d11::ID3D11VertexShader>,
    ps_partial_clear_float: ComPtr<d3d11::ID3D11PixelShader>,
    ps_partial_clear_uint: ComPtr<d3d11::ID3D11PixelShader>,
    ps_partial_clear_int: ComPtr<d3d11::ID3D11PixelShader>,
    ps_partial_clear_depth: ComPtr<d3d11::ID3D11PixelShader>,
    ps_partial_clear_stencil: ComPtr<d3d11::ID3D11PixelShader>,
    partial_clear_depth_stencil_state: ComPtr<d3d11::ID3D11DepthStencilState>,
    partial_clear_depth_state: ComPtr<d3d11::ID3D11DepthStencilState>,
    partial_clear_stencil_state: ComPtr<d3d11::ID3D11DepthStencilState>,

    // blitting
    vs_blit_2d: ComPtr<d3d11::ID3D11VertexShader>,

    sampler_nearest: ComPtr<d3d11::ID3D11SamplerState>,
    sampler_linear: ComPtr<d3d11::ID3D11SamplerState>,

    ps_blit_2d_uint: ComPtr<d3d11::ID3D11PixelShader>,
    ps_blit_2d_int: ComPtr<d3d11::ID3D11PixelShader>,
    ps_blit_2d_float: ComPtr<d3d11::ID3D11PixelShader>,

    // all compute shader based workarounds, so they can be None when running on an older version without compute shaders.
    compute_internal: PossibleComputeInternal,

    // internal constant buffer that is used by internal shaders
    internal_buffer: Mutex<ConstantBuffer>,

    // public buffer that is used as intermediate storage for some operations (memory invalidation)
    pub working_buffer: ComPtr<d3d11::ID3D11Buffer>,
    pub working_buffer_size: u64,

    pub constant_buffer_count_buffer:
        [UINT; d3d11::D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT as _],

    /// Command lists are not supported by graphics card and are being emulated.
    /// Requires various workarounds to make things work correctly.
    pub command_list_emulation: bool,

    pub device_features: hal::Features,
    pub device_feature_level: u32,

    pub downlevel: hal::DownlevelProperties,
}

fn compile_blob(
    src: &[u8],
    entrypoint: &str,
    stage: ShaderStage,
    shader_model: ShaderModel,
) -> ComPtr<d3dcommon::ID3DBlob> {
    unsafe {
        ComPtr::from_raw(shader::compile_hlsl_shader(stage, shader_model, entrypoint, src).unwrap())
    }
}

fn compile_vs(
    device: &ComPtr<d3d11::ID3D11Device>,
    src: &[u8],
    entrypoint: &str,
    shader_model: ShaderModel,
) -> ComPtr<d3d11::ID3D11VertexShader> {
    let bytecode = compile_blob(src, entrypoint, ShaderStage::Vertex, shader_model);
    let mut shader = ptr::null_mut();
    let hr = unsafe {
        device.CreateVertexShader(
            bytecode.GetBufferPointer(),
            bytecode.GetBufferSize(),
            ptr::null_mut(),
            &mut shader as *mut *mut _ as *mut *mut _,
        )
    };
    assert_eq!(true, winerror::SUCCEEDED(hr));

    unsafe { ComPtr::from_raw(shader) }
}

fn compile_ps(
    device: &ComPtr<d3d11::ID3D11Device>,
    src: &[u8],
    entrypoint: &str,
    shader_model: ShaderModel,
) -> ComPtr<d3d11::ID3D11PixelShader> {
    let bytecode = compile_blob(src, entrypoint, ShaderStage::Fragment, shader_model);
    let mut shader = ptr::null_mut();
    let hr = unsafe {
        device.CreatePixelShader(
            bytecode.GetBufferPointer(),
            bytecode.GetBufferSize(),
            ptr::null_mut(),
            &mut shader as *mut *mut _ as *mut *mut _,
        )
    };
    assert_eq!(true, winerror::SUCCEEDED(hr));

    unsafe { ComPtr::from_raw(shader) }
}

fn compile_cs(
    device: &ComPtr<d3d11::ID3D11Device>,
    src: &[u8],
    entrypoint: &str,
    shader_model: ShaderModel,
) -> ComPtr<d3d11::ID3D11ComputeShader> {
    let bytecode = compile_blob(src, entrypoint, ShaderStage::Compute, shader_model);
    let mut shader = ptr::null_mut();
    let hr = unsafe {
        device.CreateComputeShader(
            bytecode.GetBufferPointer(),
            bytecode.GetBufferSize(),
            ptr::null_mut(),
            &mut shader as *mut *mut _ as *mut *mut _,
        )
    };
    assert_eq!(true, winerror::SUCCEEDED(hr));

    unsafe { ComPtr::from_raw(shader) }
}

impl Internal {
    pub fn new(
        device: &ComPtr<d3d11::ID3D11Device>,
        device_features: hal::Features,
        device_feature_level: u32,
        downlevel: hal::DownlevelProperties,
    ) -> Self {
        let internal_buffer = {
            let desc = d3d11::D3D11_BUFFER_DESC {
                ByteWidth: mem::size_of::<BufferImageCopyInfo>() as _,
                Usage: d3d11::D3D11_USAGE_DYNAMIC,
                BindFlags: d3d11::D3D11_BIND_CONSTANT_BUFFER,
                CPUAccessFlags: d3d11::D3D11_CPU_ACCESS_WRITE,
                MiscFlags: 0,
                StructureByteStride: 0,
            };

            let mut buffer = ptr::null_mut();
            let hr = unsafe {
                device.CreateBuffer(
                    &desc,
                    ptr::null_mut(),
                    &mut buffer as *mut *mut _ as *mut *mut _,
                )
            };
            assert_eq!(true, winerror::SUCCEEDED(hr));

            unsafe { ComPtr::from_raw(buffer) }
        };

        let (depth_stencil_state, depth_state, stencil_state) = {
            let mut depth_state = ptr::null_mut();
            let mut stencil_state = ptr::null_mut();
            let mut depth_stencil_state = ptr::null_mut();

            let mut desc = d3d11::D3D11_DEPTH_STENCIL_DESC {
                DepthEnable: TRUE,
                DepthWriteMask: d3d11::D3D11_DEPTH_WRITE_MASK_ALL,
                DepthFunc: d3d11::D3D11_COMPARISON_ALWAYS,
                StencilEnable: TRUE,
                StencilReadMask: 0,
                StencilWriteMask: !0,
                FrontFace: d3d11::D3D11_DEPTH_STENCILOP_DESC {
                    StencilFailOp: d3d11::D3D11_STENCIL_OP_REPLACE,
                    StencilDepthFailOp: d3d11::D3D11_STENCIL_OP_REPLACE,
                    StencilPassOp: d3d11::D3D11_STENCIL_OP_REPLACE,
                    StencilFunc: d3d11::D3D11_COMPARISON_ALWAYS,
                },
                BackFace: d3d11::D3D11_DEPTH_STENCILOP_DESC {
                    StencilFailOp: d3d11::D3D11_STENCIL_OP_REPLACE,
                    StencilDepthFailOp: d3d11::D3D11_STENCIL_OP_REPLACE,
                    StencilPassOp: d3d11::D3D11_STENCIL_OP_REPLACE,
                    StencilFunc: d3d11::D3D11_COMPARISON_ALWAYS,
                },
            };

            let hr = unsafe {
                device.CreateDepthStencilState(
                    &desc,
                    &mut depth_stencil_state as *mut *mut _ as *mut *mut _,
                )
            };
            assert_eq!(winerror::S_OK, hr);

            desc.DepthEnable = TRUE;
            desc.StencilEnable = FALSE;

            let hr = unsafe {
                device
                    .CreateDepthStencilState(&desc, &mut depth_state as *mut *mut _ as *mut *mut _)
            };
            assert_eq!(winerror::S_OK, hr);

            desc.DepthEnable = FALSE;
            desc.StencilEnable = TRUE;

            let hr = unsafe {
                device.CreateDepthStencilState(
                    &desc,
                    &mut stencil_state as *mut *mut _ as *mut *mut _,
                )
            };
            assert_eq!(winerror::S_OK, hr);

            unsafe {
                (
                    ComPtr::from_raw(depth_stencil_state),
                    ComPtr::from_raw(depth_state),
                    ComPtr::from_raw(stencil_state),
                )
            }
        };

        let (sampler_nearest, sampler_linear) = {
            let mut desc = d3d11::D3D11_SAMPLER_DESC {
                Filter: d3d11::D3D11_FILTER_MIN_MAG_MIP_POINT,
                AddressU: d3d11::D3D11_TEXTURE_ADDRESS_CLAMP,
                AddressV: d3d11::D3D11_TEXTURE_ADDRESS_CLAMP,
                AddressW: d3d11::D3D11_TEXTURE_ADDRESS_CLAMP,
                MipLODBias: 0f32,
                MaxAnisotropy: 0,
                ComparisonFunc: 0,
                BorderColor: [0f32; 4],
                MinLOD: 0f32,
                MaxLOD: d3d11::D3D11_FLOAT32_MAX,
            };

            let mut nearest = ptr::null_mut();
            let mut linear = ptr::null_mut();

            assert_eq!(winerror::S_OK, unsafe {
                device.CreateSamplerState(&desc, &mut nearest as *mut *mut _ as *mut *mut _)
            });

            desc.Filter = d3d11::D3D11_FILTER_MIN_MAG_MIP_LINEAR;

            assert_eq!(winerror::S_OK, unsafe {
                device.CreateSamplerState(&desc, &mut linear as *mut *mut _ as *mut *mut _)
            });

            unsafe { (ComPtr::from_raw(nearest), ComPtr::from_raw(linear)) }
        };

        let (working_buffer, working_buffer_size) = {
            let working_buffer_size = 1 << 16;

            let desc = d3d11::D3D11_BUFFER_DESC {
                ByteWidth: working_buffer_size,
                Usage: d3d11::D3D11_USAGE_STAGING,
                BindFlags: 0,
                CPUAccessFlags: d3d11::D3D11_CPU_ACCESS_READ | d3d11::D3D11_CPU_ACCESS_WRITE,
                MiscFlags: 0,
                StructureByteStride: 0,
            };
            let mut working_buffer = ptr::null_mut();

            assert_eq!(winerror::S_OK, unsafe {
                device.CreateBuffer(
                    &desc,
                    ptr::null_mut(),
                    &mut working_buffer as *mut *mut _ as *mut *mut _,
                )
            });

            (
                unsafe { ComPtr::from_raw(working_buffer) },
                working_buffer_size,
            )
        };

        let compute_shaders = if device_feature_level >= d3dcommon::D3D_FEATURE_LEVEL_11_0 {
            true
        } else {
            // FL10 does support compute shaders but we need typed UAVs, which FL10 compute shaders don't support, so we might as well not have them.
            false
        };

        let shader_model = conv::map_feature_level_to_shader_model(device_feature_level);

        let compute_internal = if compute_shaders {
            let copy_shaders = include_bytes!("../shaders/copy.hlsl");
            let mut cs_copy_image_shaders = FastHashMap::default();
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R8G8_UINT,
                    dxgiformat::DXGI_FORMAT_R16_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r8g8_image2d_r16",
                    shader_model,
                ),
            );
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R16_UINT,
                    dxgiformat::DXGI_FORMAT_R8G8_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r16_image2d_r8g8",
                    shader_model,
                ),
            );
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
                    dxgiformat::DXGI_FORMAT_R32_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r8g8b8a8_image2d_r32",
                    shader_model,
                ),
            );
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
                    dxgiformat::DXGI_FORMAT_R16G16_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r8g8b8a8_image2d_r16g16",
                    shader_model,
                ),
            );
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R16G16_UINT,
                    dxgiformat::DXGI_FORMAT_R32_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r16g16_image2d_r32",
                    shader_model,
                ),
            );
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R16G16_UINT,
                    dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r16g16_image2d_r8g8b8a8",
                    shader_model,
                ),
            );
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R32_UINT,
                    dxgiformat::DXGI_FORMAT_R16G16_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r32_image2d_r16g16",
                    shader_model,
                ),
            );
            cs_copy_image_shaders.insert(
                (
                    dxgiformat::DXGI_FORMAT_R32_UINT,
                    dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
                ),
                compile_cs(
                    device,
                    copy_shaders,
                    "cs_copy_image2d_r32_image2d_r8g8b8a8",
                    shader_model,
                ),
            );

            let mut cs_copy_buffer_shaders = FastHashMap::default();
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R32G32B32A32_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: None,
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r32g32b32a32",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r32g32b32a32_buffer",
                        shader_model,
                    ),
                    scale: (1, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R32G32_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: None,
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r32g32",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r32g32_buffer",
                        shader_model,
                    ),
                    scale: (1, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R32_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image1d_r32",
                        shader_model,
                    )),
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r32",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r32_buffer",
                        shader_model,
                    ),
                    scale: (1, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R16G16B16A16_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: None,
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r16g16b16a16",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r16g16b16a16_buffer",
                        shader_model,
                    ),
                    scale: (1, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R16G16_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: None,
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r16g16",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r16g16_buffer",
                        shader_model,
                    ),
                    scale: (1, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R16_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: None,
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r16",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r16_buffer",
                        shader_model,
                    ),
                    scale: (2, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_B8G8R8A8_UNORM,
                ComputeCopyBuffer {
                    d1_from_buffer: None,
                    d2_from_buffer: None,
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_b8g8r8a8_buffer",
                        shader_model,
                    ),
                    scale: (1, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image1d_r8g8b8a8",
                        shader_model,
                    )),
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r8g8b8a8",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r8g8b8a8_buffer",
                        shader_model,
                    ),
                    scale: (1, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R8G8_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image1d_r8g8",
                        shader_model,
                    )),
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r8g8",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r8g8_buffer",
                        shader_model,
                    ),
                    scale: (2, 1),
                },
            );
            cs_copy_buffer_shaders.insert(
                dxgiformat::DXGI_FORMAT_R8_UINT,
                ComputeCopyBuffer {
                    d1_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image1d_r8",
                        shader_model,
                    )),
                    d2_from_buffer: Some(compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_buffer_image2d_r8",
                        shader_model,
                    )),
                    d2_into_buffer: compile_cs(
                        device,
                        copy_shaders,
                        "cs_copy_image2d_r8_buffer",
                        shader_model,
                    ),
                    scale: (4, 1),
                },
            );

            PossibleComputeInternal::Available(ComputeInternal {
                cs_copy_buffer_shaders,
                cs_copy_image_shaders,
            })
        } else {
            let mut cs_copy_image_shaders = FastHashSet::default();
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R8G8_UINT,
                dxgiformat::DXGI_FORMAT_R16_UINT,
            ));
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R16_UINT,
                dxgiformat::DXGI_FORMAT_R8G8_UINT,
            ));
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
                dxgiformat::DXGI_FORMAT_R32_UINT,
            ));
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
                dxgiformat::DXGI_FORMAT_R16G16_UINT,
            ));
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R16G16_UINT,
                dxgiformat::DXGI_FORMAT_R32_UINT,
            ));
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R16G16_UINT,
                dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
            ));
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R32_UINT,
                dxgiformat::DXGI_FORMAT_R16G16_UINT,
            ));
            cs_copy_image_shaders.insert((
                dxgiformat::DXGI_FORMAT_R32_UINT,
                dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT,
            ));

            let mut cs_copy_buffer_shaders = FastHashSet::default();
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R32G32B32A32_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R32G32_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R32_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R16G16B16A16_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R16G16_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R16_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_B8G8R8A8_UNORM);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R8G8B8A8_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R8G8_UINT);
            cs_copy_buffer_shaders.insert(dxgiformat::DXGI_FORMAT_R8_UINT);

            PossibleComputeInternal::Missing(MissingComputeInternal {
                cs_copy_image_shaders,
                cs_copy_buffer_shaders,
            })
        };

        let mut threading_capability: d3d11::D3D11_FEATURE_DATA_THREADING =
            unsafe { mem::zeroed() };
        let hr = unsafe {
            device.CheckFeatureSupport(
                d3d11::D3D11_FEATURE_THREADING,
                &mut threading_capability as *mut _ as *mut _,
                mem::size_of::<d3d11::D3D11_FEATURE_DATA_THREADING>() as _,
            )
        };
        assert_eq!(hr, winerror::S_OK);

        let command_list_emulation = !(threading_capability.DriverCommandLists >= 1);
        if command_list_emulation {
            info!("D3D11 command list emulation is active");
        }

        let clear_shaders = include_bytes!("../shaders/clear.hlsl");
        let blit_shaders = include_bytes!("../shaders/blit.hlsl");

        Internal {
            vs_partial_clear: compile_vs(device, clear_shaders, "vs_partial_clear", shader_model),
            ps_partial_clear_float: compile_ps(
                device,
                clear_shaders,
                "ps_partial_clear_float",
                shader_model,
            ),
            ps_partial_clear_uint: compile_ps(
                device,
                clear_shaders,
                "ps_partial_clear_uint",
                shader_model,
            ),
            ps_partial_clear_int: compile_ps(
                device,
                clear_shaders,
                "ps_partial_clear_int",
                shader_model,
            ),
            ps_partial_clear_depth: compile_ps(
                device,
                clear_shaders,
                "ps_partial_clear_depth",
                shader_model,
            ),
            ps_partial_clear_stencil: compile_ps(
                device,
                clear_shaders,
                "ps_partial_clear_stencil",
                shader_model,
            ),
            partial_clear_depth_stencil_state: depth_stencil_state,
            partial_clear_depth_state: depth_state,
            partial_clear_stencil_state: stencil_state,

            vs_blit_2d: compile_vs(device, blit_shaders, "vs_blit_2d", shader_model),

            sampler_nearest,
            sampler_linear,

            ps_blit_2d_uint: compile_ps(device, blit_shaders, "ps_blit_2d_uint", shader_model),
            ps_blit_2d_int: compile_ps(device, blit_shaders, "ps_blit_2d_int", shader_model),
            ps_blit_2d_float: compile_ps(device, blit_shaders, "ps_blit_2d_float", shader_model),

            compute_internal,

            internal_buffer: Mutex::new(ConstantBuffer {
                buffer: internal_buffer,
            }),
            working_buffer,
            working_buffer_size: working_buffer_size as _,

            constant_buffer_count_buffer: [4096_u32;
                d3d11::D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT as _],

            command_list_emulation,

            device_features,
            device_feature_level,

            downlevel,
        }
    }

    pub fn copy_image_2d<T>(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Image,
        dst: &Image,
        regions: T,
    ) where
        T: Iterator<Item = command::ImageCopy>,
    {
        let key = (
            src.decomposed_format.copy_srv.unwrap(),
            dst.decomposed_format.copy_srv.unwrap(),
        );

        let compute_shader_copy = if let PossibleComputeInternal::Available(ref compute_internal) =
            self.compute_internal
        {
            compute_internal.cs_copy_image_shaders.get(&key)
        } else if let PossibleComputeInternal::Missing(ref compute_internal) = self.compute_internal
        {
            if compute_internal.cs_copy_image_shaders.contains(&key) {
                panic!("Tried to copy between images types ({:?} -> {:?}) that require Compute Shaders under FL9 or FL10", src.format, dst.format);
            } else {
                None
            }
        } else {
            None
        };

        if let Some(shader) = compute_shader_copy {
            // Some formats cant go through default path, since they cant
            // be cast between formats of different component types (eg.
            // Rg16 <-> Rgba8)

            // TODO: subresources
            let srv = src.internal.copy_srv.clone().unwrap().as_raw();
            let mut const_buf = self.internal_buffer.lock();

            unsafe {
                context.CSSetShader(shader.as_raw(), ptr::null_mut(), 0);
                context.CSSetConstantBuffers(0, 1, &const_buf.buffer.as_raw());
                context.CSSetShaderResources(0, 1, [srv].as_ptr());

                for info in regions {
                    let image = ImageCopy {
                        src: [
                            info.src_offset.x as _,
                            info.src_offset.y as _,
                            info.src_offset.z as _,
                            0,
                        ],
                        dst: [
                            info.dst_offset.x as _,
                            info.dst_offset.y as _,
                            info.dst_offset.z as _,
                            0,
                        ],
                    };
                    const_buf.update(
                        context,
                        BufferImageCopyInfo {
                            image,
                            ..mem::zeroed()
                        },
                    );

                    let uav = dst.get_uav(info.dst_subresource.level, 0).unwrap().as_raw();
                    context.CSSetUnorderedAccessViews(0, 1, [uav].as_ptr(), ptr::null_mut());

                    context.Dispatch(info.extent.width as u32, info.extent.height as u32, 1);
                }

                // unbind external resources
                context.CSSetShaderResources(0, 1, [ptr::null_mut(); 1].as_ptr());
                context.CSSetUnorderedAccessViews(
                    0,
                    1,
                    [ptr::null_mut(); 1].as_ptr(),
                    ptr::null_mut(),
                );
            }
        } else {
            // Default copy path
            for info in regions {
                assert_eq!(
                    src.decomposed_format.typeless, dst.decomposed_format.typeless,
                    "DX11 backend cannot copy between underlying image formats: {} to {}.",
                    src.decomposed_format.typeless, dst.decomposed_format.typeless,
                );

                // Formats are the same per above assert, only need to do it for one of the formats
                let full_copy_only =
                    src.format.is_depth() || src.format.is_stencil() || src.kind.num_samples() > 1;

                let copy_box = if full_copy_only {
                    let offset_zero = info.src_offset.x == 0
                        && info.src_offset.y == 0
                        && info.src_offset.z == 0
                        && info.dst_offset.x == 0
                        && info.dst_offset.y == 0
                        && info.dst_offset.z == 0;

                    let full_extent = info.extent == src.kind.extent();

                    if !offset_zero || !full_extent {
                        warn!("image to image copies of depth-stencil or multisampled textures must copy the whole resource. Ignoring non-zero offset or non-full extent.");
                    }

                    None
                } else {
                    Some(d3d11::D3D11_BOX {
                        left: info.src_offset.x as _,
                        top: info.src_offset.y as _,
                        front: info.src_offset.z as _,
                        right: info.src_offset.x as u32 + info.extent.width as u32,
                        bottom: info.src_offset.y as u32 + info.extent.height as u32,
                        back: info.src_offset.z as u32 + info.extent.depth as u32,
                    })
                };

                // TODO: layer subresources
                unsafe {
                    context.CopySubresourceRegion(
                        dst.internal.raw,
                        src.calc_subresource(info.src_subresource.level as _, 0),
                        info.dst_offset.x as _,
                        info.dst_offset.y as _,
                        info.dst_offset.z as _,
                        src.internal.raw,
                        dst.calc_subresource(info.dst_subresource.level as _, 0),
                        copy_box.map_or_else(ptr::null, |b| &b),
                    );
                }
            }
        }
    }

    pub fn copy_image_to_buffer<T>(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Image,
        dst: &Buffer,
        regions: T,
    ) where
        T: Iterator<Item = command::BufferImageCopy>,
    {
        let _scope = debug_scope!(
            context,
            "Image (format={:?},kind={:?}) => Buffer",
            src.format,
            src.kind
        );

        let shader = if let PossibleComputeInternal::Available(ref compute_internal) =
            self.compute_internal
        {
            compute_internal
                .cs_copy_buffer_shaders
                .get(&src.decomposed_format.copy_srv.unwrap())
                .unwrap_or_else(|| panic!("The DX11 backend does not currently support copying from an image of format {:?} to a buffer", src.format))
                .clone()
        } else {
            // TODO(cwfitzgerald): Can we fall back to a inherent D3D11 method when copying to a CPU only buffer.
            panic!("Tried to copy from an image to a buffer under FL9 or FL10");
        };

        let srv = src.internal.copy_srv.clone().unwrap().as_raw();
        let uav = dst.internal.uav.unwrap();
        let format_desc = src.format.base_format().0.desc();
        let bytes_per_texel = format_desc.bits as u32 / 8;
        let mut const_buf = self.internal_buffer.lock();

        unsafe {
            context.CSSetShader(shader.d2_into_buffer.as_raw(), ptr::null_mut(), 0);
            context.CSSetConstantBuffers(0, 1, &const_buf.buffer.as_raw());

            context.CSSetShaderResources(0, 1, [srv].as_ptr());
            context.CSSetUnorderedAccessViews(0, 1, [uav].as_ptr(), ptr::null_mut());

            for info in regions {
                let size = src.kind.extent();
                let buffer_image = BufferImageCopy {
                    buffer_offset: info.buffer_offset as _,
                    buffer_size: [info.buffer_width, info.buffer_height],
                    _padding: 0,
                    image_offset: [
                        info.image_offset.x as _,
                        info.image_offset.y as _,
                        (info.image_offset.z + info.image_layers.layers.start as i32) as _,
                        0,
                    ],
                    image_extent: [
                        info.image_extent.width,
                        info.image_extent.height,
                        info.image_extent.depth,
                        0,
                    ],
                    image_size: [size.width, size.height, size.depth, 0],
                };

                const_buf.update(
                    context,
                    BufferImageCopyInfo {
                        buffer_image,
                        ..mem::zeroed()
                    },
                );

                debug_marker!(context, "{:?}", info);

                context.Dispatch(
                    ((info.image_extent.width + (COPY_THREAD_GROUP_X - 1))
                        / COPY_THREAD_GROUP_X
                        / shader.scale.0)
                        .max(1),
                    ((info.image_extent.height + (COPY_THREAD_GROUP_X - 1))
                        / COPY_THREAD_GROUP_Y
                        / shader.scale.1)
                        .max(1),
                    1,
                );

                if let Some(disjoint_cb) = dst.internal.disjoint_cb {
                    let total_size = info.image_extent.depth
                        * (info.buffer_height * info.buffer_width * bytes_per_texel);
                    let copy_box = d3d11::D3D11_BOX {
                        left: info.buffer_offset as u32,
                        top: 0,
                        front: 0,
                        right: info.buffer_offset as u32 + total_size,
                        bottom: 1,
                        back: 1,
                    };

                    context.CopySubresourceRegion(
                        disjoint_cb as _,
                        0,
                        info.buffer_offset as _,
                        0,
                        0,
                        dst.internal.raw as _,
                        0,
                        &copy_box,
                    );
                }
            }

            // unbind external resources
            context.CSSetShaderResources(0, 1, [ptr::null_mut(); 1].as_ptr());
            context.CSSetUnorderedAccessViews(0, 1, [ptr::null_mut(); 1].as_ptr(), ptr::null_mut());
        }
    }

    pub fn copy_buffer_to_image<T>(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Buffer,
        dst: &Image,
        regions: T,
    ) where
        T: Iterator<Item = command::BufferImageCopy>,
    {
        let _scope = debug_scope!(
            context,
            "Buffer => Image (format={:?},kind={:?})",
            dst.format,
            dst.kind
        );

        let compute_internal = match self.compute_internal {
            PossibleComputeInternal::Available(ref available) => Some(available),
            PossibleComputeInternal::Missing(_) => None,
        };

        // NOTE: we have two separate paths for Buffer -> Image transfers. we need to special case
        //       uploads to compressed formats through `UpdateSubresource` since we cannot get a
        //       UAV of any compressed format.

        let format_desc = dst.format.base_format().0.desc();
        let is_compressed = format_desc.is_compressed();
        if format_desc.is_compressed() || compute_internal.is_none() {
            // we dont really care about non-4x4 block formats..
            if is_compressed {
                assert_eq!(format_desc.dim, (4, 4));
                assert!(
                    !src.memory_ptr.is_null(),
                    "Only CPU to GPU upload of compressed texture is currently supported"
                );
            } else if compute_internal.is_none() {
                assert!(
                    !src.memory_ptr.is_null(),
                    "Only CPU to GPU upload of textures is supported under FL9 or FL10"
                );
            }

            for info in regions {
                let bytes_per_texel = format_desc.bits as u32 / 8;

                let bounds = d3d11::D3D11_BOX {
                    left: info.image_offset.x as _,
                    top: info.image_offset.y as _,
                    front: info.image_offset.z as _,
                    right: info.image_offset.x as u32 + info.image_extent.width,
                    bottom: info.image_offset.y as u32 + info.image_extent.height,
                    back: info.image_offset.z as u32 + info.image_extent.depth,
                };

                let row_pitch =
                    bytes_per_texel * info.image_extent.width / format_desc.dim.0 as u32;
                let depth_pitch = row_pitch * info.image_extent.height / format_desc.dim.1 as u32;

                for layer in info.image_layers.layers.clone() {
                    let layer_offset = layer - info.image_layers.layers.start;

                    unsafe {
                        context.UpdateSubresource(
                            dst.internal.raw,
                            dst.calc_subresource(info.image_layers.level as _, layer as _),
                            &bounds,
                            src.memory_ptr.offset(
                                src.bound_range.start as isize
                                    + info.buffer_offset as isize
                                    + depth_pitch as isize * layer_offset as isize,
                            ) as _,
                            row_pitch,
                            depth_pitch,
                        );
                    }
                }
            }
        } else {
            let shader = compute_internal
                .unwrap()
                .cs_copy_buffer_shaders
                .get(&dst.decomposed_format.copy_uav.unwrap())
                .unwrap_or_else(|| panic!("The DX11 backend does not currently support copying from a buffer to an image of format {:?}", dst.format))
                .clone();

            let srv = src.internal.srv.unwrap();
            let mut const_buf = self.internal_buffer.lock();
            let shader_raw = match dst.kind {
                image::Kind::D1(..) => shader.d1_from_buffer.unwrap().as_raw(),
                image::Kind::D2(..) => shader.d2_from_buffer.unwrap().as_raw(),
                image::Kind::D3(..) => panic!("Copies into 3D images are not supported"),
            };

            unsafe {
                context.CSSetShader(shader_raw, ptr::null_mut(), 0);
                context.CSSetConstantBuffers(0, 1, &const_buf.buffer.as_raw());
                context.CSSetShaderResources(0, 1, [srv].as_ptr());

                for info in regions {
                    let size = dst.kind.extent();
                    let buffer_image = BufferImageCopy {
                        buffer_offset: info.buffer_offset as _,
                        buffer_size: [info.buffer_width, info.buffer_height],
                        _padding: 0,
                        image_offset: [
                            info.image_offset.x as _,
                            info.image_offset.y as _,
                            (info.image_offset.z + info.image_layers.layers.start as i32) as _,
                            0,
                        ],
                        image_extent: [
                            info.image_extent.width,
                            info.image_extent.height,
                            info.image_extent.depth,
                            0,
                        ],
                        image_size: [size.width, size.height, size.depth, 0],
                    };

                    const_buf.update(
                        context,
                        BufferImageCopyInfo {
                            buffer_image,
                            ..mem::zeroed()
                        },
                    );

                    debug_marker!(context, "{:?}", info);

                    // TODO: multiple layers? do we introduce a stride and do multiple dispatch
                    //       calls or handle this in the shader? (use z component in dispatch call
                    //
                    // NOTE: right now our copy UAV is a 2D array, and we set the layer in the
                    //       `update_buffer_image` call above
                    let uav = dst
                        .get_uav(
                            info.image_layers.level,
                            0, /*info.image_layers.layers.start*/
                        )
                        .unwrap()
                        .as_raw();
                    context.CSSetUnorderedAccessViews(0, 1, [uav].as_ptr(), ptr::null_mut());

                    context.Dispatch(
                        ((info.image_extent.width + (COPY_THREAD_GROUP_X - 1))
                            / COPY_THREAD_GROUP_X
                            / shader.scale.0)
                            .max(1),
                        ((info.image_extent.height + (COPY_THREAD_GROUP_X - 1))
                            / COPY_THREAD_GROUP_Y
                            / shader.scale.1)
                            .max(1),
                        1,
                    );
                }

                // unbind external resources
                context.CSSetShaderResources(0, 1, [ptr::null_mut(); 1].as_ptr());
                context.CSSetUnorderedAccessViews(
                    0,
                    1,
                    [ptr::null_mut(); 1].as_ptr(),
                    ptr::null_mut(),
                );
            }
        }
    }

    fn find_blit_shader(&self, src: &Image) -> Option<*mut d3d11::ID3D11PixelShader> {
        use crate::format::ChannelType as Ct;

        match src.format.base_format().1 {
            Ct::Uint => Some(self.ps_blit_2d_uint.as_raw()),
            Ct::Sint => Some(self.ps_blit_2d_int.as_raw()),
            Ct::Unorm | Ct::Snorm | Ct::Sfloat | Ct::Srgb => Some(self.ps_blit_2d_float.as_raw()),
            Ct::Ufloat | Ct::Uscaled | Ct::Sscaled => None,
        }
    }

    pub fn blit_2d_image<T>(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Image,
        dst: &Image,
        filter: image::Filter,
        regions: T,
    ) where
        T: Iterator<Item = command::ImageBlit>,
    {
        use std::cmp;

        let _scope = debug_scope!(
            context,
            "Blit: Image (format={:?},kind={:?}) => Image (format={:?},kind={:?})",
            src.format,
            src.kind,
            dst.format,
            dst.kind
        );

        let shader = self.find_blit_shader(src).unwrap();

        let srv = src.internal.srv.clone().unwrap().as_raw();
        let mut const_buf = self.internal_buffer.lock();

        unsafe {
            context.IASetPrimitiveTopology(d3dcommon::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context.VSSetShader(self.vs_blit_2d.as_raw(), ptr::null_mut(), 0);
            context.VSSetConstantBuffers(0, 1, [const_buf.buffer.as_raw()].as_ptr());
            context.PSSetShader(shader, ptr::null_mut(), 0);
            context.PSSetShaderResources(0, 1, [srv].as_ptr());
            context.PSSetSamplers(
                0,
                1,
                match filter {
                    image::Filter::Nearest => [self.sampler_nearest.as_raw()],
                    image::Filter::Linear => [self.sampler_linear.as_raw()],
                }
                .as_ptr(),
            );

            for info in regions {
                let blit_info = {
                    let (sx, dx) = if info.dst_bounds.start.x > info.dst_bounds.end.x {
                        (
                            info.src_bounds.end.x,
                            info.src_bounds.start.x - info.src_bounds.end.x,
                        )
                    } else {
                        (
                            info.src_bounds.start.x,
                            info.src_bounds.end.x - info.src_bounds.start.x,
                        )
                    };
                    let (sy, dy) = if info.dst_bounds.start.y > info.dst_bounds.end.y {
                        (
                            info.src_bounds.end.y,
                            info.src_bounds.start.y - info.src_bounds.end.y,
                        )
                    } else {
                        (
                            info.src_bounds.start.y,
                            info.src_bounds.end.y - info.src_bounds.start.y,
                        )
                    };
                    let image::Extent { width, height, .. } =
                        src.kind.level_extent(info.src_subresource.level);
                    BlitInfo {
                        offset: [sx as f32 / width as f32, sy as f32 / height as f32],
                        extent: [dx as f32 / width as f32, dy as f32 / height as f32],
                        z: 0f32, // TODO
                        level: info.src_subresource.level as _,
                    }
                };

                const_buf.update(context, blit_info);

                // TODO: more layers
                let rtv = dst
                    .get_rtv(
                        info.dst_subresource.level,
                        info.dst_subresource.layers.start,
                    )
                    .unwrap()
                    .as_raw();

                context.RSSetViewports(
                    1,
                    [d3d11::D3D11_VIEWPORT {
                        TopLeftX: cmp::min(info.dst_bounds.start.x, info.dst_bounds.end.x) as _,
                        TopLeftY: cmp::min(info.dst_bounds.start.y, info.dst_bounds.end.y) as _,
                        Width: (info.dst_bounds.end.x - info.dst_bounds.start.x).abs() as _,
                        Height: (info.dst_bounds.end.y - info.dst_bounds.start.y).abs() as _,
                        MinDepth: 0.0f32,
                        MaxDepth: 1.0f32,
                    }]
                    .as_ptr(),
                );
                context.OMSetRenderTargets(1, [rtv].as_ptr(), ptr::null_mut());
                context.Draw(3, 0);
            }

            context.PSSetShaderResources(0, 1, [ptr::null_mut()].as_ptr());
            context.OMSetRenderTargets(1, [ptr::null_mut()].as_ptr(), ptr::null_mut());
        }
    }

    pub fn clear_attachments<T, U>(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        clears: T,
        rects: U,
        cache: &RenderPassCache,
    ) where
        T: Iterator<Item = command::AttachmentClear>,
        U: Iterator<Item = pso::ClearRect>,
    {
        use hal::format::ChannelType as Ct;
        let _scope = debug_scope!(context, "ClearAttachments");

        let clear_rects: SmallVec<[pso::ClearRect; 8]> = rects.collect();
        let mut const_buf = self.internal_buffer.lock();

        unsafe {
            context.IASetPrimitiveTopology(d3dcommon::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context.IASetInputLayout(ptr::null_mut());
            context.VSSetShader(self.vs_partial_clear.as_raw(), ptr::null_mut(), 0);
            context.PSSetConstantBuffers(0, 1, [const_buf.buffer.as_raw()].as_ptr());
        }

        let subpass = &cache.render_pass.subpasses[cache.current_subpass as usize];

        for clear in clears {
            let _scope = debug_scope!(context, "{:?}", clear);

            match clear {
                command::AttachmentClear::Color { index, value } => {
                    unsafe {
                        const_buf.update(
                            context,
                            PartialClearInfo {
                                data: mem::transmute(value),
                            },
                        )
                    };

                    let attachment = {
                        let rtv_id = subpass.color_attachments[index];
                        &cache.attachments[rtv_id.0].view
                    };

                    unsafe {
                        context.OMSetRenderTargets(
                            1,
                            [attachment.rtv_handle.unwrap()].as_ptr(),
                            ptr::null_mut(),
                        );
                    }

                    let shader = match attachment.format.base_format().1 {
                        Ct::Uint => self.ps_partial_clear_uint.as_raw(),
                        Ct::Sint => self.ps_partial_clear_int.as_raw(),
                        _ => self.ps_partial_clear_float.as_raw(),
                    };
                    unsafe { context.PSSetShader(shader, ptr::null_mut(), 0) };

                    for clear_rect in &clear_rects {
                        let viewport = conv::map_viewport(&pso::Viewport {
                            rect: clear_rect.rect,
                            depth: 0f32..1f32,
                        });

                        debug_marker!(context, "{:?}", clear_rect.rect);

                        unsafe {
                            context.RSSetViewports(1, [viewport].as_ptr());
                            context.Draw(3, 0);
                        }
                    }
                }
                command::AttachmentClear::DepthStencil { depth, stencil } => {
                    unsafe {
                        const_buf.update(
                            context,
                            PartialClearInfo {
                                data: [
                                    mem::transmute(depth.unwrap_or(0f32)),
                                    stencil.unwrap_or(0),
                                    0,
                                    0,
                                ],
                            },
                        )
                    };

                    let attachment = {
                        let dsv_id = subpass.depth_stencil_attachment.unwrap();
                        &cache.attachments[dsv_id.0].view
                    };

                    unsafe {
                        match (depth, stencil) {
                            (Some(_), Some(stencil)) => {
                                context.OMSetDepthStencilState(
                                    self.partial_clear_depth_stencil_state.as_raw(),
                                    stencil,
                                );
                                context.PSSetShader(
                                    self.ps_partial_clear_depth.as_raw(),
                                    ptr::null_mut(),
                                    0,
                                );
                            }

                            (Some(_), None) => {
                                context.OMSetDepthStencilState(
                                    self.partial_clear_depth_state.as_raw(),
                                    0,
                                );
                                context.PSSetShader(
                                    self.ps_partial_clear_depth.as_raw(),
                                    ptr::null_mut(),
                                    0,
                                );
                            }

                            (None, Some(stencil)) => {
                                context.OMSetDepthStencilState(
                                    self.partial_clear_stencil_state.as_raw(),
                                    stencil,
                                );
                                context.PSSetShader(
                                    self.ps_partial_clear_stencil.as_raw(),
                                    ptr::null_mut(),
                                    0,
                                );
                            }
                            (None, None) => {}
                        }

                        context.OMSetRenderTargets(
                            0,
                            ptr::null_mut(),
                            attachment.dsv_handle.unwrap(),
                        );
                        context.PSSetShader(
                            self.ps_partial_clear_depth.as_raw(),
                            ptr::null_mut(),
                            0,
                        );
                    }

                    for clear_rect in &clear_rects {
                        let viewport = conv::map_viewport(&pso::Viewport {
                            rect: clear_rect.rect,
                            depth: 0f32..1f32,
                        });

                        unsafe {
                            context.RSSetViewports(1, [viewport].as_ptr());
                            context.Draw(3, 0);
                        }
                    }
                }
            }
        }
    }
}
