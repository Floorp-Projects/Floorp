//! Command buffers.
//!
//! A command buffer collects a list of commands to be submitted to the device.
//! Each command buffer has specific capabilities for graphics, compute or transfer operations,
//! and can be either a "primary" command buffer or a "secondary" command buffer.  Operations
//! always start from a primary command buffer, but a primary command buffer can contain calls
//! to secondary command buffers that contain snippets of commands that do specific things, similar
//! to function calls.
//!
//! All the possible commands are implemented in the `RawCommandBuffer` trait, and then the `CommandBuffer`
//! and related types make a generic, strongly-typed wrapper around it that only expose the methods that
//! are valid for the capabilities it provides.

// TODO: Document pipelines and subpasses better.

mod clear;
mod structs;

use std::any::Any;
use std::borrow::Borrow;
use std::fmt;
use std::ops::Range;

use crate::image::{Filter, Layout, SubresourceRange};
use crate::memory::{Barrier, Dependencies};
use crate::{buffer, pass, pso, query};
use crate::{
    Backend,
    DrawCount,
    IndexCount,
    InstanceCount,
    VertexCount,
    VertexOffset,
    WorkGroupCount,
};

pub use self::clear::*;
pub use self::structs::*;

/// Offset for dynamic descriptors.
pub type DescriptorSetOffset = u32;

bitflags! {
    /// Option flags for various command buffer settings.
    #[derive(Default)]
    pub struct CommandBufferFlags: u32 {
        // TODO: Remove once 'const fn' is stabilized: https://github.com/rust-lang/rust/issues/24111
        /// No flags.
        const EMPTY = 0x0;

        /// Says that the command buffer will be recorded, submitted only once, and then reset and re-filled
        /// for another submission.
        const ONE_TIME_SUBMIT = 0x1;

        /// If set on a secondary command buffer, it says the command buffer takes place entirely inside
        /// a render pass. Ignored on primary command buffer.
        const RENDER_PASS_CONTINUE = 0x2;

        /// Says that a command buffer can be recorded into multiple primary command buffers,
        /// and submitted to a queue while it is still pending.
        const SIMULTANEOUS_USE = 0x4;
    }
}

/// An enum that indicates at runtime whether a command buffer
/// is primary or secondary, similar to what `command::Primary`
/// and `command::Secondary` do at compile-time.
#[allow(missing_docs)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Level {
    Primary,
    Secondary,
}

/// Specifies how commands for the following renderpasses will be recorded.
#[derive(Debug)]
pub enum SubpassContents {
    /// Contents of the subpass will be inline in the command buffer,
    /// NOT in secondary command buffers.
    Inline,
    /// Contents of the subpass will be in secondary command buffers, and
    /// the primary command buffer will only contain `execute_command()` calls
    /// until the subpass or render pass is complete.
    SecondaryBuffers,
}

#[allow(missing_docs)]
#[derive(Debug)]
pub struct CommandBufferInheritanceInfo<'a, B: Backend> {
    pub subpass: Option<pass::Subpass<'a, B>>,
    pub framebuffer: Option<&'a B::Framebuffer>,
    pub occlusion_query_enable: bool,
    pub occlusion_query_flags: query::ControlFlags,
    pub pipeline_statistics: query::PipelineStatistic,
}

impl<'a, B: Backend> Default for CommandBufferInheritanceInfo<'a, B> {
    fn default() -> Self {
        CommandBufferInheritanceInfo {
            subpass: None,
            framebuffer: None,
            occlusion_query_enable: false,
            occlusion_query_flags: query::ControlFlags::empty(),
            pipeline_statistics: query::PipelineStatistic::empty(),
        }
    }
}

