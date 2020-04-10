use crate::{
    conversions as conv,
    internal::{BlitVertex, ClearKey, ClearVertex},
    native,
    soft,
    window,
    AsNative,
    Backend,
    BufferPtr,
    OnlineRecording,
    PrivateDisabilities,
    ResourceIndex,
    ResourcePtr,
    SamplerPtr,
    Shared,
    TexturePtr,
    MAX_BOUND_DESCRIPTOR_SETS,
};

use hal::{
    buffer,
    command as com,
    device::OutOfMemory,
    format::{Aspects, FormatDesc},
    image::{Extent, Filter, Layout, Level, SubresourceRange},
    memory,
    pass::AttachmentLoadOp,
    pso,
    query,
    range::RangeArg,
    window::{PresentError, Suboptimal, SwapImageIndex},
    DrawCount,
    IndexCount,
    IndexType,
    InstanceCount,
    VertexCount,
    VertexOffset,
    WorkGroupCount,
};

use arrayvec::ArrayVec;
use auxil::FastHashMap;
use block::ConcreteBlock;
use cocoa::foundation::{NSRange, NSUInteger};
use copyless::VecHelper;
#[cfg(feature = "dispatch")]
use dispatch;
use foreign_types::ForeignType;
use metal::{self, MTLIndexType, MTLPrimitiveType, MTLScissorRect, MTLSize, MTLViewport};
use objc::rc::autoreleasepool;
use parking_lot::Mutex;

#[cfg(feature = "dispatch")]
use std::fmt;
use std::{
    borrow::Borrow,
    cell::RefCell,
    iter,
    mem,
    ops::{Deref, Range},
    ptr,
    slice,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
    thread,
    time,
};


const WORD_SIZE: usize = 4;
const WORD_ALIGNMENT: u64 = WORD_SIZE as _;
/// Number of frames to average when reporting the performance counters.
const COUNTERS_REPORT_WINDOW: usize = 0;

#[cfg(feature = "dispatch")]
struct NoDebug<T>(T);
#[cfg(feature = "dispatch")]
impl<T> fmt::Debug for NoDebug<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "<hidden>")
    }
}

#[derive(Debug)]
pub struct QueueInner {
    raw: metal::CommandQueue,
    reserve: Range<usize>,
    debug_retain_references: bool,
}

#[must_use]
#[derive(Debug)]
pub struct Token {
    active: bool,
}

impl Drop for Token {
    fn drop(&mut self) {
        // poor man's linear type...
        if !thread::panicking() {
            debug_assert!(!self.active);
        }
    }
}

impl QueueInner {
    pub(crate) fn new(device: &metal::DeviceRef, pool_size: Option<usize>) -> Self {
        match pool_size {
            Some(count) => QueueInner {
                raw: device.new_command_queue_with_max_command_buffer_count(count as u64),
                reserve: 0 .. count,
                debug_retain_references: false,
            },
            None => QueueInner {
                raw: device.new_command_queue(),
                reserve: 0 .. 64,
                debug_retain_references: true,
            },
        }
    }

    /// Spawns a command buffer from a virtual pool.
    pub(crate) fn spawn(&mut self) -> (metal::CommandBuffer, Token) {
        self.reserve.start += 1;
        let cmd_buf = autoreleasepool(|| self.spawn_temp().to_owned());
        (cmd_buf, Token { active: true })
    }

    pub(crate) fn spawn_temp(&self) -> &metal::CommandBufferRef {
        if self.debug_retain_references {
            self.raw.new_command_buffer()
        } else {
            self.raw.new_command_buffer_with_unretained_references()
        }
    }

    /// Returns a command buffer to a virtual pool.
    pub(crate) fn release(&mut self, mut token: Token) {
        token.active = false;
        self.reserve.start -= 1;
    }

    /// Block until GPU is idle.
    pub(crate) fn wait_idle(queue: &Mutex<Self>) {
        debug!("waiting for idle");
        // note: we deliberately don't hold the Mutex lock while waiting,
        // since the completion handlers need to access it.
        let (cmd_buf, token) = queue.lock().spawn();
        cmd_buf.set_label("empty");
        cmd_buf.commit();
        cmd_buf.wait_until_completed();
        queue.lock().release(token);
    }
}

#[derive(Debug)]
pub struct BlockedSubmission {
    wait_events: Vec<Arc<AtomicBool>>,
    command_buffers: Vec<metal::CommandBuffer>,
}

/// Class responsible for keeping the state of submissions between the
/// requested user submission that is blocked by a host event, and
/// setting the event itself on the host.
#[derive(Debug, Default)]
pub struct QueueBlocker {
    submissions: Vec<BlockedSubmission>,
}

impl QueueBlocker {
    fn submit_impl(&mut self, cmd_buffer: &metal::CommandBufferRef) {
        match self.submissions.last_mut() {
            Some(blocked) => blocked.command_buffers.push(cmd_buffer.to_owned()),
            None => cmd_buffer.commit(),
        }
    }

    pub(crate) fn triage(&mut self) {
        // clean up the relevant blocks
        let done = {
            let blocked = match self.submissions.first_mut() {
                Some(blocked) => blocked,
                None => return,
            };
            blocked.wait_events.retain(|ev| !ev.load(Ordering::Acquire));
            blocked.wait_events.is_empty()
        };

        // execute unblocked command buffers
        if done {
            let blocked = self.submissions.remove(0);
            for cmd_buf in blocked.command_buffers {
                cmd_buf.commit();
            }
        }
    }
}


#[derive(Debug)]
struct PoolShared {
    online_recording: OnlineRecording,
    #[cfg(feature = "dispatch")]
    dispatch_queue: Option<NoDebug<dispatch::Queue>>,
}

type CommandBufferInnerPtr = Arc<RefCell<CommandBufferInner>>;
type PoolSharedPtr = Arc<RefCell<PoolShared>>;

#[derive(Debug)]
pub struct CommandPool {
    shared: Arc<Shared>,
    allocated: Vec<CommandBufferInnerPtr>,
    pool_shared: PoolSharedPtr,
}

unsafe impl Send for CommandPool {}
unsafe impl Sync for CommandPool {}

impl CommandPool {
    pub(crate) fn new(shared: &Arc<Shared>, online_recording: OnlineRecording) -> Self {
        let pool_shared = PoolShared {
            #[cfg(feature = "dispatch")]
            dispatch_queue: match online_recording {
                OnlineRecording::Immediate | OnlineRecording::Deferred => None,
                OnlineRecording::Remote(ref priority) => {
                    Some(NoDebug(dispatch::Queue::global(priority.clone())))
                }
            },
            online_recording,
        };
        CommandPool {
            shared: Arc::clone(shared),
            allocated: Vec::new(),
            pool_shared: Arc::new(RefCell::new(pool_shared)),
        }
    }
}

#[derive(Debug)]
pub struct CommandBuffer {
    shared: Arc<Shared>,
    pool_shared: PoolSharedPtr,
    inner: CommandBufferInnerPtr,
    state: State,
    temp: Temp,
    pub name: String,
}

unsafe impl Send for CommandBuffer {}
unsafe impl Sync for CommandBuffer {}

#[derive(Debug)]
struct Temp {
    clear_vertices: Vec<ClearVertex>,
    blit_vertices: FastHashMap<(Aspects, Level), Vec<BlitVertex>>,
    clear_values: Vec<Option<com::ClearValue>>,
}

type VertexBufferMaybeVec = Vec<Option<(pso::VertexBufferDesc, pso::ElemOffset)>>;

#[derive(Debug)]
struct RenderPipelineState {
    raw: metal::RenderPipelineState,
    ds_desc: pso::DepthStencilDesc,
    vertex_buffers: VertexBufferMaybeVec,
    formats: native::SubpassFormats,
}

#[derive(Debug)]
struct SubpassInfo {
    descriptor: metal::RenderPassDescriptor,
    combined_aspects: Aspects,
    formats: native::SubpassFormats,
}

#[derive(Debug, Default)]
struct DescriptorSetInfo {
    graphics_resources: Vec<(ResourcePtr, metal::MTLResourceUsage)>,
    compute_resources: Vec<(ResourcePtr, metal::MTLResourceUsage)>,
}

/// The current state of a command buffer, used for two distinct purposes:
///   1. inherit resource bindings between passes
///   2. avoid redundant state settings
///
/// ## Spaces
/// Note that these two usages are distinct and operate in technically different
/// spaces (1 - Vulkan, 2 - Metal), so be careful not to confuse them.
/// For example, Vulkan spaces are `pending_subpasses`, `rasterizer_state`, `target_*`.
/// While Metal spaces are `resources_*`.
///
/// ## Vertex buffers
/// You may notice that vertex buffers are stored in two separate places: per pipeline, and
/// here in the state. These can't be merged together easily because at binding time we
/// want one input vertex buffer to potentially be bound to multiple entry points....
#[derive(Debug)]
struct State {
    // Note: this could be `MTLViewport` but we have to patch the depth separately.
    viewport: Option<(pso::Rect, Range<f32>)>,
    scissors: Option<MTLScissorRect>,
    blend_color: Option<pso::ColorValue>,
    //TODO: move some of that state out, to avoid redundant allocations
    render_pso: Option<RenderPipelineState>,
    /// A flag to handle edge cases of Vulkan binding inheritance:
    /// we don't want to consider the current PSO bound for a new pass if it's not compatible.
    render_pso_is_compatible: bool,
    compute_pso: Option<metal::ComputePipelineState>,
    work_group_size: MTLSize,
    primitive_type: MTLPrimitiveType,
    //TODO: move Metal-side state into a separate struct
    resources_vs: StageResources,
    resources_ps: StageResources,
    resources_cs: StageResources,
    index_buffer: Option<IndexBuffer<BufferPtr>>,
    rasterizer_state: Option<native::RasterizerState>,
    depth_bias: pso::DepthBias,
    stencil: native::StencilState<pso::StencilValue>,
    push_constants: Vec<u32>,
    vertex_buffers: Vec<Option<(BufferPtr, u64)>>,
    ///TODO: add a structure to store render target state
    target_aspects: Aspects,
    target_extent: Extent,
    target_formats: native::SubpassFormats,
    visibility_query: (metal::MTLVisibilityResultMode, buffer::Offset),
    pending_subpasses: Vec<SubpassInfo>,
    descriptor_sets: ArrayVec<[DescriptorSetInfo; MAX_BOUND_DESCRIPTOR_SETS]>,
}

impl State {
    /// Resets the current Metal side of the state tracking.
    fn reset_resources(&mut self) {
        self.resources_vs.clear();
        self.resources_ps.clear();
        self.resources_cs.clear();
        self.push_constants.clear();
        self.vertex_buffers.clear();
        self.pending_subpasses.clear();
        for ds in self.descriptor_sets.iter_mut() {
            ds.graphics_resources.clear();
            ds.compute_resources.clear();
        }
    }

    fn clamp_scissor(sr: MTLScissorRect, extent: Extent) -> MTLScissorRect {
        // sometimes there is not even an active render pass at this point
        let x = sr.x.min(extent.width.max(1) as u64 - 1);
        let y = sr.y.min(extent.height.max(1) as u64 - 1);
        //TODO: handle the zero scissor size sensibly
        MTLScissorRect {
            x,
            y,
            width: ((sr.x + sr.width).min(extent.width as u64) - x).max(1),
            height: ((sr.y + sr.height).min(extent.height as u64) - y).max(1),
        }
    }

    fn make_pso_commands(
        &self,
    ) -> (
        Option<soft::RenderCommand<&soft::Ref>>,
        Option<soft::RenderCommand<&soft::Ref>>,
    ) {
        if self.render_pso_is_compatible {
            (
                self.render_pso
                    .as_ref()
                    .map(|ps| soft::RenderCommand::BindPipeline(&*ps.raw)),
                self.rasterizer_state
                    .clone()
                    .map(soft::RenderCommand::SetRasterizerState),
            )
        } else {
            // Note: this is technically valid, we should not warn.
            (None, None)
        }
    }

    fn make_render_commands(
        &self,
        aspects: Aspects,
    ) -> impl Iterator<Item = soft::RenderCommand<&soft::Ref>> {
        // Apply previously bound values for this command buffer
        let com_vp = self
            .viewport
            .as_ref()
            .map(|&(rect, ref depth)| soft::RenderCommand::SetViewport(rect, depth.clone()));
        let com_scissor = self
            .scissors
            .map(|sr| soft::RenderCommand::SetScissor(Self::clamp_scissor(sr, self.target_extent)));
        let com_blend = if aspects.contains(Aspects::COLOR) {
            self.blend_color.map(soft::RenderCommand::SetBlendColor)
        } else {
            None
        };
        let com_depth_bias = if aspects.contains(Aspects::DEPTH) {
            Some(soft::RenderCommand::SetDepthBias(self.depth_bias))
        } else {
            None
        };
        let com_visibility = if self.visibility_query.0 != metal::MTLVisibilityResultMode::Disabled
        {
            Some(soft::RenderCommand::SetVisibilityResult(
                self.visibility_query.0,
                self.visibility_query.1,
            ))
        } else {
            None
        };
        let (com_pso, com_rast) = self.make_pso_commands();

        let render_resources = iter::once(&self.resources_vs).chain(iter::once(&self.resources_ps));
        let push_constants = self.push_constants.as_slice();
        let com_resources = [pso::Stage::Vertex, pso::Stage::Fragment]
            .iter()
            .zip(render_resources)
            .flat_map(move |(&stage, resources)| {
                let com_buffers = soft::RenderCommand::BindBuffers {
                    stage,
                    index: 0,
                    buffers: (&resources.buffers[..], &resources.buffer_offsets[..]),
                };
                let com_textures = soft::RenderCommand::BindTextures {
                    stage,
                    index: 0,
                    textures: &resources.textures[..],
                };
                let com_samplers = soft::RenderCommand::BindSamplers {
                    stage,
                    index: 0,
                    samplers: &resources.samplers[..],
                };
                let com_push_constants =
                    resources
                        .push_constants
                        .map(|pc| soft::RenderCommand::BindBufferData {
                            stage,
                            index: pc.buffer_index as _,
                            words: &push_constants[.. pc.count as usize],
                        });
                iter::once(com_buffers)
                    .chain(iter::once(com_textures))
                    .chain(iter::once(com_samplers))
                    .chain(com_push_constants)
            });
        let com_used_resources = self.descriptor_sets.iter().flat_map(|ds| {
            ds.graphics_resources
                .iter()
                .map(|&(resource, usage)| soft::RenderCommand::UseResource { resource, usage })
        });

        com_vp
            .into_iter()
            .chain(com_scissor)
            .chain(com_blend)
            .chain(com_depth_bias)
            .chain(com_visibility)
            .chain(com_pso)
            .chain(com_rast)
            //.chain(com_ds) // done outside
            .chain(com_resources)
            .chain(com_used_resources)
    }

    fn make_compute_commands(&self) -> impl Iterator<Item = soft::ComputeCommand<&soft::Ref>> {
        let resources = &self.resources_cs;
        let com_pso = self
            .compute_pso
            .as_ref()
            .map(|pso| soft::ComputeCommand::BindPipeline(&**pso));
        let com_buffers = soft::ComputeCommand::BindBuffers {
            index: 0,
            buffers: (&resources.buffers[..], &resources.buffer_offsets[..]),
        };
        let com_textures = soft::ComputeCommand::BindTextures {
            index: 0,
            textures: &resources.textures[..],
        };
        let com_samplers = soft::ComputeCommand::BindSamplers {
            index: 0,
            samplers: &resources.samplers[..],
        };
        let com_push_constants =
            resources
                .push_constants
                .map(|pc| soft::ComputeCommand::BindBufferData {
                    index: pc.buffer_index as _,
                    words: &self.push_constants[.. pc.count as usize],
                });
        let com_used_resources = self.descriptor_sets.iter().flat_map(|ds| {
            ds.compute_resources
                .iter()
                .map(|&(resource, usage)| soft::ComputeCommand::UseResource { resource, usage })
        });


        com_pso
            .into_iter()
            .chain(iter::once(com_buffers))
            .chain(iter::once(com_textures))
            .chain(iter::once(com_samplers))
            .chain(com_push_constants)
            .chain(com_used_resources)
    }

    fn set_vertex_buffers(&mut self, end: usize) -> Option<soft::RenderCommand<&soft::Ref>> {
        let rps = self.render_pso.as_ref()?;
        let start = end - rps.vertex_buffers.len();
        self.resources_vs.pre_allocate_buffers(end);

        for ((out_buffer, out_offset), vb_maybe) in self.resources_vs.buffers[.. end]
            .iter_mut()
            .rev()
            .zip(self.resources_vs.buffer_offsets[.. end].iter_mut().rev())
            .zip(&rps.vertex_buffers)
        {
            match vb_maybe {
                Some((ref vb, extra_offset)) => {
                    match self.vertex_buffers.get(vb.binding as usize) {
                        Some(&Some((buffer, base_offset))) => {
                            *out_buffer = Some(buffer);
                            *out_offset = *extra_offset as u64 + base_offset;
                        }
                        _ => {
                            // being unable to bind a buffer here is technically fine, since before this moment
                            // and actual rendering there might be more bind calls
                            *out_buffer = None;
                        }
                    }
                }
                None => {
                    *out_buffer = None;
                }
            }
        }

        Some(soft::RenderCommand::BindBuffers {
            stage: pso::Stage::Vertex,
            index: start as ResourceIndex,
            buffers: (
                &self.resources_vs.buffers[start .. end],
                &self.resources_vs.buffer_offsets[start .. end],
            ),
        })
    }

    fn build_depth_stencil(&self) -> Option<pso::DepthStencilDesc> {
        let mut desc = match self.render_pso {
            Some(ref ps) => ps.ds_desc,
            None => return None,
        };

        if !self.target_aspects.contains(Aspects::DEPTH) {
            desc.depth = None;
        }
        if !self.target_aspects.contains(Aspects::STENCIL) {
            desc.stencil = None;
        }

        if let Some(ref mut stencil) = desc.stencil {
            stencil.reference_values = pso::State::Dynamic;
            if stencil.read_masks.is_dynamic() {
                stencil.read_masks = pso::State::Static(self.stencil.read_masks);
            }
            if stencil.write_masks.is_dynamic() {
                stencil.write_masks = pso::State::Static(self.stencil.write_masks);
            }
        }

        Some(desc)
    }

