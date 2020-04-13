/*!
# Metal backend internals.

## Pipeline Layout

In Metal, push constants, vertex buffers, and resources in the descriptor sets
are all placed together in the native resource bindings, which work similarly to D3D11:
there are tables of textures, buffers, and samplers.

We put push constants first (if any) in the table, followed by descriptor set 0
resource, followed by other descriptor sets. The vertex buffers are bound at the very
end of the VS buffer table.

When argument buffers are supported, each descriptor set becomes a buffer binding,
but the general placement rule is the same.

## Command recording

One-time-submit primary command buffers are recorded "live" into `MTLCommandBuffer`.
Special care is taken to the recording state: active bindings are restored at the
start of any render or compute pass.

Multi-submit and secondary command buffers are recorded as "soft" commands into
`Journal`. Actual native recording is done at either `submit` or `execute_commands`
correspondingly. When that happens, we `enqueue` the command buffer at the start
of recording, which allows the driver to work on pass translation at the same time
as we are recording the following passes.

## Memory

In general, "Shared" storage is used for CPU-coherent memory. "Managed" is used for
non-coherent CPU-visible memory. Finally, "Private" storage is backing device-local
memory types.

Metal doesn't have CPU-visible memory for textures. We only allow RGBA8 2D textures
to be allocated from it, and only for the matter of transfer operations, which is
the minimum required by Vulkan. In fact, these become just glorified staging buffers.

## Events

Events are represented by just an atomic bool. When recording, a command buffer keeps
track of all events set or reset. Signalling within a command buffer is therefore a
matter of simply checking that local list. When making a submission, used events are
also accumulated temporarily, so that we can change their values in the completion
handler of the last command buffer. We also check this list in order to resolve events
fired in one command buffer and waited in another one within the same submission.

Waiting for an event from a different submission is accomplished similar to waiting
for the host. We block all the submissions until the host blockers are resolved, and
these are checked at certain points like setting an event by the device, or waiting
for a fence.
!*/

#[macro_use]
extern crate bitflags;
#[macro_use]
extern crate objc;
#[macro_use]
extern crate log;

use hal::{
    adapter::{Adapter, AdapterInfo, DeviceType},
    queue::{QueueFamilyId, QueueType},
};
use range_alloc::RangeAllocator;

use cocoa::foundation::NSInteger;
use core_graphics::base::CGFloat;
use core_graphics::geometry::CGRect;
#[cfg(feature = "dispatch")]
use dispatch;
use foreign_types::ForeignTypeRef;
use lazy_static::lazy_static;
use metal::MTLFeatureSet;
use metal::MTLLanguageVersion;
use objc::{
    declare::ClassDecl,
    runtime::{Class, Object, Sel, BOOL, YES},
};
use parking_lot::{Condvar, Mutex};

use std::mem;
use std::os::raw::c_void;
use std::ptr::NonNull;
use std::sync::Arc;

mod command;
mod conversions;
mod device;
mod internal;
mod native;
mod soft;
mod window;

pub use crate::command::CommandPool;
pub use crate::device::{Device, LanguageVersion, PhysicalDevice};
pub use crate::window::{AcquireMode, CAMetalLayer, Surface, Swapchain};

pub type GraphicsCommandPool = CommandPool;

//TODO: investigate why exactly using `u8` here is slower (~5% total).
/// A type representing Metal binding's resource index.
type ResourceIndex = u32;

/// Method of recording one-time-submit command buffers.
#[derive(Clone, Debug, Hash, PartialEq)]
pub enum OnlineRecording {
    /// Record natively on-the-fly.
    Immediate,
    /// Store commands and only start recording at submission time.
    Deferred,
    #[cfg(feature = "dispatch")]
    /// Start recording asynchronously upon finishing each pass.
    Remote(dispatch::QueuePriority),
}

impl Default for OnlineRecording {
    fn default() -> Self {
        OnlineRecording::Immediate
    }
}

const MAX_ACTIVE_COMMAND_BUFFERS: usize = 1 << 14;
const MAX_VISIBILITY_QUERIES: usize = 1 << 14;
const MAX_COLOR_ATTACHMENTS: usize = 4;
const MAX_BOUND_DESCRIPTOR_SETS: usize = 8;

#[derive(Debug, Clone, Copy)]
pub struct QueueFamily {}

impl hal::queue::QueueFamily for QueueFamily {
    fn queue_type(&self) -> QueueType {
        QueueType::General
    }
    fn max_queues(&self) -> usize {
        1
    }
    fn id(&self) -> QueueFamilyId {
        QueueFamilyId(0)
    }
}

