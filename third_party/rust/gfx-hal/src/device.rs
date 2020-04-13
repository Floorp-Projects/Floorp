//! Logical device
//!
//! # Device
//!
//! This module exposes the `Device` trait, which provides methods for creating
//! and managing graphics resources such as buffers, images and memory.
//!
//! The `Adapter` and `Device` types are very similar to the Vulkan concept of
//! "physical devices" vs. "logical devices"; an `Adapter` is single GPU
//! (or CPU) that implements a backend, a `Device` is a
//! handle to that physical device that has the requested capabilities
//! and is used to actually do things.

use std::any::Any;
use std::borrow::Borrow;
use std::ops::Range;
use std::{fmt, iter};

use crate::{
    buffer,
    format,
    image,
    memory::{Requirements, Segment},
    pass,
    pool::CommandPoolCreateFlags,
    pso,
    pso::DescriptorPoolCreateFlags,
    query,
    queue::QueueFamilyId,
    window::{self, SwapchainConfig},
    Backend,
    MemoryTypeId,
};

/// Error occurred caused device to be lost.
#[derive(Clone, Debug, PartialEq)]
pub struct DeviceLost;

impl std::fmt::Display for DeviceLost {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        fmt.write_str("Device lost")
    }
}

impl std::error::Error for DeviceLost {}

/// Error occurred caused surface to be lost.
#[derive(Clone, Debug, PartialEq)]
pub struct SurfaceLost;

impl std::fmt::Display for SurfaceLost {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        fmt.write_str("Surface lost")
    }
}

impl std::error::Error for SurfaceLost {}

/// Native window is already in use by graphics API.
#[derive(Clone, Debug, PartialEq)]
pub struct WindowInUse;

impl std::fmt::Display for WindowInUse {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        fmt.write_str("Window is in use")
    }
}

impl std::error::Error for WindowInUse {}

/// Error allocating memory.
#[derive(Clone, Debug, PartialEq)]
pub enum OutOfMemory {
    /// Host memory exhausted.
    Host,
    /// Device memory exhausted.
    Device,
}

impl std::fmt::Display for OutOfMemory {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            OutOfMemory::Host => write!(fmt, "Out of host memory"),
            OutOfMemory::Device => write!(fmt, "Out of device memory"),
        }
    }
}

impl std::error::Error for OutOfMemory {}

/// Error occurred caused device to be lost
/// or out of memory error.
#[derive(Clone, Debug, PartialEq)]
pub enum OomOrDeviceLost {
    /// Out of either host or device memory.
    OutOfMemory(OutOfMemory),
    /// Device is lost
    DeviceLost(DeviceLost),
}

impl From<OutOfMemory> for OomOrDeviceLost {
    fn from(error: OutOfMemory) -> Self {
        OomOrDeviceLost::OutOfMemory(error)
    }
}

impl From<DeviceLost> for OomOrDeviceLost {
    fn from(error: DeviceLost) -> Self {
        OomOrDeviceLost::DeviceLost(error)
    }
}

impl std::fmt::Display for OomOrDeviceLost {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            OomOrDeviceLost::DeviceLost(err) => write!(fmt, "Failed querying device: {}", err),
            OomOrDeviceLost::OutOfMemory(err) => write!(fmt, "Failed querying device: {}", err),
        }
    }
}

impl std::error::Error for OomOrDeviceLost {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            OomOrDeviceLost::DeviceLost(err) => Some(err),
            OomOrDeviceLost::OutOfMemory(err) => Some(err),
        }
    }
}

/// Possible cause of allocation failure.
#[derive(Clone, Debug, PartialEq)]
pub enum AllocationError {
    /// Out of either host or device memory.
    OutOfMemory(OutOfMemory),

    /// Cannot create any more objects.
    TooManyObjects,
}

impl From<OutOfMemory> for AllocationError {
    fn from(error: OutOfMemory) -> Self {
        AllocationError::OutOfMemory(error)
    }
}