    fn set_depth_bias<'a>(
        &mut self,
        depth_bias: &pso::DepthBias,
    ) -> soft::RenderCommand<&'a soft::Ref> {
        self.depth_bias = *depth_bias;
        soft::RenderCommand::SetDepthBias(*depth_bias)
    }

    fn push_vs_constants(
        &mut self,
        pc: native::PushConstantInfo,
    ) -> soft::RenderCommand<&soft::Ref> {
        self.resources_vs.push_constants = Some(pc);
        soft::RenderCommand::BindBufferData {
            stage: pso::Stage::Vertex,
            index: pc.buffer_index,
            words: &self.push_constants[.. pc.count as usize],
        }
    }

    fn push_ps_constants(
        &mut self,
        pc: native::PushConstantInfo,
    ) -> soft::RenderCommand<&soft::Ref> {
        self.resources_ps.push_constants = Some(pc);
        soft::RenderCommand::BindBufferData {
            stage: pso::Stage::Fragment,
            index: pc.buffer_index,
            words: &self.push_constants[.. pc.count as usize],
        }
    }

    fn push_cs_constants(
        &mut self,
        pc: native::PushConstantInfo,
    ) -> soft::ComputeCommand<&soft::Ref> {
        self.resources_cs.push_constants = Some(pc);
        soft::ComputeCommand::BindBufferData {
            index: pc.buffer_index,
            words: &self.push_constants[.. pc.count as usize],
        }
    }

    fn set_viewport<'a>(
        &mut self,
        vp: &'a pso::Viewport,
        disabilities: PrivateDisabilities,
    ) -> soft::RenderCommand<&'a soft::Ref> {
        let depth = vp.depth.start .. if disabilities.broken_viewport_near_depth {
            (vp.depth.end - vp.depth.start)
        } else {
            vp.depth.end
        };
        self.viewport = Some((vp.rect, depth.clone()));
        soft::RenderCommand::SetViewport(vp.rect, depth)
    }

    fn set_scissor<'a>(&mut self, rect: pso::Rect) -> soft::RenderCommand<&'a soft::Ref> {
        let scissor = MTLScissorRect {
            x: rect.x as _,
            y: rect.y as _,
            width: rect.w as _,
            height: rect.h as _,
        };
        self.scissors = Some(scissor);
        let clamped = State::clamp_scissor(scissor, self.target_extent);
        soft::RenderCommand::SetScissor(clamped)
    }

    fn set_blend_color<'a>(
        &mut self,
        color: &'a pso::ColorValue,
    ) -> soft::RenderCommand<&'a soft::Ref> {
        self.blend_color = Some(*color);
        soft::RenderCommand::SetBlendColor(*color)
    }

    fn update_push_constants(&mut self, offset: u32, constants: &[u32], total: u32) {
        assert_eq!(offset % WORD_ALIGNMENT as u32, 0);
        let offset = (offset / WORD_ALIGNMENT as u32) as usize;
        let data = &mut self.push_constants;
        if data.len() < total as usize {
            data.resize(total as usize, 0);
        }
        data[offset .. offset + constants.len()].copy_from_slice(constants);
    }

    fn set_visibility_query(
        &mut self,
        mode: metal::MTLVisibilityResultMode,
        offset: buffer::Offset,
    ) -> soft::RenderCommand<&soft::Ref> {
        self.visibility_query = (mode, offset);
        soft::RenderCommand::SetVisibilityResult(mode, offset)
    }
}

#[derive(Debug)]
struct StageResources {
    buffers: Vec<Option<BufferPtr>>,
    buffer_offsets: Vec<buffer::Offset>,
    textures: Vec<Option<TexturePtr>>,
    samplers: Vec<Option<SamplerPtr>>,
    push_constants: Option<native::PushConstantInfo>,
}

impl StageResources {
    fn new() -> Self {
        StageResources {
            buffers: Vec::new(),
            buffer_offsets: Vec::new(),
            textures: Vec::new(),
            samplers: Vec::new(),
            push_constants: None,
        }
    }

    fn clear(&mut self) {
        self.buffers.clear();
        self.buffer_offsets.clear();
        self.textures.clear();
        self.samplers.clear();
        self.push_constants = None;
    }

    fn pre_allocate_buffers(&mut self, count: usize) {
        debug_assert_eq!(self.buffers.len(), self.buffer_offsets.len());
        if self.buffers.len() < count {
            self.buffers.resize(count, None);
            self.buffer_offsets.resize(count, 0);
        }
    }
    fn pre_allocate(&mut self, counters: &native::ResourceData<ResourceIndex>) {
        if self.textures.len() < counters.textures as usize {
            self.textures.resize(counters.textures as usize, None);
        }
        if self.samplers.len() < counters.samplers as usize {
            self.samplers.resize(counters.samplers as usize, None);
        }
        self.pre_allocate_buffers(counters.buffers as usize);
    }

    fn bind_set(
        &mut self,
        stage: pso::ShaderStageFlags,
        data: &native::DescriptorEmulatedPoolInner,
        mut res_offset: native::ResourceData<ResourceIndex>,
        layouts: &[native::DescriptorLayout],
        pool_range: &native::ResourceData<Range<native::PoolResourceIndex>>,
    ) -> native::ResourceData<ResourceIndex> {
        let mut pool_offsets = pool_range.map(|r| r.start);
        for layout in layouts {
            if layout.stages.contains(stage) {
                if layout.content.contains(native::DescriptorContent::SAMPLER) {
                    self.samplers[res_offset.samplers as usize] =
                        data.samplers[pool_offsets.samplers as usize];
                    res_offset.samplers += 1;
                    pool_offsets.samplers += 1;
                }
                if layout.content.contains(native::DescriptorContent::TEXTURE) {
                    self.textures[res_offset.textures as usize] =
                        data.textures[pool_offsets.textures as usize].map(|(t, _)| t);
                    res_offset.textures += 1;
                    pool_offsets.textures += 1;
                }
                if layout.content.contains(native::DescriptorContent::BUFFER) {
                    let (buffer, offset) = match data.buffers[pool_offsets.buffers as usize] {
                        Some((buffer, offset)) => (Some(buffer), offset),
                        None => (None, 0),
                    };
                    self.buffers[res_offset.buffers as usize] = buffer;
                    self.buffer_offsets[res_offset.buffers as usize] = offset;
                    res_offset.buffers += 1;
                    pool_offsets.buffers += 1;
                }
            } else {
                pool_offsets.add(layout.content);
            }
        }
        res_offset
    }
}

#[cfg(feature = "dispatch")]
#[derive(Debug, Default)]
struct Capacity {
    render: usize,
    compute: usize,
    blit: usize,
}

//TODO: make sure to recycle the heap allocation of these commands.
#[cfg(feature = "dispatch")]
#[derive(Debug)]
enum EncodePass {
    Render(
        Vec<soft::RenderCommand<soft::Own>>,
        soft::Own,
        metal::RenderPassDescriptor,
        String,
    ),
    Compute(Vec<soft::ComputeCommand<soft::Own>>, soft::Own, String),
    Blit(Vec<soft::BlitCommand>, String),
}
#[cfg(feature = "dispatch")]
unsafe impl Send for EncodePass {}

#[cfg(feature = "dispatch")]
struct SharedCommandBuffer(Arc<Mutex<metal::CommandBuffer>>);
#[cfg(feature = "dispatch")]
unsafe impl Send for SharedCommandBuffer {}

#[cfg(feature = "dispatch")]
impl EncodePass {
    fn schedule(self, queue: &dispatch::Queue, cmd_buffer_arc: &Arc<Mutex<metal::CommandBuffer>>) {
        let cmd_buffer = SharedCommandBuffer(Arc::clone(cmd_buffer_arc));
        queue.r#async(move || match self {
            EncodePass::Render(list, resources, desc, label) => {
                let encoder = cmd_buffer
                    .0
                    .lock()
                    .new_render_command_encoder(&desc)
                    .to_owned();
                encoder.set_label(&label);
                for command in list {
                    exec_render(&encoder, command, &resources);
                }
                encoder.end_encoding();
            }
            EncodePass::Compute(list, resources, label) => {
                let encoder = cmd_buffer.0.lock().new_compute_command_encoder().to_owned();
                encoder.set_label(&label);
                for command in list {
                    exec_compute(&encoder, command, &resources);
                }
                encoder.end_encoding();
            }
            EncodePass::Blit(list, label) => {
                let encoder = cmd_buffer.0.lock().new_blit_command_encoder().to_owned();
                encoder.set_label(&label);
                for command in list {
                    exec_blit(&encoder, command);
                }
                encoder.end_encoding();
            }
        });
    }

    fn update(&self, capacity: &mut Capacity) {
        match &self {
            EncodePass::Render(ref list, _, _, _) => capacity.render = capacity.render.max(list.len()),
            EncodePass::Compute(ref list, _, _) => capacity.compute = capacity.compute.max(list.len()),
            EncodePass::Blit(ref list, _) => capacity.blit = capacity.blit.max(list.len()),
        }
    }
}

#[derive(Debug, Default)]
struct Journal {
    resources: soft::Own,
    passes: Vec<(soft::Pass, Range<usize>, String)>,
    render_commands: Vec<soft::RenderCommand<soft::Own>>,
    compute_commands: Vec<soft::ComputeCommand<soft::Own>>,
    blit_commands: Vec<soft::BlitCommand>,
}

impl Journal {
    fn clear(&mut self) {
        self.resources.clear();
        self.passes.clear();
        self.render_commands.clear();
        self.compute_commands.clear();
        self.blit_commands.clear();
    }

    fn stop(&mut self) {
        match self.passes.last_mut() {
            None => {}
            Some(&mut (soft::Pass::Render(_), ref mut range, _)) => {
                range.end = self.render_commands.len();
            }
            Some(&mut (soft::Pass::Compute, ref mut range, _)) => {
                range.end = self.compute_commands.len();
            }
            Some(&mut (soft::Pass::Blit, ref mut range, _)) => {
                range.end = self.blit_commands.len();
            }
        };
    }

    fn record(&self, command_buf: &metal::CommandBufferRef) {
        for (ref pass, ref range, ref label) in &self.passes {
            match *pass {
                soft::Pass::Render(ref desc) => {
                    let encoder = command_buf.new_render_command_encoder(desc);
                    encoder.set_label(label);
                    for command in &self.render_commands[range.clone()] {
                        exec_render(&encoder, command, &self.resources);
                    }
                    encoder.end_encoding();
                }
                soft::Pass::Blit => {
                    let encoder = command_buf.new_blit_command_encoder();
                    encoder.set_label(label);
                    for command in &self.blit_commands[range.clone()] {
                        exec_blit(&encoder, command);
                    }
                    encoder.end_encoding();
                }
                soft::Pass::Compute => {
                    let encoder = command_buf.new_compute_command_encoder();
                    encoder.set_label(label);
                    for command in &self.compute_commands[range.clone()] {
                        exec_compute(&encoder, command, &self.resources);
                    }
                    encoder.end_encoding();
                }
            }
        }
    }

    fn extend(&mut self, other: &Self, inherit_pass: bool) {
        if inherit_pass {
            assert_eq!(other.passes.len(), 1);
            match *self.passes.last_mut().unwrap() {
                (soft::Pass::Render(_), ref mut range, _) => {
                    range.end += other.render_commands.len();
                }
                (soft::Pass::Compute, _, _) | (soft::Pass::Blit, _, _) => {
                    panic!("Only render passes can inherit")
                }
            }
        } else {
            for (pass, range, label) in &other.passes {
                let offset = match *pass {
                    soft::Pass::Render(_) => self.render_commands.len(),
                    soft::Pass::Compute => self.compute_commands.len(),
                    soft::Pass::Blit => self.blit_commands.len(),
                };
                self.passes
                    .alloc()
                    .init((pass.clone(), range.start + offset .. range.end + offset, label.clone()));
            }
        }

        // Note: journals contain 3 levels of stuff:
        // resources, commands, and passes
        // Each upper level points to the lower one with index
        // sub-ranges. In order to merge two journals, we need
        // to fix those indices of the one that goes on top.
        // This is referred here as "rebasing".
        for mut com in other.render_commands.iter().cloned() {
            self.resources.rebase_render(&mut com);
            self.render_commands.push(com);
        }
        for mut com in other.compute_commands.iter().cloned() {
            self.resources.rebase_compute(&mut com);
            self.compute_commands.push(com);
        }
        self.blit_commands.extend_from_slice(&other.blit_commands);

        self.resources.extend(&other.resources);
    }
}

#[derive(Debug)]
enum CommandSink {
    Immediate {
        cmd_buffer: metal::CommandBuffer,
        token: Token,
        encoder_state: EncoderState,
        num_passes: usize,
        label: String,
    },
    Deferred {
        is_encoding: bool,
        is_inheriting: bool,
        journal: Journal,
        label: String,
    },
    #[cfg(feature = "dispatch")]
    Remote {
        queue: NoDebug<dispatch::Queue>,
        cmd_buffer: Arc<Mutex<metal::CommandBuffer>>,
        token: Token,
        pass: Option<EncodePass>,
        capacity: Capacity,
        label: String,
    },
}

/// A helper temporary object that consumes state-setting commands only
/// applicable to a render pass currently encoded.
enum PreRender<'a> {
    Immediate(&'a metal::RenderCommandEncoderRef),
    Deferred(
        &'a mut soft::Own,
        &'a mut Vec<soft::RenderCommand<soft::Own>>,
    ),
    Void,
}

impl<'a> PreRender<'a> {
    fn is_void(&self) -> bool {
        match *self {
            PreRender::Void => true,
            _ => false,
        }
    }

    fn issue(&mut self, command: soft::RenderCommand<&soft::Ref>) {
        match *self {
            PreRender::Immediate(encoder) => exec_render(encoder, command, &&soft::Ref),
            PreRender::Deferred(ref mut resources, ref mut list) => {
                list.alloc().init(resources.own_render(command));
            }
            PreRender::Void => (),
        }
    }

    fn issue_many<'b, I>(&mut self, commands: I)
    where
        I: Iterator<Item = soft::RenderCommand<&'b soft::Ref>>,
    {
        match *self {
            PreRender::Immediate(encoder) => {
                for com in commands {
                    exec_render(encoder, com, &&soft::Ref);
                }
            }
            PreRender::Deferred(ref mut resources, ref mut list) => {
                list.extend(commands.map(|com| resources.own_render(com)))
            }
            PreRender::Void => {}
        }
    }
}

/// A helper temporary object that consumes state-setting commands only
/// applicable to a compute pass currently encoded.
enum PreCompute<'a> {
    Immediate(&'a metal::ComputeCommandEncoderRef),
    Deferred(
        &'a mut soft::Own,
        &'a mut Vec<soft::ComputeCommand<soft::Own>>,
    ),
    Void,
}

impl<'a> PreCompute<'a> {
    fn issue<'b>(&mut self, command: soft::ComputeCommand<&'b soft::Ref>) {
        match *self {
            PreCompute::Immediate(encoder) => exec_compute(encoder, command, &&soft::Ref),
            PreCompute::Deferred(ref mut resources, ref mut list) => {
                list.alloc().init(resources.own_compute(command));
            }
            PreCompute::Void => (),
        }
    }

    fn issue_many<'b, I>(&mut self, commands: I)
    where
        I: Iterator<Item = soft::ComputeCommand<&'b soft::Ref>>,
    {
        match *self {
            PreCompute::Immediate(encoder) => {
                for com in commands {
                    exec_compute(encoder, com, &&soft::Ref);
                }
            }
            PreCompute::Deferred(ref mut resources, ref mut list) => {
                list.extend(commands.map(|com| resources.own_compute(com)))
            }
            PreCompute::Void => {}
        }
    }
}

impl CommandSink {
    fn label(&mut self, label: &str) -> &Self {
        match self {
            CommandSink::Immediate { label: l, .. } | CommandSink::Deferred { label: l, .. } => *l = label.to_string(),
            #[cfg(feature = "dispatch")]
            CommandSink::Remote { label: l, .. } => *l = label.to_string(),
        }
        self
    }

