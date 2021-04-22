use auxil::FastHashMap;
use hal::{
    buffer, command as com, format, format::Aspects, image, memory, pass, pso, query, DrawCount,
    IndexCount, IndexType, InstanceCount, TaskCount, VertexCount, VertexOffset, WorkGroupCount,
};

use arrayvec::ArrayVec;
use smallvec::SmallVec;
use winapi::{
    ctypes,
    shared::{dxgiformat, minwindef, winerror},
    um::{d3d12, d3dcommon},
    Interface,
};

use std::{cmp, fmt, iter, mem, ops::Range, ptr, sync::Arc};

use crate::{
    conv, descriptors_cpu, device, internal,
    pool::{CommandAllocatorIndex, PoolShared},
    resource as r, validate_line_width, Backend, Device, Shared, MAX_DESCRIPTOR_SETS,
    MAX_VERTEX_BUFFERS,
};

// Fixed size of the root signature.
// Limited by D3D12.
const ROOT_SIGNATURE_SIZE: usize = 64;

const NULL_VERTEX_BUFFER_VIEW: d3d12::D3D12_VERTEX_BUFFER_VIEW = d3d12::D3D12_VERTEX_BUFFER_VIEW {
    BufferLocation: 0,
    SizeInBytes: 0,
    StrideInBytes: 0,
};

fn get_rect(rect: &pso::Rect) -> d3d12::D3D12_RECT {
    d3d12::D3D12_RECT {
        left: rect.x as i32,
        top: rect.y as i32,
        right: (rect.x + rect.w) as i32,
        bottom: (rect.y + rect.h) as i32,
    }
}

fn div(a: u32, b: u32) -> u32 {
    (a + b - 1) / b
}

fn up_align(x: u32, alignment: u32) -> u32 {
    (x + alignment - 1) & !(alignment - 1)
}

#[derive(Clone, Debug)]
struct AttachmentInfo {
    subpass_id: Option<pass::SubpassId>,
    view: r::ImageView,
    clear_value: Option<com::ClearValue>,
    stencil_value: Option<u32>,
}

pub struct RenderPassCache {
    render_pass: r::RenderPass,
    target_rect: d3d12::D3D12_RECT,
    attachments: Vec<AttachmentInfo>,
    num_layers: image::Layer,
    has_name: bool,
}

impl fmt::Debug for RenderPassCache {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("RenderPassCache")
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum OcclusionQuery {
    Binary(minwindef::UINT),
    Precise(minwindef::UINT),
}

/// Strongly-typed root signature element
///
/// Could be removed for an unsafer variant to occupy less memory
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum RootElement {
    /// Root constant in the signature
    Constant(u32),
    /// Descriptor table, storing table offset for the current descriptor heap
    TableSrvCbvUav(u32),
    /// Descriptor table, storing table offset for the current descriptor heap
    TableSampler(u32),
    /// Root descriptors take 2 DWORDs in the root signature.
    DescriptorCbv {
        buffer: u64,
    },
    DescriptorPlaceholder,
    /// Undefined value, implementation specific
    Undefined,
}

/// Virtual data storage for the current root signature memory.
struct UserData {
    data: [RootElement; ROOT_SIGNATURE_SIZE],
    dirty_mask: u64,
}

impl Default for UserData {
    fn default() -> Self {
        UserData {
            data: [RootElement::Undefined; ROOT_SIGNATURE_SIZE],
            dirty_mask: 0,
        }
    }
}

impl fmt::Debug for UserData {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt.debug_struct("UserData")
            .field("data", &&self.data[..])
            .field("dirty_mask", &self.dirty_mask)
            .finish()
    }
}

impl UserData {
    /// Write root constant values into the user data, overwriting virtual memory
    /// range [offset..offset + data.len()]. Changes are marked as dirty.
    fn set_constants(&mut self, offset: usize, data: &[u32]) {
        assert!(offset + data.len() <= ROOT_SIGNATURE_SIZE);
        // Each root constant occupies one DWORD
        for (i, val) in data.iter().enumerate() {
            self.data[offset + i] = RootElement::Constant(*val);
            self.dirty_mask |= 1u64 << (offset + i);
        }
    }

    /// Write a SRV/CBV/UAV descriptor table into the user data, overwriting virtual
    /// memory range [offset..offset + 1]. Changes are marked as dirty.
    fn set_srv_cbv_uav_table(&mut self, offset: usize, table_start: u32) {
        assert!(offset < ROOT_SIGNATURE_SIZE);
        // A descriptor table occupies one DWORD
        self.data[offset] = RootElement::TableSrvCbvUav(table_start);
        self.dirty_mask |= 1u64 << offset;
    }

    /// Write a sampler descriptor table into the user data, overwriting virtual
    /// memory range [offset..offset + 1]. Changes are marked as dirty.
    fn set_sampler_table(&mut self, offset: usize, table_start: u32) {
        assert!(offset < ROOT_SIGNATURE_SIZE);
        // A descriptor table occupies one DWORD
        self.data[offset] = RootElement::TableSampler(table_start);
        self.dirty_mask |= 1u64 << offset;
    }

    /// Write a CBV root descriptor into the user data, overwriting virtual
    /// memory range [offset..offset + 2]. Changes are marked as dirty.
    fn set_descriptor_cbv(&mut self, offset: usize, buffer: u64) {
        assert!(offset + 1 < ROOT_SIGNATURE_SIZE);
        // A root descriptor occupies two DWORDs
        self.data[offset] = RootElement::DescriptorCbv { buffer };
        self.data[offset + 1] = RootElement::DescriptorPlaceholder;
        self.dirty_mask |= 1u64 << offset;
        self.dirty_mask |= 1u64 << offset + 1;
    }

    fn is_dirty(&self) -> bool {
        self.dirty_mask != 0
    }

    fn is_index_dirty(&self, i: u32) -> bool {
        ((self.dirty_mask >> i) & 1) == 1
    }

    /// Mark all entries as dirty.
    fn dirty_all(&mut self) {
        self.dirty_mask = !0;
    }

    /// Mark all entries as clear up to the given i.
    fn clear_up_to(&mut self, i: u32) {
        self.dirty_mask &= !((1 << i) - 1);
    }
}

#[derive(Debug, Default)]
struct PipelineCache {
    // Bound pipeline and layout info.
    // Changed on bind pipeline calls.
    pipeline: Option<(native::PipelineState, Arc<r::PipelineShared>)>,

    // Virtualized root signature user data of the shaders
    user_data: UserData,

    temp_constants: Vec<u32>,

    // Descriptor heap gpu handle offsets
    srv_cbv_uav_start: u64,
    sampler_start: u64,
}

impl PipelineCache {
    fn bind_descriptor_sets<'a, J>(
        &mut self,
        layout: &r::PipelineLayout,
        first_set: usize,
        sets: &[&r::DescriptorSet],
        offsets: J,
    ) -> [native::DescriptorHeap; 2]
    where
        J: Iterator<Item = com::DescriptorSetOffset>,
    {
        let mut offsets = offsets.map(|offset| offset as u64);

        // 'Global' GPU descriptor heaps.
        // All descriptors live in the same heaps.
        let (srv_cbv_uav_start, sampler_start, heap_srv_cbv_uav, heap_sampler) =
            if let Some(set) = sets.first() {
                (
                    set.srv_cbv_uav_gpu_start().ptr,
                    set.sampler_gpu_start().ptr,
                    set.heap_srv_cbv_uav,
                    set.heap_samplers,
                )
            } else {
                return [native::DescriptorHeap::null(); 2];
            };

        self.srv_cbv_uav_start = srv_cbv_uav_start;
        self.sampler_start = sampler_start;

        for (&set, element) in sets.iter().zip(layout.elements[first_set..].iter()) {
            let mut root_offset = element.table.offset;

            // Bind CBV/SRC/UAV descriptor tables
            if let Some(gpu) = set.first_gpu_view {
                assert!(element.table.ty.contains(r::SRV_CBV_UAV));
                // Cast is safe as offset **must** be in u32 range. Unable to
                // create heaps with more descriptors.
                let table_gpu_offset = (gpu.ptr - srv_cbv_uav_start) as u32;
                self.user_data
                    .set_srv_cbv_uav_table(root_offset, table_gpu_offset);
                root_offset += 1;
            }

            // Bind Sampler descriptor tables.
            if let Some(gpu) = set.first_gpu_sampler {
                assert!(element.table.ty.contains(r::SAMPLERS));

                // Cast is safe as offset **must** be in u32 range. Unable to
                // create heaps with more descriptors.
                let table_gpu_offset = (gpu.ptr - sampler_start) as u32;
                self.user_data
                    .set_sampler_table(root_offset, table_gpu_offset);
                root_offset += 1;
            }

            // Bind root descriptors
            // TODO: slow, can we move the dynamic descriptors into toplevel somehow during initialization?
            //       Requires changes then in the descriptor update process.
            for binding in &set.binding_infos {
                // It's not valid to modify the descriptor sets during recording -> access if safe.
                for descriptor in binding.dynamic_descriptors.iter() {
                    let gpu_offset = descriptor.gpu_buffer_location + offsets.next().unwrap();
                    self.user_data.set_descriptor_cbv(root_offset, gpu_offset);
                    root_offset += 2;
                }
            }
        }

        [heap_srv_cbv_uav, heap_sampler]
    }

    fn flush_user_data<F, G, H>(
        &mut self,
        mut constants_update: F,
        mut table_update: G,
        mut descriptor_cbv_update: H,
    ) where
        F: FnMut(u32, &[u32]),
        G: FnMut(u32, d3d12::D3D12_GPU_DESCRIPTOR_HANDLE),
        H: FnMut(u32, d3d12::D3D12_GPU_VIRTUAL_ADDRESS),
    {
        let user_data = &mut self.user_data;
        if !user_data.is_dirty() {
            return;
        }

        let shared = match self.pipeline {
            Some((_, ref shared)) => shared,
            None => return,
        };

        for (root_index, &root_offset) in shared.parameter_offsets.iter().enumerate() {
            if !user_data.is_index_dirty(root_offset) {
                continue;
            }
            match user_data.data[root_offset as usize] {
                RootElement::Constant(_) => {
                    let c = &shared.constants[root_index];
                    debug_assert_eq!(root_offset, c.range.start);
                    self.temp_constants.clear();
                    self.temp_constants.extend(
                        user_data.data[c.range.start as usize..c.range.end as usize]
                            .iter()
                            .map(|ud| match *ud {
                                RootElement::Constant(v) => v,
                                _ => {
                                    warn!(
                                        "Unset or mismatching root constant at index {:?} ({:?})",
                                        c, ud
                                    );
                                    0
                                }
                            }),
                    );
                    constants_update(root_index as u32, &self.temp_constants);
                }
                RootElement::TableSrvCbvUav(offset) => {
                    let gpu = d3d12::D3D12_GPU_DESCRIPTOR_HANDLE {
                        ptr: self.srv_cbv_uav_start + offset as u64,
                    };
                    table_update(root_index as u32, gpu);
                }
                RootElement::TableSampler(offset) => {
                    let gpu = d3d12::D3D12_GPU_DESCRIPTOR_HANDLE {
                        ptr: self.sampler_start + offset as u64,
                    };
                    table_update(root_index as u32, gpu);
                }
                RootElement::DescriptorCbv { buffer } => {
                    debug_assert!(user_data.is_index_dirty(root_offset + 1));
                    debug_assert_eq!(
                        user_data.data[root_offset as usize + 1],
                        RootElement::DescriptorPlaceholder
                    );

                    descriptor_cbv_update(root_index as u32, buffer);
                }
                RootElement::DescriptorPlaceholder | RootElement::Undefined => {
                    error!(
                        "Undefined user data element in the root signature at {}",
                        root_offset
                    );
                }
            }
        }

        user_data.clear_up_to(shared.total_slots);
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum BindPoint {
    Compute,
    Graphics {
        /// Internal pipelines used for blitting, copying, etc.
        internal: bool,
    },
}

#[derive(Clone, Debug)]
struct Copy {
    footprint_offset: u64,
    footprint: image::Extent,
    row_pitch: u32,
    img_subresource: u32,
    img_offset: image::Offset,
    buf_offset: image::Offset,
    copy_extent: image::Extent,
}

#[derive(Clone, Copy, Debug, PartialEq)]
enum Phase {
    Initial,
    Recording,
    Executable,
    // Pending, // not useful, and we don't have mutable access
    // to self in `submit()`, so we can't set it anyway.
}

pub struct CommandBuffer {
    //Note: this is going to be NULL instead of `Option` to avoid
    // `unwrap()` on every operation. This is not idiomatic.
    pub(crate) raw: native::GraphicsCommandList,
    //Note: this is NULL when the command buffer is reset, and no
    // allocation is used for the `raw` list.
    allocator_index: Option<CommandAllocatorIndex>,
    phase: Phase,
    shared: Arc<Shared>,
    pool_shared: Arc<PoolShared>,
    begin_flags: com::CommandBufferFlags,

    /// Cache renderpasses for graphics operations
    pass_cache: Option<RenderPassCache>,
    cur_subpass: pass::SubpassId,

    /// Cache current graphics root signature and pipeline to minimize rebinding and support two
    /// bindpoints.
    gr_pipeline: PipelineCache,
    /// Primitive topology of the currently bound graphics pipeline.
    /// Caching required for internal graphics pipelines.
    primitive_topology: d3d12::D3D12_PRIMITIVE_TOPOLOGY,
    /// Cache current compute root signature and pipeline.
    comp_pipeline: PipelineCache,
    /// D3D12 only has one slot for both bindpoints. Need to rebind everything if we want to switch
    /// between different bind points (ie. calling draw or dispatch).
    active_bindpoint: BindPoint,
    /// Current descriptor heaps heaps (CBV/SRV/UAV and Sampler).
    /// Required for resetting due to internal descriptor heaps.
    active_descriptor_heaps: [native::DescriptorHeap; 2],

    /// Active queries in the command buffer.
    /// Queries must begin and end in the same command buffer, which allows us to track them.
    /// The query pool type on `begin_query` must differ from all currently active queries.
    /// Therefore, only one query per query type can be active at the same time. Binary and precise
    /// occlusion queries share one queue type in Vulkan.
    occlusion_query: Option<OcclusionQuery>,
    pipeline_stats_query: Option<minwindef::UINT>,

    /// Cached vertex buffer views to bind.
    /// `Stride` values are not known at `bind_vertex_buffers` time because they are only stored
    /// inside the pipeline state.
    vertex_bindings_remap: [Option<r::VertexBinding>; MAX_VERTEX_BUFFERS],

    vertex_buffer_views: [d3d12::D3D12_VERTEX_BUFFER_VIEW; MAX_VERTEX_BUFFERS],

    /// Re-using allocation for the image-buffer copies.
    copies: Vec<Copy>,

    /// D3D12 only allows setting all viewports or all scissors at once, not partial updates.
    /// So we must cache the implied state for these partial updates.
    viewport_cache: ArrayVec<
        [d3d12::D3D12_VIEWPORT;
            d3d12::D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE as usize],
    >,
    scissor_cache: ArrayVec<
        [d3d12::D3D12_RECT;
            d3d12::D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE as usize],
    >,

    /// HACK: renderdoc workaround for temporary RTVs
    rtv_pools: Vec<native::DescriptorHeap>,
    /// Temporary gpu descriptor heaps (internal).
    temporary_gpu_heaps: Vec<native::DescriptorHeap>,
    /// Resources that need to be alive till the end of the GPU execution.
    retained_resources: Vec<native::Resource>,

    /// Temporary wide string for the marker.
    temp_marker: Vec<u16>,

    /// Temporary transition barriers.
    barriers: Vec<d3d12::D3D12_RESOURCE_BARRIER>,
    /// Name of the underlying raw `GraphicsCommandList` object.
    pub(crate) raw_name: Vec<u16>,
}

impl fmt::Debug for CommandBuffer {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("CommandBuffer")
    }
}