#[derive(Debug)]
struct VisibilityShared {
    /// Availability buffer is in shared memory, it has N double words for
    /// query results followed by N words for the availability.
    buffer: metal::Buffer,
    allocator: Mutex<RangeAllocator<hal::query::Id>>,
    availability_offset: hal::buffer::Offset,
    condvar: Condvar,
}

#[derive(Debug)]
struct Shared {
    device: Mutex<metal::Device>,
    queue: Mutex<command::QueueInner>,
    queue_blocker: Mutex<command::QueueBlocker>,
    service_pipes: internal::ServicePipes,
    disabilities: PrivateDisabilities,
    private_caps: PrivateCapabilities,
    visibility: VisibilityShared,
}

unsafe impl Send for Shared {}
unsafe impl Sync for Shared {}

impl Shared {
    fn new(device: metal::Device, experiments: &Experiments) -> Self {
        let private_caps = PrivateCapabilities::new(&device, experiments);

        let visibility = VisibilityShared {
            buffer: device.new_buffer(
                MAX_VISIBILITY_QUERIES as u64
                    * (mem::size_of::<u64>() + mem::size_of::<u32>()) as u64,
                metal::MTLResourceOptions::StorageModeShared,
            ),
            allocator: Mutex::new(RangeAllocator::new(
                0 .. MAX_VISIBILITY_QUERIES as hal::query::Id,
            )),
            availability_offset: (MAX_VISIBILITY_QUERIES * mem::size_of::<u64>())
                as hal::buffer::Offset,
            condvar: Condvar::new(),
        };
        Shared {
            queue: Mutex::new(command::QueueInner::new(
                &device,
                Some(MAX_ACTIVE_COMMAND_BUFFERS),
            )),
            queue_blocker: Mutex::new(command::QueueBlocker::default()),
            service_pipes: internal::ServicePipes::new(&device),
            disabilities: PrivateDisabilities {
                broken_viewport_near_depth: device.name().starts_with("Intel")
                    && !device.supports_feature_set(MTLFeatureSet::macOS_GPUFamily1_v4),
                broken_layered_clear_image: device.name().starts_with("Intel"),
            },
            private_caps,
            device: Mutex::new(device),
            visibility,
        }
    }
}

#[derive(Clone, Debug, Default)]
pub struct Experiments {
    pub argument_buffers: bool,
}

#[derive(Debug)]
pub struct Instance {
    pub experiments: Experiments,
    gfx_managed_metal_layer_delegate: GfxManagedMetalLayerDelegate,
}

impl hal::Instance<Backend> for Instance {
    fn create(_: &str, _: u32) -> Result<Self, hal::UnsupportedBackend> {
        Ok(Instance {
            experiments: Experiments::default(),
            gfx_managed_metal_layer_delegate: GfxManagedMetalLayerDelegate::new(),
        })
    }

    fn enumerate_adapters(&self) -> Vec<Adapter<Backend>> {
        let devices = metal::Device::all();
        let mut adapters: Vec<Adapter<Backend>> = devices
            .into_iter()
            .map(|dev| {
                let name = dev.name().into();
                let shared = Shared::new(dev, &self.experiments);
                let physical_device = device::PhysicalDevice::new(Arc::new(shared));
                Adapter {
                    info: AdapterInfo {
                        name,
                        vendor: 0,
                        device: 0,
                        device_type: if physical_device.shared.private_caps.low_power {
                            DeviceType::IntegratedGpu
                        } else {
                            DeviceType::DiscreteGpu
                        },
                    },
                    physical_device,
                    queue_families: vec![QueueFamily {}],
                }
            })
            .collect();
        adapters.sort_by_key(|adapt| {
            (
                adapt.physical_device.shared.private_caps.low_power,
                adapt.physical_device.shared.private_caps.headless,
            )
        });
        adapters
    }

    unsafe fn create_surface(
        &self,
        has_handle: &impl raw_window_handle::HasRawWindowHandle,
    ) -> Result<Surface, hal::window::InitError> {
        match has_handle.raw_window_handle() {
            #[cfg(target_os = "ios")]
            raw_window_handle::RawWindowHandle::IOS(handle) => {
                Ok(self.create_surface_from_uiview(handle.ui_view, false))
            }
            #[cfg(target_os = "macos")]
            raw_window_handle::RawWindowHandle::MacOS(handle) => {
                Ok(self.create_surface_from_nsview(handle.ns_view, false))
            }
            _ => Err(hal::window::InitError::UnsupportedWindowHandle),
        }
    }

    unsafe fn destroy_surface(&self, _surface: Surface) {
        // TODO: Implement Surface cleanup
    }
}

