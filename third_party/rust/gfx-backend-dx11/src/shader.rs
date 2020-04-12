use std::{ffi, ptr, slice};

use spirv_cross::{hlsl, spirv, ErrorCode as SpirvErrorCode};

use winapi::shared::winerror;
use winapi::um::{d3dcommon, d3dcompiler};
use wio::com::ComPtr;

use auxil::spirv_cross_specialize_ast;
use hal::{device, pso};

use {conv, Backend, PipelineLayout};

/// Emit error during shader module creation. Used if we don't expect an error
/// but might panic due to an exception in SPIRV-Cross.
fn gen_unexpected_error(err: SpirvErrorCode) -> device::ShaderError {
    let msg = match err {
        SpirvErrorCode::CompilationError(msg) => msg,
        SpirvErrorCode::Unhandled => "Unexpected error".into(),
    };
    device::ShaderError::CompilationFailed(msg)
}

/// Emit error during shader module creation. Used if we execute an query command.
fn gen_query_error(err: SpirvErrorCode) -> device::ShaderError {
    let msg = match err {
        SpirvErrorCode::CompilationError(msg) => msg,
        SpirvErrorCode::Unhandled => "Unknown query error".into(),
    };
    device::ShaderError::CompilationFailed(msg)
}

pub(crate) fn compile_spirv_entrypoint(
    raw_data: &[u32],
    stage: pso::Stage,
    source: &pso::EntryPoint<Backend>,
    layout: &PipelineLayout,
) -> Result<Option<ComPtr<d3dcommon::ID3DBlob>>, device::ShaderError> {
    let mut ast = parse_spirv(raw_data)?;
    spirv_cross_specialize_ast(&mut ast, &source.specialization)?;

    patch_spirv_resources(&mut ast, stage, layout)?;
    let shader_model = hlsl::ShaderModel::V5_0;
    let shader_code = translate_spirv(&mut ast, shader_model, layout, stage)?;
    log::debug!("Generated {:?} shader:\n{:?}", stage, shader_code.replace("\n", "\r\n"));

    let real_name = ast
        .get_cleansed_entry_point_name(source.entry, conv::map_stage(stage))
        .map_err(gen_query_error)?;

    // TODO: opt: don't query *all* entry points.
    let entry_points = ast.get_entry_points().map_err(gen_query_error)?;
    entry_points
        .iter()
        .find(|entry_point| entry_point.name == real_name)
        .ok_or(device::ShaderError::MissingEntryPoint(source.entry.into()))
        .and_then(|entry_point| {
            let stage = conv::map_execution_model(entry_point.execution_model);
            let shader = compile_hlsl_shader(
                stage,
                shader_model,
                &entry_point.name,
                shader_code.as_bytes(),
            )?;
            Ok(Some(unsafe { ComPtr::from_raw(shader) }))
        })
}