unsafe impl Send for CommandBuffer {}
unsafe impl Sync for CommandBuffer {}

// Insetion point for subpasses.
enum BarrierPoint {
    // Pre barriers are inserted of the beginning when switching into a new subpass.
    Pre,
    // Post barriers are applied after exectuing the user defined commands.
    Post,
}

impl CommandBuffer {
    pub(crate) fn new(shared: &Arc<Shared>, pool_shared: &Arc<PoolShared>) -> Self {
        CommandBuffer {
            raw: native::GraphicsCommandList::null(),
            allocator_index: None,
            shared: Arc::clone(shared),
            pool_shared: Arc::clone(pool_shared),
            phase: Phase::Initial,
            begin_flags: com::CommandBufferFlags::empty(),
            pass_cache: None,
            cur_subpass: !0,
            gr_pipeline: PipelineCache::default(),
            primitive_topology: d3dcommon::D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
            comp_pipeline: PipelineCache::default(),
            active_bindpoint: BindPoint::Graphics { internal: false },
            active_descriptor_heaps: [native::DescriptorHeap::null(); 2],
            occlusion_query: None,
            pipeline_stats_query: None,
            vertex_bindings_remap: [None; MAX_VERTEX_BUFFERS],
            vertex_buffer_views: [NULL_VERTEX_BUFFER_VIEW; MAX_VERTEX_BUFFERS],
            copies: Vec::new(),
            viewport_cache: ArrayVec::new(),
            scissor_cache: ArrayVec::new(),
            rtv_pools: Vec::new(),
            temporary_gpu_heaps: Vec::new(),
            retained_resources: Vec::new(),
            temp_marker: Vec::new(),
            barriers: Vec::new(),
            raw_name: Vec::new(),
        }
    }

    pub(crate) unsafe fn destroy(
        self,
    ) -> Option<(CommandAllocatorIndex, Option<native::GraphicsCommandList>)> {
        let list = match self.phase {
            Phase::Initial => None,
            Phase::Recording => {
                self.raw.close();
                Some(self.raw)
            }
            Phase::Executable => Some(self.raw),
        };
        for heap in &self.rtv_pools {
            heap.destroy();
        }
        for heap in &self.temporary_gpu_heaps {
            heap.destroy();
        }
        for resource in &self.retained_resources {
            resource.destroy();
        }
        self.allocator_index.map(|index| (index, list))
    }

    pub(crate) unsafe fn as_raw_list(&self) -> *mut d3d12::ID3D12CommandList {
        match self.phase {
            Phase::Executable => (),
            other => error!("Submitting a command buffer in {:?} state", other),
        }
        self.raw.as_mut_ptr() as *mut _
    }

    // Indicates that the pipeline slot has been overriden with an internal pipeline.
    //
    // This only invalidates the slot and the user data!
    fn set_internal_graphics_pipeline(&mut self) {
        self.active_bindpoint = BindPoint::Graphics { internal: true };
        self.gr_pipeline.user_data.dirty_all();
    }

    fn bind_descriptor_heaps(&mut self) {
        self.raw.set_descriptor_heaps(&self.active_descriptor_heaps);
    }

    fn mark_bound_descriptor(&mut self, index: usize, set: &r::DescriptorSet) {
        if !set.raw_name.is_empty() {
            self.temp_marker.clear();
            self.temp_marker.push(0x20); // ' '
            self.temp_marker.push(0x30 + index as u16); // '1..9'
            self.temp_marker.push(0x3A); // ':'
            self.temp_marker.push(0x20); // ' '
            self.temp_marker.extend_from_slice(&set.raw_name);
            unsafe {
                self.raw.SetMarker(
                    0,
                    self.temp_marker.as_ptr() as *const _,
                    self.temp_marker.len() as u32 * 2,
                )
            };
        }
    }

    unsafe fn insert_subpass_barriers(&mut self, insertion: BarrierPoint) {
        let state = self.pass_cache.as_ref().unwrap();
        let proto_barriers = match state.render_pass.subpasses.get(self.cur_subpass as usize) {
            Some(subpass) => match insertion {
                BarrierPoint::Pre => &subpass.pre_barriers,
                BarrierPoint::Post => &subpass.post_barriers,
            },
            None => &state.render_pass.post_barriers,
        };

        self.barriers.clear();
        for barrier in proto_barriers {
            let mut resource_barrier = d3d12::D3D12_RESOURCE_BARRIER {
                Type: d3d12::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                Flags: barrier.flags,
                u: mem::zeroed(),
            };

            *resource_barrier.u.Transition_mut() = d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                pResource: state.attachments[barrier.attachment_id]
                    .view
                    .resource
                    .as_mut_ptr(),
                Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                StateBefore: barrier.states.start,
                StateAfter: barrier.states.end,
            };

            self.barriers.push(resource_barrier);
        }

