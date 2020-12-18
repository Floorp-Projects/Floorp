use ash::{version::DeviceV1_0, vk};
use smallvec::SmallVec;
use std::{borrow::Borrow, ffi::CString, mem, ops::Range, slice, sync::Arc};

use inplace_it::inplace_or_alloc_array;

use crate::{conv, native as n, Backend, DebugMessenger, RawDevice};
use hal::{
    buffer, command as com,
    format::Aspects,
    image::{Filter, Layout, SubresourceRange},
    memory, pso, query, DrawCount, IndexCount, IndexType, InstanceCount, TaskCount, VertexCount,
    VertexOffset, WorkGroupCount,
};

#[derive(Debug)]
pub struct CommandBuffer {
    pub raw: vk::CommandBuffer,
    pub device: Arc<RawDevice>,
}

fn debug_color(color: u32) -> [f32; 4] {
    let mut result = [0.0; 4];
    for (i, c) in result.iter_mut().enumerate() {
        *c = ((color >> (24 - i * 8)) & 0xFF) as f32 / 255.0;
    }
    result
}

fn map_subpass_contents(contents: com::SubpassContents) -> vk::SubpassContents {
    match contents {
        com::SubpassContents::Inline => vk::SubpassContents::INLINE,
        com::SubpassContents::SecondaryBuffers => vk::SubpassContents::SECONDARY_COMMAND_BUFFERS,
    }
}

fn map_buffer_image_regions<T>(
    _image: &n::Image,
    regions: T,
) -> impl ExactSizeIterator<Item = vk::BufferImageCopy>
where
    T: IntoIterator,
    T::Item: Borrow<com::BufferImageCopy>,
    T::IntoIter: ExactSizeIterator,
{
    regions.into_iter().map(|region| {
        let r = region.borrow();
        let image_subresource = conv::map_subresource_layers(&r.image_layers);
        vk::BufferImageCopy {
            buffer_offset: r.buffer_offset,
            buffer_row_length: r.buffer_width,
            buffer_image_height: r.buffer_height,
            image_subresource,
            image_offset: conv::map_offset(r.image_offset),
            image_extent: conv::map_extent(r.image_extent),
        }
    })
}

struct BarrierSet {
    global: SmallVec<[vk::MemoryBarrier; 4]>,
    buffer: SmallVec<[vk::BufferMemoryBarrier; 4]>,
    image: SmallVec<[vk::ImageMemoryBarrier; 4]>,
}

fn destructure_barriers<'a, T>(barriers: T) -> BarrierSet
where
    T: IntoIterator,
    T::Item: Borrow<memory::Barrier<'a, Backend>>,
{
    let mut global: SmallVec<[vk::MemoryBarrier; 4]> = SmallVec::new();
    let mut buffer: SmallVec<[vk::BufferMemoryBarrier; 4]> = SmallVec::new();
    let mut image: SmallVec<[vk::ImageMemoryBarrier; 4]> = SmallVec::new();

    for barrier in barriers {
        match *barrier.borrow() {
            memory::Barrier::AllBuffers(ref access) => {
                global.push(
                    vk::MemoryBarrier::builder()
                        .src_access_mask(conv::map_buffer_access(access.start))
                        .dst_access_mask(conv::map_buffer_access(access.end))
                        .build(),
                );
            }
            memory::Barrier::AllImages(ref access) => {
                global.push(
                    vk::MemoryBarrier::builder()
                        .src_access_mask(conv::map_image_access(access.start))
                        .dst_access_mask(conv::map_image_access(access.end))
                        .build(),
                );
            }
            memory::Barrier::Buffer {
                ref states,
                target,
                ref range,
                ref families,
            } => {
                let families = match families {
                    Some(f) => f.start.0 as u32..f.end.0 as u32,
                    None => vk::QUEUE_FAMILY_IGNORED..vk::QUEUE_FAMILY_IGNORED,
                };
                buffer.push(
                    vk::BufferMemoryBarrier::builder()
                        .src_access_mask(conv::map_buffer_access(states.start))
                        .dst_access_mask(conv::map_buffer_access(states.end))
                        .src_queue_family_index(families.start)
                        .dst_queue_family_index(families.end)
                        .buffer(target.raw)
                        .offset(range.offset)
                        .size(range.size.unwrap_or(vk::WHOLE_SIZE))
                        .build(),
                );
            }
            memory::Barrier::Image {
                ref states,
                target,
                ref range,
                ref families,
            } => {
                let subresource_range = conv::map_subresource_range(range);
                let families = match families {
                    Some(f) => f.start.0 as u32..f.end.0 as u32,
                    None => vk::QUEUE_FAMILY_IGNORED..vk::QUEUE_FAMILY_IGNORED,
                };
                image.push(
                    vk::ImageMemoryBarrier::builder()
                        .src_access_mask(conv::map_image_access(states.start.0))
                        .dst_access_mask(conv::map_image_access(states.end.0))
                        .old_layout(conv::map_image_layout(states.start.1))
                        .new_layout(conv::map_image_layout(states.end.1))
                        .src_queue_family_index(families.start)
                        .dst_queue_family_index(families.end)
                        .image(target.raw)
                        .subresource_range(subresource_range)
                        .build(),
                );
            }
        }
    }

    BarrierSet {
        global,
        buffer,
        image,
    }
}

