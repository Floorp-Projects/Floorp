use hal::{buffer, format, image, memory, pass, pso};
use range_alloc::RangeAllocator;

use parking_lot::RwLock;
use winapi::{
    shared::{dxgiformat::DXGI_FORMAT, minwindef::UINT},
    um::d3d12,
};

use std::{collections::BTreeMap, fmt, ops::Range, slice, sync::Arc};

use crate::{
    descriptors_cpu::{Handle, MultiCopyAccumulator},
    root_constants::RootConstant,
    Backend, DescriptorIndex, MAX_VERTEX_BUFFERS,
};

// ShaderModule is either a precompiled if the source comes from HLSL or
// the SPIR-V module doesn't contain specialization constants or push constants
// because they need to be adjusted on pipeline creation.
#[derive(Debug, Hash)]
pub enum ShaderModule {
    Compiled(BTreeMap<String, native::Blob>),
    Spirv(Vec<u32>),
}
unsafe impl Send for ShaderModule {}
unsafe impl Sync for ShaderModule {}

#[derive(Clone, Debug, Hash)]
pub struct BarrierDesc {
    pub(crate) attachment_id: pass::AttachmentId,
    pub(crate) states: Range<d3d12::D3D12_RESOURCE_STATES>,
    pub(crate) flags: d3d12::D3D12_RESOURCE_BARRIER_FLAGS,
}

impl BarrierDesc {
    pub(crate) fn new(
        attachment_id: pass::AttachmentId,
        states: Range<d3d12::D3D12_RESOURCE_STATES>,
    ) -> Self {
        BarrierDesc {
            attachment_id,
            states,
            flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_NONE,
        }
    }

    pub(crate) fn split(self) -> Range<Self> {
        BarrierDesc {
            flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY,
            ..self.clone()
        }..BarrierDesc {
            flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_END_ONLY,
            ..self
        }
    }
}

#[derive(Clone, Debug, Hash)]
pub struct SubpassDesc {
    pub(crate) color_attachments: Vec<pass::AttachmentRef>,
    pub(crate) depth_stencil_attachment: Option<pass::AttachmentRef>,
    pub(crate) input_attachments: Vec<pass::AttachmentRef>,
    pub(crate) resolve_attachments: Vec<pass::AttachmentRef>,
    pub(crate) pre_barriers: Vec<BarrierDesc>,
    pub(crate) post_barriers: Vec<BarrierDesc>,
}

impl SubpassDesc {
    /// Check if an attachment is used by this sub-pass.
    //Note: preserved attachment are not considered used.
    pub(crate) fn is_using(&self, at_id: pass::AttachmentId) -> bool {
        self.color_attachments
            .iter()
            .chain(self.depth_stencil_attachment.iter())
            .chain(self.input_attachments.iter())
            .chain(self.resolve_attachments.iter())
            .any(|&(id, _)| id == at_id)
    }
}

#[derive(Clone, Debug, Hash)]
pub struct RenderPass {
    pub(crate) attachments: Vec<pass::Attachment>,
    pub(crate) subpasses: Vec<SubpassDesc>,
    pub(crate) post_barriers: Vec<BarrierDesc>,
    pub(crate) raw_name: Vec<u16>,
}

// Indirection layer attribute -> remap -> binding.
//
// Required as vulkan allows attribute offsets larger than the stride.
// Storing the stride specified in the pipeline required for vertex buffer binding.
#[derive(Copy, Clone, Debug)]
pub struct VertexBinding {
    // Map into the specified bindings on pipeline creation.
    pub mapped_binding: usize,
    pub stride: UINT,
    // Additional offset to rebase the attributes.
    pub offset: u32,
}

#[derive(Debug)]
pub struct GraphicsPipeline {
    pub(crate) raw: native::PipelineState,
    pub(crate) shared: Arc<PipelineShared>,
    pub(crate) topology: d3d12::D3D12_PRIMITIVE_TOPOLOGY,
    pub(crate) vertex_bindings: [Option<VertexBinding>; MAX_VERTEX_BUFFERS],
    pub(crate) baked_states: pso::BakedStates,
}
unsafe impl Send for GraphicsPipeline {}
unsafe impl Sync for GraphicsPipeline {}

