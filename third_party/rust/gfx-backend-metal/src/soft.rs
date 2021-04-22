use crate::{
    command::IndexBuffer, native::RasterizerState, BufferPtr, ResourceIndex, ResourcePtr,
    SamplerPtr, TexturePtr,
};

use hal;
use metal;

use std::{fmt::Debug, ops::Range};

pub type CacheResourceIndex = u32;

pub trait Resources: Debug {
    type Data: Debug;
    type BufferArray: Debug;
    type TextureArray: Debug;
    type SamplerArray: Debug;
    type DepthStencil: Debug;
    type RenderPipeline: Debug;
    type ComputePipeline: Debug;
    type Marker: Debug + AsRef<str>;
}

#[derive(Clone, Debug, Default)]
pub struct Own {
    pub buffers: Vec<Option<BufferPtr>>,
    pub buffer_offsets: Vec<hal::buffer::Offset>,
    pub textures: Vec<Option<TexturePtr>>,
    pub samplers: Vec<Option<SamplerPtr>>,
}

impl Resources for Own {
    type Data = Vec<u32>;
    type BufferArray = Range<CacheResourceIndex>;
    type TextureArray = Range<CacheResourceIndex>;
    type SamplerArray = Range<CacheResourceIndex>;
    type DepthStencil = metal::DepthStencilState;
    type RenderPipeline = metal::RenderPipelineState;
    type ComputePipeline = metal::ComputePipelineState;
    type Marker = String;
}

#[derive(Debug)]
pub struct Ref;
impl<'a> Resources for &'a Ref {
    type Data = &'a [u32];
    type BufferArray = (&'a [Option<BufferPtr>], &'a [hal::buffer::Offset]);
    type TextureArray = &'a [Option<TexturePtr>];
    type SamplerArray = &'a [Option<SamplerPtr>];
    type DepthStencil = &'a metal::DepthStencilStateRef;
    type RenderPipeline = &'a metal::RenderPipelineStateRef;
    type ComputePipeline = &'a metal::ComputePipelineStateRef;
    type Marker = &'a str;
}

//TODO: Remove `Clone` from here, blocked by arguments of `quick_render` and
// `quick_compute` which currently use `cloned()` iteration.
#[derive(Clone, Debug)]
pub enum RenderCommand<R: Resources> {
    SetViewport(hal::pso::Rect, Range<f32>),
    SetScissor(metal::MTLScissorRect),
    SetBlendColor(hal::pso::ColorValue),
    SetDepthBias(hal::pso::DepthBias),
    SetDepthStencilState(R::DepthStencil),
    SetStencilReferenceValues(hal::pso::Sided<hal::pso::StencilValue>),
    SetRasterizerState(RasterizerState),
    SetVisibilityResult(metal::MTLVisibilityResultMode, hal::buffer::Offset),
    BindBuffer {
        stage: naga::ShaderStage,
        index: ResourceIndex,
        buffer: BufferPtr,
        offset: hal::buffer::Offset,
    },
    BindBuffers {
        stage: naga::ShaderStage,
        index: ResourceIndex,
        buffers: R::BufferArray,
    },
    BindBufferData {
        stage: naga::ShaderStage,
        index: ResourceIndex,
        words: R::Data,
    },
    BindTextures {
        stage: naga::ShaderStage,
        index: ResourceIndex,
        textures: R::TextureArray,
    },
    BindSamplers {
        stage: naga::ShaderStage,
        index: ResourceIndex,
        samplers: R::SamplerArray,
    },
    BindPipeline(R::RenderPipeline),
    UseResource {
        resource: ResourcePtr,
        usage: metal::MTLResourceUsage,
    },
    Draw {
        primitive_type: metal::MTLPrimitiveType,
        vertices: Range<hal::VertexCount>,
        instances: Range<hal::InstanceCount>,
    },
    DrawIndexed {
        primitive_type: metal::MTLPrimitiveType,
        index: IndexBuffer<BufferPtr>,
        indices: Range<hal::IndexCount>,
        base_vertex: hal::VertexOffset,
        instances: Range<hal::InstanceCount>,
    },
    DrawIndirect {
        primitive_type: metal::MTLPrimitiveType,
        buffer: BufferPtr,
        offset: hal::buffer::Offset,
    },
    DrawIndexedIndirect {
        primitive_type: metal::MTLPrimitiveType,
        index: IndexBuffer<BufferPtr>,
        buffer: BufferPtr,
        offset: hal::buffer::Offset,
    },
    InsertDebugMarker {
        name: R::Marker,
    },
    PushDebugMarker {
        name: R::Marker,
    },
    PopDebugGroup,
}

