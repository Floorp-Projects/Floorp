use auxil::ShaderStage;
use hal::{
    adapter::MemoryProperties, buffer, device, format, image, memory, pass, pool, pso,
    pso::VertexInputRate, query, queue::QueueFamilyId, window,
};

use winapi::{
    shared::{dxgi, dxgiformat, dxgitype, minwindef::TRUE, windef::HWND, winerror},
    um::{d3d11, d3d11_1, d3d11sdklayers, d3dcommon},
};

use wio::com::ComPtr;

use std::{
    fmt, mem,
    ops::Range,
    ptr,
    sync::{Arc, Weak},
};

use parking_lot::{Condvar, Mutex, RwLock};

use crate::{
    conv,
    debug::{set_debug_name, set_debug_name_with_suffix, verify_debug_ascii},
    internal, shader, Backend, Buffer, BufferView, CommandBuffer, CommandPool, ComputePipeline,
    DescriptorContent, DescriptorIndex, DescriptorPool, DescriptorSet, DescriptorSetInfo,
    DescriptorSetLayout, Fence, Framebuffer, GraphicsPipeline, Image, ImageView, InternalBuffer,
    InternalImage, Memory, MultiStageData, PipelineLayout, QueryPool, RawFence,
    RegisterAccumulator, RegisterData, RenderPass, ResourceIndex, Sampler, Semaphore, ShaderModule,
    SubpassDesc, ViewInfo,
};

//TODO: expose coherent type 0x2 when it's properly supported
const BUFFER_TYPE_MASK: u32 = 0x1 | 0x4;

struct InputLayout {
    raw: ComPtr<d3d11::ID3D11InputLayout>,
    required_bindings: u32,
    max_vertex_bindings: u32,
    topology: d3d11::D3D11_PRIMITIVE_TOPOLOGY,
    vertex_strides: Vec<u32>,
}

#[derive(Clone)]
pub struct DepthStencilState {
    pub raw: ComPtr<d3d11::ID3D11DepthStencilState>,
    pub stencil_ref: pso::State<pso::StencilValue>,
    pub read_only: bool,
}

pub struct Device {
    raw: ComPtr<d3d11::ID3D11Device>,
    raw1: Option<ComPtr<d3d11_1::ID3D11Device1>>,
    pub(crate) context: ComPtr<d3d11::ID3D11DeviceContext>,
    features: hal::Features,
    memory_properties: MemoryProperties,
    render_doc: gfx_renderdoc::RenderDoc,
    pub(crate) internal: Arc<internal::Internal>,
}

impl fmt::Debug for Device {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Device")
    }
}

impl Drop for Device {
    fn drop(&mut self) {
        if let Ok(debug) = self.raw.cast::<d3d11sdklayers::ID3D11Debug>() {
            unsafe {
                debug.ReportLiveDeviceObjects(d3d11sdklayers::D3D11_RLDO_DETAIL);
            }
        }
    }
}

unsafe impl Send for Device {}
unsafe impl Sync for Device {}

impl Device {
    pub fn new(
        device: ComPtr<d3d11::ID3D11Device>,
        device1: Option<ComPtr<d3d11_1::ID3D11Device1>>,
        context: ComPtr<d3d11::ID3D11DeviceContext>,
        features: hal::Features,
        downlevel: hal::DownlevelProperties,
        memory_properties: MemoryProperties,
        feature_level: u32,
    ) -> Self {
        Device {
            internal: Arc::new(internal::Internal::new(
                &device,
                features,
                feature_level,
                downlevel,
            )),
            raw: device,
            raw1: device1,
            context,
            features,
            memory_properties,
            render_doc: Default::default(),
        }
    }

    pub fn as_raw(&self) -> *mut d3d11::ID3D11Device {
        self.raw.as_raw()
    }

