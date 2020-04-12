//#[deny(missing_docs)]

extern crate gfx_hal as hal;
extern crate auxil;
extern crate range_alloc;
#[macro_use]
extern crate bitflags;
extern crate libloading;
#[macro_use]
extern crate log;
extern crate parking_lot;
extern crate smallvec;
extern crate spirv_cross;
#[macro_use]
extern crate winapi;
extern crate wio;

use hal::{
    adapter,
    buffer,
    command,
    format,
    image,
    memory,
    pass,
    pso,
    query,
    queue,
    range::RangeArg,
    window,
    DrawCount,
    IndexCount,
    InstanceCount,
    Limits,
    VertexCount,
    VertexOffset,
    WorkGroupCount,
};

use range_alloc::RangeAllocator;

use winapi::shared::dxgi::{IDXGIAdapter, IDXGIFactory, IDXGISwapChain};
use winapi::shared::minwindef::{FALSE, UINT, HMODULE};
use winapi::shared::windef::{HWND, RECT};
use winapi::shared::{dxgiformat, winerror};
use winapi::um::winuser::GetClientRect;
use winapi::um::{d3d11, d3dcommon};
use winapi::Interface as _;

use wio::com::ComPtr;

use parking_lot::{Condvar, Mutex};

use std::borrow::Borrow;
use std::cell::RefCell;
use std::fmt;
use std::mem;
use std::ops::Range;
use std::ptr;
use std::sync::Arc;

use std::os::raw::c_void;

macro_rules! debug_scope {
    ($context:expr, $($arg:tt)+) => ({
        #[cfg(debug_assertions)]
        {
            $crate::debug::DebugScope::with_name(
                $context,
                format_args!($($arg)+),
            )
        }
        #[cfg(not(debug_assertions))]
        {
            ()
        }
    });
}

macro_rules! debug_marker {
    ($context:expr, $($arg:tt)+) => ({
        #[cfg(debug_assertions)]
        {
            $crate::debug::debug_marker(
                $context,
                format_args!($($arg)+),
            );
        }
    });
}

mod conv;
#[cfg(debug_assertions)]
mod debug;
mod device;
mod dxgi;
mod internal;
mod shader;

type CreateFun = extern "system" fn(
    *mut IDXGIAdapter,
    UINT,
    HMODULE,
    UINT,
    *const UINT,
    UINT,
    UINT,
    *mut *mut d3d11::ID3D11Device,
    *mut UINT,
    *mut *mut d3d11::ID3D11DeviceContext,
) -> winerror::HRESULT;

#[derive(Clone)]
pub(crate) struct ViewInfo {
    resource: *mut d3d11::ID3D11Resource,
    kind: image::Kind,
    caps: image::ViewCapabilities,
    view_kind: image::ViewKind,
    format: dxgiformat::DXGI_FORMAT,
    range: image::SubresourceRange,
}

impl fmt::Debug for ViewInfo {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("ViewInfo")
    }
}

#[derive(Debug)]
pub struct Instance {
    pub(crate) factory: ComPtr<IDXGIFactory>,
    pub(crate) dxgi_version: dxgi::DxgiVersion,
    library: Arc<libloading::Library>,
}

unsafe impl Send for Instance {}
unsafe impl Sync for Instance {}

impl Instance {
    pub fn create_surface_from_hwnd(&self, hwnd: *mut c_void) -> Surface {
        Surface {
            factory: self.factory.clone(),
            wnd_handle: hwnd as *mut _,
            presentation: None,
        }
    }
}

fn get_features(
    _device: ComPtr<d3d11::ID3D11Device>,
    _feature_level: d3dcommon::D3D_FEATURE_LEVEL,
) -> hal::Features {
    hal::Features::ROBUST_BUFFER_ACCESS
        | hal::Features::FULL_DRAW_INDEX_U32
        | hal::Features::FORMAT_BC
        | hal::Features::INSTANCE_RATE
        | hal::Features::SAMPLER_MIP_LOD_BIAS
}

fn get_format_properties(
    device: ComPtr<d3d11::ID3D11Device>,
) -> [format::Properties; format::NUM_FORMATS] {
    let mut format_properties = [format::Properties::default(); format::NUM_FORMATS];
    for (i, props) in &mut format_properties.iter_mut().enumerate().skip(1) {
        let format: format::Format = unsafe { mem::transmute(i as u32) };

        let dxgi_format = match conv::map_format(format) {
            Some(format) => format,
            None => continue,
        };

        let mut support = d3d11::D3D11_FEATURE_DATA_FORMAT_SUPPORT {
            InFormat: dxgi_format,
            OutFormatSupport: 0,
        };
        let mut support_2 = d3d11::D3D11_FEATURE_DATA_FORMAT_SUPPORT2 {
            InFormat: dxgi_format,
            OutFormatSupport2: 0,
        };

        let hr = unsafe {
            device.CheckFeatureSupport(
                d3d11::D3D11_FEATURE_FORMAT_SUPPORT,
                &mut support as *mut _ as *mut _,
                mem::size_of::<d3d11::D3D11_FEATURE_DATA_FORMAT_SUPPORT>() as UINT,
            )
        };

        if hr == winerror::S_OK {
            let can_buffer = 0 != support.OutFormatSupport & d3d11::D3D11_FORMAT_SUPPORT_BUFFER;
            let can_image = 0
                != support.OutFormatSupport
                    & (d3d11::D3D11_FORMAT_SUPPORT_TEXTURE1D
                        | d3d11::D3D11_FORMAT_SUPPORT_TEXTURE2D
                        | d3d11::D3D11_FORMAT_SUPPORT_TEXTURE3D
                        | d3d11::D3D11_FORMAT_SUPPORT_TEXTURECUBE);
            let can_linear = can_image && !format.surface_desc().is_compressed();
            if can_image {
                props.optimal_tiling |=
                    format::ImageFeature::SAMPLED | format::ImageFeature::BLIT_SRC;
            }
            if can_linear {
                props.linear_tiling |=
                    format::ImageFeature::SAMPLED | format::ImageFeature::BLIT_SRC;
            }
            if support.OutFormatSupport & d3d11::D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER != 0 {
                props.buffer_features |= format::BufferFeature::VERTEX;
            }
            if support.OutFormatSupport & d3d11::D3D11_FORMAT_SUPPORT_SHADER_SAMPLE != 0 {
                props.optimal_tiling |= format::ImageFeature::SAMPLED_LINEAR;
            }
            if support.OutFormatSupport & d3d11::D3D11_FORMAT_SUPPORT_RENDER_TARGET != 0 {
                props.optimal_tiling |=
                    format::ImageFeature::COLOR_ATTACHMENT | format::ImageFeature::BLIT_DST;
                if can_linear {
                    props.linear_tiling |=
                        format::ImageFeature::COLOR_ATTACHMENT | format::ImageFeature::BLIT_DST;
                }
            }
            if support.OutFormatSupport & d3d11::D3D11_FORMAT_SUPPORT_BLENDABLE != 0 {
                props.optimal_tiling |= format::ImageFeature::COLOR_ATTACHMENT_BLEND;
            }
            if support.OutFormatSupport & d3d11::D3D11_FORMAT_SUPPORT_DEPTH_STENCIL != 0 {
                props.optimal_tiling |= format::ImageFeature::DEPTH_STENCIL_ATTACHMENT;
            }
            if support.OutFormatSupport & d3d11::D3D11_FORMAT_SUPPORT_SHADER_LOAD != 0 {
                //TODO: check d3d12::D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD ?
                if can_buffer {
                    props.buffer_features |= format::BufferFeature::UNIFORM_TEXEL;
                }
            }

            let hr = unsafe {
                device.CheckFeatureSupport(
                    d3d11::D3D11_FEATURE_FORMAT_SUPPORT2,
                    &mut support_2 as *mut _ as *mut _,
                    mem::size_of::<d3d11::D3D11_FEATURE_DATA_FORMAT_SUPPORT2>() as UINT,
                )
            };
            if hr == winerror::S_OK {
                if support_2.OutFormatSupport2 & d3d11::D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD != 0 {
                    //TODO: other atomic flags?
                    if can_buffer {
                        props.buffer_features |= format::BufferFeature::STORAGE_TEXEL_ATOMIC;
                    }
                    if can_image {
                        props.optimal_tiling |= format::ImageFeature::STORAGE_ATOMIC;
                    }
                }
                if support_2.OutFormatSupport2 & d3d11::D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE != 0 {
                    if can_buffer {
                        props.buffer_features |= format::BufferFeature::STORAGE_TEXEL;
                    }
                    if can_image {
                        props.optimal_tiling |= format::ImageFeature::STORAGE;
                    }
                }
            }
        }

        //TODO: blits, linear tiling
    }

    format_properties
}

impl hal::Instance<Backend> for Instance {
    fn create(_: &str, _: u32) -> Result<Self, hal::UnsupportedBackend> {
        // TODO: get the latest factory we can find

        match dxgi::get_dxgi_factory() {
            Ok((factory, dxgi_version)) => {
                info!("DXGI version: {:?}", dxgi_version);
                let library = Arc::new(
                    libloading::Library::new("d3d11.dll")
                        .map_err(|_| hal::UnsupportedBackend)?
                );
                Ok(Instance {
                    factory,
                    dxgi_version,
                    library,
                })
            }
            Err(hr) => {
                info!("Failed on factory creation: {:?}", hr);
                Err(hal::UnsupportedBackend)
            }
        }
    }

    fn enumerate_adapters(&self) -> Vec<adapter::Adapter<Backend>> {
        let mut adapters = Vec::new();
        let mut idx = 0;

        let func: libloading::Symbol<CreateFun> = match unsafe {
            self.library.get(b"D3D11CreateDevice")
        } {
            Ok(func) => func,
            Err(e) => {
                error!("Unable to get device creation function: {:?}", e);
                return Vec::new();
            }
        };

        while let Ok((adapter, info)) =
            dxgi::get_adapter(idx, self.factory.as_raw(), self.dxgi_version)
        {
            idx += 1;

            use hal::memory::Properties;

            // TODO: move into function?
            let (device, feature_level) = {
                let feature_level = get_feature_level(&func, adapter.as_raw());

                let mut device = ptr::null_mut();
                let hr = func(
                    adapter.as_raw() as *mut _,
                    d3dcommon::D3D_DRIVER_TYPE_UNKNOWN,
                    ptr::null_mut(),
                    0,
                    [feature_level].as_ptr(),
                    1,
                    d3d11::D3D11_SDK_VERSION,
                    &mut device as *mut *mut _ as *mut *mut _,
                    ptr::null_mut(),
                    ptr::null_mut(),
                );

                if !winerror::SUCCEEDED(hr) {
                    continue;
                }

                (
                    unsafe { ComPtr::<d3d11::ID3D11Device>::from_raw(device) },
                    feature_level,
                )
            };

            let memory_properties = adapter::MemoryProperties {
                memory_types: vec![
                    adapter::MemoryType {
                        properties: Properties::DEVICE_LOCAL,
                        heap_index: 0,
                    },
                    adapter::MemoryType {
                        properties: Properties::CPU_VISIBLE
                            | Properties::COHERENT
                            | Properties::CPU_CACHED,
                        heap_index: 1,
                    },
                    adapter::MemoryType {
                        properties: Properties::CPU_VISIBLE | Properties::CPU_CACHED,
                        heap_index: 1,
                    },
                ],
                // TODO: would using *VideoMemory and *SystemMemory from
                //       DXGI_ADAPTER_DESC be too optimistic? :)
                memory_heaps: vec![!0, !0],
            };

            let limits = hal::Limits {
                max_image_1d_size: d3d11::D3D11_REQ_TEXTURE1D_U_DIMENSION as _,
                max_image_2d_size: d3d11::D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION as _,
                max_image_3d_size: d3d11::D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION as _,
                max_image_cube_size: d3d11::D3D11_REQ_TEXTURECUBE_DIMENSION as _,
                max_image_array_layers: d3d11::D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION as _,
                max_texel_elements: d3d11::D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION as _, //TODO
                max_patch_size: 0,                                                    // TODO
                max_viewports: d3d11::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE as _,
                max_viewport_dimensions: [d3d11::D3D11_VIEWPORT_BOUNDS_MAX; 2],
                max_framebuffer_extent: hal::image::Extent {
                    //TODO
                    width: 4096,
                    height: 4096,
                    depth: 1,
                },
                max_compute_work_group_count: [
                    d3d11::D3D11_CS_THREAD_GROUP_MAX_X,
                    d3d11::D3D11_CS_THREAD_GROUP_MAX_Y,
                    d3d11::D3D11_CS_THREAD_GROUP_MAX_Z,
                ],
                max_compute_work_group_size: [
                    d3d11::D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP,
                    1,
                    1,
                ], // TODO
                max_vertex_input_attribute_offset: 255, // TODO
                max_vertex_input_attributes: d3d11::D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT as _,
                max_vertex_input_binding_stride:
                    d3d11::D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES as _,
                max_vertex_input_bindings: d3d11::D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT as _, // TODO: verify same as attributes
                max_vertex_output_components: d3d11::D3D11_VS_OUTPUT_REGISTER_COUNT as _, // TODO
                min_texel_buffer_offset_alignment: 1,                                     // TODO
                min_uniform_buffer_offset_alignment: 16, // TODO: verify
                min_storage_buffer_offset_alignment: 1,  // TODO
                framebuffer_color_sample_counts: 1,      // TODO
                framebuffer_depth_sample_counts: 1,      // TODO
                framebuffer_stencil_sample_counts: 1,    // TODO
                max_color_attachments: d3d11::D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT as _,
                buffer_image_granularity: 1,
                non_coherent_atom_size: 1, // TODO
                max_sampler_anisotropy: 16.,
                optimal_buffer_copy_offset_alignment: 1, // TODO
                optimal_buffer_copy_pitch_alignment: 1,  // TODO
                min_vertex_input_binding_stride_alignment: 1,
                ..hal::Limits::default() //TODO
            };

            let features = get_features(device.clone(), feature_level);
            let format_properties = get_format_properties(device.clone());

            let physical_device = PhysicalDevice {
                adapter,
                library: Arc::clone(&self.library),
                features,
                limits,
                memory_properties,
                format_properties,
            };

            info!("{:#?}", info);

            adapters.push(adapter::Adapter {
                info,
                physical_device,
                queue_families: vec![QueueFamily],
            });
        }

        adapters
    }

