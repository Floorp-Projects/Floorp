use std::{collections::hash_map::Entry, ffi, iter, mem, ops::Range, ptr, slice, sync::Arc};

use range_alloc::RangeAllocator;
use smallvec::SmallVec;
use spirv_cross::{hlsl, spirv, ErrorCode as SpirvErrorCode};

use winapi::{
    shared::{
        dxgi, dxgi1_2, dxgi1_4, dxgiformat, dxgitype,
        minwindef::{FALSE, TRUE, UINT},
        windef, winerror,
    },
    um::{d3d12, d3dcompiler, synchapi, winbase, winnt},
    Interface,
};

use auxil::{spirv_cross_specialize_ast, ShaderStage};
use hal::{
    buffer, device as d, format, format::Aspects, image, memory, memory::Requirements, pass,
    pool::CommandPoolCreateFlags, pso, pso::VertexInputRate, query, queue::QueueFamilyId,
    window as w,
};

use crate::{
    command as cmd, conv, descriptors_cpu, pool::CommandPool, resource as r, root_constants,
    root_constants::RootConstant, window::Swapchain, Backend as B, Device, MemoryGroup,
    MAX_VERTEX_BUFFERS, NUM_HEAP_PROPERTIES, QUEUE_FAMILIES,
};
use native::{PipelineStateSubobject, Subobject};

// Register space used for root constants.
const ROOT_CONSTANT_SPACE: u32 = 0;

const MEM_TYPE_MASK: u32 = 0x7;
const MEM_TYPE_SHIFT: u32 = 3;

const MEM_TYPE_UNIVERSAL_SHIFT: u32 = MEM_TYPE_SHIFT * MemoryGroup::Universal as u32;
const MEM_TYPE_BUFFER_SHIFT: u32 = MEM_TYPE_SHIFT * MemoryGroup::BufferOnly as u32;
const MEM_TYPE_IMAGE_SHIFT: u32 = MEM_TYPE_SHIFT * MemoryGroup::ImageOnly as u32;
const MEM_TYPE_TARGET_SHIFT: u32 = MEM_TYPE_SHIFT * MemoryGroup::TargetOnly as u32;

pub const IDENTITY_MAPPING: UINT = 0x1688; // D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING

fn wide_cstr(name: &str) -> Vec<u16> {
    name.encode_utf16().chain(iter::once(0)).collect()
}

/// Emit error during shader module creation. Used if we don't expect an error
/// but might panic due to an exception in SPIRV-Cross.
fn gen_unexpected_error(stage: ShaderStage, err: SpirvErrorCode) -> pso::CreationError {
    let msg = match err {
        SpirvErrorCode::CompilationError(msg) => msg,
        SpirvErrorCode::Unhandled => "Unexpected error".into(),
    };
    let error = format!("SPIR-V unexpected error {:?}", msg);
    pso::CreationError::ShaderCreationError(stage.to_flag(), error)
}

/// Emit error during shader module creation. Used if we execute an query command.
fn gen_query_error(stage: ShaderStage, err: SpirvErrorCode) -> pso::CreationError {
    let msg = match err {
        SpirvErrorCode::CompilationError(msg) => msg,
        SpirvErrorCode::Unhandled => "Unknown query error".into(),
    };
    let error = format!("SPIR-V query error {:?}", msg);
    pso::CreationError::ShaderCreationError(stage.to_flag(), error)
}

#[derive(Clone, Debug)]
pub(crate) struct ViewInfo {
    pub(crate) resource: native::Resource,
    pub(crate) kind: image::Kind,
    pub(crate) caps: image::ViewCapabilities,
    pub(crate) view_kind: image::ViewKind,
    pub(crate) format: dxgiformat::DXGI_FORMAT,
    pub(crate) component_mapping: UINT,
    pub(crate) levels: Range<image::Level>,
    pub(crate) layers: Range<image::Layer>,
}

pub(crate) enum CommandSignature {
    Draw,
    DrawIndexed,
    Dispatch,
}

/// Compile a single shader entry point from a HLSL text shader
pub(crate) fn compile_shader(
    stage: ShaderStage,
    shader_model: hlsl::ShaderModel,
    features: &hal::Features,
    entry: &str,
    code: &[u8],
) -> Result<native::Blob, pso::CreationError> {
    let stage_str = match stage {
        ShaderStage::Vertex => "vs",
        ShaderStage::Fragment => "ps",
        ShaderStage::Compute => "cs",
        _ => unimplemented!(),
    };
    let model_str = match shader_model {
        hlsl::ShaderModel::V5_0 => "5_0",
        hlsl::ShaderModel::V5_1 => "5_1",
        hlsl::ShaderModel::V6_0 => "6_0",
        _ => unimplemented!(),
    };
    let full_stage = format!("{}_{}\0", stage_str, model_str);

    let mut shader_data = native::Blob::null();
    let mut error = native::Blob::null();
    let entry = ffi::CString::new(entry).unwrap();
    let mut compile_flags = d3dcompiler::D3DCOMPILE_ENABLE_STRICTNESS;
    if cfg!(debug_assertions) {
        compile_flags |= d3dcompiler::D3DCOMPILE_DEBUG;
    }
    if features.contains(hal::Features::UNSIZED_DESCRIPTOR_ARRAY) {
        compile_flags |= d3dcompiler::D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
    }
    let hr = unsafe {
        d3dcompiler::D3DCompile(
            code.as_ptr() as *const _,
            code.len(),
            ptr::null(),
            ptr::null(),
            ptr::null_mut(),
            entry.as_ptr() as *const _,
            full_stage.as_ptr() as *const i8,
            compile_flags,
            0,
            shader_data.mut_void() as *mut *mut _,
            error.mut_void() as *mut *mut _,
        )
    };
    if !winerror::SUCCEEDED(hr) {
        let message = unsafe {
            let pointer = error.GetBufferPointer();
            let size = error.GetBufferSize();
            let slice = slice::from_raw_parts(pointer as *const u8, size as usize);
            String::from_utf8_lossy(slice).into_owned()
        };
        unsafe {
            error.destroy();
        }
        let error = format!("D3DCompile error {:x}: {}", hr, message);
        Err(pso::CreationError::ShaderCreationError(
            stage.to_flag(),
            error,
        ))
    } else {
        Ok(shader_data)
    }
}

#[repr(C)]
struct GraphicsPipelineStateSubobjectStream {
    root_signature: PipelineStateSubobject<*mut d3d12::ID3D12RootSignature>,
    vs: PipelineStateSubobject<d3d12::D3D12_SHADER_BYTECODE>,
    ps: PipelineStateSubobject<d3d12::D3D12_SHADER_BYTECODE>,
    ds: PipelineStateSubobject<d3d12::D3D12_SHADER_BYTECODE>,
    hs: PipelineStateSubobject<d3d12::D3D12_SHADER_BYTECODE>,
    gs: PipelineStateSubobject<d3d12::D3D12_SHADER_BYTECODE>,
    stream_output: PipelineStateSubobject<d3d12::D3D12_STREAM_OUTPUT_DESC>,
    blend: PipelineStateSubobject<d3d12::D3D12_BLEND_DESC>,
    sample_mask: PipelineStateSubobject<UINT>,
    rasterizer: PipelineStateSubobject<d3d12::D3D12_RASTERIZER_DESC>,
    depth_stencil: PipelineStateSubobject<d3d12::D3D12_DEPTH_STENCIL_DESC1>,
    input_layout: PipelineStateSubobject<d3d12::D3D12_INPUT_LAYOUT_DESC>,
    ib_strip_cut_value: PipelineStateSubobject<d3d12::D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>,
    primitive_topology: PipelineStateSubobject<d3d12::D3D12_PRIMITIVE_TOPOLOGY_TYPE>,
    render_target_formats: PipelineStateSubobject<d3d12::D3D12_RT_FORMAT_ARRAY>,
    depth_stencil_format: PipelineStateSubobject<dxgiformat::DXGI_FORMAT>,
    sample_desc: PipelineStateSubobject<dxgitype::DXGI_SAMPLE_DESC>,
    node_mask: PipelineStateSubobject<UINT>,
    cached_pso: PipelineStateSubobject<d3d12::D3D12_CACHED_PIPELINE_STATE>,
    flags: PipelineStateSubobject<d3d12::D3D12_PIPELINE_STATE_FLAGS>,
}

impl GraphicsPipelineStateSubobjectStream {
    fn new(
        pso_desc: &d3d12::D3D12_GRAPHICS_PIPELINE_STATE_DESC,
        depth_bounds_test_enable: bool,
    ) -> Self {
        GraphicsPipelineStateSubobjectStream {
            root_signature: PipelineStateSubobject::new(
                Subobject::RootSignature,
                pso_desc.pRootSignature,
            ),
            vs: PipelineStateSubobject::new(Subobject::VS, pso_desc.VS),
            ps: PipelineStateSubobject::new(Subobject::PS, pso_desc.PS),
            ds: PipelineStateSubobject::new(Subobject::DS, pso_desc.DS),
            hs: PipelineStateSubobject::new(Subobject::HS, pso_desc.HS),
            gs: PipelineStateSubobject::new(Subobject::GS, pso_desc.GS),
            stream_output: PipelineStateSubobject::new(
                Subobject::StreamOutput,
                pso_desc.StreamOutput,
            ),
            blend: PipelineStateSubobject::new(Subobject::Blend, pso_desc.BlendState),
            sample_mask: PipelineStateSubobject::new(Subobject::SampleMask, pso_desc.SampleMask),
            rasterizer: PipelineStateSubobject::new(
                Subobject::Rasterizer,
                pso_desc.RasterizerState,
            ),
            depth_stencil: PipelineStateSubobject::new(
                Subobject::DepthStencil1,
                d3d12::D3D12_DEPTH_STENCIL_DESC1 {
                    DepthEnable: pso_desc.DepthStencilState.DepthEnable,
                    DepthWriteMask: pso_desc.DepthStencilState.DepthWriteMask,
                    DepthFunc: pso_desc.DepthStencilState.DepthFunc,
                    StencilEnable: pso_desc.DepthStencilState.StencilEnable,
                    StencilReadMask: pso_desc.DepthStencilState.StencilReadMask,
                    StencilWriteMask: pso_desc.DepthStencilState.StencilWriteMask,
                    FrontFace: pso_desc.DepthStencilState.FrontFace,
                    BackFace: pso_desc.DepthStencilState.BackFace,
                    DepthBoundsTestEnable: depth_bounds_test_enable as _,
                },
            ),
            input_layout: PipelineStateSubobject::new(Subobject::InputLayout, pso_desc.InputLayout),
            ib_strip_cut_value: PipelineStateSubobject::new(
                Subobject::IBStripCut,
                pso_desc.IBStripCutValue,
            ),
            primitive_topology: PipelineStateSubobject::new(
                Subobject::PrimitiveTopology,
                pso_desc.PrimitiveTopologyType,
            ),
            render_target_formats: PipelineStateSubobject::new(
                Subobject::RTFormats,
                d3d12::D3D12_RT_FORMAT_ARRAY {
                    RTFormats: pso_desc.RTVFormats,
                    NumRenderTargets: pso_desc.NumRenderTargets,
                },
            ),
            depth_stencil_format: PipelineStateSubobject::new(
                Subobject::DSFormat,
                pso_desc.DSVFormat,
            ),
            sample_desc: PipelineStateSubobject::new(Subobject::SampleDesc, pso_desc.SampleDesc),
            node_mask: PipelineStateSubobject::new(Subobject::NodeMask, pso_desc.NodeMask),
            cached_pso: PipelineStateSubobject::new(Subobject::CachedPSO, pso_desc.CachedPSO),
            flags: PipelineStateSubobject::new(Subobject::Flags, pso_desc.Flags),
        }
    }
}

impl Device {
    fn parse_spirv(
        stage: ShaderStage,
        raw_data: &[u32],
    ) -> Result<spirv::Ast<hlsl::Target>, pso::CreationError> {
        let module = spirv::Module::from_words(raw_data);

        spirv::Ast::parse(&module).map_err(|err| {
            let msg = match err {
                SpirvErrorCode::CompilationError(msg) => msg,
                SpirvErrorCode::Unhandled => "Unknown parsing error".into(),
            };
            let error = format!("SPIR-V parsing failed: {:?}", msg);
            pso::CreationError::ShaderCreationError(stage.to_flag(), error)
        })
    }

    /// Introspects the input attributes of given SPIR-V shader and returns an optional vertex semantic remapping.
    ///
    /// The returned hashmap has attribute location as a key and an Optional remapping to a two part semantic.
    ///
    /// eg.
    /// `2 -> None` means use default semantic `TEXCOORD2`
    /// `2 -> Some((0, 2))` means use two part semantic `TEXCOORD0_2`. This is how matrices are represented by spirv-cross.
    ///
    /// This is a temporary workaround for https://github.com/KhronosGroup/SPIRV-Cross/issues/1512.
    ///
    /// This workaround also exists under the same name in the DX11 backend.
    pub(crate) fn introspect_spirv_vertex_semantic_remapping(
        raw_data: &[u32],
    ) -> Result<auxil::FastHashMap<u32, Option<(u32, u32)>>, pso::CreationError> {
        const SHADER_STAGE: ShaderStage = ShaderStage::Vertex;
        // This is inefficient as we already parse it once before. This is a temporary workaround only called
        // on vertex shaders. If this becomes permanent or shows up in profiles, deduplicate these as first course of action.
        let ast = Self::parse_spirv(SHADER_STAGE, raw_data)?;

        let mut map = auxil::FastHashMap::default();

        let inputs = ast
            .get_shader_resources()
            .map_err(|err| gen_query_error(SHADER_STAGE, err))?
            .stage_inputs;
        for input in inputs {
            let idx = ast
                .get_decoration(input.id, spirv::Decoration::Location)
                .map_err(|err| gen_query_error(SHADER_STAGE, err))?;

            let ty = ast
                .get_type(input.type_id)
                .map_err(|err| gen_query_error(SHADER_STAGE, err))?;

            match ty {
                spirv::Type::Boolean { columns, .. }
                | spirv::Type::Int { columns, .. }
                | spirv::Type::UInt { columns, .. }
                | spirv::Type::Half { columns, .. }
                | spirv::Type::Float { columns, .. }
                | spirv::Type::Double { columns, .. }
                    if columns > 1 =>
                {
                    for col in 0..columns {
                        if let Some(_) = map.insert(idx + col, Some((idx, col))) {
                            let error = format!(
                                "Shader has overlapping input attachments at location {}",
                                idx
                            );
                            return Err(pso::CreationError::ShaderCreationError(
                                SHADER_STAGE.to_flag(),
                                error,
                            ));
                        }
                    }
                }
                _ => {
                    if let Some(_) = map.insert(idx, None) {
                        let error = format!(
                            "Shader has overlapping input attachments at location {}",
                            idx
                        );
                        return Err(pso::CreationError::ShaderCreationError(
                            SHADER_STAGE.to_flag(),
                            error,
                        ));
                    }
                }
            }
        }

        Ok(map)
    }