        self.flush_barriers();
    }

    fn bind_targets(&mut self) {
        let state = self.pass_cache.as_ref().unwrap();
        let subpass = &state.render_pass.subpasses[self.cur_subpass as usize];

        // collect render targets
        let color_views = subpass
            .color_attachments
            .iter()
            .map(|&(id, _)| state.attachments[id].view.handle_rtv.raw().unwrap())
            .collect::<Vec<_>>();
        let ds_view = match subpass.depth_stencil_attachment {
            Some((id, _)) => state.attachments[id]
                .view
                .handle_dsv
                .as_ref()
                .map(|handle| &handle.raw)
                .unwrap() as *const _,
            None => ptr::null(),
        };
        // set render targets
        unsafe {
            self.raw.OMSetRenderTargets(
                color_views.len() as _,
                color_views.as_ptr(),
                minwindef::FALSE,
                ds_view,
            );
        }

        // performs clears for all the attachments first used in this subpass
        for at in state.attachments.iter() {
            if at.subpass_id != Some(self.cur_subpass) {
                continue;
            }

            if let (Some(rtv), Some(cv)) = (at.view.handle_rtv.raw(), at.clear_value) {
                self.clear_render_target_view(rtv, unsafe { cv.color }, &[state.target_rect]);
            }

            if let Some(handle) = at.view.handle_dsv {
                let depth = at.clear_value.map(|cv| unsafe { cv.depth_stencil.depth });
                let stencil = at.stencil_value;

                if depth.is_some() || stencil.is_some() {
                    self.clear_depth_stencil_view(handle.raw, depth, stencil, &[state.target_rect]);
                }
            }
        }
    }

    fn resolve_attachments(&self) {
        let state = self.pass_cache.as_ref().unwrap();
        let subpass = &state.render_pass.subpasses[self.cur_subpass as usize];

        for (&(src_attachment, _), &(dst_attachment, _)) in subpass
            .color_attachments
            .iter()
            .zip(subpass.resolve_attachments.iter())
        {
            if dst_attachment == pass::ATTACHMENT_UNUSED {
                continue;
            }

            let resolve_src = &state.attachments[src_attachment].view;
            let resolve_dst = &state.attachments[dst_attachment].view;

            // The number of layers of the render area are given on framebuffer creation.
            for l in 0..state.num_layers {
                // Attachtments only have a single mip level by specification.
                let subresource_src = resolve_src.calc_subresource(
                    resolve_src.mip_levels.0 as _,
                    (resolve_src.layers.0 + l) as _,
                );
                let subresource_dst = resolve_dst.calc_subresource(
                    resolve_dst.mip_levels.0 as _,
                    (resolve_dst.layers.0 + l) as _,
                );

                // TODO: take width and height of render area into account.
                unsafe {
                    self.raw.ResolveSubresource(
                        resolve_dst.resource.as_mut_ptr(),
                        subresource_dst,
                        resolve_src.resource.as_mut_ptr(),
                        subresource_src,
                        resolve_dst.dxgi_format,
                    );
                }
            }
        }
    }

    fn clear_render_target_view(
        &self,
        rtv: d3d12::D3D12_CPU_DESCRIPTOR_HANDLE,
        color: com::ClearColor,
        rects: &[d3d12::D3D12_RECT],
    ) {
        let num_rects = rects.len() as _;
        let rects = if num_rects > 0 {
            rects.as_ptr()
        } else {
            ptr::null()
        };

        unsafe {
            self.raw
                .clone()
                .ClearRenderTargetView(rtv, &color.float32, num_rects, rects);
        }
    }

    fn clear_depth_stencil_view(
        &self,
        dsv: d3d12::D3D12_CPU_DESCRIPTOR_HANDLE,
        depth: Option<f32>,
        stencil: Option<u32>,
        rects: &[d3d12::D3D12_RECT],
    ) {
        let mut flags = native::ClearFlags::empty();
        if depth.is_some() {
            flags |= native::ClearFlags::DEPTH;
        }
        if stencil.is_some() {
            flags |= native::ClearFlags::STENCIL;
        }

        self.raw.clear_depth_stencil_view(
            dsv,
            flags,
            depth.unwrap_or_default(),
            stencil.unwrap_or_default() as _,
            rects,
        );
    }

    fn set_graphics_bind_point(&mut self) {
        match self.active_bindpoint {
            BindPoint::Compute => {
                // Switch to graphics bind point
                let &(pipeline, _) = self
                    .gr_pipeline
                    .pipeline
                    .as_ref()
                    .expect("No graphics pipeline bound");
                self.raw.set_pipeline_state(pipeline);
            }
            BindPoint::Graphics { internal: true } => {
                // Switch to graphics bind point
                let &(pipeline, ref shared) = self
                    .gr_pipeline
                    .pipeline
                    .as_ref()
                    .expect("No graphics pipeline bound");
                self.raw.set_pipeline_state(pipeline);
                self.raw.set_graphics_root_signature(shared.signature);
                self.bind_descriptor_heaps();
            }
            BindPoint::Graphics { internal: false } => {}
        }

        self.active_bindpoint = BindPoint::Graphics { internal: false };
        let cmd_buffer = &mut self.raw;

        // Flush root signature data
        self.gr_pipeline.flush_user_data(
            |slot, data| unsafe {
                cmd_buffer.clone().SetGraphicsRoot32BitConstants(
                    slot,
                    data.len() as _,
                    data.as_ptr() as *const _,
                    0,
                )
            },
            |slot, gpu| cmd_buffer.set_graphics_root_descriptor_table(slot, gpu),
            |slot, buffer| cmd_buffer.set_graphics_root_constant_buffer_view(slot, buffer),
        );
    }

    fn set_compute_bind_point(&mut self) {
        match self.active_bindpoint {
            BindPoint::Graphics { internal } => {
                // Switch to compute bind point
                let &(pipeline, _) = self
                    .comp_pipeline
                    .pipeline
                    .as_ref()
                    .expect("No compute pipeline bound");

                self.raw.set_pipeline_state(pipeline);

                self.active_bindpoint = BindPoint::Compute;

                if internal {
                    self.bind_descriptor_heaps();
                    // Rebind the graphics root signature as we come from an internal graphics.
                    // Issuing a draw call afterwards would hide the information that we internally
                    // changed the graphics root signature.
                    if let Some((_, ref shared)) = self.gr_pipeline.pipeline {
                        self.raw.set_graphics_root_signature(shared.signature);
                    }
                }
            }
            BindPoint::Compute => {} // Nothing to do
        }

        let cmd_buffer = &mut self.raw;
        self.comp_pipeline.flush_user_data(
            |slot, data| unsafe {
                cmd_buffer.clone().SetComputeRoot32BitConstants(
                    slot,
                    data.len() as _,
                    data.as_ptr() as *const _,
                    0,
                )
            },
            |slot, gpu| cmd_buffer.set_compute_root_descriptor_table(slot, gpu),
            |slot, buffer| cmd_buffer.set_compute_root_constant_buffer_view(slot, buffer),
        );
    }

    fn transition_barrier(
        transition: d3d12::D3D12_RESOURCE_TRANSITION_BARRIER,
    ) -> d3d12::D3D12_RESOURCE_BARRIER {
        let mut barrier = d3d12::D3D12_RESOURCE_BARRIER {
            Type: d3d12::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            Flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_NONE,
            u: unsafe { mem::zeroed() },
        };

        *unsafe { barrier.u.Transition_mut() } = transition;
        barrier
    }

    fn dual_transition_barriers(
        resource: native::Resource,
        sub: u32,
        states: Range<u32>,
    ) -> (d3d12::D3D12_RESOURCE_BARRIER, d3d12::D3D12_RESOURCE_BARRIER) {
        (
            Self::transition_barrier(d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                pResource: resource.as_mut_ptr(),
                Subresource: sub,
                StateBefore: states.start,
                StateAfter: states.end,
            }),
            Self::transition_barrier(d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                pResource: resource.as_mut_ptr(),
                Subresource: sub,
                StateBefore: states.end,
                StateAfter: states.start,
            }),
        )
    }

    fn split_buffer_copy(copies: &mut Vec<Copy>, r: com::BufferImageCopy, image: &r::ImageBound) {
        let buffer_width = if r.buffer_width == 0 {
            r.image_extent.width
        } else {
            r.buffer_width
        };
        let buffer_height = if r.buffer_height == 0 {
            r.image_extent.height
        } else {
            r.buffer_height
        };
        let desc = image.surface_type.desc();
        let bytes_per_block = desc.bits as u32 / 8;
        let image_extent_aligned = image::Extent {
            width: up_align(r.image_extent.width, desc.dim.0 as _),
            height: up_align(r.image_extent.height, desc.dim.1 as _),
            depth: r.image_extent.depth,
        };
        let row_pitch = div(buffer_width, desc.dim.0 as _) * bytes_per_block;
        let slice_pitch = div(buffer_height, desc.dim.1 as _) * row_pitch;
        let is_pitch_aligned = row_pitch % d3d12::D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0;

        for layer in r.image_layers.layers.clone() {
            let img_subresource = image.calc_subresource(r.image_layers.level as _, layer as _, 0);
            let layer_relative = (layer - r.image_layers.layers.start) as u32;
            let layer_offset = r.buffer_offset as u64
                + (layer_relative * slice_pitch * r.image_extent.depth) as u64;
            let aligned_offset =
                layer_offset & !(d3d12::D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT as u64 - 1);
            if layer_offset == aligned_offset && is_pitch_aligned {
                // trivial case: everything is aligned, ready for copying
                copies.push(Copy {
                    footprint_offset: aligned_offset,
                    footprint: image_extent_aligned,
                    row_pitch,
                    img_subresource,
                    img_offset: r.image_offset,
                    buf_offset: image::Offset::ZERO,
                    copy_extent: image_extent_aligned,
                });
            } else if is_pitch_aligned {
                // buffer offset is not aligned
                let row_pitch_texels = row_pitch / bytes_per_block * desc.dim.0 as u32;
                let gap = (layer_offset - aligned_offset) as i32;
                let buf_offset = image::Offset {
                    x: (gap % row_pitch as i32) / bytes_per_block as i32 * desc.dim.0 as i32,
                    y: (gap % slice_pitch as i32) / row_pitch as i32 * desc.dim.1 as i32,
                    z: gap / slice_pitch as i32,
                };
                let footprint = image::Extent {
                    width: buf_offset.x as u32 + image_extent_aligned.width,
                    height: buf_offset.y as u32 + image_extent_aligned.height,
                    depth: buf_offset.z as u32 + image_extent_aligned.depth,
                };
                if r.image_extent.width + buf_offset.x as u32 <= row_pitch_texels {
                    // we can map it to the aligned one and adjust the offsets accordingly
                    copies.push(Copy {
                        footprint_offset: aligned_offset,
                        footprint,
                        row_pitch,
                        img_subresource,
                        img_offset: r.image_offset,
                        buf_offset,
                        copy_extent: image_extent_aligned,
                    });
                } else {
                    // split the copy region into 2 that suffice the previous condition
                    assert!(buf_offset.x as u32 <= row_pitch_texels);
                    let half = row_pitch_texels - buf_offset.x as u32;
                    assert!(half <= r.image_extent.width);

                    copies.push(Copy {
                        footprint_offset: aligned_offset,
                        footprint: image::Extent {
                            width: row_pitch_texels,
                            ..footprint
                        },
                        row_pitch,
                        img_subresource,
                        img_offset: r.image_offset,
                        buf_offset,
                        copy_extent: image::Extent {
                            width: half,
                            ..r.image_extent
                        },
                    });
                    copies.push(Copy {
                        footprint_offset: aligned_offset,
                        footprint: image::Extent {
                            width: image_extent_aligned.width - half,
                            height: footprint.height + desc.dim.1 as u32,
                            depth: footprint.depth,
                        },
                        row_pitch,
                        img_subresource,
                        img_offset: image::Offset {
                            x: r.image_offset.x + half as i32,
                            ..r.image_offset
                        },
                        buf_offset: image::Offset {
                            x: 0,
                            y: buf_offset.y + desc.dim.1 as i32,
                            z: buf_offset.z,
                        },
                        copy_extent: image::Extent {
                            width: image_extent_aligned.width - half,
                            ..image_extent_aligned
                        },
                    });
                }
            } else {
                // worst case: row by row copy
                for z in 0..r.image_extent.depth {
                    for y in 0..image_extent_aligned.height / desc.dim.1 as u32 {
                        // an image row starts non-aligned
                        let row_offset = layer_offset
                            + z as u64 * slice_pitch as u64
                            + y as u64 * row_pitch as u64;
                        let aligned_offset = row_offset
                            & !(d3d12::D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT as u64 - 1);
                        let next_aligned_offset =
                            aligned_offset + d3d12::D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT as u64;
                        let cut_row_texels = (next_aligned_offset - row_offset)
                            / bytes_per_block as u64
                            * desc.dim.0 as u64;
                        let cut_width =
                            cmp::min(image_extent_aligned.width, cut_row_texels as image::Size);
                        let gap_texels = (row_offset - aligned_offset) as image::Size
                            / bytes_per_block as image::Size
                            * desc.dim.0 as image::Size;
                        // this is a conservative row pitch that should be compatible with both copies
                        let max_unaligned_pitch =
                            (r.image_extent.width + gap_texels) * bytes_per_block;
                        let row_pitch = (max_unaligned_pitch
                            | (d3d12::D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1))
                            + 1;

                        copies.push(Copy {
                            footprint_offset: aligned_offset,
                            footprint: image::Extent {
                                width: cut_width + gap_texels,
                                height: desc.dim.1 as _,
                                depth: 1,
                            },
                            row_pitch,
                            img_subresource,
                            img_offset: image::Offset {
                                x: r.image_offset.x,
                                y: r.image_offset.y + desc.dim.1 as i32 * y as i32,
                                z: r.image_offset.z + z as i32,
                            },
                            buf_offset: image::Offset {
                                x: gap_texels as i32,
                                y: 0,
                                z: 0,
                            },
                            copy_extent: image::Extent {
                                width: cut_width,
                                height: desc.dim.1 as _,
                                depth: 1,
                            },
                        });

                        // and if it crosses a pitch alignment - we copy the rest separately
                        if cut_width >= image_extent_aligned.width {
                            continue;
                        }
                        let leftover = image_extent_aligned.width - cut_width;

                        copies.push(Copy {
                            footprint_offset: next_aligned_offset,
                            footprint: image::Extent {
                                width: leftover,
                                height: desc.dim.1 as _,
                                depth: 1,
                            },
                            row_pitch,
                            img_subresource,
                            img_offset: image::Offset {
                                x: r.image_offset.x + cut_width as i32,
                                y: r.image_offset.y + y as i32 * desc.dim.1 as i32,
                                z: r.image_offset.z + z as i32,
                            },
                            buf_offset: image::Offset::ZERO,
                            copy_extent: image::Extent {
                                width: leftover,
                                height: desc.dim.1 as _,
                                depth: 1,
                            },
                        });
                    }
                }
            }
        }
    }

    fn set_vertex_buffers(&mut self) {
        let cmd_buffer = &mut self.raw;
        let vbs_remap = &self.vertex_bindings_remap;
        let vbs = &self.vertex_buffer_views;
        let mut last_end_slot = 0;
        loop {
            let start_offset = match vbs_remap[last_end_slot..]
                .iter()
                .position(|remap| remap.is_some())
            {
                Some(offset) => offset,
                None => break,
            };

            let start_slot = last_end_slot + start_offset;
            let buffers = vbs_remap[start_slot..]
                .iter()
                .take_while(|x| x.is_some())
                .filter_map(|mapping| {
                    let mapping = mapping.unwrap();
                    let view = vbs[mapping.mapped_binding];
                    // Skip bindings that don't make sense. Since this function is called eagerly,
                    // we expect it to be called with all the valid inputs prior to drawing.
                    view.SizeInBytes.checked_sub(mapping.offset).map(|size| {
                        d3d12::D3D12_VERTEX_BUFFER_VIEW {
                            BufferLocation: view.BufferLocation + mapping.offset as u64,
                            SizeInBytes: size,
                            StrideInBytes: mapping.stride,
                        }
                    })
                })
                .collect::<ArrayVec<[_; MAX_VERTEX_BUFFERS]>>();

            if buffers.is_empty() {
                last_end_slot = start_slot + 1;
            } else {
                let num_views = buffers.len();
                unsafe {
                    cmd_buffer.IASetVertexBuffers(
                        start_slot as _,
                        num_views as _,
                        buffers.as_ptr(),
                    );
                }
                last_end_slot = start_slot + num_views;
            }
        }
    }

    fn flip_barriers(&mut self) {
        for barrier in self.barriers.iter_mut() {
            if barrier.Type == d3d12::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION {
                let transition = unsafe { barrier.u.Transition_mut() };
                mem::swap(&mut transition.StateBefore, &mut transition.StateAfter);
            }
        }
    }

    fn _set_buffer_barrier(
        &mut self,
        target: &r::BufferBound,
        state: d3d12::D3D12_RESOURCE_STATES,
    ) {
        let barrier = Self::transition_barrier(d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
            pResource: target.resource.as_mut_ptr(),
            Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            StateBefore: d3d12::D3D12_RESOURCE_STATE_COMMON,
            StateAfter: state,
        });
        self.barriers.clear();
        self.barriers.push(barrier);
    }

    fn fill_texture_barries(
        &mut self,
        target: &r::ImageBound,
        states: Range<d3d12::D3D12_RESOURCE_STATES>,
        range: &image::SubresourceRange,
    ) {
        if states.start == states.end {
            return;
        }

        let mut bar = Self::transition_barrier(d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
            pResource: target.resource.as_mut_ptr(),
            Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            StateBefore: states.start,
            StateAfter: states.end,
        });
        let full_range = image::SubresourceRange {
            aspects: range.aspects,
            ..Default::default()
        };
        let num_levels = range.resolve_level_count(target.mip_levels);
        let num_layers = range.resolve_layer_count(target.kind.num_layers());

        if *range == full_range {
            // Only one barrier if it affects the whole image.
            self.barriers.push(bar);
        } else {
            // Generate barrier for each layer/level combination.
            for rel_level in 0..num_levels {
                for rel_layer in 0..num_layers {
                    unsafe {
                        let transition_barrier = &mut *bar.u.Transition_mut();
                        transition_barrier.Subresource = target.calc_subresource(
                            (range.level_start + rel_level) as _,
                            (range.layer_start + rel_layer) as _,
                            0,
                        );
                    }
                    self.barriers.push(bar);
                }
            }
        }
    }

    fn fill_marker(&mut self, name: &str) -> (*const ctypes::c_void, u32) {
        self.temp_marker.clear();
        self.temp_marker.extend(name.encode_utf16());
        self.temp_marker.push(0);
        (
            self.temp_marker.as_ptr() as *const _,
            self.temp_marker.len() as u32 * 2,
        )
    }

    unsafe fn flush_barriers(&self) {
        if !self.barriers.is_empty() {
            self.raw
                .ResourceBarrier(self.barriers.len() as _, self.barriers.as_ptr());
        }
    }
}