    fn create_rasterizer_state(
        &self,
        rasterizer_desc: &pso::Rasterizer,
        multisampling_desc: &Option<pso::Multisampling>,
    ) -> Result<ComPtr<d3d11::ID3D11RasterizerState>, pso::CreationError> {
        let mut rasterizer = ptr::null_mut();
        let desc = conv::map_rasterizer_desc(rasterizer_desc, multisampling_desc);

        let hr = unsafe {
            self.raw
                .CreateRasterizerState(&desc, &mut rasterizer as *mut *mut _ as *mut *mut _)
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(rasterizer) })
        } else {
            Err(pso::CreationError::Other)
        }
    }

    fn create_blend_state(
        &self,
        blend_desc: &pso::BlendDesc,
        multisampling: &Option<pso::Multisampling>,
    ) -> Result<ComPtr<d3d11::ID3D11BlendState>, pso::CreationError> {
        let mut blend = ptr::null_mut();
        let desc = conv::map_blend_desc(blend_desc, multisampling);

        let hr = unsafe {
            self.raw
                .CreateBlendState(&desc, &mut blend as *mut *mut _ as *mut *mut _)
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(blend) })
        } else {
            Err(pso::CreationError::Other)
        }
    }

    fn create_depth_stencil_state(
        &self,
        depth_desc: &pso::DepthStencilDesc,
    ) -> Result<DepthStencilState, pso::CreationError> {
        let mut depth = ptr::null_mut();
        let (desc, stencil_ref, read_only) = conv::map_depth_stencil_desc(depth_desc);

        let hr = unsafe {
            self.raw
                .CreateDepthStencilState(&desc, &mut depth as *mut *mut _ as *mut *mut _)
        };

        if winerror::SUCCEEDED(hr) {
            Ok(DepthStencilState {
                raw: unsafe { ComPtr::from_raw(depth) },
                stencil_ref,
                read_only,
            })
        } else {
            Err(pso::CreationError::Other)
        }
    }

    fn create_input_layout(
        &self,
        vs: ComPtr<d3dcommon::ID3DBlob>,
        vertex_buffers: &[pso::VertexBufferDesc],
        attributes: &[pso::AttributeDesc],
        input_assembler: &pso::InputAssemblerDesc,
        vertex_semantic_remapping: auxil::FastHashMap<u32, Option<(u32, u32)>>,
    ) -> Result<InputLayout, pso::CreationError> {
        let mut layout = ptr::null_mut();

        let mut vertex_strides = Vec::new();
        let mut required_bindings = 0u32;
        let mut max_vertex_bindings = 0u32;
        for buffer in vertex_buffers {
            required_bindings |= 1 << buffer.binding as u32;
            max_vertex_bindings = max_vertex_bindings.max(1u32 + buffer.binding as u32);

            while vertex_strides.len() <= buffer.binding as usize {
                vertex_strides.push(0);
            }

            vertex_strides[buffer.binding as usize] = buffer.stride;
        }

        // See [`shader::introspect_spirv_vertex_semantic_remapping`] for details of why this is needed.
        let semantics: Vec<_> = attributes
            .iter()
            .map(
                |attrib| match vertex_semantic_remapping.get(&attrib.location) {
                    Some(Some((major, minor))) => {
                        let name = std::borrow::Cow::Owned(format!("TEXCOORD{}_\0", major));
                        let location = *minor;
                        (name, location)
                    }
                    _ => {
                        let name = std::borrow::Cow::Borrowed("TEXCOORD\0");
                        let location = attrib.location;
                        (name, location)
                    }
                },
            )
            .collect();

        let input_elements = attributes
            .iter()
            .zip(semantics.iter())
            .filter_map(|(attrib, (semantic_name, semantic_index))| {
                let buffer_desc = match vertex_buffers
                    .iter()
                    .find(|buffer_desc| buffer_desc.binding == attrib.binding)
                {
                    Some(buffer_desc) => buffer_desc,
                    None => {
                        // TODO:
                        // error!("Couldn't find associated vertex buffer description {:?}", attrib.binding);
                        return Some(Err(pso::CreationError::Other));
                    }
                };

                let (slot_class, step_rate) = match buffer_desc.rate {
                    VertexInputRate::Vertex => (d3d11::D3D11_INPUT_PER_VERTEX_DATA, 0),
                    VertexInputRate::Instance(divisor) => {
                        (d3d11::D3D11_INPUT_PER_INSTANCE_DATA, divisor)
                    }
                };
                let format = attrib.element.format;

                Some(Ok(d3d11::D3D11_INPUT_ELEMENT_DESC {
                    SemanticName: semantic_name.as_ptr() as *const _, // Semantic name used by SPIRV-Cross
                    SemanticIndex: *semantic_index,
                    Format: match conv::map_format(format) {
                        Some(fm) => fm,
                        None => {
                            // TODO:
                            // error!("Unable to find DXGI format for {:?}", format);
                            return Some(Err(pso::CreationError::Other));
                        }
                    },
                    InputSlot: attrib.binding as _,
                    AlignedByteOffset: attrib.element.offset,
                    InputSlotClass: slot_class,
                    InstanceDataStepRate: step_rate as _,
                }))
            })
            .collect::<Result<Vec<_>, _>>()?;

        let hr = unsafe {
            self.raw.CreateInputLayout(
                input_elements.as_ptr(),
                input_elements.len() as _,
                vs.GetBufferPointer(),
                vs.GetBufferSize(),
                &mut layout as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            let topology = conv::map_topology(input_assembler);

            Ok(InputLayout {
                raw: unsafe { ComPtr::from_raw(layout) },
                required_bindings,
                max_vertex_bindings,
                topology,
                vertex_strides,
            })
        } else {
            error!("CreateInputLayout error 0x{:X}", hr);
            Err(pso::CreationError::Other)
        }
    }

    fn create_vertex_shader(
        &self,
        blob: ComPtr<d3dcommon::ID3DBlob>,
    ) -> Result<ComPtr<d3d11::ID3D11VertexShader>, pso::CreationError> {
        let mut vs = ptr::null_mut();

        let hr = unsafe {
            self.raw.CreateVertexShader(
                blob.GetBufferPointer(),
                blob.GetBufferSize(),
                ptr::null_mut(),
                &mut vs as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(vs) })
        } else {
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::VERTEX,
                String::from("Failed to create a vertex shader"),
            ))
        }
    }

    fn create_pixel_shader(
        &self,
        blob: ComPtr<d3dcommon::ID3DBlob>,
    ) -> Result<ComPtr<d3d11::ID3D11PixelShader>, pso::CreationError> {
        let mut ps = ptr::null_mut();

        let hr = unsafe {
            self.raw.CreatePixelShader(
                blob.GetBufferPointer(),
                blob.GetBufferSize(),
                ptr::null_mut(),
                &mut ps as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(ps) })
        } else {
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::FRAGMENT,
                String::from("Failed to create a pixel shader"),
            ))
        }
    }

    fn create_geometry_shader(
        &self,
        blob: ComPtr<d3dcommon::ID3DBlob>,
    ) -> Result<ComPtr<d3d11::ID3D11GeometryShader>, pso::CreationError> {
        let mut gs = ptr::null_mut();

        let hr = unsafe {
            self.raw.CreateGeometryShader(
                blob.GetBufferPointer(),
                blob.GetBufferSize(),
                ptr::null_mut(),
                &mut gs as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(gs) })
        } else {
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::GEOMETRY,
                String::from("Failed to create a geometry shader"),
            ))
        }
    }

    fn create_hull_shader(
        &self,
        blob: ComPtr<d3dcommon::ID3DBlob>,
    ) -> Result<ComPtr<d3d11::ID3D11HullShader>, pso::CreationError> {
        let mut hs = ptr::null_mut();

        let hr = unsafe {
            self.raw.CreateHullShader(
                blob.GetBufferPointer(),
                blob.GetBufferSize(),
                ptr::null_mut(),
                &mut hs as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(hs) })
        } else {
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::HULL,
                String::from("Failed to create a hull shader"),
            ))
        }
    }

    fn create_domain_shader(
        &self,
        blob: ComPtr<d3dcommon::ID3DBlob>,
    ) -> Result<ComPtr<d3d11::ID3D11DomainShader>, pso::CreationError> {
        let mut ds = ptr::null_mut();

        let hr = unsafe {
            self.raw.CreateDomainShader(
                blob.GetBufferPointer(),
                blob.GetBufferSize(),
                ptr::null_mut(),
                &mut ds as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(ds) })
        } else {
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::DOMAIN,
                String::from("Failed to create a domain shader"),
            ))
        }
    }

    fn create_compute_shader(
        &self,
        blob: ComPtr<d3dcommon::ID3DBlob>,
    ) -> Result<ComPtr<d3d11::ID3D11ComputeShader>, pso::CreationError> {
        let mut cs = ptr::null_mut();

        let hr = unsafe {
            self.raw.CreateComputeShader(
                blob.GetBufferPointer(),
                blob.GetBufferSize(),
                ptr::null_mut(),
                &mut cs as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(cs) })
        } else {
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::COMPUTE,
                String::from("Failed to create a compute shader"),
            ))
        }
    }

    // TODO: fix return type..
    fn extract_entry_point(
        stage: ShaderStage,
        source: &pso::EntryPoint<Backend>,
        layout: &PipelineLayout,
        features: &hal::Features,
        device_feature_level: u32,
    ) -> Result<Option<ComPtr<d3dcommon::ID3DBlob>>, pso::CreationError> {
        // TODO: entrypoint stuff
        match *source.module {
            ShaderModule::Dxbc(ref _shader) => Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::ALL,
                String::from("DXBC modules are not supported yet"),
            )),
            ShaderModule::Spirv(ref raw_data) => Ok(shader::compile_spirv_entrypoint(
                raw_data,
                stage,
                source,
                layout,
                features,
                device_feature_level,
            )?),
        }
    }

    fn view_image_as_shader_resource(
        &self,
        info: &ViewInfo,
    ) -> Result<ComPtr<d3d11::ID3D11ShaderResourceView>, image::ViewCreationError> {
        let mut desc: d3d11::D3D11_SHADER_RESOURCE_VIEW_DESC = unsafe { mem::zeroed() };
        desc.Format = info.format;
        if desc.Format == dxgiformat::DXGI_FORMAT_D32_FLOAT_S8X24_UINT {
            desc.Format = dxgiformat::DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        }

        #[allow(non_snake_case)]
        let MostDetailedMip = info.levels.start as _;
        #[allow(non_snake_case)]
        let MipLevels = (info.levels.end - info.levels.start) as _;
        #[allow(non_snake_case)]
        let FirstArraySlice = info.layers.start as _;
        #[allow(non_snake_case)]
        let ArraySize = (info.layers.end - info.layers.start) as _;

        match info.view_kind {
            image::ViewKind::D1 => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURE1D;
                *unsafe { desc.u.Texture1D_mut() } = d3d11::D3D11_TEX1D_SRV {
                    MostDetailedMip,
                    MipLevels,
                }
            }
            image::ViewKind::D1Array => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                *unsafe { desc.u.Texture1DArray_mut() } = d3d11::D3D11_TEX1D_ARRAY_SRV {
                    MostDetailedMip,
                    MipLevels,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2 if info.kind.num_samples() > 1 => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURE2DMS;
                *unsafe { desc.u.Texture2DMS_mut() } = d3d11::D3D11_TEX2DMS_SRV {
                    UnusedField_NothingToDefine: 0,
                }
            }
            image::ViewKind::D2 => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURE2D;
                *unsafe { desc.u.Texture2D_mut() } = d3d11::D3D11_TEX2D_SRV {
                    MostDetailedMip,
                    MipLevels,
                }
            }
            image::ViewKind::D2Array if info.kind.num_samples() > 1 => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
                *unsafe { desc.u.Texture2DMSArray_mut() } = d3d11::D3D11_TEX2DMS_ARRAY_SRV {
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2Array => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                *unsafe { desc.u.Texture2DArray_mut() } = d3d11::D3D11_TEX2D_ARRAY_SRV {
                    MostDetailedMip,
                    MipLevels,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D3 => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURE3D;
                *unsafe { desc.u.Texture3D_mut() } = d3d11::D3D11_TEX3D_SRV {
                    MostDetailedMip,
                    MipLevels,
                }
            }
            image::ViewKind::Cube => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURECUBE;
                *unsafe { desc.u.TextureCube_mut() } = d3d11::D3D11_TEXCUBE_SRV {
                    MostDetailedMip,
                    MipLevels,
                }
            }
            image::ViewKind::CubeArray => {
                desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                *unsafe { desc.u.TextureCubeArray_mut() } = d3d11::D3D11_TEXCUBE_ARRAY_SRV {
                    MostDetailedMip,
                    MipLevels,
                    First2DArrayFace: FirstArraySlice,
                    NumCubes: ArraySize / 6,
                }
            }
        }

        let mut srv = ptr::null_mut();
        let hr = unsafe {
            self.raw.CreateShaderResourceView(
                info.resource,
                &desc,
                &mut srv as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(srv) })
        } else {
            Err(image::ViewCreationError::Unsupported)
        }
    }

    fn view_image_as_unordered_access(
        &self,
        info: &ViewInfo,
    ) -> Result<ComPtr<d3d11::ID3D11UnorderedAccessView>, image::ViewCreationError> {
        let mut desc: d3d11::D3D11_UNORDERED_ACCESS_VIEW_DESC = unsafe { mem::zeroed() };
        desc.Format = info.format;

        #[allow(non_snake_case)]
        let MipSlice = info.levels.start as _;
        #[allow(non_snake_case)]
        let FirstArraySlice = info.layers.start as _;
        #[allow(non_snake_case)]
        let ArraySize = (info.layers.end - info.layers.start) as _;

        match info.view_kind {
            image::ViewKind::D1 => {
                desc.ViewDimension = d3d11::D3D11_UAV_DIMENSION_TEXTURE1D;
                *unsafe { desc.u.Texture1D_mut() } = d3d11::D3D11_TEX1D_UAV {
                    MipSlice: info.levels.start as _,
                }
            }
            image::ViewKind::D1Array => {
                desc.ViewDimension = d3d11::D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
                *unsafe { desc.u.Texture1DArray_mut() } = d3d11::D3D11_TEX1D_ARRAY_UAV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2 => {
                desc.ViewDimension = d3d11::D3D11_UAV_DIMENSION_TEXTURE2D;
                *unsafe { desc.u.Texture2D_mut() } = d3d11::D3D11_TEX2D_UAV {
                    MipSlice: info.levels.start as _,
                }
            }
            image::ViewKind::D2Array => {
                desc.ViewDimension = d3d11::D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                *unsafe { desc.u.Texture2DArray_mut() } = d3d11::D3D11_TEX2D_ARRAY_UAV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D3 => {
                desc.ViewDimension = d3d11::D3D11_UAV_DIMENSION_TEXTURE3D;
                *unsafe { desc.u.Texture3D_mut() } = d3d11::D3D11_TEX3D_UAV {
                    MipSlice,
                    FirstWSlice: FirstArraySlice,
                    WSize: ArraySize,
                }
            }
            _ => unimplemented!(),
        }

        let mut uav = ptr::null_mut();
        let hr = unsafe {
            self.raw.CreateUnorderedAccessView(
                info.resource,
                &desc,
                &mut uav as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(uav) })
        } else {
            error!("CreateUnorderedAccessView failed: 0x{:x}", hr);

            Err(image::ViewCreationError::Unsupported)
        }
    }

    pub(crate) fn view_image_as_render_target(
        &self,
        info: &ViewInfo,
    ) -> Result<ComPtr<d3d11::ID3D11RenderTargetView>, image::ViewCreationError> {
        let mut desc: d3d11::D3D11_RENDER_TARGET_VIEW_DESC = unsafe { mem::zeroed() };
        desc.Format = info.format;

        #[allow(non_snake_case)]
        let MipSlice = info.levels.start as _;
        #[allow(non_snake_case)]
        let FirstArraySlice = info.layers.start as _;
        #[allow(non_snake_case)]
        let ArraySize = (info.layers.end - info.layers.start) as _;

        match info.view_kind {
            image::ViewKind::D1 => {
                desc.ViewDimension = d3d11::D3D11_RTV_DIMENSION_TEXTURE1D;
                *unsafe { desc.u.Texture1D_mut() } = d3d11::D3D11_TEX1D_RTV { MipSlice }
            }
            image::ViewKind::D1Array => {
                desc.ViewDimension = d3d11::D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
                *unsafe { desc.u.Texture1DArray_mut() } = d3d11::D3D11_TEX1D_ARRAY_RTV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2 => {
                if info.kind.num_samples() > 1 {
                    desc.ViewDimension = d3d11::D3D11_RTV_DIMENSION_TEXTURE2DMS;
                    *unsafe { desc.u.Texture2DMS_mut() } = d3d11::D3D11_TEX2DMS_RTV {
                        UnusedField_NothingToDefine: 0,
                    }
                } else {
                    desc.ViewDimension = d3d11::D3D11_RTV_DIMENSION_TEXTURE2D;
                    *unsafe { desc.u.Texture2D_mut() } = d3d11::D3D11_TEX2D_RTV { MipSlice }
                }
            }
            image::ViewKind::D2Array => {
                if info.kind.num_samples() > 1 {
                    desc.ViewDimension = d3d11::D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    *unsafe { desc.u.Texture2DMSArray_mut() } = d3d11::D3D11_TEX2DMS_ARRAY_RTV {
                        FirstArraySlice,
                        ArraySize,
                    }
                } else {
                    desc.ViewDimension = d3d11::D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    *unsafe { desc.u.Texture2DArray_mut() } = d3d11::D3D11_TEX2D_ARRAY_RTV {
                        MipSlice,
                        FirstArraySlice,
                        ArraySize,
                    }
                }
            }
            image::ViewKind::D3 => {
                desc.ViewDimension = d3d11::D3D11_RTV_DIMENSION_TEXTURE3D;
                *unsafe { desc.u.Texture3D_mut() } = d3d11::D3D11_TEX3D_RTV {
                    MipSlice,
                    FirstWSlice: FirstArraySlice,
                    WSize: ArraySize,
                }
            }
            _ => unimplemented!(),
        }

        let mut rtv = ptr::null_mut();
        let hr = unsafe {
            self.raw.CreateRenderTargetView(
                info.resource,
                &desc,
                &mut rtv as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(rtv) })
        } else {
            error!("CreateRenderTargetView failed: 0x{:x}", hr);

            Err(image::ViewCreationError::Unsupported)
        }
    }

    fn view_image_as_depth_stencil(
        &self,
        info: &ViewInfo,
        read_only_stencil: Option<bool>,
    ) -> Result<ComPtr<d3d11::ID3D11DepthStencilView>, image::ViewCreationError> {
        #![allow(non_snake_case)]

        let MipSlice = info.levels.start as _;
        let FirstArraySlice = info.layers.start as _;
        let ArraySize = (info.layers.end - info.layers.start) as _;
        assert_eq!(info.levels.start + 1, info.levels.end);
        assert!(info.layers.end <= info.kind.num_layers());

        let mut desc: d3d11::D3D11_DEPTH_STENCIL_VIEW_DESC = unsafe { mem::zeroed() };
        desc.Format = info.format;

        if let Some(stencil) = read_only_stencil {
            desc.Flags = match stencil {
                true => d3d11::D3D11_DSV_READ_ONLY_DEPTH | d3d11::D3D11_DSV_READ_ONLY_STENCIL,
                false => d3d11::D3D11_DSV_READ_ONLY_DEPTH,
            }
        }

        match info.view_kind {
            image::ViewKind::D2 => {
                if info.kind.num_samples() > 1 {
                    desc.ViewDimension = d3d11::D3D11_DSV_DIMENSION_TEXTURE2DMS;
                    *unsafe { desc.u.Texture2DMS_mut() } = d3d11::D3D11_TEX2DMS_DSV {
                        UnusedField_NothingToDefine: 0,
                    }
                } else {
                    desc.ViewDimension = d3d11::D3D11_DSV_DIMENSION_TEXTURE2D;
                    *unsafe { desc.u.Texture2D_mut() } = d3d11::D3D11_TEX2D_DSV { MipSlice }
                }
            }
            image::ViewKind::D2Array => {
                if info.kind.num_samples() > 1 {
                    desc.ViewDimension = d3d11::D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    *unsafe { desc.u.Texture2DMSArray_mut() } = d3d11::D3D11_TEX2DMS_ARRAY_DSV {
                        FirstArraySlice,
                        ArraySize,
                    }
                } else {
                    desc.ViewDimension = d3d11::D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                    *unsafe { desc.u.Texture2DArray_mut() } = d3d11::D3D11_TEX2D_ARRAY_DSV {
                        MipSlice,
                        FirstArraySlice,
                        ArraySize,
                    }
                }
            }
            image::ViewKind::D1
            | image::ViewKind::D1Array
            | image::ViewKind::D3
            | image::ViewKind::Cube
            | image::ViewKind::CubeArray => {
                warn!(
                    "3D and cube views are not supported for the image, kind: {:?}",
                    info.kind
                );
                return Err(image::ViewCreationError::BadKind(info.view_kind));
            }
        }

        let mut dsv = ptr::null_mut();
        let hr = unsafe {
            self.raw.CreateDepthStencilView(
                info.resource,
                &desc,
                &mut dsv as *mut *mut _ as *mut *mut _,
            )
        };

        if winerror::SUCCEEDED(hr) {
            Ok(unsafe { ComPtr::from_raw(dsv) })
        } else {
            error!("CreateDepthStencilView failed: 0x{:x}", hr);

            Err(image::ViewCreationError::Unsupported)
        }
    }

    pub(crate) fn create_swapchain_impl(
        &self,
        config: &window::SwapchainConfig,
        window_handle: HWND,
        factory: ComPtr<dxgi::IDXGIFactory>,
    ) -> Result<(ComPtr<dxgi::IDXGISwapChain>, dxgiformat::DXGI_FORMAT), window::SwapchainError>
    {
        // TODO: use IDXGIFactory2 for >=11.1
        // TODO: this function should be able to fail (Result)?

        debug!("{:#?}", config);
        let non_srgb_format = conv::map_format_nosrgb(config.format).unwrap();

        let mut desc = dxgi::DXGI_SWAP_CHAIN_DESC {
            BufferDesc: dxgitype::DXGI_MODE_DESC {
                Width: config.extent.width,
                Height: config.extent.height,
                // TODO: should this grab max value of all monitor hz? vsync
                //       will clamp to current monitor anyways?
                RefreshRate: dxgitype::DXGI_RATIONAL {
                    Numerator: 1,
                    Denominator: 60,
                },
                Format: non_srgb_format,
                ScanlineOrdering: dxgitype::DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                Scaling: dxgitype::DXGI_MODE_SCALING_UNSPECIFIED,
            },
            SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            BufferUsage: dxgitype::DXGI_USAGE_RENDER_TARGET_OUTPUT,
            BufferCount: config.image_count,
            OutputWindow: window_handle,
            // TODO:
            Windowed: TRUE,
            // TODO:
            SwapEffect: dxgi::DXGI_SWAP_EFFECT_DISCARD,
            Flags: 0,
        };

        let dxgi_swapchain = {
            let mut swapchain: *mut dxgi::IDXGISwapChain = ptr::null_mut();
            let hr = unsafe {
                factory.CreateSwapChain(
                    self.raw.as_raw() as *mut _,
                    &mut desc as *mut _,
                    &mut swapchain as *mut *mut _ as *mut *mut _,
                )
            };
            assert_eq!(hr, winerror::S_OK);

            unsafe { ComPtr::from_raw(swapchain) }
        };
        Ok((dxgi_swapchain, non_srgb_format))
    }
}