    unsafe fn create_surface(
        &self,
        has_handle: &impl raw_window_handle::HasRawWindowHandle,
    ) -> Result<Surface, hal::window::InitError> {
        match has_handle.raw_window_handle() {
            raw_window_handle::RawWindowHandle::Windows(handle) => {
                Ok(self.create_surface_from_hwnd(handle.hwnd))
            }
            _ => Err(hal::window::InitError::UnsupportedWindowHandle),
        }
    }

    unsafe fn destroy_surface(&self, _surface: Surface) {
        // TODO: Implement Surface cleanup
    }
}

pub struct PhysicalDevice {
    adapter: ComPtr<IDXGIAdapter>,
    library: Arc<libloading::Library>,
    features: hal::Features,
    limits: hal::Limits,
    memory_properties: adapter::MemoryProperties,
    format_properties: [format::Properties; format::NUM_FORMATS],
}

impl fmt::Debug for PhysicalDevice {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("PhysicalDevice")
    }
}

unsafe impl Send for PhysicalDevice {}
unsafe impl Sync for PhysicalDevice {}

// TODO: does the adapter we get earlier matter for feature level?
fn get_feature_level(func: &CreateFun, adapter: *mut IDXGIAdapter) -> d3dcommon::D3D_FEATURE_LEVEL {
    let requested_feature_levels = [
        d3dcommon::D3D_FEATURE_LEVEL_11_1,
        d3dcommon::D3D_FEATURE_LEVEL_11_0,
        d3dcommon::D3D_FEATURE_LEVEL_10_1,
        d3dcommon::D3D_FEATURE_LEVEL_10_0,
        d3dcommon::D3D_FEATURE_LEVEL_9_3,
        d3dcommon::D3D_FEATURE_LEVEL_9_2,
        d3dcommon::D3D_FEATURE_LEVEL_9_1,
    ];

    let mut feature_level = d3dcommon::D3D_FEATURE_LEVEL_9_1;
    let hr = func(
        adapter,
        d3dcommon::D3D_DRIVER_TYPE_UNKNOWN,
        ptr::null_mut(),
        0,
        requested_feature_levels[..].as_ptr(),
        requested_feature_levels.len() as _,
        d3d11::D3D11_SDK_VERSION,
        ptr::null_mut(),
        &mut feature_level as *mut _,
        ptr::null_mut(),
    );

    if !winerror::SUCCEEDED(hr) {
        // if there is no 11.1 runtime installed, requesting
        // `D3D_FEATURE_LEVEL_11_1` will return E_INVALIDARG so we just retry
        // without that
        if hr == winerror::E_INVALIDARG {
            let hr = func(
                adapter,
                d3dcommon::D3D_DRIVER_TYPE_UNKNOWN,
                ptr::null_mut(),
                0,
                requested_feature_levels[1 ..].as_ptr(),
                (requested_feature_levels.len() - 1) as _,
                d3d11::D3D11_SDK_VERSION,
                ptr::null_mut(),
                &mut feature_level as *mut _,
                ptr::null_mut(),
            );

            if !winerror::SUCCEEDED(hr) {
                // TODO: device might not support any feature levels?
                unimplemented!();
            }
        }
    }

    feature_level
}

// TODO: PhysicalDevice
impl adapter::PhysicalDevice<Backend> for PhysicalDevice {
    unsafe fn open(
        &self,
        families: &[(&QueueFamily, &[queue::QueuePriority])],
        requested_features: hal::Features,
    ) -> Result<adapter::Gpu<Backend>, hal::device::CreationError> {
        let func: libloading::Symbol<CreateFun> = self.library
            .get(b"D3D11CreateDevice")
            .unwrap();

        let (device, cxt) = {
            if !self.features().contains(requested_features) {
                return Err(hal::device::CreationError::MissingFeature);
            }

            let feature_level = get_feature_level(&func, self.adapter.as_raw());
            let mut returned_level = d3dcommon::D3D_FEATURE_LEVEL_9_1;

            #[cfg(debug_assertions)]
            let create_flags = d3d11::D3D11_CREATE_DEVICE_DEBUG;
            #[cfg(not(debug_assertions))]
            let create_flags = 0;

            // TODO: request debug device only on debug config?
            let mut device = ptr::null_mut();
            let mut cxt = ptr::null_mut();
            let hr = func(
                self.adapter.as_raw() as *mut _,
                d3dcommon::D3D_DRIVER_TYPE_UNKNOWN,
                ptr::null_mut(),
                create_flags,
                [feature_level].as_ptr(),
                1,
                d3d11::D3D11_SDK_VERSION,
                &mut device as *mut *mut _ as *mut *mut _,
                &mut returned_level as *mut _,
                &mut cxt as *mut *mut _ as *mut *mut _,
            );

            // NOTE: returns error if adapter argument is non-null and driver
            // type is not unknown; or if debug device is requested but not
            // present
            if !winerror::SUCCEEDED(hr) {
                return Err(hal::device::CreationError::InitializationFailed);
            }

            info!("feature level={:x}", feature_level);

            (ComPtr::from_raw(device), ComPtr::from_raw(cxt))
        };

        let device = device::Device::new(device, cxt, self.memory_properties.clone());

        // TODO: deferred context => 1 cxt/queue?
        let queue_groups = families
            .into_iter()
            .map(|&(_family, prio)| {
                assert_eq!(prio.len(), 1);
                let mut group = queue::QueueGroup::new(queue::QueueFamilyId(0));

                // TODO: multiple queues?
                let queue = CommandQueue {
                    context: device.context.clone(),
                };
                group.add_queue(queue);
                group
            })
            .collect();

        Ok(adapter::Gpu {
            device,
            queue_groups,
        })
    }

    fn format_properties(&self, fmt: Option<format::Format>) -> format::Properties {
        let idx = fmt.map(|fmt| fmt as usize).unwrap_or(0);
        self.format_properties[idx]
    }

    fn image_format_properties(
        &self,
        format: format::Format,
        dimensions: u8,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Option<image::FormatProperties> {
        conv::map_format(format)?; //filter out unknown formats

        let supported_usage = {
            use hal::image::Usage as U;
            let format_props = &self.format_properties[format as usize];
            let props = match tiling {
                image::Tiling::Optimal => format_props.optimal_tiling,
                image::Tiling::Linear => format_props.linear_tiling,
            };
            let mut flags = U::empty();
            // Note: these checks would have been nicer if we had explicit BLIT usage
            if props.contains(format::ImageFeature::BLIT_SRC) {
                flags |= U::TRANSFER_SRC;
            }
            if props.contains(format::ImageFeature::BLIT_DST) {
                flags |= U::TRANSFER_DST;
            }
            if props.contains(format::ImageFeature::SAMPLED) {
                flags |= U::SAMPLED;
            }
            if props.contains(format::ImageFeature::STORAGE) {
                flags |= U::STORAGE;
            }
            if props.contains(format::ImageFeature::COLOR_ATTACHMENT) {
                flags |= U::COLOR_ATTACHMENT;
            }
            if props.contains(format::ImageFeature::DEPTH_STENCIL_ATTACHMENT) {
                flags |= U::DEPTH_STENCIL_ATTACHMENT;
            }
            flags
        };
        if !supported_usage.contains(usage) {
            return None;
        }

        let max_resource_size =
            (d3d11::D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM as usize) << 20;
        Some(match tiling {
            image::Tiling::Optimal => image::FormatProperties {
                max_extent: match dimensions {
                    1 => image::Extent {
                        width: d3d11::D3D11_REQ_TEXTURE1D_U_DIMENSION,
                        height: 1,
                        depth: 1,
                    },
                    2 => image::Extent {
                        width: d3d11::D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                        height: d3d11::D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                        depth: 1,
                    },
                    3 => image::Extent {
                        width: d3d11::D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION,
                        height: d3d11::D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION,
                        depth: d3d11::D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION,
                    },
                    _ => return None,
                },
                max_levels: d3d11::D3D11_REQ_MIP_LEVELS as _,
                max_layers: match dimensions {
                    1 => d3d11::D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION as _,
                    2 => d3d11::D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION as _,
                    _ => return None,
                },
                sample_count_mask: if dimensions == 2
                    && !view_caps.contains(image::ViewCapabilities::KIND_CUBE)
                    && (usage.contains(image::Usage::COLOR_ATTACHMENT)
                        | usage.contains(image::Usage::DEPTH_STENCIL_ATTACHMENT))
                {
                    0x3F //TODO: use D3D12_FEATURE_DATA_FORMAT_SUPPORT
                } else {
                    0x1
                },
                max_resource_size,
            },
            image::Tiling::Linear => image::FormatProperties {
                max_extent: match dimensions {
                    2 => image::Extent {
                        width: d3d11::D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                        height: d3d11::D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                        depth: 1,
                    },
                    _ => return None,
                },
                max_levels: 1,
                max_layers: 1,
                sample_count_mask: 0x1,
                max_resource_size,
            },
        })
    }

    fn memory_properties(&self) -> adapter::MemoryProperties {
        self.memory_properties.clone()
    }

    fn features(&self) -> hal::Features {
        self.features
    }

    fn limits(&self) -> Limits {
        self.limits
    }
}

struct Presentation {
    swapchain: ComPtr<IDXGISwapChain>,
    view: ComPtr<d3d11::ID3D11RenderTargetView>,
    format: format::Format,
    size: window::Extent2D,
}

pub struct Surface {
    pub(crate) factory: ComPtr<IDXGIFactory>,
    wnd_handle: HWND,
    presentation: Option<Presentation>,
}


impl fmt::Debug for Surface {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Surface")
    }
}

unsafe impl Send for Surface {}
unsafe impl Sync for Surface {}

impl window::Surface<Backend> for Surface {
    fn supports_queue_family(&self, _queue_family: &QueueFamily) -> bool {
        true
    }