#[derive(Debug)]
pub struct ComputePipeline {
    pub(crate) raw: native::PipelineState,
    pub(crate) shared: Arc<PipelineShared>,
}

unsafe impl Send for ComputePipeline {}
unsafe impl Sync for ComputePipeline {}

bitflags! {
    pub struct SetTableTypes: u8 {
        const SRV_CBV_UAV = 0x1;
        const SAMPLERS = 0x2;
    }
}

pub const SRV_CBV_UAV: SetTableTypes = SetTableTypes::SRV_CBV_UAV;
pub const SAMPLERS: SetTableTypes = SetTableTypes::SAMPLERS;

pub type RootSignatureOffset = usize;

#[derive(Debug, Hash)]
pub struct RootTable {
    pub ty: SetTableTypes,
    pub offset: RootSignatureOffset,
}

#[derive(Debug)]
pub struct RootElement {
    pub table: RootTable,
    pub mutable_bindings: auxil::FastHashSet<pso::DescriptorBinding>,
}

#[derive(Debug)]
pub struct PipelineShared {
    pub(crate) signature: native::RootSignature,
    /// Disjunct, sorted vector of root constant ranges.
    pub(crate) constants: Vec<RootConstant>,
    /// A root offset per parameter.
    pub(crate) parameter_offsets: Vec<u32>,
    /// Total number of root slots occupied by the pipeline.
    pub(crate) total_slots: u32,
}

unsafe impl Send for PipelineShared {}
unsafe impl Sync for PipelineShared {}

#[derive(Debug)]
pub struct PipelineLayout {
    pub(crate) shared: Arc<PipelineShared>,
    // Storing for each associated descriptor set layout, which tables we created
    // in the root signature. This is required for binding descriptor sets.
    pub(crate) elements: Vec<RootElement>,
}

#[derive(Debug, Clone)]
pub struct Framebuffer {
    /// Number of layers in the render area. Required for subpass resolves.
    pub(crate) layers: image::Layer,
}

#[derive(Clone, Debug)]
pub struct BufferUnbound {
    pub(crate) requirements: memory::Requirements,
    pub(crate) usage: buffer::Usage,
    pub(crate) name: Option<Vec<u16>>,
}

pub struct BufferBound {
    pub(crate) resource: native::Resource,
    pub(crate) requirements: memory::Requirements,
    pub(crate) clear_uav: Option<Handle>,
}

impl fmt::Debug for BufferBound {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("BufferBound")
    }
}

unsafe impl Send for BufferBound {}
unsafe impl Sync for BufferBound {}

pub enum Buffer {
    Unbound(BufferUnbound),
    Bound(BufferBound),
}

impl fmt::Debug for Buffer {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Buffer")
    }
}

impl Buffer {
    pub(crate) fn expect_unbound(&self) -> &BufferUnbound {
        match *self {
            Buffer::Unbound(ref unbound) => unbound,
            Buffer::Bound(_) => panic!("Expected unbound buffer"),
        }
    }

    pub(crate) fn expect_bound(&self) -> &BufferBound {
        match *self {
            Buffer::Unbound(_) => panic!("Expected bound buffer"),
            Buffer::Bound(ref bound) => bound,
        }
    }
}

#[derive(Copy, Clone)]
pub struct BufferView {
    // Descriptor handle for uniform texel buffers.
    pub(crate) handle_srv: Option<Handle>,
    // Descriptor handle for storage texel buffers.
    pub(crate) handle_uav: Option<Handle>,
}

impl fmt::Debug for BufferView {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("BufferView")
    }
}

unsafe impl Send for BufferView {}
unsafe impl Sync for BufferView {}

#[derive(Clone)]
pub enum Place {
    Heap { raw: native::Heap, offset: u64 },
    Swapchain {},
}

