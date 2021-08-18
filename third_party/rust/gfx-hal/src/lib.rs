#![warn(
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications
)]
#![deny(
    broken_intra_doc_links,
    missing_debug_implementations,
    missing_docs,
    unused
)]

//! Low-level graphics abstraction for Rust. Mostly operates on data, not types.
//! Designed for use by libraries and higher-level abstractions only.
//!
//! This crate provides a [hardware abstraction layer][hal] for [graphics adapters][gpus],
//! for both compute and graphics operations. The API design is heavily inspired by
//! the [Vulkan API](https://www.khronos.org/vulkan/), and borrows some of the terminology.
//!
//! [hal]: https://en.wikipedia.org/wiki/Hardware_abstraction
//! [gpus]: https://en.wikipedia.org/wiki/Video_card
//!
//! # Usage
//!
//! Most of the functionality is implemented in separate crates, one for each backend.
//! This crate only exposes a few generic traits and structures. You can import
//! all the necessary traits through the [`prelude`][prelude] module.
//!
//! The first step to using `gfx-hal` is to initialize one of the available
//! [backends][Backend], by creating an [`Instance`][Instance]. Then proceed by
//! [enumerating][Instance::enumerate_adapters] the available
//! [graphics adapters][adapter::Adapter] and querying their available
//! [features][Features] and [queues][queue::family::QueueFamily].
//!
//! You can use the [`open`][adapter::PhysicalDevice::open] method on a
//! [`PhysicalDevice`][adapter::PhysicalDevice] to get a [logical device
//! handle][device::Device], from which you can manage all the other device-specific
//! resources.

#[macro_use]
extern crate bitflags;

#[cfg(feature = "serde")]
#[macro_use]
extern crate serde;

use std::{any::Any, fmt, hash::Hash};

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