    fn stop_encoding(&mut self) {
        match *self {
            CommandSink::Immediate {
                ref mut encoder_state,
                ..
            } => {
                encoder_state.end();
            }
            CommandSink::Deferred {
                ref mut is_encoding,
                ref mut journal,
                ..
            } => {
                *is_encoding = false;
                journal.stop();
            }
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                queue: NoDebug(ref queue),
                ref cmd_buffer,
                ref mut pass,
                ref mut capacity,
                ..
            } => {
                if let Some(pass) = pass.take() {
                    pass.update(capacity);
                    pass.schedule(queue, cmd_buffer);
                }
            }
        }
    }

    /// Start issuing pre-render commands. Those can be rejected, so the caller is responsible
    /// for updating the state cache accordingly, so that it's set upon the start of a next pass.
    fn pre_render(&mut self) -> PreRender {
        match *self {
            CommandSink::Immediate {
                encoder_state: EncoderState::Render(ref encoder),
                ..
            } => PreRender::Immediate(encoder),
            CommandSink::Deferred {
                is_encoding: true,
                ref mut journal,
                ..
            } => match journal.passes.last() {
                Some(&(soft::Pass::Render(_), _, _)) => {
                    PreRender::Deferred(&mut journal.resources, &mut journal.render_commands)
                }
                _ => PreRender::Void,
            },
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                pass: Some(EncodePass::Render(ref mut list, ref mut resources, _, _)),
                ..
            } => PreRender::Deferred(resources, list),
            _ => PreRender::Void,
        }
    }

    /// Switch the active encoder to render by starting a render pass.
    fn switch_render(&mut self, descriptor: metal::RenderPassDescriptor) -> PreRender {
        //assert!(AutoReleasePool::is_active());
        self.stop_encoding();

        match *self {
            CommandSink::Immediate {
                ref cmd_buffer,
                ref mut encoder_state,
                ref mut num_passes,
                ref label,
                ..
            } => {
                *num_passes += 1;
                let encoder = cmd_buffer.new_render_command_encoder(&descriptor);
                encoder.set_label(label);
                *encoder_state = EncoderState::Render(encoder.to_owned());
                PreRender::Immediate(encoder)
            }
            CommandSink::Deferred {
                ref mut is_encoding,
                ref mut journal,
                is_inheriting,
                ref label,
                ..
            } => {
                assert!(!is_inheriting);
                *is_encoding = true;
                journal.passes.alloc().init((
                    soft::Pass::Render(descriptor),
                    journal.render_commands.len() .. 0,
                    label.clone(),
                ));
                PreRender::Deferred(&mut journal.resources, &mut journal.render_commands)
            }
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                ref mut pass,
                ref capacity,
                ref label,
                ..
            } => {
                let list = Vec::with_capacity(capacity.render);
                *pass = Some(EncodePass::Render(list, soft::Own::default(), descriptor, label.clone()));
                match *pass {
                    Some(EncodePass::Render(ref mut list, ref mut resources, _, _)) => {
                        PreRender::Deferred(resources, list)
                    }
                    _ => unreachable!(),
                }
            }
        }
    }

    fn quick_render<'a, I>(
        &mut self,
        label: &str,
        descriptor: metal::RenderPassDescriptor,
        commands: I,
    ) where
        I: Iterator<Item = soft::RenderCommand<&'a soft::Ref>>,
    {
        {
            let mut pre = self.switch_render(descriptor);
            if let PreRender::Immediate(encoder) = pre {
                encoder.set_label(label);
            }
            pre.issue_many(commands);
        }
        self.stop_encoding();
    }

    /// Issue provided blit commands. This function doesn't expect an active blit pass,
    /// it will automatically start one when needed.
    fn blit_commands<I>(&mut self, commands: I)
    where
        I: Iterator<Item = soft::BlitCommand>,
    {
        enum PreBlit<'b> {
            Immediate(&'b metal::BlitCommandEncoderRef),
            Deferred(&'b mut Vec<soft::BlitCommand>),
        }

        let pre = match *self {
            CommandSink::Immediate {
                encoder_state: EncoderState::Blit(ref encoder),
                ..
            } => PreBlit::Immediate(encoder),
            CommandSink::Immediate {
                ref cmd_buffer,
                ref mut encoder_state,
                ref mut num_passes,
                ..
            } => {
                *num_passes += 1;
                encoder_state.end();
                let encoder = cmd_buffer.new_blit_command_encoder();
                *encoder_state = EncoderState::Blit(encoder.to_owned());
                PreBlit::Immediate(encoder)
            }
            CommandSink::Deferred {
                ref mut is_encoding,
                is_inheriting,
                ref mut journal,
                ref label,
                ..
            } => {
                assert!(!is_inheriting);
                *is_encoding = true;
                if let Some(&(soft::Pass::Blit, _, _)) = journal.passes.last() {
                } else {
                    journal.stop();
                    journal
                        .passes
                        .alloc()
                        .init((soft::Pass::Blit, journal.blit_commands.len() .. 0, label.clone()));
                }
                PreBlit::Deferred(&mut journal.blit_commands)
            }
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                pass: Some(EncodePass::Blit(ref mut list, _)),
                ..
            } => PreBlit::Deferred(list),
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                queue: NoDebug(ref queue),
                ref cmd_buffer,
                ref mut pass,
                ref mut capacity,
                ref label,
                ..
            } => {
                if let Some(pass) = pass.take() {
                    pass.update(capacity);
                    pass.schedule(queue, cmd_buffer);
                }
                let list = Vec::with_capacity(capacity.blit);
                *pass = Some(EncodePass::Blit(list, label.clone()));
                match *pass {
                    Some(EncodePass::Blit(ref mut list, _)) => PreBlit::Deferred(list),
                    _ => unreachable!(),
                }
            }
        };

        match pre {
            PreBlit::Immediate(encoder) => {
                for com in commands {
                    exec_blit(encoder, com);
                }
            }
            PreBlit::Deferred(list) => {
                list.extend(commands);
            }
        }
    }

    /// Start issuing pre-compute commands. Those can be rejected, so the caller is responsible
    /// for updating the state cache accordingly, so that it's set upon the start of a next pass.
    fn pre_compute(&mut self) -> PreCompute {
        match *self {
            CommandSink::Immediate {
                encoder_state: EncoderState::Compute(ref encoder),
                ..
            } => PreCompute::Immediate(encoder),
            CommandSink::Deferred {
                is_encoding: true,
                is_inheriting: false,
                ref mut journal,
                ..
            } => match journal.passes.last() {
                Some(&(soft::Pass::Compute, _, _)) => {
                    PreCompute::Deferred(&mut journal.resources, &mut journal.compute_commands)
                }
                _ => PreCompute::Void,
            },
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                pass: Some(EncodePass::Compute(ref mut list, ref mut resources, _)),
                ..
            } => PreCompute::Deferred(resources, list),
            _ => PreCompute::Void,
        }
    }

    /// Switch the active encoder to compute.
    /// Second returned value is `true` if the switch has just happened.
    fn switch_compute(&mut self) -> (PreCompute, bool) {
        match *self {
            CommandSink::Immediate {
                encoder_state: EncoderState::Compute(ref encoder),
                ..
            } => (PreCompute::Immediate(encoder), false),
            CommandSink::Immediate {
                ref cmd_buffer,
                ref mut encoder_state,
                ref mut num_passes,
                ..
            } => {
                *num_passes += 1;
                encoder_state.end();
                let encoder = cmd_buffer.new_compute_command_encoder();
                *encoder_state = EncoderState::Compute(encoder.to_owned());
                (PreCompute::Immediate(encoder), true)
            }
            CommandSink::Deferred {
                ref mut is_encoding,
                is_inheriting,
                ref mut journal,
                ref label,
                ..
            } => {
                assert!(!is_inheriting);
                *is_encoding = true;
                let switch = if let Some(&(soft::Pass::Compute, _, _)) = journal.passes.last() {
                    false
                } else {
                    journal.stop();
                    journal
                        .passes
                        .alloc()
                        .init((soft::Pass::Compute, journal.compute_commands.len() .. 0, label.clone()));
                    true
                };
                (
                    PreCompute::Deferred(&mut journal.resources, &mut journal.compute_commands),
                    switch,
                )
            }
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                pass: Some(EncodePass::Compute(ref mut list, ref mut resources, _)),
                ..
            } => (PreCompute::Deferred(resources, list), false),
            #[cfg(feature = "dispatch")]
            CommandSink::Remote {
                queue: NoDebug(ref queue),
                ref cmd_buffer,
                ref mut pass,
                ref mut capacity,
                ref label,
                ..
            } => {
                if let Some(pass) = pass.take() {
                    pass.update(capacity);
                    pass.schedule(queue, cmd_buffer);
                }
                let list = Vec::with_capacity(capacity.compute);
                *pass = Some(EncodePass::Compute(list, soft::Own::default(), label.clone()));
                match *pass {
                    Some(EncodePass::Compute(ref mut list, ref mut resources, _)) => {
                        (PreCompute::Deferred(resources, list), true)
                    }
                    _ => unreachable!(),
                }
            }
        }
    }

    fn quick_compute<'a, I>(&mut self, label: &str, commands: I)
    where
        I: Iterator<Item = soft::ComputeCommand<&'a soft::Ref>>,
    {
        {
            let (mut pre, switch) = self.switch_compute();
            pre.issue_many(commands);
            if switch {
                if let PreCompute::Immediate(encoder) = pre {
                    encoder.set_label(label);
                }
            }
        }
        self.stop_encoding();
    }
}

#[derive(Clone, Debug)]
pub struct IndexBuffer<B> {
    buffer: B,
    offset: u32,
    stride: u32,
}

/// This is an inner mutable part of the command buffer that is
/// accessible by the owning command pool for one single reason:
/// to reset it.
#[derive(Debug)]
pub struct CommandBufferInner {
    sink: Option<CommandSink>,
    level: com::Level,
    backup_journal: Option<Journal>,
    #[cfg(feature = "dispatch")]
    backup_capacity: Option<Capacity>,
    retained_buffers: Vec<metal::Buffer>,
    retained_textures: Vec<metal::Texture>,
    active_visibility_queries: Vec<query::Id>,
    events: Vec<(Arc<AtomicBool>, bool)>,
    host_events: Vec<Arc<AtomicBool>>,
}

impl Drop for CommandBufferInner {
    fn drop(&mut self) {
        if self.sink.is_some() {
            error!("Command buffer not released properly!");
        }
    }
}

impl CommandBufferInner {
    pub(crate) fn reset(&mut self, shared: &Shared, release: bool) {
        match self.sink.take() {
            Some(CommandSink::Immediate {
                token,
                mut encoder_state,
                ..
            }) => {
                encoder_state.end();
                shared.queue.lock().release(token);
            }
            Some(CommandSink::Deferred { mut journal, .. }) => {
                if !release {
                    journal.clear();
                    self.backup_journal = Some(journal);
                }
            }
            #[cfg(feature = "dispatch")]
            Some(CommandSink::Remote {
                token, capacity, ..
            }) => {
                shared.queue.lock().release(token);
                if !release {
                    self.backup_capacity = Some(capacity);
                }
            }
            None => {}
        };
        self.retained_buffers.clear();
        self.retained_textures.clear();
        self.active_visibility_queries.clear();
        self.events.clear();
    }

    fn sink(&mut self) -> &mut CommandSink {
        self.sink.as_mut().unwrap()
    }
}

#[derive(Debug)]
enum EncoderState {
    None,
    Blit(metal::BlitCommandEncoder),
    Render(metal::RenderCommandEncoder),
    Compute(metal::ComputeCommandEncoder),
}

impl EncoderState {
    fn end(&mut self) {
        match mem::replace(self, EncoderState::None) {
            EncoderState::None => {}
            EncoderState::Render(ref encoder) => {
                encoder.end_encoding();
            }
            EncoderState::Blit(ref encoder) => {
                encoder.end_encoding();
            }
            EncoderState::Compute(ref encoder) => {
                encoder.end_encoding();
            }
        }
    }
}

fn div(a: u32, b: u32) -> u32 {
    (a + b - 1) / b
}

fn compute_pitches(region: &com::BufferImageCopy, fd: FormatDesc, extent: &MTLSize) -> (u32, u32) {
    let buffer_width = if region.buffer_width == 0 {
        extent.width as u32
    } else {
        region.buffer_width
    };
    let buffer_height = if region.buffer_height == 0 {
        extent.height as u32
    } else {
        region.buffer_height
    };
    let row_pitch = div(buffer_width, fd.dim.0 as _) * (fd.bits / 8) as u32;
    let slice_pitch = div(buffer_height, fd.dim.1 as _) * row_pitch;
    (row_pitch, slice_pitch)
}

fn exec_render<R, C>(encoder: &metal::RenderCommandEncoderRef, command: C, resources: &R)
where
    R: soft::Resources,
    R::Data: Borrow<[u32]>,
    R::BufferArray: soft::AsSlice<Option<BufferPtr>, R> + soft::AsSlice<buffer::Offset, R>,
    R::TextureArray: soft::AsSlice<Option<TexturePtr>, R>,
    R::SamplerArray: soft::AsSlice<Option<SamplerPtr>, R>,
    R::DepthStencil: Borrow<metal::DepthStencilStateRef>,
    R::RenderPipeline: Borrow<metal::RenderPipelineStateRef>,
    C: Borrow<soft::RenderCommand<R>>,
{
    use crate::soft::RenderCommand as Cmd;
    match *command.borrow() {
        Cmd::SetViewport(ref rect, ref depth) => {
            encoder.set_viewport(MTLViewport {
                originX: rect.x as _,
                originY: rect.y as _,
                width: rect.w as _,
                height: rect.h as _,
                znear: depth.start as _,
                zfar: depth.end as _,
            });
        }
        Cmd::SetScissor(scissor) => {
            encoder.set_scissor_rect(scissor);
        }
        Cmd::SetBlendColor(color) => {
            encoder.set_blend_color(color[0], color[1], color[2], color[3]);
        }
        Cmd::SetDepthBias(depth_bias) => {
            encoder.set_depth_bias(
                depth_bias.const_factor,
                depth_bias.slope_factor,
                depth_bias.clamp,
            );
        }
        Cmd::SetDepthStencilState(ref depth_stencil) => {
            encoder.set_depth_stencil_state(depth_stencil.borrow());
        }
        Cmd::SetStencilReferenceValues(sided) => {
            encoder.set_stencil_front_back_reference_value(sided.front, sided.back);
        }
        Cmd::SetRasterizerState(ref rs) => {
            encoder.set_front_facing_winding(rs.front_winding);
            encoder.set_cull_mode(rs.cull_mode);
            encoder.set_triangle_fill_mode(rs.fill_mode);
            if let Some(depth_clip) = rs.depth_clip {
                encoder.set_depth_clip_mode(depth_clip);
            }
        }
        Cmd::SetVisibilityResult(mode, offset) => {
            encoder.set_visibility_result_mode(mode, offset);
        }
        Cmd::BindBuffer {
            stage,
            index,
            buffer,
            offset,
        } => {
            let native = Some(buffer.as_native());
            match stage {
                pso::Stage::Vertex => encoder.set_vertex_buffer(index as _, native, offset as _),
                pso::Stage::Fragment => {
                    encoder.set_fragment_buffer(index as _, native, offset as _)
                }
                _ => unreachable!(),
            }
        }
        Cmd::BindBuffers {
            stage,
            index,
            ref buffers,
        } => {
            use crate::soft::AsSlice;
            let values: &[Option<BufferPtr>] = buffers.as_slice(resources);
            if !values.is_empty() {
                let data = unsafe {
                    // convert `BufferPtr` -> `&metal::BufferRef`
                    mem::transmute(values)
                };
                let offsets = buffers.as_slice(resources);
                match stage {
                    pso::Stage::Vertex => encoder.set_vertex_buffers(index as _, data, offsets),
                    pso::Stage::Fragment => encoder.set_fragment_buffers(index as _, data, offsets),
                    _ => unreachable!(),
                }
            }
        }
        Cmd::BindBufferData {
            stage,
            index,
            ref words,
        } => {
            let slice = words.borrow();
            match stage {
                pso::Stage::Vertex => encoder.set_vertex_bytes(
                    index as _,
                    (slice.len() * WORD_SIZE) as u64,
                    slice.as_ptr() as _,
                ),
                pso::Stage::Fragment => encoder.set_fragment_bytes(
                    index as _,
                    (slice.len() * WORD_SIZE) as u64,
                    slice.as_ptr() as _,
                ),
                _ => unreachable!(),
            }
        }
        Cmd::BindTextures {
            stage,
            index,
            ref textures,
        } => {
            use crate::soft::AsSlice;
            let values = textures.as_slice(resources);
            if !values.is_empty() {
                let data = unsafe {
                    // convert `TexturePtr` -> `&metal::TextureRef`
                    mem::transmute(values)
                };
                match stage {
                    pso::Stage::Vertex => encoder.set_vertex_textures(index as _, data),
                    pso::Stage::Fragment => encoder.set_fragment_textures(index as _, data),
                    _ => unreachable!(),
                }
            }
        }
        Cmd::BindSamplers {
            stage,
            index,
            ref samplers,
        } => {
            use crate::soft::AsSlice;
            let values = samplers.as_slice(resources);
            if !values.is_empty() {
                let data = unsafe {
                    // convert `SamplerPtr` -> `&metal::SamplerStateRef`
                    mem::transmute(values)
                };
                match stage {
                    pso::Stage::Vertex => encoder.set_vertex_sampler_states(index as _, data),
                    pso::Stage::Fragment => encoder.set_fragment_sampler_states(index as _, data),
                    _ => unreachable!(),
                }
            }
        }
        Cmd::BindPipeline(ref pipeline_state) => {
            encoder.set_render_pipeline_state(pipeline_state.borrow());
        }
        Cmd::UseResource { resource, usage } => {
            encoder.use_resource(resource.as_native(), usage);
        }
        Cmd::Draw {
            primitive_type,
            ref vertices,
            ref instances,
        } => {
            if instances.end == 1 {
                encoder.draw_primitives(
                    primitive_type,
                    vertices.start as _,
                    (vertices.end - vertices.start) as _,
                );
            } else if instances.start == 0 {
                encoder.draw_primitives_instanced(
                    primitive_type,
                    vertices.start as _,
                    (vertices.end - vertices.start) as _,
                    instances.end as _,
                );
            } else {
                encoder.draw_primitives_instanced_base_instance(
                    primitive_type,
                    vertices.start as _,
                    (vertices.end - vertices.start) as _,
                    (instances.end - instances.start) as _,
                    instances.start as _,
                );
            }
        }
        Cmd::DrawIndexed {
            primitive_type,
            ref index,
            ref indices,
            base_vertex,
            ref instances,
        } => {
            let index_count = (indices.end - indices.start) as _;
            let index_type = match index.stride {
                2 => MTLIndexType::UInt16,
                4 => MTLIndexType::UInt32,
                _ => unreachable!(),
            };
            let offset = (index.offset + indices.start * index.stride) as u64;
            let index_buffer = index.buffer.as_native();
            if base_vertex == 0 && instances.end == 1 {
                encoder.draw_indexed_primitives(
                    primitive_type,
                    index_count,
                    index_type,
                    index_buffer,
                    offset,
                );
            } else if base_vertex == 0 && instances.start == 0 {
                encoder.draw_indexed_primitives_instanced(
                    primitive_type,
                    index_count,
                    index_type,
                    index_buffer,
                    offset,
                    instances.end as _,
                );
            } else {
                encoder.draw_indexed_primitives_instanced_base_instance(
                    primitive_type,
                    index_count,
                    index_type,
                    index_buffer,
                    offset,
                    (instances.end - instances.start) as _,
                    base_vertex as _,
                    instances.start as _,
                );
            }
        }
        Cmd::DrawIndirect {
            primitive_type,
            buffer,
            offset,
        } => {
            encoder.draw_primitives_indirect(primitive_type, buffer.as_native(), offset);
        }
        Cmd::DrawIndexedIndirect {
            primitive_type,
            ref index,
            buffer,
            offset,
        } => {
            let index_type = match index.stride {
                2 => MTLIndexType::UInt16,
                4 => MTLIndexType::UInt32,
                _ => unreachable!(),
            };
            encoder.draw_indexed_primitives_indirect(
                primitive_type,
                index_type,
                index.buffer.as_native(),
                index.offset as u64,
                buffer.as_native(),
                offset,
            );
        }
    }
}

fn exec_blit<C>(encoder: &metal::BlitCommandEncoderRef, command: C)
where
    C: Borrow<soft::BlitCommand>,
{
    use crate::soft::BlitCommand as Cmd;
    match *command.borrow() {
        Cmd::FillBuffer {
            dst,
            ref range,
            value,
        } => {
            encoder.fill_buffer(
                dst.as_native(),
                NSRange {
                    location: range.start,
                    length: range.end - range.start,
                },
                value,
            );
        }
        Cmd::CopyBuffer {
            src,
            dst,
            ref region,
        } => {
            encoder.copy_from_buffer(
                src.as_native(),
                region.src as NSUInteger,
                dst.as_native(),
                region.dst as NSUInteger,
                region.size as NSUInteger,
            );
        }
        Cmd::CopyImage {
            src,
            dst,
            ref region,
        } => {
            let size = conv::map_extent(region.extent);
            let src_offset = conv::map_offset(region.src_offset);
            let dst_offset = conv::map_offset(region.dst_offset);
            let layers = region
                .src_subresource
                .layers
                .clone()
                .zip(region.dst_subresource.layers.clone());
            for (src_layer, dst_layer) in layers {
                encoder.copy_from_texture(
                    src.as_native(),
                    src_layer as _,
                    region.src_subresource.level as _,
                    src_offset,
                    size,
                    dst.as_native(),
                    dst_layer as _,
                    region.dst_subresource.level as _,
                    dst_offset,
                );
            }
        }
        Cmd::CopyBufferToImage {
            src,
            dst,
            dst_desc,
            ref region,
        } => {
            let extent = conv::map_extent(region.image_extent);
            let origin = conv::map_offset(region.image_offset);
            let (row_pitch, slice_pitch) = compute_pitches(&region, dst_desc, &extent);
            let r = &region.image_layers;

            for layer in r.layers.clone() {
                let offset = region.buffer_offset
                    + slice_pitch as NSUInteger * (layer - r.layers.start) as NSUInteger;
                encoder.copy_from_buffer_to_texture(
                    src.as_native(),
                    offset as NSUInteger,
                    row_pitch as NSUInteger,
                    slice_pitch as NSUInteger,
                    extent,
                    dst.as_native(),
                    layer as NSUInteger,
                    r.level as NSUInteger,
                    origin,
                    metal::MTLBlitOption::empty(),
                );
            }
        }
        Cmd::CopyImageToBuffer {
            src,
            src_desc,
            dst,
            ref region,
        } => {
            let extent = conv::map_extent(region.image_extent);
            let origin = conv::map_offset(region.image_offset);
            let (row_pitch, slice_pitch) = compute_pitches(&region, src_desc, &extent);
            let r = &region.image_layers;

            for layer in r.layers.clone() {
                let offset = region.buffer_offset
                    + slice_pitch as NSUInteger * (layer - r.layers.start) as NSUInteger;
                encoder.copy_from_texture_to_buffer(
                    src.as_native(),
                    layer as NSUInteger,
                    r.level as NSUInteger,
                    origin,
                    extent,
                    dst.as_native(),
                    offset as NSUInteger,
                    row_pitch as NSUInteger,
                    slice_pitch as NSUInteger,
                    metal::MTLBlitOption::empty(),
                );
            }
        }
    }
}