impl CommandBuffer {
    fn bind_descriptor_sets<I, J>(
        &mut self,
        bind_point: vk::PipelineBindPoint,
        layout: &n::PipelineLayout,
        first_set: usize,
        sets: I,
        offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<n::DescriptorSet>,
        I::IntoIter: ExactSizeIterator,
        J: IntoIterator,
        J::Item: Borrow<com::DescriptorSetOffset>,
        J::IntoIter: ExactSizeIterator,
    {
        let sets = sets.into_iter().map(|set| set.borrow().raw);
        let dynamic_offsets = offsets.into_iter().map(|offset| *offset.borrow());

        inplace_or_alloc_array(sets.len(), |uninit_guard| {
            let sets = uninit_guard.init_with_iter(sets);
            inplace_or_alloc_array(dynamic_offsets.len(), |uninit_guard| {
                let dynamic_offsets = uninit_guard.init_with_iter(dynamic_offsets);

                unsafe {
                    self.device.raw.cmd_bind_descriptor_sets(
                        self.raw,
                        bind_point,
                        layout.raw,
                        first_set as u32,
                        &sets,
                        &dynamic_offsets,
                    );
                }
            });
        });
    }
}

impl com::CommandBuffer<Backend> for CommandBuffer {
    unsafe fn begin(
        &mut self,
        flags: com::CommandBufferFlags,
        info: com::CommandBufferInheritanceInfo<Backend>,
    ) {
        let inheritance_info = vk::CommandBufferInheritanceInfo::builder()
            .render_pass(
                info.subpass
                    .map_or(vk::RenderPass::null(), |subpass| subpass.main_pass.raw),
            )
            .subpass(info.subpass.map_or(0, |subpass| subpass.index as u32))
            .framebuffer(
                info.framebuffer
                    .map_or(vk::Framebuffer::null(), |buffer| buffer.raw),
            )
            .occlusion_query_enable(info.occlusion_query_enable)
            .query_flags(conv::map_query_control_flags(info.occlusion_query_flags))
            .pipeline_statistics(conv::map_pipeline_statistics(info.pipeline_statistics));

        let info = vk::CommandBufferBeginInfo::builder()
            .flags(conv::map_command_buffer_flags(flags))
            .inheritance_info(&inheritance_info);

        assert_eq!(
            Ok(()),
            self.device.raw.begin_command_buffer(self.raw, &info)
        );
    }

    unsafe fn finish(&mut self) {
        assert_eq!(Ok(()), self.device.raw.end_command_buffer(self.raw));
    }

    unsafe fn reset(&mut self, release_resources: bool) {
        let flags = if release_resources {
            vk::CommandBufferResetFlags::RELEASE_RESOURCES
        } else {
            vk::CommandBufferResetFlags::empty()
        };

        assert_eq!(
            Ok(()),
            self.device.raw.reset_command_buffer(self.raw, flags)
        );
    }

