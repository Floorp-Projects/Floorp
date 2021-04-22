use crate::{
    internal::Channel, Backend, BufferPtr, FastHashMap, ResourceIndex, SamplerPtr, TexturePtr,
    MAX_COLOR_ATTACHMENTS,
};

use hal::{
    buffer,
    format::FormatDesc,
    image,
    memory::Segment,
    pass::{Attachment, AttachmentId},
    pso, MemoryTypeId,
};
use range_alloc::RangeAllocator;

use arrayvec::ArrayVec;
use metal;
use parking_lot::RwLock;

use std::{
    fmt,
    ops::Range,
    os::raw::{c_long, c_void},
    ptr,
    sync::{atomic::AtomicBool, Arc},
};

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(
    feature = "pipeline-cache",
    derive(serde::Serialize, serde::Deserialize)
)]
pub struct EntryPoint {
    pub internal_name: Result<String, naga::back::msl::EntryPointError>,
    pub work_group_size: [u32; 3],
}

pub type EntryPointMap = FastHashMap<(naga::ShaderStage, String), EntryPoint>;
/// An index of a resource within descriptor pool.
pub type PoolResourceIndex = u32;

pub struct ShaderModule {
    pub(crate) prefer_naga: bool,
    #[cfg(feature = "cross")]
    pub(crate) spv: Vec<u32>,
    #[cfg(feature = "pipeline-cache")]
    pub(crate) spv_hash: u64,
    pub(crate) naga: Result<hal::device::NagaShader, String>,
}

impl fmt::Debug for ShaderModule {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "ShaderModule()")
    }
}

