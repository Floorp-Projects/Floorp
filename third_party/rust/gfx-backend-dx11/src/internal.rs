use hal::pso::{Stage, Viewport};
use hal::{command, image, pso};

use winapi::shared::dxgiformat;
use winapi::shared::minwindef::{FALSE, TRUE};
use winapi::shared::winerror;
use winapi::um::d3d11;
use winapi::um::d3dcommon;

use wio::com::ComPtr;

use std::borrow::Borrow;
use std::{mem, ptr};

use smallvec::SmallVec;
use spirv_cross;

use {conv, shader};

use {Buffer, Image, RenderPassCache};

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

// Holds everything we need for fallback implementations of features that are not in DX.
//
// TODO: maybe get rid of `Clone`? there's _a lot_ of refcounts here and it is used as a singleton
//       anyway :s
//
// TODO: make struct fields more modular and group them up in structs depending on if it is a
//       fallback version or not (eg. Option<PartialClear>), should make struct definition and
//       `new` function smaller
#[derive(Clone, Debug)]
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

    // Image<->Image not covered by `CopySubresourceRegion`
    cs_copy_image2d_r8g8_image2d_r16: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r16_image2d_r8g8: ComPtr<d3d11::ID3D11ComputeShader>,

    cs_copy_image2d_r8g8b8a8_image2d_r32: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r8g8b8a8_image2d_r16g16: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r16g16_image2d_r32: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r16g16_image2d_r8g8b8a8: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r32_image2d_r16g16: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r32_image2d_r8g8b8a8: ComPtr<d3d11::ID3D11ComputeShader>,

    // Image -> Buffer
    cs_copy_image2d_r32g32b32a32_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r32g32_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r16g16b16a16_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r32_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r16g16_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r8g8b8a8_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r16_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r8g8_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_r8_buffer: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_image2d_b8g8r8a8_buffer: ComPtr<d3d11::ID3D11ComputeShader>,

    // Buffer -> Image
    cs_copy_buffer_image2d_r32g32b32a32: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r32g32: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r16g16b16a16: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r32: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r16g16: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r8g8b8a8: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r16: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r8g8: ComPtr<d3d11::ID3D11ComputeShader>,
    cs_copy_buffer_image2d_r8: ComPtr<d3d11::ID3D11ComputeShader>,

    // internal constant buffer that is used by internal shaders
    internal_buffer: ComPtr<d3d11::ID3D11Buffer>,

    // public buffer that is used as intermediate storage for some operations (memory invalidation)
    pub working_buffer: ComPtr<d3d11::ID3D11Buffer>,
    pub working_buffer_size: u64,
}

fn compile_blob(src: &[u8], entrypoint: &str, stage: Stage) -> ComPtr<d3dcommon::ID3DBlob> {
    unsafe {
        ComPtr::from_raw(
            shader::compile_hlsl_shader(
                stage,
                spirv_cross::hlsl::ShaderModel::V5_0,
                entrypoint,
                src,
            )
            .unwrap(),
        )
    }
}