#[derive(Clone)]
pub struct ImageBound {
    pub(crate) resource: native::Resource,
    pub(crate) place: Place,
    pub(crate) surface_type: format::SurfaceType,
    pub(crate) kind: image::Kind,
    pub(crate) mip_levels: image::Level,
    pub(crate) default_view_format: Option<DXGI_FORMAT>,
    pub(crate) view_caps: image::ViewCapabilities,
    pub(crate) descriptor: d3d12::D3D12_RESOURCE_DESC,
    pub(crate) clear_cv: Vec<Handle>,
    pub(crate) clear_dv: Vec<Handle>,
    pub(crate) clear_sv: Vec<Handle>,
    pub(crate) requirements: memory::Requirements,
}

impl fmt::Debug for ImageBound {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("ImageBound")
    }
}

unsafe impl Send for ImageBound {}
unsafe impl Sync for ImageBound {}

impl ImageBound {
    pub fn calc_subresource(&self, mip_level: UINT, layer: UINT, plane: UINT) -> UINT {
        mip_level
            + (layer * self.descriptor.MipLevels as UINT)
            + (plane * self.descriptor.MipLevels as UINT * self.kind.num_layers() as UINT)
    }
}

#[derive(Clone)]
pub struct ImageUnbound {
    pub(crate) desc: d3d12::D3D12_RESOURCE_DESC,
    pub(crate) view_format: Option<DXGI_FORMAT>,
    pub(crate) dsv_format: Option<DXGI_FORMAT>,
    pub(crate) requirements: memory::Requirements,
    pub(crate) format: format::Format,
    pub(crate) kind: image::Kind,
    pub(crate) mip_levels: image::Level,
    pub(crate) usage: image::Usage,
    pub(crate) tiling: image::Tiling,
    pub(crate) view_caps: image::ViewCapabilities,
    //TODO: use hal::format::FormatDesc
    pub(crate) bytes_per_block: u8,
    // Dimension of a texel block (compressed formats).
    pub(crate) block_dim: (u8, u8),
    pub(crate) name: Option<Vec<u16>>,
}

impl fmt::Debug for ImageUnbound {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("ImageUnbound")
    }
}

impl ImageUnbound {
    pub fn calc_subresource(&self, mip_level: UINT, layer: UINT, plane: UINT) -> UINT {
        mip_level
            + (layer * self.desc.MipLevels as UINT)
            + (plane * self.desc.MipLevels as UINT * self.kind.num_layers() as UINT)
    }
}

#[derive(Clone)]
pub enum Image {
    Unbound(ImageUnbound),
    Bound(ImageBound),
}

impl fmt::Debug for Image {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Image")
    }
}

impl Image {
    pub(crate) fn expect_unbound(&self) -> &ImageUnbound {
        match *self {
            Image::Unbound(ref unbound) => unbound,
            Image::Bound(_) => panic!("Expected unbound image"),
        }
    }

    pub(crate) fn expect_bound(&self) -> &ImageBound {
        match *self {
            Image::Unbound(_) => panic!("Expected bound image"),
            Image::Bound(ref bound) => bound,
        }
    }

    pub fn get_desc(&self) -> &d3d12::D3D12_RESOURCE_DESC {
        match self {
            Image::Bound(i) => &i.descriptor,
            Image::Unbound(i) => &i.desc,
        }
    }

    pub fn calc_subresource(&self, mip_level: UINT, layer: UINT, plane: UINT) -> UINT {
        match self {
            Image::Bound(i) => i.calc_subresource(mip_level, layer, plane),
            Image::Unbound(i) => i.calc_subresource(mip_level, layer, plane),
        }
    }
}

#[derive(Copy, Clone)]
pub enum RenderTargetHandle {
    None,
    Swapchain(native::CpuDescriptor),
    Pool(Handle),
}

impl RenderTargetHandle {
    pub fn raw(&self) -> Option<native::CpuDescriptor> {
        match *self {
            RenderTargetHandle::None => None,
            RenderTargetHandle::Swapchain(rtv) => Some(rtv),
            RenderTargetHandle::Pool(ref handle) => Some(handle.raw),
        }
    }
}