bitflags! {
    /// Subpass attachment operations.
    pub struct AttachmentOps: u8 {
        const LOAD = 0x1;
        const STORE = 0x2;
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SubpassData<T> {
    pub colors: ArrayVec<[T; MAX_COLOR_ATTACHMENTS]>,
    pub depth_stencil: Option<T>,
}

impl<T> Default for SubpassData<T> {
    fn default() -> Self {
        SubpassData {
            colors: ArrayVec::new(),
            depth_stencil: None,
        }
    }
}

impl<T> SubpassData<T> {
    pub fn map<V, F: Fn(&T) -> V>(&self, fun: F) -> SubpassData<V> {
        SubpassData {
            colors: self.colors.iter().map(&fun).collect(),
            depth_stencil: self.depth_stencil.as_ref().map(fun),
        }
    }
}

pub type SubpassFormats = SubpassData<(metal::MTLPixelFormat, Channel)>;

#[derive(Debug)]
pub struct AttachmentInfo {
    pub id: AttachmentId,
    pub resolve_id: Option<AttachmentId>,
    pub ops: AttachmentOps,
    pub format: metal::MTLPixelFormat,
    pub channel: Channel,
}

#[derive(Debug)]
pub struct Subpass {
    pub attachments: SubpassData<AttachmentInfo>,
    pub inputs: Vec<AttachmentId>,
    pub samples: image::NumSamples,
}

#[derive(Debug)]
pub struct RenderPass {
    pub(crate) attachments: Vec<Attachment>,
    pub(crate) subpasses: Vec<Subpass>,
    pub(crate) name: String,
}

#[derive(Debug)]
pub struct Framebuffer {
    pub(crate) extent: image::Extent,
}

#[derive(Clone, Debug)]
pub struct ResourceData<T> {
    pub buffers: T,
    pub textures: T,
    pub samplers: T,
}

impl<T> ResourceData<T> {
    pub fn map<V, F: Fn(&T) -> V>(&self, fun: F) -> ResourceData<V> {
        ResourceData {
            buffers: fun(&self.buffers),
            textures: fun(&self.textures),
            samplers: fun(&self.samplers),
        }
    }
}

impl<T: Copy + Ord> ResourceData<Range<T>> {
    pub fn expand(&mut self, point: ResourceData<T>) {
        //TODO: modify `start` as well?
        self.buffers.end = self.buffers.end.max(point.buffers);
        self.textures.end = self.textures.end.max(point.textures);
        self.samplers.end = self.samplers.end.max(point.samplers);
    }
}

impl ResourceData<PoolResourceIndex> {
    pub fn new() -> Self {
        ResourceData {
            buffers: 0,
            textures: 0,
            samplers: 0,
        }
    }

    #[inline]
    pub fn add_many(&mut self, content: DescriptorContent, count: PoolResourceIndex) {
        if content.contains(DescriptorContent::BUFFER) {
            self.buffers += count;
        }
        if content.contains(DescriptorContent::TEXTURE) {
            self.textures += count;
        }
        if content.contains(DescriptorContent::SAMPLER) {
            self.samplers += count;
        }
    }
    #[inline]
    pub fn add(&mut self, content: DescriptorContent) {
        self.add_many(content, 1)
    }
}

#[derive(Clone, Debug)]
pub struct MultiStageData<T> {
    pub vs: T,
    pub ps: T,
    pub cs: T,
}

pub type MultiStageResourceCounters = MultiStageData<ResourceData<ResourceIndex>>;

#[derive(Debug)]
pub struct DescriptorSetInfo {
    pub offsets: MultiStageResourceCounters,
    pub dynamic_buffers: Vec<MultiStageData<PoolResourceIndex>>,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct PushConstantInfo {
    pub count: u32,
    pub buffer_index: ResourceIndex,
}

#[derive(Debug)]
pub struct PipelineLayout {
    #[cfg(feature = "cross")]
    pub(crate) spirv_cross_options: spirv_cross::msl::CompilerOptions,
    pub(crate) naga_options: naga::back::msl::Options,
    pub(crate) infos: Vec<DescriptorSetInfo>,
    pub(crate) total: MultiStageResourceCounters,
    pub(crate) push_constants: MultiStageData<Option<PushConstantInfo>>,
    pub(crate) total_push_constants: u32,
}

#[derive(Clone, Debug)]
pub struct ModuleInfo {
    pub library: metal::Library,
    pub entry_point_map: EntryPointMap,
    pub rasterization_enabled: bool,
}

#[derive(Clone, Debug)]
#[cfg_attr(
    feature = "pipeline-cache",
    derive(serde::Serialize, serde::Deserialize)
)]
pub struct SerializableModuleInfo {
    pub source: String,
    pub entry_point_map: EntryPointMap,
    pub rasterization_enabled: bool,
}

#[cfg(not(feature = "pipeline-cache"))]
pub type PipelineCache = ();

#[cfg(feature = "pipeline-cache")]
pub use crate::pipeline_cache::PipelineCache;

#[derive(Clone, Debug, PartialEq)]
pub struct RasterizerState {
    //TODO: more states
    pub front_winding: metal::MTLWinding,
    pub fill_mode: metal::MTLTriangleFillMode,
    pub cull_mode: metal::MTLCullMode,
    pub depth_clip: Option<metal::MTLDepthClipMode>,
}

impl Default for RasterizerState {
    fn default() -> Self {
        RasterizerState {
            front_winding: metal::MTLWinding::Clockwise,
            fill_mode: metal::MTLTriangleFillMode::Fill,
            cull_mode: metal::MTLCullMode::None,
            depth_clip: None,
        }
    }
}

#[derive(Debug)]
pub struct StencilState<T: Clone> {
    pub reference_values: pso::Sided<T>,
    pub read_masks: pso::Sided<T>,
    pub write_masks: pso::Sided<T>,
}

pub type VertexBufferVec = Vec<(pso::VertexBufferDesc, pso::ElemOffset)>;

#[derive(Debug)]
pub struct GraphicsPipeline {
    // we hold the compiled libraries here for now
    // TODO: move to some cache in `Device`
    pub(crate) vs_lib: metal::Library,
    pub(crate) fs_lib: Option<metal::Library>,
    pub(crate) raw: metal::RenderPipelineState,
    pub(crate) primitive_type: metal::MTLPrimitiveType,
    pub(crate) vs_pc_info: Option<PushConstantInfo>,
    pub(crate) ps_pc_info: Option<PushConstantInfo>,
    pub(crate) rasterizer_state: Option<RasterizerState>,
    pub(crate) depth_bias: pso::State<pso::DepthBias>,
    pub(crate) depth_stencil_desc: pso::DepthStencilDesc,
    pub(crate) baked_states: pso::BakedStates,
    /// The mapping from Metal vertex buffers to Vulkan ones.
    /// This is needed because Vulkan allows attribute offsets to exceed the strides,
    /// while Metal does not. Thus, we register extra vertex buffer bindings with
    /// adjusted offsets to cover this use case.
    pub(crate) vertex_buffers: VertexBufferVec,
    /// Tracked attachment formats
    pub(crate) attachment_formats: SubpassFormats,
    pub(crate) samples: image::NumSamples,
}

unsafe impl Send for GraphicsPipeline {}
unsafe impl Sync for GraphicsPipeline {}

#[derive(Debug)]
pub struct ComputePipeline {
    pub(crate) cs_lib: metal::Library,
    pub(crate) raw: metal::ComputePipelineState,
    pub(crate) work_group_size: metal::MTLSize,
    pub(crate) pc_info: Option<PushConstantInfo>,
}

unsafe impl Send for ComputePipeline {}
unsafe impl Sync for ComputePipeline {}

#[derive(Debug)]
pub enum ImageLike {
    /// This image has not yet been bound to memory.
    Unbound {
        descriptor: metal::TextureDescriptor,
        mip_sizes: Vec<buffer::Offset>,
        host_visible: bool,
        name: String,
    },
    /// This is a linearly tiled HOST-visible image, which is represented by a buffer.
    Buffer(Buffer),
    /// This is a regular image represented by a texture.
    Texture(metal::Texture),
}

impl ImageLike {
    pub fn as_texture(&self) -> &metal::TextureRef {
        match *self {
            ImageLike::Unbound { .. } | ImageLike::Buffer(..) => panic!("Expected bound image!"),
            ImageLike::Texture(ref tex) => tex,
        }
    }
}

#[derive(Debug)]
pub struct Image {
    pub(crate) like: ImageLike,
    pub(crate) kind: image::Kind,
    pub(crate) mip_levels: image::Level,
    pub(crate) format_desc: FormatDesc,
    pub(crate) shader_channel: Channel,
    pub(crate) mtl_format: metal::MTLPixelFormat,
    pub(crate) mtl_type: metal::MTLTextureType,
}

impl Image {
    pub(crate) fn pitches_impl(
        extent: image::Extent,
        format_desc: FormatDesc,
    ) -> [buffer::Offset; 4] {
        let bytes_per_texel = format_desc.bits as image::Size >> 3;
        let row_pitch = extent.width * bytes_per_texel;
        let depth_pitch = extent.height * row_pitch;
        let array_pitch = extent.depth * depth_pitch;
        [
            bytes_per_texel as _,
            row_pitch as _,
            depth_pitch as _,
            array_pitch as _,
        ]
    }
    pub(crate) fn pitches(&self, level: image::Level) -> [buffer::Offset; 4] {
        let extent = self.kind.extent().at_level(level);
        Self::pitches_impl(extent, self.format_desc)
    }
    pub(crate) fn byte_offset(&self, offset: image::Offset) -> buffer::Offset {
        let pitches = Self::pitches_impl(self.kind.extent(), self.format_desc);
        pitches[0] * offset.x as buffer::Offset
            + pitches[1] * offset.y as buffer::Offset
            + pitches[2] * offset.z as buffer::Offset
    }
    pub(crate) fn byte_extent(&self, extent: image::Extent) -> buffer::Offset {
        let bytes_per_texel = self.format_desc.bits as image::Size >> 3;
        (bytes_per_texel * extent.width * extent.height * extent.depth) as _
    }
    /// View this cube texture as a 2D array.
    pub(crate) fn view_cube_as_2d(&self) -> Option<metal::Texture> {
        match self.mtl_type {
            metal::MTLTextureType::Cube | metal::MTLTextureType::CubeArray => {
                let raw = self.like.as_texture();
                Some(raw.new_texture_view_from_slice(
                    self.mtl_format,
                    metal::MTLTextureType::D2Array,
                    metal::NSRange {
                        location: 0,
                        length: raw.mipmap_level_count(),
                    },
                    metal::NSRange {
                        location: 0,
                        length: self.kind.num_layers() as _,
                    },
                ))
            }
            _ => None,
        }
    }
}

unsafe impl Send for Image {}
unsafe impl Sync for Image {}

#[derive(Debug)]
pub struct BufferView {
    pub(crate) raw: metal::Texture,
}

unsafe impl Send for BufferView {}
unsafe impl Sync for BufferView {}

#[derive(Debug)]
pub struct ImageView {
    pub(crate) texture: metal::Texture,
    pub(crate) mtl_format: metal::MTLPixelFormat,
}

unsafe impl Send for ImageView {}
unsafe impl Sync for ImageView {}

#[derive(Debug)]
pub struct Sampler {
    pub(crate) raw: Option<metal::SamplerState>,
    #[cfg(feature = "cross")]
    pub(crate) cross_data: spirv_cross::msl::SamplerData,
    pub(crate) data: naga::back::msl::sampler::InlineSampler,
}

unsafe impl Send for Sampler {}
unsafe impl Sync for Sampler {}

#[derive(Clone, Debug)]
pub struct Semaphore {
    pub(crate) system: Option<SystemSemaphore>,
}

#[derive(Debug)]
pub enum Buffer {
    Unbound {
        size: u64,
        usage: buffer::Usage,
        name: String,
    },
    Bound {
        raw: metal::Buffer,
        range: Range<u64>,
        options: metal::MTLResourceOptions,
    },
}

unsafe impl Send for Buffer {}
unsafe impl Sync for Buffer {}

impl Buffer {
    //TODO: consider returning `AsNative`?
    pub fn as_bound(&self) -> (&metal::BufferRef, &Range<u64>) {
        match *self {
            Buffer::Unbound { .. } => panic!("Expected bound buffer!"),
            Buffer::Bound {
                ref raw, ref range, ..
            } => (raw, range),
        }
    }
}

#[derive(Debug)]
pub struct DescriptorEmulatedPoolInner {
    pub(crate) samplers: Vec<(pso::ShaderStageFlags, Option<SamplerPtr>)>,
    pub(crate) textures: Vec<(pso::ShaderStageFlags, Option<TexturePtr>, image::Layout)>,
    pub(crate) buffers: Vec<(pso::ShaderStageFlags, Option<BufferPtr>, buffer::Offset)>,
}

#[derive(Debug)]
pub struct DescriptorArgumentPoolInner {
    pub(crate) resources: Vec<UsedResource>,
}

#[derive(Debug)]
pub enum DescriptorPool {
    Emulated {
        inner: Arc<RwLock<DescriptorEmulatedPoolInner>>,
        allocators: ResourceData<RangeAllocator<PoolResourceIndex>>,
    },
    ArgumentBuffer {
        raw: metal::Buffer,
        raw_allocator: RangeAllocator<buffer::Offset>,
        alignment: buffer::Offset,
        inner: Arc<RwLock<DescriptorArgumentPoolInner>>,
        res_allocator: RangeAllocator<PoolResourceIndex>,
    },
}
//TODO: re-evaluate Send/Sync here
unsafe impl Send for DescriptorPool {}
unsafe impl Sync for DescriptorPool {}

impl DescriptorPool {
    pub(crate) fn new_emulated(counters: ResourceData<PoolResourceIndex>) -> Self {
        let inner = DescriptorEmulatedPoolInner {
            samplers: vec![Default::default(); counters.samplers as usize],
            textures: vec![Default::default(); counters.textures as usize],
            buffers: vec![Default::default(); counters.buffers as usize],
        };
        DescriptorPool::Emulated {
            inner: Arc::new(RwLock::new(inner)),
            allocators: ResourceData {
                samplers: RangeAllocator::new(0..counters.samplers),
                textures: RangeAllocator::new(0..counters.textures),
                buffers: RangeAllocator::new(0..counters.buffers),
            },
        }
    }

    pub(crate) fn new_argument(
        raw: metal::Buffer,
        total_bytes: buffer::Offset,
        alignment: buffer::Offset,
        total_resources: usize,
    ) -> Self {
        let default = UsedResource {
            ptr: ptr::null_mut(),
            usage: metal::MTLResourceUsage::empty(),
        };
        DescriptorPool::ArgumentBuffer {
            raw,
            raw_allocator: RangeAllocator::new(0..total_bytes),
            alignment,
            inner: Arc::new(RwLock::new(DescriptorArgumentPoolInner {
                resources: vec![default; total_resources],
            })),
            res_allocator: RangeAllocator::new(0..total_resources as PoolResourceIndex),
        }
    }

    fn report_available(&self) {
        match *self {
            DescriptorPool::Emulated { ref allocators, .. } => {
                trace!(
                    "\tavailable {} samplers, {} textures, and {} buffers",
                    allocators.samplers.total_available(),
                    allocators.textures.total_available(),
                    allocators.buffers.total_available(),
                );
            }
            DescriptorPool::ArgumentBuffer {
                ref raw_allocator,
                ref res_allocator,
                ..
            } => {
                trace!(
                    "\tavailable {} bytes for {} resources",
                    raw_allocator.total_available(),
                    res_allocator.total_available(),
                );
            }
        }
    }
}

impl pso::DescriptorPool<Backend> for DescriptorPool {
    unsafe fn allocate_one(
        &mut self,
        set_layout: &DescriptorSetLayout,
    ) -> Result<DescriptorSet, pso::AllocationError> {
        self.report_available();
        match *self {
            DescriptorPool::Emulated {
                ref inner,
                ref mut allocators,
            } => {
                debug!("pool: allocate_one");
                let (layouts, total, immutable_samplers) = match *set_layout {
                    DescriptorSetLayout::Emulated {
                        ref layouts,
                        ref total,
                        ref immutable_samplers,
                    } => (layouts, total, immutable_samplers),
                    _ => return Err(pso::AllocationError::IncompatibleLayout),
                };

                // try to allocate the ranges from the pool
                let sampler_range = if total.samplers != 0 {
                    match allocators.samplers.allocate_range(total.samplers as _) {
                        Ok(range) => {
                            // fill out the stages for immutable samplers
                            let mut data = inner.write();
                            let mut offset = range.start as usize;
                            for layout in layouts.iter() {
                                if layout.content.contains(DescriptorContent::SAMPLER) {
                                    if immutable_samplers.contains_key(&layout.binding) {
                                        data.samplers[offset] = (layout.stages, None);
                                    }
                                    offset += 1;
                                }
                            }
                            range
                        }
                        Err(e) => {
                            return Err(if e.fragmented_free_length >= total.samplers {
                                pso::AllocationError::FragmentedPool
                            } else {
                                pso::AllocationError::OutOfPoolMemory
                            });
                        }
                    }
                } else {
                    0..0
                };
                let texture_range = if total.textures != 0 {
                    match allocators.textures.allocate_range(total.textures as _) {
                        Ok(range) => range,
                        Err(e) => {
                            if sampler_range.end != 0 {
                                allocators.samplers.free_range(sampler_range);
                            }
                            return Err(if e.fragmented_free_length >= total.samplers {
                                pso::AllocationError::FragmentedPool
                            } else {
                                pso::AllocationError::OutOfPoolMemory
                            });
                        }
                    }
                } else {
                    0..0
                };
                let buffer_range = if total.buffers != 0 {
                    match allocators.buffers.allocate_range(total.buffers as _) {
                        Ok(range) => range,
                        Err(e) => {
                            if sampler_range.end != 0 {
                                allocators.samplers.free_range(sampler_range);
                            }
                            if texture_range.end != 0 {
                                allocators.textures.free_range(texture_range);
                            }
                            return Err(if e.fragmented_free_length >= total.samplers {
                                pso::AllocationError::FragmentedPool
                            } else {
                                pso::AllocationError::OutOfPoolMemory
                            });
                        }
                    }
                } else {
                    0..0
                };

                let resources = ResourceData {
                    buffers: buffer_range,
                    textures: texture_range,
                    samplers: sampler_range,
                };

                Ok(DescriptorSet::Emulated {
                    pool: Arc::clone(inner),
                    layouts: Arc::clone(layouts),
                    resources,
                })
            }
            DescriptorPool::ArgumentBuffer {
                ref raw,
                ref mut raw_allocator,
                alignment,
                ref inner,
                ref mut res_allocator,
            } => {
                let (encoder, stage_flags, bindings, total) = match *set_layout {
                    DescriptorSetLayout::ArgumentBuffer {
                        ref encoder,
                        stage_flags,
                        ref bindings,
                        total,
                        ..
                    } => (encoder, stage_flags, bindings, total),
                    _ => return Err(pso::AllocationError::IncompatibleLayout),
                };
                let range = res_allocator
                    .allocate_range(total as PoolResourceIndex)
                    .map_err(|_| pso::AllocationError::OutOfPoolMemory)?;

                let raw_range = raw_allocator
                    .allocate_range(encoder.encoded_length() + alignment)
                    .expect("Argument encoding length is inconsistent!");
                let raw_offset = (raw_range.start + alignment - 1) & !(alignment - 1);

                #[cfg(feature = "cross")]
                {
                    let mut data = inner.write();
                    for arg in bindings.values() {
                        if arg.bind_target.buffer.is_some() || arg.bind_target.texture.is_some() {
                            let pos = (range.start + arg.res_offset) as usize;
                            for ur in data.resources[pos..pos + arg.count].iter_mut() {
                                ur.usage = arg.usage;
                            }
                        }
                    }
                }

                Ok(DescriptorSet::ArgumentBuffer {
                    raw: raw.clone(),
                    raw_offset,
                    pool: Arc::clone(inner),
                    range,
                    encoder: encoder.clone(),
                    bindings: Arc::clone(bindings),
                    stage_flags,
                })
            }
        }
    }

    unsafe fn free<I>(&mut self, descriptor_sets: I)
    where
        I: Iterator<Item = DescriptorSet>,
    {
        match self {
            DescriptorPool::Emulated {
                ref inner,
                ref mut allocators,
            } => {
                debug!("pool: free_sets");
                let mut data = inner.write();
                for descriptor_set in descriptor_sets {
                    match descriptor_set {
                        DescriptorSet::Emulated { resources, .. } => {
                            debug!("\t{:?} resources", resources);
                            for sampler in &mut data.samplers
                                [resources.samplers.start as usize..resources.samplers.end as usize]
                            {
                                sampler.1 = None;
                            }
                            if resources.samplers.start != resources.samplers.end {
                                allocators.samplers.free_range(resources.samplers);
                            }
                            for image in &mut data.textures
                                [resources.textures.start as usize..resources.textures.end as usize]
                            {
                                image.1 = None;
                            }
                            if resources.textures.start != resources.textures.end {
                                allocators.textures.free_range(resources.textures);
                            }
                            for buffer in &mut data.buffers
                                [resources.buffers.start as usize..resources.buffers.end as usize]
                            {
                                buffer.1 = None;
                            }
                            if resources.buffers.start != resources.buffers.end {
                                allocators.buffers.free_range(resources.buffers);
                            }
                        }
                        DescriptorSet::ArgumentBuffer { .. } => panic!(
                            "Tried to free a DescriptorSet not given out by this DescriptorPool!"
                        ),
                    }
                }
            }
            DescriptorPool::ArgumentBuffer {
                ref mut raw_allocator,
                ref mut res_allocator,
                ref inner,
                ..
            } => {
                let mut data = inner.write();
                for descriptor_set in descriptor_sets {
                    match descriptor_set {
                        DescriptorSet::Emulated { .. } => panic!(
                            "Tried to free a DescriptorSet not given out by this DescriptorPool!"
                        ),
                        DescriptorSet::ArgumentBuffer {
                            raw_offset,
                            range,
                            encoder,
                            ..
                        } => {
                            for ur in
                                data.resources[range.start as usize..range.end as usize].iter_mut()
                            {
                                ur.ptr = ptr::null_mut();
                                ur.usage = metal::MTLResourceUsage::empty();
                            }

                            let handle_range = raw_offset..raw_offset + encoder.encoded_length();
                            raw_allocator.free_range(handle_range);
                            res_allocator.free_range(range);
                        }
                    }
                }
            }
        }
        self.report_available();
    }

    unsafe fn reset(&mut self) {
        match *self {
            DescriptorPool::Emulated {
                ref inner,
                ref mut allocators,
            } => {
                debug!("pool: reset");
                if allocators.samplers.is_empty()
                    && allocators.textures.is_empty()
                    && allocators.buffers.is_empty()
                {
                    return; // spare the locking
                }
                let mut data = inner.write();

                for range in allocators.samplers.allocated_ranges() {
                    for sampler in &mut data.samplers[range.start as usize..range.end as usize] {
                        sampler.1 = None;
                    }
                }
                for range in allocators.textures.allocated_ranges() {
                    for texture in &mut data.textures[range.start as usize..range.end as usize] {
                        texture.1 = None;
                    }
                }
                for range in allocators.buffers.allocated_ranges() {
                    for buffer in &mut data.buffers[range.start as usize..range.end as usize] {
                        buffer.1 = None;
                    }
                }

                allocators.samplers.reset();
                allocators.textures.reset();
                allocators.buffers.reset();
            }
            DescriptorPool::ArgumentBuffer {
                ref mut raw_allocator,
                ref mut res_allocator,
                ..
            } => {
                raw_allocator.reset();
                res_allocator.reset();
            }
        }
    }
}

bitflags! {
    /// Descriptor content flags.
    pub struct DescriptorContent: u8 {
        const BUFFER = 1<<0;
        const DYNAMIC_BUFFER = 1<<1;
        const TEXTURE = 1<<2;
        const SAMPLER = 1<<3;
        const IMMUTABLE_SAMPLER = 1<<4;
        const WRITABLE = 1 << 5;
    }
}

impl From<pso::DescriptorType> for DescriptorContent {
    fn from(ty: pso::DescriptorType) -> Self {
        match ty {
            pso::DescriptorType::Sampler => DescriptorContent::SAMPLER,
            pso::DescriptorType::Image { ty } => match ty {
                pso::ImageDescriptorType::Sampled { with_sampler: true } => {
                    DescriptorContent::TEXTURE | DescriptorContent::SAMPLER
                }
                pso::ImageDescriptorType::Storage { read_only: false } => {
                    DescriptorContent::TEXTURE | DescriptorContent::WRITABLE
                }
                _ => DescriptorContent::TEXTURE,
            },
            pso::DescriptorType::Buffer { format, ty } => {
                let base = match format {
                    pso::BufferDescriptorFormat::Structured { dynamic_offset } => {
                        match dynamic_offset {
                            true => DescriptorContent::BUFFER | DescriptorContent::DYNAMIC_BUFFER,
                            false => DescriptorContent::BUFFER,
                        }
                    }
                    pso::BufferDescriptorFormat::Texel => DescriptorContent::TEXTURE,
                };
                match ty {
                    pso::BufferDescriptorType::Storage { read_only: false } => {
                        base | DescriptorContent::WRITABLE
                    }
                    _ => base,
                }
            }
            pso::DescriptorType::InputAttachment => DescriptorContent::TEXTURE,
        }
    }
}

// Note: this structure is iterated often, so it makes sense to keep it dense
#[derive(Debug)]
pub struct DescriptorLayout {
    pub content: DescriptorContent,
    pub stages: pso::ShaderStageFlags,
    pub binding: pso::DescriptorBinding,
    pub array_index: pso::DescriptorArrayIndex,
}

#[derive(Debug)]
pub struct ArgumentLayout {
    pub(crate) bind_target: naga::back::msl::BindTarget,
    pub(crate) res_offset: PoolResourceIndex,
    pub(crate) count: pso::DescriptorArrayIndex,
    pub(crate) usage: metal::MTLResourceUsage,
    pub(crate) content: DescriptorContent,
}

#[derive(Debug)]
pub struct ImmutableSampler {
    pub(crate) data: naga::back::msl::sampler::InlineSampler,
    #[cfg(feature = "cross")]
    pub(crate) cross_data: spirv_cross::msl::SamplerData,
}

#[derive(Debug)]
pub enum DescriptorSetLayout {
    Emulated {
        layouts: Arc<Vec<DescriptorLayout>>,
        total: ResourceData<PoolResourceIndex>,
        immutable_samplers: FastHashMap<pso::DescriptorBinding, ImmutableSampler>,
    },
    ArgumentBuffer {
        encoder: metal::ArgumentEncoder,
        stage_flags: pso::ShaderStageFlags,
        bindings: Arc<FastHashMap<pso::DescriptorBinding, ArgumentLayout>>,
        total: PoolResourceIndex,
    },
}
unsafe impl Send for DescriptorSetLayout {}
unsafe impl Sync for DescriptorSetLayout {}

#[derive(Clone, Debug)]
pub struct UsedResource {
    pub(crate) ptr: *mut metal::MTLResource,
    pub(crate) usage: metal::MTLResourceUsage,
}

#[derive(Debug)]
pub enum DescriptorSet {
    Emulated {
        //TODO: consider storing the descriptors right here,
        // to reduce the amount of locking, e.g. in descriptor binding.
        pool: Arc<RwLock<DescriptorEmulatedPoolInner>>,
        layouts: Arc<Vec<DescriptorLayout>>,
        resources: ResourceData<Range<PoolResourceIndex>>,
    },
    ArgumentBuffer {
        raw: metal::Buffer,
        raw_offset: buffer::Offset,
        pool: Arc<RwLock<DescriptorArgumentPoolInner>>,
        range: Range<PoolResourceIndex>,
        encoder: metal::ArgumentEncoder,
        bindings: Arc<FastHashMap<pso::DescriptorBinding, ArgumentLayout>>,
        stage_flags: pso::ShaderStageFlags,
    },
}
unsafe impl Send for DescriptorSet {}
unsafe impl Sync for DescriptorSet {}

#[derive(Debug)]
pub struct Memory {
    pub(crate) heap: MemoryHeap,
    pub(crate) size: u64,
}

impl Memory {
    pub(crate) fn new(heap: MemoryHeap, size: u64) -> Self {
        Memory { heap, size }
    }

    pub(crate) fn resolve(&self, range: &Segment) -> Range<u64> {
        range.offset..range.size.map_or(self.size, |s| range.offset + s)
    }
}

unsafe impl Send for Memory {}
unsafe impl Sync for Memory {}

#[derive(Debug)]
pub(crate) enum MemoryHeap {
    Private,
    Public(MemoryTypeId, metal::Buffer),
    Native(metal::Heap),
}

#[derive(Default)]
pub(crate) struct ArgumentArray {
    arguments: Vec<metal::ArgumentDescriptor>,
    position: usize,
}

impl ArgumentArray {
    pub fn describe_usage(ty: pso::DescriptorType) -> metal::MTLResourceUsage {
        use hal::pso::DescriptorType as Dt;
        use metal::MTLResourceUsage;

        match ty {
            Dt::Sampler => MTLResourceUsage::empty(),
            Dt::Image { ty } => match ty {
                pso::ImageDescriptorType::Sampled { .. } => MTLResourceUsage::Sample,
                pso::ImageDescriptorType::Storage { read_only: true } => MTLResourceUsage::Read,
                pso::ImageDescriptorType::Storage { .. } => MTLResourceUsage::Write,
            },
            Dt::Buffer { ty, format } => match ty {
                pso::BufferDescriptorType::Storage { read_only: true } => MTLResourceUsage::Read,
                pso::BufferDescriptorType::Storage { .. } => MTLResourceUsage::Write,
                pso::BufferDescriptorType::Uniform => match format {
                    pso::BufferDescriptorFormat::Structured { .. } => MTLResourceUsage::Read,
                    pso::BufferDescriptorFormat::Texel => MTLResourceUsage::Sample,
                },
            },
            Dt::InputAttachment => MTLResourceUsage::Sample,
        }
    }

    pub fn push(
        &mut self,
        ty: metal::MTLDataType,
        count: usize,
        usage: metal::MTLResourceUsage,
    ) -> usize {
        use metal::{MTLArgumentAccess, MTLResourceUsage};

        let pos = self.position;
        self.position += count;
        let access = if usage == MTLResourceUsage::Write {
            MTLArgumentAccess::ReadWrite
        } else {
            MTLArgumentAccess::ReadOnly
        };

        let arg = metal::ArgumentDescriptor::new();
        arg.set_array_length(count as u64);
        arg.set_index(pos as u64);
        arg.set_access(access);
        arg.set_data_type(ty);
        self.arguments.push(arg.to_owned());

        pos
    }

    pub fn build<'a>(self) -> (&'a metal::ArrayRef<metal::ArgumentDescriptor>, usize) {
        (
            metal::Array::from_owned_slice(&self.arguments),
            self.position,
        )
    }
}

