use arrayvec::ArrayVec;
use ash::extensions::khr;
use ash::version::DeviceV1_0;
use ash::vk;
use ash::vk::Handle;
use smallvec::SmallVec;

use hal::{
    memory::{Requirements, Segment},
    pool::CommandPoolCreateFlags,
    pso::VertexInputRate,
    window::SwapchainConfig,
    {buffer, device as d, format, image, pass, pso, query, queue}, {Features, MemoryTypeId},
};

use std::borrow::Borrow;
use std::ffi::{CStr, CString};
use std::ops::Range;
use std::pin::Pin;
use std::sync::Arc;
use std::{mem, ptr};

use crate::pool::RawCommandPool;
use crate::{command as cmd, conv, native as n, window as w};
use crate::{Backend as B, DebugMessenger, Device};

#[derive(Debug, Default)]
struct GraphicsPipelineInfoBuf {
    // 10 is the max amount of dynamic states
    dynamic_states: ArrayVec<[vk::DynamicState; 10]>,

    // 5 is the amount of stages
    c_strings: ArrayVec<[CString; 5]>,
    stages: ArrayVec<[vk::PipelineShaderStageCreateInfo; 5]>,
    specializations: ArrayVec<[vk::SpecializationInfo; 5]>,
    specialization_entries: ArrayVec<[SmallVec<[vk::SpecializationMapEntry; 4]>; 5]>,

    vertex_bindings: Vec<vk::VertexInputBindingDescription>,
    vertex_attributes: Vec<vk::VertexInputAttributeDescription>,
    blend_states: Vec<vk::PipelineColorBlendAttachmentState>,

    sample_mask: [u32; 2],
    vertex_input_state: vk::PipelineVertexInputStateCreateInfo,
    input_assembly_state: vk::PipelineInputAssemblyStateCreateInfo,
    tessellation_state: Option<vk::PipelineTessellationStateCreateInfo>,
    viewport_state: vk::PipelineViewportStateCreateInfo,
    rasterization_state: vk::PipelineRasterizationStateCreateInfo,
    multisample_state: vk::PipelineMultisampleStateCreateInfo,
    depth_stencil_state: vk::PipelineDepthStencilStateCreateInfo,
    color_blend_state: vk::PipelineColorBlendStateCreateInfo,
    pipeline_dynamic_state: vk::PipelineDynamicStateCreateInfo,
    viewport: vk::Viewport,
    scissor: vk::Rect2D,
}
impl GraphicsPipelineInfoBuf {
    unsafe fn add_stage<'a>(
        &mut self,
        stage: vk::ShaderStageFlags,
        source: &pso::EntryPoint<'a, B>,
    ) {
        let string = CString::new(source.entry).unwrap();
        self.c_strings.push(string);
        let name = self.c_strings.last().unwrap().as_c_str();

        self.specialization_entries.push(
            source
                .specialization
                .constants
                .iter()
                .map(|c| vk::SpecializationMapEntry {
                    constant_id: c.id,
                    offset: c.range.start as _,
                    size: (c.range.end - c.range.start) as _,
                })
                .collect(),
        );
        let map_entries = self.specialization_entries.last().unwrap();

        self.specializations.push(vk::SpecializationInfo {
            map_entry_count: map_entries.len() as _,
            p_map_entries: map_entries.as_ptr(),
            data_size: source.specialization.data.len() as _,
            p_data: source.specialization.data.as_ptr() as _,
        });

        self.stages.push(
            vk::PipelineShaderStageCreateInfo::builder()
                .flags(vk::PipelineShaderStageCreateFlags::empty())
                .stage(stage)
                .module(source.module.raw)
                .name(name)
                .specialization_info(self.specializations.last().unwrap())
                .build(),
        )
    }

    unsafe fn initialize<'a>(
        this: &mut Pin<&mut Self>,
        device: &Device,
        desc: &pso::GraphicsPipelineDesc<'a, B>,
    ) {
        let mut this = Pin::get_mut(this.as_mut()); // use into_inner when it gets stable

        match desc.primitive_assembler {
            pso::PrimitiveAssemblerDesc::Vertex {
                ref buffers,
                ref attributes,
                ref input_assembler,
                ref vertex,
                ref tessellation,
                ref geometry,
            } => {
                // Vertex stage
                // vertex shader is required
                this.add_stage(vk::ShaderStageFlags::VERTEX, vertex);

                // Geometry stage
                if let Some(ref entry) = geometry {
                    this.add_stage(vk::ShaderStageFlags::GEOMETRY, entry);
                }
                // Tessellation stage
                if let Some(ts) = tessellation {
                    this.add_stage(vk::ShaderStageFlags::TESSELLATION_CONTROL, &ts.0);
                    this.add_stage(vk::ShaderStageFlags::TESSELLATION_EVALUATION, &ts.1);
                }
                this.vertex_bindings = buffers.iter().map(|vbuf| {
                    vk::VertexInputBindingDescription {
                        binding: vbuf.binding,
                        stride: vbuf.stride as u32,
                        input_rate: match vbuf.rate {
                            VertexInputRate::Vertex => vk::VertexInputRate::VERTEX,
                            VertexInputRate::Instance(divisor) => {
                                debug_assert_eq!(divisor, 1, "Custom vertex rate divisors not supported in Vulkan backend without extension");
                                vk::VertexInputRate::INSTANCE
                            },
                        },
                    }
                }).collect();

                this.vertex_attributes = attributes
                    .iter()
                    .map(|attr| vk::VertexInputAttributeDescription {
                        location: attr.location as u32,
                        binding: attr.binding as u32,
                        format: conv::map_format(attr.element.format),
                        offset: attr.element.offset as u32,
                    })
                    .collect();

                this.vertex_input_state = vk::PipelineVertexInputStateCreateInfo::builder()
                    .flags(vk::PipelineVertexInputStateCreateFlags::empty())
                    .vertex_binding_descriptions(&this.vertex_bindings)
                    .vertex_attribute_descriptions(&this.vertex_attributes)
                    .build();

                this.input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo::builder()
                    .flags(vk::PipelineInputAssemblyStateCreateFlags::empty())
                    .topology(conv::map_topology(&input_assembler))
                    .primitive_restart_enable(input_assembler.restart_index.is_some())
                    .build();
            }
            pso::PrimitiveAssemblerDesc::Mesh { ref task, ref mesh } => {
                this.vertex_bindings = Vec::new();
                this.vertex_attributes = Vec::new();
                this.vertex_input_state = vk::PipelineVertexInputStateCreateInfo::default();
                this.input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo::default();

                // Task stage, optional
                if let Some(ref entry) = task {
                    this.add_stage(vk::ShaderStageFlags::TASK_NV, entry);
                }

                // Mesh stage
                this.add_stage(vk::ShaderStageFlags::MESH_NV, mesh);
            }
        };

        // Pixel stage
        if let Some(ref entry) = desc.fragment {
            this.add_stage(vk::ShaderStageFlags::FRAGMENT, entry);
        }

        let depth_bias = match desc.rasterizer.depth_bias {
            Some(pso::State::Static(db)) => db,
            Some(pso::State::Dynamic) => {
                this.dynamic_states.push(vk::DynamicState::DEPTH_BIAS);
                pso::DepthBias::default()
            }
            None => pso::DepthBias::default(),
        };

        let polygon_mode = match desc.rasterizer.polygon_mode {
            pso::PolygonMode::Point => vk::PolygonMode::POINT,
            pso::PolygonMode::Line => vk::PolygonMode::LINE,
            pso::PolygonMode::Fill => vk::PolygonMode::FILL,
        };

        let line_width = match desc.rasterizer.line_width {
            pso::State::Static(w) => w,
            pso::State::Dynamic => {
                this.dynamic_states.push(vk::DynamicState::LINE_WIDTH);
                1.0
            }
        };

        this.rasterization_state = vk::PipelineRasterizationStateCreateInfo::builder()
            .flags(vk::PipelineRasterizationStateCreateFlags::empty())
            .depth_clamp_enable(if desc.rasterizer.depth_clamping {
                if device.shared.features.contains(Features::DEPTH_CLAMP) {
                    true
                } else {
                    warn!("Depth clamping was requested on a device with disabled feature");
                    false
                }
            } else {
                false
            })
            .rasterizer_discard_enable(
                desc.fragment.is_none()
                    && desc.depth_stencil.depth.is_none()
                    && desc.depth_stencil.stencil.is_none(),
            )
            .polygon_mode(polygon_mode)
            .cull_mode(conv::map_cull_face(desc.rasterizer.cull_face))
            .front_face(conv::map_front_face(desc.rasterizer.front_face))
            .depth_bias_enable(desc.rasterizer.depth_bias.is_some())
            .depth_bias_constant_factor(depth_bias.const_factor)
            .depth_bias_clamp(depth_bias.clamp)
            .depth_bias_slope_factor(depth_bias.slope_factor)
            .line_width(line_width)
            .build();

        this.tessellation_state = {
            if let pso::PrimitiveAssemblerDesc::Vertex {
                input_assembler, ..
            } = &desc.primitive_assembler
            {
                if let pso::Primitive::PatchList(patch_control_points) = input_assembler.primitive {
                    Some(
                        vk::PipelineTessellationStateCreateInfo::builder()
                            .flags(vk::PipelineTessellationStateCreateFlags::empty())
                            .patch_control_points(patch_control_points as _)
                            .build(),
                    )
                } else {
                    None
                }
            } else {
                None
            }
        };

        this.viewport_state = {
            let scissors = match desc.baked_states.scissor {
                Some(ref rect) => {
                    this.scissor = conv::map_rect(rect);
                    Some([this.scissor])
                }
                None => {
                    this.dynamic_states.push(vk::DynamicState::SCISSOR);
                    None
                }
            };
            let viewports = match desc.baked_states.viewport {
                Some(ref vp) => {
                    this.viewport = device.shared.map_viewport(vp);
                    Some([this.viewport])
                }
                None => {
                    this.dynamic_states.push(vk::DynamicState::VIEWPORT);
                    None
                }
            };

            let mut builder = vk::PipelineViewportStateCreateInfo::builder()
                .flags(vk::PipelineViewportStateCreateFlags::empty());
            if let Some(scissors) = &scissors {
                builder = builder.scissors(scissors);
            }
            if let Some(viewports) = &viewports {
                builder = builder.viewports(viewports);
            }
            builder.build()
        };

        this.multisample_state = match desc.multisampling {
            Some(ref ms) => {
                this.sample_mask = [
                    (ms.sample_mask & 0xFFFFFFFF) as u32,
                    ((ms.sample_mask >> 32) & 0xFFFFFFFF) as u32,
                ];
                vk::PipelineMultisampleStateCreateInfo::builder()
                    .flags(vk::PipelineMultisampleStateCreateFlags::empty())
                    .rasterization_samples(vk::SampleCountFlags::from_raw(
                        (ms.rasterization_samples as u32) & vk::SampleCountFlags::all().as_raw(),
                    ))
                    .sample_shading_enable(ms.sample_shading.is_some())
                    .min_sample_shading(ms.sample_shading.unwrap_or(0.0))
                    .sample_mask(&this.sample_mask)
                    .alpha_to_coverage_enable(ms.alpha_coverage)
                    .alpha_to_one_enable(ms.alpha_to_one)
                    .build()
            }
            None => vk::PipelineMultisampleStateCreateInfo::builder()
                .flags(vk::PipelineMultisampleStateCreateFlags::empty())
                .rasterization_samples(vk::SampleCountFlags::TYPE_1)
                .build(),
        };

        let depth_stencil = desc.depth_stencil;
        let (depth_test_enable, depth_write_enable, depth_compare_op) = match depth_stencil.depth {
            Some(ref depth) => (true, depth.write as _, conv::map_comparison(depth.fun)),
            None => (false, false, vk::CompareOp::NEVER),
        };
        let (stencil_test_enable, front, back) = match depth_stencil.stencil {
            Some(ref stencil) => {
                let mut front = conv::map_stencil_side(&stencil.faces.front);
                let mut back = conv::map_stencil_side(&stencil.faces.back);
                match stencil.read_masks {
                    pso::State::Static(ref sides) => {
                        front.compare_mask = sides.front;
                        back.compare_mask = sides.back;
                    }
                    pso::State::Dynamic => {
                        this.dynamic_states
                            .push(vk::DynamicState::STENCIL_COMPARE_MASK);
                    }
                }
                match stencil.write_masks {
                    pso::State::Static(ref sides) => {
                        front.write_mask = sides.front;
                        back.write_mask = sides.back;
                    }
                    pso::State::Dynamic => {
                        this.dynamic_states
                            .push(vk::DynamicState::STENCIL_WRITE_MASK);
                    }
                }
                match stencil.reference_values {
                    pso::State::Static(ref sides) => {
                        front.reference = sides.front;
                        back.reference = sides.back;
                    }
                    pso::State::Dynamic => {
                        this.dynamic_states
                            .push(vk::DynamicState::STENCIL_REFERENCE);
                    }
                }
                (true, front, back)
            }
            None => mem::zeroed(),
        };
        let (min_depth_bounds, max_depth_bounds) = match desc.baked_states.depth_bounds {
            Some(ref range) => (range.start, range.end),
            None => {
                this.dynamic_states.push(vk::DynamicState::DEPTH_BOUNDS);
                (0.0, 1.0)
            }
        };

        this.depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo::builder()
            .flags(vk::PipelineDepthStencilStateCreateFlags::empty())
            .depth_test_enable(depth_test_enable)
            .depth_write_enable(depth_write_enable)
            .depth_compare_op(depth_compare_op)
            .depth_bounds_test_enable(depth_stencil.depth_bounds)
            .stencil_test_enable(stencil_test_enable)
            .front(front)
            .back(back)
            .min_depth_bounds(min_depth_bounds)
            .max_depth_bounds(max_depth_bounds)
            .build();

        this.blend_states = desc
            .blender
            .targets
            .iter()
            .map(|color_desc| {
                let color_write_mask =
                    vk::ColorComponentFlags::from_raw(color_desc.mask.bits() as _);
                match color_desc.blend {
                    Some(ref bs) => {
                        let (color_blend_op, src_color_blend_factor, dst_color_blend_factor) =
                            conv::map_blend_op(bs.color);
                        let (alpha_blend_op, src_alpha_blend_factor, dst_alpha_blend_factor) =
                            conv::map_blend_op(bs.alpha);
                        vk::PipelineColorBlendAttachmentState {
                            color_write_mask,
                            blend_enable: vk::TRUE,
                            src_color_blend_factor,
                            dst_color_blend_factor,
                            color_blend_op,
                            src_alpha_blend_factor,
                            dst_alpha_blend_factor,
                            alpha_blend_op,
                        }
                    }
                    None => vk::PipelineColorBlendAttachmentState {
                        color_write_mask,
                        ..mem::zeroed()
                    },
                }
            })
            .collect();

        this.color_blend_state = vk::PipelineColorBlendStateCreateInfo::builder()
            .flags(vk::PipelineColorBlendStateCreateFlags::empty())
            .logic_op_enable(false) // TODO
            .logic_op(vk::LogicOp::CLEAR)
            .attachments(&this.blend_states) // TODO:
            .blend_constants(match desc.baked_states.blend_color {
                Some(value) => value,
                None => {
                    this.dynamic_states.push(vk::DynamicState::BLEND_CONSTANTS);
                    [0.0; 4]
                }
            })
            .build();

        this.pipeline_dynamic_state = vk::PipelineDynamicStateCreateInfo::builder()
            .flags(vk::PipelineDynamicStateCreateFlags::empty())
            .dynamic_states(&this.dynamic_states)
            .build();
    }
}