#[derive(Copy, Clone)]
pub struct ImageView {
    pub(crate) resource: native::Resource, // weak-ptr owned by image.
    pub(crate) handle_srv: Option<Handle>,
    pub(crate) handle_rtv: RenderTargetHandle,
    pub(crate) handle_dsv: Option<Handle>,
    pub(crate) handle_uav: Option<Handle>,
    // Required for attachment resolves.
    pub(crate) dxgi_format: DXGI_FORMAT,
    pub(crate) num_levels: image::Level,
    pub(crate) mip_levels: (image::Level, image::Level),
    pub(crate) layers: (image::Layer, image::Layer),
    pub(crate) kind: image::Kind,
}

impl fmt::Debug for ImageView {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("ImageView")
    }
}

unsafe impl Send for ImageView {}
unsafe impl Sync for ImageView {}

impl ImageView {
    pub fn calc_subresource(&self, mip_level: UINT, layer: UINT) -> UINT {
        mip_level + (layer * self.num_levels as UINT)
    }

    pub fn is_swapchain(&self) -> bool {
        match self.handle_rtv {
            RenderTargetHandle::Swapchain(_) => true,
            _ => false,
        }
    }
}

pub struct Sampler {
    pub(crate) handle: Handle,
}

impl fmt::Debug for Sampler {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Sampler")
    }
}

#[derive(Debug)]
pub struct DescriptorSetLayout {
    pub(crate) bindings: Vec<pso::DescriptorSetLayoutBinding>,
}

#[derive(Debug)]
pub struct Fence {
    pub(crate) raw: native::Fence,
}
unsafe impl Send for Fence {}
unsafe impl Sync for Fence {}

#[derive(Debug)]
pub struct Semaphore {
    pub(crate) raw: native::Fence,
}

unsafe impl Send for Semaphore {}
unsafe impl Sync for Semaphore {}

#[derive(Debug)]
pub struct Memory {
    pub(crate) heap: native::Heap,
    pub(crate) type_id: usize,
    pub(crate) size: u64,
    // Buffer containing the whole memory for mapping (only for host visible heaps)
    pub(crate) resource: Option<native::Resource>,
}

unsafe impl Send for Memory {}
unsafe impl Sync for Memory {}

bitflags! {
    /// A set of D3D12 descriptor types that need to be associated
    /// with a single gfx-hal `DescriptorType`.
    #[derive(Default)]
    pub struct DescriptorContent: u8 {
        const CBV = 0x1;
        const SRV = 0x2;
        const UAV = 0x4;
        const SAMPLER = 0x8;

        /// Indicates if the descriptor is a dynamic uniform/storage buffer.
        /// Important as dynamic buffers are implemented as root descriptors.
        const DYNAMIC = 0x10;

        const VIEW = DescriptorContent::CBV.bits |DescriptorContent::SRV.bits | DescriptorContent::UAV.bits;
    }
}

impl DescriptorContent {
    pub fn is_dynamic(&self) -> bool {
        self.contains(DescriptorContent::DYNAMIC)
    }
}

impl From<pso::DescriptorType> for DescriptorContent {
    fn from(ty: pso::DescriptorType) -> Self {
        use hal::pso::{
            BufferDescriptorFormat as Bdf, BufferDescriptorType as Bdt, DescriptorType as Dt,
            ImageDescriptorType as Idt,
        };

        use DescriptorContent as Dc;

        match ty {
            Dt::Sampler => Dc::SAMPLER,
            Dt::Image { ty } => match ty {
                Idt::Storage { read_only: true } => Dc::SRV,
                Idt::Storage { read_only: false } => Dc::SRV | Dc::UAV,
                Idt::Sampled { with_sampler } => match with_sampler {
                    true => Dc::SRV | Dc::SAMPLER,
                    false => Dc::SRV,
                },
            },
            Dt::Buffer { ty, format } => match ty {
                Bdt::Storage { read_only: true } => match format {
                    Bdf::Structured {
                        dynamic_offset: true,
                    } => Dc::SRV | Dc::DYNAMIC,
                    Bdf::Structured {
                        dynamic_offset: false,
                    }
                    | Bdf::Texel => Dc::SRV,
                },
                Bdt::Storage { read_only: false } => match format {
                    Bdf::Structured {
                        dynamic_offset: true,
                    } => Dc::SRV | Dc::UAV | Dc::DYNAMIC,
                    Bdf::Structured {
                        dynamic_offset: false,
                    }
                    | Bdf::Texel => Dc::SRV | Dc::UAV,
                },
                Bdt::Uniform => match format {
                    Bdf::Structured {
                        dynamic_offset: true,
                    } => Dc::CBV | Dc::DYNAMIC,
                    Bdf::Structured {
                        dynamic_offset: false,
                    } => Dc::CBV,
                    Bdf::Texel => Dc::SRV,
                },
            },
            Dt::InputAttachment => Dc::SRV,
        }
    }
}