impl std::fmt::Display for AllocationError {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AllocationError::OutOfMemory(err) => write!(fmt, "Failed to allocate object: {}", err),
            AllocationError::TooManyObjects => {
                write!(fmt, "Failed to allocate object: Too many objects")
            }
        }
    }
}

impl std::error::Error for AllocationError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            AllocationError::OutOfMemory(err) => Some(err),
            _ => None,
        }
    }
}

/// Device creation errors during `open`.
#[derive(Clone, Debug, PartialEq)]
pub enum CreationError {
    /// Out of either host or device memory.
    OutOfMemory(OutOfMemory),
    /// Device initialization failed due to implementation specific errors.
    InitializationFailed,
    /// At least one of the user requested extensions if not supported by the
    /// physical device.
    MissingExtension,
    /// At least one of the user requested features if not supported by the
    /// physical device.
    ///
    /// Use [`features`](trait.PhysicalDevice.html#tymethod.features)
    /// for checking the supported features.
    MissingFeature,
    /// Too many logical devices have been created from this physical device.
    ///
    /// The implementation may only support one logical device for each physical
    /// device or lacks resources to allocate a new device.
    TooManyObjects,
    /// The logical or physical device are lost during the device creation
    /// process.
    ///
    /// This may be caused by hardware failure, physical device removal,
    /// power outage, etc.
    DeviceLost,
}

impl std::fmt::Display for CreationError {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CreationError::OutOfMemory(err) => write!(fmt, "Failed to create device: {}", err),
            CreationError::InitializationFailed => write!(
                fmt,
                "Failed to create device: Implementation specific error occurred"
            ),
            CreationError::MissingExtension => write!(
                fmt,
                "Failed to create device: Requested extension is missing"
            ),
            CreationError::MissingFeature => {
                write!(fmt, "Failed to create device: Requested feature is missing")
            }
            CreationError::TooManyObjects => {
                write!(fmt, "Failed to create device: Too many objects")
            }
            CreationError::DeviceLost => write!(
                fmt,
                "Failed to create device: Logical or Physical device was lost during creation"
            ),
        }
    }
}

impl std::error::Error for CreationError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            CreationError::OutOfMemory(err) => Some(err),
            _ => None,
        }
    }
}

/// Error accessing a mapping.
#[derive(Clone, Debug, PartialEq)]
pub enum MapError {
    /// Out of either host or device memory.
    OutOfMemory(OutOfMemory),
    /// The requested mapping range is outside of the resource.
    OutOfBounds,
    /// Failed to allocate an appropriately sized contiguous virtual address range
    MappingFailed,
}

impl From<OutOfMemory> for MapError {
    fn from(error: OutOfMemory) -> Self {
        MapError::OutOfMemory(error)
    }
}

impl std::fmt::Display for MapError {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MapError::OutOfMemory(err) => write!(fmt, "Failed to map memory: {}", err),
            MapError::OutOfBounds => write!(fmt, "Failed to map memory: Requested range is outside the resource"),
            MapError::MappingFailed => write!(fmt, "Failed to map memory: Unable to allocate an appropriately sized contiguous virtual address range"),
        }
    }
}

impl std::error::Error for MapError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            MapError::OutOfMemory(err) => Some(err),
            _ => None,
        }
    }
}

/// Error binding a resource to memory allocation.
#[derive(Clone, Debug, PartialEq)]
pub enum BindError {
    /// Out of either host or device memory.
    OutOfMemory(OutOfMemory),
    /// Requested binding to memory that doesn't support the required operations.
    WrongMemory,
    /// Requested binding to an invalid memory.
    OutOfBounds,
}

impl From<OutOfMemory> for BindError {
    fn from(error: OutOfMemory) -> Self {
        BindError::OutOfMemory(error)
    }
}

impl std::fmt::Display for BindError {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            BindError::OutOfMemory(err) => {
                write!(fmt, "Failed to bind object to memory range: {}", err)
            }
            BindError::OutOfBounds => write!(
                fmt,
                "Failed to bind object to memory range: Requested range is outside the resource"
            ),
            BindError::WrongMemory => {
                write!(fmt, "Failed to bind object to memory range: Wrong memory")
            }
        }
    }
}