    fn capabilities(&self, _physical_device: &PhysicalDevice) -> window::SurfaceCapabilities {
        let current_extent = unsafe {
            let mut rect: RECT = mem::zeroed();
            assert_ne!(
                0,
                GetClientRect(self.wnd_handle as *mut _, &mut rect as *mut RECT)
            );
            Some(window::Extent2D {
                width: (rect.right - rect.left) as u32,
                height: (rect.bottom - rect.top) as u32,
            })
        };

        // TODO: flip swap effects require dx11.1/windows8
        // NOTE: some swap effects affect msaa capabilities..
        // TODO: _DISCARD swap effects can only have one image?
        window::SurfaceCapabilities {
            present_modes: window::PresentMode::FIFO, //TODO
            composite_alpha_modes: window::CompositeAlphaMode::OPAQUE, //TODO
            image_count: 1 ..= 16, // TODO:
            current_extent,
            extents: window::Extent2D {
                width: 16,
                height: 16,
            } ..= window::Extent2D {
                width: 4096,
                height: 4096,
            },
            max_image_layers: 1,
            usage: image::Usage::COLOR_ATTACHMENT | image::Usage::TRANSFER_SRC,
        }
    }

    fn supported_formats(&self, _physical_device: &PhysicalDevice) -> Option<Vec<format::Format>> {
         Some(vec![
            format::Format::Bgra8Srgb,
            format::Format::Bgra8Unorm,
            format::Format::Rgba8Srgb,
            format::Format::Rgba8Unorm,
            format::Format::A2b10g10r10Unorm,
            format::Format::Rgba16Sfloat,
        ])
    }
}

impl window::PresentationSurface<Backend> for Surface {
    type SwapchainImage = ImageView;

    unsafe fn configure_swapchain(
        &mut self,
        device: &device::Device,
        config: window::SwapchainConfig,
    ) -> Result<(), window::CreationError> {
        assert!(image::Usage::COLOR_ATTACHMENT.contains(config.image_usage));

        let swapchain = match self.presentation.take() {
            Some(present) => {
                if present.format == config.format && present.size == config.extent {
                    self.presentation = Some(present);
                    return Ok(());
                }
                let non_srgb_format = conv::map_format_nosrgb(config.format).unwrap();
                drop(present.view);
                let result = present.swapchain.ResizeBuffers(
                    config.image_count,
                    config.extent.width,
                    config.extent.height,
                    non_srgb_format,
                    0,
                );
                if result != winerror::S_OK {
                    error!("ResizeBuffers failed with 0x{:x}", result as u32);
                    return Err(window::CreationError::WindowInUse(hal::device::WindowInUse));
                }
                present.swapchain
            }
            None => {
                let (swapchain, _) =
                    device.create_swapchain_impl(&config, self.wnd_handle, self.factory.clone())?;
                swapchain
            }
        };

        let mut resource: *mut d3d11::ID3D11Resource = ptr::null_mut();
        assert_eq!(
            winerror::S_OK,
            swapchain.GetBuffer(
                0 as _,
                &d3d11::ID3D11Resource::uuidof(),
                &mut resource as *mut *mut _ as *mut *mut _,
            )
        );

        let kind = image::Kind::D2(config.extent.width, config.extent.height, 1, 1);
        let format = conv::map_format(config.format).unwrap();
        let decomposed = conv::DecomposedDxgiFormat::from_dxgi_format(format);

        let view_info = ViewInfo {
            resource,
            kind,
            caps: image::ViewCapabilities::empty(),
            view_kind: image::ViewKind::D2,
            format: decomposed.rtv.unwrap(),
            range: image::SubresourceRange {
                aspects: format::Aspects::COLOR,
                levels: 0 .. 1,
                layers: 0 .. 1,
            },
        };
        let view = device.view_image_as_render_target(&view_info).unwrap();

        (*resource).Release();

        self.presentation = Some(Presentation {
            swapchain,
            view,
            format: config.format,
            size: config.extent,
        });
        Ok(())
    }

    unsafe fn unconfigure_swapchain(&mut self, _device: &device::Device) {
        self.presentation = None;
    }

    unsafe fn acquire_image(
        &mut self,
        _timeout_ns: u64, //TODO: use the timeout
    ) -> Result<(ImageView, Option<window::Suboptimal>), window::AcquireError> {
        let present = self.presentation.as_ref().unwrap();
        let image_view = ImageView {
            format: present.format,
            rtv_handle: Some(present.view.clone()),
            dsv_handle: None,
            srv_handle: None,
            uav_handle: None,
        };
        Ok((image_view, None))
    }
}


pub struct Swapchain {
    dxgi_swapchain: ComPtr<IDXGISwapChain>,
}


impl fmt::Debug for Swapchain {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Swapchain")
    }
}

unsafe impl Send for Swapchain {}
unsafe impl Sync for Swapchain {}

impl window::Swapchain<Backend> for Swapchain {
    unsafe fn acquire_image(
        &mut self,
        _timeout_ns: u64,
        _semaphore: Option<&Semaphore>,
        _fence: Option<&Fence>,
    ) -> Result<(window::SwapImageIndex, Option<window::Suboptimal>), window::AcquireError> {
        // TODO: non-`_DISCARD` swap effects have more than one buffer, `FLIP`
        //       effects are dxgi 1.3 (w10+?) in which case there is
        //       `GetCurrentBackBufferIndex()` on the swapchain
        Ok((0, None))
    }
}

#[derive(Debug, Clone, Copy)]
pub struct QueueFamily;

impl queue::QueueFamily for QueueFamily {
    fn queue_type(&self) -> queue::QueueType {
        queue::QueueType::General
    }
    fn max_queues(&self) -> usize {
        1
    }
    fn id(&self) -> queue::QueueFamilyId {
        queue::QueueFamilyId(0)
    }
}

#[derive(Clone)]
pub struct CommandQueue {
    context: ComPtr<d3d11::ID3D11DeviceContext>,
}

impl fmt::Debug for CommandQueue {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("CommandQueue")
    }
}

unsafe impl Send for CommandQueue {}
unsafe impl Sync for CommandQueue {}

impl queue::CommandQueue<Backend> for CommandQueue {
    unsafe fn submit<'a, T, Ic, S, Iw, Is>(
        &mut self,
        submission: queue::Submission<Ic, Iw, Is>,
        fence: Option<&Fence>,
    ) where
        T: 'a + Borrow<CommandBuffer>,
        Ic: IntoIterator<Item = &'a T>,
        S: 'a + Borrow<Semaphore>,
        Iw: IntoIterator<Item = (&'a S, pso::PipelineStage)>,
        Is: IntoIterator<Item = &'a S>,
    {
        let _scope = debug_scope!(&self.context, "Submit(fence={:?})", fence);
        for cmd_buf in submission.command_buffers {
            let cmd_buf = cmd_buf.borrow();

            let _scope = debug_scope!(
                &self.context,
                "CommandBuffer ({}/{})",
                cmd_buf.flush_coherent_memory.len(),
                cmd_buf.invalidate_coherent_memory.len()
            );

            {
                let _scope = debug_scope!(&self.context, "Pre-Exec: Flush");
                for sync in &cmd_buf.flush_coherent_memory {
                    sync.do_flush(&self.context);
                }
            }
            self.context
                .ExecuteCommandList(cmd_buf.as_raw_list().as_raw(), FALSE);
            {
                let _scope = debug_scope!(&self.context, "Post-Exec: Invalidate");
                for sync in &cmd_buf.invalidate_coherent_memory {
                    sync.do_invalidate(&self.context);
                }
            }
        }

        if let Some(fence) = fence {
            *fence.mutex.lock() = true;
            fence.condvar.notify_all();
        }
    }

    unsafe fn present<'a, W, Is, S, Iw>(
        &mut self,
        swapchains: Is,
        _wait_semaphores: Iw,
    ) -> Result<Option<window::Suboptimal>, window::PresentError>
    where
        W: 'a + Borrow<Swapchain>,
        Is: IntoIterator<Item = (&'a W, window::SwapImageIndex)>,
        S: 'a + Borrow<Semaphore>,
        Iw: IntoIterator<Item = &'a S>,
    {
        for (swapchain, _idx) in swapchains {
            swapchain.borrow().dxgi_swapchain.Present(1, 0);
        }

        Ok(None)
    }

    unsafe fn present_surface(
        &mut self,
        surface: &mut Surface,
        _image: ImageView,
        _wait_semaphore: Option<&Semaphore>,
    ) -> Result<Option<window::Suboptimal>, window::PresentError> {
        surface
            .presentation
            .as_ref()
            .unwrap()
            .swapchain
            .Present(1, 0);
        Ok(None)
    }

    fn wait_idle(&self) -> Result<(), hal::device::OutOfMemory> {
        // unimplemented!()
        Ok(())
    }
}

#[derive(Debug)]
pub struct AttachmentClear {
    subpass_id: Option<pass::SubpassId>,
    attachment_id: usize,
    raw: command::AttachmentClear,
}

#[derive(Debug)]
pub struct RenderPassCache {
    pub render_pass: RenderPass,
    pub framebuffer: Framebuffer,
    pub attachment_clear_values: Vec<AttachmentClear>,
    pub target_rect: pso::Rect,
    pub current_subpass: usize,
}

impl RenderPassCache {
    pub fn start_subpass(
        &mut self,
        internal: &mut internal::Internal,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        cache: &mut CommandBufferState,
    ) {
        let attachments = self
            .attachment_clear_values
            .iter()
            .filter(|clear| clear.subpass_id == Some(self.current_subpass))
            .map(|clear| clear.raw);

        cache
            .dirty_flag
            .insert(DirtyStateFlag::GRAPHICS_PIPELINE | DirtyStateFlag::VIEWPORTS);
        internal.clear_attachments(
            context,
            attachments,
            &[pso::ClearRect {
                rect: self.target_rect,
                layers: 0 .. 1,
            }],
            &self,
        );

        let subpass = &self.render_pass.subpasses[self.current_subpass];
        let color_views = subpass
            .color_attachments
            .iter()
            .map(|&(id, _)| {
                self.framebuffer.attachments[id]
                    .rtv_handle
                    .clone()
                    .unwrap()
                    .as_raw()
            })
            .collect::<Vec<_>>();
        let ds_view = match subpass.depth_stencil_attachment {
            Some((id, _)) => Some(
                self.framebuffer.attachments[id]
                    .dsv_handle
                    .clone()
                    .unwrap()
                    .as_raw(),
            ),
            None => None,
        };

        cache.set_render_targets(&color_views, ds_view);
        cache.bind(context);
    }

    pub fn next_subpass(&mut self) {
        self.current_subpass += 1;
    }
}

bitflags! {
    struct DirtyStateFlag : u32 {
        const RENDER_TARGETS = (1 << 1);
        const VERTEX_BUFFERS = (1 << 2);
        const GRAPHICS_PIPELINE = (1 << 3);
        const VIEWPORTS = (1 << 4);
        const BLEND_STATE = (1 << 5);
    }
}

pub struct CommandBufferState {
    dirty_flag: DirtyStateFlag,

    render_target_len: u32,
    render_targets: [*mut d3d11::ID3D11RenderTargetView; 8],
    depth_target: Option<*mut d3d11::ID3D11DepthStencilView>,
    graphics_pipeline: Option<GraphicsPipeline>,

    // a bitmask that keeps track of what vertex buffer bindings have been "bound" into
    // our vec
    bound_bindings: u32,
    // a bitmask that hold the required binding slots to be bound for the currently
    // bound pipeline
    required_bindings: Option<u32>,
    // the highest binding number in currently bound pipeline
    max_bindings: Option<u32>,
    viewports: Vec<d3d11::D3D11_VIEWPORT>,
    vertex_buffers: Vec<*mut d3d11::ID3D11Buffer>,
    vertex_offsets: Vec<u32>,
    vertex_strides: Vec<u32>,
    blend_factor: Option<[f32; 4]>,
    // we can only support one face (rather, both faces must have the same value)
    stencil_ref: Option<pso::StencilValue>,
    stencil_read_mask: Option<pso::StencilValue>,
    stencil_write_mask: Option<pso::StencilValue>,
    current_blend: Option<*mut d3d11::ID3D11BlendState>,
}


impl fmt::Debug for CommandBufferState {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("CommandBufferState")
    }
}

impl CommandBufferState {
    fn new() -> Self {
        CommandBufferState {
            dirty_flag: DirtyStateFlag::empty(),
            render_target_len: 0,
            render_targets: [ptr::null_mut(); 8],
            depth_target: None,
            graphics_pipeline: None,
            bound_bindings: 0,
            required_bindings: None,
            max_bindings: None,
            viewports: Vec::new(),
            vertex_buffers: Vec::new(),
            vertex_offsets: Vec::new(),
            vertex_strides: Vec::new(),
            blend_factor: None,
            stencil_ref: None,
            stencil_read_mask: None,
            stencil_write_mask: None,
            current_blend: None,
        }
    }