fn exec_compute<R, C>(encoder: &metal::ComputeCommandEncoderRef, command: C, resources: &R)
where
    R: soft::Resources,
    R::Data: Borrow<[u32]>,
    R::BufferArray: soft::AsSlice<Option<BufferPtr>, R> + soft::AsSlice<buffer::Offset, R>,
    R::TextureArray: soft::AsSlice<Option<TexturePtr>, R>,
    R::SamplerArray: soft::AsSlice<Option<SamplerPtr>, R>,
    R::ComputePipeline: Borrow<metal::ComputePipelineStateRef>,
    C: Borrow<soft::ComputeCommand<R>>,
{
    use crate::soft::ComputeCommand as Cmd;
    match *command.borrow() {
        Cmd::BindBuffer {
            index,
            buffer,
            offset,
        } => {
            let native = Some(buffer.as_native());
            encoder.set_buffer(index as _, native, offset);
        }
        Cmd::BindBuffers { index, ref buffers } => {
            use crate::soft::AsSlice;
            let values: &[Option<BufferPtr>] = buffers.as_slice(resources);
            if !values.is_empty() {
                let data = unsafe {
                    // convert `BufferPtr` -> `&metal::BufferRef`
                    mem::transmute(values)
                };
                let offsets = buffers.as_slice(resources);
                encoder.set_buffers(index as _, data, offsets);
            }
        }
        Cmd::BindBufferData { ref words, index } => {
            let slice = words.borrow();
            encoder.set_bytes(
                index as _,
                (slice.len() * WORD_SIZE) as u64,
                slice.as_ptr() as _,
            );
        }
        Cmd::BindTextures {
            index,
            ref textures,
        } => {
            use crate::soft::AsSlice;
            let values = textures.as_slice(resources);
            if !values.is_empty() {
                let data = unsafe {
                    // convert `TexturePtr` -> `&metal::TextureRef`
                    mem::transmute(values)
                };
                encoder.set_textures(index as _, data);
            }
        }
        Cmd::BindSamplers {
            index,
            ref samplers,
        } => {
            use crate::soft::AsSlice;
            let values = samplers.as_slice(resources);
            if !values.is_empty() {
                let data = unsafe {
                    // convert `SamplerPtr` -> `&metal::SamplerStateRef`
                    mem::transmute(values)
                };
                encoder.set_sampler_states(index as _, data);
            }
        }
        Cmd::BindPipeline(ref pipeline) => {
            encoder.set_compute_pipeline_state(pipeline.borrow());
        }
        Cmd::UseResource { resource, usage } => {
            encoder.use_resource(resource.as_native(), usage);
        }
        Cmd::Dispatch { wg_size, wg_count } => {
            encoder.dispatch_thread_groups(wg_count, wg_size);
        }
        Cmd::DispatchIndirect {
            wg_size,
            buffer,
            offset,
        } => {
            encoder.dispatch_thread_groups_indirect(buffer.as_native(), offset, wg_size);
        }
    }
}


#[derive(Default, Debug)]
struct PerformanceCounters {
    immediate_command_buffers: usize,
    deferred_command_buffers: usize,
    remote_command_buffers: usize,
    signal_command_buffers: usize,
    frame_wait_duration: time::Duration,
    frame_wait_count: usize,
    frame: usize,
}

#[derive(Debug)]
pub struct CommandQueue {
    shared: Arc<Shared>,
    retained_buffers: Vec<metal::Buffer>,
    retained_textures: Vec<metal::Texture>,
    active_visibility_queries: Vec<query::Id>,
    perf_counters: Option<PerformanceCounters>,
    /// If true, we combine deferred command buffers together into one giant
    /// command buffer per submission, including the signalling logic.
    pub stitch_deferred: bool,
    /// Hack around the Metal System Trace logic that ignores empty command buffers entirely.
    pub insert_dummy_encoders: bool,
}

unsafe impl Send for CommandQueue {}
unsafe impl Sync for CommandQueue {}

impl CommandQueue {
    pub(crate) fn new(shared: Arc<Shared>) -> Self {
        CommandQueue {
            shared,
            retained_buffers: Vec::new(),
            retained_textures: Vec::new(),
            active_visibility_queries: Vec::new(),
            perf_counters: if COUNTERS_REPORT_WINDOW != 0 {
                Some(PerformanceCounters::default())
            } else {
                None
            },
            stitch_deferred: true,
            insert_dummy_encoders: false,
        }
    }

    /// This is a hack around Metal System Trace logic that ignores empty command buffers entirely.
    fn record_empty(&self, command_buf: &metal::CommandBufferRef) {
        if self.insert_dummy_encoders {
            command_buf.new_blit_command_encoder().end_encoding();
        }
    }

    fn wait<'a, T, I>(&mut self, wait_semaphores: I)
    where
        T: 'a + Borrow<native::Semaphore>,
        I: IntoIterator<Item = &'a T>,
    {
        for semaphore in wait_semaphores {
            let sem = semaphore.borrow();
            if let Some(ref system) = sem.system {
                system.wait(!0);
            }
            if let Some(swap_image) = sem.image_ready.lock().take() {
                let start = time::Instant::now();
                let count = swap_image.wait_until_ready();
                if let Some(ref mut counters) = self.perf_counters {
                    counters.frame_wait_count += count;
                    counters.frame_wait_duration += start.elapsed();
                }
            }
        }
    }
}

impl hal::queue::CommandQueue<Backend> for CommandQueue {
    unsafe fn submit<'a, T, Ic, S, Iw, Is>(
        &mut self,
        hal::queue::Submission {
            command_buffers,
            wait_semaphores,
            signal_semaphores,
        }: hal::queue::Submission<Ic, Iw, Is>,
        fence: Option<&native::Fence>,
    ) where
        T: 'a + Borrow<CommandBuffer>,
        Ic: IntoIterator<Item = &'a T>,
        S: 'a + Borrow<native::Semaphore>,
        Iw: IntoIterator<Item = (&'a S, pso::PipelineStage)>,
        Is: IntoIterator<Item = &'a S>,
    {
        use smallvec::SmallVec;

        debug!("submitting with fence {:?}", fence);
        self.wait(wait_semaphores.into_iter().map(|(s, _)| s));

        const BLOCK_BUCKET: usize = 4;
        let system_semaphores = signal_semaphores
            .into_iter()
            .filter_map(|sem| sem.borrow().system.clone())
            .collect::<SmallVec<[_; BLOCK_BUCKET]>>();

        #[allow(unused_mut)]
        let (mut num_immediate, mut num_deferred, mut num_remote) = (0, 0, 0);
        let mut event_commands = Vec::new();
        let do_signal = fence.is_some() || !system_semaphores.is_empty();

        autoreleasepool(|| {
            // for command buffers
            let cmd_queue = self.shared.queue.lock();
            let mut blocker = self.shared.queue_blocker.lock();
            let mut deferred_cmd_buffer = None::<&metal::CommandBufferRef>;

            for buffer in command_buffers {
                let mut inner = buffer.borrow().inner.borrow_mut();
                let CommandBufferInner {
                    ref sink,
                    ref mut retained_buffers,
                    ref mut retained_textures,
                    ref mut active_visibility_queries,
                    ref events,
                    ref host_events,
                    ..
                } = *inner;

                //TODO: split event commands into immediate/blocked submissions?
                event_commands.extend_from_slice(events);
                // wait for anything not previously fired
                let wait_events = host_events
                    .iter()
                    .filter(|event| {
                        event_commands
                            .iter()
                            .rfind(|ev| Arc::ptr_eq(event, &ev.0))
                            .map_or(true, |ev| !ev.1)
                    })
                    .cloned()
                    .collect::<Vec<_>>();
                if !wait_events.is_empty() {
                    blocker.submissions.push(BlockedSubmission {
                        wait_events,
                        command_buffers: Vec::new(),
                    });
                }

                match *sink {
                    Some(CommandSink::Immediate {
                        ref cmd_buffer,
                        ref token,
                        num_passes,
                        ..
                    }) => {
                        num_immediate += 1;
                        trace!("\timmediate {:?} with {} passes", token, num_passes);
                        self.retained_buffers.extend(retained_buffers.drain(..));
                        self.retained_textures.extend(retained_textures.drain(..));
                        self.active_visibility_queries
                            .extend(active_visibility_queries.drain(..));
                        if num_passes != 0 {
                            // flush the deferred recording, if any
                            if let Some(cb) = deferred_cmd_buffer.take() {
                                blocker.submit_impl(cb);
                            }
                            blocker.submit_impl(cmd_buffer);
                        }
                    }
                    Some(CommandSink::Deferred { ref journal, .. }) => {
                        num_deferred += 1;
                        trace!("\tdeferred with {} passes", journal.passes.len());
                        self.active_visibility_queries
                            .extend_from_slice(active_visibility_queries);
                        if !journal.passes.is_empty() {
                            let cmd_buffer = deferred_cmd_buffer.take().unwrap_or_else(|| {
                                let cmd_buffer = cmd_queue.spawn_temp();
                                cmd_buffer.enqueue();
                                cmd_buffer.set_label("deferred");
                                cmd_buffer
                            });
                            journal.record(&*cmd_buffer);
                            if self.stitch_deferred {
                                deferred_cmd_buffer = Some(cmd_buffer);
                            } else {
                                blocker.submit_impl(cmd_buffer);
                            }
                        }
                    }
                    #[cfg(feature = "dispatch")]
                    Some(CommandSink::Remote {
                        queue: NoDebug(ref queue),
                        ref cmd_buffer,
                        ref token,
                        ..
                    }) => {
                        num_remote += 1;
                        trace!("\tremote {:?}", token);
                        cmd_buffer.lock().enqueue();
                        let shared_cb = SharedCommandBuffer(Arc::clone(cmd_buffer));
                        //TODO: make this compatible with events
                        queue.sync(move || {
                            shared_cb.0.lock().commit();
                        });
                    }
                    None => panic!("Command buffer not recorded for submission"),
                }
            }

            if do_signal || !event_commands.is_empty() || !self.active_visibility_queries.is_empty()
            {
                //Note: there is quite a bit copying here
                let free_buffers = self
                    .retained_buffers
                    .drain(..)
                    .collect::<SmallVec<[_; BLOCK_BUCKET]>>();
                let free_textures = self
                    .retained_textures
                    .drain(..)
                    .collect::<SmallVec<[_; BLOCK_BUCKET]>>();
                let visibility = if self.active_visibility_queries.is_empty() {
                    None
                } else {
                    let queries = self
                        .active_visibility_queries
                        .drain(..)
                        .collect::<SmallVec<[_; BLOCK_BUCKET]>>();
                    Some((Arc::clone(&self.shared), queries))
                };

                let block = ConcreteBlock::new(move |_cb: *mut ()| {
                    // signal the semaphores
                    for semaphore in &system_semaphores {
                        semaphore.signal();
                    }
                    // process events
                    for &(ref atomic, value) in &event_commands {
                        atomic.store(value, Ordering::Release);
                    }
                    // free all the manually retained resources
                    let _ = free_buffers;
                    let _ = free_textures;
                    // update visibility queries
                    if let Some((ref shared, ref queries)) = visibility {
                        let vis = &shared.visibility;
                        let availability_ptr = (vis.buffer.contents() as *mut u8)
                            .offset(vis.availability_offset as isize)
                            as *mut u32;
                        for &q in queries {
                            *availability_ptr.offset(q as isize) = 1;
                        }
                        //HACK: the lock is needed to wake up, but it doesn't hold the checked data
                        let _ = vis.allocator.lock();
                        vis.condvar.notify_all();
                    }
                })
                .copy();

                let cmd_buffer = deferred_cmd_buffer.take().unwrap_or_else(|| {
                    let cmd_buffer = cmd_queue.spawn_temp();
                    cmd_buffer.set_label("signal");
                    self.record_empty(cmd_buffer);
                    cmd_buffer
                });
                let () = msg_send![cmd_buffer, addCompletedHandler: block.deref() as *const _];
                blocker.submit_impl(cmd_buffer);

                if let Some(fence) = fence {
                    debug!("\tmarking fence ptr {:?} as pending", fence.0.as_ptr());
                    fence
                        .0
                        .replace(native::FenceInner::PendingSubmission(cmd_buffer.to_owned()));
                }
            } else if let Some(cmd_buffer) = deferred_cmd_buffer {
                blocker.submit_impl(cmd_buffer);
            }
        });

        debug!(
            "\t{} immediate, {} deferred, and {} remote command buffers",
            num_immediate, num_deferred, num_remote
        );
        if let Some(ref mut counters) = self.perf_counters {
            counters.immediate_command_buffers += num_immediate;
            counters.deferred_command_buffers += num_deferred;
            counters.remote_command_buffers += num_remote;
            if do_signal {
                counters.signal_command_buffers += 1;
            }
        }
    }

    unsafe fn present<'a, W, Is, S, Iw>(
        &mut self,
        swapchains: Is,
        wait_semaphores: Iw,
    ) -> Result<Option<Suboptimal>, PresentError>
    where
        W: 'a + Borrow<window::Swapchain>,
        Is: IntoIterator<Item = (&'a W, SwapImageIndex)>,
        S: 'a + Borrow<native::Semaphore>,
        Iw: IntoIterator<Item = &'a S>,
    {
        self.wait(wait_semaphores);

        let queue = self.shared.queue.lock();
        autoreleasepool(|| {
            let command_buffer = queue.raw.new_command_buffer();
            command_buffer.set_label("present");
            self.record_empty(command_buffer);

            for (swapchain, index) in swapchains {
                debug!("presenting frame {}", index);
                let drawable = swapchain
                    .borrow()
                    .take_drawable(index)
                    .map_err(|()| PresentError::OutOfDate)?; // What `Err(())` represents?
                command_buffer.present_drawable(&drawable);
            }
            command_buffer.commit();
            Ok(())
        })?;

        if let Some(ref mut counters) = self.perf_counters {
            counters.frame += 1;
            if counters.frame >= COUNTERS_REPORT_WINDOW {
                let time = counters.frame_wait_duration / counters.frame as u32;
                let total_submitted = counters.immediate_command_buffers
                    + counters.deferred_command_buffers
                    + counters.remote_command_buffers
                    + counters.signal_command_buffers;
                println!("Performance counters:");
                println!(
                    "\tCommand buffers: {} immediate, {} deferred, {} remote, {} signals",
                    counters.immediate_command_buffers / counters.frame,
                    counters.deferred_command_buffers / counters.frame,
                    counters.remote_command_buffers / counters.frame,
                    counters.signal_command_buffers / counters.frame,
                );
                println!("\tEstimated pipeline length is {} frames, given the total active {} command buffers",
                    counters.frame * queue.reserve.start / total_submitted.max(1),
                    queue.reserve.start,
                );
                println!(
                    "\tFrame wait time is {}ms over {} requests",
                    time.as_secs() as u32 * 1000 + time.subsec_millis(),
                    counters.frame_wait_count as f32 / counters.frame as f32,
                );
                *counters = PerformanceCounters::default();
            }
        }

        Ok(None)
    }

    unsafe fn present_surface(
        &mut self,
        _surface: &mut window::Surface,
        image: window::SurfaceImage,
        wait_semaphore: Option<&native::Semaphore>,
    ) -> Result<Option<Suboptimal>, PresentError> {
        self.wait(wait_semaphore);

        let queue = self.shared.queue.lock();
        let drawable = image.into_drawable();
        autoreleasepool(|| {
            let command_buffer = queue.raw.new_command_buffer();
            command_buffer.set_label("present");
            self.record_empty(command_buffer);

            command_buffer.present_drawable(&drawable);
            command_buffer.commit();
        });
        Ok(None)
    }

    fn wait_idle(&self) -> Result<(), OutOfMemory> {
        QueueInner::wait_idle(&self.shared.queue);
        Ok(())
    }
}

fn assign_sides(
    this: &mut pso::Sided<pso::StencilValue>,
    faces: pso::Face,
    value: pso::StencilValue,
) {
    if faces.contains(pso::Face::FRONT) {
        this.front = value;
    }
    if faces.contains(pso::Face::BACK) {
        this.back = value;
    }
}

impl hal::pool::CommandPool<Backend> for CommandPool {
    unsafe fn reset(&mut self, release_resources: bool) {
        for cmd_buffer in &self.allocated {
            cmd_buffer
                .borrow_mut()
                .reset(&self.shared, release_resources);
        }
    }

    unsafe fn allocate_one(&mut self, level: com::Level) -> CommandBuffer {
        //TODO: fail with OOM if we allocate more actual command buffers
        // than our mega-queue supports.
        let inner = Arc::new(RefCell::new(CommandBufferInner {
            sink: None,
            level,
            backup_journal: None,
            #[cfg(feature = "dispatch")]
            backup_capacity: None,
            retained_buffers: Vec::new(),
            retained_textures: Vec::new(),
            active_visibility_queries: Vec::new(),
            events: Vec::new(),
            host_events: Vec::new(),
        }));
        self.allocated.push(Arc::clone(&inner));

        CommandBuffer {
            shared: Arc::clone(&self.shared),
            pool_shared: Arc::clone(&self.pool_shared),
            inner,
            state: State {
                viewport: None,
                scissors: None,
                blend_color: None,
                render_pso: None,
                render_pso_is_compatible: false,
                compute_pso: None,
                work_group_size: MTLSize {
                    width: 0,
                    height: 0,
                    depth: 0,
                },
                primitive_type: MTLPrimitiveType::Point,
                resources_vs: StageResources::new(),
                resources_ps: StageResources::new(),
                resources_cs: StageResources::new(),
                index_buffer: None,
                rasterizer_state: None,
                depth_bias: pso::DepthBias::default(),
                stencil: native::StencilState {
                    reference_values: pso::Sided::new(0),
                    read_masks: pso::Sided::new(!0),
                    write_masks: pso::Sided::new(!0),
                },
                push_constants: Vec::new(),
                vertex_buffers: Vec::new(),
                target_aspects: Aspects::empty(),
                target_extent: Extent::default(),
                target_formats: native::SubpassFormats::default(),
                visibility_query: (metal::MTLVisibilityResultMode::Disabled, 0),
                pending_subpasses: Vec::new(),
                descriptor_sets: (0 .. MAX_BOUND_DESCRIPTOR_SETS)
                    .map(|_| DescriptorSetInfo::default())
                    .collect(),
            },
            temp: Temp {
                clear_vertices: Vec::new(),
                blit_vertices: FastHashMap::default(),
                clear_values: Vec::new(),
            },
            name: String::new(),
        }
    }