#[derive(Debug, Default)]
struct ComputePipelineInfoBuf {
    c_string: CString,
    specialization: vk::SpecializationInfo,
    entries: SmallVec<[vk::SpecializationMapEntry; 4]>,
}
impl ComputePipelineInfoBuf {
    unsafe fn initialize<'a>(this: &mut Pin<&mut Self>, desc: &pso::ComputePipelineDesc<'a, B>) {
        let mut this = Pin::get_mut(this.as_mut()); // use into_inner when it gets stable

        this.c_string = CString::new(desc.shader.entry).unwrap();
        this.entries = desc
            .shader
            .specialization
            .constants
            .iter()
            .map(|c| vk::SpecializationMapEntry {
                constant_id: c.id,
                offset: c.range.start as _,
                size: (c.range.end - c.range.start) as _,
            })
            .collect();
        this.specialization = vk::SpecializationInfo {
            map_entry_count: this.entries.len() as _,
            p_map_entries: this.entries.as_ptr(),
            data_size: desc.shader.specialization.data.len() as _,
            p_data: desc.shader.specialization.data.as_ptr() as _,
        };
    }
}

impl d::Device<B> for Device {
    unsafe fn allocate_memory(
        &self,
        mem_type: MemoryTypeId,
        size: u64,
    ) -> Result<n::Memory, d::AllocationError> {
        let info = vk::MemoryAllocateInfo::builder()
            .allocation_size(size)
            .memory_type_index(self.get_ash_memory_type_index(mem_type));

        let result = self.shared.raw.allocate_memory(&info, None);

        match result {
            Ok(memory) => Ok(n::Memory { raw: memory }),
            Err(vk::Result::ERROR_TOO_MANY_OBJECTS) => Err(d::AllocationError::TooManyObjects),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn create_command_pool(
        &self,
        family: queue::QueueFamilyId,
        create_flags: CommandPoolCreateFlags,
    ) -> Result<RawCommandPool, d::OutOfMemory> {
        let mut flags = vk::CommandPoolCreateFlags::empty();
        if create_flags.contains(CommandPoolCreateFlags::TRANSIENT) {
            flags |= vk::CommandPoolCreateFlags::TRANSIENT;
        }
        if create_flags.contains(CommandPoolCreateFlags::RESET_INDIVIDUAL) {
            flags |= vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER;
        }

        let info = vk::CommandPoolCreateInfo::builder()
            .flags(flags)
            .queue_family_index(family.0 as _);

        let result = self.shared.raw.create_command_pool(&info, None);

        match result {
            Ok(pool) => Ok(RawCommandPool {
                raw: pool,
                device: self.shared.clone(),
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn destroy_command_pool(&self, pool: RawCommandPool) {
        self.shared.raw.destroy_command_pool(pool.raw, None);
    }

    unsafe fn create_render_pass<'a, IA, IS, ID>(
        &self,
        attachments: IA,
        subpasses: IS,
        dependencies: ID,
    ) -> Result<n::RenderPass, d::OutOfMemory>
    where
        IA: IntoIterator,
        IA::Item: Borrow<pass::Attachment>,
        IA::IntoIter: ExactSizeIterator,
        IS: IntoIterator,
        IS::Item: Borrow<pass::SubpassDesc<'a>>,
        IS::IntoIter: ExactSizeIterator,
        ID: IntoIterator,
        ID::Item: Borrow<pass::SubpassDependency>,
        ID::IntoIter: ExactSizeIterator,
    {
        let attachments = attachments.into_iter().map(|attachment| {
            let attachment = attachment.borrow();
            vk::AttachmentDescription {
                flags: vk::AttachmentDescriptionFlags::empty(), // TODO: may even alias!
                format: attachment
                    .format
                    .map_or(vk::Format::UNDEFINED, conv::map_format),
                samples: vk::SampleCountFlags::from_raw(
                    (attachment.samples as u32) & vk::SampleCountFlags::all().as_raw(),
                ),
                load_op: conv::map_attachment_load_op(attachment.ops.load),
                store_op: conv::map_attachment_store_op(attachment.ops.store),
                stencil_load_op: conv::map_attachment_load_op(attachment.stencil_ops.load),
                stencil_store_op: conv::map_attachment_store_op(attachment.stencil_ops.store),
                initial_layout: conv::map_image_layout(attachment.layouts.start),
                final_layout: conv::map_image_layout(attachment.layouts.end),
            }
        });

        let dependencies = dependencies.into_iter().map(|subpass_dep| {
            let sdep = subpass_dep.borrow();
            // TODO: checks
            vk::SubpassDependency {
                src_subpass: sdep
                    .passes
                    .start
                    .map_or(vk::SUBPASS_EXTERNAL, |id| id as u32),
                dst_subpass: sdep.passes.end.map_or(vk::SUBPASS_EXTERNAL, |id| id as u32),
                src_stage_mask: conv::map_pipeline_stage(sdep.stages.start),
                dst_stage_mask: conv::map_pipeline_stage(sdep.stages.end),
                src_access_mask: conv::map_image_access(sdep.accesses.start),
                dst_access_mask: conv::map_image_access(sdep.accesses.end),
                dependency_flags: mem::transmute(sdep.flags),
            }
        });

        let (clear_attachments_mask, result) =
            inplace_it::inplace_or_alloc_array(attachments.len(), |uninit_guard| {
                let attachments = uninit_guard.init_with_iter(attachments);

                let clear_attachments_mask = attachments
                    .iter()
                    .enumerate()
                    .filter_map(|(i, at)| {
                        if at.load_op == vk::AttachmentLoadOp::CLEAR
                            || at.stencil_load_op == vk::AttachmentLoadOp::CLEAR
                        {
                            Some(1 << i as u64)
                        } else {
                            None
                        }
                    })
                    .sum();

                let attachment_refs = subpasses
                    .into_iter()
                    .map(|subpass| {
                        let subpass = subpass.borrow();
                        fn make_ref(
                            &(id, layout): &pass::AttachmentRef,
                        ) -> vk::AttachmentReference {
                            vk::AttachmentReference {
                                attachment: id as _,
                                layout: conv::map_image_layout(layout),
                            }
                        }
                        let colors = subpass.colors.iter().map(make_ref).collect::<Box<[_]>>();
                        let depth_stencil = subpass.depth_stencil.map(make_ref);
                        let inputs = subpass.inputs.iter().map(make_ref).collect::<Box<[_]>>();
                        let preserves = subpass
                            .preserves
                            .iter()
                            .map(|&id| id as u32)
                            .collect::<Box<[_]>>();
                        let resolves = subpass.resolves.iter().map(make_ref).collect::<Box<[_]>>();

                        (colors, depth_stencil, inputs, preserves, resolves)
                    })
                    .collect::<Box<[_]>>();

                let subpasses = attachment_refs
                    .iter()
                    .map(|(colors, depth_stencil, inputs, preserves, resolves)| {
                        vk::SubpassDescription {
                            flags: vk::SubpassDescriptionFlags::empty(),
                            pipeline_bind_point: vk::PipelineBindPoint::GRAPHICS,
                            input_attachment_count: inputs.len() as u32,
                            p_input_attachments: inputs.as_ptr(),
                            color_attachment_count: colors.len() as u32,
                            p_color_attachments: colors.as_ptr(),
                            p_resolve_attachments: if resolves.is_empty() {
                                ptr::null()
                            } else {
                                resolves.as_ptr()
                            },
                            p_depth_stencil_attachment: match depth_stencil {
                                Some(ref aref) => aref as *const _,
                                None => ptr::null(),
                            },
                            preserve_attachment_count: preserves.len() as u32,
                            p_preserve_attachments: preserves.as_ptr(),
                        }
                    })
                    .collect::<Box<[_]>>();

                let result =
                    inplace_it::inplace_or_alloc_array(dependencies.len(), |uninit_guard| {
                        let dependencies = uninit_guard.init_with_iter(dependencies);

                        let info = vk::RenderPassCreateInfo::builder()
                            .flags(vk::RenderPassCreateFlags::empty())
                            .attachments(&attachments)
                            .subpasses(&subpasses)
                            .dependencies(&dependencies);

                        self.shared.raw.create_render_pass(&info, None)
                    });

                (clear_attachments_mask, result)
            });

        match result {
            Ok(renderpass) => Ok(n::RenderPass {
                raw: renderpass,
                clear_attachments_mask,
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn create_pipeline_layout<IS, IR>(
        &self,
        sets: IS,
        push_constant_ranges: IR,
    ) -> Result<n::PipelineLayout, d::OutOfMemory>
    where
        IS: IntoIterator,
        IS::Item: Borrow<n::DescriptorSetLayout>,
        IS::IntoIter: ExactSizeIterator,
        IR: IntoIterator,
        IR::Item: Borrow<(pso::ShaderStageFlags, Range<u32>)>,
        IR::IntoIter: ExactSizeIterator,
    {
        let set_layouts = sets.into_iter().map(|set| set.borrow().raw);

        let push_constant_ranges = push_constant_ranges.into_iter().map(|range| {
            let &(s, ref r) = range.borrow();
            vk::PushConstantRange {
                stage_flags: conv::map_stage_flags(s),
                offset: r.start,
                size: r.end - r.start,
            }
        });

        let result = inplace_it::inplace_or_alloc_array(set_layouts.len(), |uninit_guard| {
            let set_layouts = uninit_guard.init_with_iter(set_layouts);

            // TODO: set_layouts doesnt implement fmt::Debug, submit PR?
            // debug!("create_pipeline_layout {:?}", set_layouts);

            inplace_it::inplace_or_alloc_array(push_constant_ranges.len(), |uninit_guard| {
                let push_constant_ranges = uninit_guard.init_with_iter(push_constant_ranges);

                let info = vk::PipelineLayoutCreateInfo::builder()
                    .flags(vk::PipelineLayoutCreateFlags::empty())
                    .set_layouts(&set_layouts)
                    .push_constant_ranges(&push_constant_ranges);

                self.shared.raw.create_pipeline_layout(&info, None)
            })
        });

        match result {
            Ok(raw) => Ok(n::PipelineLayout { raw }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn create_pipeline_cache(
        &self,
        data: Option<&[u8]>,
    ) -> Result<n::PipelineCache, d::OutOfMemory> {
        let info =
            vk::PipelineCacheCreateInfo::builder().flags(vk::PipelineCacheCreateFlags::empty());
        let info = if let Some(d) = data {
            info.initial_data(d)
        } else {
            info
        };

        let result = self.shared.raw.create_pipeline_cache(&info, None);

        match result {
            Ok(raw) => Ok(n::PipelineCache { raw }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn get_pipeline_cache_data(
        &self,
        cache: &n::PipelineCache,
    ) -> Result<Vec<u8>, d::OutOfMemory> {
        let result = self.shared.raw.get_pipeline_cache_data(cache.raw);

        match result {
            Ok(data) => Ok(data),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn destroy_pipeline_cache(&self, cache: n::PipelineCache) {
        self.shared.raw.destroy_pipeline_cache(cache.raw, None);
    }

    unsafe fn merge_pipeline_caches<I>(
        &self,
        target: &n::PipelineCache,
        sources: I,
    ) -> Result<(), d::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<n::PipelineCache>,
        I::IntoIter: ExactSizeIterator,
    {
        let caches = sources.into_iter().map(|s| s.borrow().raw);

        let result = inplace_it::inplace_or_alloc_array(caches.len(), |uninit_guard| {
            let caches = uninit_guard.init_with_iter(caches);

            self.shared.raw.fp_v1_0().merge_pipeline_caches(
                self.shared.raw.handle(),
                target.raw,
                caches.len() as u32,
                caches.as_ptr(),
            )
        });

        match result {
            vk::Result::SUCCESS => Ok(()),
            vk::Result::ERROR_OUT_OF_HOST_MEMORY => Err(d::OutOfMemory::Host),
            vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn create_graphics_pipeline<'a>(
        &self,
        desc: &pso::GraphicsPipelineDesc<'a, B>,
        cache: Option<&n::PipelineCache>,
    ) -> Result<n::GraphicsPipeline, pso::CreationError> {
        debug!("create_graphics_pipeline {:?}", desc);

        let mut buf = GraphicsPipelineInfoBuf::default();
        let mut buf = Pin::new(&mut buf);
        GraphicsPipelineInfoBuf::initialize(&mut buf, self, desc);

        let info = {
            let (base_handle, base_index) = match desc.parent {
                pso::BasePipeline::Pipeline(pipeline) => (pipeline.0, -1),
                pso::BasePipeline::Index(index) => (vk::Pipeline::null(), index as _),
                pso::BasePipeline::None => (vk::Pipeline::null(), -1),
            };

            let mut flags = vk::PipelineCreateFlags::empty();
            match desc.parent {
                pso::BasePipeline::None => (),
                _ => {
                    flags |= vk::PipelineCreateFlags::DERIVATIVE;
                }
            }
            if desc
                .flags
                .contains(pso::PipelineCreationFlags::DISABLE_OPTIMIZATION)
            {
                flags |= vk::PipelineCreateFlags::DISABLE_OPTIMIZATION;
            }
            if desc
                .flags
                .contains(pso::PipelineCreationFlags::ALLOW_DERIVATIVES)
            {
                flags |= vk::PipelineCreateFlags::ALLOW_DERIVATIVES;
            }

            let builder = vk::GraphicsPipelineCreateInfo::builder()
                .flags(flags)
                .stages(&buf.stages)
                .vertex_input_state(&buf.vertex_input_state)
                .input_assembly_state(&buf.input_assembly_state)
                .rasterization_state(&buf.rasterization_state);
            let builder = match buf.tessellation_state.as_ref() {
                Some(t) => builder.tessellation_state(t),
                None => builder,
            };
            builder
                .viewport_state(&buf.viewport_state)
                .multisample_state(&buf.multisample_state)
                .depth_stencil_state(&buf.depth_stencil_state)
                .color_blend_state(&buf.color_blend_state)
                .dynamic_state(&buf.pipeline_dynamic_state)
                .layout(desc.layout.raw)
                .render_pass(desc.subpass.main_pass.raw)
                .subpass(desc.subpass.index as _)
                .base_pipeline_handle(base_handle)
                .base_pipeline_index(base_index)
        };

        let mut pipeline = vk::Pipeline::null();

        match self.shared.raw.fp_v1_0().create_graphics_pipelines(
            self.shared.raw.handle(),
            cache.map_or(vk::PipelineCache::null(), |cache| cache.raw),
            1,
            &*info,
            ptr::null(),
            &mut pipeline,
        ) {
            vk::Result::SUCCESS => Ok(n::GraphicsPipeline(pipeline)),
            vk::Result::ERROR_OUT_OF_HOST_MEMORY => Err(d::OutOfMemory::Host.into()),
            vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => Err(d::OutOfMemory::Device.into()),
            _ => Err(pso::CreationError::Other),
        }
    }

    unsafe fn create_graphics_pipelines<'a, T>(
        &self,
        descs: T,
        cache: Option<&n::PipelineCache>,
    ) -> Vec<Result<n::GraphicsPipeline, pso::CreationError>>
    where
        T: IntoIterator,
        T::Item: Borrow<pso::GraphicsPipelineDesc<'a, B>>,
    {
        debug!("create_graphics_pipelines:");

        let mut bufs: Pin<Box<[_]>> = descs
            .into_iter()
            .enumerate()
            .inspect(|(idx, desc)| debug!("# {} {:?}", idx, desc.borrow()))
            .map(|(_, desc)| (desc, GraphicsPipelineInfoBuf::default()))
            .collect::<Box<[_]>>()
            .into();

        for (desc, buf) in bufs.as_mut().get_unchecked_mut() {
            let desc: &T::Item = desc;
            GraphicsPipelineInfoBuf::initialize(&mut Pin::new_unchecked(buf), self, desc.borrow());
        }

        let infos: Vec<_> = bufs
            .iter()
            .map(|(desc, buf)| {
                let desc = desc.borrow();

                let (base_handle, base_index) = match desc.parent {
                    pso::BasePipeline::Pipeline(pipeline) => (pipeline.0, -1),
                    pso::BasePipeline::Index(index) => (vk::Pipeline::null(), index as _),
                    pso::BasePipeline::None => (vk::Pipeline::null(), -1),
                };

                let mut flags = vk::PipelineCreateFlags::empty();
                match desc.parent {
                    pso::BasePipeline::None => (),
                    _ => {
                        flags |= vk::PipelineCreateFlags::DERIVATIVE;
                    }
                }
                if desc
                    .flags
                    .contains(pso::PipelineCreationFlags::DISABLE_OPTIMIZATION)
                {
                    flags |= vk::PipelineCreateFlags::DISABLE_OPTIMIZATION;
                }
                if desc
                    .flags
                    .contains(pso::PipelineCreationFlags::ALLOW_DERIVATIVES)
                {
                    flags |= vk::PipelineCreateFlags::ALLOW_DERIVATIVES;
                }

                let builder = vk::GraphicsPipelineCreateInfo::builder()
                    .flags(flags)
                    .stages(&buf.stages)
                    .vertex_input_state(&buf.vertex_input_state)
                    .input_assembly_state(&buf.input_assembly_state)
                    .rasterization_state(&buf.rasterization_state);
                let builder = match buf.tessellation_state.as_ref() {
                    Some(t) => builder.tessellation_state(t),
                    None => builder,
                };
                builder
                    .viewport_state(&buf.viewport_state)
                    .multisample_state(&buf.multisample_state)
                    .depth_stencil_state(&buf.depth_stencil_state)
                    .color_blend_state(&buf.color_blend_state)
                    .dynamic_state(&buf.pipeline_dynamic_state)
                    .layout(desc.layout.raw)
                    .render_pass(desc.subpass.main_pass.raw)
                    .subpass(desc.subpass.index as _)
                    .base_pipeline_handle(base_handle)
                    .base_pipeline_index(base_index)
                    .build()
            })
            .collect();

        let (pipelines, error) = if infos.is_empty() {
            (Vec::new(), None)
        } else {
            match self.shared.raw.create_graphics_pipelines(
                cache.map_or(vk::PipelineCache::null(), |cache| cache.raw),
                &infos,
                None,
            ) {
                Ok(pipelines) => (pipelines, None),
                Err((pipelines, error)) => (pipelines, Some(error)),
            }
        };

        pipelines
            .into_iter()
            .map(|pso| {
                if pso == vk::Pipeline::null() {
                    match error {
                        Some(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => {
                            Err(d::OutOfMemory::Host.into())
                        }
                        Some(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => {
                            Err(d::OutOfMemory::Device.into())
                        }
                        _ => unreachable!(),
                    }
                } else {
                    Ok(n::GraphicsPipeline(pso))
                }
            })
            .collect()
    }

    unsafe fn create_compute_pipeline<'a>(
        &self,
        desc: &pso::ComputePipelineDesc<'a, B>,
        cache: Option<&n::PipelineCache>,
    ) -> Result<n::ComputePipeline, pso::CreationError> {
        let mut buf = ComputePipelineInfoBuf::default();
        let mut buf = Pin::new(&mut buf);
        ComputePipelineInfoBuf::initialize(&mut buf, desc);

        let info = {
            let stage = vk::PipelineShaderStageCreateInfo::builder()
                .flags(vk::PipelineShaderStageCreateFlags::empty())
                .stage(vk::ShaderStageFlags::COMPUTE)
                .module(desc.shader.module.raw)
                .name(buf.c_string.as_c_str())
                .specialization_info(&buf.specialization);

            let (base_handle, base_index) = match desc.parent {
                pso::BasePipeline::Pipeline(pipeline) => (pipeline.0, -1),
                pso::BasePipeline::Index(index) => (vk::Pipeline::null(), index as _),
                pso::BasePipeline::None => (vk::Pipeline::null(), -1),
            };

            let mut flags = vk::PipelineCreateFlags::empty();
            match desc.parent {
                pso::BasePipeline::None => (),
                _ => {
                    flags |= vk::PipelineCreateFlags::DERIVATIVE;
                }
            }
            if desc
                .flags
                .contains(pso::PipelineCreationFlags::DISABLE_OPTIMIZATION)
            {
                flags |= vk::PipelineCreateFlags::DISABLE_OPTIMIZATION;
            }
            if desc
                .flags
                .contains(pso::PipelineCreationFlags::ALLOW_DERIVATIVES)
            {
                flags |= vk::PipelineCreateFlags::ALLOW_DERIVATIVES;
            }

            vk::ComputePipelineCreateInfo::builder()
                .flags(flags)
                .stage(*stage)
                .layout(desc.layout.raw)
                .base_pipeline_handle(base_handle)
                .base_pipeline_index(base_index)
                .build()
        };

        let mut pipeline = vk::Pipeline::null();

        match self.shared.raw.fp_v1_0().create_compute_pipelines(
            self.shared.raw.handle(),
            cache.map_or(vk::PipelineCache::null(), |cache| cache.raw),
            1,
            &info,
            ptr::null(),
            &mut pipeline,
        ) {
            vk::Result::SUCCESS => Ok(n::ComputePipeline(pipeline)),
            vk::Result::ERROR_OUT_OF_HOST_MEMORY => Err(d::OutOfMemory::Host.into()),
            vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => Err(d::OutOfMemory::Device.into()),
            _ => Err(pso::CreationError::Other),
        }
    }

    unsafe fn create_compute_pipelines<'a, T>(
        &self,
        descs: T,
        cache: Option<&n::PipelineCache>,
    ) -> Vec<Result<n::ComputePipeline, pso::CreationError>>
    where
        T: IntoIterator,
        T::Item: Borrow<pso::ComputePipelineDesc<'a, B>>,
    {
        let mut bufs: Pin<Box<[_]>> = descs
            .into_iter()
            .map(|desc| (desc, ComputePipelineInfoBuf::default()))
            .collect::<Box<[_]>>()
            .into();

        for (desc, buf) in bufs.as_mut().get_unchecked_mut() {
            let desc: &T::Item = desc;
            ComputePipelineInfoBuf::initialize(&mut Pin::new_unchecked(buf), desc.borrow());
        }

        let infos: Vec<_> = bufs
            .iter()
            .map(|(desc, buf)| {
                let desc = desc.borrow();

                let stage = vk::PipelineShaderStageCreateInfo::builder()
                    .flags(vk::PipelineShaderStageCreateFlags::empty())
                    .stage(vk::ShaderStageFlags::COMPUTE)
                    .module(desc.shader.module.raw)
                    .name(buf.c_string.as_c_str())
                    .specialization_info(&buf.specialization);

                let (base_handle, base_index) = match desc.parent {
                    pso::BasePipeline::Pipeline(pipeline) => (pipeline.0, -1),
                    pso::BasePipeline::Index(index) => (vk::Pipeline::null(), index as _),
                    pso::BasePipeline::None => (vk::Pipeline::null(), -1),
                };

                let mut flags = vk::PipelineCreateFlags::empty();
                match desc.parent {
                    pso::BasePipeline::None => (),
                    _ => {
                        flags |= vk::PipelineCreateFlags::DERIVATIVE;
                    }
                }
                if desc
                    .flags
                    .contains(pso::PipelineCreationFlags::DISABLE_OPTIMIZATION)
                {
                    flags |= vk::PipelineCreateFlags::DISABLE_OPTIMIZATION;
                }
                if desc
                    .flags
                    .contains(pso::PipelineCreationFlags::ALLOW_DERIVATIVES)
                {
                    flags |= vk::PipelineCreateFlags::ALLOW_DERIVATIVES;
                }

                vk::ComputePipelineCreateInfo::builder()
                    .flags(flags)
                    .stage(*stage)
                    .layout(desc.layout.raw)
                    .base_pipeline_handle(base_handle)
                    .base_pipeline_index(base_index)
                    .build()
            })
            .collect();

        let (pipelines, error) = if infos.is_empty() {
            (Vec::new(), None)
        } else {
            match self.shared.raw.create_compute_pipelines(
                cache.map_or(vk::PipelineCache::null(), |cache| cache.raw),
                &infos,
                None,
            ) {
                Ok(pipelines) => (pipelines, None),
                Err((pipelines, error)) => (pipelines, Some(error)),
            }
        };

        pipelines
            .into_iter()
            .map(|pso| {
                if pso == vk::Pipeline::null() {
                    match error {
                        Some(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => {
                            Err(d::OutOfMemory::Host.into())
                        }
                        Some(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => {
                            Err(d::OutOfMemory::Device.into())
                        }
                        _ => unreachable!(),
                    }
                } else {
                    Ok(n::ComputePipeline(pso))
                }
            })
            .collect()
    }

    unsafe fn create_framebuffer<T>(
        &self,
        renderpass: &n::RenderPass,
        attachments: T,
        extent: image::Extent,
    ) -> Result<n::Framebuffer, d::OutOfMemory>
    where
        T: IntoIterator,
        T::Item: Borrow<n::ImageView>,
    {
        let mut framebuffers_ptr = None;
        let mut raw_attachments = SmallVec::<[_; 4]>::new();
        for attachment in attachments {
            let at = attachment.borrow();
            raw_attachments.push(at.view);
            match at.owner {
                n::ImageViewOwner::User => {}
                n::ImageViewOwner::Surface(ref fbo_ptr) => {
                    framebuffers_ptr = Some(Arc::clone(&fbo_ptr.0));
                }
            }
        }

        let info = vk::FramebufferCreateInfo::builder()
            .flags(vk::FramebufferCreateFlags::empty())
            .render_pass(renderpass.raw)
            .attachments(&raw_attachments)
            .width(extent.width)
            .height(extent.height)
            .layers(extent.depth);

        let result = self.shared.raw.create_framebuffer(&info, None);

        match result {
            Ok(raw) => Ok(n::Framebuffer {
                raw,
                owned: match framebuffers_ptr {
                    Some(fbo_ptr) => {
                        fbo_ptr.lock().unwrap().framebuffers.push(raw);
                        false
                    }
                    None => true,
                },
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn create_shader_module(
        &self,
        spirv_data: &[u32],
    ) -> Result<n::ShaderModule, d::ShaderError> {
        let info = vk::ShaderModuleCreateInfo::builder()
            .flags(vk::ShaderModuleCreateFlags::empty())
            .code(spirv_data);

        let module = self.shared.raw.create_shader_module(&info, None);

        match module {
            Ok(raw) => Ok(n::ShaderModule { raw }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            Err(_) => {
                Err(d::ShaderError::CompilationFailed(String::new())) // TODO
            }
        }
    }

    unsafe fn create_sampler(
        &self,
        desc: &image::SamplerDesc,
    ) -> Result<n::Sampler, d::AllocationError> {
        use hal::pso::Comparison;

        let (anisotropy_enable, max_anisotropy) =
            desc.anisotropy_clamp.map_or((false, 1.0), |aniso| {
                if self.shared.features.contains(Features::SAMPLER_ANISOTROPY) {
                    (true, aniso as f32)
                } else {
                    warn!(
                        "Anisotropy({}) was requested on a device with disabled feature",
                        aniso
                    );
                    (false, 1.0)
                }
            });
        let info = vk::SamplerCreateInfo::builder()
            .flags(vk::SamplerCreateFlags::empty())
            .mag_filter(conv::map_filter(desc.mag_filter))
            .min_filter(conv::map_filter(desc.min_filter))
            .mipmap_mode(conv::map_mip_filter(desc.mip_filter))
            .address_mode_u(conv::map_wrap(desc.wrap_mode.0))
            .address_mode_v(conv::map_wrap(desc.wrap_mode.1))
            .address_mode_w(conv::map_wrap(desc.wrap_mode.2))
            .mip_lod_bias(desc.lod_bias.0)
            .anisotropy_enable(anisotropy_enable)
            .max_anisotropy(max_anisotropy)
            .compare_enable(desc.comparison.is_some())
            .compare_op(conv::map_comparison(
                desc.comparison.unwrap_or(Comparison::Never),
            ))
            .min_lod(desc.lod_range.start.0)
            .max_lod(desc.lod_range.end.0)
            .border_color(conv::map_border_color(desc.border))
            .unnormalized_coordinates(!desc.normalized);

        let result = self.shared.raw.create_sampler(&info, None);

        match result {
            Ok(sampler) => Ok(n::Sampler(sampler)),
            Err(vk::Result::ERROR_TOO_MANY_OBJECTS) => Err(d::AllocationError::TooManyObjects),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    ///
    unsafe fn create_buffer(
        &self,
        size: u64,
        usage: buffer::Usage,
    ) -> Result<n::Buffer, buffer::CreationError> {
        let info = vk::BufferCreateInfo::builder()
            .flags(vk::BufferCreateFlags::empty()) // TODO:
            .size(size)
            .usage(conv::map_buffer_usage(usage))
            .sharing_mode(vk::SharingMode::EXCLUSIVE); // TODO:

        let result = self.shared.raw.create_buffer(&info, None);

        match result {
            Ok(raw) => Ok(n::Buffer { raw }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn get_buffer_requirements(&self, buffer: &n::Buffer) -> Requirements {
        let req = self.shared.raw.get_buffer_memory_requirements(buffer.raw);

        Requirements {
            size: req.size,
            alignment: req.alignment,
            type_mask: self.filter_memory_requirements(req.memory_type_bits),
        }
    }

    unsafe fn bind_buffer_memory(
        &self,
        memory: &n::Memory,
        offset: u64,
        buffer: &mut n::Buffer,
    ) -> Result<(), d::BindError> {
        let result = self
            .shared
            .raw
            .bind_buffer_memory(buffer.raw, memory.raw, offset);

        match result {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn create_buffer_view(
        &self,
        buffer: &n::Buffer,
        format: Option<format::Format>,
        range: buffer::SubRange,
    ) -> Result<n::BufferView, buffer::ViewCreationError> {
        let info = vk::BufferViewCreateInfo::builder()
            .flags(vk::BufferViewCreateFlags::empty())
            .buffer(buffer.raw)
            .format(format.map_or(vk::Format::UNDEFINED, conv::map_format))
            .offset(range.offset)
            .range(range.size.unwrap_or(vk::WHOLE_SIZE));

        let result = self.shared.raw.create_buffer_view(&info, None);

        match result {
            Ok(raw) => Ok(n::BufferView { raw }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn create_image(
        &self,
        kind: image::Kind,
        mip_levels: image::Level,
        format: format::Format,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Result<n::Image, image::CreationError> {
        let flags = conv::map_view_capabilities(view_caps);
        let extent = conv::map_extent(kind.extent());
        let array_layers = kind.num_layers();
        let samples = kind.num_samples() as u32;
        let image_type = match kind {
            image::Kind::D1(..) => vk::ImageType::TYPE_1D,
            image::Kind::D2(..) => vk::ImageType::TYPE_2D,
            image::Kind::D3(..) => vk::ImageType::TYPE_3D,
        };

        let info = vk::ImageCreateInfo::builder()
            .flags(flags)
            .image_type(image_type)
            .format(conv::map_format(format))
            .extent(extent.clone())
            .mip_levels(mip_levels as u32)
            .array_layers(array_layers as u32)
            .samples(vk::SampleCountFlags::from_raw(
                samples & vk::SampleCountFlags::all().as_raw(),
            ))
            .tiling(conv::map_tiling(tiling))
            .usage(conv::map_image_usage(usage))
            .sharing_mode(vk::SharingMode::EXCLUSIVE) // TODO:
            .initial_layout(vk::ImageLayout::UNDEFINED);

        let result = self.shared.raw.create_image(&info, None);

        match result {
            Ok(raw) => Ok(n::Image {
                raw,
                ty: image_type,
                flags,
                extent,
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn get_image_requirements(&self, image: &n::Image) -> Requirements {
        let req = self.shared.raw.get_image_memory_requirements(image.raw);

        Requirements {
            size: req.size,
            alignment: req.alignment,
            type_mask: self.filter_memory_requirements(req.memory_type_bits),
        }
    }

    unsafe fn get_image_subresource_footprint(
        &self,
        image: &n::Image,
        subresource: image::Subresource,
    ) -> image::SubresourceFootprint {
        let sub = conv::map_subresource(&subresource);
        let layout = self.shared.raw.get_image_subresource_layout(image.raw, sub);

        image::SubresourceFootprint {
            slice: layout.offset..layout.offset + layout.size,
            row_pitch: layout.row_pitch,
            array_pitch: layout.array_pitch,
            depth_pitch: layout.depth_pitch,
        }
    }

    unsafe fn bind_image_memory(
        &self,
        memory: &n::Memory,
        offset: u64,
        image: &mut n::Image,
    ) -> Result<(), d::BindError> {
        // TODO: error handling
        // TODO: check required type
        let result = self
            .shared
            .raw
            .bind_image_memory(image.raw, memory.raw, offset);

        match result {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn create_image_view(
        &self,
        image: &n::Image,
        kind: image::ViewKind,
        format: format::Format,
        swizzle: format::Swizzle,
        range: image::SubresourceRange,
    ) -> Result<n::ImageView, image::ViewCreationError> {
        let is_cube = image
            .flags
            .intersects(vk::ImageCreateFlags::CUBE_COMPATIBLE);
        let info = vk::ImageViewCreateInfo::builder()
            .flags(vk::ImageViewCreateFlags::empty())
            .image(image.raw)
            .view_type(match conv::map_view_kind(kind, image.ty, is_cube) {
                Some(ty) => ty,
                None => return Err(image::ViewCreationError::BadKind(kind)),
            })
            .format(conv::map_format(format))
            .components(conv::map_swizzle(swizzle))
            .subresource_range(conv::map_subresource_range(&range));

        let result = self.shared.raw.create_image_view(&info, None);

        match result {
            Ok(view) => Ok(n::ImageView {
                image: image.raw,
                view,
                range,
                owner: n::ImageViewOwner::User,
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn create_descriptor_pool<T>(
        &self,
        max_sets: usize,
        descriptor_pools: T,
        flags: pso::DescriptorPoolCreateFlags,
    ) -> Result<n::DescriptorPool, d::OutOfMemory>
    where
        T: IntoIterator,
        T::Item: Borrow<pso::DescriptorRangeDesc>,
        T::IntoIter: ExactSizeIterator,
    {
        let pools = descriptor_pools.into_iter().map(|pool| {
            let pool = pool.borrow();
            vk::DescriptorPoolSize {
                ty: conv::map_descriptor_type(pool.ty),
                descriptor_count: pool.count as u32,
            }
        });

        let result = inplace_it::inplace_or_alloc_array(pools.len(), |uninit_guard| {
            let pools = uninit_guard.init_with_iter(pools);

            let info = vk::DescriptorPoolCreateInfo::builder()
                .flags(conv::map_descriptor_pool_create_flags(flags))
                .max_sets(max_sets as u32)
                .pool_sizes(&pools);

            self.shared.raw.create_descriptor_pool(&info, None)
        });

        match result {
            Ok(pool) => Ok(n::DescriptorPool {
                raw: pool,
                device: self.shared.clone(),
                set_free_vec: Vec::new(),
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn create_descriptor_set_layout<I, J>(
        &self,
        binding_iter: I,
        immutable_sampler_iter: J,
    ) -> Result<n::DescriptorSetLayout, d::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetLayoutBinding>,
        J: IntoIterator,
        J::Item: Borrow<n::Sampler>,
        J::IntoIter: ExactSizeIterator,
    {
        let immutable_samplers = immutable_sampler_iter.into_iter().map(|is| is.borrow().0);
        let mut sampler_offset = 0;

        let mut bindings = binding_iter
            .into_iter()
            .map(|b| b.borrow().clone())
            .collect::<Vec<_>>();
        // Sorting will come handy in `write_descriptor_sets`.
        bindings.sort_by_key(|b| b.binding);

        let result = inplace_it::inplace_or_alloc_array(immutable_samplers.len(), |uninit_guard| {
            let immutable_samplers = uninit_guard.init_with_iter(immutable_samplers);

            let raw_bindings = bindings.iter().map(|b| vk::DescriptorSetLayoutBinding {
                binding: b.binding,
                descriptor_type: conv::map_descriptor_type(b.ty),
                descriptor_count: b.count as _,
                stage_flags: conv::map_stage_flags(b.stage_flags),
                p_immutable_samplers: if b.immutable_samplers {
                    let slice = &immutable_samplers[sampler_offset..];
                    sampler_offset += b.count;
                    slice.as_ptr()
                } else {
                    ptr::null()
                },
            });

            inplace_it::inplace_or_alloc_array(raw_bindings.len(), |uninit_guard| {
                let raw_bindings = uninit_guard.init_with_iter(raw_bindings);

                // TODO raw_bindings doesnt implement fmt::Debug
                // debug!("create_descriptor_set_layout {:?}", raw_bindings);

                let info = vk::DescriptorSetLayoutCreateInfo::builder()
                    .flags(vk::DescriptorSetLayoutCreateFlags::empty())
                    .bindings(&raw_bindings);

                self.shared.raw.create_descriptor_set_layout(&info, None)
            })
        });

        match result {
            Ok(layout) => Ok(n::DescriptorSetLayout {
                raw: layout,
                bindings: Arc::new(bindings),
            }),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn write_descriptor_sets<'a, I, J>(&self, write_iter: I)
    where
        I: IntoIterator<Item = pso::DescriptorSetWrite<'a, B, J>>,
        J: IntoIterator,
        J::Item: Borrow<pso::Descriptor<'a, B>>,
    {
        let mut raw_writes = Vec::<vk::WriteDescriptorSet>::new();
        let mut image_infos = Vec::new();
        let mut buffer_infos = Vec::new();
        let mut texel_buffer_views = Vec::new();

        for sw in write_iter {
            // gfx-hal allows the type and stages to be different between the descriptor
            // in a single write, while Vulkan requires them to be the same.
            let mut last_type = vk::DescriptorType::SAMPLER;
            let mut last_stages = pso::ShaderStageFlags::empty();

            let mut binding_pos = sw
                .set
                .bindings
                .binary_search_by_key(&sw.binding, |b| b.binding)
                .expect("Descriptor set writes don't match the set layout!");
            let mut array_offset = sw.array_offset;

            for descriptor in sw.descriptors {
                let layout_binding = &sw.set.bindings[binding_pos];
                array_offset += 1;
                if array_offset == layout_binding.count {
                    array_offset = 0;
                    binding_pos += 1;
                }

                let descriptor_type = conv::map_descriptor_type(layout_binding.ty);
                if descriptor_type == last_type && layout_binding.stage_flags == last_stages {
                    raw_writes.last_mut().unwrap().descriptor_count += 1;
                } else {
                    last_type = descriptor_type;
                    last_stages = layout_binding.stage_flags;
                    raw_writes.push(vk::WriteDescriptorSet {
                        s_type: vk::StructureType::WRITE_DESCRIPTOR_SET,
                        p_next: ptr::null(),
                        dst_set: sw.set.raw,
                        dst_binding: layout_binding.binding,
                        dst_array_element: if layout_binding.binding == sw.binding {
                            sw.array_offset as _
                        } else {
                            0
                        },
                        descriptor_count: 1,
                        descriptor_type,
                        p_image_info: image_infos.len() as _,
                        p_buffer_info: buffer_infos.len() as _,
                        p_texel_buffer_view: texel_buffer_views.len() as _,
                    });
                }

                match *descriptor.borrow() {
                    pso::Descriptor::Sampler(sampler) => {
                        image_infos.push(
                            vk::DescriptorImageInfo::builder()
                                .sampler(sampler.0)
                                .image_view(vk::ImageView::null())
                                .image_layout(vk::ImageLayout::GENERAL)
                                .build(),
                        );
                    }
                    pso::Descriptor::Image(view, layout) => {
                        image_infos.push(
                            vk::DescriptorImageInfo::builder()
                                .sampler(vk::Sampler::null())
                                .image_view(view.view)
                                .image_layout(conv::map_image_layout(layout))
                                .build(),
                        );
                    }
                    pso::Descriptor::CombinedImageSampler(view, layout, sampler) => {
                        image_infos.push(
                            vk::DescriptorImageInfo::builder()
                                .sampler(sampler.0)
                                .image_view(view.view)
                                .image_layout(conv::map_image_layout(layout))
                                .build(),
                        );
                    }
                    pso::Descriptor::Buffer(buffer, ref sub) => {
                        buffer_infos.push(
                            vk::DescriptorBufferInfo::builder()
                                .buffer(buffer.raw)
                                .offset(sub.offset)
                                .range(sub.size.unwrap_or(vk::WHOLE_SIZE))
                                .build(),
                        );
                    }
                    pso::Descriptor::TexelBuffer(view) => {
                        texel_buffer_views.push(view.raw);
                    }
                }
            }
        }

        // Patch the pointers now that we have all the storage allocated.
        for raw in &mut raw_writes {
            use crate::vk::DescriptorType as Dt;
            match raw.descriptor_type {
                Dt::SAMPLER
                | Dt::SAMPLED_IMAGE
                | Dt::STORAGE_IMAGE
                | Dt::COMBINED_IMAGE_SAMPLER
                | Dt::INPUT_ATTACHMENT => {
                    raw.p_buffer_info = ptr::null();
                    raw.p_texel_buffer_view = ptr::null();
                    raw.p_image_info = image_infos[raw.p_image_info as usize..].as_ptr();
                }
                Dt::UNIFORM_TEXEL_BUFFER | Dt::STORAGE_TEXEL_BUFFER => {
                    raw.p_buffer_info = ptr::null();
                    raw.p_image_info = ptr::null();
                    raw.p_texel_buffer_view =
                        texel_buffer_views[raw.p_texel_buffer_view as usize..].as_ptr();
                }
                Dt::UNIFORM_BUFFER
                | Dt::STORAGE_BUFFER
                | Dt::STORAGE_BUFFER_DYNAMIC
                | Dt::UNIFORM_BUFFER_DYNAMIC => {
                    raw.p_image_info = ptr::null();
                    raw.p_texel_buffer_view = ptr::null();
                    raw.p_buffer_info = buffer_infos[raw.p_buffer_info as usize..].as_ptr();
                }
                _ => panic!("unknown descriptor type"),
            }
        }

        self.shared.raw.update_descriptor_sets(&raw_writes, &[]);
    }

    unsafe fn copy_descriptor_sets<'a, I>(&self, copies: I)
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetCopy<'a, B>>,
        I::IntoIter: ExactSizeIterator,
    {
        let copies = copies.into_iter().map(|copy| {
            let c = copy.borrow();
            vk::CopyDescriptorSet::builder()
                .src_set(c.src_set.raw)
                .src_binding(c.src_binding as u32)
                .src_array_element(c.src_array_offset as u32)
                .dst_set(c.dst_set.raw)
                .dst_binding(c.dst_binding as u32)
                .dst_array_element(c.dst_array_offset as u32)
                .descriptor_count(c.count as u32)
                .build()
        });

        inplace_it::inplace_or_alloc_array(copies.len(), |uninit_guard| {
            let copies = uninit_guard.init_with_iter(copies);

            self.shared.raw.update_descriptor_sets(&[], &copies);
        });
    }

    unsafe fn map_memory(
        &self,
        memory: &n::Memory,
        segment: Segment,
    ) -> Result<*mut u8, d::MapError> {
        let result = self.shared.raw.map_memory(
            memory.raw,
            segment.offset,
            segment.size.unwrap_or(vk::WHOLE_SIZE),
            vk::MemoryMapFlags::empty(),
        );

        match result {
            Ok(ptr) => Ok(ptr as *mut _),
            Err(vk::Result::ERROR_MEMORY_MAP_FAILED) => Err(d::MapError::MappingFailed),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn unmap_memory(&self, memory: &n::Memory) {
        self.shared.raw.unmap_memory(memory.raw)
    }

    unsafe fn flush_mapped_memory_ranges<'a, I>(&self, ranges: I) -> Result<(), d::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a n::Memory, Segment)>,
    {
        let ranges = conv::map_memory_ranges(ranges);
        let result = self.shared.raw.flush_mapped_memory_ranges(&ranges);

        match result {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn invalidate_mapped_memory_ranges<'a, I>(&self, ranges: I) -> Result<(), d::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a n::Memory, Segment)>,
    {
        let ranges = conv::map_memory_ranges(ranges);
        let result = self.shared.raw.invalidate_mapped_memory_ranges(&ranges);

        match result {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    fn create_semaphore(&self) -> Result<n::Semaphore, d::OutOfMemory> {
        let info = vk::SemaphoreCreateInfo::builder().flags(vk::SemaphoreCreateFlags::empty());

        let result = unsafe { self.shared.raw.create_semaphore(&info, None) };

        match result {
            Ok(semaphore) => Ok(n::Semaphore(semaphore)),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    fn create_fence(&self, signaled: bool) -> Result<n::Fence, d::OutOfMemory> {
        let info = vk::FenceCreateInfo::builder().flags(if signaled {
            vk::FenceCreateFlags::SIGNALED
        } else {
            vk::FenceCreateFlags::empty()
        });

        let result = unsafe { self.shared.raw.create_fence(&info, None) };

        match result {
            Ok(fence) => Ok(n::Fence(fence)),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn reset_fences<I>(&self, fences: I) -> Result<(), d::OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<n::Fence>,
        I::IntoIter: ExactSizeIterator,
    {
        let fences = fences.into_iter().map(|fence| fence.borrow().0);

        let result = inplace_it::inplace_or_alloc_array(fences.len(), |uninit_guard| {
            let fences = uninit_guard.init_with_iter(fences);
            self.shared.raw.reset_fences(&fences)
        });

        match result {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn wait_for_fences<I>(
        &self,
        fences: I,
        wait: d::WaitFor,
        timeout_ns: u64,
    ) -> Result<bool, d::OomOrDeviceLost>
    where
        I: IntoIterator,
        I::Item: Borrow<n::Fence>,
        I::IntoIter: ExactSizeIterator,
    {
        let fences = fences.into_iter().map(|fence| fence.borrow().0);

        let all = match wait {
            d::WaitFor::Any => false,
            d::WaitFor::All => true,
        };

        let result = inplace_it::inplace_or_alloc_array(fences.len(), |uninit_guard| {
            let fences = uninit_guard.init_with_iter(fences);
            self.shared.raw.wait_for_fences(&fences, all, timeout_ns)
        });

        match result {
            Ok(()) => Ok(true),
            Err(vk::Result::TIMEOUT) => Ok(false),
            Err(vk::Result::ERROR_DEVICE_LOST) => Err(d::DeviceLost.into()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn get_fence_status(&self, fence: &n::Fence) -> Result<bool, d::DeviceLost> {
        let result = self.shared.raw.get_fence_status(fence.0);
        match result {
            Ok(ok) => Ok(ok),
            Err(vk::Result::NOT_READY) => Ok(false), //TODO: shouldn't be needed
            Err(vk::Result::ERROR_DEVICE_LOST) => Err(d::DeviceLost),
            _ => unreachable!(),
        }
    }

    fn create_event(&self) -> Result<n::Event, d::OutOfMemory> {
        let info = vk::EventCreateInfo::builder().flags(vk::EventCreateFlags::empty());

        let result = unsafe { self.shared.raw.create_event(&info, None) };
        match result {
            Ok(e) => Ok(n::Event(e)),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn get_event_status(&self, event: &n::Event) -> Result<bool, d::OomOrDeviceLost> {
        let result = self.shared.raw.get_event_status(event.0);
        match result {
            Ok(b) => Ok(b),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            Err(vk::Result::ERROR_DEVICE_LOST) => Err(d::DeviceLost.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn set_event(&self, event: &n::Event) -> Result<(), d::OutOfMemory> {
        let result = self.shared.raw.set_event(event.0);
        match result {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn reset_event(&self, event: &n::Event) -> Result<(), d::OutOfMemory> {
        let result = self.shared.raw.reset_event(event.0);
        match result {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn free_memory(&self, memory: n::Memory) {
        self.shared.raw.free_memory(memory.raw, None);
    }

    unsafe fn create_query_pool(
        &self,
        ty: query::Type,
        query_count: query::Id,
    ) -> Result<n::QueryPool, query::CreationError> {
        let (query_type, pipeline_statistics) = match ty {
            query::Type::Occlusion => (
                vk::QueryType::OCCLUSION,
                vk::QueryPipelineStatisticFlags::empty(),
            ),
            query::Type::PipelineStatistics(statistics) => (
                vk::QueryType::PIPELINE_STATISTICS,
                conv::map_pipeline_statistics(statistics),
            ),
            query::Type::Timestamp => (
                vk::QueryType::TIMESTAMP,
                vk::QueryPipelineStatisticFlags::empty(),
            ),
        };

        let info = vk::QueryPoolCreateInfo::builder()
            .flags(vk::QueryPoolCreateFlags::empty())
            .query_type(query_type)
            .query_count(query_count)
            .pipeline_statistics(pipeline_statistics);

        let result = self.shared.raw.create_query_pool(&info, None);

        match result {
            Ok(pool) => Ok(n::QueryPool(pool)),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn get_query_pool_results(
        &self,
        pool: &n::QueryPool,
        queries: Range<query::Id>,
        data: &mut [u8],
        stride: buffer::Offset,
        flags: query::ResultFlags,
    ) -> Result<bool, d::OomOrDeviceLost> {
        let result = self.shared.raw.fp_v1_0().get_query_pool_results(
            self.shared.raw.handle(),
            pool.0,
            queries.start,
            queries.end - queries.start,
            data.len(),
            data.as_mut_ptr() as *mut _,
            stride,
            conv::map_query_result_flags(flags),
        );

        match result {
            vk::Result::SUCCESS => Ok(true),
            vk::Result::NOT_READY => Ok(false),
            vk::Result::ERROR_DEVICE_LOST => Err(d::DeviceLost.into()),
            vk::Result::ERROR_OUT_OF_HOST_MEMORY => Err(d::OutOfMemory::Host.into()),
            vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => Err(d::OutOfMemory::Device.into()),
            _ => unreachable!(),
        }
    }

    unsafe fn destroy_query_pool(&self, pool: n::QueryPool) {
        self.shared.raw.destroy_query_pool(pool.0, None);
    }

    unsafe fn destroy_shader_module(&self, module: n::ShaderModule) {
        self.shared.raw.destroy_shader_module(module.raw, None);
    }

    unsafe fn destroy_render_pass(&self, rp: n::RenderPass) {
        self.shared.raw.destroy_render_pass(rp.raw, None);
    }

    unsafe fn destroy_pipeline_layout(&self, pl: n::PipelineLayout) {
        self.shared.raw.destroy_pipeline_layout(pl.raw, None);
    }

    unsafe fn destroy_graphics_pipeline(&self, pipeline: n::GraphicsPipeline) {
        self.shared.raw.destroy_pipeline(pipeline.0, None);
    }

    unsafe fn destroy_compute_pipeline(&self, pipeline: n::ComputePipeline) {
        self.shared.raw.destroy_pipeline(pipeline.0, None);
    }

    unsafe fn destroy_framebuffer(&self, fb: n::Framebuffer) {
        if fb.owned {
            self.shared.raw.destroy_framebuffer(fb.raw, None);
        }
    }

    unsafe fn destroy_buffer(&self, buffer: n::Buffer) {
        self.shared.raw.destroy_buffer(buffer.raw, None);
    }

    unsafe fn destroy_buffer_view(&self, view: n::BufferView) {
        self.shared.raw.destroy_buffer_view(view.raw, None);
    }

    unsafe fn destroy_image(&self, image: n::Image) {
        self.shared.raw.destroy_image(image.raw, None);
    }

    unsafe fn destroy_image_view(&self, view: n::ImageView) {
        match view.owner {
            n::ImageViewOwner::User => {
                self.shared.raw.destroy_image_view(view.view, None);
            }
            n::ImageViewOwner::Surface(_fbo_cache) => {
                //TODO: mark as deleted?
            }
        }
    }

    unsafe fn destroy_sampler(&self, sampler: n::Sampler) {
        self.shared.raw.destroy_sampler(sampler.0, None);
    }

    unsafe fn destroy_descriptor_pool(&self, pool: n::DescriptorPool) {
        self.shared.raw.destroy_descriptor_pool(pool.raw, None);
    }

    unsafe fn destroy_descriptor_set_layout(&self, layout: n::DescriptorSetLayout) {
        self.shared
            .raw
            .destroy_descriptor_set_layout(layout.raw, None);
    }

    unsafe fn destroy_fence(&self, fence: n::Fence) {
        self.shared.raw.destroy_fence(fence.0, None);
    }

    unsafe fn destroy_semaphore(&self, semaphore: n::Semaphore) {
        self.shared.raw.destroy_semaphore(semaphore.0, None);
    }

    unsafe fn destroy_event(&self, event: n::Event) {
        self.shared.raw.destroy_event(event.0, None);
    }

    fn wait_idle(&self) -> Result<(), d::OutOfMemory> {
        match unsafe { self.shared.raw.device_wait_idle() } {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(d::OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(d::OutOfMemory::Device),
            _ => unreachable!(),
        }
    }

    unsafe fn set_image_name(&self, image: &mut n::Image, name: &str) {
        self.set_object_name(vk::ObjectType::IMAGE, image.raw.as_raw(), name)
    }

    unsafe fn set_buffer_name(&self, buffer: &mut n::Buffer, name: &str) {
        self.set_object_name(vk::ObjectType::BUFFER, buffer.raw.as_raw(), name)
    }

    unsafe fn set_command_buffer_name(&self, command_buffer: &mut cmd::CommandBuffer, name: &str) {
        self.set_object_name(
            vk::ObjectType::COMMAND_BUFFER,
            command_buffer.raw.as_raw(),
            name,
        )
    }

    unsafe fn set_semaphore_name(&self, semaphore: &mut n::Semaphore, name: &str) {
        self.set_object_name(vk::ObjectType::SEMAPHORE, semaphore.0.as_raw(), name)
    }

    unsafe fn set_fence_name(&self, fence: &mut n::Fence, name: &str) {
        self.set_object_name(vk::ObjectType::FENCE, fence.0.as_raw(), name)
    }

    unsafe fn set_framebuffer_name(&self, framebuffer: &mut n::Framebuffer, name: &str) {
        self.set_object_name(vk::ObjectType::FRAMEBUFFER, framebuffer.raw.as_raw(), name)
    }

    unsafe fn set_render_pass_name(&self, render_pass: &mut n::RenderPass, name: &str) {
        self.set_object_name(vk::ObjectType::RENDER_PASS, render_pass.raw.as_raw(), name)
    }

    unsafe fn set_descriptor_set_name(&self, descriptor_set: &mut n::DescriptorSet, name: &str) {
        self.set_object_name(
            vk::ObjectType::DESCRIPTOR_SET,
            descriptor_set.raw.as_raw(),
            name,
        )
    }

    unsafe fn set_descriptor_set_layout_name(
        &self,
        descriptor_set_layout: &mut n::DescriptorSetLayout,
        name: &str,
    ) {
        self.set_object_name(
            vk::ObjectType::DESCRIPTOR_SET_LAYOUT,
            descriptor_set_layout.raw.as_raw(),
            name,
        )
    }

    unsafe fn set_pipeline_layout_name(&self, pipeline_layout: &mut n::PipelineLayout, name: &str) {
        self.set_object_name(
            vk::ObjectType::PIPELINE_LAYOUT,
            pipeline_layout.raw.as_raw(),
            name,
        )
    }

    unsafe fn set_compute_pipeline_name(
        &self,
        compute_pipeline: &mut n::ComputePipeline,
        name: &str,
    ) {
        self.set_object_name(vk::ObjectType::PIPELINE, compute_pipeline.0.as_raw(), name)
    }

    unsafe fn set_graphics_pipeline_name(
        &self,
        graphics_pipeline: &mut n::GraphicsPipeline,
        name: &str,
    ) {
        self.set_object_name(vk::ObjectType::PIPELINE, graphics_pipeline.0.as_raw(), name)
    }
}

impl Device {
    /// We only work with a subset of Ash-exposed memory types that we know.
    /// This function filters an ash mask into our mask.
    fn filter_memory_requirements(&self, ash_mask: u32) -> u32 {
        let mut hal_index = 0;
        let mut mask = 0;
        for ash_index in 0..32 {
            if self.valid_ash_memory_types & (1 << ash_index) != 0 {
                if ash_mask & (1 << ash_index) != 0 {
                    mask |= 1 << hal_index;
                }
                hal_index += 1;
            }
        }
        mask
    }

    fn get_ash_memory_type_index(&self, hal_type: MemoryTypeId) -> u32 {
        let mut hal_count = hal_type.0;
        for ash_index in 0..32 {
            if self.valid_ash_memory_types & (1 << ash_index) != 0 {
                if hal_count == 0 {
                    return ash_index;
                }
                hal_count -= 1;
            }
        }
        panic!("Unable to get Ash memory type for {:?}", hal_type);
    }

    unsafe fn set_object_name(&self, object_type: vk::ObjectType, object_handle: u64, name: &str) {
        let instance = &self.shared.instance;
        if let Some(DebugMessenger::Utils(ref debug_utils_ext, _)) = instance.debug_messenger {
            // Keep variables outside the if-else block to ensure they do not
            // go out of scope while we hold a pointer to them
            let mut buffer: [u8; 64] = [0u8; 64];
            let buffer_vec: Vec<u8>;

            // Append a null terminator to the string
            let name_cstr = if name.len() < 64 {
                // Common case, string is very small. Allocate a copy on the stack.
                std::ptr::copy_nonoverlapping(name.as_ptr(), buffer.as_mut_ptr(), name.len());
                // Add null terminator
                buffer[name.len()] = 0;
                CStr::from_bytes_with_nul(&buffer[..name.len() + 1]).unwrap()
            } else {
                // Less common case, the string is large.
                // This requires a heap allocation.
                buffer_vec = name
                    .as_bytes()
                    .iter()
                    .cloned()
                    .chain(std::iter::once(0))
                    .collect::<Vec<u8>>();
                CStr::from_bytes_with_nul(&buffer_vec).unwrap()
            };
            let _result = debug_utils_ext.debug_utils_set_object_name(
                self.shared.raw.handle(),
                &vk::DebugUtilsObjectNameInfoEXT::builder()
                    .object_type(object_type)
                    .object_handle(object_handle)
                    .object_name(name_cstr),
            );
        }
    }

    pub(crate) unsafe fn create_swapchain(
        &self,
        surface: &mut w::Surface,
        config: SwapchainConfig,
        provided_old_swapchain: Option<w::Swapchain>,
    ) -> Result<(w::Swapchain, Vec<n::Image>), hal::window::CreationError> {
        let functor = khr::Swapchain::new(&surface.raw.instance.inner, &self.shared.raw);

        let old_swapchain = match provided_old_swapchain {
            Some(osc) => osc.raw,
            None => vk::SwapchainKHR::null(),
        };

        let info = vk::SwapchainCreateInfoKHR::builder()
            .flags(vk::SwapchainCreateFlagsKHR::empty())
            .surface(surface.raw.handle)
            .min_image_count(config.image_count)
            .image_format(conv::map_format(config.format))
            .image_color_space(vk::ColorSpaceKHR::SRGB_NONLINEAR)
            .image_extent(vk::Extent2D {
                width: config.extent.width,
                height: config.extent.height,
            })
            .image_array_layers(1)
            .image_usage(conv::map_image_usage(config.image_usage))
            .image_sharing_mode(vk::SharingMode::EXCLUSIVE)
            .pre_transform(vk::SurfaceTransformFlagsKHR::IDENTITY)
            .composite_alpha(conv::map_composite_alpha_mode(config.composite_alpha_mode))
            .present_mode(conv::map_present_mode(config.present_mode))
            .clipped(true)
            .old_swapchain(old_swapchain);

        let result = functor.create_swapchain(&info, None);

        if old_swapchain != vk::SwapchainKHR::null() {
            functor.destroy_swapchain(old_swapchain, None)
        }

        let swapchain_raw = match result {
            Ok(swapchain_raw) => swapchain_raw,
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => {
                return Err(d::OutOfMemory::Host.into());
            }
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => {
                return Err(d::OutOfMemory::Device.into());
            }
            Err(vk::Result::ERROR_DEVICE_LOST) => return Err(d::DeviceLost.into()),
            Err(vk::Result::ERROR_SURFACE_LOST_KHR) => return Err(d::SurfaceLost.into()),
            Err(vk::Result::ERROR_NATIVE_WINDOW_IN_USE_KHR) => return Err(d::WindowInUse.into()),
            _ => unreachable!("Unexpected result - driver bug? {:?}", result),
        };

        let result = functor.get_swapchain_images(swapchain_raw);

        let backbuffer_images = match result {
            Ok(backbuffer_images) => backbuffer_images,
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => {
                return Err(d::OutOfMemory::Host.into());
            }
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => {
                return Err(d::OutOfMemory::Device.into());
            }
            _ => unreachable!(),
        };

        let extent = vk::Extent3D {
            width: config.extent.width,
            height: config.extent.height,
            depth: 1,
        };
        let swapchain = w::Swapchain {
            raw: swapchain_raw,
            functor,
            vendor_id: self.vendor_id,
            extent,
        };

        let images = backbuffer_images
            .into_iter()
            .map(|image| n::Image {
                raw: image,
                ty: vk::ImageType::TYPE_2D,
                flags: vk::ImageCreateFlags::empty(),
                extent,
            })
            .collect();

        Ok((swapchain, images))
    }
}

#[test]
fn test_send_sync() {
    fn foo<T: Send + Sync>() {}
    foo::<Device>()
}