#[derive(Debug)]
pub enum QueryPool {
    Occlusion(Range<u32>),
    Timestamp,
}

#[derive(Debug)]
pub enum Fence {
    Idle { signaled: bool },
    PendingSubmission(metal::CommandBuffer),
}

unsafe impl Send for Fence {}
unsafe impl Sync for Fence {}

//TODO: review the atomic ordering
//TODO: reconsider if Arc<Atomic> is needed
#[derive(Debug)]
pub struct Event(pub(crate) Arc<AtomicBool>);

extern "C" {
    fn dispatch_semaphore_wait(semaphore: *mut c_void, timeout: u64) -> c_long;
    fn dispatch_semaphore_signal(semaphore: *mut c_void) -> c_long;
    fn dispatch_semaphore_create(value: c_long) -> *mut c_void;
    fn dispatch_release(object: *mut c_void);
}

#[cfg(feature = "signpost")]
extern "C" {
    fn kdebug_signpost(code: u32, arg1: usize, arg2: usize, arg3: usize, arg4: usize);
    fn kdebug_signpost_start(code: u32, arg1: usize, arg2: usize, arg3: usize, arg4: usize);
    fn kdebug_signpost_end(code: u32, arg1: usize, arg2: usize, arg3: usize, arg4: usize);
}