lazy_static! {
    static ref GFX_MANAGED_METAL_LAYER_DELEGATE_CLASS: &'static Class = unsafe {
        let mut decl = ClassDecl::new("GfxManagedMetalLayerDelegate", class!(NSObject)).unwrap();
        decl.add_method(
            sel!(layer:shouldInheritContentsScale:fromWindow:),
            layer_should_inherit_contents_scale_from_window
                as extern "C" fn(&Object, Sel, *mut Object, CGFloat, *mut Object) -> BOOL,
        );
        decl.register()
    };
}

extern "C" fn layer_should_inherit_contents_scale_from_window(
    _: &Object,
    _: Sel,
    _layer: *mut Object,
    _new_scale: CGFloat,
    _from_window: *mut Object,
) -> BOOL {
    return YES;
}

#[derive(Debug)]
struct GfxManagedMetalLayerDelegate(*mut Object);

impl GfxManagedMetalLayerDelegate {
    pub fn new() -> Self {
        unsafe {
            let mut delegate: *mut Object =
                msg_send![*GFX_MANAGED_METAL_LAYER_DELEGATE_CLASS, alloc];
            delegate = msg_send![delegate, init];
            Self(delegate)
        }
    }
}

impl Drop for GfxManagedMetalLayerDelegate {
    fn drop(&mut self) {
        unsafe {
            let () = msg_send![self.0, release];
        }
    }
}

unsafe impl Send for GfxManagedMetalLayerDelegate {}
unsafe impl Sync for GfxManagedMetalLayerDelegate {}

impl Instance {
    #[cfg(target_os = "ios")]
    unsafe fn create_from_uiview(&self, uiview: *mut c_void) -> window::SurfaceInner {
        let view: cocoa::base::id = mem::transmute(uiview);
        if view.is_null() {
            panic!("window does not have a valid contentView");
        }

        let main_layer: CAMetalLayer = msg_send![view, layer];
        let class = class!(CAMetalLayer);
        let is_valid_layer: BOOL = msg_send![main_layer, isKindOfClass: class];
        let render_layer = if is_valid_layer == YES {
            main_layer
        } else {
            // If the main layer is not a CAMetalLayer, we create a CAMetalLayer sublayer and use it instead.
            // Unlike on macOS, we cannot replace the main view as UIView does not allow it (when NSView does).
            let new_layer: CAMetalLayer = msg_send![class, new];

            let bounds: CGRect = msg_send![main_layer, bounds];
            let () = msg_send![new_layer, setFrame: bounds];

            let () = msg_send![main_layer, addSublayer: new_layer];
            new_layer
        };

        let window: cocoa::base::id = msg_send![view, window];
        if !window.is_null() {
            let screen: cocoa::base::id = msg_send![window, screen];
            assert!(!screen.is_null(), "window is not attached to a screen");

            let scale_factor: CGFloat = msg_send![screen, nativeScale];
            let () = msg_send![view, setContentScaleFactor: scale_factor];
        }

        let _: *mut c_void = msg_send![view, retain];
        window::SurfaceInner::new(NonNull::new(view), render_layer)
    }

    #[cfg(target_os = "macos")]
    unsafe fn create_from_nsview(&self, nsview: *mut c_void) -> window::SurfaceInner {
        let view: cocoa::base::id = mem::transmute(nsview);
        if view.is_null() {
            panic!("window does not have a valid contentView");
        }

        let existing: CAMetalLayer = msg_send![view, layer];
        let class = class!(CAMetalLayer);
        // Deprecated! Clients should use `create_surface_from_layer` instead.
        let is_actually_layer: BOOL = msg_send![view, isKindOfClass: class];
        if is_actually_layer == YES {
            return self.create_from_layer(view);
        }

        let use_current = if existing.is_null() {
            false
        } else {
            let result: BOOL = msg_send![existing, isKindOfClass: class];
            result == YES
        };

        let render_layer: CAMetalLayer = if use_current {
            existing
        } else {
            let layer: CAMetalLayer = msg_send![class, new];
            let () = msg_send![view, setLayer: layer];
            let () = msg_send![view, setWantsLayer: YES];
            let bounds: CGRect = msg_send![view, bounds];
            let () = msg_send![layer, setBounds: bounds];

            let window: cocoa::base::id = msg_send![view, window];
            if !window.is_null() {
                let scale_factor: CGFloat = msg_send![window, backingScaleFactor];
                let () = msg_send![layer, setContentsScale: scale_factor];
            }
            let () = msg_send![layer, setDelegate: self.gfx_managed_metal_layer_delegate.0];
            layer
        };

        let _: *mut c_void = msg_send![view, retain];
        window::SurfaceInner::new(NonNull::new(view), render_layer)
    }