/// A trait that describes all the operations that must be
/// provided by a `Backend`'s command buffer.
pub trait CommandBuffer<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Begins recording commands to a command buffer.
    unsafe fn begin(
        &mut self,
        flags: CommandBufferFlags,
        inheritance_info: CommandBufferInheritanceInfo<B>,
    );

    /// Begins recording a primary command buffer
    /// (that has no inheritance information).
    unsafe fn begin_primary(&mut self, flags: CommandBufferFlags) {
        self.begin(flags, CommandBufferInheritanceInfo::default());
    }

    /// Finish recording commands to a command buffer.
    unsafe fn finish(&mut self);

    /// Empties the command buffer, optionally releasing all
    /// resources from the commands that have been submitted.
    unsafe fn reset(&mut self, release_resources: bool);

    // TODO: This REALLY needs to be deeper, but it's complicated.
    // Should probably be a whole book chapter on synchronization and stuff really.
    /// Inserts a synchronization dependency between pipeline stages
    /// in the command buffer.
    unsafe fn pipeline_barrier<'a, T>(
        &mut self,
        stages: Range<pso::PipelineStage>,
        dependencies: Dependencies,
        barriers: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<Barrier<'a, B>>;

    /// Fill a buffer with the given `u32` value.
    unsafe fn fill_buffer(&mut self, buffer: &B::Buffer, range: buffer::SubRange, data: u32);

    /// Copy data from the given slice into a buffer.
    unsafe fn update_buffer(&mut self, buffer: &B::Buffer, offset: buffer::Offset, data: &[u8]);

    /// Clears an image to the given color/depth/stencil.
    unsafe fn clear_image<T>(
        &mut self,
        image: &B::Image,
        layout: Layout,
        value: ClearValue,
        subresource_ranges: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<SubresourceRange>;

    /// Takes an iterator of attachments and an iterator of rect's,
    /// and clears the given rect's for *each* attachment.
    unsafe fn clear_attachments<T, U>(&mut self, clears: T, rects: U)
    where
        T: IntoIterator,
        T::Item: Borrow<AttachmentClear>,
        U: IntoIterator,
        U::Item: Borrow<pso::ClearRect>;

    /// "Resolves" a multisampled image, converting it into a non-multisampled
    /// image. Takes an iterator of regions to apply the resolution to.
    unsafe fn resolve_image<T>(
        &mut self,
        src: &B::Image,
        src_layout: Layout,
        dst: &B::Image,
        dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<ImageResolve>;

    /// Copies regions from the source to destination image,
    /// applying scaling, filtering and potentially format conversion.
    unsafe fn blit_image<T>(
        &mut self,
        src: &B::Image,
        src_layout: Layout,
        dst: &B::Image,
        dst_layout: Layout,
        filter: Filter,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<ImageBlit>;

    /// Bind the index buffer view, making it the "current" one that draw commands
    /// will operate on.
    unsafe fn bind_index_buffer(&mut self, view: buffer::IndexBufferView<B>);

    /// Bind the vertex buffer set, making it the "current" one that draw commands
    /// will operate on.
    ///
    /// Each buffer passed corresponds to the vertex input binding with the same index,
    /// starting from an offset index `first_binding`. For example an iterator with
    /// two items and `first_binding` of 1 would fill vertex buffer binding numbers
    /// 1 and 2.
    ///
    /// This binding number refers only to binding points for vertex buffers and is
    /// completely separate from the binding numbers of `Descriptor`s in `DescriptorSet`s.
    /// It needs to match with the `VertexBufferDesc` and `AttributeDesc`s to which the
    /// data from each bound vertex buffer should flow.
    ///
    /// The `buffers` iterator should yield the `Buffer` to bind, as well as a subrange,
    /// in bytes, into that buffer where the vertex data that should be bound.
    unsafe fn bind_vertex_buffers<I, T>(&mut self, first_binding: pso::BufferIndex, buffers: I)
    where
        I: IntoIterator<Item = (T, buffer::SubRange)>,
        T: Borrow<B::Buffer>;

    /// Set the viewport parameters for the rasterizer.
    ///
    /// Each viewport passed corresponds to the viewport with the same index,
    /// starting from an offset index `first_viewport`.
    ///
    /// # Errors
    ///
    /// This function does not return an error. Invalid usage of this function
    /// will result in undefined behavior.
    ///
    /// - Command buffer must be in recording state.
    /// - Number of viewports must be between 1 and `max_viewports - first_viewport`.
    /// - The first viewport must be less than `max_viewports`.
    /// - Only queues with graphics capability support this function.
    /// - The bound pipeline must not have baked viewport state.
    /// - All viewports used by the pipeline must be specified before the first
    ///   draw call.
    unsafe fn set_viewports<T>(&mut self, first_viewport: u32, viewports: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Viewport>;

    /// Set the scissor rectangles for the rasterizer.
    ///
    /// Each scissor corresponds to the viewport with the same index, starting
    /// from an offset index `first_scissor`.
    ///
    /// # Errors
    ///
    /// This function does not return an error. Invalid usage of this function
    /// will result in undefined behavior.
    ///
    /// - Command buffer must be in recording state.
    /// - Number of scissors must be between 1 and `max_viewports - first_scissor`.
    /// - The first scissor must be less than `max_viewports`.
    /// - Only queues with graphics capability support this function.
    /// - The bound pipeline must not have baked scissor state.
    /// - All scissors used by the pipeline must be specified before the first draw
    ///   call.
    unsafe fn set_scissors<T>(&mut self, first_scissor: u32, rects: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Rect>;

    /// Sets the stencil reference value for comparison operations and store operations.
    /// Will be used on the LHS of stencil compare ops and as store value when the
    /// store op is Reference.
    unsafe fn set_stencil_reference(&mut self, faces: pso::Face, value: pso::StencilValue);

    /// Sets the stencil read mask.
    unsafe fn set_stencil_read_mask(&mut self, faces: pso::Face, value: pso::StencilValue);

    /// Sets the stencil write mask.
    unsafe fn set_stencil_write_mask(&mut self, faces: pso::Face, value: pso::StencilValue);

    /// Set the blend constant values dynamically.
    unsafe fn set_blend_constants(&mut self, color: pso::ColorValue);

    /// Set the depth bounds test values dynamically.
    unsafe fn set_depth_bounds(&mut self, bounds: Range<f32>);

    /// Set the line width dynamically.
    ///
    /// Only valid to call if `Features::LINE_WIDTH` is enabled.
    unsafe fn set_line_width(&mut self, width: f32);

    /// Set the depth bias dynamically.
    unsafe fn set_depth_bias(&mut self, depth_bias: pso::DepthBias);

    /// Begins recording commands for a render pass on the given framebuffer.
    /// `render_area` is the section of the framebuffer to render,
    /// `clear_values` is an iterator of `ClearValueRaw`'s to use to use for
    /// `clear_*` commands, one for each attachment of the render pass
    /// that has a clear operation.
    /// `first_subpass` specifies, for the first subpass, whether the
    /// rendering commands are provided inline or whether the render
    /// pass is composed of subpasses.
    unsafe fn begin_render_pass<T>(
        &mut self,
        render_pass: &B::RenderPass,
        framebuffer: &B::Framebuffer,
        render_area: pso::Rect,
        clear_values: T,
        first_subpass: SubpassContents,
    ) where
        T: IntoIterator,
        T::Item: Borrow<ClearValue>;

    /// Steps to the next subpass in the current render pass.
    unsafe fn next_subpass(&mut self, contents: SubpassContents);

    /// Finishes recording commands for the current a render pass.
    unsafe fn end_render_pass(&mut self);

    /// Bind a graphics pipeline.
    ///
    /// # Errors
    ///
    /// This function does not return an error. Invalid usage of this function
    /// will result in an error on `finish`.
    ///
    /// - Command buffer must be in recording state.
    /// - Only queues with graphics capability support this function.
    unsafe fn bind_graphics_pipeline(&mut self, pipeline: &B::GraphicsPipeline);

    /// Takes an iterator of graphics `DescriptorSet`'s, and binds them to the command buffer.
    /// `first_set` is the index that the first descriptor is mapped to in the command buffer.
    unsafe fn bind_graphics_descriptor_sets<I, J>(
        &mut self,
        layout: &B::PipelineLayout,
        first_set: usize,
        sets: I,
        offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<B::DescriptorSet>,
        J: IntoIterator,
        J::Item: Borrow<DescriptorSetOffset>;

    /// Bind a compute pipeline.
    ///
    /// # Errors
    ///
    /// This function does not return an error. Invalid usage of this function
    /// will result in an error on `finish`.
    ///
    /// - Command buffer must be in recording state.
    /// - Only queues with compute capability support this function.
    unsafe fn bind_compute_pipeline(&mut self, pipeline: &B::ComputePipeline);

    /// Takes an iterator of compute `DescriptorSet`'s, and binds them to the command buffer,
    /// `first_set` is the index that the first descriptor is mapped to in the command buffer.
    unsafe fn bind_compute_descriptor_sets<I, J>(
        &mut self,
        layout: &B::PipelineLayout,
        first_set: usize,
        sets: I,
        offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<B::DescriptorSet>,
        J: IntoIterator,
        J::Item: Borrow<DescriptorSetOffset>;

    /// Execute a workgroup in the compute pipeline. `x`, `y` and `z` are the
    /// number of local workgroups to dispatch along each "axis"; a total of `x`*`y`*`z`
    /// local workgroups will be created.
    ///
    /// # Errors
    ///
    /// This function does not return an error. Invalid usage of this function
    /// will result in an error on `finish`.
    ///
    /// - Command buffer must be in recording state.
    /// - A compute pipeline must be bound using `bind_compute_pipeline`.
    /// - Only queues with compute capability support this function.
    /// - This function must be called outside of a render pass.
    /// - `count` must be less than or equal to `Limits::max_compute_work_group_count`
    ///
    /// TODO:
    unsafe fn dispatch(&mut self, count: WorkGroupCount);

    /// Works similarly to `dispatch()` but reads parameters from the given
    /// buffer during execution.
    unsafe fn dispatch_indirect(&mut self, buffer: &B::Buffer, offset: buffer::Offset);

    /// Adds a command to copy regions from the source to destination buffer.
    unsafe fn copy_buffer<T>(&mut self, src: &B::Buffer, dst: &B::Buffer, regions: T)
    where
        T: IntoIterator,
        T::Item: Borrow<BufferCopy>;

    /// Copies regions from the source to the destination images, which
    /// have the given layouts.  No format conversion is done; the source and destination
    /// `Layout`'s **must** have the same sized image formats (such as `Rgba8Unorm` and
    /// `R32`, both of which are 32 bits).
    unsafe fn copy_image<T>(
        &mut self,
        src: &B::Image,
        src_layout: Layout,
        dst: &B::Image,
        dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<ImageCopy>;

    /// Copies regions from the source buffer to the destination image.
    unsafe fn copy_buffer_to_image<T>(
        &mut self,
        src: &B::Buffer,
        dst: &B::Image,
        dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<BufferImageCopy>;

    /// Copies regions from the source image to the destination buffer.
    unsafe fn copy_image_to_buffer<T>(
        &mut self,
        src: &B::Image,
        src_layout: Layout,
        dst: &B::Buffer,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<BufferImageCopy>;

    // TODO: This explanation needs improvement.
    /// Performs a non-indexed drawing operation, fetching vertex attributes
    /// from the currently bound vertex buffers.  It performs instanced
    /// drawing, drawing `instances.len()`
    /// times with an `instanceIndex` starting with the start of the range.
    unsafe fn draw(&mut self, vertices: Range<VertexCount>, instances: Range<InstanceCount>);

    /// Performs indexed drawing, drawing the range of indices
    /// given by the current index buffer and any bound vertex buffers.
    /// `base_vertex` specifies the vertex offset corresponding to index 0.
    /// That is, the offset into the vertex buffer is `(current_index + base_vertex)`
    ///
    /// It also performs instanced drawing, identical to `draw()`.
    unsafe fn draw_indexed(
        &mut self,
        indices: Range<IndexCount>,
        base_vertex: VertexOffset,
        instances: Range<InstanceCount>,
    );

    /// Functions identically to `draw()`, except the parameters are read
    /// from the given buffer, starting at `offset` and increasing `stride`
    /// bytes with each successive draw.  Performs `draw_count` draws total.
    /// `draw_count` may be zero.
    ///
    /// Each draw command in the buffer is a series of 4 `u32` values specifying,
    /// in order, the number of vertices to draw, the number of instances to draw,
    /// the index of the first vertex to draw, and the instance ID of the first
    /// instance to draw.
    unsafe fn draw_indirect(
        &mut self,
        buffer: &B::Buffer,
        offset: buffer::Offset,
        draw_count: DrawCount,
        stride: u32,
    );

    /// Like `draw_indirect()`, this does indexed drawing a la `draw_indexed()` but
    /// reads the draw parameters out of the given buffer.
    ///
    /// Each draw command in the buffer is a series of 5 values specifying,
    /// in order, the number of indices, the number of instances, the first index,
    /// the vertex offset, and the first instance.  All are `u32`'s except
    /// the vertex offset, which is an `i32`.
    unsafe fn draw_indexed_indirect(
        &mut self,
        buffer: &B::Buffer,
        offset: buffer::Offset,
        draw_count: DrawCount,
        stride: u32,
    );

    /// Signals an event once all specified stages of the shader pipeline have completed.
    unsafe fn set_event(&mut self, event: &B::Event, stages: pso::PipelineStage);

    /// Resets an event once all specified stages of the shader pipeline have completed.
    unsafe fn reset_event(&mut self, event: &B::Event, stages: pso::PipelineStage);

    /// Waits at some shader stage(s) until all events have been signalled.
    ///
    /// - `src_stages` specifies the shader pipeline stages in which the events were signalled.
    /// - `dst_stages` specifies the shader pipeline stages at which execution should wait.
    /// - `barriers` specifies a series of memory barriers to be executed before pipeline execution
    ///   resumes.
    unsafe fn wait_events<'a, I, J>(
        &mut self,
        events: I,
        stages: Range<pso::PipelineStage>,
        barriers: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<B::Event>,
        J: IntoIterator,
        J::Item: Borrow<Barrier<'a, B>>;

    /// Begins a query operation.  Queries count operations or record timestamps
    /// resulting from commands that occur between the beginning and end of the query,
    /// and save the results to the query pool.
    unsafe fn begin_query(&mut self, query: query::Query<B>, flags: query::ControlFlags);

    /// End a query.
    unsafe fn end_query(&mut self, query: query::Query<B>);

    /// Reset/clear the values in the given range of the query pool.
    unsafe fn reset_query_pool(&mut self, pool: &B::QueryPool, queries: Range<query::Id>);

    /// Copy query results into a buffer.
    unsafe fn copy_query_pool_results(
        &mut self,
        pool: &B::QueryPool,
        queries: Range<query::Id>,
        buffer: &B::Buffer,
        offset: buffer::Offset,
        stride: buffer::Offset,
        flags: query::ResultFlags,
    );

    /// Requests a timestamp to be written.
    unsafe fn write_timestamp(&mut self, stage: pso::PipelineStage, query: query::Query<B>);

    /// Modify constant data in a graphics pipeline. Push constants are intended to modify data in a
    /// pipeline more quickly than a updating the values inside a descriptor set.
    ///
    /// Push constants must be aligned to 4 bytes, and to guarantee alignment, this function takes a
    /// `&[u32]` instead of a `&[u8]`. Note that the offset is still specified in units of bytes.
    unsafe fn push_graphics_constants(
        &mut self,
        layout: &B::PipelineLayout,
        stages: pso::ShaderStageFlags,
        offset: u32,
        constants: &[u32],
    );

    /// Modify constant data in a compute pipeline. Push constants are intended to modify data in a
    /// pipeline more quickly than a updating the values inside a descriptor set.
    ///
    /// Push constants must be aligned to 4 bytes, and to guarantee alignment, this function takes a
    /// `&[u32]` instead of a `&[u8]`. Note that the offset is still specified in units of bytes.
    unsafe fn push_compute_constants(
        &mut self,
        layout: &B::PipelineLayout,
        offset: u32,
        constants: &[u32],
    );

    /// Execute the given secondary command buffers.
    unsafe fn execute_commands<'a, T, I>(&mut self, cmd_buffers: I)
    where
        T: 'a + Borrow<B::CommandBuffer>,
        I: IntoIterator<Item = &'a T>;

    /// Debug mark the current spot in the command buffer.
    unsafe fn insert_debug_marker(&mut self, name: &str, color: u32);
    /// Start a debug marker at the current place in the command buffer.
    unsafe fn begin_debug_marker(&mut self, name: &str, color: u32);
    /// End the last started debug marker scope.
    unsafe fn end_debug_marker(&mut self);
}