    fn clear(&mut self) {
        self.render_target_len = 0;
        self.depth_target = None;
        self.graphics_pipeline = None;
        self.bound_bindings = 0;
        self.required_bindings = None;
        self.max_bindings = None;
        self.viewports.clear();
        self.vertex_buffers.clear();
        self.vertex_offsets.clear();
        self.vertex_strides.clear();
        self.blend_factor = None;
        self.stencil_ref = None;
        self.stencil_read_mask = None;
        self.stencil_write_mask = None;
        self.current_blend = None;
    }

    pub fn set_vertex_buffer(
        &mut self,
        index: usize,
        offset: u32,
        buffer: *mut d3d11::ID3D11Buffer,
    ) {
        self.bound_bindings |= 1 << index as u32;

        if index >= self.vertex_buffers.len() {
            self.vertex_buffers.push(buffer);
            self.vertex_offsets.push(offset);
        } else {
            self.vertex_buffers[index] = buffer;
            self.vertex_offsets[index] = offset;
        }

        self.dirty_flag.insert(DirtyStateFlag::VERTEX_BUFFERS);
    }

    pub fn bind_vertex_buffers(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        if let Some(binding_count) = self.max_bindings {
            if self.vertex_buffers.len() >= binding_count as usize
                && self.vertex_strides.len() >= binding_count as usize
            {
                unsafe {
                    context.IASetVertexBuffers(
                        0,
                        binding_count,
                        self.vertex_buffers.as_ptr(),
                        self.vertex_strides.as_ptr(),
                        self.vertex_offsets.as_ptr(),
                    );
                }

                self.dirty_flag.remove(DirtyStateFlag::VERTEX_BUFFERS);
            }
        }
    }

    pub fn set_viewports(&mut self, viewports: &[d3d11::D3D11_VIEWPORT]) {
        self.viewports.clear();
        self.viewports.extend(viewports);

        self.dirty_flag.insert(DirtyStateFlag::VIEWPORTS);
    }

    pub fn bind_viewports(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        if let Some(ref pipeline) = self.graphics_pipeline {
            if let Some(ref viewport) = pipeline.baked_states.viewport {
                unsafe {
                    context.RSSetViewports(1, [conv::map_viewport(&viewport)].as_ptr());
                }
            } else {
                unsafe {
                    context.RSSetViewports(self.viewports.len() as u32, self.viewports.as_ptr());
                }
            }
        } else {
            unsafe {
                context.RSSetViewports(self.viewports.len() as u32, self.viewports.as_ptr());
            }
        }

        self.dirty_flag.remove(DirtyStateFlag::VIEWPORTS);
    }

    pub fn set_render_targets(
        &mut self,
        render_targets: &[*mut d3d11::ID3D11RenderTargetView],
        depth_target: Option<*mut d3d11::ID3D11DepthStencilView>,
    ) {
        for (idx, &rt) in render_targets.iter().enumerate() {
            self.render_targets[idx] = rt;
        }

        self.render_target_len = render_targets.len() as u32;
        self.depth_target = depth_target;

        self.dirty_flag.insert(DirtyStateFlag::RENDER_TARGETS);
    }

    pub fn bind_render_targets(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        unsafe {
            context.OMSetRenderTargets(
                self.render_target_len,
                self.render_targets.as_ptr(),
                if let Some(dsv) = self.depth_target {
                    dsv
                } else {
                    ptr::null_mut()
                },
            );
        }

        self.dirty_flag.remove(DirtyStateFlag::RENDER_TARGETS);
    }

    pub fn set_blend_factor(&mut self, factor: [f32; 4]) {
        self.blend_factor = Some(factor);

        self.dirty_flag.insert(DirtyStateFlag::BLEND_STATE);
    }

    pub fn bind_blend_state(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        if let Some(blend) = self.current_blend {
            let blend_color = if let Some(ref pipeline) = self.graphics_pipeline {
                pipeline
                    .baked_states
                    .blend_color
                    .or(self.blend_factor)
                    .unwrap_or([0f32; 4])
            } else {
                self.blend_factor.unwrap_or([0f32; 4])
            };

            // TODO: MSAA
            unsafe {
                context.OMSetBlendState(blend, &blend_color, !0);
            }

            self.dirty_flag.remove(DirtyStateFlag::BLEND_STATE);
        }
    }

    pub fn set_graphics_pipeline(&mut self, pipeline: GraphicsPipeline) {
        self.graphics_pipeline = Some(pipeline);

        self.dirty_flag.insert(DirtyStateFlag::GRAPHICS_PIPELINE);
    }

    pub fn bind_graphics_pipeline(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        if let Some(ref pipeline) = self.graphics_pipeline {
            self.vertex_strides.clear();
            self.vertex_strides.extend(&pipeline.strides);

            self.required_bindings = Some(pipeline.required_bindings);
            self.max_bindings = Some(pipeline.max_vertex_bindings);
        };

        self.bind_vertex_buffers(context);

        if let Some(ref pipeline) = self.graphics_pipeline {
            unsafe {
                context.IASetPrimitiveTopology(pipeline.topology);
                context.IASetInputLayout(pipeline.input_layout.as_raw());

                context.VSSetShader(pipeline.vs.as_raw(), ptr::null_mut(), 0);
                if let Some(ref ps) = pipeline.ps {
                    context.PSSetShader(ps.as_raw(), ptr::null_mut(), 0);
                }
                if let Some(ref gs) = pipeline.gs {
                    context.GSSetShader(gs.as_raw(), ptr::null_mut(), 0);
                }
                if let Some(ref hs) = pipeline.hs {
                    context.HSSetShader(hs.as_raw(), ptr::null_mut(), 0);
                }
                if let Some(ref ds) = pipeline.ds {
                    context.DSSetShader(ds.as_raw(), ptr::null_mut(), 0);
                }

                context.RSSetState(pipeline.rasterizer_state.as_raw());
                if let Some(ref viewport) = pipeline.baked_states.viewport {
                    context.RSSetViewports(1, [conv::map_viewport(&viewport)].as_ptr());
                }
                if let Some(ref scissor) = pipeline.baked_states.scissor {
                    context.RSSetScissorRects(1, [conv::map_rect(&scissor)].as_ptr());
                }

                if let Some((ref state, reference)) = pipeline.depth_stencil_state {
                    let stencil_ref = if let pso::State::Static(reference) = reference {
                        reference
                    } else {
                        self.stencil_ref.unwrap_or(0)
                    };

                    context.OMSetDepthStencilState(state.as_raw(), stencil_ref);
                }
                self.current_blend = Some(pipeline.blend_state.as_raw());
            }
        };

        self.bind_blend_state(context);

        self.dirty_flag.remove(DirtyStateFlag::GRAPHICS_PIPELINE);
    }

    pub fn bind(&mut self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        if self.dirty_flag.contains(DirtyStateFlag::RENDER_TARGETS) {
            self.bind_render_targets(context);
        }

        if self.dirty_flag.contains(DirtyStateFlag::GRAPHICS_PIPELINE) {
            self.bind_graphics_pipeline(context);
        }

        if self.dirty_flag.contains(DirtyStateFlag::VERTEX_BUFFERS) {
            self.bind_vertex_buffers(context);
        }

        if self.dirty_flag.contains(DirtyStateFlag::VIEWPORTS) {
            self.bind_viewports(context);
        }
    }
}

pub struct CommandBuffer {
    // TODO: better way of sharing
    internal: internal::Internal,
    context: ComPtr<d3d11::ID3D11DeviceContext>,
    list: RefCell<Option<ComPtr<d3d11::ID3D11CommandList>>>,

    // since coherent memory needs to be synchronized at submission, we need to gather up all
    // coherent resources that are used in the command buffer and flush/invalidate them accordingly
    // before executing.
    flush_coherent_memory: Vec<MemoryFlush>,
    invalidate_coherent_memory: Vec<MemoryInvalidate>,

    // holds information about the active render pass
    render_pass_cache: Option<RenderPassCache>,

    cache: CommandBufferState,

    one_time_submit: bool,
}

impl fmt::Debug for CommandBuffer {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("CommandBuffer")
    }
}

unsafe impl Send for CommandBuffer {}
unsafe impl Sync for CommandBuffer {}

impl CommandBuffer {
    fn create_deferred(device: ComPtr<d3d11::ID3D11Device>, internal: internal::Internal) -> Self {
        let mut context: *mut d3d11::ID3D11DeviceContext = ptr::null_mut();
        let hr =
            unsafe { device.CreateDeferredContext(0, &mut context as *mut *mut _ as *mut *mut _) };
        assert_eq!(hr, winerror::S_OK);

        CommandBuffer {
            internal,
            context: unsafe { ComPtr::from_raw(context) },
            list: RefCell::new(None),
            flush_coherent_memory: Vec::new(),
            invalidate_coherent_memory: Vec::new(),
            render_pass_cache: None,
            cache: CommandBufferState::new(),
            one_time_submit: false,
        }
    }

    fn as_raw_list(&self) -> ComPtr<d3d11::ID3D11CommandList> {
        if self.one_time_submit {
            self.list.replace(None).unwrap()
        } else {
            self.list.borrow().clone().unwrap()
        }
    }

    unsafe fn bind_vertex_descriptor(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        binding: &PipelineBinding,
        handles: *mut Descriptor,
    ) {
        use pso::DescriptorType::*;

        let handles = handles.offset(binding.handle_offset as isize);
        let start = binding.binding_range.start as UINT;
        let len = binding.binding_range.end as UINT - start;

        match binding.ty {
            Sampler => context.VSSetSamplers(start, len, handles as *const *mut _ as *const *mut _),
            SampledImage | InputAttachment => {
                context.VSSetShaderResources(start, len, handles as *const *mut _ as *const *mut _)
            }
            CombinedImageSampler => {
                context.VSSetShaderResources(start, len, handles as *const *mut _ as *const *mut _);
                context.VSSetSamplers(
                    start,
                    len,
                    handles.offset(1) as *const *mut _ as *const *mut _,
                );
            }
            UniformBuffer | UniformBufferDynamic => {
                context.VSSetConstantBuffers(start, len, handles as *const *mut _ as *const *mut _)
            }
            _ => {}
        }
    }

    unsafe fn bind_fragment_descriptor(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        binding: &PipelineBinding,
        handles: *mut Descriptor,
    ) {
        use pso::DescriptorType::*;

        let handles = handles.offset(binding.handle_offset as isize);
        let start = binding.binding_range.start as UINT;
        let len = binding.binding_range.end as UINT - start;

        match binding.ty {
            Sampler => context.PSSetSamplers(start, len, handles as *const *mut _ as *const *mut _),
            SampledImage | InputAttachment => {
                context.PSSetShaderResources(start, len, handles as *const *mut _ as *const *mut _)
            }
            CombinedImageSampler => {
                context.PSSetShaderResources(start, len, handles as *const *mut _ as *const *mut _);
                context.PSSetSamplers(
                    start,
                    len,
                    handles.offset(1) as *const *mut _ as *const *mut _,
                );
            }
            UniformBuffer | UniformBufferDynamic => {
                context.PSSetConstantBuffers(start, len, handles as *const *mut _ as *const *mut _)
            }
            _ => {}
        }
    }

    unsafe fn bind_compute_descriptor(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        binding: &PipelineBinding,
        handles: *mut Descriptor,
    ) {
        use pso::DescriptorType::*;

        let handles = handles.offset(binding.handle_offset as isize);
        let start = binding.binding_range.start as UINT;
        let len = binding.binding_range.end as UINT - start;

        match binding.ty {
            Sampler => context.CSSetSamplers(start, len, handles as *const *mut _ as *const *mut _),
            SampledImage | InputAttachment => {
                context.CSSetShaderResources(start, len, handles as *const *mut _ as *const *mut _)
            }
            CombinedImageSampler => {
                context.CSSetShaderResources(start, len, handles as *const *mut _ as *const *mut _);
                context.CSSetSamplers(
                    start,
                    len,
                    handles.offset(1) as *const *mut _ as *const *mut _,
                );
            }
            UniformBuffer | UniformBufferDynamic => {
                context.CSSetConstantBuffers(start, len, handles as *const *mut _ as *const *mut _)
            }
            StorageImage | StorageBuffer => context.CSSetUnorderedAccessViews(
                start,
                len,
                handles as *const *mut _ as *const *mut _,
                ptr::null_mut(),
            ),
            _ => unimplemented!(),
        }
    }