    unsafe fn begin_render_pass<T>(
        &mut self,
        render_pass: &n::RenderPass,
        frame_buffer: &n::Framebuffer,
        render_area: pso::Rect,
        clear_values: T,
        first_subpass: com::SubpassContents,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ClearValue>,
        T::IntoIter: ExactSizeIterator,
    {
        let render_area = conv::map_rect(&render_area);

        // Vulkan wants one clear value per attachment (even those that don't need clears),
        // but can receive less clear values than total attachments.
        let clear_value_count = 64 - render_pass.clear_attachments_mask.leading_zeros() as u32;
        let mut clear_value_iter = clear_values.into_iter();
        let raw_clear_values = (0..clear_value_count).map(|i| {
            if render_pass.clear_attachments_mask & (1 << i) != 0 {
                // Vulkan and HAL share same memory layout
                let next = clear_value_iter.next().unwrap();
                mem::transmute(*next.borrow())
            } else {
                mem::zeroed()
            }
        });

        inplace_or_alloc_array(raw_clear_values.len(), |uninit_guard| {
            let raw_clear_values = uninit_guard.init_with_iter(raw_clear_values);

            let info = vk::RenderPassBeginInfo::builder()
                .render_pass(render_pass.raw)
                .framebuffer(frame_buffer.raw)
                .render_area(render_area)
                .clear_values(&raw_clear_values);

            let contents = map_subpass_contents(first_subpass);
            self.device
                .raw
                .cmd_begin_render_pass(self.raw, &info, contents);
        });
    }

    unsafe fn next_subpass(&mut self, contents: com::SubpassContents) {
        let contents = map_subpass_contents(contents);
        self.device.raw.cmd_next_subpass(self.raw, contents);
    }

    unsafe fn end_render_pass(&mut self) {
        self.device.raw.cmd_end_render_pass(self.raw);
    }

    unsafe fn pipeline_barrier<'a, T>(
        &mut self,
        stages: Range<pso::PipelineStage>,
        dependencies: memory::Dependencies,
        barriers: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<memory::Barrier<'a, Backend>>,
    {
        let BarrierSet {
            global,
            buffer,
            image,
        } = destructure_barriers(barriers);

        self.device.raw.cmd_pipeline_barrier(
            self.raw, // commandBuffer
            conv::map_pipeline_stage(stages.start),
            conv::map_pipeline_stage(stages.end),
            mem::transmute(dependencies),
            &global,
            &buffer,
            &image,
        );
    }

    unsafe fn fill_buffer(&mut self, buffer: &n::Buffer, range: buffer::SubRange, data: u32) {
        self.device.raw.cmd_fill_buffer(
            self.raw,
            buffer.raw,
            range.offset,
            range.size.unwrap_or(vk::WHOLE_SIZE),
            data,
        );
    }

    unsafe fn update_buffer(&mut self, buffer: &n::Buffer, offset: buffer::Offset, data: &[u8]) {
        self.device
            .raw
            .cmd_update_buffer(self.raw, buffer.raw, offset, data);
    }

    unsafe fn clear_image<T>(
        &mut self,
        image: &n::Image,
        layout: Layout,
        value: com::ClearValue,
        subresource_ranges: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<SubresourceRange>,
    {
        let mut color_ranges = Vec::new();
        let mut ds_ranges = Vec::new();

        for subresource_range in subresource_ranges {
            let sub = subresource_range.borrow();
            let aspect_ds = sub.aspects & (Aspects::DEPTH | Aspects::STENCIL);
            let vk_range = conv::map_subresource_range(sub);
            if sub.aspects.contains(Aspects::COLOR) {
                color_ranges.push(vk::ImageSubresourceRange {
                    aspect_mask: conv::map_image_aspects(Aspects::COLOR),
                    ..vk_range
                });
            }
            if !aspect_ds.is_empty() {
                ds_ranges.push(vk::ImageSubresourceRange {
                    aspect_mask: conv::map_image_aspects(aspect_ds),
                    ..vk_range
                });
            }
        }

        // Vulkan and HAL share same memory layout
        let color_value = mem::transmute(value.color);
        let depth_stencil_value = vk::ClearDepthStencilValue {
            depth: value.depth_stencil.depth,
            stencil: value.depth_stencil.stencil,
        };

        if !color_ranges.is_empty() {
            self.device.raw.cmd_clear_color_image(
                self.raw,
                image.raw,
                conv::map_image_layout(layout),
                &color_value,
                &color_ranges,
            )
        }
        if !ds_ranges.is_empty() {
            self.device.raw.cmd_clear_depth_stencil_image(
                self.raw,
                image.raw,
                conv::map_image_layout(layout),
                &depth_stencil_value,
                &ds_ranges,
            )
        }
    }

    unsafe fn clear_attachments<T, U>(&mut self, clears: T, rects: U)
    where
        T: IntoIterator,
        T::Item: Borrow<com::AttachmentClear>,
        T::IntoIter: ExactSizeIterator,
        U: IntoIterator,
        U::Item: Borrow<pso::ClearRect>,
        U::IntoIter: ExactSizeIterator,
    {
        let clears = clears.into_iter().map(|clear| match *clear.borrow() {
            com::AttachmentClear::Color { index, value } => vk::ClearAttachment {
                aspect_mask: vk::ImageAspectFlags::COLOR,
                color_attachment: index as _,
                clear_value: vk::ClearValue {
                    color: mem::transmute(value),
                },
            },
            com::AttachmentClear::DepthStencil { depth, stencil } => vk::ClearAttachment {
                aspect_mask: if depth.is_some() {
                    vk::ImageAspectFlags::DEPTH
                } else {
                    vk::ImageAspectFlags::empty()
                } | if stencil.is_some() {
                    vk::ImageAspectFlags::STENCIL
                } else {
                    vk::ImageAspectFlags::empty()
                },
                color_attachment: 0,
                clear_value: vk::ClearValue {
                    depth_stencil: vk::ClearDepthStencilValue {
                        depth: depth.unwrap_or_default(),
                        stencil: stencil.unwrap_or_default(),
                    },
                },
            },
        });

        let rects = rects
            .into_iter()
            .map(|rect| conv::map_clear_rect(rect.borrow()));

        inplace_or_alloc_array(clears.len(), |uninit_guard| {
            let clears = uninit_guard.init_with_iter(clears);

            inplace_or_alloc_array(rects.len(), |uninit_guard| {
                let rects = uninit_guard.init_with_iter(rects);

                self.device
                    .raw
                    .cmd_clear_attachments(self.raw, &clears, &rects);
            });
        });
    }

    unsafe fn resolve_image<T>(
        &mut self,
        src: &n::Image,
        src_layout: Layout,
        dst: &n::Image,
        dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ImageResolve>,
        T::IntoIter: ExactSizeIterator,
    {
        let regions = regions.into_iter().map(|region| {
            let r = region.borrow();
            vk::ImageResolve {
                src_subresource: conv::map_subresource_layers(&r.src_subresource),
                src_offset: conv::map_offset(r.src_offset),
                dst_subresource: conv::map_subresource_layers(&r.dst_subresource),
                dst_offset: conv::map_offset(r.dst_offset),
                extent: conv::map_extent(r.extent),
            }
        });

        inplace_or_alloc_array(regions.len(), |uninit_guard| {
            let regions = uninit_guard.init_with_iter(regions);

            self.device.raw.cmd_resolve_image(
                self.raw,
                src.raw,
                conv::map_image_layout(src_layout),
                dst.raw,
                conv::map_image_layout(dst_layout),
                &regions,
            );
        });
    }

    unsafe fn blit_image<T>(
        &mut self,
        src: &n::Image,
        src_layout: Layout,
        dst: &n::Image,
        dst_layout: Layout,
        filter: Filter,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ImageBlit>,
        T::IntoIter: ExactSizeIterator,
    {
        let regions = regions.into_iter().map(|region| {
            let r = region.borrow();
            vk::ImageBlit {
                src_subresource: conv::map_subresource_layers(&r.src_subresource),
                src_offsets: [
                    conv::map_offset(r.src_bounds.start),
                    conv::map_offset(r.src_bounds.end),
                ],
                dst_subresource: conv::map_subresource_layers(&r.dst_subresource),
                dst_offsets: [
                    conv::map_offset(r.dst_bounds.start),
                    conv::map_offset(r.dst_bounds.end),
                ],
            }
        });

        inplace_or_alloc_array(regions.len(), |uninit_guard| {
            let regions = uninit_guard.init_with_iter(regions);

            self.device.raw.cmd_blit_image(
                self.raw,
                src.raw,
                conv::map_image_layout(src_layout),
                dst.raw,
                conv::map_image_layout(dst_layout),
                &regions,
                conv::map_filter(filter),
            );
        });
    }

    unsafe fn bind_index_buffer(
        &mut self,
        buffer: &n::Buffer,
        sub: buffer::SubRange,
        ty: IndexType,
    ) {
        self.device.raw.cmd_bind_index_buffer(
            self.raw,
            buffer.raw,
            sub.offset,
            conv::map_index_type(ty),
        );
    }

    unsafe fn bind_vertex_buffers<I, T>(&mut self, first_binding: pso::BufferIndex, buffers: I)
    where
        I: IntoIterator<Item = (T, buffer::SubRange)>,
        T: Borrow<n::Buffer>,
    {
        let (buffers, offsets): (SmallVec<[vk::Buffer; 16]>, SmallVec<[vk::DeviceSize; 16]>) =
            buffers
                .into_iter()
                .map(|(buffer, sub)| (buffer.borrow().raw, sub.offset))
                .unzip();

        self.device
            .raw
            .cmd_bind_vertex_buffers(self.raw, first_binding, &buffers, &offsets);
    }

    unsafe fn set_viewports<T>(&mut self, first_viewport: u32, viewports: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Viewport>,
        T::IntoIter: ExactSizeIterator,
    {
        let viewports = viewports
            .into_iter()
            .map(|viewport| self.device.map_viewport(viewport.borrow()));

        inplace_or_alloc_array(viewports.len(), |uninit_guard| {
            let viewports = uninit_guard.init_with_iter(viewports);
            self.device
                .raw
                .cmd_set_viewport(self.raw, first_viewport, &viewports);
        });
    }

    unsafe fn set_scissors<T>(&mut self, first_scissor: u32, scissors: T)
    where
        T: IntoIterator,
        T::Item: Borrow<pso::Rect>,
        T::IntoIter: ExactSizeIterator,
    {
        let scissors = scissors
            .into_iter()
            .map(|scissor| conv::map_rect(scissor.borrow()));

        inplace_or_alloc_array(scissors.len(), |uninit_guard| {
            let scissors = uninit_guard.init_with_iter(scissors);

            self.device
                .raw
                .cmd_set_scissor(self.raw, first_scissor, &scissors);
        });
    }

    unsafe fn set_stencil_reference(&mut self, faces: pso::Face, value: pso::StencilValue) {
        // Vulkan and HAL share same faces bit flags
        self.device
            .raw
            .cmd_set_stencil_reference(self.raw, mem::transmute(faces), value);
    }

    unsafe fn set_stencil_read_mask(&mut self, faces: pso::Face, value: pso::StencilValue) {
        // Vulkan and HAL share same faces bit flags
        self.device
            .raw
            .cmd_set_stencil_compare_mask(self.raw, mem::transmute(faces), value);
    }

    unsafe fn set_stencil_write_mask(&mut self, faces: pso::Face, value: pso::StencilValue) {
        // Vulkan and HAL share same faces bit flags
        self.device
            .raw
            .cmd_set_stencil_write_mask(self.raw, mem::transmute(faces), value);
    }

    unsafe fn set_blend_constants(&mut self, color: pso::ColorValue) {
        self.device.raw.cmd_set_blend_constants(self.raw, &color);
    }

    unsafe fn set_depth_bounds(&mut self, bounds: Range<f32>) {
        self.device
            .raw
            .cmd_set_depth_bounds(self.raw, bounds.start, bounds.end);
    }

    unsafe fn set_line_width(&mut self, width: f32) {
        self.device.raw.cmd_set_line_width(self.raw, width);
    }

    unsafe fn set_depth_bias(&mut self, depth_bias: pso::DepthBias) {
        self.device.raw.cmd_set_depth_bias(
            self.raw,
            depth_bias.const_factor,
            depth_bias.clamp,
            depth_bias.slope_factor,
        );
    }

    unsafe fn bind_graphics_pipeline(&mut self, pipeline: &n::GraphicsPipeline) {
        self.device
            .raw
            .cmd_bind_pipeline(self.raw, vk::PipelineBindPoint::GRAPHICS, pipeline.0)
    }

    unsafe fn bind_graphics_descriptor_sets<I, J>(
        &mut self,
        layout: &n::PipelineLayout,
        first_set: usize,
        sets: I,
        offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<n::DescriptorSet>,
        I::IntoIter: ExactSizeIterator,
        J: IntoIterator,
        J::Item: Borrow<com::DescriptorSetOffset>,
        J::IntoIter: ExactSizeIterator,
    {
        self.bind_descriptor_sets(
            vk::PipelineBindPoint::GRAPHICS,
            layout,
            first_set,
            sets,
            offsets,
        );
    }

    unsafe fn bind_compute_pipeline(&mut self, pipeline: &n::ComputePipeline) {
        self.device
            .raw
            .cmd_bind_pipeline(self.raw, vk::PipelineBindPoint::COMPUTE, pipeline.0)
    }

    unsafe fn bind_compute_descriptor_sets<I, J>(
        &mut self,
        layout: &n::PipelineLayout,
        first_set: usize,
        sets: I,
        offsets: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<n::DescriptorSet>,
        I::IntoIter: ExactSizeIterator,
        J: IntoIterator,
        J::Item: Borrow<com::DescriptorSetOffset>,
        J::IntoIter: ExactSizeIterator,
    {
        self.bind_descriptor_sets(
            vk::PipelineBindPoint::COMPUTE,
            layout,
            first_set,
            sets,
            offsets,
        );
    }

    unsafe fn dispatch(&mut self, count: WorkGroupCount) {
        self.device
            .raw
            .cmd_dispatch(self.raw, count[0], count[1], count[2])
    }

    unsafe fn dispatch_indirect(&mut self, buffer: &n::Buffer, offset: buffer::Offset) {
        self.device
            .raw
            .cmd_dispatch_indirect(self.raw, buffer.raw, offset)
    }

    unsafe fn copy_buffer<T>(&mut self, src: &n::Buffer, dst: &n::Buffer, regions: T)
    where
        T: IntoIterator,
        T::Item: Borrow<com::BufferCopy>,
        T::IntoIter: ExactSizeIterator,
    {
        let regions = regions.into_iter().map(|region| {
            let region = region.borrow();
            vk::BufferCopy {
                src_offset: region.src,
                dst_offset: region.dst,
                size: region.size,
            }
        });

        inplace_or_alloc_array(regions.len(), |uninit_guard| {
            let regions = uninit_guard.init_with_iter(regions);
            self.device
                .raw
                .cmd_copy_buffer(self.raw, src.raw, dst.raw, &regions)
        })
    }

    unsafe fn copy_image<T>(
        &mut self,
        src: &n::Image,
        src_layout: Layout,
        dst: &n::Image,
        dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::ImageCopy>,
        T::IntoIter: ExactSizeIterator,
    {
        let regions = regions.into_iter().map(|region| {
            let r = region.borrow();
            vk::ImageCopy {
                src_subresource: conv::map_subresource_layers(&r.src_subresource),
                src_offset: conv::map_offset(r.src_offset),
                dst_subresource: conv::map_subresource_layers(&r.dst_subresource),
                dst_offset: conv::map_offset(r.dst_offset),
                extent: conv::map_extent(r.extent),
            }
        });

        inplace_or_alloc_array(regions.len(), |uninit_guard| {
            let regions = uninit_guard.init_with_iter(regions);
            self.device.raw.cmd_copy_image(
                self.raw,
                src.raw,
                conv::map_image_layout(src_layout),
                dst.raw,
                conv::map_image_layout(dst_layout),
                &regions,
            );
        });
    }

    unsafe fn copy_buffer_to_image<T>(
        &mut self,
        src: &n::Buffer,
        dst: &n::Image,
        dst_layout: Layout,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::BufferImageCopy>,
        T::IntoIter: ExactSizeIterator,
    {
        let regions_iter = map_buffer_image_regions(dst, regions);

        inplace_or_alloc_array(regions_iter.len(), |uninit_guard| {
            let regions = uninit_guard.init_with_iter(regions_iter);

            self.device.raw.cmd_copy_buffer_to_image(
                self.raw,
                src.raw,
                dst.raw,
                conv::map_image_layout(dst_layout),
                &regions,
            );
        });
    }

    unsafe fn copy_image_to_buffer<T>(
        &mut self,
        src: &n::Image,
        src_layout: Layout,
        dst: &n::Buffer,
        regions: T,
    ) where
        T: IntoIterator,
        T::Item: Borrow<com::BufferImageCopy>,
        T::IntoIter: ExactSizeIterator,
    {
        let regions_iter = map_buffer_image_regions(src, regions);

        inplace_or_alloc_array(regions_iter.len(), |uninit_guard| {
            let regions = uninit_guard.init_with_iter(regions_iter);

            self.device.raw.cmd_copy_image_to_buffer(
                self.raw,
                src.raw,
                conv::map_image_layout(src_layout),
                dst.raw,
                &regions,
            );
        });
    }

    unsafe fn draw(&mut self, vertices: Range<VertexCount>, instances: Range<InstanceCount>) {
        self.device.raw.cmd_draw(
            self.raw,
            vertices.end - vertices.start,
            instances.end - instances.start,
            vertices.start,
            instances.start,
        )
    }

    unsafe fn draw_indexed(
        &mut self,
        indices: Range<IndexCount>,
        base_vertex: VertexOffset,
        instances: Range<InstanceCount>,
    ) {
        self.device.raw.cmd_draw_indexed(
            self.raw,
            indices.end - indices.start,
            instances.end - instances.start,
            indices.start,
            base_vertex,
            instances.start,
        )
    }

    unsafe fn draw_indirect(
        &mut self,
        buffer: &n::Buffer,
        offset: buffer::Offset,
        draw_count: DrawCount,
        stride: u32,
    ) {
        self.device
            .raw
            .cmd_draw_indirect(self.raw, buffer.raw, offset, draw_count, stride)
    }

    unsafe fn draw_indexed_indirect(
        &mut self,
        buffer: &n::Buffer,
        offset: buffer::Offset,
        draw_count: DrawCount,
        stride: u32,
    ) {
        self.device
            .raw
            .cmd_draw_indexed_indirect(self.raw, buffer.raw, offset, draw_count, stride)
    }

    unsafe fn draw_mesh_tasks(&mut self, task_count: TaskCount, first_task: TaskCount) {
        self.device
            .extension_fns
            .mesh_shaders
            .as_ref()
            .expect("Draw command not supported. You must request feature MESH_SHADER.")
            .cmd_draw_mesh_tasks(self.raw, task_count, first_task);
    }

    unsafe fn draw_mesh_tasks_indirect(
        &mut self,
        buffer: &n::Buffer,
        offset: buffer::Offset,
        draw_count: hal::DrawCount,
        stride: u32,
    ) {
        self.device
            .extension_fns
            .mesh_shaders
            .as_ref()
            .expect("Draw command not supported. You must request feature MESH_SHADER.")
            .cmd_draw_mesh_tasks_indirect(self.raw, buffer.raw, offset, draw_count, stride);
    }

    unsafe fn draw_mesh_tasks_indirect_count(
        &mut self,
        buffer: &n::Buffer,
        offset: buffer::Offset,
        count_buffer: &n::Buffer,
        count_buffer_offset: buffer::Offset,
        max_draw_count: DrawCount,
        stride: u32,
    ) {
        self.device
            .extension_fns
            .mesh_shaders
            .as_ref()
            .expect("Draw command not supported. You must request feature MESH_SHADER.")
            .cmd_draw_mesh_tasks_indirect_count(
                self.raw,
                buffer.raw,
                offset,
                count_buffer.raw,
                count_buffer_offset,
                max_draw_count,
                stride,
            );
    }

    unsafe fn draw_indirect_count(
        &mut self,
        buffer: &n::Buffer,
        offset: buffer::Offset,
        count_buffer: &n::Buffer,
        count_buffer_offset: buffer::Offset,
        max_draw_count: DrawCount,
        stride: u32,
    ) {
        self.device
            .extension_fns
            .draw_indirect_count
            .as_ref()
            .expect("Feature DRAW_INDIRECT_COUNT must be enabled to call draw_indirect_count")
            .cmd_draw_indirect_count(
                self.raw,
                buffer.raw,
                offset,
                count_buffer.raw,
                count_buffer_offset,
                max_draw_count,
                stride,
            );
    }

    unsafe fn draw_indexed_indirect_count(
        &mut self,
        buffer: &n::Buffer,
        offset: buffer::Offset,
        count_buffer: &n::Buffer,
        count_buffer_offset: buffer::Offset,
        max_draw_count: DrawCount,
        stride: u32,
    ) {
        self.device
            .extension_fns
            .draw_indirect_count
            .as_ref()
            .expect(
                "Feature DRAW_INDIRECT_COUNT must be enabled to call draw_indexed_indirect_count",
            )
            .cmd_draw_indexed_indirect_count(
                self.raw,
                buffer.raw,
                offset,
                count_buffer.raw,
                count_buffer_offset,
                max_draw_count,
                stride,
            );
    }

    unsafe fn set_event(&mut self, event: &n::Event, stage_mask: pso::PipelineStage) {
        self.device.raw.cmd_set_event(
            self.raw,
            event.0,
            vk::PipelineStageFlags::from_raw(stage_mask.bits()),
        )
    }

    unsafe fn reset_event(&mut self, event: &n::Event, stage_mask: pso::PipelineStage) {
        self.device.raw.cmd_reset_event(
            self.raw,
            event.0,
            vk::PipelineStageFlags::from_raw(stage_mask.bits()),
        )
    }

    unsafe fn wait_events<'a, I, J>(
        &mut self,
        events: I,
        stages: Range<pso::PipelineStage>,
        barriers: J,
    ) where
        I: IntoIterator,
        I::Item: Borrow<n::Event>,
        I::IntoIter: ExactSizeIterator,
        J: IntoIterator,
        J::Item: Borrow<memory::Barrier<'a, Backend>>,
    {
        let events = events.into_iter().map(|e| e.borrow().0);

        let BarrierSet {
            global,
            buffer,
            image,
        } = destructure_barriers(barriers);

        inplace_or_alloc_array(events.len(), |uninit_guard| {
            let events = uninit_guard.init_with_iter(events);

            self.device.raw.cmd_wait_events(
                self.raw,
                &events,
                vk::PipelineStageFlags::from_raw(stages.start.bits()),
                vk::PipelineStageFlags::from_raw(stages.end.bits()),
                &global,
                &buffer,
                &image,
            )
        })
    }

    unsafe fn begin_query(&mut self, query: query::Query<Backend>, flags: query::ControlFlags) {
        self.device.raw.cmd_begin_query(
            self.raw,
            query.pool.0,
            query.id,
            conv::map_query_control_flags(flags),
        )
    }

    unsafe fn end_query(&mut self, query: query::Query<Backend>) {
        self.device
            .raw
            .cmd_end_query(self.raw, query.pool.0, query.id)
    }

    unsafe fn reset_query_pool(&mut self, pool: &n::QueryPool, queries: Range<query::Id>) {
        self.device.raw.cmd_reset_query_pool(
            self.raw,
            pool.0,
            queries.start,
            queries.end - queries.start,
        )
    }

    unsafe fn copy_query_pool_results(
        &mut self,
        pool: &n::QueryPool,
        queries: Range<query::Id>,
        buffer: &n::Buffer,
        offset: buffer::Offset,
        stride: buffer::Offset,
        flags: query::ResultFlags,
    ) {
        //TODO: use safer wrapper
        self.device.raw.fp_v1_0().cmd_copy_query_pool_results(
            self.raw,
            pool.0,
            queries.start,
            queries.end - queries.start,
            buffer.raw,
            offset,
            stride,
            conv::map_query_result_flags(flags),
        );
    }

    unsafe fn write_timestamp(&mut self, stage: pso::PipelineStage, query: query::Query<Backend>) {
        self.device.raw.cmd_write_timestamp(
            self.raw,
            conv::map_pipeline_stage(stage),
            query.pool.0,
            query.id,
        )
    }

    unsafe fn push_compute_constants(
        &mut self,
        layout: &n::PipelineLayout,
        offset: u32,
        constants: &[u32],
    ) {
        self.device.raw.cmd_push_constants(
            self.raw,
            layout.raw,
            vk::ShaderStageFlags::COMPUTE,
            offset,
            slice::from_raw_parts(constants.as_ptr() as _, constants.len() * 4),
        );
    }

    unsafe fn push_graphics_constants(
        &mut self,
        layout: &n::PipelineLayout,
        stages: pso::ShaderStageFlags,
        offset: u32,
        constants: &[u32],
    ) {
        self.device.raw.cmd_push_constants(
            self.raw,
            layout.raw,
            conv::map_stage_flags(stages),
            offset,
            slice::from_raw_parts(constants.as_ptr() as _, constants.len() * 4),
        );
    }

    unsafe fn execute_commands<'a, T, I>(&mut self, buffers: I)
    where
        T: 'a + Borrow<CommandBuffer>,
        I: IntoIterator,
        I: IntoIterator<Item = &'a T>,
        I::IntoIter: ExactSizeIterator,
    {
        let command_buffers = buffers.into_iter().map(|b| b.borrow().raw);

        inplace_or_alloc_array(command_buffers.len(), |uninit_guard| {
            let command_buffers = uninit_guard.init_with_iter(command_buffers);
            self.device
                .raw
                .cmd_execute_commands(self.raw, &command_buffers);
        });
    }

    unsafe fn insert_debug_marker(&mut self, name: &str, color: u32) {
        if let Some(&DebugMessenger::Utils(ref ext, _)) = self.device.debug_messenger() {
            let cstr = CString::new(name).unwrap();
            let label = vk::DebugUtilsLabelEXT::builder()
                .label_name(&cstr)
                .color(debug_color(color))
                .build();
            ext.cmd_insert_debug_utils_label(self.raw, &label);
        }
    }
    unsafe fn begin_debug_marker(&mut self, name: &str, color: u32) {
        if let Some(&DebugMessenger::Utils(ref ext, _)) = self.device.debug_messenger() {
            let cstr = CString::new(name).unwrap();
            let label = vk::DebugUtilsLabelEXT::builder()
                .label_name(&cstr)
                .color(debug_color(color))
                .build();
            ext.cmd_begin_debug_utils_label(self.raw, &label);
        }
    }
    unsafe fn end_debug_marker(&mut self) {
        if let Some(&DebugMessenger::Utils(ref ext, _)) = self.device.debug_messenger() {
            ext.cmd_end_debug_utils_label(self.raw);
        }
    }
}