    fn patch_spirv_resources(
        stage: ShaderStage,
        ast: &mut spirv::Ast<hlsl::Target>,
        layout: &r::PipelineLayout,
    ) -> Result<(), pso::CreationError> {
        // Move the descriptor sets away to yield for the root constants at "space0".
        let space_offset = if layout.shared.constants.is_empty() {
            0
        } else {
            1
        };
        let shader_resources = ast
            .get_shader_resources()
            .map_err(|err| gen_query_error(stage, err))?;

        if space_offset != 0 {
            for image in &shader_resources.separate_images {
                let set = ast
                    .get_decoration(image.id, spirv::Decoration::DescriptorSet)
                    .map_err(|err| gen_query_error(stage, err))?;
                ast.set_decoration(
                    image.id,
                    spirv::Decoration::DescriptorSet,
                    space_offset + set,
                )
                .map_err(|err| gen_unexpected_error(stage, err))?;
            }
        }

        if space_offset != 0 {
            for uniform_buffer in &shader_resources.uniform_buffers {
                let set = ast
                    .get_decoration(uniform_buffer.id, spirv::Decoration::DescriptorSet)
                    .map_err(|err| gen_query_error(stage, err))?;
                ast.set_decoration(
                    uniform_buffer.id,
                    spirv::Decoration::DescriptorSet,
                    space_offset + set,
                )
                .map_err(|err| gen_unexpected_error(stage, err))?;
            }
        }

        for storage_buffer in &shader_resources.storage_buffers {
            let set = ast
                .get_decoration(storage_buffer.id, spirv::Decoration::DescriptorSet)
                .map_err(|err| gen_query_error(stage, err))?;
            let binding = ast
                .get_decoration(storage_buffer.id, spirv::Decoration::Binding)
                .map_err(|err| gen_query_error(stage, err))?;
            if space_offset != 0 {
                ast.set_decoration(
                    storage_buffer.id,
                    spirv::Decoration::DescriptorSet,
                    space_offset + set,
                )
                .map_err(|err| gen_unexpected_error(stage, err))?;
            }
            if !layout.elements[set as usize]
                .mutable_bindings
                .contains(&binding)
            {
                ast.set_decoration(storage_buffer.id, spirv::Decoration::NonWritable, 1)
                    .map_err(|err| gen_unexpected_error(stage, err))?
            }
        }

        for image in &shader_resources.storage_images {
            let set = ast
                .get_decoration(image.id, spirv::Decoration::DescriptorSet)
                .map_err(|err| gen_query_error(stage, err))?;
            let binding = ast
                .get_decoration(image.id, spirv::Decoration::Binding)
                .map_err(|err| gen_query_error(stage, err))?;
            if space_offset != 0 {
                ast.set_decoration(
                    image.id,
                    spirv::Decoration::DescriptorSet,
                    space_offset + set,
                )
                .map_err(|err| gen_unexpected_error(stage, err))?;
            }
            if !layout.elements[set as usize]
                .mutable_bindings
                .contains(&binding)
            {
                ast.set_decoration(image.id, spirv::Decoration::NonWritable, 1)
                    .map_err(|err| gen_unexpected_error(stage, err))?
            }
        }

        if space_offset != 0 {
            for sampler in &shader_resources.separate_samplers {
                let set = ast
                    .get_decoration(sampler.id, spirv::Decoration::DescriptorSet)
                    .map_err(|err| gen_query_error(stage, err))?;
                ast.set_decoration(
                    sampler.id,
                    spirv::Decoration::DescriptorSet,
                    space_offset + set,
                )
                .map_err(|err| gen_unexpected_error(stage, err))?;
            }
        }

        if space_offset != 0 {
            for image in &shader_resources.sampled_images {
                let set = ast
                    .get_decoration(image.id, spirv::Decoration::DescriptorSet)
                    .map_err(|err| gen_query_error(stage, err))?;
                ast.set_decoration(
                    image.id,
                    spirv::Decoration::DescriptorSet,
                    space_offset + set,
                )
                .map_err(|err| gen_unexpected_error(stage, err))?;
            }
        }

        if space_offset != 0 {
            for input in &shader_resources.subpass_inputs {
                let set = ast
                    .get_decoration(input.id, spirv::Decoration::DescriptorSet)
                    .map_err(|err| gen_query_error(stage, err))?;
                ast.set_decoration(
                    input.id,
                    spirv::Decoration::DescriptorSet,
                    space_offset + set,
                )
                .map_err(|err| gen_unexpected_error(stage, err))?;
            }
        }

        Ok(())
    }

    fn translate_spirv(
        ast: &mut spirv::Ast<hlsl::Target>,
        shader_model: hlsl::ShaderModel,
        layout: &r::PipelineLayout,
        stage: ShaderStage,
        features: &hal::Features,
        entry_point: &str,
    ) -> Result<String, pso::CreationError> {
        let mut compile_options = hlsl::CompilerOptions::default();
        compile_options.shader_model = shader_model;
        compile_options.vertex.invert_y = !features.contains(hal::Features::NDC_Y_UP);
        compile_options.force_zero_initialized_variables = true;
        compile_options.nonwritable_uav_texture_as_srv = true;
        // these are technically incorrect, but better than panicking
        compile_options.point_size_compat = true;
        compile_options.point_coord_compat = true;
        compile_options.entry_point = Some((entry_point.to_string(), conv::map_stage(stage)));

        let stage_flag = stage.to_flag();
        let root_constant_layout = layout
            .shared
            .constants
            .iter()
            .filter_map(|constant| {
                if constant.stages.contains(stage_flag) {
                    Some(hlsl::RootConstant {
                        start: constant.range.start * 4,
                        end: constant.range.end * 4,
                        binding: constant.range.start,
                        space: 0,
                    })
                } else {
                    None
                }
            })
            .collect();
        ast.set_compiler_options(&compile_options)
            .map_err(|err| gen_unexpected_error(stage, err))?;
        ast.set_root_constant_layout(root_constant_layout)
            .map_err(|err| gen_unexpected_error(stage, err))?;
        ast.compile().map_err(|err| {
            let msg = match err {
                SpirvErrorCode::CompilationError(msg) => msg,
                SpirvErrorCode::Unhandled => "Unknown compile error".into(),
            };
            let error = format!("SPIR-V compile failed: {}", msg);
            pso::CreationError::ShaderCreationError(stage.to_flag(), error)
        })
    }

    // Extract entry point from shader module on pipeline creation.
    // Returns compiled shader blob and bool to indicate if the shader should be
    // destroyed after pipeline creation
    fn extract_entry_point(
        stage: ShaderStage,
        source: &pso::EntryPoint<B>,
        layout: &r::PipelineLayout,
        features: &hal::Features,
    ) -> Result<(native::Blob, bool), pso::CreationError> {
        match *source.module {
            r::ShaderModule::Compiled(ref shaders) => {
                // TODO: do we need to check for specialization constants?
                // Use precompiled shader, ignore specialization or layout.
                shaders
                    .get(source.entry)
                    .map(|src| (*src, false))
                    .ok_or(pso::CreationError::MissingEntryPoint(source.entry.into()))
            }
            r::ShaderModule::Spirv(ref raw_data) => {
                let mut ast = Self::parse_spirv(stage, raw_data)?;
                spirv_cross_specialize_ast(&mut ast, &source.specialization)
                    .map_err(pso::CreationError::InvalidSpecialization)?;
                Self::patch_spirv_resources(stage, &mut ast, layout)?;

                let execution_model = conv::map_stage(stage);
                let shader_model = hlsl::ShaderModel::V5_1;
                let shader_code = Self::translate_spirv(
                    &mut ast,
                    shader_model,
                    layout,
                    stage,
                    features,
                    source.entry,
                )?;
                debug!("SPIRV-Cross generated shader:\n{}", shader_code);

                let real_name = ast
                    .get_cleansed_entry_point_name(source.entry, execution_model)
                    .map_err(|err| gen_query_error(stage, err))?;

                let shader = compile_shader(
                    stage,
                    shader_model,
                    features,
                    &real_name,
                    shader_code.as_bytes(),
                )?;
                Ok((shader, true))
            }
        }
    }

    pub(crate) fn create_command_signature(
        device: native::Device,
        ty: CommandSignature,
    ) -> native::CommandSignature {
        let (arg, stride) = match ty {
            CommandSignature::Draw => (native::IndirectArgument::draw(), 16),
            CommandSignature::DrawIndexed => (native::IndirectArgument::draw_indexed(), 20),
            CommandSignature::Dispatch => (native::IndirectArgument::dispatch(), 12),
        };

        let (signature, hr) =
            device.create_command_signature(native::RootSignature::null(), &[arg], stride, 0);

        if !winerror::SUCCEEDED(hr) {
            error!("error on command signature creation: {:x}", hr);
        }
        signature
    }

    pub(crate) fn create_descriptor_heap_impl(
        device: native::Device,
        heap_type: native::DescriptorHeapType,
        shader_visible: bool,
        capacity: usize,
    ) -> r::DescriptorHeap {
        assert_ne!(capacity, 0);

        let (heap, _hr) = device.create_descriptor_heap(
            capacity as _,
            heap_type,
            if shader_visible {
                native::DescriptorHeapFlags::SHADER_VISIBLE
            } else {
                native::DescriptorHeapFlags::empty()
            },
            0,
        );

        let descriptor_size = device.get_descriptor_increment_size(heap_type);
        let cpu_handle = heap.start_cpu_descriptor();
        let gpu_handle = heap.start_gpu_descriptor();

        r::DescriptorHeap {
            raw: heap,
            handle_size: descriptor_size as _,
            total_handles: capacity as _,
            start: r::DualHandle {
                cpu: cpu_handle,
                gpu: gpu_handle,
                size: 0,
            },
        }
    }

