use winapi::{
    shared::{dxgiformat::DXGI_FORMAT, minwindef::UINT},
    um::d3d12,
};

use hal::{buffer, format, image, memory, pass, pso};
use range_alloc::RangeAllocator;

use crate::{root_constants::RootConstant, Backend, MAX_VERTEX_BUFFERS};

use std::{cell::UnsafeCell, collections::BTreeMap, fmt, ops::Range};

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
        } .. BarrierDesc {
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
    pub(crate) signature: native::RootSignature, // weak-ptr, owned by `PipelineLayout`
    pub(crate) num_parameter_slots: usize,       // signature parameter slots, see `PipelineLayout`
    pub(crate) topology: d3d12::D3D12_PRIMITIVE_TOPOLOGY,
    pub(crate) constants: Vec<RootConstant>,
    pub(crate) vertex_bindings: [Option<VertexBinding>; MAX_VERTEX_BUFFERS],
    pub(crate) baked_states: pso::BakedStates,
}
unsafe impl Send for GraphicsPipeline {}
unsafe impl Sync for GraphicsPipeline {}

#[derive(Debug)]
pub struct ComputePipeline {
    pub(crate) raw: native::PipelineState,
    pub(crate) signature: native::RootSignature, // weak-ptr, owned by `PipelineLayout`
    pub(crate) num_parameter_slots: usize,       // signature parameter slots, see `PipelineLayout`
    pub(crate) constants: Vec<RootConstant>,
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

#[derive(Debug, Hash)]
pub struct RootDescriptor {
    pub offset: RootSignatureOffset,
}

#[derive(Debug)]
pub struct RootElement {
    pub table: RootTable,
    pub descriptors: Vec<RootDescriptor>,
    pub mutable_bindings: auxil::FastHashSet<pso::DescriptorBinding>,
}

#[derive(Debug)]
pub struct PipelineLayout {
    pub(crate) raw: native::RootSignature,
    // Disjunct, sorted vector of root constant ranges.
    pub(crate) constants: Vec<RootConstant>,
    // Storing for each associated descriptor set layout, which tables we created
    // in the root signature. This is required for binding descriptor sets.
    pub(crate) elements: Vec<RootElement>,
    // Number of parameter slots in this layout, can be larger than number of tables.
    // Required for updating the root signature when flushing user data.
    pub(crate) num_parameter_slots: usize,
}
unsafe impl Send for PipelineLayout {}
unsafe impl Sync for PipelineLayout {}

#[derive(Debug, Clone)]
pub struct Framebuffer {
    pub(crate) attachments: Vec<ImageView>,
    // Number of layers in the render area. Required for subpass resolves.
    pub(crate) layers: image::Layer,
}

#[derive(Copy, Clone, Debug)]
pub struct BufferUnbound {
    pub(crate) requirements: memory::Requirements,
    pub(crate) usage: buffer::Usage,
}

pub struct BufferBound {
    pub(crate) resource: native::Resource,
    pub(crate) requirements: memory::Requirements,
    pub(crate) clear_uav: Option<native::CpuDescriptor>,
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
    pub(crate) handle_srv: native::CpuDescriptor,
    // Descriptor handle for storage texel buffers.
    pub(crate) handle_uav: native::CpuDescriptor,
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
    SwapChain,
    Heap { raw: native::Heap, offset: u64 },
}

#[derive(Clone)]
pub struct ImageBound {
    pub(crate) resource: native::Resource,
    pub(crate) place: Place,
    pub(crate) surface_type: format::SurfaceType,
    pub(crate) kind: image::Kind,
    pub(crate) usage: image::Usage,
    pub(crate) default_view_format: Option<DXGI_FORMAT>,
    pub(crate) view_caps: image::ViewCapabilities,
    pub(crate) descriptor: d3d12::D3D12_RESOURCE_DESC,
    pub(crate) bytes_per_block: u8,
    // Dimension of a texel block (compressed formats).
    pub(crate) block_dim: (u8, u8),
    pub(crate) clear_cv: Vec<native::CpuDescriptor>,
    pub(crate) clear_dv: Vec<native::CpuDescriptor>,
    pub(crate) clear_sv: Vec<native::CpuDescriptor>,
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
    /// Get `SubresourceRange` of the whole image.
    pub fn to_subresource_range(&self, aspects: format::Aspects) -> image::SubresourceRange {
        image::SubresourceRange {
            aspects,
            levels: 0 .. self.descriptor.MipLevels as _,
            layers: 0 .. self.kind.num_layers(),
        }
    }

