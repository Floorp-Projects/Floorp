use std::{ffi, ptr, slice};

use spirv_cross::{hlsl, spirv, ErrorCode as SpirvErrorCode};

use winapi::{
    shared::winerror,
    um::{d3d11, d3dcommon, d3dcompiler},
};
use wio::com::ComPtr;

use auxil::{spirv_cross_specialize_ast, ShaderStage};
use hal::pso;

use crate::{conv, Backend, PipelineLayout};

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
/// This workaround also exists under the same name in the DX12 backend.
pub(crate) fn introspect_spirv_vertex_semantic_remapping(
    raw_data: &[u32],
) -> Result<auxil::FastHashMap<u32, Option<(u32, u32)>>, pso::CreationError> {
    const SHADER_STAGE: ShaderStage = ShaderStage::Vertex;
    // This is inefficient as we already parse it once before. This is a temporary workaround only called
    // on vertex shaders. If this becomes permanent or shows up in profiles, deduplicate these as first course of action.
    let ast = parse_spirv(SHADER_STAGE, raw_data)?;

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

pub(crate) fn compile_spirv_entrypoint(
    raw_data: &[u32],
    stage: ShaderStage,
    source: &pso::EntryPoint<Backend>,
    layout: &PipelineLayout,
    features: &hal::Features,
    device_feature_level: u32,
) -> Result<Option<ComPtr<d3dcommon::ID3DBlob>>, pso::CreationError> {
    let mut ast = parse_spirv(stage, raw_data)?;
    spirv_cross_specialize_ast(&mut ast, &source.specialization)
        .map_err(pso::CreationError::InvalidSpecialization)?;

    patch_spirv_resources(&mut ast, stage, layout)?;
    let shader_model = conv::map_feature_level_to_shader_model(device_feature_level);
    let shader_code = translate_spirv(
        &mut ast,
        shader_model,
        layout,
        stage,
        features,
        source.entry,
    )?;
    log::debug!("Generated {:?} shader:\n{}", stage, shader_code,);

    let real_name = ast
        .get_cleansed_entry_point_name(source.entry, conv::map_stage(stage))
        .map_err(|err| gen_query_error(stage, err))?;

    let shader = compile_hlsl_shader(stage, shader_model, &real_name, shader_code.as_bytes())?;
    Ok(Some(unsafe { ComPtr::from_raw(shader) }))
}

pub(crate) fn compile_hlsl_shader(
    stage: ShaderStage,
    shader_model: hlsl::ShaderModel,
    entry: &str,
    code: &[u8],
) -> Result<*mut d3dcommon::ID3DBlob, pso::CreationError> {
    let stage_str = {
        let stage = match stage {
            ShaderStage::Vertex => "vs",
            ShaderStage::Fragment => "ps",
            ShaderStage::Compute => "cs",
            _ => unimplemented!(),
        };

        let model = match shader_model {
            hlsl::ShaderModel::V4_0L9_1 => "4_0_level_9_1",
            hlsl::ShaderModel::V4_0L9_3 => "4_0_level_9_3",
            hlsl::ShaderModel::V4_0 => "4_0",
            hlsl::ShaderModel::V4_1 => "4_1",
            hlsl::ShaderModel::V5_0 => "5_0",
            // TODO: >= 11.3
            hlsl::ShaderModel::V5_1 => "5_1",
            // TODO: >= 12?, no mention of 11 on msdn
            hlsl::ShaderModel::V6_0 => "6_0",
            _ => unimplemented!(),
        };

        format!("{}_{}\0", stage, model)
    };

    let mut blob = ptr::null_mut();
    let mut error = ptr::null_mut();
    let entry = ffi::CString::new(entry).unwrap();
    let hr = unsafe {
        d3dcompiler::D3DCompile(
            code.as_ptr() as *const _,
            code.len(),
            ptr::null(),
            ptr::null(),
            ptr::null_mut(),
            entry.as_ptr() as *const _,
            stage_str.as_ptr() as *const i8,
            1,
            0,
            &mut blob as *mut *mut _,
            &mut error as *mut *mut _,
        )
    };

    if !winerror::SUCCEEDED(hr) {
        let error = unsafe { ComPtr::<d3dcommon::ID3DBlob>::from_raw(error) };
        let message = unsafe {
            let pointer = error.GetBufferPointer();
            let size = error.GetBufferSize();
            let slice = slice::from_raw_parts(pointer as *const u8, size as usize);
            String::from_utf8_lossy(slice).into_owned()
        };
        error!("D3DCompile error {:x}: {}", hr, message);
        Err(pso::CreationError::ShaderCreationError(
            stage.to_flag(),
            message,
        ))
    } else {
        Ok(blob)
    }
}

fn parse_spirv(
    stage: ShaderStage,
    raw_data: &[u32],
) -> Result<spirv::Ast<hlsl::Target>, pso::CreationError> {
    let module = spirv::Module::from_words(raw_data);

    match spirv::Ast::parse(&module) {
        Ok(ast) => Ok(ast),
        Err(err) => {
            let msg = match err {
                SpirvErrorCode::CompilationError(msg) => msg,
                SpirvErrorCode::Unhandled => "Unknown parsing error".into(),
            };
            let error = format!("SPIR-V parsing failed: {:?}", msg);
            Err(pso::CreationError::ShaderCreationError(
                stage.to_flag(),
                error,
            ))
        }
    }
}

fn patch_spirv_resources(
    ast: &mut spirv::Ast<hlsl::Target>,
    stage: ShaderStage,
    layout: &PipelineLayout,
) -> Result<(), pso::CreationError> {
    // we remap all `layout(binding = n, set = n)` to a flat space which we get from our
    // `PipelineLayout` which knows of all descriptor set layouts

    // With multi-entry-point shaders, we will end up with resources in the SPIRV that aren't part of the provided
    // pipeline layout. We need to be tolerant of getting out of bounds set and binding indices in the spirv which we can safely ignore.

    let shader_resources = ast
        .get_shader_resources()
        .map_err(|err| gen_query_error(stage, err))?;
    for image in &shader_resources.separate_images {
        let set = ast
            .get_decoration(image.id, spirv::Decoration::DescriptorSet)
            .map_err(|err| gen_query_error(stage, err))? as usize;
        let binding = ast
            .get_decoration(image.id, spirv::Decoration::Binding)
            .map_err(|err| gen_query_error(stage, err))?;

        let set_ref = match layout.sets.get(set) {
            Some(set) => set,
            None => continue,
        };

        if let Some((_content, res_index)) = set_ref.find_register(stage, binding) {
            ast.set_decoration(image.id, spirv::Decoration::Binding, res_index.t as u32)
                .map_err(|err| gen_unexpected_error(stage, err))?;
        }
    }

    for uniform_buffer in &shader_resources.uniform_buffers {
        let set = ast
            .get_decoration(uniform_buffer.id, spirv::Decoration::DescriptorSet)
            .map_err(|err| gen_query_error(stage, err))? as usize;
        let binding = ast
            .get_decoration(uniform_buffer.id, spirv::Decoration::Binding)
            .map_err(|err| gen_query_error(stage, err))?;

        let set_ref = match layout.sets.get(set) {
            Some(set) => set,
            None => continue,
        };

        if let Some((_content, res_index)) = set_ref.find_register(stage, binding) {
            ast.set_decoration(
                uniform_buffer.id,
                spirv::Decoration::Binding,
                res_index.c as u32,
            )
            .map_err(|err| gen_unexpected_error(stage, err))?;
        }
    }

    for storage_buffer in &shader_resources.storage_buffers {
        let set = ast
            .get_decoration(storage_buffer.id, spirv::Decoration::DescriptorSet)
            .map_err(|err| gen_query_error(stage, err))? as usize;
        let binding = ast
            .get_decoration(storage_buffer.id, spirv::Decoration::Binding)
            .map_err(|err| gen_query_error(stage, err))?;

        let set_ref = match layout.sets.get(set) {
            Some(set) => set,
            None => continue,
        };

        let binding_ref = match set_ref.bindings.get(binding as usize) {
            Some(binding) => binding,
            None => continue,
        };

        let read_only = match binding_ref.ty {
            pso::DescriptorType::Buffer {
                ty: pso::BufferDescriptorType::Storage { read_only },
                ..
            } => read_only,
            _ => unreachable!(),
        };

        let (_content, res_index) = if read_only {
            if let Some((_content, res_index)) = set_ref.find_register(stage, binding) {
                (_content, res_index)
            } else {
                continue;
            }
        } else {
            set_ref.find_uav_register(stage, binding)
        };

        // If the binding is read/write, we need to generate a UAV here.
        if !read_only {
            ast.set_member_decoration(storage_buffer.type_id, 0, spirv::Decoration::NonWritable, 0)
                .map_err(|err| gen_unexpected_error(stage, err))?;
        }

        let index = if read_only {
            res_index.t as u32
        } else if stage == ShaderStage::Compute {
            res_index.u as u32
        } else {
            d3d11::D3D11_PS_CS_UAV_REGISTER_COUNT - 1 - res_index.u as u32
        };

        ast.set_decoration(storage_buffer.id, spirv::Decoration::Binding, index)
            .map_err(|err| gen_unexpected_error(stage, err))?;
    }

    for image in &shader_resources.storage_images {
        let set = ast
            .get_decoration(image.id, spirv::Decoration::DescriptorSet)
            .map_err(|err| gen_query_error(stage, err))? as usize;
        let binding = ast
            .get_decoration(image.id, spirv::Decoration::Binding)
            .map_err(|err| gen_query_error(stage, err))?;

        let set_ref = match layout.sets.get(set) {
            Some(set) => set,
            None => continue,
        };

        let (_content, res_index) = set_ref.find_uav_register(stage, binding);

        // Read only storage images are generated as UAVs by spirv-cross.
        //
        // Compute uses bottom up stack, all other stages use top down.
        let index = if stage == ShaderStage::Compute {
            res_index.u as u32
        } else {
            d3d11::D3D11_PS_CS_UAV_REGISTER_COUNT - 1 - res_index.u as u32
        };

        ast.set_decoration(image.id, spirv::Decoration::Binding, index)
            .map_err(|err| gen_unexpected_error(stage, err))?;
    }

    for sampler in &shader_resources.separate_samplers {
        let set = ast
            .get_decoration(sampler.id, spirv::Decoration::DescriptorSet)
            .map_err(|err| gen_query_error(stage, err))? as usize;
        let binding = ast
            .get_decoration(sampler.id, spirv::Decoration::Binding)
            .map_err(|err| gen_query_error(stage, err))?;

        let set_ref = match layout.sets.get(set) {
            Some(set) => set,
            None => continue,
        };

        if let Some((_content, res_index)) = set_ref.find_register(stage, binding) {
            ast.set_decoration(sampler.id, spirv::Decoration::Binding, res_index.s as u32)
                .map_err(|err| gen_unexpected_error(stage, err))?;
        }
    }

    for image in &shader_resources.sampled_images {
        let set = ast
            .get_decoration(image.id, spirv::Decoration::DescriptorSet)
            .map_err(|err| gen_query_error(stage, err))? as usize;
        let binding = ast
            .get_decoration(image.id, spirv::Decoration::Binding)
            .map_err(|err| gen_query_error(stage, err))?;

        let set_ref = match layout.sets.get(set) {
            Some(set) => set,
            None => continue,
        };

        if let Some((_content, res_index)) = set_ref.find_register(stage, binding) {
            ast.set_decoration(image.id, spirv::Decoration::Binding, res_index.t as u32)
                .map_err(|err| gen_unexpected_error(stage, err))?;
        }
    }

    assert!(
        shader_resources.push_constant_buffers.len() <= 1,
        "Only 1 push constant buffer is supported"
    );
    for push_constant_buffer in &shader_resources.push_constant_buffers {
        ast.set_decoration(
            push_constant_buffer.id,
            spirv::Decoration::DescriptorSet,
            0, // value doesn't matter, just needs a value
        )
        .map_err(|err| gen_unexpected_error(stage, err))?;
        ast.set_decoration(
            push_constant_buffer.id,
            spirv::Decoration::Binding,
            d3d11::D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1,
        )
        .map_err(|err| gen_unexpected_error(stage, err))?;
    }

    Ok(())
}

fn translate_spirv(
    ast: &mut spirv::Ast<hlsl::Target>,
    shader_model: hlsl::ShaderModel,
    _layout: &PipelineLayout,
    stage: ShaderStage,
    features: &hal::Features,
    entry_point: &str,
) -> Result<String, pso::CreationError> {
    let mut compile_options = hlsl::CompilerOptions::default();
    compile_options.shader_model = shader_model;
    compile_options.vertex.invert_y = !features.contains(hal::Features::NDC_Y_UP);
    compile_options.force_zero_initialized_variables = true;
    compile_options.entry_point = Some((entry_point.to_string(), conv::map_stage(stage)));

    //let stage_flag = stage.into();

    // TODO:
    /*let root_constant_layout = layout
    .root_constants
    .iter()
    .filter_map(|constant| if constant.stages.contains(stage_flag) {
        Some(hlsl::RootConstant {
            start: constant.range.start * 4,
            end: constant.range.end * 4,
            binding: constant.range.start,
            space: 0,
        })
    } else {
        None
    })
    .collect();*/
    ast.set_compiler_options(&compile_options)
        .map_err(|err| gen_unexpected_error(stage, err))?;
    //ast.set_root_constant_layout(root_constant_layout)
    //    .map_err(gen_unexpected_error)?;
    ast.compile().map_err(|err| {
        let msg = match err {
            SpirvErrorCode::CompilationError(msg) => msg,
            SpirvErrorCode::Unhandled => "Unknown compile error".into(),
        };
        let error = format!("SPIR-V compile failed: {:?}", msg);
        pso::CreationError::ShaderCreationError(stage.to_flag(), error)
    })
}