    pub(crate) fn view_image_as_render_target_impl(
        device: native::Device,
        handle: d3d12::D3D12_CPU_DESCRIPTOR_HANDLE,
        info: &ViewInfo,
    ) -> Result<(), image::ViewCreationError> {
        #![allow(non_snake_case)]

        let mut desc = d3d12::D3D12_RENDER_TARGET_VIEW_DESC {
            Format: info.format,
            ViewDimension: 0,
            u: unsafe { mem::zeroed() },
        };

        let MipSlice = info.levels.start as _;
        let FirstArraySlice = info.layers.start as _;
        let ArraySize = (info.layers.end - info.layers.start) as _;
        let is_msaa = info.kind.num_samples() > 1;
        if info.levels.start + 1 != info.levels.end {
            return Err(image::ViewCreationError::Level(info.levels.start));
        }
        if info.layers.end > info.kind.num_layers() {
            return Err(image::ViewCreationError::Layer(
                image::LayerError::OutOfBounds,
            ));
        }

        match info.view_kind {
            image::ViewKind::D1 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE1D;
                *unsafe { desc.u.Texture1D_mut() } = d3d12::D3D12_TEX1D_RTV { MipSlice }
            }
            image::ViewKind::D1Array => {
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                *unsafe { desc.u.Texture1DArray_mut() } = d3d12::D3D12_TEX1D_ARRAY_RTV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2 if is_msaa => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE2DMS;
                *unsafe { desc.u.Texture2DMS_mut() } = d3d12::D3D12_TEX2DMS_RTV {
                    UnusedField_NothingToDefine: 0,
                }
            }
            image::ViewKind::D2 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE2D;
                *unsafe { desc.u.Texture2D_mut() } = d3d12::D3D12_TEX2D_RTV {
                    MipSlice,
                    PlaneSlice: 0, //TODO
                }
            }
            image::ViewKind::D2Array if is_msaa => {
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                *unsafe { desc.u.Texture2DMSArray_mut() } = d3d12::D3D12_TEX2DMS_ARRAY_RTV {
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2Array => {
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                *unsafe { desc.u.Texture2DArray_mut() } = d3d12::D3D12_TEX2D_ARRAY_RTV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                    PlaneSlice: 0, //TODO
                }
            }
            image::ViewKind::D3 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE3D;
                *unsafe { desc.u.Texture3D_mut() } = d3d12::D3D12_TEX3D_RTV {
                    MipSlice,
                    FirstWSlice: 0,
                    WSize: info.kind.extent().depth as _,
                }
            }
            image::ViewKind::Cube | image::ViewKind::CubeArray => {
                desc.ViewDimension = d3d12::D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                //TODO: double-check if any *6 are needed
                *unsafe { desc.u.Texture2DArray_mut() } = d3d12::D3D12_TEX2D_ARRAY_RTV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                    PlaneSlice: 0, //TODO
                }
            }
        };

        unsafe {
            device.CreateRenderTargetView(info.resource.as_mut_ptr(), &desc, handle);
        }

        Ok(())
    }

    pub(crate) fn view_image_as_render_target(
        &self,
        info: &ViewInfo,
    ) -> Result<descriptors_cpu::Handle, image::ViewCreationError> {
        let handle = self.rtv_pool.lock().alloc_handle();
        Self::view_image_as_render_target_impl(self.raw, handle.raw, info).map(|_| handle)
    }

    pub(crate) fn view_image_as_depth_stencil_impl(
        device: native::Device,
        handle: d3d12::D3D12_CPU_DESCRIPTOR_HANDLE,
        info: &ViewInfo,
    ) -> Result<(), image::ViewCreationError> {
        #![allow(non_snake_case)]

        let mut desc = d3d12::D3D12_DEPTH_STENCIL_VIEW_DESC {
            Format: info.format,
            ViewDimension: 0,
            Flags: 0,
            u: unsafe { mem::zeroed() },
        };

        let MipSlice = info.levels.start as _;
        let FirstArraySlice = info.layers.start as _;
        let ArraySize = (info.layers.end - info.layers.start) as _;
        let is_msaa = info.kind.num_samples() > 1;
        if info.levels.start + 1 != info.levels.end {
            return Err(image::ViewCreationError::Level(info.levels.start));
        }
        if info.layers.end > info.kind.num_layers() {
            return Err(image::ViewCreationError::Layer(
                image::LayerError::OutOfBounds,
            ));
        }

        match info.view_kind {
            image::ViewKind::D1 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_DSV_DIMENSION_TEXTURE1D;
                *unsafe { desc.u.Texture1D_mut() } = d3d12::D3D12_TEX1D_DSV { MipSlice }
            }
            image::ViewKind::D1Array => {
                desc.ViewDimension = d3d12::D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                *unsafe { desc.u.Texture1DArray_mut() } = d3d12::D3D12_TEX1D_ARRAY_DSV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2 if is_msaa => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_DSV_DIMENSION_TEXTURE2DMS;
                *unsafe { desc.u.Texture2DMS_mut() } = d3d12::D3D12_TEX2DMS_DSV {
                    UnusedField_NothingToDefine: 0,
                }
            }
            image::ViewKind::D2 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_DSV_DIMENSION_TEXTURE2D;
                *unsafe { desc.u.Texture2D_mut() } = d3d12::D3D12_TEX2D_DSV { MipSlice }
            }
            image::ViewKind::D2Array if is_msaa => {
                desc.ViewDimension = d3d12::D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                *unsafe { desc.u.Texture2DMSArray_mut() } = d3d12::D3D12_TEX2DMS_ARRAY_DSV {
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2Array => {
                desc.ViewDimension = d3d12::D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                *unsafe { desc.u.Texture2DArray_mut() } = d3d12::D3D12_TEX2D_ARRAY_DSV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D3 | image::ViewKind::Cube | image::ViewKind::CubeArray => {
                warn!(
                    "3D and cube views are not supported for the image, kind: {:?}",
                    info.kind
                );
                return Err(image::ViewCreationError::BadKind(info.view_kind));
            }
        };

        unsafe {
            device.CreateDepthStencilView(info.resource.as_mut_ptr(), &desc, handle);
        }

        Ok(())
    }

    pub(crate) fn view_image_as_depth_stencil(
        &self,
        info: &ViewInfo,
    ) -> Result<descriptors_cpu::Handle, image::ViewCreationError> {
        let handle = self.dsv_pool.lock().alloc_handle();
        Self::view_image_as_depth_stencil_impl(self.raw, handle.raw, info).map(|_| handle)
    }

    pub(crate) fn build_image_as_shader_resource_desc(
        info: &ViewInfo,
    ) -> Result<d3d12::D3D12_SHADER_RESOURCE_VIEW_DESC, image::ViewCreationError> {
        #![allow(non_snake_case)]

        let mut desc = d3d12::D3D12_SHADER_RESOURCE_VIEW_DESC {
            Format: info.format,
            ViewDimension: 0,
            Shader4ComponentMapping: info.component_mapping,
            u: unsafe { mem::zeroed() },
        };

        let MostDetailedMip = info.levels.start as _;
        let MipLevels = (info.levels.end - info.levels.start) as _;
        let FirstArraySlice = info.layers.start as _;
        let ArraySize = (info.layers.end - info.layers.start) as _;

        if info.layers.end > info.kind.num_layers() {
            return Err(image::ViewCreationError::Layer(
                image::LayerError::OutOfBounds,
            ));
        }
        let is_msaa = info.kind.num_samples() > 1;
        let is_cube = info.caps.contains(image::ViewCapabilities::KIND_CUBE);

        match info.view_kind {
            image::ViewKind::D1 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURE1D;
                *unsafe { desc.u.Texture1D_mut() } = d3d12::D3D12_TEX1D_SRV {
                    MostDetailedMip,
                    MipLevels,
                    ResourceMinLODClamp: 0.0,
                }
            }
            image::ViewKind::D1Array => {
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                *unsafe { desc.u.Texture1DArray_mut() } = d3d12::D3D12_TEX1D_ARRAY_SRV {
                    MostDetailedMip,
                    MipLevels,
                    FirstArraySlice,
                    ArraySize,
                    ResourceMinLODClamp: 0.0,
                }
            }
            image::ViewKind::D2 if is_msaa => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURE2DMS;
                *unsafe { desc.u.Texture2DMS_mut() } = d3d12::D3D12_TEX2DMS_SRV {
                    UnusedField_NothingToDefine: 0,
                }
            }
            image::ViewKind::D2 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURE2D;
                *unsafe { desc.u.Texture2D_mut() } = d3d12::D3D12_TEX2D_SRV {
                    MostDetailedMip,
                    MipLevels,
                    PlaneSlice: 0, //TODO
                    ResourceMinLODClamp: 0.0,
                }
            }
            image::ViewKind::D2Array if is_msaa => {
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                *unsafe { desc.u.Texture2DMSArray_mut() } = d3d12::D3D12_TEX2DMS_ARRAY_SRV {
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2Array => {
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                *unsafe { desc.u.Texture2DArray_mut() } = d3d12::D3D12_TEX2D_ARRAY_SRV {
                    MostDetailedMip,
                    MipLevels,
                    FirstArraySlice,
                    ArraySize,
                    PlaneSlice: 0, //TODO
                    ResourceMinLODClamp: 0.0,
                }
            }
            image::ViewKind::D3 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURE3D;
                *unsafe { desc.u.Texture3D_mut() } = d3d12::D3D12_TEX3D_SRV {
                    MostDetailedMip,
                    MipLevels,
                    ResourceMinLODClamp: 0.0,
                }
            }
            image::ViewKind::Cube if is_cube => {
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURECUBE;
                *unsafe { desc.u.TextureCube_mut() } = d3d12::D3D12_TEXCUBE_SRV {
                    MostDetailedMip,
                    MipLevels,
                    ResourceMinLODClamp: 0.0,
                }
            }
            image::ViewKind::CubeArray if is_cube => {
                assert_eq!(0, ArraySize % 6);
                desc.ViewDimension = d3d12::D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                *unsafe { desc.u.TextureCubeArray_mut() } = d3d12::D3D12_TEXCUBE_ARRAY_SRV {
                    MostDetailedMip,
                    MipLevels,
                    First2DArrayFace: FirstArraySlice,
                    NumCubes: ArraySize / 6,
                    ResourceMinLODClamp: 0.0,
                }
            }
            image::ViewKind::Cube | image::ViewKind::CubeArray => {
                warn!(
                    "Cube views are not supported for the image, kind: {:?}",
                    info.kind
                );
                return Err(image::ViewCreationError::BadKind(info.view_kind));
            }
        }

        Ok(desc)
    }

    fn view_image_as_shader_resource(
        &self,
        info: &ViewInfo,
    ) -> Result<descriptors_cpu::Handle, image::ViewCreationError> {
        let desc = Self::build_image_as_shader_resource_desc(&info)?;
        let handle = self.srv_uav_pool.lock().alloc_handle();
        unsafe {
            self.raw
                .CreateShaderResourceView(info.resource.as_mut_ptr(), &desc, handle.raw);
        }

        Ok(handle)
    }

    fn view_image_as_storage(
        &self,
        info: &ViewInfo,
    ) -> Result<descriptors_cpu::Handle, image::ViewCreationError> {
        #![allow(non_snake_case)]

        // Cannot make a storage image over multiple mips
        if info.levels.start + 1 != info.levels.end {
            return Err(image::ViewCreationError::Unsupported);
        }

        let mut desc = d3d12::D3D12_UNORDERED_ACCESS_VIEW_DESC {
            Format: info.format,
            ViewDimension: 0,
            u: unsafe { mem::zeroed() },
        };

        let MipSlice = info.levels.start as _;
        let FirstArraySlice = info.layers.start as _;
        let ArraySize = (info.layers.end - info.layers.start) as _;

        if info.layers.end > info.kind.num_layers() {
            return Err(image::ViewCreationError::Layer(
                image::LayerError::OutOfBounds,
            ));
        }
        if info.kind.num_samples() > 1 {
            error!("MSAA images can't be viewed as UAV");
            return Err(image::ViewCreationError::Unsupported);
        }

        match info.view_kind {
            image::ViewKind::D1 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_UAV_DIMENSION_TEXTURE1D;
                *unsafe { desc.u.Texture1D_mut() } = d3d12::D3D12_TEX1D_UAV { MipSlice }
            }
            image::ViewKind::D1Array => {
                desc.ViewDimension = d3d12::D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                *unsafe { desc.u.Texture1DArray_mut() } = d3d12::D3D12_TEX1D_ARRAY_UAV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                }
            }
            image::ViewKind::D2 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_UAV_DIMENSION_TEXTURE2D;
                *unsafe { desc.u.Texture2D_mut() } = d3d12::D3D12_TEX2D_UAV {
                    MipSlice,
                    PlaneSlice: 0, //TODO
                }
            }
            image::ViewKind::D2Array => {
                desc.ViewDimension = d3d12::D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                *unsafe { desc.u.Texture2DArray_mut() } = d3d12::D3D12_TEX2D_ARRAY_UAV {
                    MipSlice,
                    FirstArraySlice,
                    ArraySize,
                    PlaneSlice: 0, //TODO
                }
            }
            image::ViewKind::D3 => {
                assert_eq!(info.layers, 0..1);
                desc.ViewDimension = d3d12::D3D12_UAV_DIMENSION_TEXTURE3D;
                *unsafe { desc.u.Texture3D_mut() } = d3d12::D3D12_TEX3D_UAV {
                    MipSlice,
                    FirstWSlice: 0,
                    WSize: info.kind.extent().depth as _,
                }
            }
            image::ViewKind::Cube | image::ViewKind::CubeArray => {
                error!("Cubic images can't be viewed as UAV");
                return Err(image::ViewCreationError::Unsupported);
            }
        }

        let handle = self.srv_uav_pool.lock().alloc_handle();
        unsafe {
            self.raw.CreateUnorderedAccessView(
                info.resource.as_mut_ptr(),
                ptr::null_mut(),
                &desc,
                handle.raw,
            );
        }

        Ok(handle)
    }

    pub(crate) fn create_raw_fence(&self, signalled: bool) -> native::Fence {
        let mut handle = native::Fence::null();
        assert_eq!(winerror::S_OK, unsafe {
            self.raw.CreateFence(
                if signalled { 1 } else { 0 },
                d3d12::D3D12_FENCE_FLAG_NONE,
                &d3d12::ID3D12Fence::uuidof(),
                handle.mut_void(),
            )
        });
        handle
    }

    pub(crate) fn create_swapchain_impl(
        &self,
        config: &w::SwapchainConfig,
        window_handle: windef::HWND,
        factory: native::WeakPtr<dxgi1_4::IDXGIFactory4>,
    ) -> Result<
        (
            native::WeakPtr<dxgi1_4::IDXGISwapChain3>,
            dxgiformat::DXGI_FORMAT,
        ),
        w::SwapchainError,
    > {
        let mut swap_chain1 = native::WeakPtr::<dxgi1_2::IDXGISwapChain1>::null();

        //TODO: proper error type?
        let non_srgb_format = conv::map_format_nosrgb(config.format).unwrap();

        let mut flags = dxgi::DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        if config.present_mode.contains(w::PresentMode::IMMEDIATE) {
            flags |= dxgi::DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        // TODO: double-check values
        let desc = dxgi1_2::DXGI_SWAP_CHAIN_DESC1 {
            AlphaMode: dxgi1_2::DXGI_ALPHA_MODE_IGNORE,
            BufferCount: config.image_count,
            Width: config.extent.width,
            Height: config.extent.height,
            Format: non_srgb_format,
            Flags: flags,
            BufferUsage: dxgitype::DXGI_USAGE_RENDER_TARGET_OUTPUT,
            SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Scaling: dxgi1_2::DXGI_SCALING_STRETCH,
            Stereo: FALSE,
            SwapEffect: dxgi::DXGI_SWAP_EFFECT_FLIP_DISCARD,
        };

        unsafe {
            let hr = factory.CreateSwapChainForHwnd(
                self.present_queue.as_mut_ptr() as *mut _,
                window_handle,
                &desc,
                ptr::null(),
                ptr::null_mut(),
                swap_chain1.mut_void() as *mut *mut _,
            );

            if !winerror::SUCCEEDED(hr) {
                error!("error on swapchain creation 0x{:x}", hr);
            }

            let (swap_chain3, hr3) = swap_chain1.cast::<dxgi1_4::IDXGISwapChain3>();
            if !winerror::SUCCEEDED(hr3) {
                error!("error on swapchain cast 0x{:x}", hr3);
            }

            swap_chain1.destroy();
            Ok((swap_chain3, non_srgb_format))
        }
    }

    pub(crate) fn wrap_swapchain(
        &self,
        inner: native::WeakPtr<dxgi1_4::IDXGISwapChain3>,
        config: &w::SwapchainConfig,
    ) -> Swapchain {
        let waitable = unsafe {
            inner.SetMaximumFrameLatency(config.image_count);
            inner.GetFrameLatencyWaitableObject()
        };

        let rtv_desc = d3d12::D3D12_RENDER_TARGET_VIEW_DESC {
            Format: conv::map_format(config.format).unwrap(),
            ViewDimension: d3d12::D3D12_RTV_DIMENSION_TEXTURE2D,
            ..unsafe { mem::zeroed() }
        };
        let rtv_heap = Device::create_descriptor_heap_impl(
            self.raw,
            native::DescriptorHeapType::Rtv,
            false,
            config.image_count as _,
        );

        let mut resources = vec![native::Resource::null(); config.image_count as usize];
        for (i, res) in resources.iter_mut().enumerate() {
            let rtv_handle = rtv_heap.at(i as _, 0).cpu;
            unsafe {
                inner.GetBuffer(i as _, &d3d12::ID3D12Resource::uuidof(), res.mut_void());
                self.raw
                    .CreateRenderTargetView(res.as_mut_ptr(), &rtv_desc, rtv_handle);
            }
        }

        Swapchain {
            inner,
            rtv_heap,
            resources,
            waitable,
            usage: config.image_usage,
            acquired_count: 0,
        }
    }

    pub(crate) fn bind_image_resource(
        &self,
        resource: native::WeakPtr<d3d12::ID3D12Resource>,
        image: &mut r::Image,
        place: r::Place,
    ) {
        let image_unbound = image.expect_unbound();
        let num_layers = image_unbound.kind.num_layers();

        //TODO: the clear_Xv is incomplete. We should support clearing images created without XXX_ATTACHMENT usage.
        // for this, we need to check the format and force the `RENDER_TARGET` flag behind the user's back
        // if the format supports being rendered into, allowing us to create clear_Xv
        let info = ViewInfo {
            resource,
            kind: image_unbound.kind,
            caps: image::ViewCapabilities::empty(),
            view_kind: match image_unbound.kind {
                image::Kind::D1(..) => image::ViewKind::D1Array,
                image::Kind::D2(..) => image::ViewKind::D2Array,
                image::Kind::D3(..) => image::ViewKind::D3,
            },
            format: image_unbound.desc.Format,
            component_mapping: IDENTITY_MAPPING,
            levels: 0..1,
            layers: 0..0,
        };
        let format_properties = self
            .format_properties
            .resolve(image_unbound.format as usize)
            .properties;
        let props = match image_unbound.tiling {
            image::Tiling::Optimal => format_properties.optimal_tiling,
            image::Tiling::Linear => format_properties.linear_tiling,
        };
        let can_clear_color = image_unbound
            .usage
            .intersects(image::Usage::TRANSFER_DST | image::Usage::COLOR_ATTACHMENT)
            && props.contains(format::ImageFeature::COLOR_ATTACHMENT);
        let can_clear_depth = image_unbound
            .usage
            .intersects(image::Usage::TRANSFER_DST | image::Usage::DEPTH_STENCIL_ATTACHMENT)
            && props.contains(format::ImageFeature::DEPTH_STENCIL_ATTACHMENT);
        let aspects = image_unbound.format.surface_desc().aspects;

        *image = r::Image::Bound(r::ImageBound {
            resource,
            place,
            surface_type: image_unbound.format.base_format().0,
            kind: image_unbound.kind,
            mip_levels: image_unbound.mip_levels,
            usage: image_unbound.usage,
            default_view_format: image_unbound.view_format,
            view_caps: image_unbound.view_caps,
            descriptor: image_unbound.desc,
            clear_cv: if aspects.contains(Aspects::COLOR) && can_clear_color {
                let format = image_unbound.view_format.unwrap();
                (0..num_layers)
                    .map(|layer| {
                        self.view_image_as_render_target(&ViewInfo {
                            format,
                            layers: layer..layer + 1,
                            ..info.clone()
                        })
                        .unwrap()
                    })
                    .collect()
            } else {
                Vec::new()
            },
            clear_dv: if aspects.contains(Aspects::DEPTH) && can_clear_depth {
                let format = image_unbound.dsv_format.unwrap();
                (0..num_layers)
                    .map(|layer| {
                        self.view_image_as_depth_stencil(&ViewInfo {
                            format,
                            layers: layer..layer + 1,
                            ..info.clone()
                        })
                        .unwrap()
                    })
                    .collect()
            } else {
                Vec::new()
            },
            clear_sv: if aspects.contains(Aspects::STENCIL) && can_clear_depth {
                let format = image_unbound.dsv_format.unwrap();
                (0..num_layers)
                    .map(|layer| {
                        self.view_image_as_depth_stencil(&ViewInfo {
                            format,
                            layers: layer..layer + 1,
                            ..info.clone()
                        })
                        .unwrap()
                    })
                    .collect()
            } else {
                Vec::new()
            },
            requirements: image_unbound.requirements,
        });
    }
}

impl d::Device<B> for Device {
    unsafe fn allocate_memory(
        &self,
        mem_type: hal::MemoryTypeId,
        size: u64,
    ) -> Result<r::Memory, d::AllocationError> {
        let mem_type = mem_type.0;
        let mem_base_id = mem_type % NUM_HEAP_PROPERTIES;
        let heap_property = &self.heap_properties[mem_base_id];

        let properties = d3d12::D3D12_HEAP_PROPERTIES {
            Type: d3d12::D3D12_HEAP_TYPE_CUSTOM,
            CPUPageProperty: heap_property.page_property,
            MemoryPoolPreference: heap_property.memory_pool,
            CreationNodeMask: 0,
            VisibleNodeMask: 0,
        };

        // Exposed memory types are grouped according to their capabilities.
        // See `MemoryGroup` for more details.
        let mem_group = mem_type / NUM_HEAP_PROPERTIES;

        let desc = d3d12::D3D12_HEAP_DESC {
            SizeInBytes: size,
            Properties: properties,
            Alignment: d3d12::D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT as _, // TODO: not always..?
            Flags: match mem_group {
                0 => d3d12::D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
                1 => d3d12::D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
                2 => d3d12::D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
                3 => d3d12::D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,
                _ => unreachable!(),
            },
        };

        let mut heap = native::Heap::null();
        let hr = self
            .raw
            .clone()
            .CreateHeap(&desc, &d3d12::ID3D12Heap::uuidof(), heap.mut_void());
        if hr != winerror::S_OK {
            if hr != winerror::E_OUTOFMEMORY {
                error!("Error in CreateHeap: 0x{:X}", hr);
            }
            return Err(d::OutOfMemory::Device.into());
        }

        // The first memory heap of each group corresponds to the default heap, which is can never
        // be mapped.
        // Devices supporting heap tier 1 can only created buffers on mem group 1 (ALLOW_ONLY_BUFFERS).
        // Devices supporting heap tier 2 always expose only mem group 0 and don't have any further restrictions.
        let is_mapable = mem_base_id != 0
            && (mem_group == MemoryGroup::Universal as _
                || mem_group == MemoryGroup::BufferOnly as _);

        // Create a buffer resource covering the whole memory slice to be able to map the whole memory.
        let resource = if is_mapable {
            let mut resource = native::Resource::null();
            let desc = d3d12::D3D12_RESOURCE_DESC {
                Dimension: d3d12::D3D12_RESOURCE_DIMENSION_BUFFER,
                Alignment: 0,
                Width: size,
                Height: 1,
                DepthOrArraySize: 1,
                MipLevels: 1,
                Format: dxgiformat::DXGI_FORMAT_UNKNOWN,
                SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                    Count: 1,
                    Quality: 0,
                },
                Layout: d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                Flags: d3d12::D3D12_RESOURCE_FLAG_NONE,
            };

            assert_eq!(
                winerror::S_OK,
                self.raw.clone().CreatePlacedResource(
                    heap.as_mut_ptr(),
                    0,
                    &desc,
                    d3d12::D3D12_RESOURCE_STATE_COMMON,
                    ptr::null(),
                    &d3d12::ID3D12Resource::uuidof(),
                    resource.mut_void(),
                )
            );

            Some(resource)
        } else {
            None
        };