    pub fn calc_subresource(&self, mip_level: UINT, layer: UINT, plane: UINT) -> UINT {
        mip_level
            + (layer * self.descriptor.MipLevels as UINT)
            + (plane * self.descriptor.MipLevels as UINT * self.kind.num_layers() as UINT)
    }
}

#[derive(Copy, Clone)]
pub struct ImageUnbound {
    pub(crate) desc: d3d12::D3D12_RESOURCE_DESC,
    pub(crate) view_format: Option<DXGI_FORMAT>,
    pub(crate) dsv_format: Option<DXGI_FORMAT>,
    pub(crate) requirements: memory::Requirements,
    pub(crate) format: format::Format,
    pub(crate) kind: image::Kind,
    pub(crate) usage: image::Usage,
    pub(crate) tiling: image::Tiling,
    pub(crate) view_caps: image::ViewCapabilities,
    //TODO: use hal::format::FormatDesc
    pub(crate) bytes_per_block: u8,
    // Dimension of a texel block (compressed formats).
    pub(crate) block_dim: (u8, u8),
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
pub struct ImageView {
    pub(crate) resource: native::Resource, // weak-ptr owned by image.
    pub(crate) handle_srv: Option<native::CpuDescriptor>,
    pub(crate) handle_rtv: Option<native::CpuDescriptor>,
    pub(crate) handle_dsv: Option<native::CpuDescriptor>,
    pub(crate) handle_uav: Option<native::CpuDescriptor>,
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
}

pub struct Sampler {
    pub(crate) handle: native::CpuDescriptor,
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
            BufferDescriptorFormat as Bdf,
            BufferDescriptorType as Bdt,
            DescriptorType as Dt,
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
    pub(crate) count: u64,
}

impl DescriptorRange {
    pub(crate) fn at(&self, index: u64) -> native::CpuDescriptor {
        assert!(index < self.count);
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
    pub(crate) sampler_range: Option<DescriptorRange>,
    pub(crate) dynamic_descriptors: UnsafeCell<Vec<DynamicDescriptor>>,
    pub(crate) content: DescriptorContent,
}

pub struct DescriptorSet {
    // Required for binding at command buffer
    pub(crate) heap_srv_cbv_uav: native::DescriptorHeap,
    pub(crate) heap_samplers: native::DescriptorHeap,
    pub(crate) binding_infos: Vec<DescriptorBindingInfo>,
    pub(crate) first_gpu_sampler: Option<native::GpuDescriptor>,
    pub(crate) first_gpu_view: Option<native::GpuDescriptor>,
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
    pub(crate) range_allocator: RangeAllocator<u64>,
}

impl fmt::Debug for DescriptorHeap {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("DescriptorHeap")
    }
}

impl DescriptorHeap {
    pub(crate) fn at(&self, index: u64, size: u64) -> DualHandle {
        assert!(index < self.total_handles);
        DualHandle {
            cpu: native::CpuDescriptor {
                ptr: self.start.cpu.ptr + (self.handle_size * index) as usize,
            },
            gpu: native::GpuDescriptor {
                ptr: self.start.gpu.ptr + self.handle_size * index,
            },
            size,
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

    /// Free handles previously given out by this `DescriptorHeapSlice`.  Do not use this with handles not given out by this `DescriptorHeapSlice`.
    pub(crate) fn free_handles(&mut self, handle: DualHandle) {
        let start = (handle.gpu.ptr - self.start.gpu.ptr) / self.handle_size;
        let handle_range = start .. start + handle.size as u64;
        self.range_allocator.free_range(handle_range);
    }

    pub(crate) fn clear(&mut self) {
        self.range_allocator.reset();
    }
}

#[derive(Debug)]
pub struct DescriptorPool {
    pub(crate) heap_srv_cbv_uav: DescriptorHeapSlice,
    pub(crate) heap_sampler: DescriptorHeapSlice,
    pub(crate) pools: Vec<pso::DescriptorRangeDesc>,
    pub(crate) max_size: u64,
}
unsafe impl Send for DescriptorPool {}
unsafe impl Sync for DescriptorPool {}

impl pso::DescriptorPool<Backend> for DescriptorPool {
    unsafe fn allocate_set(
        &mut self,
        layout: &DescriptorSetLayout,
    ) -> Result<DescriptorSet, pso::AllocationError> {
        let mut binding_infos = Vec::new();
        let mut first_gpu_sampler = None;
        let mut first_gpu_view = None;

        info!("allocate_set");
        for binding in &layout.bindings {
            // Add dummy bindings in case of out-of-range or sparse binding layout.
            while binding_infos.len() <= binding.binding as usize {
                binding_infos.push(DescriptorBindingInfo::default());
            }
            let content = DescriptorContent::from(binding.ty);
            debug!("\tbinding {:?} with content {:?}", binding, content);

            let (view_range, sampler_range, dynamic_descriptors) = if content.is_dynamic() {
                let descriptor = DynamicDescriptor {
                    content: content ^ DescriptorContent::DYNAMIC,
                    gpu_buffer_location: 0,
                };
                (None, None, vec![descriptor; binding.count])
            } else {
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
                        count,
                        handle_size: self.heap_srv_cbv_uav.handle_size,
                    })
                } else {
                    None
                };

                let sampler_range =
                    if content.intersects(DescriptorContent::SAMPLER) && !content.is_dynamic() {
                        let count = binding.count as u64;
                        debug!("\tsampler handles: {}", count);
                        let handle = self
                            .heap_sampler
                            .alloc_handles(count)
                            .ok_or(pso::AllocationError::OutOfPoolMemory)?;
                        if first_gpu_sampler.is_none() {
                            first_gpu_sampler = Some(handle.gpu);
                        }
                        Some(DescriptorRange {
                            handle,
                            ty: binding.ty,
                            count,
                            handle_size: self.heap_sampler.handle_size,
                        })
                    } else {
                        None
                    };

                (view_range, sampler_range, Vec::new())
            };

            binding_infos[binding.binding as usize] = DescriptorBindingInfo {
                count: binding.count as _,
                view_range,
                sampler_range,
                dynamic_descriptors: UnsafeCell::new(dynamic_descriptors),
                content,
            };
        }