    unsafe fn create_from_layer(&self, layer: CAMetalLayer) -> window::SurfaceInner {
        let class = class!(CAMetalLayer);
        let proper_kind: BOOL = msg_send![layer, isKindOfClass: class];
        assert_eq!(proper_kind, YES);
        let _: *mut c_void = msg_send![layer, retain];
        window::SurfaceInner::new(None, layer)
    }

    pub fn create_surface_from_layer(
        &self,
        layer: CAMetalLayer,
        enable_signposts: bool,
    ) -> Surface {
        unsafe { self.create_from_layer(layer) }.into_surface(enable_signposts)
    }

    #[cfg(target_os = "macos")]
    pub fn create_surface_from_nsview(
        &self,
        nsview: *mut c_void,
        enable_signposts: bool,
    ) -> Surface {
        unsafe { self.create_from_nsview(nsview) }.into_surface(enable_signposts)
    }

    #[cfg(target_os = "ios")]
    pub fn create_surface_from_uiview(
        &self,
        uiview: *mut c_void,
        enable_signposts: bool,
    ) -> Surface {
        unsafe { self.create_from_uiview(uiview) }.into_surface(enable_signposts)
    }
}

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub enum Backend {}
impl hal::Backend for Backend {
    type Instance = Instance;
    type PhysicalDevice = device::PhysicalDevice;
    type Device = device::Device;

    type Surface = window::Surface;
    type Swapchain = window::Swapchain;

    type QueueFamily = QueueFamily;
    type CommandQueue = command::CommandQueue;
    type CommandBuffer = command::CommandBuffer;

    type Memory = native::Memory;
    type CommandPool = command::CommandPool;

    type ShaderModule = native::ShaderModule;
    type RenderPass = native::RenderPass;
    type Framebuffer = native::Framebuffer;

    type Buffer = native::Buffer;
    type BufferView = native::BufferView;
    type Image = native::Image;
    type ImageView = native::ImageView;
    type Sampler = native::Sampler;

    type ComputePipeline = native::ComputePipeline;
    type GraphicsPipeline = native::GraphicsPipeline;
    type PipelineCache = native::PipelineCache;
    type PipelineLayout = native::PipelineLayout;
    type DescriptorSetLayout = native::DescriptorSetLayout;
    type DescriptorPool = native::DescriptorPool;
    type DescriptorSet = native::DescriptorSet;

    type Fence = native::Fence;
    type Semaphore = native::Semaphore;
    type Event = native::Event;
    type QueryPool = native::QueryPool;
}

const RESOURCE_HEAP_SUPPORT: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v3,
    MTLFeatureSet::iOS_GPUFamily2_v3,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::tvOS_GPUFamily1_v2,
];

const ARGUMENT_BUFFER_SUPPORT: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v4,
    MTLFeatureSet::tvOS_GPUFamily1_v3,
    MTLFeatureSet::macOS_GPUFamily1_v3,
];

const MUTABLE_COMPARISON_SAMPLER_SUPPORT: &[MTLFeatureSet] = &[
    MTLFeatureSet::macOS_GPUFamily1_v1,
    MTLFeatureSet::iOS_GPUFamily3_v1,
];

const ASTC_PIXEL_FORMAT_FEATURES: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily2_v1,
    MTLFeatureSet::iOS_GPUFamily2_v2,
    MTLFeatureSet::iOS_GPUFamily3_v1,
    MTLFeatureSet::iOS_GPUFamily2_v3,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::iOS_GPUFamily2_v4,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily1_v1,
    MTLFeatureSet::tvOS_GPUFamily1_v2,
    MTLFeatureSet::tvOS_GPUFamily1_v3,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
];

const R8UNORM_SRGB_ALL: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily3_v1,
    MTLFeatureSet::iOS_GPUFamily2_v3,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::iOS_GPUFamily2_v4,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily1_v2,
    MTLFeatureSet::tvOS_GPUFamily1_v3,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
];

const R8SNORM_NO_RESOLVE: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v1,
    MTLFeatureSet::iOS_GPUFamily1_v2,
    MTLFeatureSet::iOS_GPUFamily1_v3,
    MTLFeatureSet::iOS_GPUFamily1_v4,
];

const RG8UNORM_SRGB_NO_WRITE: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v1,
    MTLFeatureSet::iOS_GPUFamily2_v1,
    MTLFeatureSet::iOS_GPUFamily1_v2,
    MTLFeatureSet::iOS_GPUFamily2_v2,
    MTLFeatureSet::iOS_GPUFamily1_v3,
    MTLFeatureSet::iOS_GPUFamily1_v4,
    MTLFeatureSet::tvOS_GPUFamily1_v1,
];