/// Prelude module re-exports all the traits necessary to use `gfx-hal`.
pub mod prelude {
    pub use crate::{
        adapter::PhysicalDevice,
        command::CommandBuffer,
        device::Device,
        pool::CommandPool,
        pso::DescriptorPool,
        queue::{Queue, QueueFamily},
        window::{PresentationSurface, Surface},
        Instance,
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
/// Number of tasks.
pub type TaskCount = u32;

bitflags! {
    //TODO: add a feature for non-normalized samplers
    //TODO: add a feature for mutable comparison samplers
    /// Features that the device supports.
    ///
    /// These only include features of the core interface and not API extensions.
    ///
    /// Can be obtained from a [physical device][adapter::PhysicalDevice] by calling
    /// [`features`][adapter::PhysicalDevice::features].
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Features: u128 {
        /// Bit mask of Vulkan Core/Extension features.
        const CORE_MASK = 0xFFFF_FFFF_FFFF_FFFF;
        /// Bit mask of Vulkan Portability features.
        const PORTABILITY_MASK  = 0xFFFF << 64;
        /// Bit mask for extra WebGPU features.
        const WEBGPU_MASK = 0xFFFF << 80;
        /// Bit mask for all extensions.
        const EXTENSIONS_MASK = 0xFFFF_FFFF << 96;

        // Bits for Vulkan Core/Extension features

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
        /// Support for arrays of texture descriptors
        const TEXTURE_DESCRIPTOR_ARRAY = 0x0080_0000_0000_0000;
        /// Support for
        const SAMPLER_MIRROR_CLAMP_EDGE = 0x0100_0000_0000_0000;
        /// Allow indexing sampled texture descriptor arrays with dynamically non-uniform data
        const SAMPLED_TEXTURE_DESCRIPTOR_INDEXING = 0x0200_0000_0000_0000;
        /// Allow indexing storage texture descriptor arrays with dynamically non-uniform data
        const STORAGE_TEXTURE_DESCRIPTOR_INDEXING = 0x0400_0000_0000_0000;
        /// Allow descriptor arrays to be unsized in shaders
        const UNSIZED_DESCRIPTOR_ARRAY = 0x0800_0000_0000_0000;
        /// Mask for all the features associated with descriptor indexing.
        const DESCRIPTOR_INDEXING_MASK = Features::SAMPLED_TEXTURE_DESCRIPTOR_INDEXING.bits | Features::STORAGE_TEXTURE_DESCRIPTOR_INDEXING.bits | Features::UNSIZED_DESCRIPTOR_ARRAY.bits | Features::UNIFORM_BUFFER_DESCRIPTOR_INDEXING.bits | Features::STORAGE_BUFFER_DESCRIPTOR_INDEXING.bits;

        /// Enable draw_indirect_count and draw_indexed_indirect_count
        const DRAW_INDIRECT_COUNT = 0x1000_0000_0000_0000;

        /// Support for conservative rasterization. Presence of this flag only indicates basic overestimation rasterization for triangles only.
        /// (no guarantee on underestimation, overestimation, handling of degenerate primitives, fragment shader coverage reporting and uncertainty ranges)
        const CONSERVATIVE_RASTERIZATION = 0x2000_0000_0000_0000;

        /// Support for arrays of buffer descriptors
        const BUFFER_DESCRIPTOR_ARRAY = 0x4000_0000_0000_0000;
        /// Allow indexing uniform buffer descriptor arrays with dynamically non-uniform data
        const UNIFORM_BUFFER_DESCRIPTOR_INDEXING = 0x8000_0000_0000_0000;
        /// Allow indexing storage buffer descriptor arrays with dynamically non-uniform data
        const STORAGE_BUFFER_DESCRIPTOR_INDEXING = 0x0001_0000_0000_0000_0000;

        // Bits for Vulkan Portability features

        /// Support triangle fan primitive topology.
        const TRIANGLE_FAN = 0x0001 << 64;
        /// Support separate stencil reference values for front and back sides.
        const SEPARATE_STENCIL_REF_VALUES = 0x0002 << 64;
        /// Support manually specified vertex attribute rates (divisors).
        const INSTANCE_RATE = 0x0004 << 64;
        /// Support non-zero mipmap bias on samplers.
        const SAMPLER_MIP_LOD_BIAS = 0x0008 << 64;
        /// Support sampler wrap mode that clamps to border.
        const SAMPLER_BORDER_COLOR = 0x0010 << 64;
        /// Can create comparison samplers in regular descriptor sets.
        const MUTABLE_COMPARISON_SAMPLER = 0x0020 << 64;
        /// Can create non-normalized samplers in regular descriptor sets.
        const MUTABLE_UNNORMALIZED_SAMPLER = 0x0040 << 64;

        // Bits for WebGPU features

        /// Make the NDC coordinate system pointing Y up, to match D3D and Metal.
        const NDC_Y_UP = 0x0001 << 80;

        // Bits for Extensions

        /// Supports task shader stage.
        const TASK_SHADER = 0x0001 << 96;
        /// Supports mesh shader stage.
        const MESH_SHADER = 0x0002 << 96;
        /// Mask for all the features associated with mesh shader stages.
        const MESH_SHADER_MASK = Features::TASK_SHADER.bits | Features::MESH_SHADER.bits;
        /// Support sampler min/max reduction mode.
        const SAMPLER_REDUCTION = 0x0004 << 96;
    }
}

bitflags! {
    /// Features that the device doesn't support natively, but is able to emulate
    /// at some performance cost.
    #[derive(Default)]
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct PerformanceCaveats: u32 {
        /// Emulate indexed, instanced drawing with base vertex and instance.
        const BASE_VERTEX_INSTANCE_DRAWING = 0x0001;
    }
}

bitflags! {
    /// Dynamic pipeline states.
    #[derive(Default)]
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct DynamicStates: u32 {
        /// Supports `BakedStates::viewport == None`
        const VIEWPORT = 0x0001;
        /// Supports `BakedStates::scissor == None`
        const SCISSOR = 0x0002;
        /// Supports `Rasterizer::line_width == State::Dynamic(_)`
        const LINE_WIDTH = 0x0004;
        /// Supports `BakedStates::blend_constants == None`
        const BLEND_CONSTANTS = 0x0008;
        /// Supports `Rasterizer::depth_bias == Some(State::Dynamic(_))`
        const DEPTH_BIAS = 0x0010;
        /// Supports `BakedStates::depth_bounds == None`
        const DEPTH_BOUNDS = 0x0020;
        /// Supports `StencilTest::read_masks == State::Dynamic(_)`
        const STENCIL_READ_MASK = 0x0100;
        /// Supports `StencilTest::write_masks == State::Dynamic(_)`
        const STENCIL_WRITE_MASK = 0x0200;
        /// Supports `StencilTest::reference_values == State::Dynamic(_)`
        const STENCIL_REFERENCE = 0x0400;
    }
}

/// Properties of physical devices that are exposed but do not need to be explicitly opted into.
///
/// This contains things like resource limits, alignment requirements, and finer-grained feature
/// capabilities.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct PhysicalDeviceProperties {
    /// Core limits.
    pub limits: Limits,
    /// Descriptor Indexing properties.
    pub descriptor_indexing: DescriptorIndexingProperties,
    /// Mesh Shader properties.
    pub mesh_shader: MeshShaderProperties,
    /// Sampler reduction modes.
    pub sampler_reduction: SamplerReductionProperties,
    /// Downlevel properties.
    pub downlevel: DownlevelProperties,
    /// Performance caveats.
    pub performance_caveats: PerformanceCaveats,
    /// Dynamic pipeline states.
    pub dynamic_pipeline_states: DynamicStates,
}

///
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DescriptorLimits {
    ///
    pub max_per_stage_descriptor_samplers: u32,
    ///
    pub max_per_stage_descriptor_uniform_buffers: u32,
    ///
    pub max_per_stage_descriptor_storage_buffers: u32,
    ///
    pub max_per_stage_descriptor_sampled_images: u32,
    ///
    pub max_per_stage_descriptor_storage_images: u32,
    ///
    pub max_per_stage_descriptor_input_attachments: u32,
    ///
    pub max_per_stage_resources: u32,
    ///
    pub max_descriptor_set_samplers: u32,
    ///
    pub max_descriptor_set_uniform_buffers: u32,
    ///
    pub max_descriptor_set_uniform_buffers_dynamic: u32,
    ///
    pub max_descriptor_set_storage_buffers: u32,
    ///
    pub max_descriptor_set_storage_buffers_dynamic: u32,
    ///
    pub max_descriptor_set_sampled_images: u32,
    ///
    pub max_descriptor_set_storage_images: u32,
    ///
    pub max_descriptor_set_input_attachments: u32,
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
    pub descriptor_limits: DescriptorLimits,

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
    /// Timestamp queries are supported on all compute and graphics queues.
    pub timestamp_compute_and_graphics: bool,
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

/// Feature capabilities related to Descriptor Indexing.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DescriptorIndexingProperties {
    ///
    pub shader_uniform_buffer_array_non_uniform_indexing_native: bool,
    ///
    pub shader_sampled_image_array_non_uniform_indexing_native: bool,
    ///
    pub shader_storage_buffer_array_non_uniform_indexing_native: bool,
    ///
    pub shader_storage_image_array_non_uniform_indexing_native: bool,
    ///
    pub shader_input_attachment_array_non_uniform_indexing_native: bool,
    ///
    pub quad_divergent_implicit_lod: bool,
}

/// Resource limits related to the Mesh Shaders.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct MeshShaderProperties {
    /// The maximum number of local workgroups that can be launched by a single draw mesh tasks command
    pub max_draw_mesh_tasks_count: u32,
    /// The maximum total number of task shader invocations in a single local workgroup. The product of the X, Y, and
    /// Z sizes, as specified by the LocalSize execution mode in shader modules or by the object decorated by the
    /// WorkgroupSize decoration, must be less than or equal to this limit.
    pub max_task_work_group_invocations: u32,
    /// The maximum size of a local task workgroup. These three values represent the maximum local workgroup size in
    /// the X, Y, and Z dimensions, respectively. The x, y, and z sizes, as specified by the LocalSize execution mode
    /// or by the object decorated by the WorkgroupSize decoration in shader modules, must be less than or equal to
    /// the corresponding limit.
    pub max_task_work_group_size: [u32; 3],
    /// The maximum number of bytes that the task shader can use in total for shared and output memory combined.
    pub max_task_total_memory_size: u32,
    /// The maximum number of output tasks a single task shader workgroup can emit.
    pub max_task_output_count: u32,
    /// The maximum total number of mesh shader invocations in a single local workgroup. The product of the X, Y, and
    /// Z sizes, as specified by the LocalSize execution mode in shader modules or by the object decorated by the
    /// WorkgroupSize decoration, must be less than or equal to this limit.
    pub max_mesh_work_group_invocations: u32,
    /// The maximum size of a local mesh workgroup. These three values represent the maximum local workgroup size in
    /// the X, Y, and Z dimensions, respectively. The x, y, and z sizes, as specified by the LocalSize execution mode
    /// or by the object decorated by the WorkgroupSize decoration in shader modules, must be less than or equal to the
    /// corresponding limit.
    pub max_mesh_work_group_size: [u32; 3],
    /// The maximum number of bytes that the mesh shader can use in total for shared and output memory combined.
    pub max_mesh_total_memory_size: u32,
    /// The maximum number of vertices a mesh shader output can store.
    pub max_mesh_output_vertices: u32,
    /// The maximum number of primitives a mesh shader output can store.
    pub max_mesh_output_primitives: u32,
    /// The maximum number of multi-view views a mesh shader can use.
    pub max_mesh_multiview_view_count: u32,
    /// The granularity with which mesh vertex outputs are allocated. The value can be used to compute the memory size
    /// used by the mesh shader, which must be less than or equal to maxMeshTotalMemorySize.
    pub mesh_output_per_vertex_granularity: u32,
    /// The granularity with which mesh outputs qualified as per-primitive are allocated. The value can be used to
    /// compute the memory size used by the mesh shader, which must be less than or equal to
    pub mesh_output_per_primitive_granularity: u32,
}