    fn bind_descriptor(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        binding: &PipelineBinding,
        handles: *mut Descriptor,
    ) {
        //use pso::ShaderStageFlags::*;

        unsafe {
            if binding.stage.contains(pso::ShaderStageFlags::VERTEX) {
                self.bind_vertex_descriptor(context, binding, handles);
            }

            if binding.stage.contains(pso::ShaderStageFlags::FRAGMENT) {
                self.bind_fragment_descriptor(context, binding, handles);
            }

            if binding.stage.contains(pso::ShaderStageFlags::COMPUTE) {
                self.bind_compute_descriptor(context, binding, handles);
            }
        }
    }

    fn defer_coherent_flush(&mut self, buffer: &Buffer) {
        if !self
            .flush_coherent_memory
            .iter()
            .any(|m| m.buffer == buffer.internal.raw)
        {
            self.flush_coherent_memory.push(MemoryFlush {
                host_memory: buffer.host_ptr,
                sync_range: SyncRange::Whole,
                buffer: buffer.internal.raw,
            });
        }
    }

    fn defer_coherent_invalidate(&mut self, buffer: &Buffer) {
        if !self
            .invalidate_coherent_memory
            .iter()
            .any(|m| m.buffer == buffer.internal.raw)
        {
            self.invalidate_coherent_memory.push(MemoryInvalidate {
                working_buffer: Some(self.internal.working_buffer.clone()),
                working_buffer_size: self.internal.working_buffer_size,
                host_memory: buffer.host_ptr,
                sync_range: buffer.bound_range.clone(),
                buffer: buffer.internal.raw,
            });
        }
    }

    fn reset(&mut self) {
        self.flush_coherent_memory.clear();
        self.invalidate_coherent_memory.clear();
        self.render_pass_cache = None;
        self.cache.clear();
    }
}

impl command::CommandBuffer<Backend> for CommandBuffer {
    unsafe fn begin(
        &mut self,
        flags: command::CommandBufferFlags,
        _info: command::CommandBufferInheritanceInfo<Backend>,
    ) {
        self.one_time_submit = flags.contains(command::CommandBufferFlags::ONE_TIME_SUBMIT);
        self.reset();
    }

    unsafe fn finish(&mut self) {
        let mut list = ptr::null_mut();
        let hr = self
            .context
            .FinishCommandList(FALSE, &mut list as *mut *mut _ as *mut *mut _);
        assert_eq!(hr, winerror::S_OK);

        self.list.replace(Some(ComPtr::from_raw(list)));
    }

    unsafe fn reset(&mut self, _release_resources: bool) {
        self.reset();
    }