impl std::error::Error for BindError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            BindError::OutOfMemory(err) => Some(err),
            _ => None,
        }
    }
}

/// Specifies the waiting targets.
#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum WaitFor {
    /// Wait for any target.
    Any,
    /// Wait for all targets at once.
    All,
}

/// An error from creating a shader module.
#[derive(Clone, Debug, PartialEq)]
pub enum ShaderError {
    /// The shader failed to compile.
    CompilationFailed(String),
    /// The shader is missing an entry point.
    MissingEntryPoint(String),
    /// The shader has a mismatch of interface (e.g missing push constants).
    InterfaceMismatch(String),
    /// The shader stage is not supported.
    UnsupportedStage(pso::Stage),
    /// Out of either host or device memory.
    OutOfMemory(OutOfMemory),
}

impl From<OutOfMemory> for ShaderError {
    fn from(error: OutOfMemory) -> Self {
        ShaderError::OutOfMemory(error)
    }
}

impl std::fmt::Display for ShaderError {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ShaderError::OutOfMemory(err) => write!(fmt, "Shader error: {}", err),
            ShaderError::CompilationFailed(string) => {
                write!(fmt, "Shader error: Compilation failed: {}", string)
            }
            ShaderError::MissingEntryPoint(string) => {
                write!(fmt, "Shader error: Missing entry point: {}", string)
            }
            ShaderError::InterfaceMismatch(string) => {
                write!(fmt, "Shader error: Interface mismatch: {}", string)
            }
            ShaderError::UnsupportedStage(stage) => {
                write!(fmt, "Shader error: Unsupported stage: {:?}", stage)
            }
        }
    }
}

impl std::error::Error for ShaderError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            ShaderError::OutOfMemory(err) => Some(err),
            _ => None,
        }
    }
}