impl com::CommandBuffer<Backend> for CommandBuffer {
    unsafe fn begin(
        &mut self,
        flags: com::CommandBufferFlags,
        _info: com::CommandBufferInheritanceInfo<Backend>,
    ) {
        // TODO: Implement flags and secondary command buffers (bundles).
        // Note: we need to be ready for a situation where the whole
        // command pool was reset.
        self.reset(true);
        self.phase = Phase::Recording;
        self.begin_flags = flags;
        let (allocator_index, list) = self.pool_shared.acquire();

        assert!(self.allocator_index.is_none());
        assert_eq!(self.raw, native::GraphicsCommandList::null());
        self.allocator_index = Some(allocator_index);
        self.raw = list;

        if !self.raw_name.is_empty() {
            self.raw.SetName(self.raw_name.as_ptr());
        }
    }

    unsafe fn finish(&mut self) {
        self.raw.close();
        assert_eq!(self.phase, Phase::Recording);
        self.phase = Phase::Executable;
        self.pool_shared
            .release_allocator(self.allocator_index.unwrap());
    }

    unsafe fn reset(&mut self, _release_resources: bool) {
        if self.phase == Phase::Recording {
            self.raw.close();
            self.pool_shared
                .release_allocator(self.allocator_index.unwrap());
        }
        if self.phase != Phase::Initial {
            // Reset the name so it won't get used later for an unnamed `CommandBuffer`.
            const EMPTY_NAME: u16 = 0;
            self.raw.SetName(&EMPTY_NAME);

            let allocator_index = self.allocator_index.take().unwrap();
            self.pool_shared.release_list(self.raw, allocator_index);
            self.raw = native::GraphicsCommandList::null();
        }
        self.phase = Phase::Initial;

        self.pass_cache = None;
        self.cur_subpass = !0;
        self.gr_pipeline = PipelineCache::default();
        self.primitive_topology = d3dcommon::D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        self.comp_pipeline = PipelineCache::default();
        self.active_bindpoint = BindPoint::Graphics { internal: false };
        self.active_descriptor_heaps = [native::DescriptorHeap::null(); 2];
        self.occlusion_query = None;
        self.pipeline_stats_query = None;
        self.vertex_bindings_remap = [None; MAX_VERTEX_BUFFERS];
        self.vertex_buffer_views = [NULL_VERTEX_BUFFER_VIEW; MAX_VERTEX_BUFFERS];
        for heap in self.rtv_pools.drain(..) {
            heap.destroy();
        }
        for heap in self.temporary_gpu_heaps.drain(..) {
            heap.destroy();
        }
        for resource in self.retained_resources.drain(..) {
            resource.destroy();
        }
    }

