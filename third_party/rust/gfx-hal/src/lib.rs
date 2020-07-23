#![deny(missing_debug_implementations, missing_docs, unused)]

//! Low-level graphics abstraction for Rust. Mostly operates on data, not types.
//! Designed for use by libraries and higher-level abstractions only.

#[macro_use]
extern crate bitflags;

#[cfg(feature = "serde")]
#[macro_use]
extern crate serde;

use std::any::Any;
use std::fmt;
use std::hash::Hash;

pub mod adapter;
pub mod buffer;
pub mod command;
pub mod device;
pub mod format;
pub mod image;
pub mod memory;
pub mod pass;
pub mod pool;
pub mod pso;
pub mod query;
pub mod queue;
pub mod window;

/// Prelude module re-exports all the traits necessary to use gfx-hal.
pub mod prelude {
    pub use crate::{
        adapter::PhysicalDevice as _,
        command::CommandBuffer as _,
        device::Device as _,
        pool::CommandPool as _,
        pso::DescriptorPool as _,
        queue::{CommandQueue as _, QueueFamily as _},
        window::{PresentationSurface as _, Surface as _, Swapchain as _},
        Instance as _,
    };
}

/// Draw vertex count.
pub type VertexCount = u32;
/// Draw vertex base offset.
pub type VertexOffset = i32;
/// Draw number of indices.
pub type IndexCount = u32;
/// Draw number of instances.
pub type InstanceCount = u32;
/// Indirect draw calls count.
pub type DrawCount = u32;
/// Number of work groups.
pub type WorkGroupCount = [u32; 3];