/// # Overview
///
/// A `Device` is responsible for creating and managing resources for the physical device
/// it was created from.
///
/// ## Resource Construction and Handling
///
/// This device structure can then be used to create and manage different resources, like buffers,
/// shader programs and textures. See the individual methods for more information.
///
/// ## Mutability
///
/// All the methods get `&self`. Any internal mutability of the `Device` is hidden from the user.
///
/// ## Synchronization
///
/// `Device` should be usable concurrently from multiple threads. The `Send` and `Sync` bounds
/// are not enforced at the HAL level due to OpenGL constraint (to be revised). Users can still
/// benefit from the backends that support synchronization of the `Device`.
///
pub trait Device<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Allocates a memory segment of a specified type.
    ///
    /// There is only a limited amount of allocations allowed depending on the implementation!
    ///
    /// # Arguments
    ///
    /// * `memory_type` - Index of the memory type in the memory properties of the associated physical device.
    /// * `size` - Size of the allocation.
    unsafe fn allocate_memory(
        &self,
        memory_type: MemoryTypeId,
        size: u64,
    ) -> Result<B::Memory, AllocationError>;

    /// Free device memory
    unsafe fn free_memory(&self, memory: B::Memory);

    /// Create a new command pool for a given queue family.
    ///
    /// *Note*: the family has to be associated by one as the `Gpu::queue_groups`.
    unsafe fn create_command_pool(
        &self,
        family: QueueFamilyId,
        create_flags: CommandPoolCreateFlags,
    ) -> Result<B::CommandPool, OutOfMemory>;

    /// Destroy a command pool.
    unsafe fn destroy_command_pool(&self, pool: B::CommandPool);

    /// Create a render pass with the given attachments and subpasses.
    ///
    /// A *render pass* represents a collection of attachments, subpasses, and dependencies between
    /// the subpasses, and describes how the attachments are used over the course of the subpasses.
    /// The use of a render pass in a command buffer is a *render pass* instance.
    unsafe fn create_render_pass<'a, IA, IS, ID>(
        &self,
        attachments: IA,
        subpasses: IS,
        dependencies: ID,
    ) -> Result<B::RenderPass, OutOfMemory>
    where
        IA: IntoIterator,
        IA::Item: Borrow<pass::Attachment>,
        IS: IntoIterator,
        IS::Item: Borrow<pass::SubpassDesc<'a>>,
        ID: IntoIterator,
        ID::Item: Borrow<pass::SubpassDependency>;

    /// Destroy a `RenderPass`.
    unsafe fn destroy_render_pass(&self, rp: B::RenderPass);

    /// Create a new pipeline layout object.
    ///
    /// # Arguments
    ///
    /// * `set_layouts` - Descriptor set layouts
    /// * `push_constants` - Ranges of push constants. A shader stage may only contain one push
    ///     constant block. The range is defined in units of bytes.
    ///
    /// # PipelineLayout
    ///
    /// Access to descriptor sets from a pipeline is accomplished through a *pipeline layout*.
    /// Zero or more descriptor set layouts and zero or more push constant ranges are combined to
    /// form a pipeline layout object which describes the complete set of resources that **can** be
    /// accessed by a pipeline. The pipeline layout represents a sequence of descriptor sets with
    /// each having a specific layout. This sequence of layouts is used to determine the interface
    /// between shader stages and shader resources. Each pipeline is created using a pipeline layout.
    unsafe fn create_pipeline_layout<IS, IR>(
        &self,
        set_layouts: IS,
        push_constant: IR,
    ) -> Result<B::PipelineLayout, OutOfMemory>
    where
        IS: IntoIterator,
        IS::Item: Borrow<B::DescriptorSetLayout>,
        IR: IntoIterator,
        IR::Item: Borrow<(pso::ShaderStageFlags, Range<u32>)>;

    /// Destroy a pipeline layout object
    unsafe fn destroy_pipeline_layout(&self, layout: B::PipelineLayout);

    /// Create a pipeline cache object.
    unsafe fn create_pipeline_cache(
        &self,
        data: Option<&[u8]>,
    ) -> Result<B::PipelineCache, OutOfMemory>;

    /// Retrieve data from pipeline cache object.
    unsafe fn get_pipeline_cache_data(
        &self,
        cache: &B::PipelineCache,
    ) -> Result<Vec<u8>, OutOfMemory>;

    /// Merge a number of source pipeline caches into the target one.
    unsafe fn merge_pipeline_caches<I>(
        &self,
        target: &B::PipelineCache,
        sources: I,
    ) -> Result<(), OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<B::PipelineCache>;

    /// Destroy a pipeline cache object.
    unsafe fn destroy_pipeline_cache(&self, cache: B::PipelineCache);

    /// Create a graphics pipeline.
    unsafe fn create_graphics_pipeline<'a>(
        &self,
        desc: &pso::GraphicsPipelineDesc<'a, B>,
        cache: Option<&B::PipelineCache>,
    ) -> Result<B::GraphicsPipeline, pso::CreationError>;

    /// Create graphics pipelines.
    unsafe fn create_graphics_pipelines<'a, I>(
        &self,
        descs: I,
        cache: Option<&B::PipelineCache>,
    ) -> Vec<Result<B::GraphicsPipeline, pso::CreationError>>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::GraphicsPipelineDesc<'a, B>>,
    {
        descs
            .into_iter()
            .map(|desc| self.create_graphics_pipeline(desc.borrow(), cache))
            .collect()
    }

    /// Destroy a graphics pipeline.
    ///
    /// The graphics pipeline shouldn't be destroyed before any submitted command buffer,
    /// which references the graphics pipeline, has finished execution.
    unsafe fn destroy_graphics_pipeline(&self, pipeline: B::GraphicsPipeline);

    /// Create a compute pipeline.
    unsafe fn create_compute_pipeline<'a>(
        &self,
        desc: &pso::ComputePipelineDesc<'a, B>,
        cache: Option<&B::PipelineCache>,
    ) -> Result<B::ComputePipeline, pso::CreationError>;

    /// Create compute pipelines.
    unsafe fn create_compute_pipelines<'a, I>(
        &self,
        descs: I,
        cache: Option<&B::PipelineCache>,
    ) -> Vec<Result<B::ComputePipeline, pso::CreationError>>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::ComputePipelineDesc<'a, B>>,
    {
        descs
            .into_iter()
            .map(|desc| self.create_compute_pipeline(desc.borrow(), cache))
            .collect()
    }

    /// Destroy a compute pipeline.
    ///
    /// The compute pipeline shouldn't be destroyed before any submitted command buffer,
    /// which references the compute pipeline, has finished execution.
    unsafe fn destroy_compute_pipeline(&self, pipeline: B::ComputePipeline);

    /// Create a new framebuffer object.
    ///
    /// # Safety
    /// - `extent.width`, `extent.height` and `extent.depth` **must** be greater than `0`.
    unsafe fn create_framebuffer<I>(
        &self,
        pass: &B::RenderPass,
        attachments: I,
        extent: image::Extent,
    ) -> Result<B::Framebuffer, OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<B::ImageView>;

    /// Destroy a framebuffer.
    ///
    /// The framebuffer shouldn't be destroy before any submitted command buffer,
    /// which references the framebuffer, has finished execution.
    unsafe fn destroy_framebuffer(&self, buf: B::Framebuffer);

    /// Create a new shader module object through the SPIR-V binary data.
    ///
    /// Once a shader module has been created, any entry points it contains can be used in pipeline
    /// shader stages as described in *Compute Pipelines* and *Graphics Pipelines*.
    unsafe fn create_shader_module(
        &self,
        spirv_data: &[u32],
    ) -> Result<B::ShaderModule, ShaderError>;

    /// Destroy a shader module module
    ///
    /// A shader module can be destroyed while pipelines created using its shaders are still in use.
    unsafe fn destroy_shader_module(&self, shader: B::ShaderModule);

    /// Create a new buffer (unbound).
    ///
    /// The created buffer won't have associated memory until `bind_buffer_memory` is called.
    unsafe fn create_buffer(
        &self,
        size: u64,
        usage: buffer::Usage,
    ) -> Result<B::Buffer, buffer::CreationError>;

    /// Get memory requirements for the buffer
    unsafe fn get_buffer_requirements(&self, buf: &B::Buffer) -> Requirements;

    /// Bind memory to a buffer.
    ///
    /// Be sure to check that there is enough memory available for the buffer.
    /// Use `get_buffer_requirements` to acquire the memory requirements.
    unsafe fn bind_buffer_memory(
        &self,
        memory: &B::Memory,
        offset: u64,
        buf: &mut B::Buffer,
    ) -> Result<(), BindError>;

    /// Destroy a buffer.
    ///
    /// The buffer shouldn't be destroyed before any submitted command buffer,
    /// which references the images, has finished execution.
    unsafe fn destroy_buffer(&self, buffer: B::Buffer);

    /// Create a new buffer view object
    unsafe fn create_buffer_view(
        &self,
        buf: &B::Buffer,
        fmt: Option<format::Format>,
        range: buffer::SubRange,
    ) -> Result<B::BufferView, buffer::ViewCreationError>;

    /// Destroy a buffer view object
    unsafe fn destroy_buffer_view(&self, view: B::BufferView);

    /// Create a new image object
    unsafe fn create_image(
        &self,
        kind: image::Kind,
        mip_levels: image::Level,
        format: format::Format,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Result<B::Image, image::CreationError>;

    /// Get memory requirements for the Image
    unsafe fn get_image_requirements(&self, image: &B::Image) -> Requirements;

    ///
    unsafe fn get_image_subresource_footprint(
        &self,
        image: &B::Image,
        subresource: image::Subresource,
    ) -> image::SubresourceFootprint;

    /// Bind device memory to an image object
    unsafe fn bind_image_memory(
        &self,
        memory: &B::Memory,
        offset: u64,
        image: &mut B::Image,
    ) -> Result<(), BindError>;

    /// Destroy an image.
    ///
    /// The image shouldn't be destroyed before any submitted command buffer,
    /// which references the images, has finished execution.
    unsafe fn destroy_image(&self, image: B::Image);

    /// Create an image view from an existing image
    unsafe fn create_image_view(
        &self,
        image: &B::Image,
        view_kind: image::ViewKind,
        format: format::Format,
        swizzle: format::Swizzle,
        range: image::SubresourceRange,
    ) -> Result<B::ImageView, image::ViewCreationError>;

    /// Destroy an image view object
    unsafe fn destroy_image_view(&self, view: B::ImageView);

    /// Create a new sampler object
    unsafe fn create_sampler(
        &self,
        desc: &image::SamplerDesc,
    ) -> Result<B::Sampler, AllocationError>;

    /// Destroy a sampler object
    unsafe fn destroy_sampler(&self, sampler: B::Sampler);

    /// Create a descriptor pool.
    ///
    /// Descriptor pools allow allocation of descriptor sets.
    /// The pool can't be modified directly, only through updating descriptor sets.
    unsafe fn create_descriptor_pool<I>(
        &self,
        max_sets: usize,
        descriptor_ranges: I,
        flags: DescriptorPoolCreateFlags,
    ) -> Result<B::DescriptorPool, OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorRangeDesc>;

    /// Destroy a descriptor pool object
    ///
    /// When a pool is destroyed, all descriptor sets allocated from the pool are implicitly freed
    /// and become invalid. Descriptor sets allocated from a given pool do not need to be freed
    /// before destroying that descriptor pool.
    unsafe fn destroy_descriptor_pool(&self, pool: B::DescriptorPool);

    /// Create a descriptor set layout.
    ///
    /// A descriptor set layout object is defined by an array of zero or more descriptor bindings.
    /// Each individual descriptor binding is specified by a descriptor type, a count (array size)
    /// of the number of descriptors in the binding, a set of shader stages that **can** access the
    /// binding, and (if using immutable samplers) an array of sampler descriptors.
    unsafe fn create_descriptor_set_layout<I, J>(
        &self,
        bindings: I,
        immutable_samplers: J,
    ) -> Result<B::DescriptorSetLayout, OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetLayoutBinding>,
        J: IntoIterator,
        J::Item: Borrow<B::Sampler>;

    /// Destroy a descriptor set layout object
    unsafe fn destroy_descriptor_set_layout(&self, layout: B::DescriptorSetLayout);

    /// Specifying the parameters of a descriptor set write operation
    unsafe fn write_descriptor_sets<'a, I, J>(&self, write_iter: I)
    where
        I: IntoIterator<Item = pso::DescriptorSetWrite<'a, B, J>>,
        J: IntoIterator,
        J::Item: Borrow<pso::Descriptor<'a, B>>;

    /// Structure specifying a copy descriptor set operation
    unsafe fn copy_descriptor_sets<'a, I>(&self, copy_iter: I)
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetCopy<'a, B>>;

    /// Map a memory object into application address space
    ///
    /// Call `map_memory()` to retrieve a host virtual address pointer to a region of a mappable memory object
    unsafe fn map_memory(&self, memory: &B::Memory, segment: Segment) -> Result<*mut u8, MapError>;

    /// Flush mapped memory ranges
    unsafe fn flush_mapped_memory_ranges<'a, I>(&self, ranges: I) -> Result<(), OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a B::Memory, Segment)>;

    /// Invalidate ranges of non-coherent memory from the host caches
    unsafe fn invalidate_mapped_memory_ranges<'a, I>(&self, ranges: I) -> Result<(), OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a B::Memory, Segment)>;

    /// Unmap a memory object once host access to it is no longer needed by the application
    unsafe fn unmap_memory(&self, memory: &B::Memory);

    /// Create a new semaphore object
    fn create_semaphore(&self) -> Result<B::Semaphore, OutOfMemory>;

    /// Destroy a semaphore object
    unsafe fn destroy_semaphore(&self, semaphore: B::Semaphore);

    /// Create a new fence object
    ///
    /// Fences are a synchronization primitive that **can** be used to insert a dependency from
    /// a queue to the host. Fences have two states - signaled and unsignaled. A fence **can** be
    /// signaled as part of the execution of a *queue submission* command. Fences **can** be unsignaled
    /// on the host with *reset_fences*. Fences **can** be waited on by the host with the
    /// *wait_for_fences* command, and the current state **can** be queried with *get_fence_status*.
    fn create_fence(&self, signaled: bool) -> Result<B::Fence, OutOfMemory>;

    ///
    unsafe fn reset_fence(&self, fence: &B::Fence) -> Result<(), OutOfMemory> {
        self.reset_fences(iter::once(fence))
    }

    ///
    unsafe fn reset_fences<I>(&self, fences: I) -> Result<(), OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<B::Fence>,
    {
        for fence in fences {
            self.reset_fence(fence.borrow())?;
        }
        Ok(())
    }

    /// Blocks until the given fence is signaled.
    /// Returns true if the fence was signaled before the timeout.
    unsafe fn wait_for_fence(
        &self,
        fence: &B::Fence,
        timeout_ns: u64,
    ) -> Result<bool, OomOrDeviceLost> {
        self.wait_for_fences(iter::once(fence), WaitFor::All, timeout_ns)
    }

    /// Blocks until all or one of the given fences are signaled.
    /// Returns true if fences were signaled before the timeout.
    unsafe fn wait_for_fences<I>(
        &self,
        fences: I,
        wait: WaitFor,
        timeout_ns: u64,
    ) -> Result<bool, OomOrDeviceLost>
    where
        I: IntoIterator,
        I::Item: Borrow<B::Fence>,
    {
        use std::{thread, time};
        fn to_ns(duration: time::Duration) -> u64 {
            duration.as_secs() * 1_000_000_000 + duration.subsec_nanos() as u64
        }

        let start = time::Instant::now();
        match wait {
            WaitFor::All => {
                for fence in fences {
                    if !self.wait_for_fence(fence.borrow(), 0)? {
                        let elapsed_ns = to_ns(start.elapsed());
                        if elapsed_ns > timeout_ns {
                            return Ok(false);
                        }
                        if !self.wait_for_fence(fence.borrow(), timeout_ns - elapsed_ns)? {
                            return Ok(false);
                        }
                    }
                }
                Ok(true)
            }
            WaitFor::Any => {
                let fences: Vec<_> = fences.into_iter().collect();
                loop {
                    for fence in &fences {
                        if self.wait_for_fence(fence.borrow(), 0)? {
                            return Ok(true);
                        }
                    }
                    if to_ns(start.elapsed()) >= timeout_ns {
                        return Ok(false);
                    }
                    thread::sleep(time::Duration::from_millis(1));
                }
            }
        }
    }

    /// true for signaled, false for not ready
    unsafe fn get_fence_status(&self, fence: &B::Fence) -> Result<bool, DeviceLost>;

    /// Destroy a fence object
    unsafe fn destroy_fence(&self, fence: B::Fence);

    /// Create an event object.
    fn create_event(&self) -> Result<B::Event, OutOfMemory>;

    /// Destroy an event object.
    unsafe fn destroy_event(&self, event: B::Event);

    /// Query the status of an event.
    ///
    /// Returns `true` if the event is set, or `false` if it is reset.
    unsafe fn get_event_status(&self, event: &B::Event) -> Result<bool, OomOrDeviceLost>;

    /// Sets an event.
    unsafe fn set_event(&self, event: &B::Event) -> Result<(), OutOfMemory>;

    /// Resets an event.
    unsafe fn reset_event(&self, event: &B::Event) -> Result<(), OutOfMemory>;

    /// Create a new query pool object
    ///
    /// Queries are managed using query pool objects. Each query pool is a collection of a specific
    /// number of queries of a particular type.
    unsafe fn create_query_pool(
        &self,
        ty: query::Type,
        count: query::Id,
    ) -> Result<B::QueryPool, query::CreationError>;

    /// Destroy a query pool object
    unsafe fn destroy_query_pool(&self, pool: B::QueryPool);

    /// Get query pool results into the specified CPU memory.
    /// Returns `Ok(false)` if the results are not ready yet and neither of `WAIT` or `PARTIAL` flags are set.
    unsafe fn get_query_pool_results(
        &self,
        pool: &B::QueryPool,
        queries: Range<query::Id>,
        data: &mut [u8],
        stride: buffer::Offset,
        flags: query::ResultFlags,
    ) -> Result<bool, OomOrDeviceLost>;

    /// Create a new swapchain from a surface and a queue family, optionally providing the old
    /// swapchain to aid in resource reuse and rendering continuity.
    ///
    /// *Note*: The number of exposed images in the back buffer might differ
    /// from number of internally used buffers.
    ///
    /// # Safety
    ///
    /// The queue family _must_ support surface presentation.
    /// This can be checked by calling [`supports_queue_family`](trait.Surface.html#tymethod.supports_queue_family)
    /// on this surface.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # extern crate gfx_backend_empty as empty;
    /// # extern crate gfx_hal;
    /// # fn main() {
    /// use gfx_hal::{prelude::*, format::Format, window::SwapchainConfig};
    ///
    /// # let mut surface: empty::Surface = return;
    /// # let device: empty::Device = return;
    /// # unsafe {
    /// let swapchain_config = SwapchainConfig::new(100, 100, Format::Rgba8Srgb, 2);
    /// device.create_swapchain(&mut surface, swapchain_config, None);
    /// # }}
    /// ```
    unsafe fn create_swapchain(
        &self,
        surface: &mut B::Surface,
        config: SwapchainConfig,
        old_swapchain: Option<B::Swapchain>,
    ) -> Result<(B::Swapchain, Vec<B::Image>), window::CreationError>;

    ///
    unsafe fn destroy_swapchain(&self, swapchain: B::Swapchain);

    /// Wait for all queues associated with this device to idle.
    ///
    /// Host access to all queues needs to be **externally** sycnhronized!
    fn wait_idle(&self) -> Result<(), OutOfMemory>;

    /// Associate a name with an image, for easier debugging in external tools or with validation
    /// layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_image_name(&self, image: &mut B::Image, name: &str);
    /// Associate a name with a buffer, for easier debugging in external tools or with validation
    /// layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_buffer_name(&self, buffer: &mut B::Buffer, name: &str);
    /// Associate a name with a command buffer, for easier debugging in external tools or with
    /// validation layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_command_buffer_name(&self, command_buffer: &mut B::CommandBuffer, name: &str);
    /// Associate a name with a semaphore, for easier debugging in external tools or with validation
    /// layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_semaphore_name(&self, semaphore: &mut B::Semaphore, name: &str);
    /// Associate a name with a fence, for easier debugging in external tools or with validation
    /// layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_fence_name(&self, fence: &mut B::Fence, name: &str);
    /// Associate a name with a framebuffer, for easier debugging in external tools or with
    /// validation layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_framebuffer_name(&self, framebuffer: &mut B::Framebuffer, name: &str);
    /// Associate a name with a render pass, for easier debugging in external tools or with
    /// validation layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_render_pass_name(&self, render_pass: &mut B::RenderPass, name: &str);
    /// Associate a name with a descriptor set, for easier debugging in external tools or with
    /// validation layers that can print a friendly name when referring to objects in error messages
    unsafe fn set_descriptor_set_name(&self, descriptor_set: &mut B::DescriptorSet, name: &str);
    /// Associate a name with a descriptor set layout, for easier debugging in external tools or
    /// with validation layers that can print a friendly name when referring to objects in error
    /// messages
    unsafe fn set_descriptor_set_layout_name(
        &self,
        descriptor_set_layout: &mut B::DescriptorSetLayout,
        name: &str,
    );
}