    unsafe fn begin_render_pass<T>(
        &mut self,
        render_pass: &RenderPass,
        framebuffer: &Framebuffer,
        target_rect: pso::Rect,
        clear_values: T,
        _first_subpass: command::SubpassContents,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ClearValue>,
    {
        use pass::AttachmentLoadOp as Alo;

        let mut clear_iter = clear_values.into_iter();
        let mut attachment_clears = Vec::new();

        for (idx, attachment) in render_pass.attachments.iter().enumerate() {
            //let attachment = render_pass.attachments[attachment_ref];
            let format = attachment.format.unwrap();

            let subpass_id = render_pass.subpasses.iter().position(|sp| sp.is_using(idx));

            if attachment.has_clears() {
                let value = *clear_iter.next().unwrap().borrow();

                match (attachment.ops.load, attachment.stencil_ops.load) {
                    (Alo::Clear, Alo::Clear) if format.is_depth() => {
                        attachment_clears.push(AttachmentClear {
                            subpass_id,
                            attachment_id: idx,
                            raw: command::AttachmentClear::DepthStencil {
                                depth: Some(value.depth_stencil.depth),
                                stencil: Some(value.depth_stencil.stencil),
                            },
                        });
                    }
                    (Alo::Clear, Alo::Clear) => {
                        attachment_clears.push(AttachmentClear {
                            subpass_id,
                            attachment_id: idx,
                            raw: command::AttachmentClear::Color {
                                index: idx,
                                value: value.color,
                            },
                        });

                        attachment_clears.push(AttachmentClear {
                            subpass_id,
                            attachment_id: idx,
                            raw: command::AttachmentClear::DepthStencil {
                                depth: None,
                                stencil: Some(value.depth_stencil.stencil),
                            },
                        });
                    }
                    (Alo::Clear, _) if format.is_depth() => {
                        attachment_clears.push(AttachmentClear {
                            subpass_id,
                            attachment_id: idx,
                            raw: command::AttachmentClear::DepthStencil {
                                depth: Some(value.depth_stencil.depth),
                                stencil: None,
                            },
                        });
                    }
                    (Alo::Clear, _) => {
                        attachment_clears.push(AttachmentClear {
                            subpass_id,
                            attachment_id: idx,
                            raw: command::AttachmentClear::Color {
                                index: idx,
                                value: value.color,
                            },
                        });
                    }
                    (_, Alo::Clear) => {
                        attachment_clears.push(AttachmentClear {
                            subpass_id,
                            attachment_id: idx,
                            raw: command::AttachmentClear::DepthStencil {
                                depth: None,
                                stencil: Some(value.depth_stencil.stencil),
                            },
                        });
                    }
                    _ => {}
                }
            }
        }

        self.render_pass_cache = Some(RenderPassCache {
            render_pass: render_pass.clone(),
            framebuffer: framebuffer.clone(),
            attachment_clear_values: attachment_clears,
            target_rect,
            current_subpass: 0,
        });

        if let Some(ref mut current_render_pass) = self.render_pass_cache {
            current_render_pass.start_subpass(&mut self.internal, &self.context, &mut self.cache);
        }
    }

    unsafe fn next_subpass(&mut self, _contents: command::SubpassContents) {
        if let Some(ref mut current_render_pass) = self.render_pass_cache {
            // TODO: resolve msaa
            current_render_pass.next_subpass();
            current_render_pass.start_subpass(&mut self.internal, &self.context, &mut self.cache);
        }
    }

    unsafe fn end_render_pass(&mut self) {
        self.context
            .OMSetRenderTargets(8, [ptr::null_mut(); 8].as_ptr(), ptr::null_mut());

        self.render_pass_cache = None;
    }

    unsafe fn pipeline_barrier<'a, T>(
        &mut self,
        _stages: Range<pso::PipelineStage>,
        _dependencies: memory::Dependencies,
        _barriers: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<memory::Barrier<'a, Backend>>,
    {
        // TODO: should we track and assert on resource states?
        // unimplemented!()
    }

    unsafe fn clear_image<T>(
        &mut self,
        image: &Image,
        _: image::Layout,
        value: command::ClearValue,
        subresource_ranges: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<image::SubresourceRange>,
    {
        for range in subresource_ranges {
            let range = range.borrow();

            // TODO: clear Int/Uint depending on format
            if range.aspects.contains(format::Aspects::COLOR) {
                for layer in range.layers.clone() {
                    for level in range.levels.clone() {
                        self.context.ClearRenderTargetView(
                            image.get_rtv(level, layer).unwrap().as_raw(),
                            &value.color.float32,
                        );
                    }
                }
            }

            let mut depth_stencil_flags = 0;
            if range.aspects.contains(format::Aspects::DEPTH) {
                depth_stencil_flags |= d3d11::D3D11_CLEAR_DEPTH;
            }

            if range.aspects.contains(format::Aspects::STENCIL) {
                depth_stencil_flags |= d3d11::D3D11_CLEAR_STENCIL;
            }

            if depth_stencil_flags != 0 {
                for layer in range.layers.clone() {
                    for level in range.levels.clone() {
                        self.context.ClearDepthStencilView(
                            image.get_dsv(level, layer).unwrap().as_raw(),
                            depth_stencil_flags,
                            value.depth_stencil.depth,
                            value.depth_stencil.stencil as _,
                        );
                    }
                }
            }
        }
    }

    unsafe fn clear_attachments<T, U>(&mut self, clears: T, rects: U)
    where
        T: IntoIterator,
        T::Item: Borrow<command::AttachmentClear>,
        U: IntoIterator,
        U::Item: Borrow<pso::ClearRect>,
    {
        if let Some(ref pass) = self.render_pass_cache {
            self.cache.dirty_flag.insert(
                DirtyStateFlag::GRAPHICS_PIPELINE
                    | DirtyStateFlag::VIEWPORTS
                    | DirtyStateFlag::RENDER_TARGETS,
            );
            self.internal
                .clear_attachments(&self.context, clears, rects, pass);
            self.cache.bind(&self.context);
        } else {
            panic!("`clear_attachments` can only be called inside a renderpass")
        }
    }

    unsafe fn resolve_image<T>(
        &mut self,
        _src: &Image,
        _src_layout: image::Layout,
        _dst: &Image,
        _dst_layout: image::Layout,
        _regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ImageResolve>,
    {
        unimplemented!()
    }

    unsafe fn blit_image<T>(
        &mut self,
        src: &Image,
        _src_layout: image::Layout,
        dst: &Image,
        _dst_layout: image::Layout,
        filter: image::Filter,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ImageBlit>,
    {
        self.cache
            .dirty_flag
            .insert(DirtyStateFlag::GRAPHICS_PIPELINE);

        self.internal
            .blit_2d_image(&self.context, src, dst, filter, regions);

        self.cache.bind(&self.context);
    }

    unsafe fn bind_index_buffer(&mut self, ibv: buffer::IndexBufferView<Backend>) {
        self.context.IASetIndexBuffer(
            ibv.buffer.internal.raw,
            conv::map_index_type(ibv.index_type),
            ibv.offset as u32,
        );
    }

    unsafe fn bind_vertex_buffers<I, T>(&mut self, first_binding: pso::BufferIndex, buffers: I)
    where
        I: IntoIterator<Item = (T, buffer::Offset)>,
        T: Borrow<Buffer>,
    {
        for (i, (buf, offset)) in buffers.into_iter().enumerate() {
            let idx = i + first_binding as usize;
            let buf = buf.borrow();

            if buf.ty == MemoryHeapFlags::HOST_COHERENT {
                self.defer_coherent_flush(buf);
            }

            self.cache
                .set_vertex_buffer(idx, offset as u32, buf.internal.raw);
        }

        self.cache.bind_vertex_buffers(&self.context);
    }

    unsafe fn set_viewports<T>(&mut self, _first_viewport: u32, viewports: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Viewport>,
    {
        let viewports = viewports
            .into_iter()
            .map(|v| {
                let v = v.borrow();
                conv::map_viewport(v)
            })
            .collect::<Vec<_>>();

        // TODO: DX only lets us set all VPs at once, so cache in slice?
        self.cache.set_viewports(&viewports);
        self.cache.bind_viewports(&self.context);
    }

    unsafe fn set_scissors<T>(&mut self, _first_scissor: u32, scissors: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Rect>,
    {
        let scissors = scissors
            .into_iter()
            .map(|s| {
                let s = s.borrow();
                conv::map_rect(s)
            })
            .collect::<Vec<_>>();

        // TODO: same as for viewports
        self.context
            .RSSetScissorRects(scissors.len() as _, scissors.as_ptr());
    }

    unsafe fn set_blend_constants(&mut self, color: pso::ColorValue) {
        self.cache.set_blend_factor(color);
        self.cache.bind_blend_state(&self.context);
    }

    unsafe fn set_stencil_reference(&mut self, _faces: pso::Face, value: pso::StencilValue) {
        self.cache.stencil_ref = Some(value);
    }

    unsafe fn set_stencil_read_mask(&mut self, _faces: pso::Face, value: pso::StencilValue) {
        self.cache.stencil_read_mask = Some(value);
    }

    unsafe fn set_stencil_write_mask(&mut self, _faces: pso::Face, value: pso::StencilValue) {
        self.cache.stencil_write_mask = Some(value);
    }

    unsafe fn set_depth_bounds(&mut self, _bounds: Range<f32>) {
        unimplemented!()
    }

    unsafe fn set_line_width(&mut self, width: f32) {
        validate_line_width(width);
    }

    unsafe fn set_depth_bias(&mut self, _depth_bias: pso::DepthBias) {
        // TODO:
        // unimplemented!()
    }

    unsafe fn bind_graphics_pipeline(&mut self, pipeline: &GraphicsPipeline) {
        self.cache.set_graphics_pipeline(pipeline.clone());
        self.cache.bind_graphics_pipeline(&self.context);
    }

    unsafe fn bind_graphics_descriptor_sets<'a, I, J>(
        &mut self,
        layout: &PipelineLayout,
        first_set: usize,
        sets: I,
        _offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<DescriptorSet>,
        J: IntoIterator,
        J::Item: Borrow<command::DescriptorSetOffset>,
    {
        let _scope = debug_scope!(&self.context, "BindGraphicsDescriptorSets");

        // TODO: find a better solution to invalidating old bindings..
        self.context.CSSetUnorderedAccessViews(
            0,
            16,
            [ptr::null_mut(); 16].as_ptr(),
            ptr::null_mut(),
        );

        //let offsets: Vec<command::DescriptorSetOffset> = offsets.into_iter().map(|o| *o.borrow()).collect();

        let iter = sets
            .into_iter()
            .zip(layout.set_bindings.iter().skip(first_set));

        for (set, bindings) in iter {
            let set = set.borrow();

            {
                let coherent_buffers = set.coherent_buffers.lock();
                for sync in coherent_buffers.flush_coherent_buffers.borrow().iter() {
                    // TODO: merge sync range if a flush already exists
                    if !self
                        .flush_coherent_memory
                        .iter()
                        .any(|m| m.buffer == sync.device_buffer)
                    {
                        self.flush_coherent_memory.push(MemoryFlush {
                            host_memory: sync.host_ptr,
                            sync_range: sync.range.clone(),
                            buffer: sync.device_buffer,
                        });
                    }
                }

                for sync in coherent_buffers.invalidate_coherent_buffers.borrow().iter() {
                    if !self
                        .invalidate_coherent_memory
                        .iter()
                        .any(|m| m.buffer == sync.device_buffer)
                    {
                        self.invalidate_coherent_memory.push(MemoryInvalidate {
                            working_buffer: Some(self.internal.working_buffer.clone()),
                            working_buffer_size: self.internal.working_buffer_size,
                            host_memory: sync.host_ptr,
                            sync_range: sync.range.clone(),
                            buffer: sync.device_buffer,
                        });
                    }
                }
            }

            // TODO: offsets
            for binding in bindings.iter() {
                self.bind_descriptor(&self.context, binding, set.handles);
            }
        }
    }

    unsafe fn bind_compute_pipeline(&mut self, pipeline: &ComputePipeline) {
        self.context
            .CSSetShader(pipeline.cs.as_raw(), ptr::null_mut(), 0);
    }

    unsafe fn bind_compute_descriptor_sets<I, J>(
        &mut self,
        layout: &PipelineLayout,
        first_set: usize,
        sets: I,
        _offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<DescriptorSet>,
        J: IntoIterator,
        J::Item: Borrow<command::DescriptorSetOffset>,
    {
        let _scope = debug_scope!(&self.context, "BindComputeDescriptorSets");

        self.context.CSSetUnorderedAccessViews(
            0,
            16,
            [ptr::null_mut(); 16].as_ptr(),
            ptr::null_mut(),
        );
        let iter = sets
            .into_iter()
            .zip(layout.set_bindings.iter().skip(first_set));

        for (set, bindings) in iter {
            let set = set.borrow();

            {
                let coherent_buffers = set.coherent_buffers.lock();
                for sync in coherent_buffers.flush_coherent_buffers.borrow().iter() {
                    if !self
                        .flush_coherent_memory
                        .iter()
                        .any(|m| m.buffer == sync.device_buffer)
                    {
                        self.flush_coherent_memory.push(MemoryFlush {
                            host_memory: sync.host_ptr,
                            sync_range: sync.range.clone(),
                            buffer: sync.device_buffer,
                        });
                    }
                }

                for sync in coherent_buffers.invalidate_coherent_buffers.borrow().iter() {
                    if !self
                        .invalidate_coherent_memory
                        .iter()
                        .any(|m| m.buffer == sync.device_buffer)
                    {
                        self.invalidate_coherent_memory.push(MemoryInvalidate {
                            working_buffer: Some(self.internal.working_buffer.clone()),
                            working_buffer_size: self.internal.working_buffer_size,
                            host_memory: sync.host_ptr,
                            sync_range: sync.range.clone(),
                            buffer: sync.device_buffer,
                        });
                    }
                }
            }

            // TODO: offsets
            for binding in bindings.iter() {
                self.bind_descriptor(&self.context, binding, set.handles);
            }
        }
    }

    unsafe fn dispatch(&mut self, count: WorkGroupCount) {
        self.context.Dispatch(count[0], count[1], count[2]);
    }

    unsafe fn dispatch_indirect(&mut self, _buffer: &Buffer, _offset: buffer::Offset) {
        unimplemented!()
    }

    unsafe fn fill_buffer<R>(&mut self, _buffer: &Buffer, _range: R, _data: u32)
    where
        R: RangeArg<buffer::Offset>,
    {
        unimplemented!()
    }

    unsafe fn update_buffer(&mut self, _buffer: &Buffer, _offset: buffer::Offset, _data: &[u8]) {
        unimplemented!()
    }

    unsafe fn copy_buffer<T>(&mut self, src: &Buffer, dst: &Buffer, regions: T)
    where
        T: IntoIterator,
        T::Item: Borrow<command::BufferCopy>,
    {
        if src.ty == MemoryHeapFlags::HOST_COHERENT {
            self.defer_coherent_flush(src);
        }

        for region in regions.into_iter() {
            let info = region.borrow();
            let dst_box = d3d11::D3D11_BOX {
                left: info.src as _,
                top: 0,
                front: 0,
                right: (info.src + info.size) as _,
                bottom: 1,
                back: 1,
            };

            self.context.CopySubresourceRegion(
                dst.internal.raw as _,
                0,
                info.dst as _,
                0,
                0,
                src.internal.raw as _,
                0,
                &dst_box,
            );

            if let Some(disjoint_cb) = dst.internal.disjoint_cb {
                self.context.CopySubresourceRegion(
                    disjoint_cb as _,
                    0,
                    info.dst as _,
                    0,
                    0,
                    src.internal.raw as _,
                    0,
                    &dst_box,
                );
            }
        }
    }

    unsafe fn copy_image<T>(
        &mut self,
        src: &Image,
        _: image::Layout,
        dst: &Image,
        _: image::Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::ImageCopy>,
    {
        self.internal
            .copy_image_2d(&self.context, src, dst, regions);
    }

    unsafe fn copy_buffer_to_image<T>(
        &mut self,
        buffer: &Buffer,
        image: &Image,
        _: image::Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::BufferImageCopy>,
    {
        if buffer.ty == MemoryHeapFlags::HOST_COHERENT {
            self.defer_coherent_flush(buffer);
        }

        self.internal
            .copy_buffer_to_image_2d(&self.context, buffer, image, regions);
    }

    unsafe fn copy_image_to_buffer<T>(
        &mut self,
        image: &Image,
        _: image::Layout,
        buffer: &Buffer,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<command::BufferImageCopy>,
    {
        if buffer.ty == MemoryHeapFlags::HOST_COHERENT {
            self.defer_coherent_invalidate(buffer);
        }

        self.internal
            .copy_image_2d_to_buffer(&self.context, image, buffer, regions);
    }

    unsafe fn draw(&mut self, vertices: Range<VertexCount>, instances: Range<InstanceCount>) {
        self.context.DrawInstanced(
            vertices.end - vertices.start,
            instances.end - instances.start,
            vertices.start,
            instances.start,
        );
    }

    unsafe fn draw_indexed(
        &mut self,
        indices: Range<IndexCount>,
        base_vertex: VertexOffset,
        instances: Range<InstanceCount>,
    ) {
        self.context.DrawIndexedInstanced(
            indices.end - indices.start,
            instances.end - instances.start,
            indices.start,
            base_vertex,
            instances.start,
        );
    }

    unsafe fn draw_indirect(
        &mut self,
        _buffer: &Buffer,
        _offset: buffer::Offset,
        _draw_count: DrawCount,
        _stride: u32,
    ) {
        unimplemented!()
    }

    unsafe fn draw_indexed_indirect(
        &mut self,
        _buffer: &Buffer,
        _offset: buffer::Offset,
        _draw_count: DrawCount,
        _stride: u32,
    ) {
        unimplemented!()
    }

    unsafe fn set_event(&mut self, _: &(), _: pso::PipelineStage) {
        unimplemented!()
    }

    unsafe fn reset_event(&mut self, _: &(), _: pso::PipelineStage) {
        unimplemented!()
    }

    unsafe fn wait_events<'a, I, J>(&mut self, _: I, _: Range<pso::PipelineStage>, _: J)
    where
        I: IntoIterator,
        I::Item: Borrow<()>,
        J: IntoIterator,
        J::Item: Borrow<memory::Barrier<'a, Backend>>,
    {
        unimplemented!()
    }

    unsafe fn begin_query(&mut self, _query: query::Query<Backend>, _flags: query::ControlFlags) {
        unimplemented!()
    }

    unsafe fn end_query(&mut self, _query: query::Query<Backend>) {
        unimplemented!()
    }

    unsafe fn reset_query_pool(&mut self, _pool: &QueryPool, _queries: Range<query::Id>) {
        unimplemented!()
    }

    unsafe fn copy_query_pool_results(
        &mut self,
        _pool: &QueryPool,
        _queries: Range<query::Id>,
        _buffer: &Buffer,
        _offset: buffer::Offset,
        _stride: buffer::Offset,
        _flags: query::ResultFlags,
    ) {
        unimplemented!()
    }

    unsafe fn write_timestamp(&mut self, _: pso::PipelineStage, _query: query::Query<Backend>) {
        unimplemented!()
    }

    unsafe fn push_graphics_constants(
        &mut self,
        _layout: &PipelineLayout,
        _stages: pso::ShaderStageFlags,
        _offset: u32,
        _constants: &[u32],
    ) {
        // unimplemented!()
    }

    unsafe fn push_compute_constants(
        &mut self,
        _layout: &PipelineLayout,
        _offset: u32,
        _constants: &[u32],
    ) {
        unimplemented!()
    }

    unsafe fn execute_commands<'a, T, I>(&mut self, _buffers: I)
    where
        T: 'a + Borrow<CommandBuffer>,
        I: IntoIterator<Item = &'a T>,
    {
        unimplemented!()
    }
}

bitflags! {
    struct MemoryHeapFlags: u64 {
        const DEVICE_LOCAL = 0x1;
        const HOST_VISIBLE = 0x2 | 0x4;
        const HOST_COHERENT = 0x2;
    }
}

#[derive(Clone, Debug)]
enum SyncRange {
    Whole,
    Partial(Range<u64>),
}

#[derive(Debug)]
pub struct MemoryFlush {
    host_memory: *mut u8,
    sync_range: SyncRange,
    buffer: *mut d3d11::ID3D11Buffer,
}

pub struct MemoryInvalidate {
    working_buffer: Option<ComPtr<d3d11::ID3D11Buffer>>,
    working_buffer_size: u64,
    host_memory: *mut u8,
    sync_range: Range<u64>,
    buffer: *mut d3d11::ID3D11Buffer,
}

impl fmt::Debug for MemoryInvalidate {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("MemoryInvalidate")
    }
}

fn intersection(a: &Range<u64>, b: &Range<u64>) -> Option<Range<u64>> {
    let min = if a.start < b.start { a } else { b };
    let max = if min == a { b } else { a };

    if min.end < max.start {
        None
    } else {
        let end = if min.end < max.end { min.end } else { max.end };
        Some(max.start .. end)
    }
}

impl MemoryFlush {
    fn do_flush(&self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        let src = self.host_memory;

        debug_marker!(context, "Flush({:?})", self.sync_range);
        let region = if let SyncRange::Partial(range) = &self.sync_range {
            Some(d3d11::D3D11_BOX {
                left: range.start as _,
                top: 0,
                front: 0,
                right: range.end as _,
                bottom: 1,
                back: 1,
            })
        } else {
            None
        };

        unsafe {
            context.UpdateSubresource(
                self.buffer as _,
                0,
                if let Some(region) = region {
                    &region
                } else {
                    ptr::null_mut()
                },
                src as _,
                0,
                0,
            );
        }
    }
}

impl MemoryInvalidate {
    fn download(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        buffer: *mut d3d11::ID3D11Buffer,
        range: Range<u64>,
    ) {
        unsafe {
            context.CopySubresourceRegion(
                self.working_buffer.clone().unwrap().as_raw() as _,
                0,
                0,
                0,
                0,
                buffer as _,
                0,
                &d3d11::D3D11_BOX {
                    left: range.start as _,
                    top: 0,
                    front: 0,
                    right: range.end as _,
                    bottom: 1,
                    back: 1,
                },
            );

            // copy over to our vec
            let dst = self.host_memory.offset(range.start as isize);
            let src = self.map(&context);
            ptr::copy(src, dst, (range.end - range.start) as usize);
            self.unmap(&context);
        }
    }

    fn do_invalidate(&self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        let stride = self.working_buffer_size;
        let range = &self.sync_range;
        let len = range.end - range.start;
        let chunks = len / stride;
        let remainder = len % stride;

        // we split up the copies into chunks the size of our working buffer
        for i in 0 .. chunks {
            let offset = range.start + i * stride;
            let range = offset .. (offset + stride);

            self.download(context, self.buffer, range);
        }

        if remainder != 0 {
            self.download(context, self.buffer, (chunks * stride) .. range.end);
        }
    }

    fn map(&self, context: &ComPtr<d3d11::ID3D11DeviceContext>) -> *mut u8 {
        assert_eq!(self.working_buffer.is_some(), true);

        unsafe {
            let mut map = mem::zeroed();
            let hr = context.Map(
                self.working_buffer.clone().unwrap().as_raw() as _,
                0,
                d3d11::D3D11_MAP_READ,
                0,
                &mut map,
            );

            assert_eq!(hr, winerror::S_OK);

            map.pData as _
        }
    }

    fn unmap(&self, context: &ComPtr<d3d11::ID3D11DeviceContext>) {
        unsafe {
            context.Unmap(self.working_buffer.clone().unwrap().as_raw() as _, 0);
        }
    }
}

// Since we dont have any heaps to work with directly, everytime we bind a
// buffer/image to memory we allocate a dx11 resource and assign it a range.
//
// `HOST_VISIBLE` memory gets a `Vec<u8>` which covers the entire memory
// range. This forces us to only expose non-coherent memory, as this
// abstraction acts as a "cache" since the "staging buffer" vec is disjoint
// from all the dx11 resources we store in the struct.
pub struct Memory {
    ty: MemoryHeapFlags,
    properties: memory::Properties,
    size: u64,

    mapped_ptr: *mut u8,

    // staging buffer covering the whole memory region, if it's HOST_VISIBLE
    host_visible: Option<RefCell<Vec<u8>>>,

    // list of all buffers bound to this memory
    local_buffers: RefCell<Vec<(Range<u64>, InternalBuffer)>>,

    // list of all images bound to this memory
    local_images: RefCell<Vec<(Range<u64>, InternalImage)>>,
}

impl fmt::Debug for Memory {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Memory")
    }
}

unsafe impl Send for Memory {}
unsafe impl Sync for Memory {}

impl Memory {
    pub fn resolve<R: RangeArg<u64>>(&self, range: &R) -> Range<u64> {
        *range.start().unwrap_or(&0) .. *range.end().unwrap_or(&self.size)
    }

    pub fn bind_buffer(&self, range: Range<u64>, buffer: InternalBuffer) {
        self.local_buffers.borrow_mut().push((range, buffer));
    }

    pub fn flush(&self, context: &ComPtr<d3d11::ID3D11DeviceContext>, range: Range<u64>) {
        use buffer::Usage;

        for &(ref buffer_range, ref buffer) in self.local_buffers.borrow().iter() {
            if let Some(range) = intersection(&range, &buffer_range) {
                let ptr = self.mapped_ptr;

                // we need to handle 3 cases for updating buffers:
                //
                //   1. if our buffer was created as a `UNIFORM` buffer *and* other usage flags, we
                //      also have a disjoint buffer which only has `D3D11_BIND_CONSTANT_BUFFER` due
                //      to DX11 limitation. we then need to update both the original buffer and the
                //      disjoint one with the *whole* range (TODO: allow for partial updates)
                //
                //   2. if our buffer was created with *only* `UNIFORM` usage we need to upload
                //      the whole range (TODO: allow for partial updates)
                //
                //   3. the general case, without any `UNIFORM` usage has no restrictions on
                //      partial updates, so we upload the specified range
                //
                if buffer.usage.contains(Usage::UNIFORM) && buffer.usage != Usage::UNIFORM {
                    MemoryFlush {
                        host_memory: unsafe { ptr.offset(buffer_range.start as _) },
                        sync_range: SyncRange::Whole,
                        buffer: buffer.raw,
                    }
                    .do_flush(&context);

                    if let Some(disjoint) = buffer.disjoint_cb {
                        MemoryFlush {
                            host_memory: unsafe { ptr.offset(buffer_range.start as _) },
                            sync_range: SyncRange::Whole,
                            buffer: disjoint,
                        }
                        .do_flush(&context);
                    }
                } else if buffer.usage == Usage::UNIFORM {
                    MemoryFlush {
                        host_memory: unsafe { ptr.offset(buffer_range.start as _) },
                        sync_range: SyncRange::Whole,
                        buffer: buffer.raw,
                    }
                    .do_flush(&context);
                } else {
                    let local_start = range.start - buffer_range.start;
                    let local_len = range.end - range.start;

                    MemoryFlush {
                        host_memory: unsafe { ptr.offset(range.start as _) },
                        sync_range: SyncRange::Partial(local_start .. (local_start + local_len)),
                        buffer: buffer.raw,
                    }
                    .do_flush(&context);
                }
            }
        }
    }

    pub fn invalidate(
        &self,
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        range: Range<u64>,
        working_buffer: ComPtr<d3d11::ID3D11Buffer>,
        working_buffer_size: u64,
    ) {
        for &(ref buffer_range, ref buffer) in self.local_buffers.borrow().iter() {
            if let Some(range) = intersection(&range, &buffer_range) {
                MemoryInvalidate {
                    working_buffer: Some(working_buffer.clone()),
                    working_buffer_size,
                    host_memory: self.mapped_ptr,
                    sync_range: range.clone(),
                    buffer: buffer.raw,
                }
                .do_invalidate(&context);
            }
        }
    }
}

#[derive(Debug)]
pub struct CommandPool {
    device: ComPtr<d3d11::ID3D11Device>,
    internal: internal::Internal,
}

unsafe impl Send for CommandPool {}
unsafe impl Sync for CommandPool {}

impl hal::pool::CommandPool<Backend> for CommandPool {
    unsafe fn reset(&mut self, _release_resources: bool) {
        //unimplemented!()
    }

    unsafe fn allocate_one(&mut self, _level: command::Level) -> CommandBuffer {
        CommandBuffer::create_deferred(self.device.clone(), self.internal.clone())
    }

    unsafe fn free<I>(&mut self, _cbufs: I)
    where
        I: IntoIterator<Item = CommandBuffer>,
    {
        // TODO:
        // unimplemented!()
    }
}

/// Similarily to dx12 backend, we can handle either precompiled dxbc or spirv
pub enum ShaderModule {
    Dxbc(Vec<u8>),
    Spirv(Vec<u32>),
}

// TODO: temporary
impl ::fmt::Debug for ShaderModule {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}", "ShaderModule { ... }")
    }
}

unsafe impl Send for ShaderModule {}
unsafe impl Sync for ShaderModule {}

#[derive(Clone, Debug)]
pub struct SubpassDesc {
    pub color_attachments: Vec<pass::AttachmentRef>,
    pub depth_stencil_attachment: Option<pass::AttachmentRef>,
    pub input_attachments: Vec<pass::AttachmentRef>,
    pub resolve_attachments: Vec<pass::AttachmentRef>,
}

impl SubpassDesc {
    pub(crate) fn is_using(&self, at_id: pass::AttachmentId) -> bool {
        self.color_attachments
            .iter()
            .chain(self.depth_stencil_attachment.iter())
            .chain(self.input_attachments.iter())
            .chain(self.resolve_attachments.iter())
            .any(|&(id, _)| id == at_id)
    }
}

#[derive(Clone, Debug)]
pub struct RenderPass {
    pub attachments: Vec<pass::Attachment>,
    pub subpasses: Vec<SubpassDesc>,
}

#[derive(Clone, Debug)]
pub struct Framebuffer {
    attachments: Vec<ImageView>,
    layers: image::Layer,
}

#[derive(Clone, Debug)]
pub struct InternalBuffer {
    raw: *mut d3d11::ID3D11Buffer,
    // TODO: need to sync between `raw` and `disjoint_cb`, same way as we do with
    // `MemoryFlush/Invalidate`
    disjoint_cb: Option<*mut d3d11::ID3D11Buffer>, // if unbound this buffer might be null.
    srv: Option<*mut d3d11::ID3D11ShaderResourceView>,
    uav: Option<*mut d3d11::ID3D11UnorderedAccessView>,
    usage: buffer::Usage,
}

pub struct Buffer {
    internal: InternalBuffer,
    ty: MemoryHeapFlags,     // empty if unbound
    host_ptr: *mut u8,       // null if unbound
    bound_range: Range<u64>, // 0 if unbound
    requirements: memory::Requirements,
    bind: d3d11::D3D11_BIND_FLAG,
}

impl fmt::Debug for Buffer {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Buffer")
    }
}

unsafe impl Send for Buffer {}
unsafe impl Sync for Buffer {}

#[derive(Debug)]
pub struct BufferView;

pub struct Image {
    kind: image::Kind,
    usage: image::Usage,
    format: format::Format,
    view_caps: image::ViewCapabilities,
    decomposed_format: conv::DecomposedDxgiFormat,
    mip_levels: image::Level,
    internal: InternalImage,
    tiling: image::Tiling,
    bind: d3d11::D3D11_BIND_FLAG,
    requirements: memory::Requirements,
}

impl fmt::Debug for Image {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Image")
    }
}

pub struct InternalImage {
    raw: *mut d3d11::ID3D11Resource,
    copy_srv: Option<ComPtr<d3d11::ID3D11ShaderResourceView>>,
    srv: Option<ComPtr<d3d11::ID3D11ShaderResourceView>>,

    /// Contains UAVs for all subresources
    unordered_access_views: Vec<ComPtr<d3d11::ID3D11UnorderedAccessView>>,

    /// Contains DSVs for all subresources
    depth_stencil_views: Vec<ComPtr<d3d11::ID3D11DepthStencilView>>,

    /// Contains RTVs for all subresources
    render_target_views: Vec<ComPtr<d3d11::ID3D11RenderTargetView>>,
}

impl fmt::Debug for InternalImage {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("InternalImage")
    }
}

unsafe impl Send for Image {}
unsafe impl Sync for Image {}

impl Image {
    pub fn calc_subresource(&self, mip_level: UINT, layer: UINT) -> UINT {
        mip_level + (layer * self.mip_levels as UINT)
    }

    pub fn get_uav(
        &self,
        mip_level: image::Level,
        _layer: image::Layer,
    ) -> Option<&ComPtr<d3d11::ID3D11UnorderedAccessView>> {
        self.internal
            .unordered_access_views
            .get(self.calc_subresource(mip_level as _, 0) as usize)
    }

    pub fn get_dsv(
        &self,
        mip_level: image::Level,
        layer: image::Layer,
    ) -> Option<&ComPtr<d3d11::ID3D11DepthStencilView>> {
        self.internal
            .depth_stencil_views
            .get(self.calc_subresource(mip_level as _, layer as _) as usize)
    }

    pub fn get_rtv(
        &self,
        mip_level: image::Level,
        layer: image::Layer,
    ) -> Option<&ComPtr<d3d11::ID3D11RenderTargetView>> {
        self.internal
            .render_target_views
            .get(self.calc_subresource(mip_level as _, layer as _) as usize)
    }
}

#[derive(Clone)]
pub struct ImageView {
    format: format::Format,
    rtv_handle: Option<ComPtr<d3d11::ID3D11RenderTargetView>>,
    srv_handle: Option<ComPtr<d3d11::ID3D11ShaderResourceView>>,
    dsv_handle: Option<ComPtr<d3d11::ID3D11DepthStencilView>>,
    uav_handle: Option<ComPtr<d3d11::ID3D11UnorderedAccessView>>,
}

impl fmt::Debug for ImageView {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("ImageView")
    }
}

unsafe impl Send for ImageView {}
unsafe impl Sync for ImageView {}

pub struct Sampler {
    sampler_handle: ComPtr<d3d11::ID3D11SamplerState>,
}

impl fmt::Debug for Sampler {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Sampler")
    }
}

unsafe impl Send for Sampler {}
unsafe impl Sync for Sampler {}

pub struct ComputePipeline {
    cs: ComPtr<d3d11::ID3D11ComputeShader>,
}

impl fmt::Debug for ComputePipeline {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("ComputePipeline")
    }
}