#[derive(Clone, Debug)]
pub enum BlitCommand {
    FillBuffer {
        dst: BufferPtr,
        range: Range<hal::buffer::Offset>,
        value: u8,
    },
    CopyBuffer {
        src: BufferPtr,
        dst: BufferPtr,
        region: hal::command::BufferCopy,
    },
    CopyImage {
        src: TexturePtr,
        dst: TexturePtr,
        region: hal::command::ImageCopy,
    },
    CopyBufferToImage {
        src: BufferPtr,
        dst: TexturePtr,
        dst_desc: hal::format::FormatDesc,
        region: hal::command::BufferImageCopy,
    },
    CopyImageToBuffer {
        src: TexturePtr,
        src_desc: hal::format::FormatDesc,
        dst: BufferPtr,
        region: hal::command::BufferImageCopy,
    },
}

#[derive(Clone, Debug)]
pub enum ComputeCommand<R: Resources> {
    BindBuffer {
        index: ResourceIndex,
        buffer: BufferPtr,
        offset: hal::buffer::Offset,
    },
    BindBuffers {
        index: ResourceIndex,
        buffers: R::BufferArray,
    },
    BindBufferData {
        index: ResourceIndex,
        words: R::Data,
    },
    BindTextures {
        index: ResourceIndex,
        textures: R::TextureArray,
    },
    BindSamplers {
        index: ResourceIndex,
        samplers: R::SamplerArray,
    },
    BindPipeline(R::ComputePipeline),
    UseResource {
        resource: ResourcePtr,
        usage: metal::MTLResourceUsage,
    },
    Dispatch {
        wg_size: metal::MTLSize,
        wg_count: metal::MTLSize,
    },
    DispatchIndirect {
        wg_size: metal::MTLSize,
        buffer: BufferPtr,
        offset: hal::buffer::Offset,
    },
}

#[derive(Clone, Debug)]
pub enum Pass {
    Render(metal::RenderPassDescriptor),
    Blit,
    Compute,
}

impl Own {
    pub fn clear(&mut self) {
        self.buffers.clear();
        self.buffer_offsets.clear();
        self.textures.clear();
        self.samplers.clear();
    }

    pub fn own_render(&mut self, com: RenderCommand<&Ref>) -> RenderCommand<Self> {
        use self::RenderCommand::*;
        match com {
            SetViewport(rect, depth) => SetViewport(rect, depth),
            SetScissor(rect) => SetScissor(rect),
            SetBlendColor(color) => SetBlendColor(color),
            SetDepthBias(bias) => SetDepthBias(bias),
            SetDepthStencilState(state) => SetDepthStencilState(state.to_owned()),
            SetStencilReferenceValues(sided) => SetStencilReferenceValues(sided),
            SetRasterizerState(ref state) => SetRasterizerState(state.clone()),
            SetVisibilityResult(mode, offset) => SetVisibilityResult(mode, offset),
            BindBuffer {
                stage,
                index,
                buffer,
                offset,
            } => BindBuffer {
                stage,
                index,
                buffer,
                offset,
            },
            BindBuffers {
                stage,
                index,
                buffers: (buffers, offsets),
            } => BindBuffers {
                stage,
                index,
                buffers: {
                    let start = self.buffers.len() as CacheResourceIndex;
                    self.buffers.extend_from_slice(buffers);
                    self.buffer_offsets.extend_from_slice(offsets);
                    start..self.buffers.len() as CacheResourceIndex
                },
            },
            BindBufferData {
                stage,
                index,
                words,
            } => BindBufferData {
                stage,
                index,
                words: words.to_vec(),
            },
            BindTextures {
                stage,
                index,
                textures,
            } => BindTextures {
                stage,
                index,
                textures: {
                    let start = self.textures.len() as CacheResourceIndex;
                    self.textures.extend_from_slice(textures);
                    start..self.textures.len() as CacheResourceIndex
                },
            },
            BindSamplers {
                stage,
                index,
                samplers,
            } => BindSamplers {
                stage,
                index,
                samplers: {
                    let start = self.samplers.len() as CacheResourceIndex;
                    self.samplers.extend_from_slice(samplers);
                    start..self.samplers.len() as CacheResourceIndex
                },
            },
            BindPipeline(pso) => BindPipeline(pso.to_owned()),
            UseResource { resource, usage } => UseResource { resource, usage },
            Draw {
                primitive_type,
                vertices,
                instances,
            } => Draw {
                primitive_type,
                vertices,
                instances,
            },
            DrawIndexed {
                primitive_type,
                index,
                indices,
                base_vertex,
                instances,
            } => DrawIndexed {
                primitive_type,
                index,
                indices,
                base_vertex,
                instances,
            },
            DrawIndirect {
                primitive_type,
                buffer,
                offset,
            } => DrawIndirect {
                primitive_type,
                buffer,
                offset,
            },
            DrawIndexedIndirect {
                primitive_type,
                index,
                buffer,
                offset,
            } => DrawIndexedIndirect {
                primitive_type,
                index,
                buffer,
                offset,
            },
            InsertDebugMarker { name } => InsertDebugMarker {
                name: name.to_owned(),
            },
            PushDebugMarker { name } => PushDebugMarker {
                name: name.to_owned(),
            },
            PopDebugGroup => PopDebugGroup,
        }
    }