pub(crate) fn compile_hlsl_shader(
    stage: pso::Stage,
    shader_model: hlsl::ShaderModel,
    entry: &str,
    code: &[u8],
) -> Result<*mut d3dcommon::ID3DBlob, device::ShaderError> {
    let stage_str = {
        let stage = match stage {
            pso::Stage::Vertex => "vs",
            pso::Stage::Fragment => "ps",
            pso::Stage::Compute => "cs",
            _ => unimplemented!(),
        };

        let model = match shader_model {
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

        Err(device::ShaderError::CompilationFailed(message))
    } else {
        Ok(blob)
    }
}

fn parse_spirv(raw_data: &[u32]) -> Result<spirv::Ast<hlsl::Target>, device::ShaderError> {
    let module = spirv::Module::from_words(raw_data);

    spirv::Ast::parse(&module).map_err(|err| {
        let msg = match err {
            SpirvErrorCode::CompilationError(msg) => msg,
            SpirvErrorCode::Unhandled => "Unknown parsing error".into(),
        };
        device::ShaderError::CompilationFailed(msg)
    })
}

fn patch_spirv_resources(
    ast: &mut spirv::Ast<hlsl::Target>,
    stage: pso::Stage,
    layout: &PipelineLayout,
) -> Result<(), device::ShaderError> {
    // we remap all `layout(binding = n, set = n)` to a flat space which we get from our
    // `PipelineLayout` which knows of all descriptor set layouts

    let shader_resources = ast.get_shader_resources().map_err(gen_query_error)?;
    for image in &shader_resources.separate_images {
        let set = ast
            .get_decoration(image.id, spirv::Decoration::DescriptorSet)
            .map_err(gen_query_error)? as usize;
        let binding = ast
            .get_decoration(image.id, spirv::Decoration::Binding)
            .map_err(gen_query_error)?;
        let (_content, res_index) = layout.sets[set]
            .find_register(stage, binding);

        ast.set_decoration(
            image.id,
            spirv::Decoration::Binding,
            res_index.t as u32,
        )
        .map_err(gen_unexpected_error)?;
    }

    for uniform_buffer in &shader_resources.uniform_buffers {
        let set = ast
            .get_decoration(uniform_buffer.id, spirv::Decoration::DescriptorSet)
            .map_err(gen_query_error)? as usize;
        let binding = ast
            .get_decoration(uniform_buffer.id, spirv::Decoration::Binding)
            .map_err(gen_query_error)?;
        let (_content, res_index) = layout.sets[set]
            .find_register(stage, binding);

        ast.set_decoration(
            uniform_buffer.id,
            spirv::Decoration::Binding,
            res_index.c as u32,
        )
        .map_err(gen_unexpected_error)?;
    }

    for storage_buffer in &shader_resources.storage_buffers {
        let set = ast
            .get_decoration(storage_buffer.id, spirv::Decoration::DescriptorSet)
            .map_err(gen_query_error)? as usize;
        let binding = ast
            .get_decoration(storage_buffer.id, spirv::Decoration::Binding)
            .map_err(gen_query_error)?;
        let (_content, res_index) = layout.sets[set]
            .find_register(stage, binding);

        ast.set_decoration(
            storage_buffer.id,
            spirv::Decoration::Binding,
            res_index.u as u32, //TODO: also decorate `res_index.t`
        )
        .map_err(gen_unexpected_error)?;
    }

    for image in &shader_resources.storage_images {
        let set = ast
            .get_decoration(image.id, spirv::Decoration::DescriptorSet)
            .map_err(gen_query_error)? as usize;
        let binding = ast
            .get_decoration(image.id, spirv::Decoration::Binding)
            .map_err(gen_query_error)?;
        let (_content, res_index) = layout.sets[set]
            .find_register(stage, binding);

        ast.set_decoration(
            image.id,
            spirv::Decoration::Binding,
            res_index.u as u32, //TODO: also decorate `res_index.t`
        )
        .map_err(gen_unexpected_error)?;
    }

    for sampler in &shader_resources.separate_samplers {
        let set = ast
            .get_decoration(sampler.id, spirv::Decoration::DescriptorSet)
            .map_err(gen_query_error)? as usize;
        let binding = ast
            .get_decoration(sampler.id, spirv::Decoration::Binding)
            .map_err(gen_query_error)?;
        let (_content, res_index) = layout.sets[set]
            .find_register(stage, binding);

        ast.set_decoration(
            sampler.id,
            spirv::Decoration::Binding,
            res_index.s as u32,
        )
        .map_err(gen_unexpected_error)?;
    }

    for image in &shader_resources.sampled_images {
        let set = ast
            .get_decoration(image.id, spirv::Decoration::DescriptorSet)
            .map_err(gen_query_error)? as usize;
        let binding = ast
            .get_decoration(image.id, spirv::Decoration::Binding)
            .map_err(gen_query_error)?;
        let (_content, res_index) = layout.sets[set]
            .find_register(stage, binding);

        ast.set_decoration(
            image.id,
            spirv::Decoration::Binding,
            res_index.t as u32,
        )
        .map_err(gen_unexpected_error)?;
    }

    Ok(())
}

fn translate_spirv(
    ast: &mut spirv::Ast<hlsl::Target>,
    shader_model: hlsl::ShaderModel,
    _layout: &PipelineLayout,
    _stage: pso::Stage,
) -> Result<String, device::ShaderError> {
    let mut compile_options = hlsl::CompilerOptions::default();
    compile_options.shader_model = shader_model;
    compile_options.vertex.invert_y = true;

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
        .map_err(gen_unexpected_error)?;
    //ast.set_root_constant_layout(root_constant_layout)
    //    .map_err(gen_unexpected_error)?;
    ast.compile().map_err(|err| {
        let msg = match err {
            SpirvErrorCode::CompilationError(msg) => msg,
            SpirvErrorCode::Unhandled => "Unknown compile error".into(),
        };
        device::ShaderError::CompilationFailed(msg)
    })
}