bitflags! {
    //TODO: add a feature for non-normalized samplers
    //TODO: add a feature for mutable comparison samplers
    /// Features that the device supports.
    /// These only include features of the core interface and not API extensions.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Features: u128 {
        /// Bit mask of Vulkan Core features.
        const CORE_MASK = 0xFFFF_FFFF_FFFF_FFFF;
        /// Bit mask of Vulkan Portability features.
        const PORTABILITY_MASK  = 0x0000_FFFF_0000_0000_0000_0000;
        /// Bit mask for extra WebGPU features.
        const WEBGPU_MASK = 0xFFFF_0000_0000_0000_0000_0000;

        /// Support for robust buffer access.
        /// Buffer access by SPIR-V shaders is checked against the buffer/image boundaries.
        const ROBUST_BUFFER_ACCESS = 0x0000_0000_0000_0001;
        /// Support the full 32-bit range of indexed for draw calls.
        /// If not supported, the maximum index value is determined by `Limits::max_draw_index_value`.
        const FULL_DRAW_INDEX_U32 = 0x0000_0000_0000_0002;
        /// Support cube array image views.
        const IMAGE_CUBE_ARRAY = 0x0000_0000_0000_0004;
        /// Support different color blending settings per attachments on graphics pipeline creation.
        const INDEPENDENT_BLENDING = 0x0000_0000_0000_0008;
        /// Support geometry shader.
        const GEOMETRY_SHADER = 0x0000_0000_0000_0010;
        /// Support tessellation shaders.
        const TESSELLATION_SHADER = 0x0000_0000_0000_0020;
        /// Support per-sample shading and multisample interpolation.
        const SAMPLE_RATE_SHADING = 0x0000_0000_0000_0040;
        /// Support dual source blending.
        const DUAL_SRC_BLENDING = 0x0000_0000_0000_0080;
        /// Support logic operations.
        const LOGIC_OP = 0x0000_0000_0000_0100;
        /// Support multiple draws per indirect call.
        const MULTI_DRAW_INDIRECT = 0x0000_0000_0000_0200;
        /// Support indirect drawing with first instance value.
        /// If not supported the first instance value **must** be 0.
        const DRAW_INDIRECT_FIRST_INSTANCE = 0x0000_0000_0000_0400;
        /// Support depth clamping.
        const DEPTH_CLAMP = 0x0000_0000_0000_0800;
        /// Support depth bias clamping.
        const DEPTH_BIAS_CLAMP = 0x0000_0000_0000_1000;
        /// Support non-fill polygon modes.
        const NON_FILL_POLYGON_MODE = 0x0000_0000_0000_2000;
        /// Support depth bounds test.
        const DEPTH_BOUNDS = 0x0000_0000_0000_4000;
        /// Support lines with width other than 1.0.
        const LINE_WIDTH = 0x0000_0000_0000_8000;
        /// Support points with size greater than 1.0.
        const POINT_SIZE = 0x0000_0000_0001_0000;
        /// Support replacing alpha values with 1.0.
        const ALPHA_TO_ONE = 0x0000_0000_0002_0000;
        /// Support multiple viewports and scissors.
        const MULTI_VIEWPORTS = 0x0000_0000_0004_0000;
        /// Support anisotropic filtering.
        const SAMPLER_ANISOTROPY = 0x0000_0000_0008_0000;
        /// Support ETC2 texture compression formats.
        const FORMAT_ETC2 = 0x0000_0000_0010_0000;
        /// Support ASTC (LDR) texture compression formats.
        const FORMAT_ASTC_LDR = 0x0000_0000_0020_0000;
        /// Support BC texture compression formats.
        const FORMAT_BC = 0x0000_0000_0040_0000;
        /// Support precise occlusion queries, returning the actual number of samples.
        /// If not supported, queries return a non-zero value when at least **one** sample passes.
        const PRECISE_OCCLUSION_QUERY = 0x0000_0000_0080_0000;
        /// Support query of pipeline statistics.
        const PIPELINE_STATISTICS_QUERY = 0x0000_0000_0100_0000;
        /// Support unordered access stores and atomic ops in the vertex, geometry
        /// and tessellation shader stage.
        /// If not supported, the shader resources **must** be annotated as read-only.
        const VERTEX_STORES_AND_ATOMICS = 0x0000_0000_0200_0000;
        /// Support unordered access stores and atomic ops in the fragment shader stage
        /// If not supported, the shader resources **must** be annotated as read-only.
        const FRAGMENT_STORES_AND_ATOMICS = 0x0000_0000_0400_0000;
        ///
        const SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE = 0x0000_0000_0800_0000;
        ///
        const SHADER_IMAGE_GATHER_EXTENDED = 0x0000_0000_1000_0000;
        ///
        const SHADER_STORAGE_IMAGE_EXTENDED_FORMATS = 0x0000_0000_2000_0000;
        ///
        const SHADER_STORAGE_IMAGE_MULTISAMPLE = 0x0000_0000_4000_0000;
        ///
        const SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT = 0x0000_0000_8000_0000;
        ///
        const SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT = 0x0000_0001_0000_0000;
        ///
        const SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING = 0x0000_0002_0000_0000;
        ///
        const SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING = 0x0000_0004_0000_0000;
        ///
        const SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING = 0x0000_0008_0000_0000;
        ///
        const SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING = 0x0000_0010_0000_0000;
        ///
        const SHADER_CLIP_DISTANCE = 0x0000_0020_0000_0000;
        ///
        const SHADER_CULL_DISTANCE = 0x0000_0040_0000_0000;
        ///
        const SHADER_FLOAT64 = 0x0000_0080_0000_0000;
        ///
        const SHADER_INT64 = 0x0000_0100_0000_0000;
        ///
        const SHADER_INT16 = 0x0000_0200_0000_0000;
        ///
        const SHADER_RESOURCE_RESIDENCY = 0x0000_0400_0000_0000;
        ///
        const SHADER_RESOURCE_MIN_LOD = 0x0000_0800_0000_0000;
        ///
        const SPARSE_BINDING = 0x0000_1000_0000_0000;
        ///
        const SPARSE_RESIDENCY_BUFFER = 0x0000_2000_0000_0000;
        ///
        const SPARSE_RESIDENCY_IMAGE_2D = 0x0000_4000_0000_0000;
        ///
        const SPARSE_RESIDENCY_IMAGE_3D = 0x0000_8000_0000_0000;
        ///
        const SPARSE_RESIDENCY_2_SAMPLES = 0x0001_0000_0000_0000;
        ///
        const SPARSE_RESIDENCY_4_SAMPLES = 0x0002_0000_0000_0000;
        ///
        const SPARSE_RESIDENCY_8_SAMPLES = 0x0004_0000_0000_0000;
        ///
        const SPARSE_RESIDENCY_16_SAMPLES = 0x0008_0000_0000_0000;
        ///
        const SPARSE_RESIDENCY_ALIASED = 0x0010_0000_0000_0000;
        ///
        const VARIABLE_MULTISAMPLE_RATE = 0x0020_0000_0000_0000;
        ///
        const INHERITED_QUERIES = 0x0040_0000_0000_0000;
        /// Support for
        const SAMPLER_MIRROR_CLAMP_EDGE = 0x0100_0000_0000_0000;

        /// Support triangle fan primitive topology.
        const TRIANGLE_FAN = 0x0001 << 64;
        /// Support separate stencil reference values for front and back sides.
        const SEPARATE_STENCIL_REF_VALUES = 0x0002 << 64;
        /// Support manually specified vertex attribute rates (divisors).
        const INSTANCE_RATE = 0x0004 << 64;
        /// Support non-zero mipmap bias on samplers.
        const SAMPLER_MIP_LOD_BIAS = 0x0008 << 64;

        /// Make the NDC coordinate system pointing Y up, to match D3D and Metal.
        const NDC_Y_UP = 0x01 << 80;
    }
}