#[derive(Clone, Debug)]
pub struct SystemSemaphore(*mut c_void);
unsafe impl Send for SystemSemaphore {}
unsafe impl Sync for SystemSemaphore {}

impl Drop for SystemSemaphore {
    fn drop(&mut self) {
        unsafe { dispatch_release(self.0) }
    }
}
impl SystemSemaphore {
    pub(crate) fn new() -> Self {
        SystemSemaphore(unsafe { dispatch_semaphore_create(1) })
    }
    pub(crate) fn signal(&self) {
        unsafe {
            dispatch_semaphore_signal(self.0);
        }
    }
    pub(crate) fn wait(&self, timeout: u64) {
        unsafe {
            dispatch_semaphore_wait(self.0, timeout);
        }
    }
}

#[derive(Clone, Debug)]
pub struct Signpost {
    code: u32,
    args: [usize; 4],
}

impl Drop for Signpost {
    fn drop(&mut self) {
        #[cfg(feature = "signpost")]
        unsafe {
            kdebug_signpost_end(
                self.code,
                self.args[0],
                self.args[1],
                self.args[2],
                self.args[3],
            );
        }
    }
}

#[allow(dead_code)]
impl Signpost {
    pub(crate) fn new(code: u32, args: [usize; 4]) -> Self {
        #[cfg(feature = "signpost")]
        unsafe {
            kdebug_signpost_start(code, args[0], args[1], args[2], args[3]);
        }
        Signpost { code, args }
    }
    pub(crate) fn place(code: u32, args: [usize; 4]) {
        #[cfg(feature = "signpost")]
        unsafe {
            kdebug_signpost(code, args[0], args[1], args[2], args[3]);
        }
        #[cfg(not(feature = "signpost"))]
        let _ = (code, args);
    }
}