impl device::Device<Backend> for Device {
    unsafe fn allocate_memory(
        &self,
        mem_type: hal::MemoryTypeId,
        size: u64,
    ) -> Result<Memory, device::AllocationError> {
        let properties = self.memory_properties.memory_types[mem_type.0].properties;
        let host_ptr = if properties.contains(hal::memory::Properties::CPU_VISIBLE) {
            let mut data = vec![0u8; size as usize];
            let ptr = data.as_mut_ptr();
            mem::forget(data);
            ptr
        } else {
            ptr::null_mut()
        };
        Ok(Memory {
            properties,
            size,
            host_ptr,
            local_buffers: Arc::new(RwLock::new(thunderdome::Arena::new())),
        })
    }

    unsafe fn create_command_pool(
        &self,
        _family: QueueFamilyId,
        _create_flags: pool::CommandPoolCreateFlags,
    ) -> Result<CommandPool, device::OutOfMemory> {
        // TODO:
        Ok(CommandPool {
            device: self.raw.clone(),
            device1: self.raw1.clone(),
            internal: Arc::clone(&self.internal),
        })
    }

    unsafe fn destroy_command_pool(&self, _pool: CommandPool) {
        // automatic
    }

    unsafe fn create_render_pass<'a, Ia, Is, Id>(
        &self,
        attachments: Ia,
        subpasses: Is,
        _dependencies: Id,
    ) -> Result<RenderPass, device::OutOfMemory>
    where
        Ia: Iterator<Item = pass::Attachment>,
        Is: Iterator<Item = pass::SubpassDesc<'a>>,
    {
        Ok(RenderPass {
            attachments: attachments.collect(),
            subpasses: subpasses
                .map(|desc| SubpassDesc {
                    color_attachments: desc.colors.to_vec(),
                    depth_stencil_attachment: desc.depth_stencil.cloned(),
                    input_attachments: desc.inputs.to_vec(),
                    resolve_attachments: desc.resolves.to_vec(),
                })
                .collect(),
        })
    }

    unsafe fn create_pipeline_layout<'a, Is, Ic>(
        &self,
        set_layouts: Is,
        _push_constant_ranges: Ic,
    ) -> Result<PipelineLayout, device::OutOfMemory>
    where
        Is: Iterator<Item = &'a DescriptorSetLayout>,
        Ic: Iterator<Item = (pso::ShaderStageFlags, Range<u32>)>,
    {
        let mut res_offsets = MultiStageData::<RegisterData<RegisterAccumulator>>::default();
        let mut sets = Vec::new();
        for set_layout in set_layouts {
            sets.push(DescriptorSetInfo {
                bindings: Arc::clone(&set_layout.bindings),
                registers: res_offsets.advance(&set_layout.pool_mapping),
            });
        }

        res_offsets.map_other(|data| {
            // These use <= because this tells us the _next_ register, so maximum usage will be equal to the limit.
            //
            // Leave one slot for push constants
            assert!(
                data.c.res_index as u32
                    <= d3d11::D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1,
                "{} bound constant buffers exceeds limit of {}",
                data.c.res_index as u32,
                d3d11::D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1,
            );
            assert!(
                data.s.res_index as u32 <= d3d11::D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT,
                "{} bound samplers exceeds limit of {}",
                data.s.res_index as u32,
                d3d11::D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT,
            );
            assert!(
                data.t.res_index as u32 <= d3d11::D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT,
                "{} bound sampled textures and read-only buffers exceeds limit of {}",
                data.t.res_index as u32,
                d3d11::D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT,
            );
            assert!(
                data.u.res_index as u32 <= d3d11::D3D11_PS_CS_UAV_REGISTER_COUNT,
                "{} bound storage textures and read-write buffers exceeds limit of {}",
                data.u.res_index as u32,
                d3d11::D3D11_PS_CS_UAV_REGISTER_COUNT,
            );
        });

        Ok(PipelineLayout { sets })
    }

    unsafe fn create_pipeline_cache(
        &self,
        _data: Option<&[u8]>,
    ) -> Result<(), device::OutOfMemory> {
        Ok(())
    }

    unsafe fn get_pipeline_cache_data(&self, _cache: &()) -> Result<Vec<u8>, device::OutOfMemory> {
        //empty
        Ok(Vec::new())
    }

    unsafe fn destroy_pipeline_cache(&self, _: ()) {
        //empty
    }

    unsafe fn merge_pipeline_caches<'a, I>(
        &self,
        _: &mut (),
        _: I,
    ) -> Result<(), device::OutOfMemory>
    where
        I: Iterator<Item = &'a ()>,
    {
        //empty
        Ok(())
    }

    unsafe fn create_graphics_pipeline<'a>(
        &self,
        desc: &pso::GraphicsPipelineDesc<'a, Backend>,
        _cache: Option<&()>,
    ) -> Result<GraphicsPipeline, pso::CreationError> {
        let features = &self.features;
        let build_shader =
            |stage: ShaderStage, source: Option<&pso::EntryPoint<'a, Backend>>| match source {
                Some(src) => Self::extract_entry_point(
                    stage,
                    src,
                    desc.layout,
                    features,
                    self.internal.device_feature_level,
                ),
                None => Ok(None),
            };

        let (layout, vs, gs, hs, ds) = match desc.primitive_assembler {
            pso::PrimitiveAssemblerDesc::Vertex {
                buffers,
                attributes,
                ref input_assembler,
                ref vertex,
                ref tessellation,
                ref geometry,
            } => {
                let vertex_semantic_remapping = match vertex.module {
                    ShaderModule::Spirv(spirv) => {
                        shader::introspect_spirv_vertex_semantic_remapping(spirv)?
                    }
                    _ => unimplemented!(),
                };

                let vs = build_shader(ShaderStage::Vertex, Some(&vertex))?.unwrap();
                let gs = build_shader(ShaderStage::Geometry, geometry.as_ref())?;

                let layout = self.create_input_layout(
                    vs.clone(),
                    buffers,
                    attributes,
                    input_assembler,
                    vertex_semantic_remapping,
                )?;

                let vs = self.create_vertex_shader(vs)?;
                let gs = if let Some(blob) = gs {
                    Some(self.create_geometry_shader(blob)?)
                } else {
                    None
                };
                let (hs, ds) = if let Some(ts) = tessellation {
                    let hs = build_shader(ShaderStage::Hull, Some(&ts.0))?.unwrap();
                    let ds = build_shader(ShaderStage::Domain, Some(&ts.1))?.unwrap();

                    (
                        Some(self.create_hull_shader(hs)?),
                        Some(self.create_domain_shader(ds)?),
                    )
                } else {
                    (None, None)
                };

                (layout, vs, gs, hs, ds)
            }
            pso::PrimitiveAssemblerDesc::Mesh { .. } => {
                return Err(pso::CreationError::UnsupportedPipeline)
            }
        };

        let ps = build_shader(ShaderStage::Fragment, desc.fragment.as_ref())?;
        let ps = if let Some(blob) = ps {
            Some(self.create_pixel_shader(blob)?)
        } else {
            None
        };

        let rasterizer_state =
            self.create_rasterizer_state(&desc.rasterizer, &desc.multisampling)?;
        let blend_state = self.create_blend_state(&desc.blender, &desc.multisampling)?;
        let depth_stencil_state = Some(self.create_depth_stencil_state(&desc.depth_stencil)?);

        match desc.label {
            Some(label) if verify_debug_ascii(label) => {
                let mut name = label.to_string();

                set_debug_name_with_suffix(&blend_state, &mut name, " -- Blend State");
                set_debug_name_with_suffix(&rasterizer_state, &mut name, " -- Rasterizer State");
                set_debug_name_with_suffix(&layout.raw, &mut name, " -- Input Layout");
                if let Some(ref dss) = depth_stencil_state {
                    set_debug_name_with_suffix(&dss.raw, &mut name, " -- Depth Stencil State");
                }
            }
            _ => {}
        }

        Ok(GraphicsPipeline {
            vs,
            gs,
            ds,
            hs,
            ps,
            topology: layout.topology,
            input_layout: layout.raw,
            rasterizer_state,
            blend_state,
            depth_stencil_state,
            baked_states: desc.baked_states.clone(),
            required_bindings: layout.required_bindings,
            max_vertex_bindings: layout.max_vertex_bindings,
            strides: layout.vertex_strides,
        })
    }

    unsafe fn create_compute_pipeline<'a>(
        &self,
        desc: &pso::ComputePipelineDesc<'a, Backend>,
        _cache: Option<&()>,
    ) -> Result<ComputePipeline, pso::CreationError> {
        let features = &self.features;
        let build_shader =
            |stage: ShaderStage, source: Option<&pso::EntryPoint<'a, Backend>>| match source {
                Some(src) => Self::extract_entry_point(
                    stage,
                    src,
                    desc.layout,
                    features,
                    self.internal.device_feature_level,
                ),
                None => Ok(None),
            };

        let cs = build_shader(ShaderStage::Compute, Some(&desc.shader))?.unwrap();
        let cs = self.create_compute_shader(cs)?;

        Ok(ComputePipeline { cs })
    }

    unsafe fn create_framebuffer<I>(
        &self,
        _renderpass: &RenderPass,
        _attachments: I,
        extent: image::Extent,
    ) -> Result<Framebuffer, device::OutOfMemory> {
        Ok(Framebuffer {
            layers: extent.depth as _,
        })
    }

    unsafe fn create_shader_module(
        &self,
        raw_data: &[u32],
    ) -> Result<ShaderModule, device::ShaderError> {
        Ok(ShaderModule::Spirv(raw_data.into()))
    }

    unsafe fn create_buffer(
        &self,
        size: u64,
        usage: buffer::Usage,
        _sparse: memory::SparseFlags,
    ) -> Result<Buffer, buffer::CreationError> {
        use buffer::Usage;

        let mut bind = 0;

        if usage.contains(Usage::UNIFORM) {
            bind |= d3d11::D3D11_BIND_CONSTANT_BUFFER;
        }
        if usage.contains(Usage::VERTEX) {
            bind |= d3d11::D3D11_BIND_VERTEX_BUFFER;
        }
        if usage.contains(Usage::INDEX) {
            bind |= d3d11::D3D11_BIND_INDEX_BUFFER;
        }

        // TODO: >=11.1
        if usage.intersects(
            Usage::UNIFORM_TEXEL | Usage::STORAGE_TEXEL | Usage::TRANSFER_SRC | Usage::STORAGE,
        ) {
            bind |= d3d11::D3D11_BIND_SHADER_RESOURCE;
        }

        if usage.intersects(Usage::TRANSFER_DST | Usage::STORAGE) {
            bind |= d3d11::D3D11_BIND_UNORDERED_ACCESS;
        }

        // if `D3D11_BIND_CONSTANT_BUFFER` intersects with any other bind flag, we need to handle
        // it by creating two buffers. one with `D3D11_BIND_CONSTANT_BUFFER` and one with the rest
        let needs_disjoint_cb = bind & d3d11::D3D11_BIND_CONSTANT_BUFFER != 0
            && bind != d3d11::D3D11_BIND_CONSTANT_BUFFER;

        if needs_disjoint_cb {
            bind ^= d3d11::D3D11_BIND_CONSTANT_BUFFER;
        }

        fn up_align(x: u64, alignment: u64) -> u64 {
            (x + alignment - 1) & !(alignment - 1)
        }

        // constant buffer size need to be divisible by 16
        let size = if usage.contains(Usage::UNIFORM) {
            up_align(size, 16)
        } else {
            up_align(size, 4)
        };

        Ok(Buffer {
            internal: InternalBuffer {
                raw: ptr::null_mut(),
                disjoint_cb: if needs_disjoint_cb {
                    Some(ptr::null_mut())
                } else {
                    None
                },
                srv: None,
                uav: None,
                usage,
                debug_name: None,
            },
            bound_range: 0..0,
            local_memory_arena: Weak::new(),
            memory_index: None,
            is_coherent: false,
            memory_ptr: ptr::null_mut(),
            bind,
            requirements: memory::Requirements {
                size,
                alignment: 4,
                type_mask: BUFFER_TYPE_MASK,
            },
        })
    }

    unsafe fn get_buffer_requirements(&self, buffer: &Buffer) -> memory::Requirements {
        buffer.requirements
    }

    unsafe fn bind_buffer_memory(
        &self,
        memory: &Memory,
        offset: u64,
        buffer: &mut Buffer,
    ) -> Result<(), device::BindError> {
        debug!(
            "usage={:?}, props={:b}",
            buffer.internal.usage, memory.properties
        );

        #[allow(non_snake_case)]
        let mut MiscFlags = if buffer.bind
            & (d3d11::D3D11_BIND_SHADER_RESOURCE | d3d11::D3D11_BIND_UNORDERED_ACCESS)
            != 0
        {
            d3d11::D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS
        } else {
            0
        };

        if buffer.internal.usage.contains(buffer::Usage::INDIRECT) {
            MiscFlags |= d3d11::D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        }

        let initial_data = if memory.host_ptr.is_null() {
            None
        } else {
            Some(d3d11::D3D11_SUBRESOURCE_DATA {
                pSysMem: memory.host_ptr.offset(offset as isize) as *const _,
                SysMemPitch: 0,
                SysMemSlicePitch: 0,
            })
        };

        //TODO: check `memory.properties.contains(memory::Properties::DEVICE_LOCAL)` ?
        let raw = {
            // device local memory
            let desc = d3d11::D3D11_BUFFER_DESC {
                ByteWidth: buffer.requirements.size as _,
                Usage: d3d11::D3D11_USAGE_DEFAULT,
                BindFlags: buffer.bind,
                CPUAccessFlags: 0,
                MiscFlags,
                StructureByteStride: if buffer.internal.usage.contains(buffer::Usage::TRANSFER_SRC)
                {
                    4
                } else {
                    0
                },
            };

            let mut raw: *mut d3d11::ID3D11Buffer = ptr::null_mut();
            let hr = self.raw.CreateBuffer(
                &desc,
                initial_data.as_ref().map_or(ptr::null_mut(), |id| id),
                &mut raw as *mut *mut _ as *mut *mut _,
            );

            if !winerror::SUCCEEDED(hr) {
                return Err(device::BindError::WrongMemory);
            }

            if let Some(ref mut name) = buffer.internal.debug_name {
                set_debug_name(&*raw, name);
            }

            ComPtr::from_raw(raw)
        };

        let disjoint_cb = if buffer.internal.disjoint_cb.is_some() {
            let desc = d3d11::D3D11_BUFFER_DESC {
                ByteWidth: buffer.requirements.size as _,
                Usage: d3d11::D3D11_USAGE_DEFAULT,
                BindFlags: d3d11::D3D11_BIND_CONSTANT_BUFFER,
                CPUAccessFlags: 0,
                MiscFlags: 0,
                StructureByteStride: 0,
            };

            let mut disjoint_raw: *mut d3d11::ID3D11Buffer = ptr::null_mut();
            let hr = self.raw.CreateBuffer(
                &desc,
                initial_data.as_ref().map_or(ptr::null_mut(), |id| id),
                &mut disjoint_raw as *mut *mut _ as *mut *mut _,
            );

            if !winerror::SUCCEEDED(hr) {
                return Err(device::BindError::WrongMemory);
            }

            if let Some(ref mut name) = buffer.internal.debug_name {
                set_debug_name_with_suffix(&*disjoint_raw, name, " -- Constant Buffer");
            }

            Some(disjoint_raw)
        } else {
            None
        };

        let srv = if buffer.bind & d3d11::D3D11_BIND_SHADER_RESOURCE != 0 {
            let mut desc = mem::zeroed::<d3d11::D3D11_SHADER_RESOURCE_VIEW_DESC>();
            desc.Format = dxgiformat::DXGI_FORMAT_R32_TYPELESS;
            desc.ViewDimension = d3dcommon::D3D11_SRV_DIMENSION_BUFFEREX;
            *desc.u.BufferEx_mut() = d3d11::D3D11_BUFFEREX_SRV {
                FirstElement: 0,
                NumElements: buffer.requirements.size as u32 / 4,
                Flags: d3d11::D3D11_BUFFEREX_SRV_FLAG_RAW,
            };

            let mut srv: *mut d3d11::ID3D11ShaderResourceView = ptr::null_mut();
            let hr = self.raw.CreateShaderResourceView(
                raw.as_raw() as *mut _,
                &desc,
                &mut srv as *mut *mut _ as *mut *mut _,
            );

            if !winerror::SUCCEEDED(hr) {
                error!("CreateShaderResourceView failed: 0x{:x}", hr);

                return Err(device::BindError::WrongMemory);
            }

            if let Some(ref mut name) = buffer.internal.debug_name {
                set_debug_name_with_suffix(&*srv, name, " -- SRV");
            }

            Some(srv)
        } else {
            None
        };

        let uav = if buffer.bind & d3d11::D3D11_BIND_UNORDERED_ACCESS != 0 {
            let mut desc = mem::zeroed::<d3d11::D3D11_UNORDERED_ACCESS_VIEW_DESC>();
            desc.Format = dxgiformat::DXGI_FORMAT_R32_TYPELESS;
            desc.ViewDimension = d3d11::D3D11_UAV_DIMENSION_BUFFER;
            *desc.u.Buffer_mut() = d3d11::D3D11_BUFFER_UAV {
                FirstElement: 0,
                NumElements: buffer.requirements.size as u32 / 4,
                Flags: d3d11::D3D11_BUFFER_UAV_FLAG_RAW,
            };

            let mut uav: *mut d3d11::ID3D11UnorderedAccessView = ptr::null_mut();
            let hr = self.raw.CreateUnorderedAccessView(
                raw.as_raw() as *mut _,
                &desc,
                &mut uav as *mut *mut _ as *mut *mut _,
            );

            if !winerror::SUCCEEDED(hr) {
                error!("CreateUnorderedAccessView failed: 0x{:x}", hr);

                return Err(device::BindError::WrongMemory);
            }

            if let Some(ref mut name) = buffer.internal.debug_name {
                set_debug_name_with_suffix(&*uav, name, " -- UAV");
            }

            Some(uav)
        } else {
            None
        };

        let internal = InternalBuffer {
            raw: raw.into_raw(),
            disjoint_cb,
            srv,
            uav,
            usage: buffer.internal.usage,
            debug_name: buffer.internal.debug_name.take(),
        };
        let range = offset..offset + buffer.requirements.size;

        let memory_index = memory.bind_buffer(range.clone(), internal.clone());

        buffer.internal = internal;
        buffer.is_coherent = memory
            .properties
            .contains(hal::memory::Properties::COHERENT);
        buffer.memory_ptr = memory.host_ptr;
        buffer.bound_range = range;
        buffer.local_memory_arena = Arc::downgrade(&memory.local_buffers);
        buffer.memory_index = Some(memory_index);

        Ok(())
    }

    unsafe fn create_buffer_view(
        &self,
        _buffer: &Buffer,
        _format: Option<format::Format>,
        _range: buffer::SubRange,
    ) -> Result<BufferView, buffer::ViewCreationError> {
        unimplemented!()
    }

    unsafe fn create_image(
        &self,
        kind: image::Kind,
        mip_levels: image::Level,
        format: format::Format,
        _tiling: image::Tiling,
        usage: image::Usage,
        _sparse: memory::SparseFlags,
        view_caps: image::ViewCapabilities,
    ) -> Result<Image, image::CreationError> {
        let surface_desc = format.base_format().0.desc();
        let bytes_per_texel = surface_desc.bits / 8;
        let ext = kind.extent();
        let size = (ext.width * ext.height * ext.depth) as u64 * bytes_per_texel as u64;

        let bind = conv::map_image_usage(usage, surface_desc, self.internal.device_feature_level);
        debug!("{:b}", bind);

        Ok(Image {
            internal: InternalImage {
                raw: ptr::null_mut(),
                copy_srv: None,
                srv: None,
                unordered_access_views: Vec::new(),
                depth_stencil_views: Vec::new(),
                render_target_views: Vec::new(),
                debug_name: None,
            },
            decomposed_format: conv::DecomposedDxgiFormat::UNKNOWN,
            kind,
            mip_levels,
            format,
            usage,
            view_caps,
            bind,
            requirements: memory::Requirements {
                size: size,
                alignment: 4,
                type_mask: 0x1, // device-local only
            },
        })
    }

    unsafe fn get_image_requirements(&self, image: &Image) -> memory::Requirements {
        image.requirements
    }

    unsafe fn get_image_subresource_footprint(
        &self,
        _image: &Image,
        _sub: image::Subresource,
    ) -> image::SubresourceFootprint {
        unimplemented!()
    }

    unsafe fn bind_image_memory(
        &self,
        memory: &Memory,
        _offset: u64,
        image: &mut Image,
    ) -> Result<(), device::BindError> {
        use image::Usage;
        use memory::Properties;

        let base_format = image.format.base_format();
        let format_desc = base_format.0.desc();

        let compressed = format_desc.is_compressed();
        let depth = image.format.is_depth();
        let stencil = image.format.is_stencil();

        let (bind, usage, cpu) = if memory.properties == Properties::DEVICE_LOCAL {
            (image.bind, d3d11::D3D11_USAGE_DEFAULT, 0)
        } else if memory.properties
            == (Properties::DEVICE_LOCAL | Properties::CPU_VISIBLE | Properties::CPU_CACHED)
        {
            (
                image.bind,
                d3d11::D3D11_USAGE_DYNAMIC,
                d3d11::D3D11_CPU_ACCESS_WRITE,
            )
        } else if memory.properties == (Properties::CPU_VISIBLE | Properties::CPU_CACHED) {
            (
                0,
                d3d11::D3D11_USAGE_STAGING,
                d3d11::D3D11_CPU_ACCESS_READ | d3d11::D3D11_CPU_ACCESS_WRITE,
            )
        } else {
            unimplemented!()
        };

        let dxgi_format = conv::map_format(image.format).unwrap();
        let decomposed = conv::DecomposedDxgiFormat::from_dxgi_format(dxgi_format);
        assert!(
            memory.host_ptr.is_null(),
            "Images can only be allocated from device-local memory"
        );
        let initial_data_ptr = ptr::null_mut();

        let mut resource = ptr::null_mut();
        let view_kind = match image.kind {
            image::Kind::D1(width, layers) => {
                let desc = d3d11::D3D11_TEXTURE1D_DESC {
                    Width: width,
                    MipLevels: image.mip_levels as _,
                    ArraySize: layers as _,
                    Format: decomposed.typeless,
                    Usage: usage,
                    BindFlags: bind,
                    CPUAccessFlags: cpu,
                    MiscFlags: 0,
                };

                let hr = self.raw.CreateTexture1D(
                    &desc,
                    initial_data_ptr,
                    &mut resource as *mut *mut _ as *mut *mut _,
                );

                if !winerror::SUCCEEDED(hr) {
                    error!("CreateTexture1D failed: 0x{:x}", hr);

                    return Err(device::BindError::WrongMemory);
                }

                image::ViewKind::D1Array
            }
            image::Kind::D2(width, height, layers, samples) => {
                let desc = d3d11::D3D11_TEXTURE2D_DESC {
                    Width: width,
                    Height: height,
                    MipLevels: image.mip_levels as _,
                    ArraySize: layers as _,
                    Format: decomposed.typeless,
                    SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                        Count: samples as _,
                        Quality: 0,
                    },
                    Usage: usage,
                    BindFlags: bind,
                    CPUAccessFlags: cpu,
                    MiscFlags: {
                        let mut flags = 0;
                        if image.view_caps.contains(image::ViewCapabilities::KIND_CUBE) {
                            flags |= d3d11::D3D11_RESOURCE_MISC_TEXTURECUBE;
                        }
                        flags
                    },
                };

                let hr = self.raw.CreateTexture2D(
                    &desc,
                    initial_data_ptr,
                    &mut resource as *mut *mut _ as *mut *mut _,
                );

                if !winerror::SUCCEEDED(hr) {
                    error!("CreateTexture2D failed: 0x{:x}", hr);

                    return Err(device::BindError::WrongMemory);
                }

                image::ViewKind::D2Array
            }
            image::Kind::D3(width, height, depth) => {
                let desc = d3d11::D3D11_TEXTURE3D_DESC {
                    Width: width,
                    Height: height,
                    Depth: depth,
                    MipLevels: image.mip_levels as _,
                    Format: decomposed.typeless,
                    Usage: usage,
                    BindFlags: bind,
                    CPUAccessFlags: cpu,
                    MiscFlags: 0,
                };

                let hr = self.raw.CreateTexture3D(
                    &desc,
                    initial_data_ptr,
                    &mut resource as *mut *mut _ as *mut *mut _,
                );

                if !winerror::SUCCEEDED(hr) {
                    error!("CreateTexture3D failed: 0x{:x}", hr);

                    return Err(device::BindError::WrongMemory);
                }

                image::ViewKind::D3
            }
        };

        let mut unordered_access_views = Vec::new();

        if image.usage.contains(Usage::TRANSFER_DST)
            && !compressed
            && !depth
            && self.internal.downlevel.storage_images
        {
            for mip in 0..image.mip_levels {
                let view = ViewInfo {
                    resource: resource,
                    kind: image.kind,
                    caps: image::ViewCapabilities::empty(),
                    view_kind,
                    // TODO: we should be using `uav_format` rather than `copy_uav_format`, and share
                    //       the UAVs when the formats are identical
                    format: decomposed.copy_uav.unwrap(),
                    levels: mip..(mip + 1),
                    layers: 0..image.kind.num_layers(),
                };

                let uav = self
                    .view_image_as_unordered_access(&view)
                    .map_err(|_| device::BindError::WrongMemory)?;

                if let Some(ref name) = image.internal.debug_name {
                    set_debug_name(&uav, &format!("{} -- UAV Mip {}", name, mip));
                }

                unordered_access_views.push(uav);
            }
        }

        let (copy_srv, srv) = if image.usage.contains(image::Usage::TRANSFER_SRC) {
            let mut view = ViewInfo {
                resource: resource,
                kind: image.kind,
                caps: image::ViewCapabilities::empty(),
                view_kind,
                format: decomposed.copy_srv.unwrap(),
                levels: 0..image.mip_levels,
                layers: 0..image.kind.num_layers(),
            };

            let copy_srv = if !compressed {
                Some(
                    self.view_image_as_shader_resource(&view)
                        .map_err(|_| device::BindError::WrongMemory)?,
                )
            } else {
                None
            };

            view.format = decomposed.srv.unwrap();

            let srv = if !depth && !stencil {
                Some(
                    self.view_image_as_shader_resource(&view)
                        .map_err(|_| device::BindError::WrongMemory)?,
                )
            } else {
                None
            };

            (copy_srv, srv)
        } else {
            (None, None)
        };

        let mut render_target_views = Vec::new();

        if (image.usage.contains(image::Usage::COLOR_ATTACHMENT)
            || image.usage.contains(image::Usage::TRANSFER_DST))
            && !compressed
            && !depth
        {
            for layer in 0..image.kind.num_layers() {
                for mip in 0..image.mip_levels {
                    let view = ViewInfo {
                        resource,
                        kind: image.kind,
                        caps: image::ViewCapabilities::empty(),
                        view_kind,
                        format: decomposed.rtv.unwrap(),
                        levels: mip..(mip + 1),
                        layers: layer..(layer + 1),
                    };

                    let rtv = self
                        .view_image_as_render_target(&view)
                        .map_err(|_| device::BindError::WrongMemory)?;

                    if let Some(ref name) = image.internal.debug_name {
                        set_debug_name(
                            &rtv,
                            &format!("{} -- RTV Mip {} Layer {}", name, mip, layer),
                        );
                    }

                    render_target_views.push(rtv);
                }
            }
        };

        let mut depth_stencil_views = Vec::new();

        if depth {
            for layer in 0..image.kind.num_layers() {
                for mip in 0..image.mip_levels {
                    let view = ViewInfo {
                        resource,
                        kind: image.kind,
                        caps: image::ViewCapabilities::empty(),
                        view_kind,
                        format: decomposed.dsv.unwrap(),
                        levels: mip..(mip + 1),
                        layers: layer..(layer + 1),
                    };

                    let dsv = self
                        .view_image_as_depth_stencil(&view, None)
                        .map_err(|_| device::BindError::WrongMemory)?;

                    if let Some(ref name) = image.internal.debug_name {
                        set_debug_name(
                            &dsv,
                            &format!("{} -- DSV Mip {} Layer {}", name, mip, layer),
                        );
                    }

                    depth_stencil_views.push(dsv);
                }
            }
        }

        if let Some(ref mut name) = image.internal.debug_name {
            set_debug_name(&*resource, name);
            if let Some(ref copy_srv) = copy_srv {
                set_debug_name_with_suffix(copy_srv, name, " -- Copy SRV");
            }
            if let Some(ref srv) = srv {
                set_debug_name_with_suffix(srv, name, " -- SRV");
            }
        }

        let internal = InternalImage {
            raw: resource,
            copy_srv,
            srv,
            unordered_access_views,
            depth_stencil_views,
            render_target_views,
            debug_name: image.internal.debug_name.take(),
        };

        image.decomposed_format = decomposed;
        image.internal = internal;

        Ok(())
    }

    unsafe fn create_image_view(
        &self,
        image: &Image,
        view_kind: image::ViewKind,
        format: format::Format,
        _swizzle: format::Swizzle,
        usage: image::Usage,
        range: image::SubresourceRange,
    ) -> Result<ImageView, image::ViewCreationError> {
        let is_array = image.kind.num_layers() > 1;
        let num_levels = range.resolve_level_count(image.mip_levels);
        let num_layers = range.resolve_layer_count(image.kind.num_layers());

        let info = ViewInfo {
            resource: image.internal.raw,
            kind: image.kind,
            caps: image.view_caps,
            // D3D11 doesn't allow looking at a single slice of an array as a non-array
            view_kind: if is_array && view_kind == image::ViewKind::D2 {
                image::ViewKind::D2Array
            } else if is_array && view_kind == image::ViewKind::D1 {
                image::ViewKind::D1Array
            } else {
                view_kind
            },
            format: conv::map_format(format).ok_or(image::ViewCreationError::BadFormat(format))?,
            levels: range.level_start..range.level_start + num_levels,
            layers: range.layer_start..range.layer_start + num_layers,
        };

        let srv_info = ViewInfo {
            format: conv::viewable_format(info.format),
            ..info.clone()
        };

        let mut debug_name = image.internal.debug_name.clone();

        Ok(ImageView {
            subresource: d3d11::D3D11CalcSubresource(
                0,
                range.layer_start as _,
                range.level_start as _,
            ),
            format,
            srv_handle: if usage.intersects(image::Usage::SAMPLED) {
                let srv = self.view_image_as_shader_resource(&srv_info)?;

                if let Some(ref mut name) = debug_name {
                    set_debug_name_with_suffix(&srv, name, " -- SRV");
                }

                Some(srv.into_raw())
            } else {
                None
            },
            rtv_handle: if usage.contains(image::Usage::COLOR_ATTACHMENT) {
                let rtv = self.view_image_as_render_target(&info)?;

                if let Some(ref mut name) = debug_name {
                    set_debug_name_with_suffix(&rtv, name, " -- RTV");
                }

                Some(rtv.into_raw())
            } else {
                None
            },
            uav_handle: if usage.contains(image::Usage::STORAGE) {
                let uav = self.view_image_as_unordered_access(&info)?;

                if let Some(ref mut name) = debug_name {
                    set_debug_name_with_suffix(&uav, name, " -- UAV");
                }

                Some(uav.into_raw())
            } else {
                None
            },
            dsv_handle: if usage.contains(image::Usage::DEPTH_STENCIL_ATTACHMENT) {
                if let Some(dsv) = self.view_image_as_depth_stencil(&info, None).ok() {
                    if let Some(ref mut name) = debug_name {
                        set_debug_name_with_suffix(&dsv, name, " -- DSV");
                    }

                    Some(dsv.into_raw())
                } else {
                    None
                }
            } else {
                None
            },
            rodsv_handle: if usage.contains(image::Usage::DEPTH_STENCIL_ATTACHMENT)
                && self.internal.downlevel.read_only_depth_stencil
            {
                if let Some(rodsv) = self
                    .view_image_as_depth_stencil(&info, Some(image.format.is_stencil()))
                    .ok()
                {
                    if let Some(ref mut name) = debug_name {
                        set_debug_name_with_suffix(&rodsv, name, " -- DSV");
                    }

                    Some(rodsv.into_raw())
                } else {
                    None
                }
            } else {
                None
            },
            owned: true,
        })
    }

    unsafe fn create_sampler(
        &self,
        info: &image::SamplerDesc,
    ) -> Result<Sampler, device::AllocationError> {
        assert!(info.normalized);

        let op = match info.comparison {
            Some(_) => d3d11::D3D11_FILTER_REDUCTION_TYPE_COMPARISON,
            None => d3d11::D3D11_FILTER_REDUCTION_TYPE_STANDARD,
        };

        let desc = d3d11::D3D11_SAMPLER_DESC {
            Filter: conv::map_filter(
                info.min_filter,
                info.mag_filter,
                info.mip_filter,
                op,
                info.anisotropy_clamp,
            ),
            AddressU: conv::map_wrapping(info.wrap_mode.0),
            AddressV: conv::map_wrapping(info.wrap_mode.1),
            AddressW: conv::map_wrapping(info.wrap_mode.2),
            MipLODBias: info.lod_bias.0,
            MaxAnisotropy: info.anisotropy_clamp.map_or(0, |aniso| aniso as u32),
            ComparisonFunc: info.comparison.map_or(0, |comp| conv::map_comparison(comp)),
            BorderColor: info.border.into(),
            MinLOD: info.lod_range.start.0,
            MaxLOD: info.lod_range.end.0,
        };

        let mut sampler = ptr::null_mut();
        let hr = self
            .raw
            .CreateSamplerState(&desc, &mut sampler as *mut *mut _ as *mut *mut _);

        assert_eq!(true, winerror::SUCCEEDED(hr));

        Ok(Sampler {
            sampler_handle: ComPtr::from_raw(sampler),
        })
    }

    unsafe fn create_descriptor_pool<I>(
        &self,
        _max_sets: usize,
        ranges: I,
        _flags: pso::DescriptorPoolCreateFlags,
    ) -> Result<DescriptorPool, device::OutOfMemory>
    where
        I: Iterator<Item = pso::DescriptorRangeDesc>,
    {
        let mut total = RegisterData::default();
        for range in ranges {
            let content = DescriptorContent::from(range.ty);
            total.add_content_many(content, range.count as DescriptorIndex);
        }

        let max_stages = 6;
        let count = total.sum() * max_stages;
        Ok(DescriptorPool::with_capacity(count))
    }

    unsafe fn create_descriptor_set_layout<'a, I, J>(
        &self,
        layout_bindings: I,
        _immutable_samplers: J,
    ) -> Result<DescriptorSetLayout, device::OutOfMemory>
    where
        I: Iterator<Item = pso::DescriptorSetLayoutBinding>,
        J: Iterator<Item = &'a Sampler>,
    {
        let mut total = MultiStageData::<RegisterData<_>>::default();
        let mut bindings = layout_bindings.collect::<Vec<_>>();

        for binding in bindings.iter() {
            let content = DescriptorContent::from(binding.ty);
            // If this binding is used by the graphics pipeline and is a UAV, it belongs to the "Output Merger"
            // stage, so we only put them in the fragment stage to save redundant descriptor allocations.
            let stage_flags = if content.contains(DescriptorContent::UAV)
                && binding
                    .stage_flags
                    .intersects(pso::ShaderStageFlags::ALL - pso::ShaderStageFlags::COMPUTE)
            {
                let mut stage_flags = pso::ShaderStageFlags::FRAGMENT;
                stage_flags.set(
                    pso::ShaderStageFlags::COMPUTE,
                    binding.stage_flags.contains(pso::ShaderStageFlags::COMPUTE),
                );
                stage_flags
            } else {
                binding.stage_flags
            };
            total.add_content_many(content, stage_flags, binding.count as _);
        }

        bindings.sort_by_key(|a| a.binding);

        let accum = total.map_register(|count| RegisterAccumulator {
            res_index: *count as ResourceIndex,
        });

        Ok(DescriptorSetLayout {
            bindings: Arc::new(bindings),
            pool_mapping: accum.to_mapping(),
        })
    }

    unsafe fn write_descriptor_set<'a, I>(&self, op: pso::DescriptorSetWrite<'a, Backend, I>)
    where
        I: Iterator<Item = pso::Descriptor<'a, Backend>>,
    {
        // Get baseline mapping
        let mut mapping = op
            .set
            .layout
            .pool_mapping
            .map_register(|mapping| mapping.offset);

        // Iterate over layout bindings until the first binding is found.
        let binding_start = op
            .set
            .layout
            .bindings
            .iter()
            .position(|binding| binding.binding == op.binding)
            .unwrap();

        // If we've skipped layout bindings, we need to add them to get the correct binding offset
        for binding in &op.set.layout.bindings[..binding_start] {
            let content = DescriptorContent::from(binding.ty);
            mapping.add_content_many(content, binding.stage_flags, binding.count as _);
        }

        // We start at the given binding index and array index
        let mut binding_index = binding_start;
        let mut array_index = op.array_offset;

        // If we're skipping array indices in the current binding, we need to add them to get the correct binding offset
        if array_index > 0 {
            let binding: &pso::DescriptorSetLayoutBinding = &op.set.layout.bindings[binding_index];
            let content = DescriptorContent::from(binding.ty);
            mapping.add_content_many(content, binding.stage_flags, array_index as _);
        }

        // Iterate over the descriptors, figuring out the corresponding binding, and adding
        // it to the set of bindings.
        //
        // When we hit the end of an array of descriptors and there are still descriptors left
        // over, we will spill into writing the next binding.
        for descriptor in op.descriptors {
            let binding: &pso::DescriptorSetLayoutBinding = &op.set.layout.bindings[binding_index];

            let handles = match descriptor {
                pso::Descriptor::Buffer(buffer, ref _sub) => RegisterData {
                    c: match buffer.internal.disjoint_cb {
                        Some(dj_buf) => dj_buf as *mut _,
                        None => buffer.internal.raw as *mut _,
                    },
                    t: buffer.internal.srv.map_or(ptr::null_mut(), |p| p as *mut _),
                    u: buffer.internal.uav.map_or(ptr::null_mut(), |p| p as *mut _),
                    s: ptr::null_mut(),
                },
                pso::Descriptor::Image(image, _layout) => RegisterData {
                    c: ptr::null_mut(),
                    t: image.srv_handle.map_or(ptr::null_mut(), |h| h as *mut _),
                    u: image.uav_handle.map_or(ptr::null_mut(), |h| h as *mut _),
                    s: ptr::null_mut(),
                },
                pso::Descriptor::Sampler(sampler) => RegisterData {
                    c: ptr::null_mut(),
                    t: ptr::null_mut(),
                    u: ptr::null_mut(),
                    s: sampler.sampler_handle.as_raw() as *mut _,
                },
                pso::Descriptor::CombinedImageSampler(image, _layout, sampler) => RegisterData {
                    c: ptr::null_mut(),
                    t: image.srv_handle.map_or(ptr::null_mut(), |h| h as *mut _),
                    u: image.uav_handle.map_or(ptr::null_mut(), |h| h as *mut _),
                    s: sampler.sampler_handle.as_raw() as *mut _,
                },
                pso::Descriptor::TexelBuffer(_buffer_view) => unimplemented!(),
            };

            let content = DescriptorContent::from(binding.ty);
            if content.contains(DescriptorContent::CBV) {
                let offsets = mapping.map_other(|map| map.c);
                op.set
                    .assign_stages(&offsets, binding.stage_flags, handles.c);
            };
            if content.contains(DescriptorContent::SRV) {
                let offsets = mapping.map_other(|map| map.t);
                op.set
                    .assign_stages(&offsets, binding.stage_flags, handles.t);
            };
            if content.contains(DescriptorContent::UAV) {
                // If this binding is used by the graphics pipeline and is a UAV, it belongs to the "Output Merger"
                // stage, so we only put them in the fragment stage to save redundant descriptor allocations.
                let stage_flags = if binding
                    .stage_flags
                    .intersects(pso::ShaderStageFlags::ALL - pso::ShaderStageFlags::COMPUTE)
                {
                    let mut stage_flags = pso::ShaderStageFlags::FRAGMENT;
                    stage_flags.set(
                        pso::ShaderStageFlags::COMPUTE,
                        binding.stage_flags.contains(pso::ShaderStageFlags::COMPUTE),
                    );
                    stage_flags
                } else {
                    binding.stage_flags
                };

                let offsets = mapping.map_other(|map| map.u);
                op.set.assign_stages(&offsets, stage_flags, handles.u);
            };
            if content.contains(DescriptorContent::SAMPLER) {
                let offsets = mapping.map_other(|map| map.s);
                op.set
                    .assign_stages(&offsets, binding.stage_flags, handles.s);
            };

            mapping.add_content_many(content, binding.stage_flags, 1);

            array_index += 1;
            if array_index >= binding.count {
                // We've run out of array to write to, we should overflow to the next binding.
                array_index = 0;
                binding_index += 1;
            }
        }
    }

    unsafe fn copy_descriptor_set<'a>(&self, _op: pso::DescriptorSetCopy<'a, Backend>) {
        unimplemented!()
        /*
        for offset in 0 .. copy.count {
            let (dst_ty, dst_handle_offset, dst_second_handle_offset) = copy
                .dst_set
                .get_handle_offset(copy.dst_binding + offset as u32);
            let (src_ty, src_handle_offset, src_second_handle_offset) = copy
                .src_set
                .get_handle_offset(copy.src_binding + offset as u32);
            assert_eq!(dst_ty, src_ty);

            let dst_handle = copy.dst_set.handles.offset(dst_handle_offset as isize);
            let src_handle = copy.dst_set.handles.offset(src_handle_offset as isize);

            match dst_ty {
                pso::DescriptorType::Image {
                    ty: pso::ImageDescriptorType::Sampled { with_sampler: true }
                } => {
                    let dst_second_handle = copy
                        .dst_set
                        .handles
                        .offset(dst_second_handle_offset as isize);
                    let src_second_handle = copy
                        .dst_set
                        .handles
                        .offset(src_second_handle_offset as isize);

                    *dst_handle = *src_handle;
                    *dst_second_handle = *src_second_handle;
                }
                _ => *dst_handle = *src_handle,
            }
        }*/
    }

    unsafe fn map_memory(
        &self,
        memory: &mut Memory,
        segment: memory::Segment,
    ) -> Result<*mut u8, device::MapError> {
        Ok(memory.host_ptr.offset(segment.offset as isize))
    }

    unsafe fn unmap_memory(&self, _memory: &mut Memory) {
        // persistent mapping FTW
    }

    unsafe fn flush_mapped_memory_ranges<'a, I>(&self, ranges: I) -> Result<(), device::OutOfMemory>
    where
        I: Iterator<Item = (&'a Memory, memory::Segment)>,
    {
        let _scope = debug_scope!(&self.context, "FlushMappedRanges");

        // go through every range we wrote to
        for (memory, ref segment) in ranges {
            let range = memory.resolve(segment);
            let _scope = debug_scope!(&self.context, "Range({:?})", range);
            memory.flush(&self.context, range);
        }

        Ok(())
    }

    unsafe fn invalidate_mapped_memory_ranges<'a, I>(
        &self,
        ranges: I,
    ) -> Result<(), device::OutOfMemory>
    where
        I: Iterator<Item = (&'a Memory, memory::Segment)>,
    {
        let _scope = debug_scope!(&self.context, "InvalidateMappedRanges");

        // go through every range we want to read from
        for (memory, ref segment) in ranges {
            let range = memory.resolve(segment);
            let _scope = debug_scope!(&self.context, "Range({:?})", range);
            memory.invalidate(
                &self.context,
                range,
                self.internal.working_buffer.clone(),
                self.internal.working_buffer_size,
            );
        }

        Ok(())
    }

    fn create_semaphore(&self) -> Result<Semaphore, device::OutOfMemory> {
        // TODO:
        Ok(Semaphore)
    }

    fn create_fence(&self, signalled: bool) -> Result<Fence, device::OutOfMemory> {
        Ok(Arc::new(RawFence {
            mutex: Mutex::new(signalled),
            condvar: Condvar::new(),
        }))
    }

    unsafe fn reset_fence(&self, fence: &mut Fence) -> Result<(), device::OutOfMemory> {
        *fence.mutex.lock() = false;
        Ok(())
    }

    unsafe fn wait_for_fence(
        &self,
        fence: &Fence,
        timeout_ns: u64,
    ) -> Result<bool, device::WaitError> {
        use std::time::{Duration, Instant};

        debug!("wait_for_fence {:?} for {} ns", fence, timeout_ns);
        let mut guard = fence.mutex.lock();
        match timeout_ns {
            0 => Ok(*guard),
            0xFFFFFFFFFFFFFFFF => {
                while !*guard {
                    fence.condvar.wait(&mut guard);
                }
                Ok(true)
            }
            _ => {
                let total = Duration::from_nanos(timeout_ns as u64);
                let now = Instant::now();
                while !*guard {
                    let duration = match total.checked_sub(now.elapsed()) {
                        Some(dur) => dur,
                        None => return Ok(false),
                    };
                    let result = fence.condvar.wait_for(&mut guard, duration);
                    if result.timed_out() {
                        return Ok(false);
                    }
                }
                Ok(true)
            }
        }
    }

    unsafe fn get_fence_status(&self, fence: &Fence) -> Result<bool, device::DeviceLost> {
        Ok(*fence.mutex.lock())
    }

    fn create_event(&self) -> Result<(), device::OutOfMemory> {
        unimplemented!()
    }

    unsafe fn get_event_status(&self, _event: &()) -> Result<bool, device::WaitError> {
        unimplemented!()
    }

    unsafe fn set_event(&self, _event: &mut ()) -> Result<(), device::OutOfMemory> {
        unimplemented!()
    }

    unsafe fn reset_event(&self, _event: &mut ()) -> Result<(), device::OutOfMemory> {
        unimplemented!()
    }

    unsafe fn free_memory(&self, mut memory: Memory) {
        if !memory.host_ptr.is_null() {
            let _vec =
                Vec::from_raw_parts(memory.host_ptr, memory.size as usize, memory.size as usize);
            // let it drop
            memory.host_ptr = ptr::null_mut();
        }
        for (_, (_range, mut internal)) in memory.local_buffers.write().drain() {
            internal.release_resources()
        }
    }

    unsafe fn create_query_pool(
        &self,
        _query_ty: query::Type,
        _count: query::Id,
    ) -> Result<QueryPool, query::CreationError> {
        unimplemented!()
    }

    unsafe fn destroy_query_pool(&self, _pool: QueryPool) {
        unimplemented!()
    }

    unsafe fn get_query_pool_results(
        &self,
        _pool: &QueryPool,
        _queries: Range<query::Id>,
        _data: &mut [u8],
        _stride: buffer::Stride,
        _flags: query::ResultFlags,
    ) -> Result<bool, device::WaitError> {
        unimplemented!()
    }

    unsafe fn destroy_shader_module(&self, _shader_lib: ShaderModule) {}

    unsafe fn destroy_render_pass(&self, _rp: RenderPass) {
        //unimplemented!()
    }

    unsafe fn destroy_pipeline_layout(&self, _layout: PipelineLayout) {
        //unimplemented!()
    }

    unsafe fn destroy_graphics_pipeline(&self, _pipeline: GraphicsPipeline) {}

    unsafe fn destroy_compute_pipeline(&self, _pipeline: ComputePipeline) {}

    unsafe fn destroy_framebuffer(&self, _fb: Framebuffer) {}

    unsafe fn destroy_buffer(&self, buffer: Buffer) {
        let mut internal = buffer.internal;

        if internal.raw.is_null() {
            return;
        }

        let arena_arc = match buffer.local_memory_arena.upgrade() {
            Some(arena) => arena,
            // Memory is destroyed before the buffer, we've already been destroyed.
            None => return,
        };
        let mut arena = arena_arc.write();

        let memory_index = buffer.memory_index.expect("Buffer's memory index unset");
        // Drop the internal stored by the arena on the floor, it owns nothing.
        let _ = arena.remove(memory_index);

        // Release all memory owned by this buffer
        internal.release_resources();
    }

    unsafe fn destroy_buffer_view(&self, _view: BufferView) {
        //unimplemented!()
    }

    unsafe fn destroy_image(&self, mut image: Image) {
        image.internal.release_resources();
    }

    unsafe fn destroy_image_view(&self, _view: ImageView) {
        //unimplemented!()
    }

    unsafe fn destroy_sampler(&self, _sampler: Sampler) {}

    unsafe fn destroy_descriptor_pool(&self, _pool: DescriptorPool) {
        //unimplemented!()
    }

    unsafe fn destroy_descriptor_set_layout(&self, _layout: DescriptorSetLayout) {
        //unimplemented!()
    }

    unsafe fn destroy_fence(&self, _fence: Fence) {
        // unimplemented!()
    }

    unsafe fn destroy_semaphore(&self, _semaphore: Semaphore) {
        //unimplemented!()
    }

    unsafe fn destroy_event(&self, _event: ()) {
        //unimplemented!()
    }

    fn wait_idle(&self) -> Result<(), device::OutOfMemory> {
        Ok(())
        // unimplemented!()
    }

    unsafe fn set_image_name(&self, image: &mut Image, name: &str) {
        if !verify_debug_ascii(name) {
            return;
        }

        image.internal.debug_name = Some(name.to_string());
    }

    unsafe fn set_buffer_name(&self, buffer: &mut Buffer, name: &str) {
        if !verify_debug_ascii(name) {
            return;
        }

        buffer.internal.debug_name = Some(name.to_string());
    }

    unsafe fn set_command_buffer_name(&self, command_buffer: &mut CommandBuffer, name: &str) {
        if !verify_debug_ascii(name) {
            return;
        }

        command_buffer.debug_name = Some(name.to_string());
    }

    unsafe fn set_semaphore_name(&self, _semaphore: &mut Semaphore, _name: &str) {
        // TODO
    }

    unsafe fn set_fence_name(&self, _fence: &mut Fence, _name: &str) {
        // TODO
    }

    unsafe fn set_framebuffer_name(&self, _framebuffer: &mut Framebuffer, _name: &str) {
        // TODO
    }

    unsafe fn set_render_pass_name(&self, _render_pass: &mut RenderPass, _name: &str) {
        // TODO
    }

    unsafe fn set_descriptor_set_name(&self, _descriptor_set: &mut DescriptorSet, _name: &str) {
        // TODO
    }

    unsafe fn set_descriptor_set_layout_name(
        &self,
        _descriptor_set_layout: &mut DescriptorSetLayout,
        _name: &str,
    ) {
        // TODO
    }

    unsafe fn set_pipeline_layout_name(&self, _pipeline_layout: &mut PipelineLayout, _name: &str) {
        // TODO
    }

    unsafe fn set_display_power_state(
        &self,
        _display: &hal::display::Display<Backend>,
        _power_state: &hal::display::control::PowerState,
    ) -> Result<(), hal::display::control::DisplayControlError> {
        unimplemented!()
    }

    unsafe fn register_device_event(
        &self,
        _device_event: &hal::display::control::DeviceEvent,
        _fence: &mut <Backend as hal::Backend>::Fence,
    ) -> Result<(), hal::display::control::DisplayControlError> {
        unimplemented!()
    }

    unsafe fn register_display_event(
        &self,
        _display: &hal::display::Display<Backend>,
        _display_event: &hal::display::control::DisplayEvent,
        _fence: &mut <Backend as hal::Backend>::Fence,
    ) -> Result<(), hal::display::control::DisplayControlError> {
        unimplemented!()
    }

    unsafe fn create_allocate_external_buffer(
        &self,
        _external_memory_type: hal::external_memory::ExternalBufferMemoryType,
        _usage: hal::buffer::Usage,
        _sparse: hal::memory::SparseFlags,
        _type_mask: u32,
        _size: u64,
    ) -> Result<(Buffer, Memory), hal::external_memory::ExternalResourceError> {
        unimplemented!()
    }

    unsafe fn import_external_buffer(
        &self,
        _external_memory: hal::external_memory::ExternalBufferMemory,
        _usage: hal::buffer::Usage,
        _sparse: hal::memory::SparseFlags,
        _type_mask: u32,
        _size: u64,
    ) -> Result<(Buffer, Memory), hal::external_memory::ExternalResourceError> {
        unimplemented!()
    }

    unsafe fn create_allocate_external_image(
        &self,
        _external_memory_type: hal::external_memory::ExternalImageMemoryType,
        _kind: image::Kind,
        _mip_levels: image::Level,
        _format: format::Format,
        _tiling: image::Tiling,
        _usage: image::Usage,
        _sparse: memory::SparseFlags,
        _view_caps: image::ViewCapabilities,
        _type_mask: u32,
    ) -> Result<(Image, Memory), hal::external_memory::ExternalResourceError> {
        unimplemented!()
    }

    unsafe fn import_external_image(
        &self,
        _external_memory: hal::external_memory::ExternalImageMemory,
        _kind: image::Kind,
        _mip_levels: image::Level,
        _format: format::Format,
        _tiling: image::Tiling,
        _usage: image::Usage,
        _sparse: memory::SparseFlags,
        _view_caps: image::ViewCapabilities,
        _type_mask: u32,
    ) -> Result<(Image, Memory), hal::external_memory::ExternalResourceError> {
        unimplemented!()
    }

    unsafe fn export_memory(
        &self,
        _external_memory_type: hal::external_memory::ExternalMemoryType,
        _memory: &Memory,
    ) -> Result<hal::external_memory::PlatformMemory, hal::external_memory::ExternalMemoryExportError>
    {
        unimplemented!()
    }

    unsafe fn drm_format_modifier(&self, _image: &Image) -> Option<hal::format::DrmModifier> {
        None
    }

    fn start_capture(&self) {
        unsafe {
            self.render_doc
                .start_frame_capture(self.raw.as_raw() as *mut _, ptr::null_mut())
        }
    }

    fn stop_capture(&self) {
        unsafe {
            self.render_doc
                .end_frame_capture(self.raw.as_raw() as *mut _, ptr::null_mut())
        }
    }
}