    unsafe fn begin_render_pass<'a, T>(
        &mut self,
        render_pass: &r::RenderPass,
        framebuffer: &r::Framebuffer,
        target_rect: pso::Rect,
        attachment_infos: T,
        _first_subpass: com::SubpassContents,
    ) where
        T: Iterator<Item = com::RenderAttachmentInfo<'a, Backend>>,
    {
        // Make sure that no subpass works with Present as intermediate layout.
        // This wouldn't make much sense, and proceeding with this constraint
        // allows the state transitions generated from subpass dependencies
        // to ignore the layouts completely.
        assert!(!render_pass.subpasses.iter().any(|sp| sp
            .color_attachments
            .iter()
            .chain(sp.depth_stencil_attachment.iter())
            .chain(sp.input_attachments.iter())
            .any(|aref| aref.1 == image::Layout::Present)));

        if !render_pass.raw_name.is_empty() {
            let n = &render_pass.raw_name;
            self.raw
                .BeginEvent(0, n.as_ptr() as *const _, n.len() as u32 * 2);
        }

        self.barriers.clear();
        let mut attachments = Vec::new();
        for (i, (info, attachment)) in attachment_infos
            .zip(render_pass.attachments.iter())
            .enumerate()
        {
            let view = info.image_view.clone();
            // for swapchain views, we consider the initial layout to always be `General`
            let pass_start_state =
                conv::map_image_resource_state(image::Access::empty(), attachment.layouts.start);
            if view.is_swapchain() && pass_start_state != d3d12::D3D12_RESOURCE_STATE_COMMON {
                let barrier = d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                    pResource: view.resource.as_mut_ptr(),
                    Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    StateBefore: d3d12::D3D12_RESOURCE_STATE_COMMON,
                    StateAfter: pass_start_state,
                };
                self.barriers.push(Self::transition_barrier(barrier));
            }

            attachments.push(AttachmentInfo {
                subpass_id: render_pass
                    .subpasses
                    .iter()
                    .position(|sp| sp.is_using(i))
                    .map(|i| i as pass::SubpassId),
                view,
                clear_value: if attachment.ops.load == pass::AttachmentLoadOp::Clear {
                    Some(info.clear_value)
                } else {
                    None
                },
                stencil_value: if attachment.stencil_ops.load == pass::AttachmentLoadOp::Clear {
                    Some(info.clear_value.depth_stencil.stencil)
                } else {
                    None
                },
            });
        }
        self.flush_barriers();

        self.pass_cache = Some(RenderPassCache {
            render_pass: render_pass.clone(),
            target_rect: get_rect(&target_rect),
            attachments,
            num_layers: framebuffer.layers,
            has_name: !render_pass.raw_name.is_empty(),
        });
        self.cur_subpass = 0;
        self.insert_subpass_barriers(BarrierPoint::Pre);
        self.bind_targets();
    }

    unsafe fn next_subpass(&mut self, _contents: com::SubpassContents) {
        self.insert_subpass_barriers(BarrierPoint::Post);
        self.resolve_attachments();

        self.cur_subpass += 1;
        self.insert_subpass_barriers(BarrierPoint::Pre);
        self.bind_targets();
    }

    unsafe fn end_render_pass(&mut self) {
        self.insert_subpass_barriers(BarrierPoint::Post);
        self.resolve_attachments();

        self.cur_subpass = !0;
        self.insert_subpass_barriers(BarrierPoint::Pre);

        let pc = self.pass_cache.take().unwrap();
        self.barriers.clear();
        for (at, attachment) in pc.attachments.iter().zip(pc.render_pass.attachments.iter()) {
            // for swapchain views, we consider the initial layout to always be `General`
            let pass_end_state =
                conv::map_image_resource_state(image::Access::empty(), attachment.layouts.end);
            if at.view.is_swapchain() && pass_end_state != d3d12::D3D12_RESOURCE_STATE_COMMON {
                let barrier = d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                    pResource: at.view.resource.as_mut_ptr(),
                    Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    StateBefore: pass_end_state,
                    StateAfter: d3d12::D3D12_RESOURCE_STATE_COMMON,
                };
                self.barriers.push(Self::transition_barrier(barrier));
            }
        }
        self.flush_barriers();

        if pc.has_name {
            self.raw.EndEvent();
        }
    }

    unsafe fn pipeline_barrier<'a, T>(
        &mut self,
        stages: Range<pso::PipelineStage>,
        _dependencies: memory::Dependencies,
        barriers: T,
    ) where
        T: Iterator<Item = memory::Barrier<'a, Backend>>,
    {
        self.barriers.clear();

        // transition barriers
        for barrier in barriers {
            match barrier {
                memory::Barrier::AllBuffers(_) | memory::Barrier::AllImages(_) => {
                    // Aliasing barrier with NULL resource is the closest we can get to
                    // a global memory barrier in Vulkan.
                    // Was suggested by a Microsoft representative as well as some of the IHVs.
                    let mut bar = d3d12::D3D12_RESOURCE_BARRIER {
                        Type: d3d12::D3D12_RESOURCE_BARRIER_TYPE_UAV,
                        Flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_NONE,
                        u: mem::zeroed(),
                    };
                    *bar.u.UAV_mut() = d3d12::D3D12_RESOURCE_UAV_BARRIER {
                        pResource: ptr::null_mut(),
                    };
                    self.barriers.push(bar);
                }
                memory::Barrier::Buffer {
                    ref states,
                    target,
                    ref families,
                    range: _,
                } => {
                    // TODO: Implement queue family ownership transitions for dx12
                    if let Some(f) = families {
                        if f.start.0 != f.end.0 {
                            unimplemented!("Queue family resource ownership transitions are not implemented for DX12 (attempted transition from queue family {} to {}", f.start.0, f.end.0);
                        }
                    }

                    let state_src = conv::map_buffer_resource_state(states.start);
                    let state_dst = conv::map_buffer_resource_state(states.end);

                    if state_src == state_dst {
                        continue;
                    }

                    let target = target.expect_bound();
                    let bar = Self::transition_barrier(d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                        pResource: target.resource.as_mut_ptr(),
                        Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        StateBefore: state_src,
                        StateAfter: state_dst,
                    });

                    self.barriers.push(bar);
                }
                memory::Barrier::Image {
                    ref states,
                    target,
                    ref families,
                    ref range,
                } => {
                    // TODO: Implement queue family ownership transitions for dx12
                    if let Some(f) = families {
                        if f.start.0 != f.end.0 {
                            unimplemented!("Queue family resource ownership transitions are not implemented for DX12 (attempted transition from queue family {} to {}", f.start.0, f.end.0);
                        }
                    }

                    let state_src = conv::map_image_resource_state(states.start.0, states.start.1);
                    let state_dst = conv::map_image_resource_state(states.end.0, states.end.1);

                    let target = target.expect_bound();

                    match target.place {
                        r::Place::Heap { .. } => {
                            self.fill_texture_barries(target, state_src..state_dst, range);
                        }
                        r::Place::Swapchain { .. } => {} //ignore
                    }
                }
            }
        }

        let all_shader_stages = pso::PipelineStage::VERTEX_SHADER
            | pso::PipelineStage::FRAGMENT_SHADER
            | pso::PipelineStage::COMPUTE_SHADER
            | pso::PipelineStage::GEOMETRY_SHADER
            | pso::PipelineStage::HULL_SHADER
            | pso::PipelineStage::DOMAIN_SHADER;

        // UAV barriers
        //
        // TODO: Currently always add a global UAV barrier.
        //       WAR only requires an execution barrier but D3D12 seems to need
        //       a UAV barrier for this according to docs. Can we make this better?
        if (stages.start & stages.end).intersects(all_shader_stages) {
            let mut barrier = d3d12::D3D12_RESOURCE_BARRIER {
                Type: d3d12::D3D12_RESOURCE_BARRIER_TYPE_UAV,
                Flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_NONE,
                u: mem::zeroed(),
            };
            *barrier.u.UAV_mut() = d3d12::D3D12_RESOURCE_UAV_BARRIER {
                pResource: ptr::null_mut(),
            };
            self.barriers.push(barrier);
        }

        // Alias barriers
        //
        // TODO: Optimize, don't always add an alias barrier
        if false {
            let mut barrier = d3d12::D3D12_RESOURCE_BARRIER {
                Type: d3d12::D3D12_RESOURCE_BARRIER_TYPE_ALIASING,
                Flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_NONE,
                u: mem::zeroed(),
            };
            *barrier.u.Aliasing_mut() = d3d12::D3D12_RESOURCE_ALIASING_BARRIER {
                pResourceBefore: ptr::null_mut(),
                pResourceAfter: ptr::null_mut(),
            };
            self.barriers.push(barrier);
        }

        self.flush_barriers();
    }

    unsafe fn clear_image<T>(
        &mut self,
        image: &r::Image,
        layout: image::Layout,
        value: com::ClearValue,
        subresource_ranges: T,
    ) where
        T: Iterator<Item = image::SubresourceRange>,
    {
        let image = image.expect_bound();
        let base_state = conv::map_image_resource_state(image::Access::TRANSFER_WRITE, layout);

        for sub in subresource_ranges {
            if sub.level_start != 0 || image.mip_levels != 1 {
                warn!("Clearing non-zero mipmap levels is not supported yet");
            }
            let target_state = if sub.aspects.contains(Aspects::COLOR) {
                d3d12::D3D12_RESOURCE_STATE_RENDER_TARGET
            } else {
                d3d12::D3D12_RESOURCE_STATE_DEPTH_WRITE
            };

            self.barriers.clear();
            self.fill_texture_barries(image, base_state..target_state, &sub);
            self.flush_barriers();

            for rel_layer in 0..sub.resolve_layer_count(image.kind.num_layers()) {
                let layer = (sub.layer_start + rel_layer) as usize;
                if sub.aspects.contains(Aspects::COLOR) {
                    let rtv = image.clear_cv[layer].raw;
                    self.clear_render_target_view(rtv, value.color, &[]);
                }
                if sub.aspects.contains(Aspects::DEPTH) {
                    let dsv = image.clear_dv[layer].raw;
                    self.clear_depth_stencil_view(dsv, Some(value.depth_stencil.depth), None, &[]);
                }
                if sub.aspects.contains(Aspects::STENCIL) {
                    let dsv = image.clear_sv[layer].raw;
                    self.clear_depth_stencil_view(
                        dsv,
                        None,
                        Some(value.depth_stencil.stencil as _),
                        &[],
                    );
                }
            }

            self.flip_barriers();
            self.flush_barriers();
        }
    }

    unsafe fn clear_attachments<T, U>(&mut self, clears: T, rects: U)
    where
        T: Iterator<Item = com::AttachmentClear>,
        U: Iterator<Item = pso::ClearRect>,
    {
        let pass_cache = match self.pass_cache {
            Some(ref cache) => cache,
            None => panic!("`clear_attachments` can only be called inside a renderpass"),
        };
        let sub_pass = &pass_cache.render_pass.subpasses[self.cur_subpass as usize];

        let clear_rects: SmallVec<[pso::ClearRect; 4]> = rects.collect();

        let device = self.shared.service_pipes.device;

        for clear in clears {
            match clear {
                com::AttachmentClear::Color { index, value } => {
                    let attachment = {
                        let rtv_id = sub_pass.color_attachments[index];
                        &pass_cache.attachments[rtv_id.0].view
                    };

                    let mut rtv_pool = descriptors_cpu::HeapLinear::new(
                        device,
                        native::DescriptorHeapType::Rtv,
                        clear_rects.len(),
                    );

                    for clear_rect in &clear_rects {
                        assert!(attachment.layers.0 + clear_rect.layers.end <= attachment.layers.1);
                        let rect = [get_rect(&clear_rect.rect)];

                        let view_info = device::ViewInfo {
                            resource: attachment.resource,
                            kind: attachment.kind,
                            caps: image::ViewCapabilities::empty(),
                            view_kind: image::ViewKind::D2Array,
                            format: attachment.dxgi_format,
                            component_mapping: device::IDENTITY_MAPPING,
                            levels: attachment.mip_levels.0..attachment.mip_levels.1,
                            layers: attachment.layers.0 + clear_rect.layers.start
                                ..attachment.layers.0 + clear_rect.layers.end,
                        };
                        let rtv = rtv_pool.alloc_handle();
                        Device::view_image_as_render_target_impl(device, rtv, &view_info).unwrap();
                        self.clear_render_target_view(rtv, value.into(), &rect);
                    }

                    rtv_pool.destroy();
                }
                com::AttachmentClear::DepthStencil { depth, stencil } => {
                    let attachment = {
                        let dsv_id = sub_pass.depth_stencil_attachment.unwrap();
                        &pass_cache.attachments[dsv_id.0].view
                    };

                    let mut dsv_pool = descriptors_cpu::HeapLinear::new(
                        device,
                        native::DescriptorHeapType::Dsv,
                        clear_rects.len(),
                    );

                    for clear_rect in &clear_rects {
                        assert!(attachment.layers.0 + clear_rect.layers.end <= attachment.layers.1);
                        let rect = [get_rect(&clear_rect.rect)];

                        let view_info = device::ViewInfo {
                            resource: attachment.resource,
                            kind: attachment.kind,
                            caps: image::ViewCapabilities::empty(),
                            view_kind: image::ViewKind::D2Array,
                            format: attachment.dxgi_format,
                            component_mapping: device::IDENTITY_MAPPING,
                            levels: attachment.mip_levels.0..attachment.mip_levels.1,
                            layers: attachment.layers.0 + clear_rect.layers.start
                                ..attachment.layers.0 + clear_rect.layers.end,
                        };
                        let dsv = dsv_pool.alloc_handle();
                        Device::view_image_as_depth_stencil_impl(device, dsv, &view_info).unwrap();
                        self.clear_depth_stencil_view(dsv, depth, stencil, &rect);
                    }

                    dsv_pool.destroy();
                }
            }
        }
    }

    unsafe fn resolve_image<T>(
        &mut self,
        src: &r::Image,
        _src_layout: image::Layout,
        dst: &r::Image,
        _dst_layout: image::Layout,
        regions: T,
    ) where
        T: Iterator<Item = com::ImageResolve>,
    {
        let src = src.expect_bound();
        let dst = dst.expect_bound();
        assert_eq!(src.descriptor.Format, dst.descriptor.Format);

        {
            // Insert barrier for `COPY_DEST` to `RESOLVE_DEST` as we only expose
            // `TRANSFER_WRITE` which is used for all copy commands.
            let transition_barrier =
                Self::transition_barrier(d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                    pResource: dst.resource.as_mut_ptr(),
                    Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, // TODO: only affected ranges
                    StateBefore: d3d12::D3D12_RESOURCE_STATE_COPY_DEST,
                    StateAfter: d3d12::D3D12_RESOURCE_STATE_RESOLVE_DEST,
                });
            self.raw.ResourceBarrier(1, &transition_barrier);
        }

        for r in regions {
            for layer in 0..r.extent.depth as u32 {
                self.raw.ResolveSubresource(
                    src.resource.as_mut_ptr(),
                    src.calc_subresource(
                        r.src_subresource.level as _,
                        r.src_subresource.layers.start as u32 + layer,
                        0,
                    ),
                    dst.resource.as_mut_ptr(),
                    dst.calc_subresource(
                        r.dst_subresource.level as _,
                        r.dst_subresource.layers.start as u32 + layer,
                        0,
                    ),
                    src.descriptor.Format,
                );
            }
        }

        {
            // Insert barrier for back transition from `RESOLVE_DEST` to `COPY_DEST`.
            let transition_barrier =
                Self::transition_barrier(d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                    pResource: dst.resource.as_mut_ptr(),
                    Subresource: d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, // TODO: only affected ranges
                    StateBefore: d3d12::D3D12_RESOURCE_STATE_RESOLVE_DEST,
                    StateAfter: d3d12::D3D12_RESOURCE_STATE_COPY_DEST,
                });
            self.raw.ResourceBarrier(1, &transition_barrier);
        }
    }

    unsafe fn blit_image<T>(
        &mut self,
        src: &r::Image,
        _src_layout: image::Layout,
        dst: &r::Image,
        _dst_layout: image::Layout,
        filter: image::Filter,
        regions: T,
    ) where
        T: Iterator<Item = com::ImageBlit>,
    {
        let device = self.shared.service_pipes.device.clone();
        let src = src.expect_bound();
        let dst = dst.expect_bound();

        // TODO: Resource barriers for src.
        // TODO: depth or stencil images not supported so far

        // TODO: only supporting 2D images
        match (src.kind, dst.kind) {
            (image::Kind::D2(..), image::Kind::D2(..)) => {}
            _ => unimplemented!(),
        }

        // Descriptor heap for the current blit, only storing the src image
        let (srv_heap, _) = device.create_descriptor_heap(
            1,
            native::DescriptorHeapType::CbvSrvUav,
            native::DescriptorHeapFlags::SHADER_VISIBLE,
            0,
        );
        self.raw.set_descriptor_heaps(&[srv_heap]);
        self.temporary_gpu_heaps.push(srv_heap);

        let srv_desc = Device::build_image_as_shader_resource_desc(&device::ViewInfo {
            resource: src.resource,
            kind: src.kind,
            caps: src.view_caps,
            view_kind: image::ViewKind::D2Array, // TODO
            format: src.default_view_format.unwrap(),
            component_mapping: device::IDENTITY_MAPPING,
            levels: 0..src.descriptor.MipLevels as _,
            layers: 0..src.kind.num_layers(),
        })
        .unwrap();
        device.CreateShaderResourceView(
            src.resource.as_mut_ptr(),
            &srv_desc,
            srv_heap.start_cpu_descriptor(),
        );

        let filter = match filter {
            image::Filter::Nearest => d3d12::D3D12_FILTER_MIN_MAG_MIP_POINT,
            image::Filter::Linear => d3d12::D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        };

        struct Instance {
            rtv: d3d12::D3D12_CPU_DESCRIPTOR_HANDLE,
            viewport: d3d12::D3D12_VIEWPORT,
            data: internal::BlitData,
        }
        let mut instances = FastHashMap::<internal::BlitKey, Vec<Instance>>::default();
        let mut barriers = Vec::new();

        for r in regions {
            let first_layer = r.dst_subresource.layers.start;
            let num_layers = r.dst_subresource.layers.end - first_layer;

            // WORKAROUND: renderdoc crashes if we destroy the pool too early
            let rtv_pool = Device::create_descriptor_heap_impl(
                device,
                native::DescriptorHeapType::Rtv,
                false,
                num_layers as _,
            );
            self.rtv_pools.push(rtv_pool.raw.clone());

            let key = match r.dst_subresource.aspects {
                format::Aspects::COLOR => {
                    let format = dst.default_view_format.unwrap();
                    // Create RTVs of the dst image for the miplevel of the current region
                    for i in 0..num_layers {
                        let mut desc = d3d12::D3D12_RENDER_TARGET_VIEW_DESC {
                            Format: format,
                            ViewDimension: d3d12::D3D12_RTV_DIMENSION_TEXTURE2DARRAY,
                            u: mem::zeroed(),
                        };

                        *desc.u.Texture2DArray_mut() = d3d12::D3D12_TEX2D_ARRAY_RTV {
                            MipSlice: r.dst_subresource.level as _,
                            FirstArraySlice: (i + first_layer) as u32,
                            ArraySize: 1,
                            PlaneSlice: 0, // TODO
                        };

                        let view = rtv_pool.at(i as _, 0).cpu;
                        device.CreateRenderTargetView(dst.resource.as_mut_ptr(), &desc, view);
                    }

                    (format, filter)
                }
                _ => unimplemented!(),
            };

            // Take flipping into account
            let viewport = d3d12::D3D12_VIEWPORT {
                TopLeftX: cmp::min(r.dst_bounds.start.x, r.dst_bounds.end.x) as _,
                TopLeftY: cmp::min(r.dst_bounds.start.y, r.dst_bounds.end.y) as _,
                Width: (r.dst_bounds.end.x - r.dst_bounds.start.x).abs() as _,
                Height: (r.dst_bounds.end.y - r.dst_bounds.start.y).abs() as _,
                MinDepth: 0.0,
                MaxDepth: 1.0,
            };

            let list = instances.entry(key).or_insert(Vec::new());

            for i in 0..num_layers {
                let src_layer = r.src_subresource.layers.start + i;
                // Screen space triangle blitting
                let data = {
                    // Image extents, layers are treated as depth
                    let (sx, dx) = if r.dst_bounds.start.x > r.dst_bounds.end.x {
                        (
                            r.src_bounds.end.x,
                            r.src_bounds.start.x - r.src_bounds.end.x,
                        )
                    } else {
                        (
                            r.src_bounds.start.x,
                            r.src_bounds.end.x - r.src_bounds.start.x,
                        )
                    };
                    let (sy, dy) = if r.dst_bounds.start.y > r.dst_bounds.end.y {
                        (
                            r.src_bounds.end.y,
                            r.src_bounds.start.y - r.src_bounds.end.y,
                        )
                    } else {
                        (
                            r.src_bounds.start.y,
                            r.src_bounds.end.y - r.src_bounds.start.y,
                        )
                    };
                    let image::Extent { width, height, .. } =
                        src.kind.level_extent(r.src_subresource.level);

                    internal::BlitData {
                        src_offset: [sx as f32 / width as f32, sy as f32 / height as f32],
                        src_extent: [dx as f32 / width as f32, dy as f32 / height as f32],
                        layer: src_layer as f32,
                        level: r.src_subresource.level as _,
                    }
                };

                list.push(Instance {
                    rtv: rtv_pool.at(i as _, 0).cpu,
                    viewport,
                    data,
                });

                barriers.push(Self::transition_barrier(
                    d3d12::D3D12_RESOURCE_TRANSITION_BARRIER {
                        pResource: dst.resource.as_mut_ptr(),
                        Subresource: dst.calc_subresource(
                            r.dst_subresource.level as _,
                            (first_layer + i) as _,
                            0,
                        ),
                        StateBefore: d3d12::D3D12_RESOURCE_STATE_COPY_DEST,
                        StateAfter: d3d12::D3D12_RESOURCE_STATE_RENDER_TARGET,
                    },
                ));
            }
        }

        // pre barriers
        self.raw
            .ResourceBarrier(barriers.len() as _, barriers.as_ptr());
        // execute blits
        self.set_internal_graphics_pipeline();
        for (key, list) in instances {
            let blit = self.shared.service_pipes.get_blit_2d_color(key);
            self.raw
                .IASetPrimitiveTopology(d3dcommon::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            self.raw.set_pipeline_state(blit.pipeline);
            self.raw.set_graphics_root_signature(blit.signature);
            self.raw
                .set_graphics_root_descriptor_table(0, srv_heap.start_gpu_descriptor());
            for inst in list {
                let scissor = d3d12::D3D12_RECT {
                    left: inst.viewport.TopLeftX as _,
                    top: inst.viewport.TopLeftY as _,
                    right: (inst.viewport.TopLeftX + inst.viewport.Width) as _,
                    bottom: (inst.viewport.TopLeftY + inst.viewport.Height) as _,
                };
                self.raw.RSSetViewports(1, &inst.viewport);
                self.raw.RSSetScissorRects(1, &scissor);
                self.raw.SetGraphicsRoot32BitConstants(
                    1,
                    (mem::size_of::<internal::BlitData>() / 4) as _,
                    &inst.data as *const _ as *const _,
                    0,
                );
                self.raw
                    .OMSetRenderTargets(1, &inst.rtv, minwindef::TRUE, ptr::null());
                self.raw.draw(3, 1, 0, 0);
            }
        }
        // post barriers
        for bar in &mut barriers {
            let transition = bar.u.Transition_mut();
            mem::swap(&mut transition.StateBefore, &mut transition.StateAfter);
        }
        self.raw
            .ResourceBarrier(barriers.len() as _, barriers.as_ptr());

        // Reset states
        self.raw
            .RSSetViewports(self.viewport_cache.len() as _, self.viewport_cache.as_ptr());
        self.raw
            .RSSetScissorRects(self.scissor_cache.len() as _, self.scissor_cache.as_ptr());
        if self.primitive_topology != d3dcommon::D3D_PRIMITIVE_TOPOLOGY_UNDEFINED {
            self.raw.IASetPrimitiveTopology(self.primitive_topology);
        }
    }

    unsafe fn bind_index_buffer(
        &mut self,
        buffer: &r::Buffer,
        sub: buffer::SubRange,
        ty: IndexType,
    ) {
        let buffer = buffer.expect_bound();
        let format = match ty {
            IndexType::U16 => dxgiformat::DXGI_FORMAT_R16_UINT,
            IndexType::U32 => dxgiformat::DXGI_FORMAT_R32_UINT,
        };
        let location = buffer.resource.gpu_virtual_address();
        self.raw.set_index_buffer(
            location + sub.offset,
            sub.size_to(buffer.requirements.size) as u32,
            format,
        );
    }

    unsafe fn bind_vertex_buffers<'a, T>(&mut self, first_binding: pso::BufferIndex, buffers: T)
    where
        T: Iterator<Item = (&'a r::Buffer, buffer::SubRange)>,
    {
        assert!(first_binding as usize <= MAX_VERTEX_BUFFERS);

        for (view, (buffer, sub)) in self.vertex_buffer_views[first_binding as _..]
            .iter_mut()
            .zip(buffers)
        {
            let b = buffer.expect_bound();
            let base = (*b.resource).GetGPUVirtualAddress();
            view.BufferLocation = base + sub.offset;
            view.SizeInBytes = sub.size_to(b.requirements.size) as u32;
        }
        self.set_vertex_buffers();
    }

    unsafe fn set_viewports<T>(&mut self, first_viewport: u32, viewports: T)
    where
        T: Iterator<Item = pso::Viewport>,
    {
        for (i, vp) in viewports.enumerate() {
            let viewport = d3d12::D3D12_VIEWPORT {
                TopLeftX: vp.rect.x as _,
                TopLeftY: vp.rect.y as _,
                Width: vp.rect.w as _,
                Height: vp.rect.h as _,
                MinDepth: vp.depth.start,
                MaxDepth: vp.depth.end,
            };
            if i + first_viewport as usize >= self.viewport_cache.len() {
                self.viewport_cache.push(viewport);
            } else {
                self.viewport_cache[i + first_viewport as usize] = viewport;
            }
        }

        self.raw
            .RSSetViewports(self.viewport_cache.len() as _, self.viewport_cache.as_ptr());
    }

    unsafe fn set_scissors<T>(&mut self, first_scissor: u32, scissors: T)
    where
        T: Iterator<Item = pso::Rect>,
    {
        for (i, r) in scissors.enumerate() {
            let rect = get_rect(&r);
            if i + first_scissor as usize >= self.scissor_cache.len() {
                self.scissor_cache.push(rect);
            } else {
                self.scissor_cache[i + first_scissor as usize] = rect;
            }
        }

        self.raw
            .RSSetScissorRects(self.scissor_cache.len() as _, self.scissor_cache.as_ptr())
    }

    unsafe fn set_blend_constants(&mut self, color: pso::ColorValue) {
        self.raw.set_blend_factor(color);
    }

    unsafe fn set_stencil_reference(&mut self, faces: pso::Face, value: pso::StencilValue) {
        assert!(!faces.is_empty());

        if !faces.is_all() {
            warn!(
                "Stencil ref values set for both faces but only one was requested ({})",
                faces.bits(),
            );
        }

        self.raw.set_stencil_reference(value as _);
    }

    unsafe fn set_stencil_read_mask(&mut self, _faces: pso::Face, _value: pso::StencilValue) {
        //TODO:
        // unimplemented!();
    }

    unsafe fn set_stencil_write_mask(&mut self, _faces: pso::Face, _value: pso::StencilValue) {
        //TODO:
        //unimplemented!();
    }

    unsafe fn set_depth_bounds(&mut self, bounds: Range<f32>) {
        let (cmd_list1, hr) = self.raw.cast::<d3d12::ID3D12GraphicsCommandList1>();
        if winerror::SUCCEEDED(hr) {
            cmd_list1.OMSetDepthBounds(bounds.start, bounds.end);
            cmd_list1.destroy();
        } else {
            warn!("Depth bounds test is not supported");
        }
    }

    unsafe fn set_line_width(&mut self, width: f32) {
        validate_line_width(width);
    }

    unsafe fn set_depth_bias(&mut self, _depth_bias: pso::DepthBias) {
        //TODO:
        // unimplemented!()
    }

    unsafe fn bind_graphics_pipeline(&mut self, pipeline: &r::GraphicsPipeline) {
        match self.gr_pipeline.pipeline {
            Some((_, ref shared)) if Arc::ptr_eq(shared, &pipeline.shared) => {
                // Same root signature, nothing to do
            }
            _ => {
                self.raw
                    .set_graphics_root_signature(pipeline.shared.signature);
                // All slots need to be rebound internally on signature change.
                self.gr_pipeline.user_data.dirty_all();
            }
        }
        self.raw.set_pipeline_state(pipeline.raw);
        self.raw.IASetPrimitiveTopology(pipeline.topology);
        self.primitive_topology = pipeline.topology;

        self.active_bindpoint = BindPoint::Graphics { internal: false };
        self.gr_pipeline.pipeline = Some((pipeline.raw, Arc::clone(&pipeline.shared)));
        self.vertex_bindings_remap = pipeline.vertex_bindings;

        self.set_vertex_buffers();

        if let Some(ref vp) = pipeline.baked_states.viewport {
            self.set_viewports(0, iter::once(vp.clone()));
        }
        if let Some(ref rect) = pipeline.baked_states.scissor {
            self.set_scissors(0, iter::once(rect.clone()));
        }
        if let Some(color) = pipeline.baked_states.blend_constants {
            self.set_blend_constants(color);
        }
        if let Some(ref bounds) = pipeline.baked_states.depth_bounds {
            self.set_depth_bounds(bounds.clone());
        }
    }

    unsafe fn bind_graphics_descriptor_sets<'a, I, J>(
        &mut self,
        layout: &r::PipelineLayout,
        first_set: usize,
        sets: I,
        offsets: J,
    ) where
        I: Iterator<Item = &'a r::DescriptorSet>,
        J: Iterator<Item = com::DescriptorSetOffset>,
    {
        let set_array = sets.collect::<ArrayVec<[_; MAX_DESCRIPTOR_SETS]>>();
        self.active_descriptor_heaps = self
            .gr_pipeline
            .bind_descriptor_sets(layout, first_set, &set_array, offsets);
        self.bind_descriptor_heaps();

        for (i, set) in set_array.into_iter().enumerate() {
            self.mark_bound_descriptor(first_set + i, set);
        }
    }

    unsafe fn bind_compute_pipeline(&mut self, pipeline: &r::ComputePipeline) {
        match self.comp_pipeline.pipeline {
            Some((_, ref shared)) if Arc::ptr_eq(shared, &pipeline.shared) => {
                // Same root signature, nothing to do
            }
            _ => {
                self.raw
                    .set_compute_root_signature(pipeline.shared.signature);
                // All slots need to be rebound internally on signature change.
                self.comp_pipeline.user_data.dirty_all();
            }
        }
        self.raw.set_pipeline_state(pipeline.raw);

        self.active_bindpoint = BindPoint::Compute;
        self.comp_pipeline.pipeline = Some((pipeline.raw, Arc::clone(&pipeline.shared)));
    }

    unsafe fn bind_compute_descriptor_sets<'a, I, J>(
        &mut self,
        layout: &r::PipelineLayout,
        first_set: usize,
        sets: I,
        offsets: J,
    ) where
        I: Iterator<Item = &'a r::DescriptorSet>,
        J: Iterator<Item = com::DescriptorSetOffset>,
    {
        let set_array = sets.collect::<ArrayVec<[_; MAX_DESCRIPTOR_SETS]>>();
        self.active_descriptor_heaps = self
            .comp_pipeline
            .bind_descriptor_sets(layout, first_set, &set_array, offsets);
        self.bind_descriptor_heaps();

        for (i, set) in set_array.into_iter().enumerate() {
            self.mark_bound_descriptor(first_set + i, set);
        }
    }

    unsafe fn dispatch(&mut self, count: WorkGroupCount) {
        self.set_compute_bind_point();
        self.raw.dispatch(count);
    }

    unsafe fn dispatch_indirect(&mut self, buffer: &r::Buffer, offset: buffer::Offset) {
        let buffer = buffer.expect_bound();
        self.set_compute_bind_point();
        self.raw.ExecuteIndirect(
            self.shared.signatures.dispatch.as_mut_ptr(),
            1,
            buffer.resource.as_mut_ptr(),
            offset,
            ptr::null_mut(),
            0,
        );
    }

    unsafe fn fill_buffer(&mut self, buffer: &r::Buffer, range: buffer::SubRange, data: u32) {
        let buffer = buffer.expect_bound();
        let bytes_per_unit = 4;
        let start = range.offset as i32;
        let end = range
            .size
            .map_or(buffer.requirements.size, |s| range.offset + s) as i32;
        if start % 4 != 0 || end % 4 != 0 {
            warn!("Fill buffer bounds have to be multiples of 4");
        }
        let rect = d3d12::D3D12_RECT {
            left: start / bytes_per_unit,
            top: 0,
            right: end / bytes_per_unit,
            bottom: 1,
        };

        let (pre_barrier, post_barrier) = Self::dual_transition_barriers(
            buffer.resource,
            d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            d3d12::D3D12_RESOURCE_STATE_COPY_DEST..d3d12::D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        );
        self.raw.ResourceBarrier(1, &pre_barrier);

        // Descriptor heap for the current blit, only storing the src image
        let device = self.shared.service_pipes.device.clone();
        let (uav_heap, _) = device.create_descriptor_heap(
            1,
            native::DescriptorHeapType::CbvSrvUav,
            native::DescriptorHeapFlags::SHADER_VISIBLE,
            0,
        );
        let mut uav_desc = d3d12::D3D12_UNORDERED_ACCESS_VIEW_DESC {
            Format: dxgiformat::DXGI_FORMAT_R32_TYPELESS,
            ViewDimension: d3d12::D3D12_UAV_DIMENSION_BUFFER,
            u: mem::zeroed(),
        };
        *uav_desc.u.Buffer_mut() = d3d12::D3D12_BUFFER_UAV {
            FirstElement: 0,
            NumElements: (buffer.requirements.size / bytes_per_unit as u64) as u32,
            StructureByteStride: 0,
            CounterOffsetInBytes: 0,
            Flags: d3d12::D3D12_BUFFER_UAV_FLAG_RAW,
        };
        device.CreateUnorderedAccessView(
            buffer.resource.as_mut_ptr(),
            ptr::null_mut(),
            &uav_desc,
            uav_heap.start_cpu_descriptor(),
        );
        self.raw.set_descriptor_heaps(&[uav_heap]);
        self.temporary_gpu_heaps.push(uav_heap);

        let cpu_descriptor = buffer
            .clear_uav
            .expect("Buffer needs to be created with usage `TRANSFER_DST`");

        self.raw.ClearUnorderedAccessViewUint(
            uav_heap.start_gpu_descriptor(),
            cpu_descriptor.raw,
            buffer.resource.as_mut_ptr(),
            &[data; 4],
            1,
            &rect as *const _,
        );

        self.raw.ResourceBarrier(1, &post_barrier);
    }

    unsafe fn update_buffer(&mut self, _buffer: &r::Buffer, _offset: buffer::Offset, _data: &[u8]) {
        unimplemented!()
    }

    unsafe fn copy_buffer<T>(&mut self, src: &r::Buffer, dst: &r::Buffer, regions: T)
    where
        T: Iterator<Item = com::BufferCopy>,
    {
        let src = src.expect_bound();
        let dst = dst.expect_bound();

        /*
        let (pre_barrier, post_barrier) = Self::dual_transition_barriers(
            dst.resource,
            d3d12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            d3d12::D3D12_RESOURCE_STATE_COPY_DEST,
        );
        self.raw.ResourceBarrier(1, &pre_barrier);
        */

        // TODO: Optimization: Copy whole resource if possible
        // copy each region
        for region in regions {
            self.raw.CopyBufferRegion(
                dst.resource.as_mut_ptr(),
                region.dst as _,
                src.resource.as_mut_ptr(),
                region.src as _,
                region.size as _,
            );
        }

        //self.raw.ResourceBarrier(1, &post_barrier);
    }

    unsafe fn copy_image<T>(
        &mut self,
        src: &r::Image,
        _: image::Layout,
        dst: &r::Image,
        _: image::Layout,
        regions: T,
    ) where
        T: Iterator<Item = com::ImageCopy>,
    {
        let src = src.expect_bound();
        let dst = dst.expect_bound();
        let mut src_image = d3d12::D3D12_TEXTURE_COPY_LOCATION {
            pResource: src.resource.as_mut_ptr(),
            Type: d3d12::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            u: mem::zeroed(),
        };
        let mut dst_image = d3d12::D3D12_TEXTURE_COPY_LOCATION {
            pResource: dst.resource.as_mut_ptr(),
            Type: d3d12::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            u: mem::zeroed(),
        };

        let device = self.shared.service_pipes.device.clone();
        let src_desc = src.surface_type.desc();
        let dst_desc = dst.surface_type.desc();
        assert_eq!(src_desc.bits, dst_desc.bits);
        //Note: Direct3D 10.1 enables copies between prestructured-typed textures
        // and block-compressed textures of the same bit widths.
        let do_alias = src.surface_type != dst.surface_type
            && src_desc.is_compressed() == dst_desc.is_compressed();

        if do_alias {
            // D3D12 only permits changing the channel type for copies,
            // similarly to how it allows the views to be created.

            // create an aliased resource to the source
            let mut alias = native::Resource::null();
            let desc = d3d12::D3D12_RESOURCE_DESC {
                Format: dst.descriptor.Format,
                ..src.descriptor.clone()
            };
            let (heap_ptr, heap_offset) = match src.place {
                r::Place::Heap { raw, offset } => (raw.as_mut_ptr(), offset),
                r::Place::Swapchain {} => {
                    error!("Unable to copy swapchain image, skipping");
                    return;
                }
            };
            assert_eq!(
                winerror::S_OK,
                device.CreatePlacedResource(
                    heap_ptr,
                    heap_offset,
                    &desc,
                    d3d12::D3D12_RESOURCE_STATE_COMMON,
                    ptr::null(),
                    &d3d12::ID3D12Resource::uuidof(),
                    alias.mut_void(),
                )
            );
            src_image.pResource = alias.as_mut_ptr();
            self.retained_resources.push(alias);

            // signal the aliasing transition
            let sub_barrier = d3d12::D3D12_RESOURCE_ALIASING_BARRIER {
                pResourceBefore: src.resource.as_mut_ptr(),
                pResourceAfter: src_image.pResource,
            };
            let mut barrier = d3d12::D3D12_RESOURCE_BARRIER {
                Type: d3d12::D3D12_RESOURCE_BARRIER_TYPE_ALIASING,
                Flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_NONE,
                u: mem::zeroed(),
            };
            *barrier.u.Aliasing_mut() = sub_barrier;
            self.raw.ResourceBarrier(1, &barrier as *const _);
        }

        for r in regions {
            debug_assert_eq!(
                r.src_subresource.layers.len(),
                r.dst_subresource.layers.len()
            );
            let src_box = d3d12::D3D12_BOX {
                left: r.src_offset.x as _,
                top: r.src_offset.y as _,
                right: (r.src_offset.x + r.extent.width as i32) as _,
                bottom: (r.src_offset.y + r.extent.height as i32) as _,
                front: r.src_offset.z as _,
                back: (r.src_offset.z + r.extent.depth as i32) as _,
            };

            for (src_layer, dst_layer) in r
                .src_subresource
                .layers
                .clone()
                .zip(r.dst_subresource.layers.clone())
            {
                *src_image.u.SubresourceIndex_mut() =
                    src.calc_subresource(r.src_subresource.level as _, src_layer as _, 0);
                *dst_image.u.SubresourceIndex_mut() =
                    dst.calc_subresource(r.dst_subresource.level as _, dst_layer as _, 0);
                self.raw.CopyTextureRegion(
                    &dst_image,
                    r.dst_offset.x as _,
                    r.dst_offset.y as _,
                    r.dst_offset.z as _,
                    &src_image,
                    &src_box,
                );
            }
        }

        if do_alias {
            // signal the aliasing transition - back to the original
            let sub_barrier = d3d12::D3D12_RESOURCE_ALIASING_BARRIER {
                pResourceBefore: src_image.pResource,
                pResourceAfter: src.resource.as_mut_ptr(),
            };
            let mut barrier = d3d12::D3D12_RESOURCE_BARRIER {
                Type: d3d12::D3D12_RESOURCE_BARRIER_TYPE_ALIASING,
                Flags: d3d12::D3D12_RESOURCE_BARRIER_FLAG_NONE,
                u: mem::zeroed(),
            };
            *barrier.u.Aliasing_mut() = sub_barrier;
            self.raw.ResourceBarrier(1, &barrier as *const _);
        }
    }

    unsafe fn copy_buffer_to_image<T>(
        &mut self,
        buffer: &r::Buffer,
        image: &r::Image,
        _: image::Layout,
        regions: T,
    ) where
        T: Iterator<Item = com::BufferImageCopy>,
    {
        let buffer = buffer.expect_bound();
        let image = image.expect_bound();
        assert!(self.copies.is_empty());

        for r in regions {
            Self::split_buffer_copy(&mut self.copies, r, image);
        }

        if self.copies.is_empty() {
            return;
        }

        let mut src = d3d12::D3D12_TEXTURE_COPY_LOCATION {
            pResource: buffer.resource.as_mut_ptr(),
            Type: d3d12::D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            u: mem::zeroed(),
        };
        let mut dst = d3d12::D3D12_TEXTURE_COPY_LOCATION {
            pResource: image.resource.as_mut_ptr(),
            Type: d3d12::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            u: mem::zeroed(),
        };

        for c in self.copies.drain(..) {
            let src_box = d3d12::D3D12_BOX {
                left: c.buf_offset.x as u32,
                top: c.buf_offset.y as u32,
                right: c.buf_offset.x as u32 + c.copy_extent.width,
                bottom: c.buf_offset.y as u32 + c.copy_extent.height,
                front: c.buf_offset.z as u32,
                back: c.buf_offset.z as u32 + c.copy_extent.depth,
            };
            let footprint = d3d12::D3D12_PLACED_SUBRESOURCE_FOOTPRINT {
                Offset: c.footprint_offset,
                Footprint: d3d12::D3D12_SUBRESOURCE_FOOTPRINT {
                    Format: image.descriptor.Format,
                    Width: c.footprint.width,
                    Height: c.footprint.height,
                    Depth: c.footprint.depth,
                    RowPitch: c.row_pitch,
                },
            };
            *src.u.PlacedFootprint_mut() = footprint;
            *dst.u.SubresourceIndex_mut() = c.img_subresource;
            self.raw.CopyTextureRegion(
                &dst,
                c.img_offset.x as _,
                c.img_offset.y as _,
                c.img_offset.z as _,
                &src,
                &src_box,
            );
        }
    }

    unsafe fn copy_image_to_buffer<T>(
        &mut self,
        image: &r::Image,
        _: image::Layout,
        buffer: &r::Buffer,
        regions: T,
    ) where
        T: Iterator<Item = com::BufferImageCopy>,
    {
        let image = image.expect_bound();
        let buffer = buffer.expect_bound();
        assert!(self.copies.is_empty());

        for r in regions {
            Self::split_buffer_copy(&mut self.copies, r, image);
        }

        if self.copies.is_empty() {
            return;
        }

        let mut src = d3d12::D3D12_TEXTURE_COPY_LOCATION {
            pResource: image.resource.as_mut_ptr(),
            Type: d3d12::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            u: mem::zeroed(),
        };
        let mut dst = d3d12::D3D12_TEXTURE_COPY_LOCATION {
            pResource: buffer.resource.as_mut_ptr(),
            Type: d3d12::D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            u: mem::zeroed(),
        };

        for c in self.copies.drain(..) {
            let src_box = d3d12::D3D12_BOX {
                left: c.img_offset.x as u32,
                top: c.img_offset.y as u32,
                right: c.img_offset.x as u32 + c.copy_extent.width,
                bottom: c.img_offset.y as u32 + c.copy_extent.height,
                front: c.img_offset.z as u32,
                back: c.img_offset.z as u32 + c.copy_extent.depth,
            };
            let footprint = d3d12::D3D12_PLACED_SUBRESOURCE_FOOTPRINT {
                Offset: c.footprint_offset,
                Footprint: d3d12::D3D12_SUBRESOURCE_FOOTPRINT {
                    Format: image.descriptor.Format,
                    Width: c.footprint.width,
                    Height: c.footprint.height,
                    Depth: c.footprint.depth,
                    RowPitch: c.row_pitch,
                },
            };
            *dst.u.PlacedFootprint_mut() = footprint;
            *src.u.SubresourceIndex_mut() = c.img_subresource;
            self.raw.CopyTextureRegion(
                &dst,
                c.buf_offset.x as _,
                c.buf_offset.y as _,
                c.buf_offset.z as _,
                &src,
                &src_box,
            );
        }
    }

    unsafe fn draw(&mut self, vertices: Range<VertexCount>, instances: Range<InstanceCount>) {
        self.set_graphics_bind_point();
        self.raw.draw(
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
        self.set_graphics_bind_point();
        self.raw.draw_indexed(
            indices.end - indices.start,
            instances.end - instances.start,
            indices.start,
            base_vertex,
            instances.start,
        );
    }

    unsafe fn draw_indirect(
        &mut self,
        buffer: &r::Buffer,
        offset: buffer::Offset,
        draw_count: DrawCount,
        stride: buffer::Stride,
    ) {
        assert_eq!(stride, 16);
        let buffer = buffer.expect_bound();
        self.set_graphics_bind_point();
        self.raw.ExecuteIndirect(
            self.shared.signatures.draw.as_mut_ptr(),
            draw_count,
            buffer.resource.as_mut_ptr(),
            offset,
            ptr::null_mut(),
            0,
        );
    }

    unsafe fn draw_indexed_indirect(
        &mut self,
        buffer: &r::Buffer,
        offset: buffer::Offset,
        draw_count: DrawCount,
        stride: buffer::Stride,
    ) {
        assert_eq!(stride, 20);
        let buffer = buffer.expect_bound();
        self.set_graphics_bind_point();
        self.raw.ExecuteIndirect(
            self.shared.signatures.draw_indexed.as_mut_ptr(),
            draw_count,
            buffer.resource.as_mut_ptr(),
            offset,
            ptr::null_mut(),
            0,
        );
    }

    unsafe fn draw_mesh_tasks(&mut self, _: TaskCount, _: TaskCount) {
        unimplemented!()
    }

    unsafe fn draw_mesh_tasks_indirect(
        &mut self,
        _: &r::Buffer,
        _: buffer::Offset,
        _: hal::DrawCount,
        _: buffer::Stride,
    ) {
        unimplemented!()
    }

    unsafe fn draw_mesh_tasks_indirect_count(
        &mut self,
        _: &r::Buffer,
        _: buffer::Offset,
        _: &r::Buffer,
        _: buffer::Offset,
        _: DrawCount,
        _: buffer::Stride,
    ) {
        unimplemented!()
    }

    unsafe fn draw_indirect_count(
        &mut self,
        buffer: &r::Buffer,
        offset: buffer::Offset,
        count_buffer: &r::Buffer,
        count_buffer_offset: buffer::Offset,
        max_draw_count: DrawCount,
        stride: buffer::Stride,
    ) {
        assert_eq!(stride, 16);
        let buffer = buffer.expect_bound();
        let count_buffer = count_buffer.expect_bound();
        self.set_graphics_bind_point();
        self.raw.ExecuteIndirect(
            self.shared.signatures.draw.as_mut_ptr(),
            max_draw_count,
            buffer.resource.as_mut_ptr(),
            offset,
            count_buffer.resource.as_mut_ptr(),
            count_buffer_offset,
        );
    }

    unsafe fn draw_indexed_indirect_count(
        &mut self,
        buffer: &r::Buffer,
        offset: buffer::Offset,
        count_buffer: &r::Buffer,
        count_buffer_offset: buffer::Offset,
        max_draw_count: DrawCount,
        stride: buffer::Stride,
    ) {
        assert_eq!(stride, 20);
        let buffer = buffer.expect_bound();
        let count_buffer = count_buffer.expect_bound();
        self.set_graphics_bind_point();
        self.raw.ExecuteIndirect(
            self.shared.signatures.draw_indexed.as_mut_ptr(),
            max_draw_count,
            buffer.resource.as_mut_ptr(),
            offset,
            count_buffer.resource.as_mut_ptr(),
            count_buffer_offset,
        );
    }

    unsafe fn set_event(&mut self, _: &(), _: pso::PipelineStage) {
        unimplemented!()
    }

    unsafe fn reset_event(&mut self, _: &(), _: pso::PipelineStage) {
        unimplemented!()
    }

    unsafe fn wait_events<'a, I, J>(&mut self, _: I, _: Range<pso::PipelineStage>, _: J)
    where
        I: Iterator<Item = &'a ()>,
        J: Iterator<Item = memory::Barrier<'a, Backend>>,
    {
        unimplemented!()
    }

    unsafe fn begin_query(&mut self, query: query::Query<Backend>, flags: query::ControlFlags) {
        let query_ty = match query.pool.ty {
            query::Type::Occlusion => {
                if flags.contains(query::ControlFlags::PRECISE) {
                    self.occlusion_query = Some(OcclusionQuery::Precise(query.id));
                    d3d12::D3D12_QUERY_TYPE_OCCLUSION
                } else {
                    // Default to binary occlusion as it might be faster due to early depth/stencil
                    // tests.
                    self.occlusion_query = Some(OcclusionQuery::Binary(query.id));
                    d3d12::D3D12_QUERY_TYPE_BINARY_OCCLUSION
                }
            }
            query::Type::Timestamp => panic!("Timestap queries are issued via "),
            query::Type::PipelineStatistics(_) => {
                self.pipeline_stats_query = Some(query.id);
                d3d12::D3D12_QUERY_TYPE_PIPELINE_STATISTICS
            }
        };

        self.raw
            .BeginQuery(query.pool.raw.as_mut_ptr(), query_ty, query.id);
    }

    unsafe fn end_query(&mut self, query: query::Query<Backend>) {
        let id = query.id;
        let query_ty = match query.pool.ty {
            query::Type::Occlusion if self.occlusion_query == Some(OcclusionQuery::Precise(id)) => {
                self.occlusion_query = None;
                d3d12::D3D12_QUERY_TYPE_OCCLUSION
            }
            query::Type::Occlusion if self.occlusion_query == Some(OcclusionQuery::Binary(id)) => {
                self.occlusion_query = None;
                d3d12::D3D12_QUERY_TYPE_BINARY_OCCLUSION
            }
            query::Type::PipelineStatistics(_) if self.pipeline_stats_query == Some(id) => {
                self.pipeline_stats_query = None;
                d3d12::D3D12_QUERY_TYPE_PIPELINE_STATISTICS
            }
            _ => panic!("Missing `begin_query` call for query: {:?}", query),
        };

        self.raw.EndQuery(query.pool.raw.as_mut_ptr(), query_ty, id);
    }

    unsafe fn reset_query_pool(&mut self, _pool: &r::QueryPool, _queries: Range<query::Id>) {
        // Nothing to do here
        // vkCmdResetQueryPool sets the queries to `unavailable` but the specification
        // doesn't state an affect on the `active` state. Every queries at the end of the command
        // buffer must be made inactive, which can only be done with EndQuery.
        // Therefore, every `begin_query` must follow a `end_query` state, the resulting values
        // after calling are undefined.
    }

    unsafe fn copy_query_pool_results(
        &mut self,
        _pool: &r::QueryPool,
        _queries: Range<query::Id>,
        _buffer: &r::Buffer,
        _offset: buffer::Offset,
        _stride: buffer::Stride,
        _flags: query::ResultFlags,
    ) {
        unimplemented!()
    }

    unsafe fn write_timestamp(&mut self, _: pso::PipelineStage, query: query::Query<Backend>) {
        self.raw.EndQuery(
            query.pool.raw.as_mut_ptr(),
            d3d12::D3D12_QUERY_TYPE_TIMESTAMP,
            query.id,
        );
    }

    unsafe fn push_graphics_constants(
        &mut self,
        _layout: &r::PipelineLayout,
        _stages: pso::ShaderStageFlags,
        offset: u32,
        constants: &[u32],
    ) {
        assert!(offset % 4 == 0);
        self.gr_pipeline
            .user_data
            .set_constants(offset as usize / 4, constants);
    }

    unsafe fn push_compute_constants(
        &mut self,
        _layout: &r::PipelineLayout,
        offset: u32,
        constants: &[u32],
    ) {
        assert!(offset % 4 == 0);
        self.comp_pipeline
            .user_data
            .set_constants(offset as usize / 4, constants);
    }

    unsafe fn execute_commands<'a, T>(&mut self, cmd_buffers: T)
    where
        T: Iterator<Item = &'a CommandBuffer>,
    {
        for _cmd_buf in cmd_buffers {
            error!("TODO: execute_commands");
        }
    }

    unsafe fn insert_debug_marker(&mut self, name: &str, _color: u32) {
        let (ptr, size) = self.fill_marker(name);
        self.raw.SetMarker(0, ptr, size);
    }
    unsafe fn begin_debug_marker(&mut self, name: &str, _color: u32) {
        let (ptr, size) = self.fill_marker(name);
        self.raw.BeginEvent(0, ptr, size);
    }
    unsafe fn end_debug_marker(&mut self) {
        self.raw.EndEvent();
    }
}