#[derive(Debug)]
pub struct DescriptorRange {
    pub(crate) handle: DualHandle,
    pub(crate) ty: pso::DescriptorType,
    pub(crate) handle_size: u64,
}

impl DescriptorRange {
    pub(crate) fn at(&self, index: DescriptorIndex) -> native::CpuDescriptor {
        assert!(index < self.handle.size);
        let ptr = self.handle.cpu.ptr + (self.handle_size * index) as usize;
        native::CpuDescriptor { ptr }
    }
}

#[derive(Copy, Clone, Debug)]
pub(crate) struct DynamicDescriptor {
    pub content: DescriptorContent,
    pub gpu_buffer_location: u64,
}

#[derive(Debug, Default)]
pub struct DescriptorBindingInfo {
    pub(crate) count: u64,
    pub(crate) view_range: Option<DescriptorRange>,
    pub(crate) dynamic_descriptors: Vec<DynamicDescriptor>,
    pub(crate) content: DescriptorContent,
}

#[derive(Default)]
pub struct DescriptorOrigins {
    // For each index on the heap, this array stores the origin CPU handle.
    origins: Vec<native::CpuDescriptor>,
}

impl DescriptorOrigins {
    fn find(&self, other: &[native::CpuDescriptor]) -> Option<DescriptorIndex> {
        //TODO: need a smarter algorithm here!
        for i in other.len()..=self.origins.len() {
            let base = i - other.len();
            //TODO: use slice comparison when `CpuDescriptor` implements `PartialEq`.
            if unsafe {
                slice::from_raw_parts(&self.origins[base].ptr, other.len())
                    == slice::from_raw_parts(&other[0].ptr, other.len())
            } {
                return Some(base as DescriptorIndex);
            }
        }
        None
    }

    fn grow(&mut self, other: &[native::CpuDescriptor]) -> DescriptorIndex {
        let base = self.origins.len() as DescriptorIndex;
        self.origins.extend_from_slice(other);
        base
    }
}

pub struct DescriptorSet {
    // Required for binding at command buffer
    pub(crate) heap_srv_cbv_uav: native::DescriptorHeap,
    pub(crate) heap_samplers: native::DescriptorHeap,
    pub(crate) sampler_origins: Box<[native::CpuDescriptor]>,
    pub(crate) binding_infos: Vec<DescriptorBindingInfo>,
    pub(crate) first_gpu_sampler: Option<native::GpuDescriptor>,
    pub(crate) first_gpu_view: Option<native::GpuDescriptor>,
    pub(crate) raw_name: Vec<u16>,
}

impl fmt::Debug for DescriptorSet {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("DescriptorSet")
    }
}

// TODO: is this really safe?
unsafe impl Send for DescriptorSet {}
unsafe impl Sync for DescriptorSet {}

impl DescriptorSet {
    pub fn srv_cbv_uav_gpu_start(&self) -> native::GpuDescriptor {
        self.heap_srv_cbv_uav.start_gpu_descriptor()
    }

    pub fn sampler_gpu_start(&self) -> native::GpuDescriptor {
        self.heap_samplers.start_gpu_descriptor()
    }