    /// Free command buffers which are allocated from this pool.
    unsafe fn free<I>(&mut self, cmd_buffers: I)
    where
        I: IntoIterator<Item = CommandBuffer>,
    {
        use hal::command::CommandBuffer as _;
        for mut cmd_buf in cmd_buffers {
            cmd_buf.reset(true);
            match self
                .allocated
                .iter_mut()
                .position(|b| Arc::ptr_eq(b, &cmd_buf.inner))
            {
                Some(index) => {
                    self.allocated.swap_remove(index);
                }
                None => error!("Unable to free a command buffer!"),
            }
        }
    }
}

impl CommandBuffer {
    fn update_depth_stencil(&self) {
        let mut inner = self.inner.borrow_mut();
        let mut pre = inner.sink().pre_render();
        if !pre.is_void() {
            let ds_store = &self.shared.service_pipes.depth_stencil_states;
            if let Some(desc) = self.state.build_depth_stencil() {
                let state = &**ds_store.get(desc, &self.shared.device);
                pre.issue(soft::RenderCommand::SetDepthStencilState(state));
            }
        }
    }
}

impl com::CommandBuffer<Backend> for CommandBuffer {
    unsafe fn begin(
        &mut self,
        flags: com::CommandBufferFlags,
        info: com::CommandBufferInheritanceInfo<Backend>,
    ) {
        self.reset(false);

        let mut inner = self.inner.borrow_mut();
        let can_immediate = inner.level == com::Level::Primary
            && flags.contains(com::CommandBufferFlags::ONE_TIME_SUBMIT);
        let sink = match self.pool_shared.borrow_mut().online_recording {
            OnlineRecording::Immediate if can_immediate => {
                let (cmd_buffer, token) = self.shared.queue.lock().spawn();
                cmd_buffer.set_label(&self.name);
                CommandSink::Immediate {
                    cmd_buffer,
                    token,
                    encoder_state: EncoderState::None,
                    num_passes: 0,
                    label: String::new(),
                }
            }
            #[cfg(feature = "dispatch")]
            OnlineRecording::Remote(_) if can_immediate => {
                let (cmd_buffer, token) = self.shared.queue.lock().spawn();
                cmd_buffer.set_label(&self.name);
                CommandSink::Remote {
                    queue: NoDebug(dispatch::Queue::with_target_queue(
                        "gfx-metal",
                        dispatch::QueueAttribute::Serial,
                        &self
                            .pool_shared
                            .borrow_mut()
                            .dispatch_queue
                            .as_ref()
                            .unwrap()
                            .0,
                    )),
                    cmd_buffer: Arc::new(Mutex::new(cmd_buffer)),
                    token,
                    pass: None,
                    capacity: inner.backup_capacity.take().unwrap_or_default(),
                    label: String::new(),
                }
            }
            _ => CommandSink::Deferred {
                is_encoding: false,
                is_inheriting: info.subpass.is_some(),
                journal: inner.backup_journal.take().unwrap_or_default(),
                label: String::new(),
            },
        };
        inner.sink = Some(sink);

        if let Some(framebuffer) = info.framebuffer {
            self.state.target_extent = framebuffer.extent;
        }
        if let Some(sp) = info.subpass {
            let subpass = &sp.main_pass.subpasses[sp.index];
            self.state.target_formats.copy_from(&subpass.target_formats);

            self.state.target_aspects = Aspects::empty();
            if !subpass.colors.is_empty() {
                self.state.target_aspects |= Aspects::COLOR;
            }
            if let Some((at_id, _)) = subpass.depth_stencil {
                let rat = &sp.main_pass.attachments[at_id];
                let aspects = rat.format.unwrap().surface_desc().aspects;
                self.state.target_aspects |= aspects;
            }

            match inner.sink {
                Some(CommandSink::Deferred {
                    ref mut is_encoding,
                    ref mut journal,
                    ref label,
                    ..
                }) => {
                    *is_encoding = true;
                    let pass_desc = metal::RenderPassDescriptor::new().to_owned();
                    journal
                        .passes
                        .alloc()
                        .init((soft::Pass::Render(pass_desc), 0 .. 0, label.clone()));
                }
                _ => {
                    warn!("Unexpected inheritance info on a primary command buffer");
                }
            }
        }
    }

    unsafe fn finish(&mut self) {
        self.inner.borrow_mut().sink().stop_encoding();
    }

    unsafe fn reset(&mut self, release_resources: bool) {
        self.state.reset_resources();
        self.inner
            .borrow_mut()
            .reset(&self.shared, release_resources);
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
    }

    unsafe fn fill_buffer<R>(&mut self, buffer: &native::Buffer, range: R, data: u32)
    where
        R: RangeArg<buffer::Offset>,
    {
        let (raw, base_range) = buffer.as_bound();
        let mut inner = self.inner.borrow_mut();

        let start = base_range.start + *range.start().unwrap_or(&0);
        assert_eq!(start % WORD_ALIGNMENT, 0);

        let end = match range.end() {
            Some(&e) => {
                assert_eq!(e % WORD_ALIGNMENT, 0);
                base_range.start + e
            }
            None => base_range.end,
        };

        if (data & 0xFF) * 0x0101_0101 == data {
            let command = soft::BlitCommand::FillBuffer {
                dst: AsNative::from(raw),
                range: start .. end,
                value: data as u8,
            };
            inner.sink().blit_commands(iter::once(command));
        } else {
            let pso = &*self.shared.service_pipes.fill_buffer;
            let length = (end - start) / WORD_ALIGNMENT;
            let value_and_length = [data, length as _];

            // TODO: Consider writing multiple values per thread in shader
            let threads_per_threadgroup = pso.thread_execution_width();
            let threadgroups = (length + threads_per_threadgroup - 1) / threads_per_threadgroup;

            let wg_count = MTLSize {
                width: threadgroups,
                height: 1,
                depth: 1,
            };
            let wg_size = MTLSize {
                width: threads_per_threadgroup,
                height: 1,
                depth: 1,
            };

            let commands = [
                soft::ComputeCommand::BindPipeline(pso),
                soft::ComputeCommand::BindBuffer {
                    index: 0,
                    buffer: AsNative::from(raw),
                    offset: start,
                },
                soft::ComputeCommand::BindBufferData {
                    index: 1,
                    words: &value_and_length[..],
                },
                soft::ComputeCommand::Dispatch { wg_size, wg_count },
            ];

            inner
                .sink()
                .quick_compute("fill_buffer", commands.iter().cloned());
        }
    }

    unsafe fn update_buffer(&mut self, dst: &native::Buffer, offset: buffer::Offset, data: &[u8]) {
        let (dst_raw, dst_range) = dst.as_bound();
        assert!(dst_range.start + offset + data.len() as buffer::Offset <= dst_range.end);

        let src = self.shared.device.lock().new_buffer_with_data(
            data.as_ptr() as _,
            data.len() as _,
            metal::MTLResourceOptions::CPUCacheModeWriteCombined,
        );
        src.set_label("update_buffer");

        let mut inner = self.inner.borrow_mut();
        {
            let command = soft::BlitCommand::CopyBuffer {
                src: AsNative::from(src.as_ref()),
                dst: AsNative::from(dst_raw),
                region: com::BufferCopy {
                    src: 0,
                    dst: dst_range.start + offset,
                    size: data.len() as _,
                },
            };

            inner.sink().blit_commands(iter::once(command));
        }

        inner.retained_buffers.push(src);
    }