        Ok(DescriptorSet {
            heap_srv_cbv_uav: self.heap_srv_cbv_uav.heap.clone(),
            heap_samplers: self.heap_sampler.heap.clone(),
            binding_infos,
            first_gpu_sampler,
            first_gpu_view,
        })
    }

    unsafe fn free_sets<I>(&mut self, descriptor_sets: I)
    where
        I: IntoIterator<Item = DescriptorSet>,
    {
        for descriptor_set in descriptor_sets {
            for binding_info in &descriptor_set.binding_infos {
                if let Some(ref view_range) = binding_info.view_range {
                    if binding_info.content.intersects(DescriptorContent::VIEW) {
                        self.heap_srv_cbv_uav.free_handles(view_range.handle);
                    }
                }
                if let Some(ref sampler_range) = binding_info.sampler_range {
                    if binding_info.content.intersects(DescriptorContent::SAMPLER) {
                        self.heap_sampler.free_handles(sampler_range.handle);
                    }
                }
            }
        }
    }

    unsafe fn reset(&mut self) {
        self.heap_srv_cbv_uav.clear();
        self.heap_sampler.clear();
    }
}

#[derive(Debug)]
pub struct QueryPool {
    pub(crate) raw: native::QueryHeap,
    pub(crate) ty: native::QueryHeapType,
}

unsafe impl Send for QueryPool {}
unsafe impl Sync for QueryPool {}