bitflags! {
    /// Features that the device supports natively, but is able to emulate.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Hints: u32 {
        /// Support indexed, instanced drawing with base vertex and instance.
        const BASE_VERTEX_INSTANCE_DRAWING = 0x0001;
    }
}

/// Resource limits of a particular graphics device.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Limits {
    /// Maximum supported image 1D size.
    pub max_image_1d_size: image::Size,
    /// Maximum supported image 2D size.
    pub max_image_2d_size: image::Size,
    /// Maximum supported image 3D size.
    pub max_image_3d_size: image::Size,
    /// Maximum supported image cube size.
    pub max_image_cube_size: image::Size,
    /// Maximum supporter image array size.
    pub max_image_array_layers: image::Layer,
    /// Maximum number of elements for the BufferView to see.
    pub max_texel_elements: usize,
    ///
    pub max_uniform_buffer_range: buffer::Offset,
    ///
    pub max_storage_buffer_range: buffer::Offset,
    ///
    pub max_push_constants_size: usize,
    ///
    pub max_memory_allocation_count: usize,
    ///
    pub max_sampler_allocation_count: usize,
    ///
    pub max_bound_descriptor_sets: pso::DescriptorSetIndex,
    ///
    pub max_framebuffer_layers: usize,
    ///
    pub max_per_stage_descriptor_samplers: usize,
    ///
    pub max_per_stage_descriptor_uniform_buffers: usize,
    ///
    pub max_per_stage_descriptor_storage_buffers: usize,
    ///
    pub max_per_stage_descriptor_sampled_images: usize,
    ///
    pub max_per_stage_descriptor_storage_images: usize,
    ///
    pub max_per_stage_descriptor_input_attachments: usize,
    ///
    pub max_per_stage_resources: usize,

    ///
    pub max_descriptor_set_samplers: usize,
    ///
    pub max_descriptor_set_uniform_buffers: usize,
    ///
    pub max_descriptor_set_uniform_buffers_dynamic: usize,
    ///
    pub max_descriptor_set_storage_buffers: usize,
    ///
    pub max_descriptor_set_storage_buffers_dynamic: usize,
    ///
    pub max_descriptor_set_sampled_images: usize,
    ///
    pub max_descriptor_set_storage_images: usize,
    ///
    pub max_descriptor_set_input_attachments: usize,

    /// Maximum number of vertex input attributes that can be specified for a graphics pipeline.
    pub max_vertex_input_attributes: usize,
    /// Maximum number of vertex buffers that can be specified for providing vertex attributes to a graphics pipeline.
    pub max_vertex_input_bindings: usize,
    /// Maximum vertex input attribute offset that can be added to the vertex input binding stride.
    pub max_vertex_input_attribute_offset: usize,
    /// Maximum vertex input binding stride that can be specified in a vertex input binding.
    pub max_vertex_input_binding_stride: usize,
    /// Maximum number of components of output variables which can be output by a vertex shader.
    pub max_vertex_output_components: usize,

    /// Maximum number of vertices for each patch.
    pub max_patch_size: pso::PatchSize,
    ///
    pub max_geometry_shader_invocations: usize,
    ///
    pub max_geometry_input_components: usize,
    ///
    pub max_geometry_output_components: usize,
    ///
    pub max_geometry_output_vertices: usize,
    ///
    pub max_geometry_total_output_components: usize,
    ///
    pub max_fragment_input_components: usize,
    ///
    pub max_fragment_output_attachments: usize,
    ///
    pub max_fragment_dual_source_attachments: usize,
    ///
    pub max_fragment_combined_output_resources: usize,

    ///
    pub max_compute_shared_memory_size: usize,
    ///
    pub max_compute_work_group_count: WorkGroupCount,
    ///
    pub max_compute_work_group_invocations: usize,
    ///
    pub max_compute_work_group_size: [u32; 3],

    ///
    pub max_draw_indexed_index_value: IndexCount,
    ///
    pub max_draw_indirect_count: InstanceCount,

    ///
    pub max_sampler_lod_bias: f32,
    /// Maximum degree of sampler anisotropy.
    pub max_sampler_anisotropy: f32,

    /// Maximum number of viewports.
    pub max_viewports: usize,
    ///
    pub max_viewport_dimensions: [image::Size; 2],
    ///
    pub max_framebuffer_extent: image::Extent,

    ///
    pub min_memory_map_alignment: usize,
    ///
    pub buffer_image_granularity: buffer::Offset,
    /// The alignment of the start of buffer used as a texel buffer, in bytes, non-zero.
    pub min_texel_buffer_offset_alignment: buffer::Offset,
    /// The alignment of the start of buffer used for uniform buffer updates, in bytes, non-zero.
    pub min_uniform_buffer_offset_alignment: buffer::Offset,
    /// The alignment of the start of buffer used as a storage buffer, in bytes, non-zero.
    pub min_storage_buffer_offset_alignment: buffer::Offset,
    /// Number of samples supported for color attachments of framebuffers (floating/fixed point).
    pub framebuffer_color_sample_counts: image::NumSamples,
    /// Number of samples supported for depth attachments of framebuffers.
    pub framebuffer_depth_sample_counts: image::NumSamples,
    /// Number of samples supported for stencil attachments of framebuffers.
    pub framebuffer_stencil_sample_counts: image::NumSamples,
    /// Maximum number of color attachments that can be used by a subpass in a render pass.
    pub max_color_attachments: usize,
    ///
    pub standard_sample_locations: bool,
    /// The alignment of the start of the buffer used as a GPU copy source, in bytes, non-zero.
    pub optimal_buffer_copy_offset_alignment: buffer::Offset,
    /// The alignment of the row pitch of the texture data stored in a buffer that is
    /// used in a GPU copy operation, in bytes, non-zero.
    pub optimal_buffer_copy_pitch_alignment: buffer::Offset,
    /// Size and alignment in bytes that bounds concurrent access to host-mapped device memory.
    pub non_coherent_atom_size: usize,

    /// The alignment of the vertex buffer stride.
    pub min_vertex_input_binding_stride_alignment: buffer::Offset,
}