/// Resource limits related to the reduction samplers.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct SamplerReductionProperties {
    /// Support for the minimum set of required formats support min/max filtering
    pub single_component_formats: bool,
    /// Support for the non-identity component mapping of the image when doing min/max filtering.
    pub image_component_mapping: bool,
}

/// Propterties to indicate when the backend does not support full vulkan compliance.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DownlevelProperties {
    /// Supports compute shaders.
    pub compute_shaders: bool,
    /// Which collections of features shaders support. Defined in terms of D3D's shader models.
    pub shader_model: DownlevelShaderModel,
    /// Supports creating storage images.
    pub storage_images: bool,
    /// Supports RODS
    pub read_only_depth_stencil: bool,
    /// Supports copies to/from device-local memory and device-local images.
    pub device_local_image_copies: bool,
    /// Supports textures with mipmaps which are non power of two.
    pub non_power_of_two_mipmapped_textures: bool,
}

impl DownlevelProperties {
    /// Enables all properties for a vulkan-complient backend.
    pub fn all_enabled() -> Self {
        Self {
            compute_shaders: true,
            shader_model: DownlevelShaderModel::ShaderModel5,
            storage_images: true,
            read_only_depth_stencil: true,
            device_local_image_copies: true,
            non_power_of_two_mipmapped_textures: true,
        }
    }
}