    pub fn sampler_offset(&self, binding: u32, last_offset: usize) -> usize {
        let mut offset = 0;
        for bi in &self.binding_infos[..binding as usize] {
            if bi.content.contains(DescriptorContent::SAMPLER) {
                offset += bi.count as usize;
            }
        }
        if self.binding_infos[binding as usize]
            .content
            .contains(DescriptorContent::SAMPLER)
        {
            offset += last_offset;
        }
        offset
    }

    pub fn update_samplers(
        &mut self,
        heap: &DescriptorHeap,
        origins: &RwLock<DescriptorOrigins>,
        accum: &mut MultiCopyAccumulator,
    ) {
        let start_index = if let Some(index) = {
            // explicit variable allows to limit the lifetime of that borrow
            let borrow = origins.read();
            borrow.find(&self.sampler_origins)
        } {
            Some(index)
        } else if self.sampler_origins.iter().any(|desc| desc.ptr == 0) {
            // set is incomplete, don't try to build it
            None
        } else {
            let base = origins.write().grow(&self.sampler_origins);
            // copy the descriptors from their origins into the new location
            accum.dst_samplers.add(
                heap.cpu_descriptor_at(base),
                self.sampler_origins.len() as u32,
            );
            for &origin in self.sampler_origins.iter() {
                accum.src_samplers.add(origin, 1);
            }
            Some(base)
        };

        self.first_gpu_sampler = start_index.map(|index| heap.gpu_descriptor_at(index));
    }
}

#[derive(Copy, Clone)]
pub struct DualHandle {
    pub(crate) cpu: native::CpuDescriptor,
    pub(crate) gpu: native::GpuDescriptor,
    /// How large the block allocated to this handle is.
    pub(crate) size: u64,
}

impl fmt::Debug for DualHandle {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("DualHandle")
    }
}

pub struct DescriptorHeap {
    pub(crate) raw: native::DescriptorHeap,
    pub(crate) handle_size: u64,
    pub(crate) total_handles: u64,
    pub(crate) start: DualHandle,
}

impl fmt::Debug for DescriptorHeap {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("DescriptorHeap")
    }
}

impl DescriptorHeap {
    pub(crate) fn at(&self, index: DescriptorIndex, size: u64) -> DualHandle {
        assert!(index < self.total_handles);
        DualHandle {
            cpu: self.cpu_descriptor_at(index),
            gpu: self.gpu_descriptor_at(index),
            size,
        }
    }

    pub(crate) fn cpu_descriptor_at(&self, index: u64) -> native::CpuDescriptor {
        native::CpuDescriptor {
            ptr: self.start.cpu.ptr + (self.handle_size * index) as usize,
        }
    }

    pub(crate) fn gpu_descriptor_at(&self, index: u64) -> native::GpuDescriptor {
        native::GpuDescriptor {
            ptr: self.start.gpu.ptr + self.handle_size * index,
        }
    }

    pub(crate) unsafe fn destroy(&self) {
        self.raw.destroy();
    }
}

/// Slice of an descriptor heap, which is allocated for a pool.
/// Pools will create descriptor sets inside this slice.
#[derive(Debug)]
pub struct DescriptorHeapSlice {
    pub(crate) heap: native::DescriptorHeap, // Weak reference, owned by descriptor heap.
    pub(crate) start: DualHandle,
    pub(crate) handle_size: u64,
    pub(crate) range_allocator: RangeAllocator<u64>,
}

impl DescriptorHeapSlice {
    pub(crate) fn alloc_handles(&mut self, count: u64) -> Option<DualHandle> {
        self.range_allocator
            .allocate_range(count)
            .ok()
            .map(|range| DualHandle {
                cpu: native::CpuDescriptor {
                    ptr: self.start.cpu.ptr + (self.handle_size * range.start) as usize,
                },
                gpu: native::GpuDescriptor {
                    ptr: self.start.gpu.ptr + (self.handle_size * range.start) as u64,
                },
                size: count,
            })
    }

    /// Free handles previously given out by this `DescriptorHeapSlice`.
    /// Do not use this with handles not given out by this `DescriptorHeapSlice`.
    pub(crate) fn free_handles(&mut self, handle: DualHandle) {
        let start = (handle.gpu.ptr - self.start.gpu.ptr) / self.handle_size;
        let handle_range = start..start + handle.size;
        self.range_allocator.free_range(handle_range);
    }