    pub fn own_compute(&mut self, com: ComputeCommand<&Ref>) -> ComputeCommand<Self> {
        use self::ComputeCommand::*;
        match com {
            BindBuffer {
                index,
                buffer,
                offset,
            } => BindBuffer {
                index,
                buffer,
                offset,
            },
            BindBuffers {
                index,
                buffers: (buffers, offsets),
            } => BindBuffers {
                index,
                buffers: {
                    let start = self.buffers.len() as CacheResourceIndex;
                    self.buffers.extend_from_slice(buffers);
                    self.buffer_offsets.extend_from_slice(offsets);
                    start..self.buffers.len() as CacheResourceIndex
                },
            },
            BindBufferData { index, words } => BindBufferData {
                index,
                words: words.to_vec(),
            },
            BindTextures { index, textures } => BindTextures {
                index,
                textures: {
                    let start = self.textures.len() as CacheResourceIndex;
                    self.textures.extend_from_slice(textures);
                    start..self.textures.len() as CacheResourceIndex
                },
            },
            BindSamplers { index, samplers } => BindSamplers {
                index,
                samplers: {
                    let start = self.samplers.len() as CacheResourceIndex;
                    self.samplers.extend_from_slice(samplers);
                    start..self.samplers.len() as CacheResourceIndex
                },
            },
            BindPipeline(pso) => BindPipeline(pso.to_owned()),
            UseResource { resource, usage } => UseResource { resource, usage },
            Dispatch { wg_size, wg_count } => Dispatch { wg_size, wg_count },
            DispatchIndirect {
                wg_size,
                buffer,
                offset,
            } => DispatchIndirect {
                wg_size,
                buffer,
                offset,
            },
        }
    }

    pub fn rebase_render(&self, com: &mut RenderCommand<Own>) {
        use self::RenderCommand::*;
        match *com {
            SetViewport(..)
            | SetScissor(..)
            | SetBlendColor(..)
            | SetDepthBias(..)
            | SetDepthStencilState(..)
            | SetStencilReferenceValues(..)
            | SetRasterizerState(..)
            | SetVisibilityResult(..)
            | BindBuffer { .. } => {}
            BindBuffers {
                ref mut buffers, ..
            } => {
                buffers.start += self.buffers.len() as CacheResourceIndex;
                buffers.end += self.buffers.len() as CacheResourceIndex;
            }
            BindBufferData { .. } => {}
            BindTextures {
                ref mut textures, ..
            } => {
                textures.start += self.textures.len() as CacheResourceIndex;
                textures.end += self.textures.len() as CacheResourceIndex;
            }
            BindSamplers {
                ref mut samplers, ..
            } => {
                samplers.start += self.samplers.len() as CacheResourceIndex;
                samplers.end += self.samplers.len() as CacheResourceIndex;
            }
            BindPipeline(..)
            | UseResource { .. }
            | Draw { .. }
            | DrawIndexed { .. }
            | DrawIndirect { .. }
            | DrawIndexedIndirect { .. }
            | InsertDebugMarker { .. }
            | PushDebugMarker { .. }
            | PopDebugGroup => {}
        }
    }