const RG8SNORM_NO_RESOLVE: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v1,
    MTLFeatureSet::iOS_GPUFamily1_v2,
    MTLFeatureSet::iOS_GPUFamily1_v3,
    MTLFeatureSet::iOS_GPUFamily1_v4,
];

const RGBA8_SRGB: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily3_v1,
    MTLFeatureSet::iOS_GPUFamily2_v3,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::iOS_GPUFamily2_v4,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily1_v2,
    MTLFeatureSet::tvOS_GPUFamily1_v3,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
];

const RGB10A2UNORM_ALL: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily3_v1,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
    MTLFeatureSet::macOS_GPUFamily1_v1,
    MTLFeatureSet::macOS_GPUFamily1_v2,
    MTLFeatureSet::macOS_GPUFamily1_v3,
];

const RGB10A2UINT_COLOR_WRITE: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily3_v1,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
    MTLFeatureSet::macOS_GPUFamily1_v1,
    MTLFeatureSet::macOS_GPUFamily1_v2,
    MTLFeatureSet::macOS_GPUFamily1_v3,
];

const RG11B10FLOAT_ALL: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily3_v1,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
    MTLFeatureSet::macOS_GPUFamily1_v1,
    MTLFeatureSet::macOS_GPUFamily1_v2,
    MTLFeatureSet::macOS_GPUFamily1_v3,
];

const RGB9E5FLOAT_ALL: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily3_v1,
    MTLFeatureSet::iOS_GPUFamily3_v2,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
];

const BGR10A2_ALL: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v4,
    MTLFeatureSet::iOS_GPUFamily2_v4,
    MTLFeatureSet::iOS_GPUFamily3_v3,
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily1_v3,
    MTLFeatureSet::tvOS_GPUFamily2_v1,
];

const BASE_INSTANCE_SUPPORT: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v4,
    MTLFeatureSet::iOS_GPUFamily3_v1,
];

const DUAL_SOURCE_BLEND_SUPPORT: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v4,
    MTLFeatureSet::tvOS_GPUFamily1_v3,
    MTLFeatureSet::macOS_GPUFamily1_v2,
];

const LAYERED_RENDERING_SUPPORT: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily5_v1,
    MTLFeatureSet::macOS_GPUFamily1_v1,
];

const FUNCTION_SPECIALIZATION_SUPPORT: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily1_v3,
    MTLFeatureSet::tvOS_GPUFamily1_v2,
    MTLFeatureSet::macOS_GPUFamily1_v2,
];

const DEPTH_CLIP_MODE: &[MTLFeatureSet] = &[
    MTLFeatureSet::iOS_GPUFamily4_v1,
    MTLFeatureSet::tvOS_GPUFamily1_v3,
    MTLFeatureSet::macOS_GPUFamily1_v1,
];

#[derive(Clone, Debug)]
struct PrivateCapabilities {
    pub os_is_mac: bool,
    os_version: (u32, u32),
    msl_version: metal::MTLLanguageVersion,
    exposed_queues: usize,
    // if TRUE, we'll report `NON_FILL_POLYGON_MODE` feature without the points support
    expose_line_mode: bool,
    resource_heaps: bool,
    argument_buffers: bool,
    shared_textures: bool,
    mutable_comparison_samplers: bool,
    base_instance: bool,
    base_vertex_instance_drawing: bool,
    dual_source_blending: bool,
    low_power: bool,
    headless: bool,
    layered_rendering: bool,
    function_specialization: bool,
    depth_clip_mode: bool,
    format_depth24_stencil8: bool,
    format_depth32_stencil8_filter: bool,
    format_depth32_stencil8_none: bool,
    format_min_srgb_channels: u8,
    format_b5: bool,
    format_bc: bool,
    format_eac_etc: bool,
    format_astc: bool,
    format_r8unorm_srgb_all: bool,
    format_r8unorm_srgb_no_write: bool,
    format_r8snorm_all: bool,
    format_r16_norm_all: bool,
    format_rg8unorm_srgb_all: bool,
    format_rg8unorm_srgb_no_write: bool,
    format_rg8snorm_all: bool,
    format_r32_all: bool,
    format_r32_no_write: bool,
    format_r32float_no_write_no_filter: bool,
    format_r32float_no_filter: bool,
    format_r32float_all: bool,
    format_rgba8_srgb_all: bool,
    format_rgba8_srgb_no_write: bool,
    format_rgb10a2_unorm_all: bool,
    format_rgb10a2_unorm_no_write: bool,
    format_rgb10a2_uint_color: bool,
    format_rgb10a2_uint_color_write: bool,
    format_rg11b10_all: bool,
    format_rg11b10_no_write: bool,
    format_rgb9e5_all: bool,
    format_rgb9e5_no_write: bool,
    format_rgb9e5_filter_only: bool,
    format_rg32_color: bool,
    format_rg32_color_write: bool,
    format_rg32float_all: bool,
    format_rg32float_color_blend: bool,
    format_rg32float_no_filter: bool,
    format_rgba32int_color: bool,
    format_rgba32int_color_write: bool,
    format_rgba32float_color: bool,
    format_rgba32float_color_write: bool,
    format_rgba32float_all: bool,
    format_depth16unorm: bool,
    format_depth32float_filter: bool,
    format_depth32float_none: bool,
    format_bgr10a2_all: bool,
    format_bgr10a2_no_write: bool,
    max_buffers_per_stage: ResourceIndex,
    max_textures_per_stage: ResourceIndex,
    max_samplers_per_stage: ResourceIndex,
    buffer_alignment: u64,
    max_buffer_size: u64,
    max_texture_size: u64,
    max_texture_3d_size: u64,
    max_texture_layers: u64,
    max_fragment_input_components: u64,
    sample_count_mask: u8,
    supports_debug_markers: bool,
}