unsafe impl Send for ComputePipeline {}
unsafe impl Sync for ComputePipeline {}

/// NOTE: some objects are hashed internally and reused when created with the
///       same params[0], need to investigate which interfaces this applies
///       to.
///
/// [0]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476500(v=vs.85).aspx
#[derive(Clone)]
pub struct GraphicsPipeline {
    vs: ComPtr<d3d11::ID3D11VertexShader>,
    gs: Option<ComPtr<d3d11::ID3D11GeometryShader>>,
    hs: Option<ComPtr<d3d11::ID3D11HullShader>>,
    ds: Option<ComPtr<d3d11::ID3D11DomainShader>>,
    ps: Option<ComPtr<d3d11::ID3D11PixelShader>>,
    topology: d3d11::D3D11_PRIMITIVE_TOPOLOGY,
    input_layout: ComPtr<d3d11::ID3D11InputLayout>,
    rasterizer_state: ComPtr<d3d11::ID3D11RasterizerState>,
    blend_state: ComPtr<d3d11::ID3D11BlendState>,
    depth_stencil_state: Option<(
        ComPtr<d3d11::ID3D11DepthStencilState>,
        pso::State<pso::StencilValue>,
    )>,
    baked_states: pso::BakedStates,
    required_bindings: u32,
    max_vertex_bindings: u32,
    strides: Vec<u32>,
}