    unsafe fn clear_image<T>(
        &mut self,
        image: &native::Image,
        _layout: Layout,
        value: com::ClearValue,
        subresource_ranges: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<SubresourceRange>,
    {
        let CommandBufferInner {
            ref mut retained_textures,
            ref mut sink,
            ..
        } = *self.inner.borrow_mut();

        let clear_color = image.shader_channel.interpret(value.color);
        let base_extent = image.kind.extent();
        let is_layered = !self.shared.disabilities.broken_layered_clear_image;

        autoreleasepool(|| {
            let raw = image.like.as_texture();
            for subresource_range in subresource_ranges {
                let sub = subresource_range.borrow();
                let num_layers = (sub.layers.end - sub.layers.start) as u64;
                let layers = if is_layered {
                    0 .. 1
                } else {
                    sub.layers.clone()
                };
                let texture = if is_layered && sub.layers.start > 0 {
                    // aliasing is necessary for bulk-clearing all layers starting with 0
                    let tex = raw.new_texture_view_from_slice(
                        image.mtl_format,
                        image.mtl_type,
                        NSRange {
                            location: 0,
                            length: raw.mipmap_level_count(),
                        },
                        NSRange {
                            location: sub.layers.start as _,
                            length: num_layers,
                        },
                    );
                    retained_textures.push(tex);
                    retained_textures.last().unwrap()
                } else {
                    raw
                };

                for layer in layers {
                    for level in sub.levels.clone() {
                        let descriptor = metal::RenderPassDescriptor::new().to_owned();
                        if base_extent.depth > 1 {
                            assert_eq!(sub.layers.end, 1);
                            let depth = base_extent.at_level(level).depth as u64;
                            descriptor.set_render_target_array_length(depth);
                        } else if is_layered {
                            descriptor.set_render_target_array_length(num_layers);
                        };

                        if image.format_desc.aspects.contains(Aspects::COLOR) {
                            let attachment = descriptor.color_attachments().object_at(0).unwrap();
                            attachment.set_texture(Some(texture));
                            attachment.set_level(level as _);
                            if !is_layered {
                                attachment.set_slice(layer as _);
                            }
                            attachment.set_store_action(metal::MTLStoreAction::Store);
                            if sub.aspects.contains(Aspects::COLOR) {
                                attachment.set_load_action(metal::MTLLoadAction::Clear);
                                attachment.set_clear_color(clear_color.clone());
                            } else {
                                attachment.set_load_action(metal::MTLLoadAction::Load);
                            }
                        } else {
                            assert!(!sub.aspects.contains(Aspects::COLOR));
                        };

                        if image.format_desc.aspects.contains(Aspects::DEPTH) {
                            let attachment = descriptor.depth_attachment().unwrap();
                            attachment.set_texture(Some(texture));
                            attachment.set_level(level as _);
                            if !is_layered {
                                attachment.set_slice(layer as _);
                            }
                            attachment.set_store_action(metal::MTLStoreAction::Store);
                            if sub.aspects.contains(Aspects::DEPTH) {
                                attachment.set_load_action(metal::MTLLoadAction::Clear);
                                attachment.set_clear_depth(value.depth_stencil.depth as _);
                            } else {
                                attachment.set_load_action(metal::MTLLoadAction::Load);
                            }
                        } else {
                            assert!(!sub.aspects.contains(Aspects::DEPTH));
                        };

                        if image.format_desc.aspects.contains(Aspects::STENCIL) {
                            let attachment = descriptor.stencil_attachment().unwrap();
                            attachment.set_texture(Some(texture));
                            attachment.set_level(level as _);
                            if !is_layered {
                                attachment.set_slice(layer as _);
                            }
                            attachment.set_store_action(metal::MTLStoreAction::Store);
                            if sub.aspects.contains(Aspects::STENCIL) {
                                attachment.set_load_action(metal::MTLLoadAction::Clear);
                                attachment.set_clear_stencil(value.depth_stencil.stencil);
                            } else {
                                attachment.set_load_action(metal::MTLLoadAction::Load);
                            }
                        } else {
                            assert!(!sub.aspects.contains(Aspects::STENCIL));
                        };

                        sink.as_mut().unwrap().quick_render(
                            "clear_image",
                            descriptor,
                            iter::empty(),
                        );
                    }
                }
            }
        });
    }

    unsafe fn clear_attachments<T, U>(&mut self, clears: T, rects: U)
    where
        T: IntoIterator,
        T::Item: Borrow<com::AttachmentClear>,
        U: IntoIterator,
        U::Item: Borrow<pso::ClearRect>,
    {
        // gather vertices/polygons
        let de = self.state.target_extent;
        let vertices = &mut self.temp.clear_vertices;
        vertices.clear();

        for rect in rects {
            let r = rect.borrow();
            for layer in r.layers.clone() {
                let data = [
                    [r.rect.x, r.rect.y],
                    [r.rect.x, r.rect.y + r.rect.h],
                    [r.rect.x + r.rect.w, r.rect.y + r.rect.h],
                    [r.rect.x + r.rect.w, r.rect.y],
                ];
                // now use the hard-coded index array to add 6 vertices to the list
                //TODO: could use instancing here
                // - with triangle strips
                // - with half of the data supplied per instance

                for &index in &[0usize, 1, 2, 2, 3, 0] {
                    let d = data[index];
                    vertices.alloc().init(ClearVertex {
                        pos: [
                            d[0] as f32 / de.width as f32,
                            d[1] as f32 / de.height as f32,
                            0.0, //TODO: depth Z
                            layer as f32,
                        ],
                    });
                }
            }
        }

        let mut vertex_is_dirty = true;
        let mut inner = self.inner.borrow_mut();
        let clear_pipes = &self.shared.service_pipes.clears;
        let ds_store = &self.shared.service_pipes.depth_stencil_states;
        let ds_state;

        //  issue a PSO+color switch and a draw for each requested clear
        let mut key = ClearKey {
            framebuffer_aspects: self.state.target_aspects,
            color_formats: [metal::MTLPixelFormat::Invalid; 1],
            depth_stencil_format: self
                .state
                .target_formats
                .depth_stencil
                .unwrap_or(metal::MTLPixelFormat::Invalid),
            target_index: None,
        };
        for (out, &(mtl_format, _)) in key
            .color_formats
            .iter_mut()
            .zip(&self.state.target_formats.colors)
        {
            *out = mtl_format;
        }

        for clear in clears {
            let pso; // has to live at least as long as all the commands
            let depth_stencil;
            let raw_value;

            let (com_clear, target_index) = match *clear.borrow() {
                com::AttachmentClear::Color { index, value } => {
                    let channel = self.state.target_formats.colors[index].1;
                    //Note: technically we should be able to derive the Channel from the
                    // `value` variant, but this is blocked by the portability that is
                    // always passing the attachment clears as `ClearColor::Sfloat` atm.
                    raw_value = com::ClearColor::from(value);
                    let com = soft::RenderCommand::BindBufferData {
                        stage: pso::Stage::Fragment,
                        index: 0,
                        words: slice::from_raw_parts(
                            raw_value.float32.as_ptr() as *const u32,
                            mem::size_of::<com::ClearColor>() / WORD_SIZE,
                        ),
                    };
                    (com, Some((index as u8, channel)))
                }
                com::AttachmentClear::DepthStencil { depth, stencil } => {
                    let mut aspects = Aspects::empty();
                    if let Some(value) = depth {
                        for v in vertices.iter_mut() {
                            v.pos[2] = value;
                        }
                        vertex_is_dirty = true;
                        aspects |= Aspects::DEPTH;
                    }
                    if stencil.is_some() {
                        //TODO: soft::RenderCommand::SetStencilReference
                        aspects |= Aspects::STENCIL;
                    }
                    depth_stencil = ds_store.get_write(aspects);
                    let com = soft::RenderCommand::SetDepthStencilState(&**depth_stencil);
                    (com, None)
                }
            };

            key.target_index = target_index;
            pso = clear_pipes.get(
                key,
                &self.shared.service_pipes.library,
                &self.shared.device,
                &self.shared.private_caps,
            );

            let com_pso = iter::once(soft::RenderCommand::BindPipeline(&**pso));
            let com_rast = iter::once(soft::RenderCommand::SetRasterizerState(
                native::RasterizerState::default(),
            ));

            let com_vertex = if vertex_is_dirty {
                vertex_is_dirty = false;
                Some(soft::RenderCommand::BindBufferData {
                    stage: pso::Stage::Vertex,
                    index: 0,
                    words: slice::from_raw_parts(
                        vertices.as_ptr() as *const u32,
                        vertices.len() * mem::size_of::<ClearVertex>() / WORD_SIZE,
                    ),
                })
            } else {
                None
            };

            let ext = self.state.target_extent;
            let rect = pso::Rect {
                x: 0,
                y: ext.height as _,
                w: ext.width as _,
                h: -(ext.height as i16),
            };
            let com_viewport = iter::once(soft::RenderCommand::SetViewport(rect, 0.0 .. 1.0));
            let com_scissor = iter::once(soft::RenderCommand::SetScissor(MTLScissorRect {
                x: 0,
                y: 0,
                width: ext.width as _,
                height: ext.height as _,
            }));

            let com_draw = iter::once(soft::RenderCommand::Draw {
                primitive_type: MTLPrimitiveType::Triangle,
                vertices: 0 .. vertices.len() as _,
                instances: 0 .. 1,
            });

            let commands = iter::once(com_clear)
                .chain(com_pso)
                .chain(com_rast)
                .chain(com_viewport)
                .chain(com_scissor)
                .chain(com_vertex)
                .chain(com_draw);

            inner.sink().pre_render().issue_many(commands);
        }

        // reset all the affected states
        let (com_pso, com_rast) = self.state.make_pso_commands();

        let device_lock = &self.shared.device;
        let com_ds = match self.state.build_depth_stencil() {
            Some(desc) => {
                ds_state = ds_store.get(desc, device_lock);
                Some(soft::RenderCommand::SetDepthStencilState(&**ds_state))
            }
            None => None,
        };

        let com_vs = match (
            self.state.resources_vs.buffers.first(),
            self.state.resources_vs.buffer_offsets.first(),
        ) {
            (Some(&Some(buffer)), Some(&offset)) => Some(soft::RenderCommand::BindBuffer {
                stage: pso::Stage::Vertex,
                index: 0,
                buffer,
                offset,
            }),
            _ => None,
        };
        let com_ps = match (
            self.state.resources_ps.buffers.first(),
            self.state.resources_ps.buffer_offsets.first(),
        ) {
            (Some(&Some(buffer)), Some(&offset)) => Some(soft::RenderCommand::BindBuffer {
                stage: pso::Stage::Fragment,
                index: 0,
                buffer,
                offset,
            }),
            _ => None,
        };

        let commands = com_pso
            .into_iter()
            .chain(com_rast)
            .chain(com_ds)
            .chain(com_vs)
            .chain(com_ps);

        inner.sink().pre_render().issue_many(commands);

        vertices.clear();
    }

    unsafe fn resolve_image<T>(
        &mut self,
        _src: &native::Image,
        _src_layout: Layout,
        _dst: &native::Image,
        _dst_layout: Layout,
        _regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ImageResolve>,
    {
        unimplemented!()
    }

    unsafe fn blit_image<T>(
        &mut self,
        src: &native::Image,
        _src_layout: Layout,
        dst: &native::Image,
        _dst_layout: Layout,
        filter: Filter,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ImageBlit>,
    {
        let CommandBufferInner {
            ref mut retained_textures,
            ref mut sink,
            ..
        } = *self.inner.borrow_mut();

        let src_cubish = src.view_cube_as_2d();
        let dst_cubish = dst.view_cube_as_2d();
        let dst_layers = dst.kind.num_layers();

        let vertices = &mut self.temp.blit_vertices;
        vertices.clear();

        let sampler = self.shared.service_pipes.sampler_states.get(filter);
        let ds_state;
        let key_mtl_type = match dst_cubish {
            Some(_) => metal::MTLTextureType::D2Array,
            None => dst.mtl_type,
        };
        let key = (
            key_mtl_type,
            dst.mtl_format,
            src.format_desc.aspects,
            dst.shader_channel,
        );
        let pso = self.shared.service_pipes.blits.get(
            key,
            &self.shared.service_pipes.library,
            &self.shared.device,
            &self.shared.private_caps,
        );

        for region in regions {
            let r = region.borrow();

            // layer count must be equal in both subresources
            debug_assert_eq!(
                r.src_subresource.layers.len(),
                r.dst_subresource.layers.len()
            );
            debug_assert_eq!(r.src_subresource.aspects, r.dst_subresource.aspects);
            debug_assert!(src.format_desc.aspects.contains(r.src_subresource.aspects));
            debug_assert!(dst.format_desc.aspects.contains(r.dst_subresource.aspects));

            let se = src.kind.extent().at_level(r.src_subresource.level);
            let de = dst.kind.extent().at_level(r.dst_subresource.level);
            //TODO: support 3D textures
            if se.depth != 1 || de.depth != 1 {
                warn!(
                    "3D image blits are not supported properly yet: {:?} -> {:?}",
                    se, de
                );
            }

            let layers = r
                .src_subresource
                .layers
                .clone()
                .zip(r.dst_subresource.layers.clone());
            let list = vertices
                .entry((r.dst_subresource.aspects, r.dst_subresource.level))
                .or_insert_with(Vec::new);

            for (src_layer, dst_layer) in layers {
                // this helper array defines unique data for quad vertices
                let data = [
                    [
                        r.src_bounds.start.x,
                        r.src_bounds.start.y,
                        r.dst_bounds.start.x,
                        r.dst_bounds.start.y,
                    ],
                    [
                        r.src_bounds.start.x,
                        r.src_bounds.end.y,
                        r.dst_bounds.start.x,
                        r.dst_bounds.end.y,
                    ],
                    [
                        r.src_bounds.end.x,
                        r.src_bounds.end.y,
                        r.dst_bounds.end.x,
                        r.dst_bounds.end.y,
                    ],
                    [
                        r.src_bounds.end.x,
                        r.src_bounds.start.y,
                        r.dst_bounds.end.x,
                        r.dst_bounds.start.y,
                    ],
                ];
                // now use the hard-coded index array to add 6 vertices to the list
                //TODO: could use instancing here
                // - with triangle strips
                // - with half of the data supplied per instance

                for &index in &[0usize, 1, 2, 2, 3, 0] {
                    let d = data[index];
                    list.alloc().init(BlitVertex {
                        uv: [
                            d[0] as f32 / se.width as f32,
                            d[1] as f32 / se.height as f32,
                            src_layer as f32,
                            r.src_subresource.level as f32,
                        ],
                        pos: [
                            d[2] as f32 / de.width as f32,
                            d[3] as f32 / de.height as f32,
                            0.0,
                            dst_layer as f32,
                        ],
                    });
                }
            }
        }

        // Note: we don't bother to restore any render states here, since we are currently
        // outside of a render pass, and the state will be reset automatically once
        // we enter the next pass.

        let src_native = AsNative::from(match src_cubish {
            Some(ref tex) => tex.as_ref(),
            None => src.like.as_texture(),
        });
        let prelude = [
            soft::RenderCommand::BindPipeline(&**pso),
            soft::RenderCommand::BindSamplers {
                stage: pso::Stage::Fragment,
                index: 0,
                samplers: &[Some(AsNative::from(sampler))][..],
            },
            soft::RenderCommand::BindTextures {
                stage: pso::Stage::Fragment,
                index: 0,
                textures: &[Some(src_native)][..],
            },
        ];

        let com_ds = if src
            .format_desc
            .aspects
            .intersects(Aspects::DEPTH | Aspects::STENCIL)
        {
            ds_state = self
                .shared
                .service_pipes
                .depth_stencil_states
                .get_write(src.format_desc.aspects);
            Some(soft::RenderCommand::SetDepthStencilState(&**ds_state))
        } else {
            None
        };

        let layered_rendering = self.shared.private_caps.layered_rendering;
        autoreleasepool(|| {
            let dst_new = match dst_cubish {
                Some(ref tex) => tex.as_ref(),
                None => dst.like.as_texture(),
            };

            for ((aspects, level), list) in vertices.drain() {
                let descriptor = metal::RenderPassDescriptor::new().to_owned();
                if layered_rendering {
                    descriptor.set_render_target_array_length(dst_layers as _);
                }

                if aspects.contains(Aspects::COLOR) {
                    let att = descriptor.color_attachments().object_at(0).unwrap();
                    att.set_texture(Some(dst_new));
                    att.set_level(level as _);
                }
                if aspects.contains(Aspects::DEPTH) {
                    let att = descriptor.depth_attachment().unwrap();
                    att.set_texture(Some(dst_new));
                    att.set_level(level as _);
                }
                if aspects.contains(Aspects::STENCIL) {
                    let att = descriptor.stencil_attachment().unwrap();
                    att.set_texture(Some(dst_new));
                    att.set_level(level as _);
                }

                let ext = dst.kind.extent().at_level(level);
                //Note: flipping Y coordinate of the destination here
                let rect = pso::Rect {
                    x: 0,
                    y: ext.height as _,
                    w: ext.width as _,
                    h: -(ext.height as i16),
                };

                let extra = [
                    soft::RenderCommand::SetViewport(rect, 0.0 .. 1.0),
                    soft::RenderCommand::SetScissor(MTLScissorRect {
                        x: 0,
                        y: 0,
                        width: ext.width as _,
                        height: ext.height as _,
                    }),
                    soft::RenderCommand::BindBufferData {
                        stage: pso::Stage::Vertex,
                        index: 0,
                        words: slice::from_raw_parts(
                            list.as_ptr() as *const u32,
                            list.len() * mem::size_of::<BlitVertex>() / WORD_SIZE,
                        ),
                    },
                    soft::RenderCommand::Draw {
                        primitive_type: MTLPrimitiveType::Triangle,
                        vertices: 0 .. list.len() as _,
                        instances: 0 .. 1,
                    },
                ];

                let commands = prelude.iter().chain(&com_ds).chain(&extra).cloned();

                sink.as_mut()
                    .unwrap()
                    .quick_render("blit_image", descriptor, commands);
            }
        });

        retained_textures.extend(src_cubish);
        retained_textures.extend(dst_cubish);
    }

    unsafe fn bind_index_buffer(&mut self, view: buffer::IndexBufferView<Backend>) {
        let (raw, range) = view.buffer.as_bound();
        assert!(range.start + view.offset < range.end); // conservative
        self.state.index_buffer = Some(IndexBuffer {
            buffer: AsNative::from(raw),
            offset: (range.start + view.offset) as _,
            stride: match view.index_type {
                IndexType::U16 => 2,
                IndexType::U32 => 4,
            },
        });
    }

    unsafe fn bind_vertex_buffers<I, T>(&mut self, first_binding: pso::BufferIndex, buffers: I)
    where
        I: IntoIterator<Item = (T, buffer::Offset)>,
        T: Borrow<native::Buffer>,
    {
        if self.state.vertex_buffers.len() <= first_binding as usize {
            self.state
                .vertex_buffers
                .resize(first_binding as usize + 1, None);
        }
        for (i, (buffer, offset)) in buffers.into_iter().enumerate() {
            let b = buffer.borrow();
            let (raw, range) = b.as_bound();
            let buffer_ptr = AsNative::from(raw);
            let index = first_binding as usize + i;
            self.state
                .vertex_buffers
                .entry(index)
                .set(Some((buffer_ptr, range.start + offset)));
        }

        if let Some(command) = self
            .state
            .set_vertex_buffers(self.shared.private_caps.max_buffers_per_stage as usize)
        {
            self.inner.borrow_mut().sink().pre_render().issue(command);
        }
    }

    unsafe fn set_viewports<T>(&mut self, first_viewport: u32, vps: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Viewport>,
    {
        // macOS_GPUFamily1_v3 supports >1 viewport, todo
        if first_viewport != 0 {
            panic!("First viewport != 0; Metal supports only one viewport");
        }
        let mut vps = vps.into_iter();
        let vp_borrowable = vps
            .next()
            .expect("No viewport provided, Metal supports exactly one");
        let vp = vp_borrowable.borrow();
        if vps.next().is_some() {
            // TODO should we panic here or set buffer in an erroneous state?
            panic!("More than one viewport set; Metal supports only one viewport");
        }

        let com = self.state.set_viewport(vp, self.shared.disabilities);
        self.inner.borrow_mut().sink().pre_render().issue(com);
    }

    unsafe fn set_scissors<T>(&mut self, first_scissor: u32, rects: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Rect>,
    {
        // macOS_GPUFamily1_v3 supports >1 scissor/viewport, todo
        if first_scissor != 0 {
            panic!("First scissor != 0; Metal supports only one viewport");
        }
        let mut rects = rects.into_iter();
        let rect_borrowable = rects
            .next()
            .expect("No scissor provided, Metal supports exactly one");
        let rect = rect_borrowable.borrow();
        if rects.next().is_some() {
            panic!("More than one scissor set; Metal supports only one viewport");
        }

        let com = self.state.set_scissor(*rect);
        self.inner.borrow_mut().sink().pre_render().issue(com);
    }

    unsafe fn set_blend_constants(&mut self, color: pso::ColorValue) {
        let com = self.state.set_blend_color(&color);
        self.inner.borrow_mut().sink().pre_render().issue(com);
    }

    unsafe fn set_depth_bounds(&mut self, _: Range<f32>) {
        warn!("Depth bounds test is not supported");
    }

    unsafe fn set_line_width(&mut self, width: f32) {
        // Note from the Vulkan spec:
        // > If the wide lines feature is not enabled, lineWidth must be 1.0
        // Simply assert and no-op because Metal never exposes `Features::LINE_WIDTH`
        assert_eq!(width, 1.0);
    }

    unsafe fn set_depth_bias(&mut self, depth_bias: pso::DepthBias) {
        let com = self.state.set_depth_bias(&depth_bias);
        self.inner.borrow_mut().sink().pre_render().issue(com);
    }

    unsafe fn set_stencil_reference(&mut self, faces: pso::Face, value: pso::StencilValue) {
        assign_sides(&mut self.state.stencil.reference_values, faces, value);
        let com =
            soft::RenderCommand::SetStencilReferenceValues(self.state.stencil.reference_values);
        self.inner.borrow_mut().sink().pre_render().issue(com);
    }

    unsafe fn set_stencil_read_mask(&mut self, faces: pso::Face, value: pso::StencilValue) {
        assign_sides(&mut self.state.stencil.read_masks, faces, value);
        self.update_depth_stencil();
    }

    unsafe fn set_stencil_write_mask(&mut self, faces: pso::Face, value: pso::StencilValue) {
        assign_sides(&mut self.state.stencil.write_masks, faces, value);
        self.update_depth_stencil();
    }

    unsafe fn begin_render_pass<T>(
        &mut self,
        render_pass: &native::RenderPass,
        framebuffer: &native::Framebuffer,
        _render_area: pso::Rect,
        clear_values: T,
        first_subpass_contents: com::SubpassContents,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ClearValue>,
    {
        // fill out temporary clear values per attachment
        self.temp
            .clear_values
            .resize(render_pass.attachments.len(), None);
        for ((out_val, _), in_val) in self
            .temp
            .clear_values
            .iter_mut()
            .zip(&render_pass.attachments)
            .filter(|(_, rat)| rat.has_clears())
            .zip(clear_values)
        {
            *out_val = Some(*in_val.borrow());
        }

        self.state.pending_subpasses.clear();
        self.state.target_extent = framebuffer.extent;

        //TODO: cache produced `RenderPassDescriptor` objects
        // we stack the subpasses in the opposite order
        for subpass in render_pass.subpasses.iter().rev() {
            let mut combined_aspects = Aspects::empty();
            let descriptor = autoreleasepool(|| {
                let descriptor = metal::RenderPassDescriptor::new().to_owned();
                descriptor.set_visibility_result_buffer(Some(&self.shared.visibility.buffer));
                if self.shared.private_caps.layered_rendering {
                    descriptor.set_render_target_array_length(framebuffer.extent.depth as _);
                }

                for (i, &(at_id, op_flags, resolve_id)) in subpass.colors.iter().enumerate() {
                    let rat = &render_pass.attachments[at_id];
                    let texture = framebuffer.attachments[at_id].as_ref();
                    let desc = descriptor.color_attachments().object_at(i as _).unwrap();

                    combined_aspects |= Aspects::COLOR;
                    desc.set_texture(Some(texture));

                    if op_flags.contains(native::SubpassOps::LOAD) {
                        desc.set_load_action(conv::map_load_operation(rat.ops.load));
                        if rat.ops.load == AttachmentLoadOp::Clear {
                            let channel = subpass.target_formats.colors[i].1;
                            let raw = self.temp.clear_values[at_id].unwrap().color;
                            desc.set_clear_color(channel.interpret(raw));
                        }
                    }
                    if let Some(id) = resolve_id {
                        let resolve = &framebuffer.attachments[id];
                        //Note: the selection of levels and slices is already handled by `ImageView`
                        desc.set_resolve_texture(Some(resolve));
                        desc.set_store_action(conv::map_resolved_store_operation(rat.ops.store));
                    } else if op_flags.contains(native::SubpassOps::STORE) {
                        desc.set_store_action(conv::map_store_operation(rat.ops.store));
                    }
                }

                if let Some((at_id, op_flags)) = subpass.depth_stencil {
                    let rat = &render_pass.attachments[at_id];
                    let texture = framebuffer.attachments[at_id].as_ref();
                    let aspects = rat.format.unwrap().surface_desc().aspects;
                    combined_aspects |= aspects;

                    if aspects.contains(Aspects::DEPTH) {
                        let desc = descriptor.depth_attachment().unwrap();
                        desc.set_texture(Some(texture));

                        if op_flags.contains(native::SubpassOps::LOAD) {
                            desc.set_load_action(conv::map_load_operation(rat.ops.load));
                            if rat.ops.load == AttachmentLoadOp::Clear {
                                let raw = self.temp.clear_values[at_id].unwrap().depth_stencil;
                                desc.set_clear_depth(raw.depth as f64);
                            }
                        }
                        if op_flags.contains(native::SubpassOps::STORE) {
                            desc.set_store_action(conv::map_store_operation(rat.ops.store));
                        }
                    }
                    if aspects.contains(Aspects::STENCIL) {
                        let desc = descriptor.stencil_attachment().unwrap();
                        desc.set_texture(Some(texture));

                        if op_flags.contains(native::SubpassOps::LOAD) {
                            desc.set_load_action(conv::map_load_operation(rat.stencil_ops.load));
                            if rat.stencil_ops.load == AttachmentLoadOp::Clear {
                                let raw = self.temp.clear_values[at_id].unwrap().depth_stencil;
                                desc.set_clear_stencil(raw.stencil);
                            }
                        }
                        if op_flags.contains(native::SubpassOps::STORE) {
                            desc.set_store_action(conv::map_store_operation(rat.stencil_ops.store));
                        }
                    }
                }

                descriptor
            });

            self.state.pending_subpasses.alloc().init(SubpassInfo {
                descriptor,
                combined_aspects,
                formats: subpass.target_formats.clone(),
            });
        }

        self.inner.borrow_mut().sink().label(&render_pass.name);
        self.next_subpass(first_subpass_contents);
    }

    unsafe fn next_subpass(&mut self, _contents: com::SubpassContents) {
        let sin = self.state.pending_subpasses.pop().unwrap();

        self.state.render_pso_is_compatible = match self.state.render_pso {
            Some(ref ps) => ps.formats == sin.formats,
            None => false,
        };
        self.state.target_aspects = sin.combined_aspects;
        self.state.target_formats.copy_from(&sin.formats);

        let ds_store = &self.shared.service_pipes.depth_stencil_states;
        let ds_state;
        let com_ds = if sin
            .combined_aspects
            .intersects(Aspects::DEPTH | Aspects::STENCIL)
        {
            match self.state.build_depth_stencil() {
                Some(desc) => {
                    ds_state = ds_store.get(desc, &self.shared.device);
                    Some(soft::RenderCommand::SetDepthStencilState(&**ds_state))
                }
                None => None,
            }
        } else {
            None
        };

        let init_commands = self
            .state
            .make_render_commands(sin.combined_aspects)
            .chain(com_ds);

        autoreleasepool(|| {
            self.inner
                .borrow_mut()
                .sink()
                .switch_render(sin.descriptor)
                .issue_many(init_commands);
        });
    }

    unsafe fn end_render_pass(&mut self) {
        self.inner.borrow_mut().sink().stop_encoding();
    }

    unsafe fn bind_graphics_pipeline(&mut self, pipeline: &native::GraphicsPipeline) {
        let mut inner = self.inner.borrow_mut();
        let mut pre = inner.sink().pre_render();

        if let Some(ref stencil) = pipeline.depth_stencil_desc.stencil {
            if let pso::State::Static(value) = stencil.read_masks {
                self.state.stencil.read_masks = value;
            }
            if let pso::State::Static(value) = stencil.write_masks {
                self.state.stencil.write_masks = value;
            }
            if let pso::State::Static(value) = stencil.reference_values {
                self.state.stencil.reference_values = value;
                pre.issue(soft::RenderCommand::SetStencilReferenceValues(value));
            }
        }

        self.state.render_pso_is_compatible =
            pipeline.attachment_formats == self.state.target_formats;
        let set_pipeline = match self.state.render_pso {
            Some(ref ps) if ps.raw.as_ptr() == pipeline.raw.as_ptr() => false,
            Some(ref mut ps) => {
                ps.raw = pipeline.raw.to_owned();
                ps.vertex_buffers.clear();
                ps.vertex_buffers
                    .extend(pipeline.vertex_buffers.iter().cloned().map(Some));
                ps.ds_desc = pipeline.depth_stencil_desc;
                ps.formats.copy_from(&pipeline.attachment_formats);
                true
            }
            None => {
                self.state.render_pso = Some(RenderPipelineState {
                    raw: pipeline.raw.to_owned(),
                    ds_desc: pipeline.depth_stencil_desc,
                    vertex_buffers: pipeline.vertex_buffers.iter().cloned().map(Some).collect(),
                    formats: pipeline.attachment_formats.clone(),
                });
                true
            }
        };

        if self.state.render_pso_is_compatible {
            if set_pipeline {
                self.state.rasterizer_state = pipeline.rasterizer_state.clone();
                self.state.primitive_type = pipeline.primitive_type;

                pre.issue(soft::RenderCommand::BindPipeline(&*pipeline.raw));
                if let Some(ref rs) = pipeline.rasterizer_state {
                    pre.issue(soft::RenderCommand::SetRasterizerState(rs.clone()))
                }
                // re-bind vertex buffers
                if let Some(command) = self
                    .state
                    .set_vertex_buffers(self.shared.private_caps.max_buffers_per_stage as usize)
                {
                    pre.issue(command);
                }
                // re-bind push constants
                if let Some(pc) = pipeline.vs_pc_info {
                    if Some(pc) != self.state.resources_vs.push_constants {
                        // if we don't have enough constants, then binding will follow
                        if pc.count as usize <= self.state.push_constants.len() {
                            pre.issue(self.state.push_vs_constants(pc));
                        }
                    }
                }
                if let Some(pc) = pipeline.ps_pc_info {
                    if Some(pc) != self.state.resources_ps.push_constants
                        && pc.count as usize <= self.state.push_constants.len()
                    {
                        pre.issue(self.state.push_ps_constants(pc));
                    }
                }
            } else {
                debug_assert_eq!(self.state.rasterizer_state, pipeline.rasterizer_state);
                debug_assert_eq!(self.state.primitive_type, pipeline.primitive_type);
            }

            if let Some(desc) = self.state.build_depth_stencil() {
                let ds_store = &self.shared.service_pipes.depth_stencil_states;
                let state = &**ds_store.get(desc, &self.shared.device);
                pre.issue(soft::RenderCommand::SetDepthStencilState(state));
            }
        } else {
            // This may be tricky: we expect either another pipeline to be bound
            // (this overwriting these), or a new render pass started (thus using these).
            self.state.rasterizer_state = pipeline.rasterizer_state.clone();
            self.state.primitive_type = pipeline.primitive_type;
        }

        if let pso::State::Static(value) = pipeline.depth_bias {
            self.state.depth_bias = value;
            pre.issue(soft::RenderCommand::SetDepthBias(value));
        }

        if let Some(ref vp) = pipeline.baked_states.viewport {
            pre.issue(self.state.set_viewport(vp, self.shared.disabilities));
        }
        if let Some(rect) = pipeline.baked_states.scissor {
            pre.issue(self.state.set_scissor(rect));
        }
        if let Some(ref color) = pipeline.baked_states.blend_color {
            pre.issue(self.state.set_blend_color(color));
        }
    }

    unsafe fn bind_graphics_descriptor_sets<I, J>(
        &mut self,
        pipe_layout: &native::PipelineLayout,
        first_set: usize,
        sets: I,
        dynamic_offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<native::DescriptorSet>,
        J: IntoIterator,
        J::Item: Borrow<com::DescriptorSetOffset>,
    {
        let vbuf_count = self
            .state
            .render_pso
            .as_ref()
            .map_or(0, |pso| pso.vertex_buffers.len()) as ResourceIndex;
        assert!(
            pipe_layout.total.vs.buffers + vbuf_count
                <= self.shared.private_caps.max_buffers_per_stage
        );

        self.state.resources_vs.pre_allocate(&pipe_layout.total.vs);
        self.state.resources_ps.pre_allocate(&pipe_layout.total.ps);

        let mut dynamic_offset_iter = dynamic_offsets.into_iter();
        let mut inner = self.inner.borrow_mut();
        let mut pre = inner.sink().pre_render();
        let mut bind_range = {
            let first = &pipe_layout.infos[first_set].offsets;
            native::MultiStageData {
                vs: first.vs.map(|&i| i .. i),
                ps: first.ps.map(|&i| i .. i),
                cs: first.cs.map(|&i| i .. i),
            }
        };
        for ((info, desc_set), cached_ds) in pipe_layout.infos[first_set ..]
            .iter()
            .zip(sets)
            .zip(self.state.descriptor_sets[first_set ..].iter_mut())
        {
            match *desc_set.borrow() {
                native::DescriptorSet::Emulated {
                    ref pool,
                    ref layouts,
                    ref resources,
                } => {
                    let data = pool.read();

                    let end_vs_offsets = self.state.resources_vs.bind_set(
                        pso::ShaderStageFlags::VERTEX,
                        &*data,
                        info.offsets.vs.clone(),
                        layouts,
                        resources,
                    );
                    bind_range.vs.expand(end_vs_offsets);
                    let end_ps_offsets = self.state.resources_ps.bind_set(
                        pso::ShaderStageFlags::FRAGMENT,
                        &*data,
                        info.offsets.ps.clone(),
                        layouts,
                        resources,
                    );
                    bind_range.ps.expand(end_ps_offsets);

                    for (dyn_data, offset) in info
                        .dynamic_buffers
                        .iter()
                        .zip(dynamic_offset_iter.by_ref())
                    {
                        if dyn_data.vs != !0 {
                            self.state.resources_vs.buffer_offsets[dyn_data.vs as usize] +=
                                *offset.borrow() as buffer::Offset;
                        }
                        if dyn_data.ps != !0 {
                            self.state.resources_ps.buffer_offsets[dyn_data.ps as usize] +=
                                *offset.borrow() as buffer::Offset;
                        }
                    }
                }
                native::DescriptorSet::ArgumentBuffer {
                    ref raw,
                    raw_offset,
                    ref pool,
                    ref range,
                    stage_flags,
                    ..
                } => {
                    //Note: this is incompatible with the binding scheme below
                    if stage_flags.contains(pso::ShaderStageFlags::VERTEX) {
                        let index = info.offsets.vs.buffers;
                        self.state.resources_vs.buffers[index as usize] =
                            Some(AsNative::from(raw.as_ref()));
                        self.state.resources_vs.buffer_offsets[index as usize] = raw_offset;
                        pre.issue(soft::RenderCommand::BindBuffer {
                            stage: pso::Stage::Vertex,
                            index,
                            buffer: AsNative::from(raw.as_ref()),
                            offset: raw_offset,
                        });
                    }
                    if stage_flags.contains(pso::ShaderStageFlags::FRAGMENT) {
                        let index = info.offsets.ps.buffers;
                        self.state.resources_ps.buffers[index as usize] =
                            Some(AsNative::from(raw.as_ref()));
                        self.state.resources_ps.buffer_offsets[index as usize] = raw_offset;
                        pre.issue(soft::RenderCommand::BindBuffer {
                            stage: pso::Stage::Fragment,
                            index,
                            buffer: AsNative::from(raw.as_ref()),
                            offset: raw_offset,
                        });
                    }
                    if stage_flags
                        .intersects(pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::FRAGMENT)
                    {
                        cached_ds.graphics_resources.clear();
                        cached_ds.graphics_resources.extend(
                            pool.read().resources[range.start as usize .. range.end as usize]
                                .iter()
                                .filter_map(|ur| {
                                    ptr::NonNull::new(ur.ptr).map(|res| (res, ur.usage))
                                }),
                        );
                        pre.issue_many(cached_ds.graphics_resources.iter().map(
                            |&(resource, usage)| soft::RenderCommand::UseResource {
                                resource,
                                usage,
                            },
                        ));
                    }
                }
            }
        }

        // now bind all the affected resources
        for (stage, cache, range) in
            iter::once((pso::Stage::Vertex, &self.state.resources_vs, bind_range.vs)).chain(
                iter::once((
                    pso::Stage::Fragment,
                    &self.state.resources_ps,
                    bind_range.ps,
                )),
            )
        {
            if range.textures.start != range.textures.end {
                pre.issue(soft::RenderCommand::BindTextures {
                    stage,
                    index: range.textures.start,
                    textures: &cache.textures
                        [range.textures.start as usize .. range.textures.end as usize],
                });
            }
            if range.samplers.start != range.samplers.end {
                pre.issue(soft::RenderCommand::BindSamplers {
                    stage,
                    index: range.samplers.start,
                    samplers: &cache.samplers
                        [range.samplers.start as usize .. range.samplers.end as usize],
                });
            }
            if range.buffers.start != range.buffers.end {
                pre.issue(soft::RenderCommand::BindBuffers {
                    stage,
                    index: range.buffers.start,
                    buffers: {
                        let range = range.buffers.start as usize .. range.buffers.end as usize;
                        (&cache.buffers[range.clone()], &cache.buffer_offsets[range])
                    },
                });
            }
        }
    }

    unsafe fn bind_compute_pipeline(&mut self, pipeline: &native::ComputePipeline) {
        self.state.compute_pso = Some(pipeline.raw.clone());
        self.state.work_group_size = pipeline.work_group_size;

        let mut inner = self.inner.borrow_mut();
        let mut pre = inner.sink().pre_compute();

        pre.issue(soft::ComputeCommand::BindPipeline(&*pipeline.raw));

        if let Some(pc) = pipeline.pc_info {
            if Some(pc) != self.state.resources_cs.push_constants
                && pc.count as usize <= self.state.push_constants.len()
            {
                pre.issue(self.state.push_cs_constants(pc));
            }
        }
    }

    unsafe fn bind_compute_descriptor_sets<I, J>(
        &mut self,
        pipe_layout: &native::PipelineLayout,
        first_set: usize,
        sets: I,
        dynamic_offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<native::DescriptorSet>,
        J: IntoIterator,
        J::Item: Borrow<com::DescriptorSetOffset>,
    {
        self.state.resources_cs.pre_allocate(&pipe_layout.total.cs);

        let mut dynamic_offset_iter = dynamic_offsets.into_iter();
        let mut inner = self.inner.borrow_mut();
        let mut pre = inner.sink().pre_compute();
        let cache = &mut self.state.resources_cs;
        let mut bind_range = pipe_layout.infos[first_set].offsets.cs.map(|&i| i .. i);

        for ((info, desc_set), cached_ds) in pipe_layout.infos[first_set ..]
            .iter()
            .zip(sets)
            .zip(self.state.descriptor_sets[first_set ..].iter_mut())
        {
            let res_offset = &info.offsets.cs;
            match *desc_set.borrow() {
                native::DescriptorSet::Emulated {
                    ref pool,
                    ref layouts,
                    ref resources,
                } => {
                    let data = pool.read();

                    let end_offsets = cache.bind_set(
                        pso::ShaderStageFlags::COMPUTE,
                        &*data,
                        res_offset.clone(),
                        layouts,
                        resources,
                    );
                    bind_range.expand(end_offsets);

                    for (dyn_data, offset) in info
                        .dynamic_buffers
                        .iter()
                        .zip(dynamic_offset_iter.by_ref())
                    {
                        if dyn_data.cs != !0 {
                            cache.buffer_offsets[dyn_data.cs as usize] +=
                                *offset.borrow() as buffer::Offset;
                        }
                    }
                }
                native::DescriptorSet::ArgumentBuffer {
                    ref raw,
                    raw_offset,
                    ref pool,
                    ref range,
                    stage_flags,
                    ..
                } => {
                    if stage_flags.contains(pso::ShaderStageFlags::COMPUTE) {
                        let index = res_offset.buffers;
                        cache.buffers[index as usize] = Some(AsNative::from(raw.as_ref()));
                        cache.buffer_offsets[index as usize] = raw_offset;
                        pre.issue(soft::ComputeCommand::BindBuffer {
                            index,
                            buffer: AsNative::from(raw.as_ref()),
                            offset: raw_offset,
                        });

                        cached_ds.compute_resources.clear();
                        cached_ds.compute_resources.extend(
                            pool.read().resources[range.start as usize .. range.end as usize]
                                .iter()
                                .filter_map(|ur| {
                                    ptr::NonNull::new(ur.ptr).map(|res| (res, ur.usage))
                                }),
                        );
                        pre.issue_many(cached_ds.compute_resources.iter().map(
                            |&(resource, usage)| soft::ComputeCommand::UseResource {
                                resource,
                                usage,
                            },
                        ));
                    }
                }
            }
        }

        // now bind all the affected resources
        if bind_range.textures.start != bind_range.textures.end {
            pre.issue(soft::ComputeCommand::BindTextures {
                index: bind_range.textures.start,
                textures: &cache.textures
                    [bind_range.textures.start as usize .. bind_range.textures.end as usize],
            });
        }
        if bind_range.samplers.start != bind_range.samplers.end {
            pre.issue(soft::ComputeCommand::BindSamplers {
                index: bind_range.samplers.start,
                samplers: &cache.samplers
                    [bind_range.samplers.start as usize .. bind_range.samplers.end as usize],
            });
        }
        if bind_range.buffers.start != bind_range.buffers.end {
            pre.issue(soft::ComputeCommand::BindBuffers {
                index: bind_range.buffers.start,
                buffers: {
                    let range =
                        bind_range.buffers.start as usize .. bind_range.buffers.end as usize;
                    (&cache.buffers[range.clone()], &cache.buffer_offsets[range])
                },
            });
        }
    }

    unsafe fn dispatch(&mut self, count: WorkGroupCount) {
        let mut inner = self.inner.borrow_mut();
        let (mut pre, init) = inner.sink().switch_compute();
        if init {
            pre.issue_many(self.state.make_compute_commands());
        }

        pre.issue(soft::ComputeCommand::Dispatch {
            wg_size: self.state.work_group_size,
            wg_count: MTLSize {
                width: count[0] as _,
                height: count[1] as _,
                depth: count[2] as _,
            },
        });
    }

    unsafe fn dispatch_indirect(&mut self, buffer: &native::Buffer, offset: buffer::Offset) {
        let mut inner = self.inner.borrow_mut();
        let (mut pre, init) = inner.sink().switch_compute();
        if init {
            pre.issue_many(self.state.make_compute_commands());
        }

        let (raw, range) = buffer.as_bound();
        assert!(range.start + offset < range.end);

        pre.issue(soft::ComputeCommand::DispatchIndirect {
            wg_size: self.state.work_group_size,
            buffer: AsNative::from(raw),
            offset: range.start + offset,
        });
    }

    unsafe fn copy_buffer<T>(&mut self, src: &native::Buffer, dst: &native::Buffer, regions: T)
    where
        T: IntoIterator,
        T::Item: Borrow<com::BufferCopy>,
    {
        let pso = &*self.shared.service_pipes.copy_buffer;
        let wg_size = MTLSize {
            width: pso.thread_execution_width(),
            height: 1,
            depth: 1,
        };

        let (src_raw, src_range) = src.as_bound();
        let (dst_raw, dst_range) = dst.as_bound();

        let mut compute_datas = Vec::new();
        let mut inner = self.inner.borrow_mut();
        let mut blit_commands = Vec::new();
        let mut compute_commands = vec![
            //TODO: get rid of heap
            soft::ComputeCommand::BindPipeline(pso),
        ];

        for region in regions {
            let r = region.borrow();
            if r.size % WORD_SIZE as u64 == 0
                && r.src % WORD_SIZE as u64 == 0
                && r.dst % WORD_SIZE as u64 == 0
            {
                blit_commands.alloc().init(soft::BlitCommand::CopyBuffer {
                    src: AsNative::from(src_raw),
                    dst: AsNative::from(dst_raw),
                    region: com::BufferCopy {
                        src: r.src + src_range.start,
                        dst: r.dst + dst_range.start,
                        size: r.size,
                    },
                });
            } else {
                // not natively supported, going through a compute shader
                assert_eq!(0, r.size >> 32);
                let src_aligned = r.src & !(WORD_SIZE as u64 - 1);
                let dst_aligned = r.dst & !(WORD_SIZE as u64 - 1);
                let offsets = (r.src - src_aligned) | ((r.dst - dst_aligned) << 16);
                let size_and_offsets = [r.size as u32, offsets as u32];
                compute_datas.push(Box::new(size_and_offsets));

                let wg_count = MTLSize {
                    width: (r.size + wg_size.width - 1) / wg_size.width,
                    height: 1,
                    depth: 1,
                };

                compute_commands
                    .alloc()
                    .init(soft::ComputeCommand::BindBuffer {
                        index: 0,
                        buffer: AsNative::from(dst_raw),
                        offset: dst_aligned + dst_range.start,
                    });
                compute_commands
                    .alloc()
                    .init(soft::ComputeCommand::BindBuffer {
                        index: 1,
                        buffer: AsNative::from(src_raw),
                        offset: src_aligned + src_range.start,
                    });
                compute_commands
                    .alloc()
                    .init(soft::ComputeCommand::BindBufferData {
                        index: 2,
                        // Rust doesn't see that compute_datas will not lose this
                        // item and the boxed contents can't be moved otherwise.
                        words: mem::transmute(&compute_datas.last().unwrap()[..]),
                    });
                compute_commands
                    .alloc()
                    .init(soft::ComputeCommand::Dispatch { wg_size, wg_count });
            }
        }

        let sink = inner.sink();
        if !blit_commands.is_empty() {
            sink.blit_commands(blit_commands.into_iter());
        }
        if compute_commands.len() > 1 {
            // first is bind PSO
            sink.quick_compute("copy_buffer", compute_commands.into_iter());
        }
    }

    unsafe fn copy_image<T>(
        &mut self,
        src: &native::Image,
        src_layout: Layout,
        dst: &native::Image,
        dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ImageCopy>,
    {
        match (&src.like, &dst.like) {
            (&native::ImageLike::Unbound { .. }, _) | (_, &native::ImageLike::Unbound { .. }) => {
                panic!("Unexpected Image::Unbound");
            }
            (
                &native::ImageLike::Texture(ref src_raw),
                &native::ImageLike::Texture(ref dst_raw),
            ) => {
                let CommandBufferInner {
                    ref mut retained_textures,
                    ref mut sink,
                    ..
                } = *self.inner.borrow_mut();

                let new_src = if src.mtl_format == dst.mtl_format {
                    src_raw
                } else {
                    assert_eq!(src.format_desc.bits, dst.format_desc.bits);
                    let tex = src_raw.new_texture_view(dst.mtl_format);
                    retained_textures.push(tex);
                    retained_textures.last().unwrap()
                };

                let commands = regions.into_iter().filter_map(|region| {
                    let r = region.borrow();
                    if r.extent.is_empty() {
                        None
                    } else {
                        Some(soft::BlitCommand::CopyImage {
                            src: AsNative::from(new_src.as_ref()),
                            dst: AsNative::from(dst_raw.as_ref()),
                            region: r.clone(),
                        })
                    }
                });

                sink.as_mut().unwrap().blit_commands(commands);
            }
            (&native::ImageLike::Buffer(ref src_buffer), &native::ImageLike::Texture(_)) => {
                let src_extent = src.kind.extent();
                self.copy_buffer_to_image(
                    src_buffer,
                    dst,
                    dst_layout,
                    regions.into_iter().map(|region| {
                        let r = region.borrow();
                        com::BufferImageCopy {
                            buffer_offset: src.byte_offset(r.src_offset),
                            buffer_width: src_extent.width,
                            buffer_height: src_extent.height,
                            image_layers: r.dst_subresource.clone(),
                            image_offset: r.dst_offset,
                            image_extent: r.extent,
                        }
                    }),
                )
            }
            (&native::ImageLike::Texture(_), &native::ImageLike::Buffer(ref dst_buffer)) => {
                let dst_extent = dst.kind.extent();
                self.copy_image_to_buffer(
                    src,
                    src_layout,
                    dst_buffer,
                    regions.into_iter().map(|region| {
                        let r = region.borrow();
                        com::BufferImageCopy {
                            buffer_offset: dst.byte_offset(r.dst_offset),
                            buffer_width: dst_extent.width,
                            buffer_height: dst_extent.height,
                            image_layers: r.src_subresource.clone(),
                            image_offset: r.src_offset,
                            image_extent: r.extent,
                        }
                    }),
                )
            }
            (
                &native::ImageLike::Buffer(ref src_buffer),
                &native::ImageLike::Buffer(ref dst_buffer),
            ) => self.copy_buffer(
                src_buffer,
                dst_buffer,
                regions.into_iter().map(|region| {
                    let r = region.borrow();
                    com::BufferCopy {
                        src: src.byte_offset(r.src_offset),
                        dst: dst.byte_offset(r.dst_offset),
                        size: src.byte_extent(r.extent),
                    }
                }),
            ),
        }
    }

    unsafe fn copy_buffer_to_image<T>(
        &mut self,
        src: &native::Buffer,
        dst: &native::Image,
        _dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::BufferImageCopy>,
    {
        match dst.like {
            native::ImageLike::Unbound { .. } => {
                panic!("Unexpected Image::Unbound");
            }
            native::ImageLike::Texture(ref dst_raw) => {
                let (src_raw, src_range) = src.as_bound();
                let commands = regions.into_iter().filter_map(|region| {
                    let r = region.borrow();
                    if r.image_extent.is_empty() {
                        None
                    } else {
                        Some(soft::BlitCommand::CopyBufferToImage {
                            src: AsNative::from(src_raw),
                            dst: AsNative::from(dst_raw.as_ref()),
                            dst_desc: dst.format_desc,
                            region: com::BufferImageCopy {
                                buffer_offset: r.buffer_offset + src_range.start,
                                ..r.clone()
                            },
                        })
                    }
                });
                self.inner.borrow_mut().sink().blit_commands(commands);
            }
            native::ImageLike::Buffer(ref dst_buffer) => self.copy_buffer(
                src,
                dst_buffer,
                regions.into_iter().map(|region| {
                    let r = region.borrow();
                    com::BufferCopy {
                        src: r.buffer_offset,
                        dst: dst.byte_offset(r.image_offset),
                        size: dst.byte_extent(r.image_extent),
                    }
                }),
            ),
        }
    }

    unsafe fn copy_image_to_buffer<T>(
        &mut self,
        src: &native::Image,
        _src_layout: Layout,
        dst: &native::Buffer,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::BufferImageCopy>,
    {
        match src.like {
            native::ImageLike::Unbound { .. } => {
                panic!("Unexpected Image::Unbound");
            }
            native::ImageLike::Texture(ref src_raw) => {
                let (dst_raw, dst_range) = dst.as_bound();
                let commands = regions.into_iter().filter_map(|region| {
                    let r = region.borrow();
                    if r.image_extent.is_empty() {
                        None
                    } else {
                        Some(soft::BlitCommand::CopyImageToBuffer {
                            src: AsNative::from(src_raw.as_ref()),
                            src_desc: src.format_desc,
                            dst: AsNative::from(dst_raw),
                            region: com::BufferImageCopy {
                                buffer_offset: r.buffer_offset + dst_range.start,
                                ..r.clone()
                            },
                        })
                    }
                });
                self.inner.borrow_mut().sink().blit_commands(commands);
            }
            native::ImageLike::Buffer(ref src_buffer) => self.copy_buffer(
                src_buffer,
                dst,
                regions.into_iter().map(|region| {
                    let r = region.borrow();
                    com::BufferCopy {
                        src: src.byte_offset(r.image_offset),
                        dst: r.buffer_offset,
                        size: src.byte_extent(r.image_extent),
                    }
                }),
            ),
        }
    }

    unsafe fn draw(&mut self, vertices: Range<VertexCount>, instances: Range<InstanceCount>) {
        debug_assert!(self.state.render_pso_is_compatible);
        if instances.start == instances.end {
            return;
        }

        let command = soft::RenderCommand::Draw {
            primitive_type: self.state.primitive_type,
            vertices,
            instances,
        };
        self.inner.borrow_mut().sink().pre_render().issue(command);
    }

    unsafe fn draw_indexed(
        &mut self,
        indices: Range<IndexCount>,
        base_vertex: VertexOffset,
        instances: Range<InstanceCount>,
    ) {
        debug_assert!(self.state.render_pso_is_compatible);
        if instances.start == instances.end {
            return;
        }

        let command = soft::RenderCommand::DrawIndexed {
            primitive_type: self.state.primitive_type,
            index: self
                .state
                .index_buffer
                .clone()
                .expect("must bind index buffer"),
            indices,
            base_vertex,
            instances,
        };
        self.inner.borrow_mut().sink().pre_render().issue(command);
    }

    unsafe fn draw_indirect(
        &mut self,
        buffer: &native::Buffer,
        offset: buffer::Offset,
        count: DrawCount,
        stride: u32,
    ) {
        assert_eq!(offset % WORD_ALIGNMENT, 0);
        assert_eq!(stride % WORD_ALIGNMENT as u32, 0);
        debug_assert!(self.state.render_pso_is_compatible);
        let (raw, range) = buffer.as_bound();

        let commands = (0 .. count).map(|i| soft::RenderCommand::DrawIndirect {
            primitive_type: self.state.primitive_type,
            buffer: AsNative::from(raw),
            offset: range.start + offset + (i * stride) as buffer::Offset,
        });

        self.inner
            .borrow_mut()
            .sink()
            .pre_render()
            .issue_many(commands);
    }

    unsafe fn draw_indexed_indirect(
        &mut self,
        buffer: &native::Buffer,
        offset: buffer::Offset,
        count: DrawCount,
        stride: u32,
    ) {
        assert_eq!(offset % WORD_ALIGNMENT, 0);
        assert_eq!(stride % WORD_ALIGNMENT as u32, 0);
        debug_assert!(self.state.render_pso_is_compatible);
        let (raw, range) = buffer.as_bound();

        let commands = (0 .. count).map(|i| soft::RenderCommand::DrawIndexedIndirect {
            primitive_type: self.state.primitive_type,
            index: self
                .state
                .index_buffer
                .clone()
                .expect("must bind index buffer"),
            buffer: AsNative::from(raw),
            offset: range.start + offset + (i * stride) as buffer::Offset,
        });

        self.inner
            .borrow_mut()
            .sink()
            .pre_render()
            .issue_many(commands);
    }

    unsafe fn set_event(&mut self, event: &native::Event, _: pso::PipelineStage) {
        self.inner
            .borrow_mut()
            .events
            .push((Arc::clone(&event.0), true));
    }

    unsafe fn reset_event(&mut self, event: &native::Event, _: pso::PipelineStage) {
        self.inner
            .borrow_mut()
            .events
            .push((Arc::clone(&event.0), false));
    }

    unsafe fn wait_events<'a, I, J>(
        &mut self,
        events: I,
        stages: Range<pso::PipelineStage>,
        barriers: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<native::Event>,
        J: IntoIterator,
        J::Item: Borrow<memory::Barrier<'a, Backend>>,
    {
        let mut need_barrier = false;

        for event in events {
            let mut inner = self.inner.borrow_mut();
            let event = &event.borrow().0;
            let is_local = inner
                .events
                .iter()
                .rfind(|ev| Arc::ptr_eq(&ev.0, event))
                .map_or(false, |ev| ev.1);
            if is_local {
                need_barrier = true;
            } else {
                inner.host_events.push(Arc::clone(event));
            }
        }

        if need_barrier {
            self.pipeline_barrier(stages, memory::Dependencies::empty(), barriers);
        }
    }

    unsafe fn begin_query(&mut self, query: query::Query<Backend>, flags: query::ControlFlags) {
        match query.pool {
            native::QueryPool::Occlusion(ref pool_range) => {
                debug_assert!(pool_range.start + query.id < pool_range.end);
                let offset = (query.id + pool_range.start) as buffer::Offset
                    * mem::size_of::<u64>() as buffer::Offset;
                let mode = if flags.contains(query::ControlFlags::PRECISE) {
                    metal::MTLVisibilityResultMode::Counting
                } else {
                    metal::MTLVisibilityResultMode::Boolean
                };

                let com = self.state.set_visibility_query(mode, offset);
                self.inner.borrow_mut().sink().pre_render().issue(com);
            }
        }
    }

    unsafe fn end_query(&mut self, query: query::Query<Backend>) {
        match query.pool {
            native::QueryPool::Occlusion(ref pool_range) => {
                let mut inner = self.inner.borrow_mut();
                debug_assert!(pool_range.start + query.id < pool_range.end);
                inner
                    .active_visibility_queries
                    .push(pool_range.start + query.id);

                let com = self
                    .state
                    .set_visibility_query(metal::MTLVisibilityResultMode::Disabled, 0);
                inner.sink().pre_render().issue(com);
            }
        }
    }

    unsafe fn reset_query_pool(&mut self, pool: &native::QueryPool, queries: Range<query::Id>) {
        let visibility = &self.shared.visibility;
        match *pool {
            native::QueryPool::Occlusion(ref pool_range) => {
                let mut inner = self.inner.borrow_mut();
                debug_assert!(pool_range.start + queries.end <= pool_range.end);
                inner.active_visibility_queries.retain(|&id| {
                    id < pool_range.start + queries.start || id >= pool_range.start + queries.end
                });

                let size_data = mem::size_of::<u64>() as buffer::Offset;
                let offset_data = pool_range.start as buffer::Offset * size_data;
                let command_data = soft::BlitCommand::FillBuffer {
                    dst: AsNative::from(visibility.buffer.as_ref()),
                    range: offset_data + queries.start as buffer::Offset * size_data
                        .. offset_data + queries.end as buffer::Offset * size_data,
                    value: 0,
                };

                let size_meta = mem::size_of::<u32>() as buffer::Offset;
                let offset_meta =
                    visibility.availability_offset + pool_range.start as buffer::Offset * size_meta;
                let command_meta = soft::BlitCommand::FillBuffer {
                    dst: AsNative::from(visibility.buffer.as_ref()),
                    range: offset_meta + queries.start as buffer::Offset * size_meta
                        .. offset_meta + queries.end as buffer::Offset * size_meta,
                    value: 0,
                };

                let commands = iter::once(command_data).chain(iter::once(command_meta));
                inner.sink().blit_commands(commands);
            }
        }
    }

    unsafe fn copy_query_pool_results(
        &mut self,
        pool: &native::QueryPool,
        queries: Range<query::Id>,
        buffer: &native::Buffer,
        offset: buffer::Offset,
        stride: buffer::Offset,
        flags: query::ResultFlags,
    ) {
        let (raw, range) = buffer.as_bound();
        match *pool {
            native::QueryPool::Occlusion(ref pool_range) => {
                let visibility = &self.shared.visibility;
                let size_data = mem::size_of::<u64>() as buffer::Offset;
                let size_meta = mem::size_of::<u32>() as buffer::Offset;

                if stride == size_data
                    && flags.contains(query::ResultFlags::BITS_64)
                    && !flags.contains(query::ResultFlags::WITH_AVAILABILITY)
                {
                    // if stride is matching, copy everything in one go
                    let com = soft::BlitCommand::CopyBuffer {
                        src: AsNative::from(visibility.buffer.as_ref()),
                        dst: AsNative::from(raw),
                        region: com::BufferCopy {
                            src: (pool_range.start + queries.start) as buffer::Offset * size_data,
                            dst: range.start + offset,
                            size: (queries.end - queries.start) as buffer::Offset * size_data,
                        },
                    };
                    self.inner
                        .borrow_mut()
                        .sink()
                        .blit_commands(iter::once(com));
                } else {
                    // copy parts of individual entries
                    let size_payload = if flags.contains(query::ResultFlags::BITS_64) {
                        mem::size_of::<u64>() as buffer::Offset
                    } else {
                        mem::size_of::<u32>() as buffer::Offset
                    };
                    let commands = (0 .. queries.end - queries.start).flat_map(|i| {
                        let absolute_index =
                            (pool_range.start + queries.start + i) as buffer::Offset;
                        let dst_offset = range.start + offset + i as buffer::Offset * stride;
                        let com_data = soft::BlitCommand::CopyBuffer {
                            src: AsNative::from(visibility.buffer.as_ref()),
                            dst: AsNative::from(raw),
                            region: com::BufferCopy {
                                src: absolute_index * size_data,
                                dst: dst_offset,
                                size: size_payload,
                            },
                        };

                        let (com_avail, com_pad) = if flags.contains(
                            query::ResultFlags::WITH_AVAILABILITY | query::ResultFlags::WAIT,
                        ) {
                            // Technically waiting is a no-op on a single queue. However,
                            // the client expects the availability to be set regardless.
                            let com = soft::BlitCommand::FillBuffer {
                                dst: AsNative::from(raw),
                                range: dst_offset + size_payload .. dst_offset + 2 * size_payload,
                                value: !0,
                            };
                            (Some(com), None)
                        } else if flags.contains(query::ResultFlags::WITH_AVAILABILITY) {
                            let com_avail = soft::BlitCommand::CopyBuffer {
                                src: AsNative::from(visibility.buffer.as_ref()),
                                dst: AsNative::from(raw),
                                region: com::BufferCopy {
                                    src: visibility.availability_offset
                                        + absolute_index * size_meta,
                                    dst: dst_offset + size_payload,
                                    size: size_meta,
                                },
                            };
                            // An extra padding is required if the client expects 64 bits availability without a wait
                            let com_pad = if flags.contains(query::ResultFlags::BITS_64) {
                                Some(soft::BlitCommand::FillBuffer {
                                    dst: AsNative::from(raw),
                                    range: dst_offset + size_payload + size_meta
                                        .. dst_offset + 2 * size_payload,
                                    value: 0,
                                })
                            } else {
                                None
                            };
                            (Some(com_avail), com_pad)
                        } else {
                            (None, None)
                        };

                        iter::once(com_data).chain(com_avail).chain(com_pad)
                    });
                    self.inner.borrow_mut().sink().blit_commands(commands);
                }
            }
        }
    }

    unsafe fn write_timestamp(&mut self, _: pso::PipelineStage, _: query::Query<Backend>) {
        // nothing to do, timestamps are unsupported on Metal
    }

    unsafe fn push_graphics_constants(
        &mut self,
        layout: &native::PipelineLayout,
        stages: pso::ShaderStageFlags,
        offset: u32,
        constants: &[u32],
    ) {
        self.state
            .update_push_constants(offset, constants, layout.total_push_constants);
        if stages.intersects(pso::ShaderStageFlags::GRAPHICS) {
            let mut inner = self.inner.borrow_mut();
            let mut pre = inner.sink().pre_render();
            // Note: the whole range is re-uploaded, which may be inefficient
            if stages.contains(pso::ShaderStageFlags::VERTEX) {
                let pc = layout.push_constants.vs.unwrap();
                pre.issue(self.state.push_vs_constants(pc));
            }
            if stages.contains(pso::ShaderStageFlags::FRAGMENT) {
                let pc = layout.push_constants.ps.unwrap();
                pre.issue(self.state.push_ps_constants(pc));
            }
        }
    }

    unsafe fn push_compute_constants(
        &mut self,
        layout: &native::PipelineLayout,
        offset: u32,
        constants: &[u32],
    ) {
        self.state
            .update_push_constants(offset, constants, layout.total_push_constants);
        let pc = layout.push_constants.cs.unwrap();

        // Note: the whole range is re-uploaded, which may be inefficient
        self.inner
            .borrow_mut()
            .sink()
            .pre_compute()
            .issue(self.state.push_cs_constants(pc));
    }

    unsafe fn execute_commands<'a, T, I>(&mut self, cmd_buffers: I)
    where
        T: 'a + Borrow<CommandBuffer>,
        I: IntoIterator<Item = &'a T>,
    {
        for cmd_buffer in cmd_buffers {
            let outer_borrowed = cmd_buffer.borrow();
            let inner_borrowed = outer_borrowed.inner.borrow_mut();

            let (exec_journal, is_inheriting) = match inner_borrowed.sink {
                Some(CommandSink::Deferred {
                    ref journal,
                    is_inheriting,
                    ..
                }) => (journal, is_inheriting),
                _ => panic!("Unexpected secondary sink!"),
            };

            for (a, b) in self
                .state
                .descriptor_sets
                .iter_mut()
                .zip(&outer_borrowed.state.descriptor_sets)
            {
                if !b.graphics_resources.is_empty() {
                    a.graphics_resources.clear();
                    a.graphics_resources
                        .extend_from_slice(&b.graphics_resources);
                }
                if !b.compute_resources.is_empty() {
                    a.compute_resources.clear();
                    a.compute_resources.extend_from_slice(&b.compute_resources);
                }
            }

            let mut inner_self = self.inner.borrow_mut();
            inner_self.events.extend_from_slice(&inner_borrowed.events);

            match *inner_self.sink() {
                CommandSink::Immediate {
                    ref mut cmd_buffer,
                    ref mut encoder_state,
                    ref mut num_passes,
                    ..
                } => {
                    if is_inheriting {
                        let encoder = match encoder_state {
                            EncoderState::Render(ref encoder) => encoder,
                            _ => panic!("Expected Render encoder!"),
                        };
                        for command in &exec_journal.render_commands {
                            exec_render(encoder, command, &exec_journal.resources);
                        }
                    } else {
                        encoder_state.end();
                        *num_passes += exec_journal.passes.len();
                        exec_journal.record(cmd_buffer);
                    }
                }
                CommandSink::Deferred {
                    ref mut journal, ..
                } => {
                    journal.extend(exec_journal, is_inheriting);
                }
                #[cfg(feature = "dispatch")]
                CommandSink::Remote { .. } => unimplemented!(),
            }
        }
    }
}