impl PrivateCapabilities {
    fn version_at_least(major: u32, minor: u32, needed_major: u32, needed_minor: u32) -> bool {
        major > needed_major || (major == needed_major && minor >= needed_minor)
    }

    fn supports_any(raw: &metal::DeviceRef, features_sets: &[MTLFeatureSet]) -> bool {
        features_sets
            .iter()
            .cloned()
            .any(|x| raw.supports_feature_set(x))
    }

    fn new(device: &metal::Device, experiments: &Experiments) -> Self {
        #[repr(C)]
        #[derive(Clone, Copy, Debug)]
        struct NSOperatingSystemVersion {
            major: NSInteger,
            minor: NSInteger,
            patch: NSInteger,
        }

        let version: NSOperatingSystemVersion = unsafe {
            let process_info: *mut Object = msg_send![class!(NSProcessInfo), processInfo];
            msg_send![process_info, operatingSystemVersion]
        };

        let major = version.major as u32;
        let minor = version.minor as u32;
        let os_is_mac = device.supports_feature_set(MTLFeatureSet::macOS_GPUFamily1_v1);

        let mut sample_count_mask: u8 = 1 | 4; // 1 and 4 samples are supported on all devices
        if device.supports_sample_count(2) {
            sample_count_mask |= 2;
        }
        if device.supports_sample_count(8) {
            sample_count_mask |= 8;
        }

        PrivateCapabilities {
            os_is_mac,
            os_version: (major as u32, minor as u32),
            msl_version: if os_is_mac {
                if Self::version_at_least(major, minor, 10, 15) {
                    MTLLanguageVersion::V2_2
                } else if Self::version_at_least(major, minor, 10, 14) {
                    MTLLanguageVersion::V2_1
                } else if Self::version_at_least(major, minor, 10, 13) {
                    MTLLanguageVersion::V2_0
                } else if Self::version_at_least(major, minor, 10, 12) {
                    MTLLanguageVersion::V1_2
                } else if Self::version_at_least(major, minor, 10, 11) {
                    MTLLanguageVersion::V1_1
                } else {
                    MTLLanguageVersion::V1_0
                }
            } else if Self::version_at_least(major, minor, 13, 0) {
                MTLLanguageVersion::V2_2
            } else if Self::version_at_least(major, minor, 12, 0) {
                MTLLanguageVersion::V2_1
            } else if Self::version_at_least(major, minor, 11, 0) {
                MTLLanguageVersion::V2_0
            } else if Self::version_at_least(major, minor, 10, 0) {
                MTLLanguageVersion::V1_2
            } else if Self::version_at_least(major, minor, 9, 0) {
                MTLLanguageVersion::V1_1
            } else {
                MTLLanguageVersion::V1_0
            },
            exposed_queues: 1,
            expose_line_mode: true,
            resource_heaps: Self::supports_any(&device, RESOURCE_HEAP_SUPPORT),
            argument_buffers: experiments.argument_buffers
                && Self::supports_any(&device, ARGUMENT_BUFFER_SUPPORT),
            shared_textures: !os_is_mac,
            mutable_comparison_samplers: Self::supports_any(
                &device,
                MUTABLE_COMPARISON_SAMPLER_SUPPORT,
            ),
            base_instance: Self::supports_any(&device, BASE_INSTANCE_SUPPORT),
            base_vertex_instance_drawing: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily3_v1,
                    MTLFeatureSet::iOS_GPUFamily4_v1,
                    MTLFeatureSet::iOS_GPUFamily5_v1,
                    MTLFeatureSet::tvOS_GPUFamily2_v1,
                    MTLFeatureSet::macOS_GPUFamily1_v1,
                    MTLFeatureSet::macOS_GPUFamily2_v1,
                ],
            ),
            dual_source_blending: Self::supports_any(&device, DUAL_SOURCE_BLEND_SUPPORT),
            low_power: !os_is_mac || device.is_low_power(),
            headless: os_is_mac && device.is_headless(),
            layered_rendering: Self::supports_any(&device, LAYERED_RENDERING_SUPPORT),
            function_specialization: Self::supports_any(&device, FUNCTION_SPECIALIZATION_SUPPORT),
            depth_clip_mode: Self::supports_any(&device, DEPTH_CLIP_MODE),
            format_depth24_stencil8: os_is_mac && device.d24_s8_supported(),
            format_depth32_stencil8_filter: os_is_mac,
            format_depth32_stencil8_none: !os_is_mac,
            format_min_srgb_channels: if os_is_mac { 4 } else { 1 },
            format_b5: !os_is_mac,
            format_bc: os_is_mac,
            format_eac_etc: !os_is_mac,
            format_astc: Self::supports_any(&device, ASTC_PIXEL_FORMAT_FEATURES),
            format_r8unorm_srgb_all: Self::supports_any(&device, R8UNORM_SRGB_ALL),
            format_r8unorm_srgb_no_write: !Self::supports_any(&device, R8UNORM_SRGB_ALL)
                && !os_is_mac,
            format_r8snorm_all: !Self::supports_any(&device, R8SNORM_NO_RESOLVE),
            format_r16_norm_all: os_is_mac,
            format_rg8unorm_srgb_all: Self::supports_any(&device, RG8UNORM_SRGB_NO_WRITE),
            format_rg8unorm_srgb_no_write: !Self::supports_any(&device, RG8UNORM_SRGB_NO_WRITE)
                && !os_is_mac,
            format_rg8snorm_all: !Self::supports_any(&device, RG8SNORM_NO_RESOLVE),
            format_r32_all: !Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_r32_no_write: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_r32float_no_write_no_filter: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ) && !os_is_mac,
            format_r32float_no_filter: !Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ) && !os_is_mac,
            format_r32float_all: os_is_mac,
            format_rgba8_srgb_all: Self::supports_any(&device, RGBA8_SRGB),
            format_rgba8_srgb_no_write: !Self::supports_any(&device, RGBA8_SRGB),
            format_rgb10a2_unorm_all: Self::supports_any(&device, RGB10A2UNORM_ALL),
            format_rgb10a2_unorm_no_write: !Self::supports_any(&device, RGB10A2UNORM_ALL),
            format_rgb10a2_uint_color: !Self::supports_any(&device, RGB10A2UINT_COLOR_WRITE),
            format_rgb10a2_uint_color_write: Self::supports_any(&device, RGB10A2UINT_COLOR_WRITE),
            format_rg11b10_all: Self::supports_any(&device, RG11B10FLOAT_ALL),
            format_rg11b10_no_write: !Self::supports_any(&device, RG11B10FLOAT_ALL),
            format_rgb9e5_all: Self::supports_any(&device, RGB9E5FLOAT_ALL),
            format_rgb9e5_no_write: !Self::supports_any(&device, RGB9E5FLOAT_ALL) && !os_is_mac,
            format_rgb9e5_filter_only: os_is_mac,
            format_rg32_color: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_rg32_color_write: !Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_rg32float_all: os_is_mac,
            format_rg32float_color_blend: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_rg32float_no_filter: !os_is_mac
                && !Self::supports_any(
                    &device,
                    &[
                        MTLFeatureSet::iOS_GPUFamily1_v1,
                        MTLFeatureSet::iOS_GPUFamily2_v1,
                    ],
                ),
            format_rgba32int_color: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_rgba32int_color_write: !Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_rgba32float_color: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ),
            format_rgba32float_color_write: !Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v1,
                    MTLFeatureSet::iOS_GPUFamily2_v1,
                ],
            ) && !os_is_mac,
            format_rgba32float_all: os_is_mac,
            format_depth16unorm: device.supports_feature_set(MTLFeatureSet::macOS_GPUFamily1_v2),
            format_depth32float_filter: device
                .supports_feature_set(MTLFeatureSet::macOS_GPUFamily1_v1),
            format_depth32float_none: !device
                .supports_feature_set(MTLFeatureSet::macOS_GPUFamily1_v1),
            format_bgr10a2_all: Self::supports_any(&device, BGR10A2_ALL),
            format_bgr10a2_no_write: !device
                .supports_feature_set(MTLFeatureSet::macOS_GPUFamily1_v3),
            max_buffers_per_stage: 31,
            max_textures_per_stage: if os_is_mac { 128 } else { 31 },
            max_samplers_per_stage: 16,
            buffer_alignment: if os_is_mac { 256 } else { 64 },
            max_buffer_size: if device.supports_feature_set(MTLFeatureSet::macOS_GPUFamily1_v2) {
                1 << 30 // 1GB on macOS 1.2 and up
            } else {
                1 << 28 // 256MB otherwise
            },
            max_texture_size: if Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily3_v1,
                    MTLFeatureSet::tvOS_GPUFamily2_v1,
                    MTLFeatureSet::macOS_GPUFamily1_v1,
                ],
            ) {
                16384
            } else if Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::iOS_GPUFamily1_v2,
                    MTLFeatureSet::iOS_GPUFamily2_v2,
                    MTLFeatureSet::tvOS_GPUFamily1_v1,
                ],
            ) {
                8192
            } else {
                4096
            },
            max_texture_3d_size: 2048,
            max_texture_layers: 2048,
            max_fragment_input_components: if os_is_mac { 128 } else { 60 },
            sample_count_mask,
            supports_debug_markers: Self::supports_any(
                &device,
                &[
                    MTLFeatureSet::macOS_GPUFamily1_v2,
                    MTLFeatureSet::macOS_GPUFamily2_v1,
                    MTLFeatureSet::iOS_GPUFamily1_v3,
                    MTLFeatureSet::iOS_GPUFamily2_v3,
                    MTLFeatureSet::iOS_GPUFamily3_v2,
                    MTLFeatureSet::iOS_GPUFamily4_v1,
                    MTLFeatureSet::iOS_GPUFamily5_v1,
                    MTLFeatureSet::tvOS_GPUFamily1_v2,
                    MTLFeatureSet::tvOS_GPUFamily2_v1,
                ],
            ),
        }
    }

    fn has_version_at_least(&self, needed_major: u32, needed_minor: u32) -> bool {
        let (major, minor) = self.os_version;
        Self::version_at_least(major, minor, needed_major, needed_minor)
    }
}