        Ok(r::Memory {
            heap,
            type_id: mem_type,
            size,
            resource,
        })
    }

    unsafe fn create_command_pool(
        &self,
        family: QueueFamilyId,
        create_flags: CommandPoolCreateFlags,
    ) -> Result<CommandPool, d::OutOfMemory> {
        let list_type = QUEUE_FAMILIES[family.0].native_type();
        Ok(CommandPool::new(
            self.raw,
            list_type,
            &self.shared,
            create_flags,
        ))
    }

    unsafe fn destroy_command_pool(&self, _pool: CommandPool) {}

    unsafe fn create_render_pass<'a, Ia, Is, Id>(
        &self,
        attachments: Ia,
        subpasses: Is,
        dependencies: Id,
    ) -> Result<r::RenderPass, d::OutOfMemory>
    where
        Ia: Iterator<Item = pass::Attachment>,
        Is: Iterator<Item = pass::SubpassDesc<'a>>,
        Id: Iterator<Item = pass::SubpassDependency>,
    {
        #[derive(Copy, Clone, Debug, PartialEq)]
        enum SubState {
            New(d3d12::D3D12_RESOURCE_STATES),
            // Color attachment which will be resolved at the end of the subpass
            Resolve(d3d12::D3D12_RESOURCE_STATES),
            Preserve,
            Undefined,
        }
        /// Temporary information about every sub-pass
        struct SubInfo<'a> {
            desc: pass::SubpassDesc<'a>,
            /// States before the render-pass (in self.start)
            /// and after the render-pass (in self.end).
            external_dependencies: Range<image::Access>,
            /// Counts the number of dependencies that need to be resolved
            /// before starting this subpass.
            unresolved_dependencies: u16,
        }
        struct AttachmentInfo {
            sub_states: Vec<SubState>,
            last_state: d3d12::D3D12_RESOURCE_STATES,
            barrier_start_index: usize,
        }

        let attachments = attachments.collect::<SmallVec<[_; 5]>>();
        let mut sub_infos = subpasses
            .map(|desc| SubInfo {
                desc: desc.clone(),
                external_dependencies: image::Access::empty()..image::Access::empty(),
                unresolved_dependencies: 0,
            })
            .collect::<SmallVec<[_; 1]>>();
        let dependencies = dependencies.collect::<SmallVec<[_; 2]>>();

        let mut att_infos = (0..attachments.len())
            .map(|_| AttachmentInfo {
                sub_states: vec![SubState::Undefined; sub_infos.len()],
                last_state: d3d12::D3D12_RESOURCE_STATE_COMMON, // is to be overwritten
                barrier_start_index: 0,
            })
            .collect::<SmallVec<[_; 5]>>();

        for dep in &dependencies {
            match dep.passes {
                Range {
                    start: None,
                    end: None,
                } => {
                    error!("Unexpected external-external dependency!");
                }
                Range {
                    start: None,
                    end: Some(sid),
                } => {
                    sub_infos[sid as usize].external_dependencies.start |= dep.accesses.start;
                }
                Range {
                    start: Some(sid),
                    end: None,
                } => {
                    sub_infos[sid as usize].external_dependencies.end |= dep.accesses.end;
                }
                Range {
                    start: Some(from_sid),
                    end: Some(sid),
                } => {
                    //Note: self-dependencies are ignored
                    if from_sid != sid {
                        sub_infos[sid as usize].unresolved_dependencies += 1;
                    }
                }
            }
        }

        // Fill out subpass known layouts
        for (sid, sub_info) in sub_infos.iter().enumerate() {
            let sub = &sub_info.desc;
            for (i, &(id, _layout)) in sub.colors.iter().enumerate() {
                let target_state = d3d12::D3D12_RESOURCE_STATE_RENDER_TARGET;
                let state = match sub.resolves.get(i) {
                    Some(_) => SubState::Resolve(target_state),
                    None => SubState::New(target_state),
                };
                let old = mem::replace(&mut att_infos[id].sub_states[sid], state);
                debug_assert_eq!(SubState::Undefined, old);
            }
            for &(id, layout) in sub.depth_stencil {
                let state = SubState::New(match layout {
                    image::Layout::DepthStencilAttachmentOptimal => {
                        d3d12::D3D12_RESOURCE_STATE_DEPTH_WRITE
                    }
                    image::Layout::DepthStencilReadOnlyOptimal => {
                        d3d12::D3D12_RESOURCE_STATE_DEPTH_READ
                    }
                    image::Layout::General => d3d12::D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    _ => {
                        error!("Unexpected depth/stencil layout: {:?}", layout);
                        d3d12::D3D12_RESOURCE_STATE_COMMON
                    }
                });
                let old = mem::replace(&mut att_infos[id].sub_states[sid], state);
                debug_assert_eq!(SubState::Undefined, old);
            }
            for &(id, _layout) in sub.inputs {
                let state = SubState::New(d3d12::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                let old = mem::replace(&mut att_infos[id].sub_states[sid], state);
                debug_assert_eq!(SubState::Undefined, old);
            }
            for &(id, _layout) in sub.resolves {
                let state = SubState::New(d3d12::D3D12_RESOURCE_STATE_RESOLVE_DEST);
                let old = mem::replace(&mut att_infos[id].sub_states[sid], state);
                debug_assert_eq!(SubState::Undefined, old);
            }
            for &id in sub.preserves {
                let old = mem::replace(&mut att_infos[id].sub_states[sid], SubState::Preserve);
                debug_assert_eq!(SubState::Undefined, old);
            }
        }

        let mut rp = r::RenderPass {
            attachments: attachments.iter().cloned().collect(),
            subpasses: Vec::new(),
            post_barriers: Vec::new(),
            raw_name: Vec::new(),
        };

        while let Some(sid) = sub_infos
            .iter()
            .position(|si| si.unresolved_dependencies == 0)
        {
            for dep in &dependencies {
                if dep.passes.start != dep.passes.end
                    && dep.passes.start == Some(sid as pass::SubpassId)
                {
                    if let Some(other) = dep.passes.end {
                        sub_infos[other as usize].unresolved_dependencies -= 1;
                    }
                }
            }

            let si = &mut sub_infos[sid];
            si.unresolved_dependencies = !0; // mark as done

            // Subpass barriers
            let mut pre_barriers = Vec::new();
            let mut post_barriers = Vec::new();
            for (att_id, (ai, att)) in att_infos.iter_mut().zip(attachments.iter()).enumerate() {
                // Attachment wasn't used before, figure out the initial state
                if ai.barrier_start_index == 0 {
                    //Note: the external dependencies are provided for all attachments that are
                    // first used in this sub-pass, so they may contain more states than we expect
                    // for this particular attachment.
                    ai.last_state = conv::map_image_resource_state(
                        si.external_dependencies.start,
                        att.layouts.start,
                    );
                }
                // Barrier from previous subpass to current or following subpasses.
                match ai.sub_states[sid] {
                    SubState::Preserve => {
                        ai.barrier_start_index = rp.subpasses.len() + 1;
                    }
                    SubState::New(state) if state != ai.last_state => {
                        let barrier = r::BarrierDesc::new(att_id, ai.last_state..state);
                        match rp.subpasses.get_mut(ai.barrier_start_index) {
                            Some(past_subpass) => {
                                let split = barrier.split();
                                past_subpass.pre_barriers.push(split.start);
                                pre_barriers.push(split.end);
                            }
                            None => pre_barriers.push(barrier),
                        }
                        ai.last_state = state;
                        ai.barrier_start_index = rp.subpasses.len() + 1;
                    }
                    SubState::Resolve(state) => {
                        // 1. Standard pre barrier to update state from previous pass into desired substate.
                        if state != ai.last_state {
                            let barrier = r::BarrierDesc::new(att_id, ai.last_state..state);
                            match rp.subpasses.get_mut(ai.barrier_start_index) {
                                Some(past_subpass) => {
                                    let split = barrier.split();
                                    past_subpass.pre_barriers.push(split.start);
                                    pre_barriers.push(split.end);
                                }
                                None => pre_barriers.push(barrier),
                            }
                        }

                        // 2. Post Barrier at the end of the subpass into RESOLVE_SOURCE.
                        let resolve_state = d3d12::D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
                        let barrier = r::BarrierDesc::new(att_id, state..resolve_state);
                        post_barriers.push(barrier);

                        ai.last_state = resolve_state;
                        ai.barrier_start_index = rp.subpasses.len() + 1;
                    }
                    SubState::Undefined | SubState::New(_) => {}
                };
            }

            rp.subpasses.push(r::SubpassDesc {
                color_attachments: si.desc.colors.iter().cloned().collect(),
                depth_stencil_attachment: si.desc.depth_stencil.cloned(),
                input_attachments: si.desc.inputs.iter().cloned().collect(),
                resolve_attachments: si.desc.resolves.iter().cloned().collect(),
                pre_barriers,
                post_barriers,
            });
        }
        // if this fails, our graph has cycles
        assert_eq!(rp.subpasses.len(), sub_infos.len());
        assert!(sub_infos.iter().all(|si| si.unresolved_dependencies == !0));

        // take care of the post-pass transitions at the end of the renderpass.
        for (att_id, (ai, att)) in att_infos.iter().zip(attachments.iter()).enumerate() {
            let state_dst = if ai.barrier_start_index == 0 {
                // attachment wasn't used in any sub-pass?
                continue;
            } else {
                let si = &sub_infos[ai.barrier_start_index - 1];
                conv::map_image_resource_state(si.external_dependencies.end, att.layouts.end)
            };
            if state_dst == ai.last_state {
                continue;
            }
            let barrier = r::BarrierDesc::new(att_id, ai.last_state..state_dst);
            match rp.subpasses.get_mut(ai.barrier_start_index) {
                Some(past_subpass) => {
                    let split = barrier.split();
                    past_subpass.pre_barriers.push(split.start);
                    rp.post_barriers.push(split.end);
                }
                None => rp.post_barriers.push(barrier),
            }
        }

        Ok(rp)
    }

    unsafe fn create_pipeline_layout<'a, Is, Ic>(
        &self,
        sets: Is,
        push_constant_ranges: Ic,
    ) -> Result<r::PipelineLayout, d::OutOfMemory>
    where
        Is: Iterator<Item = &'a r::DescriptorSetLayout>,
        Ic: Iterator<Item = (pso::ShaderStageFlags, Range<u32>)>,
    {
        // Pipeline layouts are implemented as RootSignature for D3D12.
        //
        // Push Constants are implemented as root constants.
        //
        // Each descriptor set layout will be one table entry of the root signature.
        // We have the additional restriction that SRV/CBV/UAV and samplers need to be
        // separated, so each set layout will actually occupy up to 2 entries!
        // SRV/CBV/UAV tables are added to the signature first, then Sampler tables,
        // and finally dynamic uniform descriptors.
        //
        // Dynamic uniform buffers are implemented as root descriptors.
        // This allows to handle the dynamic offsets properly, which would not be feasible
        // with a combination of root constant and descriptor table.
        //
        // Root signature layout:
        //     Root Constants: Register: Offest/4, Space: 0
        //     ...
        // DescriptorTable0: Space: 1 (SrvCbvUav)
        // DescriptorTable0: Space: 1 (Sampler)
        // Root Descriptors 0
        // DescriptorTable1: Space: 2 (SrvCbvUav)
        // Root Descriptors 1
        //     ...

        let sets = sets.collect::<Vec<_>>();

        let mut root_offset = 0u32;
        let root_constants = root_constants::split(push_constant_ranges)
            .iter()
            .map(|constant| {
                assert!(constant.range.start <= constant.range.end);
                root_offset += constant.range.end - constant.range.start;

                RootConstant {
                    stages: constant.stages,
                    range: constant.range.start..constant.range.end,
                }
            })
            .collect::<Vec<_>>();

        info!(
            "Creating a pipeline layout with {} sets and {} root constants",
            sets.len(),
            root_constants.len()
        );

        // Number of elements in the root signature.
        // Guarantees that no re-allocation is done, and our pointers are valid
        let mut parameters = Vec::with_capacity(root_constants.len() + sets.len() * 2);
        let mut parameter_offsets = Vec::with_capacity(parameters.capacity());

        // Convert root signature descriptions into root signature parameters.
        for root_constant in root_constants.iter() {
            debug!(
                "\tRoot constant set={} range {:?}",
                ROOT_CONSTANT_SPACE, root_constant.range
            );
            parameter_offsets.push(root_constant.range.start);
            parameters.push(native::RootParameter::constants(
                conv::map_shader_visibility(root_constant.stages),
                native::Binding {
                    register: root_constant.range.start as _,
                    space: ROOT_CONSTANT_SPACE,
                },
                (root_constant.range.end - root_constant.range.start) as _,
            ));
        }

        // Offest of `spaceN` for descriptor tables. Root constants will be in
        // `space0`.
        // This has to match `patch_spirv_resources` logic.
        let root_space_offset = if !root_constants.is_empty() { 1 } else { 0 };

        // Collect the whole number of bindings we will create upfront.
        // It allows us to preallocate enough storage to avoid reallocation,
        // which could cause invalid pointers.
        let total = sets
            .iter()
            .map(|desc_set| {
                let mut sum = 0;
                for binding in desc_set.bindings.iter() {
                    let content = r::DescriptorContent::from(binding.ty);
                    if !content.is_dynamic() {
                        sum += content.bits().count_ones() as usize;
                    }
                }
                sum
            })
            .sum();
        let mut ranges = Vec::with_capacity(total);

        let elements = sets
            .iter()
            .enumerate()
            .map(|(i, set)| {
                let space = (root_space_offset + i) as u32;
                let mut table_type = r::SetTableTypes::empty();
                let root_table_offset = root_offset;

                //TODO: split between sampler and non-sampler tables
                let visibility = conv::map_shader_visibility(
                    set.bindings
                        .iter()
                        .fold(pso::ShaderStageFlags::empty(), |u, bind| {
                            u | bind.stage_flags
                        }),
                );

                for bind in set.bindings.iter() {
                    debug!("\tRange {:?} at space={}", bind, space);
                }

                let describe = |bind: &pso::DescriptorSetLayoutBinding, ty| {
                    native::DescriptorRange::new(
                        ty,
                        bind.count as _,
                        native::Binding {
                            register: bind.binding as _,
                            space,
                        },
                        d3d12::D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
                    )
                };

                let mut mutable_bindings = auxil::FastHashSet::default();

                // SRV/CBV/UAV descriptor tables
                let mut range_base = ranges.len();
                for bind in set.bindings.iter() {
                    let content = r::DescriptorContent::from(bind.ty);
                    if !content.is_dynamic() {
                        // Descriptor table ranges
                        if content.contains(r::DescriptorContent::CBV) {
                            ranges.push(describe(bind, native::DescriptorRangeType::CBV));
                        }
                        if content.contains(r::DescriptorContent::SRV) {
                            ranges.push(describe(bind, native::DescriptorRangeType::SRV));
                        }
                        if content.contains(r::DescriptorContent::UAV) {
                            ranges.push(describe(bind, native::DescriptorRangeType::UAV));
                            mutable_bindings.insert(bind.binding);
                        }
                    }
                }
                if ranges.len() > range_base {
                    parameter_offsets.push(root_offset);
                    parameters.push(native::RootParameter::descriptor_table(
                        visibility,
                        &ranges[range_base..],
                    ));
                    table_type |= r::SRV_CBV_UAV;
                    root_offset += 1;
                }

                // Sampler descriptor tables
                range_base = ranges.len();
                for bind in set.bindings.iter() {
                    let content = r::DescriptorContent::from(bind.ty);
                    if content.contains(r::DescriptorContent::SAMPLER) {
                        ranges.push(describe(bind, native::DescriptorRangeType::Sampler));
                    }
                }
                if ranges.len() > range_base {
                    parameter_offsets.push(root_offset);
                    parameters.push(native::RootParameter::descriptor_table(
                        visibility,
                        &ranges[range_base..],
                    ));
                    table_type |= r::SAMPLERS;
                    root_offset += 1;
                }

                // Root (dynamic) descriptor tables
                for bind in set.bindings.iter() {
                    let content = r::DescriptorContent::from(bind.ty);
                    if content.is_dynamic() {
                        let binding = native::Binding {
                            register: bind.binding as _,
                            space,
                        };

                        if content.contains(r::DescriptorContent::CBV) {
                            parameter_offsets.push(root_offset);
                            parameters
                                .push(native::RootParameter::cbv_descriptor(visibility, binding));
                            root_offset += 2; // root CBV costs 2 words
                        }
                        if content.contains(r::DescriptorContent::SRV) {
                            parameter_offsets.push(root_offset);
                            parameters
                                .push(native::RootParameter::srv_descriptor(visibility, binding));
                            root_offset += 2; // root SRV costs 2 words
                        }
                        if content.contains(r::DescriptorContent::UAV) {
                            parameter_offsets.push(root_offset);
                            parameters
                                .push(native::RootParameter::uav_descriptor(visibility, binding));
                            root_offset += 2; // root UAV costs 2 words
                        }
                    }
                }

                r::RootElement {
                    table: r::RootTable {
                        ty: table_type,
                        offset: root_table_offset as _,
                    },
                    mutable_bindings,
                }
            })
            .collect();

        // Ensure that we didn't reallocate!
        debug_assert_eq!(ranges.len(), total);
        assert_eq!(parameters.len(), parameter_offsets.len());

        // TODO: error handling
        let (signature_raw, error) = match self.library.serialize_root_signature(
            native::RootSignatureVersion::V1_0,
            &parameters,
            &[],
            native::RootSignatureFlags::ALLOW_IA_INPUT_LAYOUT,
        ) {
            Ok((pair, hr)) if winerror::SUCCEEDED(hr) => pair,
            Ok((_, hr)) => panic!("Can't serialize root signature: {:?}", hr),
            Err(e) => panic!("Can't find serialization function: {:?}", e),
        };

        if !error.is_null() {
            error!(
                "Root signature serialization error: {:?}",
                error.as_c_str().to_str().unwrap()
            );
            error.destroy();
        }

        // TODO: error handling
        let (signature, _hr) = self.raw.create_root_signature(signature_raw, 0);
        signature_raw.destroy();

        Ok(r::PipelineLayout {
            shared: Arc::new(r::PipelineShared {
                signature,
                constants: root_constants,
                parameter_offsets,
                total_slots: root_offset,
            }),
            elements,
        })
    }

    unsafe fn create_pipeline_cache(&self, _data: Option<&[u8]>) -> Result<(), d::OutOfMemory> {
        Ok(())
    }

    unsafe fn get_pipeline_cache_data(&self, _cache: &()) -> Result<Vec<u8>, d::OutOfMemory> {
        //empty
        Ok(Vec::new())
    }

    unsafe fn destroy_pipeline_cache(&self, _: ()) {
        //empty
    }

    unsafe fn merge_pipeline_caches<'a, I>(&self, _: &mut (), _: I) -> Result<(), d::OutOfMemory>
    where
        I: Iterator<Item = &'a ()>,
    {
        //empty
        Ok(())
    }

    unsafe fn create_graphics_pipeline<'a>(
        &self,
        desc: &pso::GraphicsPipelineDesc<'a, B>,
        _cache: Option<&()>,
    ) -> Result<r::GraphicsPipeline, pso::CreationError> {
        enum ShaderBc {
            Owned(native::Blob),
            Borrowed(native::Blob),
            None,
        }
        let features = &self.features;
        impl ShaderBc {
            pub fn shader(&self) -> native::Shader {
                match *self {
                    ShaderBc::Owned(ref bc) | ShaderBc::Borrowed(ref bc) => {
                        native::Shader::from_blob(*bc)
                    }
                    ShaderBc::None => native::Shader::null(),
                }
            }
        }

        let build_shader = |stage: ShaderStage,
                            source: Option<&pso::EntryPoint<'a, B>>|
         -> Result<ShaderBc, pso::CreationError> {
            let source = match source {
                Some(src) => src,
                None => return Ok(ShaderBc::None),
            };

            let (shader, owned) = Self::extract_entry_point(stage, source, desc.layout, features)?;
            Ok(if owned {
                ShaderBc::Owned(shader)
            } else {
                ShaderBc::Borrowed(shader)
            })
        };

        let vertex_buffers: Vec<pso::VertexBufferDesc> = Vec::new();
        let attributes: Vec<pso::AttributeDesc> = Vec::new();
        let mesh_input_assembler = pso::InputAssemblerDesc::new(pso::Primitive::TriangleList);
        let (vertex_buffers, attributes, input_assembler, vs, gs, hs, ds, _, _) =
            match desc.primitive_assembler {
                pso::PrimitiveAssemblerDesc::Vertex {
                    buffers,
                    attributes,
                    ref input_assembler,
                    ref vertex,
                    ref tessellation,
                    ref geometry,
                } => {
                    let (hs, ds) = if let Some(ts) = tessellation {
                        (Some(&ts.0), Some(&ts.1))
                    } else {
                        (None, None)
                    };

                    (
                        buffers,
                        attributes,
                        input_assembler,
                        Some(vertex),
                        geometry.as_ref(),
                        hs,
                        ds,
                        None,
                        None,
                    )
                }
                pso::PrimitiveAssemblerDesc::Mesh { ref task, ref mesh } => (
                    &vertex_buffers[..],
                    &attributes[..],
                    &mesh_input_assembler,
                    None,
                    None,
                    None,
                    None,
                    task.as_ref(),
                    Some(mesh),
                ),
            };

        let vertex_semantic_remapping = if let Some(ref vs) = vs {
            // If we have a pre-compiled shader, we've lost the information we need to recover
            // this information, so just pretend like this workaround never existed and hope
            // for the best.
            if let crate::resource::ShaderModule::Spirv(ref spv) = vs.module {
                Some(Self::introspect_spirv_vertex_semantic_remapping(spv)?)
            } else {
                None
            }
        } else {
            None
        };

        let vs = build_shader(ShaderStage::Vertex, vs)?;
        let gs = build_shader(ShaderStage::Geometry, gs)?;
        let hs = build_shader(ShaderStage::Domain, hs)?;
        let ds = build_shader(ShaderStage::Hull, ds)?;
        let ps = build_shader(ShaderStage::Fragment, desc.fragment.as_ref())?;

        // Rebind vertex buffers, see native.rs for more details.
        let mut vertex_bindings = [None; MAX_VERTEX_BUFFERS];
        let mut vertex_strides = [0; MAX_VERTEX_BUFFERS];

        for buffer in vertex_buffers {
            vertex_strides[buffer.binding as usize] = buffer.stride;
        }
        // Fill in identity mapping where we don't need to adjust anything.
        for attrib in attributes {
            let binding = attrib.binding as usize;
            let stride = vertex_strides[attrib.binding as usize];
            if attrib.element.offset < stride {
                vertex_bindings[binding] = Some(r::VertexBinding {
                    stride: vertex_strides[attrib.binding as usize],
                    offset: 0,
                    mapped_binding: binding,
                });
            }
        }

        // See [`introspect_spirv_vertex_semantic_remapping`] for details of why this is needed.
        let semantics: Vec<_> = attributes
            .iter()
            .map(|attrib| {
                let semantics = vertex_semantic_remapping
                    .as_ref()
                    .and_then(|map| map.get(&attrib.location));
                match semantics {
                    Some(Some((major, minor))) => {
                        let name = std::borrow::Cow::Owned(format!("TEXCOORD{}_\0", major));
                        let location = *minor;
                        (name, location)
                    }
                    _ => {
                        let name = std::borrow::Cow::Borrowed("TEXCOORD\0");
                        let location = attrib.location;
                        (name, location)
                    }
                }
            })
            .collect();

        // Define input element descriptions
        let input_element_descs = attributes
            .iter()
            .zip(semantics.iter())
            .filter_map(|(attrib, (semantic_name, semantic_index))| {
                let buffer_desc = match vertex_buffers
                    .iter()
                    .find(|buffer_desc| buffer_desc.binding == attrib.binding)
                {
                    Some(buffer_desc) => buffer_desc,
                    None => {
                        error!(
                            "Couldn't find associated vertex buffer description {:?}",
                            attrib.binding
                        );
                        return Some(Err(pso::CreationError::Other));
                    }
                };

                let (slot_class, step_rate) = match buffer_desc.rate {
                    VertexInputRate::Vertex => {
                        (d3d12::D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0)
                    }
                    VertexInputRate::Instance(divisor) => {
                        (d3d12::D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, divisor)
                    }
                };
                let format = attrib.element.format;

                // Check if we need to add a new remapping in-case the offset is
                // higher than the vertex stride.
                // In this case we rebase the attribute to zero offset.
                let binding = attrib.binding as usize;
                let stride = vertex_strides[binding];
                let offset = attrib.element.offset;
                let (input_slot, offset) = if stride <= offset {
                    // Number of input attributes may not exceed bindings, see limits.
                    // We will always find at least one free binding.
                    let mapping = vertex_bindings.iter().position(Option::is_none).unwrap();
                    vertex_bindings[mapping] = Some(r::VertexBinding {
                        stride: vertex_strides[binding],
                        offset: offset,
                        mapped_binding: binding,
                    });

                    (mapping, 0)
                } else {
                    (binding, offset)
                };

                Some(Ok(d3d12::D3D12_INPUT_ELEMENT_DESC {
                    SemanticName: semantic_name.as_ptr() as *const _, // Semantic name used by SPIRV-Cross
                    SemanticIndex: *semantic_index,
                    Format: match conv::map_format(format) {
                        Some(fm) => fm,
                        None => {
                            error!("Unable to find DXGI format for {:?}", format);
                            return Some(Err(pso::CreationError::Other));
                        }
                    },
                    InputSlot: input_slot as _,
                    AlignedByteOffset: offset,
                    InputSlotClass: slot_class,
                    InstanceDataStepRate: step_rate as _,
                }))
            })
            .collect::<Result<Vec<_>, _>>()?;

        // TODO: check maximum number of rtvs
        // Get associated subpass information
        let pass = {
            let subpass = &desc.subpass;
            match subpass.main_pass.subpasses.get(subpass.index as usize) {
                Some(subpass) => subpass,
                None => return Err(pso::CreationError::InvalidSubpass(subpass.index)),
            }
        };

        // Get color attachment formats from subpass
        let (rtvs, num_rtvs) = {
            let mut rtvs = [dxgiformat::DXGI_FORMAT_UNKNOWN;
                d3d12::D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT as usize];
            let mut num_rtvs = 0;
            for (rtv, target) in rtvs.iter_mut().zip(pass.color_attachments.iter()) {
                let format = desc.subpass.main_pass.attachments[target.0].format;
                *rtv = format
                    .and_then(conv::map_format)
                    .unwrap_or(dxgiformat::DXGI_FORMAT_UNKNOWN);
                num_rtvs += 1;
            }
            (rtvs, num_rtvs)
        };

        let sample_desc = dxgitype::DXGI_SAMPLE_DESC {
            Count: match desc.multisampling {
                Some(ref ms) => ms.rasterization_samples as _,
                None => 1,
            },
            Quality: 0,
        };

        // Setup pipeline description
        let pso_desc = d3d12::D3D12_GRAPHICS_PIPELINE_STATE_DESC {
            pRootSignature: desc.layout.shared.signature.as_mut_ptr(),
            VS: *vs.shader(),
            PS: *ps.shader(),
            GS: *gs.shader(),
            DS: *ds.shader(),
            HS: *hs.shader(),
            StreamOutput: d3d12::D3D12_STREAM_OUTPUT_DESC {
                pSODeclaration: ptr::null(),
                NumEntries: 0,
                pBufferStrides: ptr::null(),
                NumStrides: 0,
                RasterizedStream: 0,
            },
            BlendState: d3d12::D3D12_BLEND_DESC {
                AlphaToCoverageEnable: desc.multisampling.as_ref().map_or(FALSE, |ms| {
                    if ms.alpha_coverage {
                        TRUE
                    } else {
                        FALSE
                    }
                }),
                IndependentBlendEnable: TRUE,
                RenderTarget: conv::map_render_targets(&desc.blender.targets),
            },
            SampleMask: match desc.multisampling {
                Some(ref ms) => ms.sample_mask as u32,
                None => UINT::max_value(),
            },
            RasterizerState: conv::map_rasterizer(&desc.rasterizer, desc.multisampling.is_some()),
            DepthStencilState: conv::map_depth_stencil(&desc.depth_stencil),
            InputLayout: d3d12::D3D12_INPUT_LAYOUT_DESC {
                pInputElementDescs: if input_element_descs.is_empty() {
                    ptr::null()
                } else {
                    input_element_descs.as_ptr()
                },
                NumElements: input_element_descs.len() as u32,
            },
            IBStripCutValue: match input_assembler.restart_index {
                Some(hal::IndexType::U16) => d3d12::D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,
                Some(hal::IndexType::U32) => d3d12::D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF,
                None => d3d12::D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            },
            PrimitiveTopologyType: conv::map_topology_type(input_assembler.primitive),
            NumRenderTargets: num_rtvs,
            RTVFormats: rtvs,
            DSVFormat: pass
                .depth_stencil_attachment
                .and_then(|att_ref| {
                    desc.subpass.main_pass.attachments[att_ref.0]
                        .format
                        .and_then(|f| conv::map_format_dsv(f.base_format().0))
                })
                .unwrap_or(dxgiformat::DXGI_FORMAT_UNKNOWN),
            SampleDesc: sample_desc,
            NodeMask: 0,
            CachedPSO: d3d12::D3D12_CACHED_PIPELINE_STATE {
                pCachedBlob: ptr::null(),
                CachedBlobSizeInBytes: 0,
            },
            Flags: d3d12::D3D12_PIPELINE_STATE_FLAG_NONE,
        };
        let topology = conv::map_topology(input_assembler);

        // Create PSO
        let mut pipeline = native::PipelineState::null();
        let hr = if desc.depth_stencil.depth_bounds {
            // The DepthBoundsTestEnable option isn't available in the original D3D12_GRAPHICS_PIPELINE_STATE_DESC struct.
            // Instead, we must use the newer subobject stream method.
            let (device2, hr) = self.raw.cast::<d3d12::ID3D12Device2>();
            if winerror::SUCCEEDED(hr) {
                let mut pss_stream = GraphicsPipelineStateSubobjectStream::new(&pso_desc, true);
                let pss_desc = d3d12::D3D12_PIPELINE_STATE_STREAM_DESC {
                    SizeInBytes: mem::size_of_val(&pss_stream),
                    pPipelineStateSubobjectStream: &mut pss_stream as *mut _ as _,
                };
                device2.CreatePipelineState(
                    &pss_desc,
                    &d3d12::ID3D12PipelineState::uuidof(),
                    pipeline.mut_void(),
                )
            } else {
                hr
            }
        } else {
            self.raw.clone().CreateGraphicsPipelineState(
                &pso_desc,
                &d3d12::ID3D12PipelineState::uuidof(),
                pipeline.mut_void(),
            )
        };

        let destroy_shader = |shader: ShaderBc| {
            if let ShaderBc::Owned(bc) = shader {
                bc.destroy();
            }
        };

        destroy_shader(vs);
        destroy_shader(ps);
        destroy_shader(gs);
        destroy_shader(hs);
        destroy_shader(ds);

        if winerror::SUCCEEDED(hr) {
            let mut baked_states = desc.baked_states.clone();
            if !desc.depth_stencil.depth_bounds {
                baked_states.depth_bounds = None;
            }
            if let Some(name) = desc.label {
                let cwstr = wide_cstr(name);
                pipeline.SetName(cwstr.as_ptr());
            }

            Ok(r::GraphicsPipeline {
                raw: pipeline,
                shared: Arc::clone(&desc.layout.shared),
                topology,
                vertex_bindings,
                baked_states,
            })
        } else {
            let error = format!("Failed to build shader: {:x}", hr);
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::GRAPHICS,
                error,
            ))
        }
    }

    unsafe fn create_compute_pipeline<'a>(
        &self,
        desc: &pso::ComputePipelineDesc<'a, B>,
        _cache: Option<&()>,
    ) -> Result<r::ComputePipeline, pso::CreationError> {
        let (cs, cs_destroy) = Self::extract_entry_point(
            ShaderStage::Compute,
            &desc.shader,
            desc.layout,
            &self.features,
        )?;

        let (pipeline, hr) = self.raw.create_compute_pipeline_state(
            desc.layout.shared.signature,
            native::Shader::from_blob(cs),
            0,
            native::CachedPSO::null(),
            native::PipelineStateFlags::empty(),
        );

        if cs_destroy {
            cs.destroy();
        }

        if winerror::SUCCEEDED(hr) {
            if let Some(name) = desc.label {
                let cwstr = wide_cstr(name);
                pipeline.SetName(cwstr.as_ptr());
            }

            Ok(r::ComputePipeline {
                raw: pipeline,
                shared: Arc::clone(&desc.layout.shared),
            })
        } else {
            let error = format!("Failed to build shader: {:x}", hr);
            Err(pso::CreationError::ShaderCreationError(
                pso::ShaderStageFlags::COMPUTE,
                error,
            ))
        }
    }

    unsafe fn create_framebuffer<I>(
        &self,
        _renderpass: &r::RenderPass,
        _attachments: I,
        extent: image::Extent,
    ) -> Result<r::Framebuffer, d::OutOfMemory> {
        Ok(r::Framebuffer {
            layers: extent.depth as _,
        })
    }

    unsafe fn create_shader_module(
        &self,
        raw_data: &[u32],
    ) -> Result<r::ShaderModule, d::ShaderError> {
        Ok(r::ShaderModule::Spirv(raw_data.into()))
    }

    unsafe fn create_buffer(
        &self,
        mut size: u64,
        usage: buffer::Usage,
        _sparse: memory::SparseFlags,
    ) -> Result<r::Buffer, buffer::CreationError> {
        if usage.contains(buffer::Usage::UNIFORM) {
            // Constant buffer view sizes need to be aligned.
            // Coupled with the offset alignment we can enforce an aligned CBV size
            // on descriptor updates.
            let mask = d3d12::D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT as u64 - 1;
            size = (size + mask) & !mask;
        }
        if usage.contains(buffer::Usage::TRANSFER_DST) {
            // minimum of 1 word for the clear UAV
            size = size.max(4);
        }

        let type_mask_shift = if self.private_caps.heterogeneous_resource_heaps {
            MEM_TYPE_UNIVERSAL_SHIFT
        } else {
            MEM_TYPE_BUFFER_SHIFT
        };

        let requirements = memory::Requirements {
            size,
            alignment: d3d12::D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT as u64,
            type_mask: MEM_TYPE_MASK << type_mask_shift,
        };

        Ok(r::Buffer::Unbound(r::BufferUnbound {
            requirements,
            usage,
            name: None,
        }))
    }

    unsafe fn get_buffer_requirements(&self, buffer: &r::Buffer) -> Requirements {
        match buffer {
            r::Buffer::Unbound(b) => b.requirements,
            r::Buffer::Bound(b) => b.requirements,
        }
    }

    unsafe fn bind_buffer_memory(
        &self,
        memory: &r::Memory,
        offset: u64,
        buffer: &mut r::Buffer,
    ) -> Result<(), d::BindError> {
        let buffer_unbound = buffer.expect_unbound();
        if buffer_unbound.requirements.type_mask & (1 << memory.type_id) == 0 {
            error!(
                "Bind memory failure: supported mask 0x{:x}, given id {}",
                buffer_unbound.requirements.type_mask, memory.type_id
            );
            return Err(d::BindError::WrongMemory);
        }
        if offset + buffer_unbound.requirements.size > memory.size {
            return Err(d::BindError::OutOfBounds);
        }

        let mut resource = native::Resource::null();
        let desc = d3d12::D3D12_RESOURCE_DESC {
            Dimension: d3d12::D3D12_RESOURCE_DIMENSION_BUFFER,
            Alignment: 0,
            Width: buffer_unbound.requirements.size,
            Height: 1,
            DepthOrArraySize: 1,
            MipLevels: 1,
            Format: dxgiformat::DXGI_FORMAT_UNKNOWN,
            SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Layout: d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            Flags: conv::map_buffer_flags(buffer_unbound.usage),
        };

        assert_eq!(
            winerror::S_OK,
            self.raw.clone().CreatePlacedResource(
                memory.heap.as_mut_ptr(),
                offset,
                &desc,
                d3d12::D3D12_RESOURCE_STATE_COMMON,
                ptr::null(),
                &d3d12::ID3D12Resource::uuidof(),
                resource.mut_void(),
            )
        );

        if let Some(ref name) = buffer_unbound.name {
            resource.SetName(name.as_ptr());
        }

        let clear_uav = if buffer_unbound.usage.contains(buffer::Usage::TRANSFER_DST) {
            let handle = self.srv_uav_pool.lock().alloc_handle();
            let mut view_desc = d3d12::D3D12_UNORDERED_ACCESS_VIEW_DESC {
                Format: dxgiformat::DXGI_FORMAT_R32_TYPELESS,
                ViewDimension: d3d12::D3D12_UAV_DIMENSION_BUFFER,
                u: mem::zeroed(),
            };

            *view_desc.u.Buffer_mut() = d3d12::D3D12_BUFFER_UAV {
                FirstElement: 0,
                NumElements: (buffer_unbound.requirements.size / 4) as _,
                StructureByteStride: 0,
                CounterOffsetInBytes: 0,
                Flags: d3d12::D3D12_BUFFER_UAV_FLAG_RAW,
            };

            self.raw.CreateUnorderedAccessView(
                resource.as_mut_ptr(),
                ptr::null_mut(),
                &view_desc,
                handle.raw,
            );
            Some(handle)
        } else {
            None
        };

        *buffer = r::Buffer::Bound(r::BufferBound {
            resource,
            requirements: buffer_unbound.requirements,
            clear_uav,
        });

        Ok(())
    }

    unsafe fn create_buffer_view(
        &self,
        buffer: &r::Buffer,
        format: Option<format::Format>,
        sub: buffer::SubRange,
    ) -> Result<r::BufferView, buffer::ViewCreationError> {
        let buffer = buffer.expect_bound();
        let buffer_features = {
            let idx = format.map(|fmt| fmt as usize).unwrap_or(0);
            self.format_properties
                .resolve(idx)
                .properties
                .buffer_features
        };
        let (format, format_desc) = match format.and_then(conv::map_format) {
            Some(fmt) => (fmt, format.unwrap().surface_desc()),
            None => return Err(buffer::ViewCreationError::UnsupportedFormat(format)),
        };

        let start = sub.offset;
        let size = sub.size.unwrap_or(buffer.requirements.size - start);

        let bytes_per_texel = (format_desc.bits / 8) as u64;
        // Check if it adheres to the texel buffer offset limit
        assert_eq!(start % bytes_per_texel, 0);
        let first_element = start / bytes_per_texel;
        let num_elements = size / bytes_per_texel; // rounds down to next smaller size

        let handle_srv = if buffer_features.contains(format::BufferFeature::UNIFORM_TEXEL) {
            let mut desc = d3d12::D3D12_SHADER_RESOURCE_VIEW_DESC {
                Format: format,
                ViewDimension: d3d12::D3D12_SRV_DIMENSION_BUFFER,
                Shader4ComponentMapping: IDENTITY_MAPPING,
                u: mem::zeroed(),
            };

            *desc.u.Buffer_mut() = d3d12::D3D12_BUFFER_SRV {
                FirstElement: first_element,
                NumElements: num_elements as _,
                StructureByteStride: bytes_per_texel as _,
                Flags: d3d12::D3D12_BUFFER_SRV_FLAG_NONE,
            };

            let handle = self.srv_uav_pool.lock().alloc_handle();
            self.raw.clone().CreateShaderResourceView(
                buffer.resource.as_mut_ptr(),
                &desc,
                handle.raw,
            );
            Some(handle)
        } else {
            None
        };

        let handle_uav = if buffer_features.intersects(
            format::BufferFeature::STORAGE_TEXEL | format::BufferFeature::STORAGE_TEXEL_ATOMIC,
        ) {
            let mut desc = d3d12::D3D12_UNORDERED_ACCESS_VIEW_DESC {
                Format: format,
                ViewDimension: d3d12::D3D12_UAV_DIMENSION_BUFFER,
                u: mem::zeroed(),
            };

            *desc.u.Buffer_mut() = d3d12::D3D12_BUFFER_UAV {
                FirstElement: first_element,
                NumElements: num_elements as _,
                StructureByteStride: bytes_per_texel as _,
                Flags: d3d12::D3D12_BUFFER_UAV_FLAG_NONE,
                CounterOffsetInBytes: 0,
            };

            let handle = self.srv_uav_pool.lock().alloc_handle();
            self.raw.clone().CreateUnorderedAccessView(
                buffer.resource.as_mut_ptr(),
                ptr::null_mut(),
                &desc,
                handle.raw,
            );
            Some(handle)
        } else {
            None
        };

        return Ok(r::BufferView {
            handle_srv,
            handle_uav,
        });
    }

    unsafe fn create_image(
        &self,
        kind: image::Kind,
        mip_levels: image::Level,
        format: format::Format,
        tiling: image::Tiling,
        usage: image::Usage,
        sparse: memory::SparseFlags,
        view_caps: image::ViewCapabilities,
    ) -> Result<r::Image, image::CreationError> {
        assert!(mip_levels <= kind.compute_num_levels());

        let base_format = format.base_format();
        let format_desc = base_format.0.desc();
        let bytes_per_block = (format_desc.bits / 8) as _;
        let block_dim = format_desc.dim;
        let view_format = conv::map_format(format);
        let extent = kind.extent();

        let format_info = self.format_properties.resolve(format as usize);
        let (layout, features) = if sparse.contains(memory::SparseFlags::SPARSE_BINDING) {
            (
                d3d12::D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE,
                format_info.properties.optimal_tiling,
            )
        } else {
            match tiling {
                image::Tiling::Optimal => (
                    d3d12::D3D12_TEXTURE_LAYOUT_UNKNOWN,
                    format_info.properties.optimal_tiling,
                ),
                image::Tiling::Linear => (
                    d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                    format_info.properties.linear_tiling,
                ),
            }
        };
        if format_info.sample_count_mask & kind.num_samples() == 0 {
            return Err(image::CreationError::Samples(kind.num_samples()));
        }

        let desc = d3d12::D3D12_RESOURCE_DESC {
            Dimension: match kind {
                image::Kind::D1(..) => d3d12::D3D12_RESOURCE_DIMENSION_TEXTURE1D,
                image::Kind::D2(..) => d3d12::D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                image::Kind::D3(..) => d3d12::D3D12_RESOURCE_DIMENSION_TEXTURE3D,
            },
            Alignment: 0,
            Width: extent.width as _,
            Height: extent.height as _,
            DepthOrArraySize: if extent.depth > 1 {
                extent.depth as _
            } else {
                kind.num_layers() as _
            },
            MipLevels: mip_levels as _,
            Format: if format_desc.is_compressed() {
                view_format.unwrap()
            } else {
                match conv::map_surface_type(base_format.0) {
                    Some(format) => format,
                    None => return Err(image::CreationError::Format(format)),
                }
            },
            SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                Count: kind.num_samples() as _,
                Quality: 0,
            },
            Layout: layout,
            Flags: conv::map_image_flags(usage, features),
        };

        let alloc_info = self.raw.clone().GetResourceAllocationInfo(0, 1, &desc);

        // Image flags which require RT/DS heap due to internal implementation.
        let target_flags = d3d12::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            | d3d12::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        let type_mask_shift = if self.private_caps.heterogeneous_resource_heaps {
            MEM_TYPE_UNIVERSAL_SHIFT
        } else if desc.Flags & target_flags != 0 {
            MEM_TYPE_TARGET_SHIFT
        } else {
            MEM_TYPE_IMAGE_SHIFT
        };

        Ok(r::Image::Unbound(r::ImageUnbound {
            view_format,
            dsv_format: conv::map_format_dsv(base_format.0),
            desc,
            requirements: memory::Requirements {
                size: alloc_info.SizeInBytes,
                alignment: alloc_info.Alignment,
                type_mask: MEM_TYPE_MASK << type_mask_shift,
            },
            format,
            kind,
            mip_levels,
            usage,
            tiling,
            view_caps,
            bytes_per_block,
            block_dim,
            name: None,
        }))
    }

    unsafe fn get_image_requirements(&self, image: &r::Image) -> Requirements {
        match image {
            r::Image::Bound(i) => i.requirements,
            r::Image::Unbound(i) => i.requirements,
        }
    }

    unsafe fn get_image_subresource_footprint(
        &self,
        image: &r::Image,
        sub: image::Subresource,
    ) -> image::SubresourceFootprint {
        let mut num_rows = 0;
        let mut total_bytes = 0;
        let _desc = match image {
            r::Image::Bound(i) => i.descriptor,
            r::Image::Unbound(i) => i.desc,
        };
        let footprint = {
            let mut footprint = mem::zeroed();
            self.raw.GetCopyableFootprints(
                image.get_desc(),
                image.calc_subresource(sub.level as _, sub.layer as _, 0),
                1,
                0,
                &mut footprint,
                &mut num_rows,
                ptr::null_mut(), // row size in bytes
                &mut total_bytes,
            );
            footprint
        };

        let depth_pitch = (footprint.Footprint.RowPitch * num_rows) as buffer::Offset;
        let array_pitch = footprint.Footprint.Depth as buffer::Offset * depth_pitch;
        image::SubresourceFootprint {
            slice: footprint.Offset..footprint.Offset + total_bytes,
            row_pitch: footprint.Footprint.RowPitch as _,
            depth_pitch,
            array_pitch,
        }
    }

    unsafe fn bind_image_memory(
        &self,
        memory: &r::Memory,
        offset: u64,
        image: &mut r::Image,
    ) -> Result<(), d::BindError> {
        let image_unbound = image.expect_unbound();
        if image_unbound.requirements.type_mask & (1 << memory.type_id) == 0 {
            error!(
                "Bind memory failure: supported mask 0x{:x}, given id {}",
                image_unbound.requirements.type_mask, memory.type_id
            );
            return Err(d::BindError::WrongMemory);
        }
        if offset + image_unbound.requirements.size > memory.size {
            return Err(d::BindError::OutOfBounds);
        }

        let mut resource = native::Resource::null();

        assert_eq!(
            winerror::S_OK,
            self.raw.clone().CreatePlacedResource(
                memory.heap.as_mut_ptr(),
                offset,
                &image_unbound.desc,
                d3d12::D3D12_RESOURCE_STATE_COMMON,
                ptr::null(),
                &d3d12::ID3D12Resource::uuidof(),
                resource.mut_void(),
            )
        );

        if let Some(ref name) = image_unbound.name {
            resource.SetName(name.as_ptr());
        }

        self.bind_image_resource(
            resource,
            image,
            r::Place::Heap {
                raw: memory.heap.clone(),
                offset,
            },
        );

        Ok(())
    }

    unsafe fn create_image_view(
        &self,
        image: &r::Image,
        view_kind: image::ViewKind,
        format: format::Format,
        swizzle: format::Swizzle,
        range: image::SubresourceRange,
    ) -> Result<r::ImageView, image::ViewCreationError> {
        let image = image.expect_bound();
        let is_array = image.kind.num_layers() > 1;
        let mip_levels = (
            range.level_start,
            range.level_start + range.resolve_level_count(image.mip_levels),
        );
        let layers = (
            range.layer_start,
            range.layer_start + range.resolve_layer_count(image.kind.num_layers()),
        );
        let surface_format = format.base_format().0;

        let info = ViewInfo {
            resource: image.resource,
            kind: image.kind,
            caps: image.view_caps,
            // D3D12 doesn't allow looking at a single slice of an array as a non-array
            view_kind: if is_array && view_kind == image::ViewKind::D2 {
                image::ViewKind::D2Array
            } else if is_array && view_kind == image::ViewKind::D1 {
                image::ViewKind::D1Array
            } else {
                view_kind
            },
            format: conv::map_format(format).ok_or(image::ViewCreationError::BadFormat(format))?,
            component_mapping: conv::map_swizzle(swizzle),
            levels: mip_levels.0..mip_levels.1,
            layers: layers.0..layers.1,
        };

        //Note: we allow RTV/DSV/SRV/UAV views to fail to be created here,
        // because we don't know if the user will even need to use them.

        Ok(r::ImageView {
            resource: image.resource,
            handle_srv: if image
                .usage
                .intersects(image::Usage::SAMPLED | image::Usage::INPUT_ATTACHMENT)
            {
                let info = if range.aspects.contains(format::Aspects::DEPTH) {
                    conv::map_format_shader_depth(surface_format).map(|format| ViewInfo {
                        format,
                        ..info.clone()
                    })
                } else if range.aspects.contains(format::Aspects::STENCIL) {
                    // Vulkan/gfx expects stencil to be read from the R channel,
                    // while DX12 exposes it in "G" always.
                    let new_swizzle = conv::swizzle_rg(swizzle);
                    conv::map_format_shader_stencil(surface_format).map(|format| ViewInfo {
                        format,
                        component_mapping: conv::map_swizzle(new_swizzle),
                        ..info.clone()
                    })
                } else {
                    Some(info.clone())
                };
                if let Some(ref info) = info {
                    self.view_image_as_shader_resource(&info).ok()
                } else {
                    None
                }
            } else {
                None
            },
            handle_rtv: if image.usage.contains(image::Usage::COLOR_ATTACHMENT) {
                // This view is not necessarily going to be rendered to, even
                // if the image supports that in general.
                match self.view_image_as_render_target(&info) {
                    Ok(handle) => r::RenderTargetHandle::Pool(handle),
                    Err(_) => r::RenderTargetHandle::None,
                }
            } else {
                r::RenderTargetHandle::None
            },
            handle_uav: if image.usage.contains(image::Usage::STORAGE) {
                self.view_image_as_storage(&info).ok()
            } else {
                None
            },
            handle_dsv: if image.usage.contains(image::Usage::DEPTH_STENCIL_ATTACHMENT) {
                match conv::map_format_dsv(surface_format) {
                    Some(dsv_format) => self
                        .view_image_as_depth_stencil(&ViewInfo {
                            format: dsv_format,
                            ..info
                        })
                        .ok(),
                    None => None,
                }
            } else {
                None
            },
            dxgi_format: image.default_view_format.unwrap(),
            num_levels: image.descriptor.MipLevels as image::Level,
            mip_levels,
            layers,
            kind: info.kind,
        })
    }

    unsafe fn create_sampler(
        &self,
        info: &image::SamplerDesc,
    ) -> Result<r::Sampler, d::AllocationError> {
        if !info.normalized {
            warn!("Sampler with unnormalized coordinates is not supported!");
        }
        let handle = match self.samplers.map.lock().entry(info.clone()) {
            Entry::Occupied(e) => *e.get(),
            Entry::Vacant(e) => {
                let handle = self.samplers.pool.lock().alloc_handle();
                let info = e.key();
                let op = match info.comparison {
                    Some(_) => d3d12::D3D12_FILTER_REDUCTION_TYPE_COMPARISON,
                    None => d3d12::D3D12_FILTER_REDUCTION_TYPE_STANDARD,
                };
                self.raw.create_sampler(
                    handle.raw,
                    conv::map_filter(
                        info.mag_filter,
                        info.min_filter,
                        info.mip_filter,
                        op,
                        info.anisotropy_clamp,
                    ),
                    [
                        conv::map_wrap(info.wrap_mode.0),
                        conv::map_wrap(info.wrap_mode.1),
                        conv::map_wrap(info.wrap_mode.2),
                    ],
                    info.lod_bias.0,
                    info.anisotropy_clamp.map_or(0, |aniso| aniso as u32),
                    conv::map_comparison(info.comparison.unwrap_or(pso::Comparison::Always)),
                    info.border.into(),
                    info.lod_range.start.0..info.lod_range.end.0,
                );
                *e.insert(handle)
            }
        };
        Ok(r::Sampler { handle })
    }

    unsafe fn create_descriptor_pool<I>(
        &self,
        max_sets: usize,
        ranges: I,
        _flags: pso::DescriptorPoolCreateFlags,
    ) -> Result<r::DescriptorPool, d::OutOfMemory>
    where
        I: Iterator<Item = pso::DescriptorRangeDesc>,
    {
        // Descriptor pools are implemented as slices of the global descriptor heaps.
        // A descriptor pool will occupy a contiguous space in each heap (CBV/SRV/UAV and Sampler) depending
        // on the total requested amount of descriptors.

        let mut num_srv_cbv_uav = 0;
        let mut num_samplers = 0;

        let ranges = ranges.collect::<Vec<_>>();

        info!("create_descriptor_pool with {} max sets", max_sets);
        for desc in &ranges {
            let content = r::DescriptorContent::from(desc.ty);
            debug!("\tcontent {:?}", content);
            if content.contains(r::DescriptorContent::CBV) {
                num_srv_cbv_uav += desc.count;
            }
            if content.contains(r::DescriptorContent::SRV) {
                num_srv_cbv_uav += desc.count;
            }
            if content.contains(r::DescriptorContent::UAV) {
                num_srv_cbv_uav += desc.count;
            }
            if content.contains(r::DescriptorContent::SAMPLER) {
                num_samplers += desc.count;
            }
        }

        info!(
            "total {} views and {} samplers",
            num_srv_cbv_uav, num_samplers
        );

        // Allocate slices of the global GPU descriptor heaps.
        let heap_srv_cbv_uav = {
            let view_heap = &self.heap_srv_cbv_uav.0;

            let range = match num_srv_cbv_uav {
                0 => 0..0,
                _ => self
                    .heap_srv_cbv_uav
                    .1
                    .lock()
                    .allocate_range(num_srv_cbv_uav as _)
                    .map_err(|e| {
                        warn!("View pool allocation error: {:?}", e);
                        d::OutOfMemory::Host
                    })?,
            };

            r::DescriptorHeapSlice {
                heap: view_heap.raw.clone(),
                handle_size: view_heap.handle_size as _,
                range_allocator: RangeAllocator::new(range),
                start: view_heap.start,
            }
        };

        Ok(r::DescriptorPool {
            heap_srv_cbv_uav,
            heap_raw_sampler: self.samplers.heap.raw,
            pools: ranges,
            max_size: max_sets as _,
        })
    }

    unsafe fn create_descriptor_set_layout<'a, I, J>(
        &self,
        bindings: I,
        _immutable_samplers: J,
    ) -> Result<r::DescriptorSetLayout, d::OutOfMemory>
    where
        I: Iterator<Item = pso::DescriptorSetLayoutBinding>,
        J: Iterator<Item = &'a r::Sampler>,
    {
        Ok(r::DescriptorSetLayout {
            bindings: bindings.collect(),
        })
    }

    unsafe fn write_descriptor_set<'a, I>(&self, op: pso::DescriptorSetWrite<'a, B, I>)
    where
        I: Iterator<Item = pso::Descriptor<'a, B>>,
    {
        let mut descriptor_updater = self.descriptor_updater.lock();
        descriptor_updater.reset();

        let mut accum = descriptors_cpu::MultiCopyAccumulator::default();
        debug!("write_descriptor_set");

        let mut offset = op.array_offset as u64;
        let mut target_binding = op.binding as usize;
        let base_sampler_offset = op.set.sampler_offset(op.binding, op.array_offset);
        trace!("\tsampler offset {}", base_sampler_offset);
        let mut sampler_offset = base_sampler_offset;
        debug!("\tbinding {} array offset {}", target_binding, offset);

        for descriptor in op.descriptors {
            // spill over the writes onto the next binding
            while offset >= op.set.binding_infos[target_binding].count {
                target_binding += 1;
                offset = 0;
            }
            let bind_info = &mut op.set.binding_infos[target_binding];
            let mut src_cbv = None;
            let mut src_srv = None;
            let mut src_uav = None;

            match descriptor {
                pso::Descriptor::Buffer(buffer, ref sub) => {
                    let buffer = buffer.expect_bound();

                    if bind_info.content.is_dynamic() {
                        // Root Descriptor
                        let buffer_address = (*buffer.resource).GetGPUVirtualAddress();
                        // Descriptor sets need to be externally synchronized according to specification
                        bind_info.dynamic_descriptors[offset as usize].gpu_buffer_location =
                            buffer_address + sub.offset;
                    } else {
                        // Descriptor table
                        let size = sub.size_to(buffer.requirements.size);

                        if bind_info.content.contains(r::DescriptorContent::CBV) {
                            // Making the size field of buffer requirements for uniform
                            // buffers a multiple of 256 and setting the required offset
                            // alignment to 256 allows us to patch the size here.
                            // We can always enforce the size to be aligned to 256 for
                            // CBVs without going out-of-bounds.
                            let mask = d3d12::D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1;
                            let desc = d3d12::D3D12_CONSTANT_BUFFER_VIEW_DESC {
                                BufferLocation: (*buffer.resource).GetGPUVirtualAddress()
                                    + sub.offset,
                                SizeInBytes: (size as u32 + mask) as u32 & !mask,
                            };
                            let handle = descriptor_updater.alloc_handle(self.raw);
                            self.raw.CreateConstantBufferView(&desc, handle);
                            src_cbv = Some(handle);
                        }
                        if bind_info.content.contains(r::DescriptorContent::SRV) {
                            assert_eq!(size % 4, 0);
                            let mut desc = d3d12::D3D12_SHADER_RESOURCE_VIEW_DESC {
                                Format: dxgiformat::DXGI_FORMAT_R32_TYPELESS,
                                Shader4ComponentMapping: IDENTITY_MAPPING,
                                ViewDimension: d3d12::D3D12_SRV_DIMENSION_BUFFER,
                                u: mem::zeroed(),
                            };
                            *desc.u.Buffer_mut() = d3d12::D3D12_BUFFER_SRV {
                                FirstElement: sub.offset as _,
                                NumElements: (size / 4) as _,
                                StructureByteStride: 0,
                                Flags: d3d12::D3D12_BUFFER_SRV_FLAG_RAW,
                            };
                            let handle = descriptor_updater.alloc_handle(self.raw);
                            self.raw.CreateShaderResourceView(
                                buffer.resource.as_mut_ptr(),
                                &desc,
                                handle,
                            );
                            src_srv = Some(handle);
                        }
                        if bind_info.content.contains(r::DescriptorContent::UAV) {
                            assert_eq!(size % 4, 0);
                            let mut desc = d3d12::D3D12_UNORDERED_ACCESS_VIEW_DESC {
                                Format: dxgiformat::DXGI_FORMAT_R32_TYPELESS,
                                ViewDimension: d3d12::D3D12_UAV_DIMENSION_BUFFER,
                                u: mem::zeroed(),
                            };
                            *desc.u.Buffer_mut() = d3d12::D3D12_BUFFER_UAV {
                                FirstElement: sub.offset as _,
                                NumElements: (size / 4) as _,
                                StructureByteStride: 0,
                                CounterOffsetInBytes: 0,
                                Flags: d3d12::D3D12_BUFFER_UAV_FLAG_RAW,
                            };
                            let handle = descriptor_updater.alloc_handle(self.raw);
                            self.raw.CreateUnorderedAccessView(
                                buffer.resource.as_mut_ptr(),
                                ptr::null_mut(),
                                &desc,
                                handle,
                            );
                            src_uav = Some(handle);
                        }
                    }
                }
                pso::Descriptor::Image(image, _layout) => {
                    if bind_info.content.contains(r::DescriptorContent::SRV) {
                        src_srv = image.handle_srv.map(|h| h.raw);
                    }
                    if bind_info.content.contains(r::DescriptorContent::UAV) {
                        src_uav = image.handle_uav.map(|h| h.raw);
                    }
                }
                pso::Descriptor::CombinedImageSampler(image, _layout, sampler) => {
                    src_srv = image.handle_srv.map(|h| h.raw);
                    op.set.sampler_origins[sampler_offset] = sampler.handle.raw;
                    sampler_offset += 1;
                }
                pso::Descriptor::Sampler(sampler) => {
                    op.set.sampler_origins[sampler_offset] = sampler.handle.raw;
                    sampler_offset += 1;
                }
                pso::Descriptor::TexelBuffer(buffer_view) => {
                    if bind_info.content.contains(r::DescriptorContent::SRV) {
                        let handle = buffer_view.handle_srv
                            .expect("SRV handle of the storage texel buffer is zero (not supported by specified format)");
                        src_srv = Some(handle.raw);
                    }
                    if bind_info.content.contains(r::DescriptorContent::UAV) {
                        let handle = buffer_view.handle_uav
                            .expect("UAV handle of the storage texel buffer is zero (not supported by specified format)");
                        src_uav = Some(handle.raw);
                    }
                }
            }

            if let Some(handle) = src_cbv {
                trace!("\tcbv offset {}", offset);
                accum.src_views.add(handle, 1);
                accum
                    .dst_views
                    .add(bind_info.view_range.as_ref().unwrap().at(offset), 1);
            }
            if let Some(handle) = src_srv {
                trace!("\tsrv offset {}", offset);
                accum.src_views.add(handle, 1);
                accum
                    .dst_views
                    .add(bind_info.view_range.as_ref().unwrap().at(offset), 1);
            }
            if let Some(handle) = src_uav {
                let uav_offset = if bind_info.content.contains(r::DescriptorContent::SRV) {
                    bind_info.count + offset
                } else {
                    offset
                };
                trace!("\tuav offset {}", uav_offset);
                accum.src_views.add(handle, 1);
                accum
                    .dst_views
                    .add(bind_info.view_range.as_ref().unwrap().at(uav_offset), 1);
            }

            offset += 1;
        }

        if sampler_offset != base_sampler_offset {
            op.set
                .update_samplers(&self.samplers.heap, &self.samplers.origins, &mut accum);
        }

        accum.flush(self.raw);
    }

    unsafe fn copy_descriptor_set<'a>(&self, op: pso::DescriptorSetCopy<'a, B>) {
        let mut accum = descriptors_cpu::MultiCopyAccumulator::default();

        let src_info = &op.src_set.binding_infos[op.src_binding as usize];
        let dst_info = &op.dst_set.binding_infos[op.dst_binding as usize];

        if let (Some(src_range), Some(dst_range)) =
            (src_info.view_range.as_ref(), dst_info.view_range.as_ref())
        {
            assert!(op.src_array_offset + op.count <= src_range.handle.size as usize);
            assert!(op.dst_array_offset + op.count <= dst_range.handle.size as usize);
            let count = op.count as u32;
            accum
                .src_views
                .add(src_range.at(op.src_array_offset as _), count);
            accum
                .dst_views
                .add(dst_range.at(op.dst_array_offset as _), count);

            if (src_info.content & dst_info.content)
                .contains(r::DescriptorContent::SRV | r::DescriptorContent::UAV)
            {
                assert!(
                    src_info.count as usize + op.src_array_offset + op.count
                        <= src_range.handle.size as usize
                );
                assert!(
                    dst_info.count as usize + op.dst_array_offset + op.count
                        <= dst_range.handle.size as usize
                );
                accum.src_views.add(
                    src_range.at(src_info.count + op.src_array_offset as u64),
                    count,
                );
                accum.dst_views.add(
                    dst_range.at(dst_info.count + op.dst_array_offset as u64),
                    count,
                );
            }
        }

        if dst_info.content.contains(r::DescriptorContent::SAMPLER) {
            let src_offset = op
                .src_set
                .sampler_offset(op.src_binding, op.src_array_offset);
            let dst_offset = op
                .dst_set
                .sampler_offset(op.dst_binding, op.dst_array_offset);
            op.dst_set.sampler_origins[dst_offset..dst_offset + op.count]
                .copy_from_slice(&op.src_set.sampler_origins[src_offset..src_offset + op.count]);

            op.dst_set
                .update_samplers(&self.samplers.heap, &self.samplers.origins, &mut accum);
        }

        accum.flush(self.raw.clone());
    }

    unsafe fn map_memory(
        &self,
        memory: &mut r::Memory,
        segment: memory::Segment,
    ) -> Result<*mut u8, d::MapError> {
        let mem = memory
            .resource
            .expect("Memory not created with a memory type exposing `CPU_VISIBLE`");
        let mut ptr = ptr::null_mut();
        assert_eq!(
            winerror::S_OK,
            (*mem).Map(0, &d3d12::D3D12_RANGE { Begin: 0, End: 0 }, &mut ptr)
        );
        ptr = ptr.offset(segment.offset as isize);
        Ok(ptr as *mut _)
    }

    unsafe fn unmap_memory(&self, memory: &mut r::Memory) {
        if let Some(mem) = memory.resource {
            (*mem).Unmap(0, &d3d12::D3D12_RANGE { Begin: 0, End: 0 });
        }
    }

    unsafe fn flush_mapped_memory_ranges<'a, I>(&self, ranges: I) -> Result<(), d::OutOfMemory>
    where
        I: Iterator<Item = (&'a r::Memory, memory::Segment)>,
    {
        for (memory, ref segment) in ranges {
            if let Some(mem) = memory.resource {
                // map and immediately unmap, hoping that dx12 drivers internally cache
                // currently mapped buffers.
                assert_eq!(
                    winerror::S_OK,
                    (*mem).Map(0, &d3d12::D3D12_RANGE { Begin: 0, End: 0 }, ptr::null_mut())
                );

                let start = segment.offset;
                let end = segment.size.map_or(memory.size, |s| start + s); // TODO: only need to be end of current mapping

                (*mem).Unmap(
                    0,
                    &d3d12::D3D12_RANGE {
                        Begin: start as _,
                        End: end as _,
                    },
                );
            }
        }

        Ok(())
    }

    unsafe fn invalidate_mapped_memory_ranges<'a, I>(&self, ranges: I) -> Result<(), d::OutOfMemory>
    where
        I: Iterator<Item = (&'a r::Memory, memory::Segment)>,
    {
        for (memory, ref segment) in ranges {
            if let Some(mem) = memory.resource {
                let start = segment.offset;
                let end = segment.size.map_or(memory.size, |s| start + s); // TODO: only need to be end of current mapping

                // map and immediately unmap, hoping that dx12 drivers internally cache
                // currently mapped buffers.
                assert_eq!(
                    winerror::S_OK,
                    (*mem).Map(
                        0,
                        &d3d12::D3D12_RANGE {
                            Begin: start as _,
                            End: end as _,
                        },
                        ptr::null_mut(),
                    )
                );

                (*mem).Unmap(0, &d3d12::D3D12_RANGE { Begin: 0, End: 0 });
            }
        }

        Ok(())
    }

    fn create_semaphore(&self) -> Result<r::Semaphore, d::OutOfMemory> {
        let fence = self.create_fence(false)?;
        Ok(r::Semaphore { raw: fence.raw })
    }

    fn create_fence(&self, signalled: bool) -> Result<r::Fence, d::OutOfMemory> {
        Ok(r::Fence {
            raw: self.create_raw_fence(signalled),
        })
    }

    unsafe fn reset_fence(&self, fence: &mut r::Fence) -> Result<(), d::OutOfMemory> {
        assert_eq!(winerror::S_OK, fence.raw.signal(0));
        Ok(())
    }

    unsafe fn wait_for_fences<'a, I>(
        &self,
        fences: I,
        wait: d::WaitFor,
        timeout_ns: u64,
    ) -> Result<bool, d::WaitError>
    where
        I: Iterator<Item = &'a r::Fence>,
    {
        let mut count = 0;
        let mut events = self.events.lock();

        for fence in fences {
            if count == events.len() {
                events.push(native::Event::create(false, false));
            }
            let event = events[count];
            synchapi::ResetEvent(event.0);
            assert_eq!(winerror::S_OK, fence.raw.set_event_on_completion(event, 1));
            count += 1;
        }

        let all = match wait {
            d::WaitFor::Any => FALSE,
            d::WaitFor::All => TRUE,
        };

        let hr = {
            // This block handles overflow when converting to u32 and always rounds up
            // The Vulkan specification allows to wait more than specified
            let timeout_ms = {
                if timeout_ns > (<u32>::max_value() as u64) * 1_000_000 {
                    <u32>::max_value()
                } else {
                    ((timeout_ns + 999_999) / 1_000_000) as u32
                }
            };

            synchapi::WaitForMultipleObjects(
                count as u32,
                events.as_ptr() as *const _,
                all,
                timeout_ms,
            )
        };

        const WAIT_OBJECT_LAST: u32 = winbase::WAIT_OBJECT_0 + winnt::MAXIMUM_WAIT_OBJECTS;
        const WAIT_ABANDONED_LAST: u32 = winbase::WAIT_ABANDONED_0 + winnt::MAXIMUM_WAIT_OBJECTS;
        match hr {
            winbase::WAIT_OBJECT_0..=WAIT_OBJECT_LAST => Ok(true),
            winbase::WAIT_ABANDONED_0..=WAIT_ABANDONED_LAST => Ok(true), //TODO?
            winbase::WAIT_FAILED => Err(d::WaitError::DeviceLost(d::DeviceLost)),
            winerror::WAIT_TIMEOUT => Ok(false),
            _ => panic!("Unexpected wait status 0x{:X}", hr),
        }
    }

    unsafe fn get_fence_status(&self, fence: &r::Fence) -> Result<bool, d::DeviceLost> {
        match fence.raw.GetCompletedValue() {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err(d::DeviceLost),
        }
    }

    fn create_event(&self) -> Result<(), d::OutOfMemory> {
        unimplemented!()
    }

    unsafe fn get_event_status(&self, _event: &()) -> Result<bool, d::WaitError> {
        unimplemented!()
    }

    unsafe fn set_event(&self, _event: &mut ()) -> Result<(), d::OutOfMemory> {
        unimplemented!()
    }

    unsafe fn reset_event(&self, _event: &mut ()) -> Result<(), d::OutOfMemory> {
        unimplemented!()
    }

    unsafe fn free_memory(&self, memory: r::Memory) {
        memory.heap.destroy();
        if let Some(buffer) = memory.resource {
            buffer.destroy();
        }
    }

    unsafe fn create_query_pool(
        &self,
        query_ty: query::Type,
        count: query::Id,
    ) -> Result<r::QueryPool, query::CreationError> {
        let heap_ty = match query_ty {
            query::Type::Occlusion => native::QueryHeapType::Occlusion,
            query::Type::PipelineStatistics(_) => native::QueryHeapType::PipelineStatistics,
            query::Type::Timestamp => native::QueryHeapType::Timestamp,
        };

        let (query_heap, hr) = self.raw.create_query_heap(heap_ty, count, 0);
        assert_eq!(winerror::S_OK, hr);

        Ok(r::QueryPool {
            raw: query_heap,
            ty: query_ty,
        })
    }

    unsafe fn destroy_query_pool(&self, pool: r::QueryPool) {
        pool.raw.destroy();
    }

    unsafe fn get_query_pool_results(
        &self,
        pool: &r::QueryPool,
        queries: Range<query::Id>,
        data: &mut [u8],
        stride: buffer::Stride,
        flags: query::ResultFlags,
    ) -> Result<bool, d::WaitError> {
        let num_queries = queries.end - queries.start;
        let size = 8 * num_queries as u64;
        let buffer_desc = d3d12::D3D12_RESOURCE_DESC {
            Dimension: d3d12::D3D12_RESOURCE_DIMENSION_BUFFER,
            Alignment: 0,
            Width: size,
            Height: 1,
            DepthOrArraySize: 1,
            MipLevels: 1,
            Format: dxgiformat::DXGI_FORMAT_UNKNOWN,
            SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Layout: d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            Flags: d3d12::D3D12_RESOURCE_FLAG_NONE,
        };

        let properties = d3d12::D3D12_HEAP_PROPERTIES {
            Type: d3d12::D3D12_HEAP_TYPE_READBACK,
            CPUPageProperty: d3d12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            MemoryPoolPreference: d3d12::D3D12_MEMORY_POOL_UNKNOWN,
            CreationNodeMask: 0,
            VisibleNodeMask: 0,
        };

        let heap_desc = d3d12::D3D12_HEAP_DESC {
            SizeInBytes: size,
            Properties: properties,
            Alignment: 0,
            Flags: d3d12::D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
        };

        let mut heap = native::Heap::null();
        assert_eq!(
            self.raw
                .clone()
                .CreateHeap(&heap_desc, &d3d12::ID3D12Heap::uuidof(), heap.mut_void()),
            winerror::S_OK
        );

        let mut temp_buffer = native::Resource::null();
        assert_eq!(
            winerror::S_OK,
            self.raw.clone().CreatePlacedResource(
                heap.as_mut_ptr(),
                0,
                &buffer_desc,
                d3d12::D3D12_RESOURCE_STATE_COPY_DEST,
                ptr::null(),
                &d3d12::ID3D12Resource::uuidof(),
                temp_buffer.mut_void(),
            )
        );

        let list_type = native::CmdListType::Direct;
        let (com_allocator, hr_alloc) = self.raw.create_command_allocator(list_type);
        assert_eq!(
            winerror::S_OK,
            hr_alloc,
            "error on command allocator creation: {:x}",
            hr_alloc
        );
        let (com_list, hr_list) = self.raw.create_graphics_command_list(
            list_type,
            com_allocator,
            native::PipelineState::null(),
            0,
        );
        assert_eq!(
            winerror::S_OK,
            hr_list,
            "error on command list creation: {:x}",
            hr_list
        );

        let query_ty = match pool.ty {
            query::Type::Occlusion => d3d12::D3D12_QUERY_TYPE_OCCLUSION,
            query::Type::PipelineStatistics(_) => d3d12::D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
            query::Type::Timestamp => d3d12::D3D12_QUERY_TYPE_TIMESTAMP,
        };
        com_list.ResolveQueryData(
            pool.raw.as_mut_ptr(),
            query_ty,
            queries.start,
            num_queries,
            temp_buffer.as_mut_ptr(),
            0,
        );
        com_list.close();

        self.queues[0]
            .raw
            .ExecuteCommandLists(1, &(com_list.as_mut_ptr() as *mut _));

        if !flags.contains(query::ResultFlags::WAIT) {
            warn!("Can't really not wait here")
        }
        let result = self.queues[0].wait_idle_impl();
        if result.is_ok() {
            let mut ptr = ptr::null_mut();
            assert_eq!(
                winerror::S_OK,
                temp_buffer.Map(0, &d3d12::D3D12_RANGE { Begin: 0, End: 0 }, &mut ptr)
            );
            let src_data = slice::from_raw_parts(ptr as *const u64, num_queries as usize);
            for (i, &value) in src_data.iter().enumerate() {
                let dst = data.as_mut_ptr().add(i * stride as usize);
                if flags.contains(query::ResultFlags::BITS_64) {
                    *(dst as *mut u64) = value;
                } else {
                    *(dst as *mut u32) = value as u32;
                }
            }
            temp_buffer.Unmap(0, &d3d12::D3D12_RANGE { Begin: 0, End: 0 });
        }

        temp_buffer.destroy();
        heap.destroy();
        com_list.destroy();
        com_allocator.destroy();

        match result {
            Ok(()) => Ok(true),
            Err(e) => Err(e.into()),
        }
    }

    unsafe fn destroy_shader_module(&self, shader_lib: r::ShaderModule) {
        if let r::ShaderModule::Compiled(shaders) = shader_lib {
            for (_, blob) in shaders {
                blob.destroy();
            }
        }
    }

    unsafe fn destroy_render_pass(&self, _rp: r::RenderPass) {
        // Just drop
    }

    unsafe fn destroy_pipeline_layout(&self, layout: r::PipelineLayout) {
        layout.shared.signature.destroy();
    }

    unsafe fn destroy_graphics_pipeline(&self, pipeline: r::GraphicsPipeline) {
        pipeline.raw.destroy();
    }

    unsafe fn destroy_compute_pipeline(&self, pipeline: r::ComputePipeline) {
        pipeline.raw.destroy();
    }

    unsafe fn destroy_framebuffer(&self, _fb: r::Framebuffer) {
        // Just drop
    }

    unsafe fn destroy_buffer(&self, buffer: r::Buffer) {
        match buffer {
            r::Buffer::Bound(buffer) => {
                if let Some(handle) = buffer.clear_uav {
                    self.srv_uav_pool.lock().free_handle(handle);
                }
                buffer.resource.destroy();
            }
            r::Buffer::Unbound(_) => {}
        }
    }

    unsafe fn destroy_buffer_view(&self, view: r::BufferView) {
        let mut pool = self.srv_uav_pool.lock();
        if let Some(handle) = view.handle_srv {
            pool.free_handle(handle);
        }
        if let Some(handle) = view.handle_uav {
            pool.free_handle(handle);
        }
    }

    unsafe fn destroy_image(&self, image: r::Image) {
        match image {
            r::Image::Bound(image) => {
                let mut dsv_pool = self.dsv_pool.lock();
                for handle in image.clear_cv {
                    self.rtv_pool.lock().free_handle(handle);
                }
                for handle in image.clear_dv {
                    dsv_pool.free_handle(handle);
                }
                for handle in image.clear_sv {
                    dsv_pool.free_handle(handle);
                }
                image.resource.destroy();
            }
            r::Image::Unbound(_) => {}
        }
    }

    unsafe fn destroy_image_view(&self, view: r::ImageView) {
        if let Some(handle) = view.handle_srv {
            self.srv_uav_pool.lock().free_handle(handle);
        }
        if let Some(handle) = view.handle_uav {
            self.srv_uav_pool.lock().free_handle(handle);
        }
        if let r::RenderTargetHandle::Pool(handle) = view.handle_rtv {
            self.rtv_pool.lock().free_handle(handle);
        }
        if let Some(handle) = view.handle_dsv {
            self.dsv_pool.lock().free_handle(handle);
        }
    }

    unsafe fn destroy_sampler(&self, _sampler: r::Sampler) {
        // We don't destroy samplers, they are permanently cached
    }

    unsafe fn destroy_descriptor_pool(&self, pool: r::DescriptorPool) {
        let view_range = pool.heap_srv_cbv_uav.range_allocator.initial_range();
        if view_range.start < view_range.end {
            self.heap_srv_cbv_uav
                .1
                .lock()
                .free_range(view_range.clone());
        }
    }

    unsafe fn destroy_descriptor_set_layout(&self, _layout: r::DescriptorSetLayout) {
        // Just drop
    }

    unsafe fn destroy_fence(&self, fence: r::Fence) {
        fence.raw.destroy();
    }

    unsafe fn destroy_semaphore(&self, semaphore: r::Semaphore) {
        semaphore.raw.destroy();
    }

    unsafe fn destroy_event(&self, _event: ()) {
        unimplemented!()
    }

    fn wait_idle(&self) -> Result<(), d::OutOfMemory> {
        for queue in &self.queues {
            queue.wait_idle_impl()?;
        }
        Ok(())
    }

    unsafe fn set_image_name(&self, image: &mut r::Image, name: &str) {
        let cwstr = wide_cstr(name);
        match *image {
            r::Image::Unbound(ref mut image) => image.name = Some(cwstr),
            r::Image::Bound(ref image) => {
                image.resource.SetName(cwstr.as_ptr());
            }
        }
    }

    unsafe fn set_buffer_name(&self, buffer: &mut r::Buffer, name: &str) {
        let cwstr = wide_cstr(name);
        match *buffer {
            r::Buffer::Unbound(ref mut buffer) => buffer.name = Some(cwstr),
            r::Buffer::Bound(ref buffer) => {
                buffer.resource.SetName(cwstr.as_ptr());
            }
        }
    }

    unsafe fn set_command_buffer_name(&self, command_buffer: &mut cmd::CommandBuffer, name: &str) {
        let cwstr = wide_cstr(name);
        if !command_buffer.raw.is_null() {
            command_buffer.raw.SetName(cwstr.as_ptr());
        }
        command_buffer.raw_name = cwstr;
    }

    unsafe fn set_semaphore_name(&self, semaphore: &mut r::Semaphore, name: &str) {
        let cwstr = wide_cstr(name);
        semaphore.raw.SetName(cwstr.as_ptr());
    }

    unsafe fn set_fence_name(&self, fence: &mut r::Fence, name: &str) {
        let cwstr = wide_cstr(name);
        fence.raw.SetName(cwstr.as_ptr());
    }

    unsafe fn set_framebuffer_name(&self, _framebuffer: &mut r::Framebuffer, _name: &str) {
        // ignored
    }

    unsafe fn set_render_pass_name(&self, render_pass: &mut r::RenderPass, name: &str) {
        render_pass.raw_name = wide_cstr(name);
    }

    unsafe fn set_descriptor_set_name(&self, descriptor_set: &mut r::DescriptorSet, name: &str) {
        descriptor_set.raw_name = wide_cstr(name);
    }

    unsafe fn set_descriptor_set_layout_name(
        &self,
        _descriptor_set_layout: &mut r::DescriptorSetLayout,
        _name: &str,
    ) {
        // ignored
    }

    unsafe fn set_pipeline_layout_name(&self, pipeline_layout: &mut r::PipelineLayout, name: &str) {
        let cwstr = wide_cstr(name);
        pipeline_layout.shared.signature.SetName(cwstr.as_ptr());
    }

    fn start_capture(&self) {
        //TODO
    }

    fn stop_capture(&self) {
        //TODO
    }
}

#[test]
fn test_identity_mapping() {
    assert_eq!(conv::map_swizzle(format::Swizzle::NO), IDENTITY_MAPPING);
}