    pub fn rebase_compute(&self, com: &mut ComputeCommand<Own>) {
        use self::ComputeCommand::*;
        match *com {
            BindBuffer { .. } => {}
            BindBuffers {
                ref mut buffers, ..
            } => {
                buffers.start += self.buffers.len() as CacheResourceIndex;
                buffers.end += self.buffers.len() as CacheResourceIndex;
            }
            BindBufferData { .. } => {}
            BindTextures {
                ref mut textures, ..
            } => {
                textures.start += self.textures.len() as CacheResourceIndex;
                textures.end += self.textures.len() as CacheResourceIndex;
            }
            BindSamplers {
                ref mut samplers, ..
            } => {
                samplers.start += self.samplers.len() as CacheResourceIndex;
                samplers.end += self.samplers.len() as CacheResourceIndex;
            }
            BindPipeline(..) | UseResource { .. } | Dispatch { .. } | DispatchIndirect { .. } => {}
        }
    }

    pub fn extend(&mut self, other: &Self) {
        self.buffers.extend_from_slice(&other.buffers);
        self.buffer_offsets.extend_from_slice(&other.buffer_offsets);
        self.textures.extend_from_slice(&other.textures);
        self.samplers.extend_from_slice(&other.samplers);
    }
}

/// This is a helper trait that allows us to unify owned and non-owned handling
/// of the context-dependent data, such as resource arrays.
pub trait AsSlice<T, R> {
    fn as_slice<'a>(&'a self, resources: &'a R) -> &'a [T];
}
impl<'b, T> AsSlice<Option<T>, &'b Ref> for &'b [Option<T>] {
    #[inline(always)]
    fn as_slice<'a>(&'a self, _: &'a &'b Ref) -> &'a [Option<T>] {
        self
    }
}
impl<'b> AsSlice<Option<BufferPtr>, &'b Ref>
    for (&'b [Option<BufferPtr>], &'b [hal::buffer::Offset])
{
    #[inline(always)]
    fn as_slice<'a>(&'a self, _: &'a &'b Ref) -> &'a [Option<BufferPtr>] {
        self.0
    }
}
impl<'b> AsSlice<hal::buffer::Offset, &'b Ref>
    for (&'b [Option<BufferPtr>], &'b [hal::buffer::Offset])
{
    #[inline(always)]
    fn as_slice<'a>(&'a self, _: &'a &'b Ref) -> &'a [hal::buffer::Offset] {
        self.1
    }
}
impl AsSlice<Option<BufferPtr>, Own> for Range<CacheResourceIndex> {
    #[inline(always)]
    fn as_slice<'a>(&'a self, resources: &'a Own) -> &'a [Option<BufferPtr>] {
        &resources.buffers[self.start as usize..self.end as usize]
    }
}
impl AsSlice<hal::buffer::Offset, Own> for Range<CacheResourceIndex> {
    #[inline(always)]
    fn as_slice<'a>(&'a self, resources: &'a Own) -> &'a [hal::buffer::Offset] {
        &resources.buffer_offsets[self.start as usize..self.end as usize]
    }
}
impl AsSlice<Option<TexturePtr>, Own> for Range<CacheResourceIndex> {
    #[inline(always)]
    fn as_slice<'a>(&'a self, resources: &'a Own) -> &'a [Option<TexturePtr>] {
        &resources.textures[self.start as usize..self.end as usize]
    }
}
impl AsSlice<Option<SamplerPtr>, Own> for Range<CacheResourceIndex> {
    #[inline(always)]
    fn as_slice<'a>(&'a self, resources: &'a Own) -> &'a [Option<SamplerPtr>] {
        &resources.samplers[self.start as usize..self.end as usize]
    }
}

fn _test_command_sizes(
    render: RenderCommand<&Ref>,
    blit: BlitCommand,
    compute: ComputeCommand<&Ref>,
) {
    use std::mem::transmute;
    let _ = unsafe {
        (
            transmute::<_, [usize; 6]>(render),
            transmute::<_, [usize; 9]>(blit),
            transmute::<_, [usize; 7]>(compute),
        )
    };
}