#[derive(Clone, Copy, Debug)]
struct PrivateDisabilities {
    /// Near depth is not respected properly on some Intel GPUs.
    broken_viewport_near_depth: bool,
    /// Multi-target clears don't appear to work properly on Intel GPUs.
    broken_layered_clear_image: bool,
}

trait AsNative {
    type Native;
    fn from(native: &Self::Native) -> Self;
    fn as_native(&self) -> &Self::Native;
}

pub type BufferPtr = NonNull<metal::MTLBuffer>;
pub type TexturePtr = NonNull<metal::MTLTexture>;
pub type SamplerPtr = NonNull<metal::MTLSamplerState>;
pub type ResourcePtr = NonNull<metal::MTLResource>;

//TODO: make this a generic struct with a single generic implementation

impl AsNative for BufferPtr {
    type Native = metal::BufferRef;
    #[inline]
    fn from(native: &metal::BufferRef) -> Self {
        unsafe { NonNull::new_unchecked(native.as_ptr()) }
    }
    #[inline]
    fn as_native(&self) -> &metal::BufferRef {
        unsafe { metal::BufferRef::from_ptr(self.as_ptr()) }
    }
}

impl AsNative for TexturePtr {
    type Native = metal::TextureRef;
    #[inline]
    fn from(native: &metal::TextureRef) -> Self {
        unsafe { NonNull::new_unchecked(native.as_ptr()) }
    }
    #[inline]
    fn as_native(&self) -> &metal::TextureRef {
        unsafe { metal::TextureRef::from_ptr(self.as_ptr()) }
    }
}

impl AsNative for SamplerPtr {
    type Native = metal::SamplerStateRef;
    #[inline]
    fn from(native: &metal::SamplerStateRef) -> Self {
        unsafe { NonNull::new_unchecked(native.as_ptr()) }
    }
    #[inline]
    fn as_native(&self) -> &metal::SamplerStateRef {
        unsafe { metal::SamplerStateRef::from_ptr(self.as_ptr()) }
    }
}

impl AsNative for ResourcePtr {
    type Native = metal::ResourceRef;
    #[inline]
    fn from(native: &metal::ResourceRef) -> Self {
        unsafe { NonNull::new_unchecked(native.as_ptr()) }
    }
    #[inline]
    fn as_native(&self) -> &metal::ResourceRef {
        unsafe { metal::ResourceRef::from_ptr(self.as_ptr()) }
    }
}