/// An enum describing the type of an index value in a slice's index buffer
#[allow(missing_docs)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum IndexType {
    U16,
    U32,
}

/// Error creating an instance of a backend on the platform that
/// doesn't support this backend.
#[derive(Clone, Debug, PartialEq)]
pub struct UnsupportedBackend;

/// An instantiated backend.
///
/// Any startup the backend needs to perform will be done when creating the type that implements
/// `Instance`.
///
/// # Examples
///
/// ```rust
/// # extern crate gfx_backend_empty;
/// # extern crate gfx_hal;
/// use gfx_backend_empty as backend;
/// use gfx_hal as hal;
///
/// // Create a concrete instance of our backend (this is backend-dependent and may be more
/// // complicated for some backends).
/// let instance = backend::Instance;
/// // We can get a list of the available adapters, which are either physical graphics
/// // devices, or virtual adapters. Because we are using the dummy `empty` backend,
/// // there will be nothing in this list.
/// for (idx, adapter) in hal::Instance::enumerate_adapters(&instance).iter().enumerate() {
///     println!("Adapter {}: {:?}", idx, adapter.info);
/// }
/// ```
pub trait Instance<B: Backend>: Any + Send + Sync + Sized {
    /// Create a new instance.
    fn create(name: &str, version: u32) -> Result<Self, UnsupportedBackend>;
    /// Return all available adapters.
    fn enumerate_adapters(&self) -> Vec<adapter::Adapter<B>>;
    /// Create a new surface.
    unsafe fn create_surface(
        &self,
        _: &impl raw_window_handle::HasRawWindowHandle,
    ) -> Result<B::Surface, window::InitError>;
    /// Destroy a surface.
    ///
    /// The surface shouldn't be destroyed before the attached
    /// swapchain is destroyed.
    unsafe fn destroy_surface(&self, surface: B::Surface);
}