    /// Clear the allocator.
    pub(crate) fn clear(&mut self) {
        self.range_allocator.reset();
    }
}

#[derive(Debug)]
pub struct DescriptorPool {
    pub(crate) heap_raw_sampler: native::DescriptorHeap,
    pub(crate) heap_srv_cbv_uav: DescriptorHeapSlice,
    pub(crate) pools: Vec<pso::DescriptorRangeDesc>,
    pub(crate) max_size: u64,
}
unsafe impl Send for DescriptorPool {}
unsafe impl Sync for DescriptorPool {}

impl pso::DescriptorPool<Backend> for DescriptorPool {
    unsafe fn allocate_one(
        &mut self,
        layout: &DescriptorSetLayout,
    ) -> Result<DescriptorSet, pso::AllocationError> {
        let mut binding_infos = Vec::new();
        let mut first_gpu_view = None;
        let mut num_samplers = 0;

        info!("allocate_one");
        for binding in &layout.bindings {
            // Add dummy bindings in case of out-of-range or sparse binding layout.
            while binding_infos.len() <= binding.binding as usize {
                binding_infos.push(DescriptorBindingInfo::default());
            }
            let content = DescriptorContent::from(binding.ty);
            debug!("\tbinding {:?} with content {:?}", binding, content);

            let (view_range, dynamic_descriptors) = if content.is_dynamic() {
                let descriptor = DynamicDescriptor {
                    content: content ^ DescriptorContent::DYNAMIC,
                    gpu_buffer_location: 0,
                };
                (None, vec![descriptor; binding.count])
            } else {
                if content.contains(DescriptorContent::SAMPLER) {
                    num_samplers += binding.count;
                }

                let view_range = if content.intersects(DescriptorContent::VIEW) {
                    let count = if content.contains(DescriptorContent::SRV | DescriptorContent::UAV)
                    {
                        2 * binding.count as u64
                    } else {
                        binding.count as u64
                    };
                    debug!("\tview handles: {}", count);
                    let handle = self
                        .heap_srv_cbv_uav
                        .alloc_handles(count)
                        .ok_or(pso::AllocationError::OutOfPoolMemory)?;
                    if first_gpu_view.is_none() {
                        first_gpu_view = Some(handle.gpu);
                    }
                    Some(DescriptorRange {
                        handle,
                        ty: binding.ty,
                        handle_size: self.heap_srv_cbv_uav.handle_size,
                    })
                } else {
                    None
                };

                (view_range, Vec::new())
            };

            binding_infos[binding.binding as usize] = DescriptorBindingInfo {
                count: binding.count as _,
                view_range,
                dynamic_descriptors,
                content,
            };
        }

        Ok(DescriptorSet {
            heap_srv_cbv_uav: self.heap_srv_cbv_uav.heap,
            heap_samplers: self.heap_raw_sampler,
            sampler_origins: vec![native::CpuDescriptor { ptr: 0 }; num_samplers]
                .into_boxed_slice(),
            binding_infos,
            first_gpu_sampler: None,
            first_gpu_view,
            raw_name: Vec::new(),
        })
    }

    unsafe fn free<I>(&mut self, descriptor_sets: I)
    where
        I: Iterator<Item = DescriptorSet>,
    {
        for descriptor_set in descriptor_sets {
            for binding_info in descriptor_set.binding_infos {
                if let Some(view_range) = binding_info.view_range {
                    if binding_info.content.intersects(DescriptorContent::VIEW) {
                        self.heap_srv_cbv_uav.free_handles(view_range.handle);
                    }
                }
            }
        }
    }

    unsafe fn reset(&mut self) {
        self.heap_srv_cbv_uav.clear();
    }
}

#[derive(Debug)]
pub struct QueryPool {
    pub(crate) raw: native::QueryHeap,
    pub(crate) ty: hal::query::Type,
}

unsafe impl Send for QueryPool {}
unsafe impl Sync for QueryPool {}