fn compile_vs(
    device: &ComPtr<d3d11::ID3D11Device>,
    src: &[u8],
    entrypoint: &str,
) -> ComPtr<d3d11::ID3D11VertexShader> {
    let bytecode = compile_blob(src, entrypoint, Stage::Vertex);
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
) -> ComPtr<d3d11::ID3D11PixelShader> {
    let bytecode = compile_blob(src, entrypoint, Stage::Fragment);
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
) -> ComPtr<d3d11::ID3D11ComputeShader> {
    let bytecode = compile_blob(src, entrypoint, Stage::Compute);
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
    pub fn new(device: &ComPtr<d3d11::ID3D11Device>) -> Self {
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

        let clear_shaders = include_bytes!("../shaders/clear.hlsl");
        let copy_shaders = include_bytes!("../shaders/copy.hlsl");
        let blit_shaders = include_bytes!("../shaders/blit.hlsl");

        Internal {
            vs_partial_clear: compile_vs(device, clear_shaders, "vs_partial_clear"),
            ps_partial_clear_float: compile_ps(device, clear_shaders, "ps_partial_clear_float"),
            ps_partial_clear_uint: compile_ps(device, clear_shaders, "ps_partial_clear_uint"),
            ps_partial_clear_int: compile_ps(device, clear_shaders, "ps_partial_clear_int"),
            ps_partial_clear_depth: compile_ps(device, clear_shaders, "ps_partial_clear_depth"),
            ps_partial_clear_stencil: compile_ps(device, clear_shaders, "ps_partial_clear_stencil"),
            partial_clear_depth_stencil_state: depth_stencil_state,
            partial_clear_depth_state: depth_state,
            partial_clear_stencil_state: stencil_state,

            vs_blit_2d: compile_vs(device, blit_shaders, "vs_blit_2d"),

            sampler_nearest,
            sampler_linear,

            ps_blit_2d_uint: compile_ps(device, blit_shaders, "ps_blit_2d_uint"),
            ps_blit_2d_int: compile_ps(device, blit_shaders, "ps_blit_2d_int"),
            ps_blit_2d_float: compile_ps(device, blit_shaders, "ps_blit_2d_float"),

            cs_copy_image2d_r8g8_image2d_r16: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r8g8_image2d_r16",
            ),
            cs_copy_image2d_r16_image2d_r8g8: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r16_image2d_r8g8",
            ),

            cs_copy_image2d_r8g8b8a8_image2d_r32: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r8g8b8a8_image2d_r32",
            ),
            cs_copy_image2d_r8g8b8a8_image2d_r16g16: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r8g8b8a8_image2d_r16g16",
            ),
            cs_copy_image2d_r16g16_image2d_r32: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r16g16_image2d_r32",
            ),
            cs_copy_image2d_r16g16_image2d_r8g8b8a8: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r16g16_image2d_r8g8b8a8",
            ),
            cs_copy_image2d_r32_image2d_r16g16: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r32_image2d_r16g16",
            ),
            cs_copy_image2d_r32_image2d_r8g8b8a8: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r32_image2d_r8g8b8a8",
            ),

            cs_copy_image2d_r32g32b32a32_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r32g32b32a32_buffer",
            ),
            cs_copy_image2d_r32g32_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r32g32_buffer",
            ),
            cs_copy_image2d_r16g16b16a16_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r16g16b16a16_buffer",
            ),
            cs_copy_image2d_r32_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r32_buffer",
            ),
            cs_copy_image2d_r16g16_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r16g16_buffer",
            ),
            cs_copy_image2d_r8g8b8a8_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r8g8b8a8_buffer",
            ),
            cs_copy_image2d_r16_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r16_buffer",
            ),
            cs_copy_image2d_r8g8_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r8g8_buffer",
            ),
            cs_copy_image2d_r8_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_r8_buffer",
            ),
            cs_copy_image2d_b8g8r8a8_buffer: compile_cs(
                device,
                copy_shaders,
                "cs_copy_image2d_b8g8r8a8_buffer",
            ),

            cs_copy_buffer_image2d_r32g32b32a32: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r32g32b32a32",
            ),
            cs_copy_buffer_image2d_r32g32: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r32g32",
            ),
            cs_copy_buffer_image2d_r16g16b16a16: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r16g16b16a16",
            ),
            cs_copy_buffer_image2d_r32: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r32",
            ),
            cs_copy_buffer_image2d_r16g16: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r16g16",
            ),
            cs_copy_buffer_image2d_r8g8b8a8: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r8g8b8a8",
            ),
            cs_copy_buffer_image2d_r16: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r16",
            ),
            cs_copy_buffer_image2d_r8g8: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r8g8",
            ),
            cs_copy_buffer_image2d_r8: compile_cs(
                device,
                copy_shaders,
                "cs_copy_buffer_image2d_r8",
            ),

            internal_buffer,
            working_buffer,
            working_buffer_size: working_buffer_size as _,
        }
    }

    fn map(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) -> *mut u8 {
        let mut mapped = unsafe { mem::zeroed::<d3d11::D3D11_MAPPED_SUBRESOURCE>() };
        let hr = unsafe {
            context.Map(
                self.internal_buffer.as_raw() as _,
                0,
                d3d11::D3D11_MAP_WRITE_DISCARD,
                0,
                &mut mapped,
            )
        };

        assert_eq!(winerror::S_OK, hr);

        mapped.pData as _
    }

    fn unmap(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        unsafe {
            context.Unmap(self.internal_buffer.as_raw() as _, 0);
        }
    }

    fn update_image(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        info: &command::ImageCopy,
    ) {
        unsafe {
            ptr::copy(
                &BufferImageCopyInfo {
                    image: ImageCopy {
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
                    },
                    ..mem::zeroed()
                },
                self.map(context) as *mut _,
                1,
            )
        };

        self.unmap(context);
    }

    fn update_buffer_image(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        info: &command::BufferImageCopy,
        image: &Image,
    ) {
        let size = image.kind.extent();

        unsafe {
            ptr::copy(
                &BufferImageCopyInfo {
                    buffer_image: BufferImageCopy {
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
                    },
                    ..mem::zeroed()
                },
                self.map(context) as *mut _,
                1,
            )
        };

        self.unmap(context);
    }

    fn update_blit(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Image,
        info: &command::ImageBlit,
    ) {
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
        let image::Extent { width, height, .. } = src.kind.level_extent(info.src_subresource.level);

        unsafe {
            ptr::copy(
                &BlitInfo {
                    offset: [sx as f32 / width as f32, sy as f32 / height as f32],
                    extent: [dx as f32 / width as f32, dy as f32 / height as f32],
                    z: 0f32, // TODO
                    level: info.src_subresource.level as _,
                },
                self.map(context) as *mut _,
                1,
            )
        };

        self.unmap(context);
    }

    fn update_clear_color(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        value: command::ClearColor,
    ) {
        unsafe {
            ptr::copy(
                &PartialClearInfo {
                    data: mem::transmute(value),
                },
                self.map(context) as *mut _,
                1,
            )
        };

        self.unmap(context);
    }

    fn update_clear_depth_stencil(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        depth: Option<f32>,
        stencil: Option<u32>,
    ) {
        unsafe {
            ptr::copy(
                &PartialClearInfo {
                    data: [
                        mem::transmute(depth.unwrap_or(0f32)),
                        stencil.unwrap_or(0),
                        0,
                        0,
                    ],
                },
                self.map(context) as *mut _,
                1,
            );
        }

        self.unmap(context);
    }

    fn find_image_copy_shader(
        &self,
        src: &Image,
        dst: &Image,
    ) -> Option<*mut d3d11::ID3D11ComputeShader> {
        use dxgiformat::*;

        let src_format = src.decomposed_format.copy_srv.unwrap();
        let dst_format = dst.decomposed_format.copy_uav.unwrap();

        match (src_format, dst_format) {
            (DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R16_UINT) => {
                Some(self.cs_copy_image2d_r8g8_image2d_r16.as_raw())
            }
            (DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R8G8_UINT) => {
                Some(self.cs_copy_image2d_r16_image2d_r8g8.as_raw())
            }
            (DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R32_UINT) => {
                Some(self.cs_copy_image2d_r8g8b8a8_image2d_r32.as_raw())
            }
            (DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R16G16_UINT) => {
                Some(self.cs_copy_image2d_r8g8b8a8_image2d_r16g16.as_raw())
            }
            (DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R32_UINT) => {
                Some(self.cs_copy_image2d_r16g16_image2d_r32.as_raw())
            }
            (DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R8G8B8A8_UINT) => {
                Some(self.cs_copy_image2d_r16g16_image2d_r8g8b8a8.as_raw())
            }
            (DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R16G16_UINT) => {
                Some(self.cs_copy_image2d_r32_image2d_r16g16.as_raw())
            }
            (DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UINT) => {
                Some(self.cs_copy_image2d_r32_image2d_r8g8b8a8.as_raw())
            }
            _ => None,
        }
    }

    pub fn copy_image_2d<T>(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Image,
        dst: &Image,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ImageCopy>,
    {
        if let Some(shader) = self.find_image_copy_shader(src, dst) {
            // Some formats cant go through default path, since they cant
            // be cast between formats of different component types (eg.
            // Rg16 <-> Rgba8)

            // TODO: subresources
            let srv = src.internal.copy_srv.clone().unwrap().as_raw();

            unsafe {
                context.CSSetShader(shader, ptr::null_mut(), 0);
                context.CSSetConstantBuffers(0, 1, &self.internal_buffer.as_raw());
                context.CSSetShaderResources(0, 1, [srv].as_ptr());

                for region in regions.into_iter() {
                    let info = region.borrow();
                    self.update_image(context, &info);

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
            for region in regions.into_iter() {
                let info = region.borrow();

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
                        &d3d11::D3D11_BOX {
                            left: info.src_offset.x as _,
                            top: info.src_offset.y as _,
                            front: info.src_offset.z as _,
                            right: info.src_offset.x as u32 + info.extent.width as u32,
                            bottom: info.src_offset.y as u32 + info.extent.height as u32,
                            back: info.src_offset.z as u32 + info.extent.depth as u32,
                        },
                    );
                }
            }
        }
    }

    fn find_image_to_buffer_shader(
        &self,
        format: dxgiformat::DXGI_FORMAT,
    ) -> Option<(*mut d3d11::ID3D11ComputeShader, u32, u32)> {
        use dxgiformat::*;

        match format {
            DXGI_FORMAT_R32G32B32A32_UINT => {
                Some((self.cs_copy_image2d_r32g32b32a32_buffer.as_raw(), 1, 1))
            }
            DXGI_FORMAT_R32G32_UINT => Some((self.cs_copy_image2d_r32g32_buffer.as_raw(), 1, 1)),
            DXGI_FORMAT_R16G16B16A16_UINT => {
                Some((self.cs_copy_image2d_r16g16b16a16_buffer.as_raw(), 1, 1))
            }
            DXGI_FORMAT_R32_UINT => Some((self.cs_copy_image2d_r32_buffer.as_raw(), 1, 1)),
            DXGI_FORMAT_R16G16_UINT => Some((self.cs_copy_image2d_r16g16_buffer.as_raw(), 1, 1)),
            DXGI_FORMAT_R8G8B8A8_UINT => {
                Some((self.cs_copy_image2d_r8g8b8a8_buffer.as_raw(), 1, 1))
            }
            DXGI_FORMAT_R16_UINT => Some((self.cs_copy_image2d_r16_buffer.as_raw(), 2, 1)),
            DXGI_FORMAT_R8G8_UINT => Some((self.cs_copy_image2d_r8g8_buffer.as_raw(), 2, 1)),
            DXGI_FORMAT_R8_UINT => Some((self.cs_copy_image2d_r8_buffer.as_raw(), 4, 1)),
            DXGI_FORMAT_B8G8R8A8_UNORM => {
                Some((self.cs_copy_image2d_b8g8r8a8_buffer.as_raw(), 1, 1))
            }
            _ => None,
        }
    }

    pub fn copy_image_2d_to_buffer<T>(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Image,
        dst: &Buffer,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::BufferImageCopy>,
    {
        let _scope = debug_scope!(
            context,
            "Image (format={:?},kind={:?}) => Buffer",
            src.format,
            src.kind
        );
        let (shader, scale_x, scale_y) = self
            .find_image_to_buffer_shader(src.decomposed_format.copy_srv.unwrap())
            .unwrap();

        let srv = src.internal.copy_srv.clone().unwrap().as_raw();
        let uav = dst.internal.uav.unwrap();
        let format_desc = src.format.base_format().0.desc();
        let bytes_per_texel = format_desc.bits as u32 / 8;

        unsafe {
            context.CSSetShader(shader, ptr::null_mut(), 0);
            context.CSSetConstantBuffers(0, 1, &self.internal_buffer.as_raw());

            context.CSSetShaderResources(0, 1, [srv].as_ptr());
            context.CSSetUnorderedAccessViews(0, 1, [uav].as_ptr(), ptr::null_mut());

            for copy in regions {
                let copy = copy.borrow();
                self.update_buffer_image(context, &copy, src);

                debug_marker!(context, "{:?}", copy);

                context.Dispatch(
                    ((copy.image_extent.width + (COPY_THREAD_GROUP_X - 1))
                        / COPY_THREAD_GROUP_X
                        / scale_x)
                        .max(1),
                    ((copy.image_extent.height + (COPY_THREAD_GROUP_X - 1))
                        / COPY_THREAD_GROUP_Y
                        / scale_y)
                        .max(1),
                    1,
                );

                if let Some(disjoint_cb) = dst.internal.disjoint_cb {
                    let total_size = copy.image_extent.depth
                        * (copy.buffer_height * copy.buffer_width * bytes_per_texel);
                    let copy_box = d3d11::D3D11_BOX {
                        left: copy.buffer_offset as u32,
                        top: 0,
                        front: 0,
                        right: copy.buffer_offset as u32 + total_size,
                        bottom: 1,
                        back: 1,
                    };

                    context.CopySubresourceRegion(
                        disjoint_cb as _,
                        0,
                        copy.buffer_offset as _,
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

    fn find_buffer_to_image_shader(
        &self,
        format: dxgiformat::DXGI_FORMAT,
    ) -> Option<(*mut d3d11::ID3D11ComputeShader, u32, u32)> {
        use dxgiformat::*;

        match format {
            DXGI_FORMAT_R32G32B32A32_UINT => {
                Some((self.cs_copy_buffer_image2d_r32g32b32a32.as_raw(), 1, 1))
            }
            DXGI_FORMAT_R32G32_UINT => Some((self.cs_copy_buffer_image2d_r32g32.as_raw(), 1, 1)),
            DXGI_FORMAT_R16G16B16A16_UINT => {
                Some((self.cs_copy_buffer_image2d_r16g16b16a16.as_raw(), 1, 1))
            }
            DXGI_FORMAT_R32_UINT => Some((self.cs_copy_buffer_image2d_r32.as_raw(), 1, 1)),
            DXGI_FORMAT_R16G16_UINT => Some((self.cs_copy_buffer_image2d_r16g16.as_raw(), 1, 1)),
            DXGI_FORMAT_R8G8B8A8_UINT => {
                Some((self.cs_copy_buffer_image2d_r8g8b8a8.as_raw(), 1, 1))
            }
            DXGI_FORMAT_R16_UINT => Some((self.cs_copy_buffer_image2d_r16.as_raw(), 2, 1)),
            DXGI_FORMAT_R8G8_UINT => Some((self.cs_copy_buffer_image2d_r8g8.as_raw(), 2, 1)),
            DXGI_FORMAT_R8_UINT => Some((self.cs_copy_buffer_image2d_r8.as_raw(), 4, 1)),
            _ => None,
        }
    }

    pub fn copy_buffer_to_image_2d<T>(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Buffer,
        dst: &Image,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::BufferImageCopy>,
    {
        let _scope = debug_scope!(
            context,
            "Buffer => Image (format={:?},kind={:?})",
            dst.format,
            dst.kind
        );
        // NOTE: we have two separate paths for Buffer -> Image transfers. we need to special case
        //       uploads to compressed formats through `UpdateSubresource` since we cannot get a
        //       UAV of any compressed format.

        let format_desc = dst.format.base_format().0.desc();
        if format_desc.is_compressed() {
            // we dont really care about non-4x4 block formats..
            assert_eq!(format_desc.dim, (4, 4));
            assert!(!src.host_ptr.is_null());

            for copy in regions {
                let info = copy.borrow();

                let bytes_per_texel = format_desc.bits as u32 / 8;

                let row_pitch = bytes_per_texel * info.image_extent.width / 4;
                let depth_pitch = row_pitch * info.image_extent.height / 4;

                unsafe {
                    context.UpdateSubresource(
                        dst.internal.raw,
                        dst.calc_subresource(
                            info.image_layers.level as _,
                            info.image_layers.layers.start as _,
                        ),
                        &d3d11::D3D11_BOX {
                            left: info.image_offset.x as _,
                            top: info.image_offset.y as _,
                            front: info.image_offset.z as _,
                            right: info.image_offset.x as u32 + info.image_extent.width,
                            bottom: info.image_offset.y as u32 + info.image_extent.height,
                            back: info.image_offset.z as u32 + info.image_extent.depth,
                        },
                        src.host_ptr
                            .offset(src.bound_range.start as isize + info.buffer_offset as isize)
                            as _,
                        row_pitch,
                        depth_pitch,
                    );
                }
            }
        } else {
            let (shader, scale_x, scale_y) = self
                .find_buffer_to_image_shader(dst.decomposed_format.copy_uav.unwrap())
                .unwrap();

            let srv = src.internal.srv.unwrap();

            unsafe {
                context.CSSetShader(shader, ptr::null_mut(), 0);
                context.CSSetConstantBuffers(0, 1, &self.internal_buffer.as_raw());
                context.CSSetShaderResources(0, 1, [srv].as_ptr());

                for copy in regions {
                    let info = copy.borrow();
                    self.update_buffer_image(context, &info, dst);

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
                            / scale_x)
                            .max(1),
                        ((info.image_extent.height + (COPY_THREAD_GROUP_X - 1))
                            / COPY_THREAD_GROUP_Y
                            / scale_y)
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
        use format::ChannelType as Ct;

        match src.format.base_format().1 {
            Ct::Uint => Some(self.ps_blit_2d_uint.as_raw()),
            Ct::Sint => Some(self.ps_blit_2d_int.as_raw()),
            Ct::Unorm | Ct::Snorm | Ct::Sfloat | Ct::Srgb => Some(self.ps_blit_2d_float.as_raw()),
            Ct::Ufloat | Ct::Uscaled | Ct::Sscaled => None,
        }
    }

    pub fn blit_2d_image<T>(
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        src: &Image,
        dst: &Image,
        filter: image::Filter,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ImageBlit>,
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

        unsafe {
            context.IASetPrimitiveTopology(d3dcommon::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context.VSSetShader(self.vs_blit_2d.as_raw(), ptr::null_mut(), 0);
            context.VSSetConstantBuffers(0, 1, [self.internal_buffer.as_raw()].as_ptr());
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

            for region in regions {
                let region = region.borrow();
                self.update_blit(context, src, &region);

                // TODO: more layers
                let rtv = dst
                    .get_rtv(
                        region.dst_subresource.level,
                        region.dst_subresource.layers.start,
                    )
                    .unwrap()
                    .as_raw();

                context.RSSetViewports(
                    1,
                    [d3d11::D3D11_VIEWPORT {
                        TopLeftX: cmp::min(region.dst_bounds.start.x, region.dst_bounds.end.x) as _,
                        TopLeftY: cmp::min(region.dst_bounds.start.y, region.dst_bounds.end.y) as _,
                        Width: (region.dst_bounds.end.x - region.dst_bounds.start.x).abs() as _,
                        Height: (region.dst_bounds.end.y - region.dst_bounds.start.y).abs() as _,
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
        &mut self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        clears: T,
        rects: U,
        cache: &RenderPassCache,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::AttachmentClear>,
        U: IntoIterator,
        U::Item: Borrow<pso::ClearRect>,
    {
        use hal::format::ChannelType as Ct;
        let _scope = debug_scope!(context, "ClearAttachments");

        let clear_rects: SmallVec<[pso::ClearRect; 8]> = rects
            .into_iter()
            .map(|rect| rect.borrow().clone())
            .collect();

        unsafe {
            context.IASetPrimitiveTopology(d3dcommon::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context.IASetInputLayout(ptr::null_mut());
            context.VSSetShader(self.vs_partial_clear.as_raw(), ptr::null_mut(), 0);
            context.PSSetConstantBuffers(0, 1, [self.internal_buffer.as_raw()].as_ptr());
        }

        let subpass = &cache.render_pass.subpasses[cache.current_subpass];

        for clear in clears {
            let clear = clear.borrow();

            let _scope = debug_scope!(context, "{:?}", clear);

            match *clear {
                command::AttachmentClear::Color { index, value } => {
                    self.update_clear_color(context, value);

                    let attachment = {
                        let rtv_id = subpass.color_attachments[index];
                        &cache.framebuffer.attachments[rtv_id.0]
                    };

                    unsafe {
                        context.OMSetRenderTargets(
                            1,
                            [attachment.rtv_handle.clone().unwrap().as_raw()].as_ptr(),
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
                        let viewport = conv::map_viewport(&Viewport {
                            rect: clear_rect.rect,
                            depth: 0f32 .. 1f32,
                        });

                        debug_marker!(context, "{:?}", clear_rect.rect);

                        unsafe {
                            context.RSSetViewports(1, [viewport].as_ptr());
                            context.Draw(3, 0);
                        }
                    }
                }
                command::AttachmentClear::DepthStencil { depth, stencil } => {
                    self.update_clear_depth_stencil(context, depth, stencil);

                    let attachment = {
                        let dsv_id = subpass.depth_stencil_attachment.unwrap();
                        &cache.framebuffer.attachments[dsv_id.0]
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
                            attachment.dsv_handle.clone().unwrap().as_raw(),
                        );
                        context.PSSetShader(
                            self.ps_partial_clear_depth.as_raw(),
                            ptr::null_mut(),
                            0,
                        );
                    }

                    for clear_rect in &clear_rects {
                        let viewport = conv::map_viewport(&Viewport {
                            rect: clear_rect.rect,
                            depth: 0f32 .. 1f32,
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