/// A strongly-typed index to a particular `MemoryType`.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct MemoryTypeId(pub usize);

impl From<usize> for MemoryTypeId {
    fn from(id: usize) -> Self {
        MemoryTypeId(id)
    }
}

struct PseudoVec<T>(Option<T>);

impl<T> std::iter::Extend<T> for PseudoVec<T> {
    fn extend<I: IntoIterator<Item = T>>(&mut self, iter: I) {
        let mut iter = iter.into_iter();
        self.0 = iter.next();
        assert!(iter.next().is_none());
    }
}

/// The `Backend` trait wraps together all the types needed
/// for a graphics backend. Each backend module, such as OpenGL
/// or Metal, will implement this trait with its own concrete types.
#[allow(missing_docs)]
pub trait Backend: 'static + Sized + Eq + Clone + Hash + fmt::Debug + Any + Send + Sync {
    type Instance: Instance<Self>;
    type PhysicalDevice: adapter::PhysicalDevice<Self>;
    type Device: device::Device<Self>;

    type Surface: window::PresentationSurface<Self>;
    type Swapchain: window::Swapchain<Self>;

    type QueueFamily: queue::QueueFamily;
    type CommandQueue: queue::CommandQueue<Self>;
    type CommandBuffer: command::CommandBuffer<Self>;

    type ShaderModule: fmt::Debug + Any + Send + Sync;
    type RenderPass: fmt::Debug + Any + Send + Sync;
    type Framebuffer: fmt::Debug + Any + Send + Sync;

    type Memory: fmt::Debug + Any + Send + Sync;
    type CommandPool: pool::CommandPool<Self>;

    type Buffer: fmt::Debug + Any + Send + Sync;
    type BufferView: fmt::Debug + Any + Send + Sync;
    type Image: fmt::Debug + Any + Send + Sync;
    type ImageView: fmt::Debug + Any + Send + Sync;
    type Sampler: fmt::Debug + Any + Send + Sync;

    type ComputePipeline: fmt::Debug + Any + Send + Sync;
    type GraphicsPipeline: fmt::Debug + Any + Send + Sync;
    type PipelineCache: fmt::Debug + Any + Send + Sync;
    type PipelineLayout: fmt::Debug + Any + Send + Sync;
    type DescriptorPool: pso::DescriptorPool<Self>;
    type DescriptorSet: fmt::Debug + Any + Send + Sync;
    type DescriptorSetLayout: fmt::Debug + Any + Send + Sync;

    type Fence: fmt::Debug + Any + Send + Sync;
    type Semaphore: fmt::Debug + Any + Send + Sync;
    type Event: fmt::Debug + Any + Send + Sync;
    type QueryPool: fmt::Debug + Any + Send + Sync;
}