/// Collections of shader features shaders support if they support less than vulkan does.
#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum DownlevelShaderModel {
    /// Extremely limited shaders, including a total instruction limit.
    ShaderModel2,
    /// Missing minor features and storage images.
    ShaderModel4,
    /// Vulkan shaders are SM5
    ShaderModel5,
}

impl Default for DownlevelShaderModel {
    fn default() -> Self {
        Self::ShaderModel2
    }
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
#[derive(Copy, Clone, Debug, Eq, PartialEq, thiserror::Error)]
#[error("Backend is not supported on this platform")]
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
/// use gfx_hal::Instance;
///
/// // Create a concrete instance of our backend (this is backend-dependent and may be more
/// // complicated for some backends).
/// let instance = backend::Instance::create("My App", 1).unwrap();
/// // We can get a list of the available adapters, which are either physical graphics
/// // devices, or virtual adapters. Because we are using the dummy `empty` backend,
/// // there will be nothing in this list.
/// for (idx, adapter) in instance.enumerate_adapters().iter().enumerate() {
///     println!("Adapter {}: {:?}", idx, adapter.info);
/// }
/// ```
pub trait Instance<B: Backend>: Any + Send + Sync + Sized {
    /// Create a new instance.
    ///
    /// # Arguments
    ///
    /// * `name` - name of the application using the API.
    /// * `version` - free form representation of the application's version.
    ///
    /// This metadata is passed further down the graphics stack.
    ///
    /// # Errors
    ///
    /// Returns an `Err` variant if the requested backend [is not supported
    /// on the current platform][UnsupportedBackend].
    fn create(name: &str, version: u32) -> Result<Self, UnsupportedBackend>;

    /// Return all available [graphics adapters][adapter::Adapter].
    fn enumerate_adapters(&self) -> Vec<adapter::Adapter<B>>;