impl fmt::Debug for GraphicsPipeline {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("GraphicsPipeline")
    }
}

unsafe impl Send for GraphicsPipeline {}
unsafe impl Sync for GraphicsPipeline {}

#[derive(Clone, Debug)]
struct PipelineBinding {
    stage: pso::ShaderStageFlags,
    ty: pso::DescriptorType,
    binding_range: Range<u32>,
    handle_offset: u32,
}

#[derive(Clone, Debug)]
struct RegisterMapping {
    ty: pso::DescriptorType,
    spirv_binding: u32,
    hlsl_register: u8,
    combined: bool,
}

#[derive(Clone, Debug)]
struct RegisterRemapping {
    mapping: Vec<RegisterMapping>,
    num_t: u8,
    num_s: u8,
    num_c: u8,
    num_u: u8,
}

/// The pipeline layout holds optimized (less api calls) ranges of objects for all descriptor sets
/// belonging to the pipeline object.
#[derive(Debug)]
pub struct PipelineLayout {
    set_bindings: Vec<Vec<PipelineBinding>>,
    set_remapping: Vec<RegisterRemapping>,
}

/// The descriptor set layout contains mappings from a given binding to the offset in our
/// descriptor pool storage and what type of descriptor it is (combined image sampler takes up two
/// handles).
#[derive(Debug)]
pub struct DescriptorSetLayout {
    bindings: Vec<PipelineBinding>,
    handle_count: u32,
    register_remap: RegisterRemapping,
}

#[derive(Debug)]
struct CoherentBufferFlushRange {
    device_buffer: *mut d3d11::ID3D11Buffer,
    host_ptr: *mut u8,
    range: SyncRange,
}

#[derive(Debug)]
struct CoherentBufferInvalidateRange {
    device_buffer: *mut d3d11::ID3D11Buffer,
    host_ptr: *mut u8,
    range: Range<u64>,
}

#[derive(Debug)]
struct CoherentBuffers {
    // descriptor set writes containing coherent resources go into these vecs and are added to the
    // command buffers own Vec on binding the set.
    flush_coherent_buffers: RefCell<Vec<CoherentBufferFlushRange>>,
    invalidate_coherent_buffers: RefCell<Vec<CoherentBufferInvalidateRange>>,
}

impl CoherentBuffers {
    fn add_flush(&self, old: *mut d3d11::ID3D11Buffer, buffer: &Buffer) {
        let new = buffer.internal.raw;

        if old != new {
            let mut buffers = self.flush_coherent_buffers.borrow_mut();

            let pos = buffers.iter().position(|sync| old == sync.device_buffer);

            let sync_range = CoherentBufferFlushRange {
                device_buffer: new,
                host_ptr: buffer.host_ptr,
                range: SyncRange::Whole,
            };

            if let Some(pos) = pos {
                buffers[pos] = sync_range;
            } else {
                buffers.push(sync_range);
            }

            if let Some(disjoint) = buffer.internal.disjoint_cb {
                let pos = buffers
                    .iter()
                    .position(|sync| disjoint == sync.device_buffer);

                let sync_range = CoherentBufferFlushRange {
                    device_buffer: disjoint,
                    host_ptr: buffer.host_ptr,
                    range: SyncRange::Whole,
                };

                if let Some(pos) = pos {
                    buffers[pos] = sync_range;
                } else {
                    buffers.push(sync_range);
                }
            }
        }
    }

    fn add_invalidate(&self, old: *mut d3d11::ID3D11Buffer, buffer: &Buffer) {
        let new = buffer.internal.raw;

        if old != new {
            let mut buffers = self.invalidate_coherent_buffers.borrow_mut();

            let pos = buffers.iter().position(|sync| old == sync.device_buffer);

            let sync_range = CoherentBufferInvalidateRange {
                device_buffer: new,
                host_ptr: buffer.host_ptr,
                range: buffer.bound_range.clone(),
            };

            if let Some(pos) = pos {
                buffers[pos] = sync_range;
            } else {
                buffers.push(sync_range);
            }
        }
    }
}

/// Newtype around a common interface that all bindable resources inherit from.
#[derive(Debug, Copy, Clone)]
#[repr(C)]
struct Descriptor(*mut d3d11::ID3D11DeviceChild);

pub struct DescriptorSet {
    offset: usize,
    len: usize,
    handles: *mut Descriptor,
    register_remap: RegisterRemapping,
    coherent_buffers: Mutex<CoherentBuffers>,
}

impl fmt::Debug for DescriptorSet {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("DescriptorSet")
    }
}

unsafe impl Send for DescriptorSet {}
unsafe impl Sync for DescriptorSet {}

impl DescriptorSet {
    fn get_handle_offset(&self, target_binding: u32) -> (pso::DescriptorType, u8, u8) {
        use pso::DescriptorType::*;

        let mapping = self
            .register_remap
            .mapping
            .iter()
            .find(|&mapping| target_binding == mapping.spirv_binding)
            .unwrap();

        let (ty, register) = (mapping.ty, mapping.hlsl_register);

        match ty {
            Sampler => {
                let (ty, t_reg) = if mapping.combined {
                    let combined_mapping = self
                        .register_remap
                        .mapping
                        .iter()
                        .find(|&mapping| {
                            mapping.ty == SampledImage && target_binding == mapping.spirv_binding
                        })
                        .unwrap();
                    (CombinedImageSampler, combined_mapping.hlsl_register)
                } else {
                    (ty, 0)
                };

                (ty, register, self.register_remap.num_s + t_reg)
            }
            SampledImage | UniformTexelBuffer => (ty, self.register_remap.num_s + register, 0),
            UniformBuffer | UniformBufferDynamic => (
                ty,
                self.register_remap.num_s + self.register_remap.num_t + register,
                0,
            ),
            StorageTexelBuffer | StorageBuffer | InputAttachment | StorageBufferDynamic
            | StorageImage => (
                ty,
                self.register_remap.num_s
                    + self.register_remap.num_t
                    + self.register_remap.num_c
                    + register,
                0,
            ),
            CombinedImageSampler => unreachable!(),
        }
    }

    fn add_flush(&self, old: *mut d3d11::ID3D11Buffer, buffer: &Buffer) {
        let new = buffer.internal.raw;

        if old != new {
            self.coherent_buffers.lock().add_flush(old, buffer);
        }
    }

    fn add_invalidate(&self, old: *mut d3d11::ID3D11Buffer, buffer: &Buffer) {
        let new = buffer.internal.raw;

        if old != new {
            self.coherent_buffers.lock().add_invalidate(old, buffer);
        }
    }
}

#[derive(Debug)]
pub struct DescriptorPool {
    handles: Vec<Descriptor>,
    allocator: RangeAllocator<usize>,
}

unsafe impl Send for DescriptorPool {}
unsafe impl Sync for DescriptorPool {}

impl DescriptorPool {
    pub fn with_capacity(size: usize) -> Self {
        DescriptorPool {
            handles: vec![Descriptor(ptr::null_mut()); size],
            allocator: RangeAllocator::new(0 .. size),
        }
    }
}

impl pso::DescriptorPool<Backend> for DescriptorPool {
    unsafe fn allocate_set(
        &mut self,
        layout: &DescriptorSetLayout,
    ) -> Result<DescriptorSet, pso::AllocationError> {
        // TODO: make sure this doesn't contradict vulkan semantics
        // if layout has 0 bindings, allocate 1 handle anyway
        let len = layout.handle_count.max(1) as _;

        self.allocator
            .allocate_range(len)
            .map(|range| {
                for handle in &mut self.handles[range.clone()] {
                    *handle = Descriptor(ptr::null_mut());
                }

                DescriptorSet {
                    offset: range.start,
                    len,
                    handles: self.handles.as_mut_ptr().offset(range.start as _),
                    register_remap: layout.register_remap.clone(),
                    coherent_buffers: Mutex::new(CoherentBuffers {
                        flush_coherent_buffers: RefCell::new(Vec::new()),
                        invalidate_coherent_buffers: RefCell::new(Vec::new()),
                    }),
                }
            })
            .map_err(|_| pso::AllocationError::OutOfPoolMemory)
    }

    unsafe fn free_sets<I>(&mut self, descriptor_sets: I)
    where
        I: IntoIterator<Item = DescriptorSet>,
    {
        for set in descriptor_sets {
            self.allocator
                .free_range(set.offset .. (set.offset + set.len))
        }
    }

    unsafe fn reset(&mut self) {
        self.allocator.reset();
    }
}

#[derive(Debug)]
pub struct RawFence {
    mutex: Mutex<bool>,
    condvar: Condvar,
}

pub type Fence = Arc<RawFence>;

#[derive(Debug)]
pub struct Semaphore;
#[derive(Debug)]
pub struct QueryPool;

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub enum Backend {}
impl hal::Backend for Backend {
    type Instance = Instance;
    type PhysicalDevice = PhysicalDevice;
    type Device = device::Device;

    type Surface = Surface;
    type Swapchain = Swapchain;

    type QueueFamily = QueueFamily;
    type CommandQueue = CommandQueue;
    type CommandBuffer = CommandBuffer;

    type Memory = Memory;
    type CommandPool = CommandPool;

    type ShaderModule = ShaderModule;
    type RenderPass = RenderPass;
    type Framebuffer = Framebuffer;

    type Buffer = Buffer;
    type BufferView = BufferView;
    type Image = Image;

    type ImageView = ImageView;
    type Sampler = Sampler;

    type ComputePipeline = ComputePipeline;
    type GraphicsPipeline = GraphicsPipeline;
    type PipelineLayout = PipelineLayout;
    type PipelineCache = ();
    type DescriptorSetLayout = DescriptorSetLayout;
    type DescriptorPool = DescriptorPool;
    type DescriptorSet = DescriptorSet;

    type Fence = Fence;
    type Semaphore = Semaphore;
    type Event = ();
    type QueryPool = QueryPool;
}

fn validate_line_width(width: f32) {
    // Note from the Vulkan spec:
    // > If the wide lines feature is not enabled, lineWidth must be 1.0
    // Simply assert and no-op because DX11 never exposes `Features::LINE_WIDTH`
    assert_eq!(width, 1.0);
}