    /// Create a new [surface][window::Surface].
    ///
    /// Surfaces can be used to render to windows.
    ///
    /// # Safety
    ///
    /// This method can cause undefined behavior if `raw_window_handle` isn't
    /// a handle to a valid window for the current platform.
    unsafe fn create_surface(
        &self,
        raw_window_handle: &impl raw_window_handle::HasRawWindowHandle,
    ) -> Result<B::Surface, window::InitError>;

    /// Destroy a surface, freeing the resources associated with it and
    /// releasing it from this graphics API.
    ///
    /// # Safety
    ///
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

impl<T> Extend<T> for PseudoVec<T> {
    fn extend<I: IntoIterator<Item = T>>(&mut self, into_iter: I) {
        let mut iter = into_iter.into_iter();
        self.0 = iter.next();
        assert!(iter.next().is_none());
    }
}

/// Wraps together all the types needed for a graphics backend.
///
/// Each backend module, such as OpenGL or Metal, will implement this trait
/// with its own concrete types.
pub trait Backend: 'static + Sized + Eq + Clone + Hash + fmt::Debug + Any + Send + Sync {
    /// The corresponding [instance][Instance] type for this backend.
    type Instance: Instance<Self>;
    /// The corresponding [physical device][adapter::PhysicalDevice] type for this backend.
    type PhysicalDevice: adapter::PhysicalDevice<Self>;
    /// The corresponding [logical device][device::Device] type for this backend.
    type Device: device::Device<Self>;
    /// The corresponding [surface][window::PresentationSurface] type for this backend.
    type Surface: window::PresentationSurface<Self>;

    /// The corresponding [queue family][queue::QueueFamily] type for this backend.
    type QueueFamily: queue::QueueFamily;
    /// The corresponding [command queue][queue::Queue] type for this backend.
    type Queue: queue::Queue<Self>;
    /// The corresponding [command buffer][command::CommandBuffer] type for this backend.
    type CommandBuffer: command::CommandBuffer<Self>;

    /// The corresponding shader module type for this backend.
    type ShaderModule: fmt::Debug + Any + Send + Sync;
    /// The corresponding render pass type for this backend.
    type RenderPass: fmt::Debug + Any + Send + Sync;
    /// The corresponding framebuffer type for this backend.
    type Framebuffer: fmt::Debug + Any + Send + Sync;

    /// The corresponding memory type for this backend.
    type Memory: fmt::Debug + Any + Send + Sync;
    /// The corresponding [command pool][pool::CommandPool] type for this backend.
    type CommandPool: pool::CommandPool<Self>;

    /// The corresponding buffer type for this backend.
    type Buffer: fmt::Debug + Any + Send + Sync;
    /// The corresponding buffer view type for this backend.
    type BufferView: fmt::Debug + Any + Send + Sync;
    /// The corresponding image type for this backend.
    type Image: fmt::Debug + Any + Send + Sync;
    /// The corresponding image view type for this backend.
    type ImageView: fmt::Debug + Any + Send + Sync;
    /// The corresponding sampler type for this backend.
    type Sampler: fmt::Debug + Any + Send + Sync;

    /// The corresponding compute pipeline type for this backend.
    type ComputePipeline: fmt::Debug + Any + Send + Sync;
    /// The corresponding graphics pipeline type for this backend.
    type GraphicsPipeline: fmt::Debug + Any + Send + Sync;
    /// The corresponding pipeline cache type for this backend.
    type PipelineCache: fmt::Debug + Any + Send + Sync;
    /// The corresponding pipeline layout type for this backend.
    type PipelineLayout: fmt::Debug + Any + Send + Sync;
    /// The corresponding [descriptor pool][pso::DescriptorPool] type for this backend.
    type DescriptorPool: pso::DescriptorPool<Self>;
    /// The corresponding descriptor set type for this backend.
    type DescriptorSet: fmt::Debug + Any + Send + Sync;
    /// The corresponding descriptor set layout type for this backend.
    type DescriptorSetLayout: fmt::Debug + Any + Send + Sync;

    /// The corresponding fence type for this backend.
    type Fence: fmt::Debug + Any + Send + Sync;
    /// The corresponding semaphore type for this backend.
    type Semaphore: fmt::Debug + Any + Send + Sync;
    /// The corresponding event type for this backend.
    type Event: fmt::Debug + Any + Send + Sync;
    /// The corresponding query pool type for this backend.
    type QueryPool: fmt::Debug + Any + Send + Sync;
}
