use crate::bindings as br;
use crate::{compiler, spirv, ErrorCode};
use std::ffi::CString;
use std::marker::PhantomData;
use std::ptr;

pub use crate::bindings::root::ScHlslRootConstant as RootConstant;

/// A HLSL target.
#[derive(Debug, Clone)]
pub enum Target {}

impl spirv::Target for Target {
    type Data = ();
}

/// A HLSL shader model version.
#[allow(non_snake_case, non_camel_case_types)]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
#[non_exhaustive]
pub enum ShaderModel {
    V3_0,
    V4_0,
    V4_0L9_0,
    V4_0L9_1,
    V4_0L9_3,
    V4_1,
    V5_0,
    V5_1,
    V6_0,
}

#[allow(non_snake_case, non_camel_case_types)]
impl ShaderModel {
    fn as_raw(self) -> i32 {
        use self::ShaderModel::*;
        match self {
            V3_0 => 30,
            V4_0 => 40,
            V4_0L9_0 => 40,
            V4_0L9_1 => 40,
            V4_0L9_3 => 40,
            V4_1 => 41,
            V5_0 => 50,
            V5_1 => 51,
            V6_0 => 60,
        }
    }
}

#[derive(Debug, Clone)]
pub struct CompilerVertexOptions {
    pub invert_y: bool,
    pub transform_clip_space: bool,
}

impl Default for CompilerVertexOptions {
    fn default() -> CompilerVertexOptions {
        CompilerVertexOptions {
            invert_y: false,
            transform_clip_space: false,
        }
    }
}

/// HLSL compiler options.
#[non_exhaustive]
#[derive(Debug, Clone)]
pub struct CompilerOptions {
    pub shader_model: ShaderModel,
    /// Support point size builtin but ignore the value.
    pub point_size_compat: bool,
    /// Support point coordinate builtin but ignore the value.
    pub point_coord_compat: bool,
    pub vertex: CompilerVertexOptions,
    pub force_storage_buffer_as_uav: bool,
    pub nonwritable_uav_texture_as_srv: bool,
    /// Whether to force all uninitialized variables to be initialized to zero.
    pub force_zero_initialized_variables: bool,
    /// The name and execution model of the entry point to use. If no entry
    /// point is specified, then the first entry point found will be used.
    pub entry_point: Option<(String, spirv::ExecutionModel)>,
}

impl Default for CompilerOptions {
    fn default() -> CompilerOptions {
        CompilerOptions {
            shader_model: ShaderModel::V3_0,
            point_size_compat: false,
            point_coord_compat: false,
            vertex: CompilerVertexOptions::default(),
            force_storage_buffer_as_uav: false,
            nonwritable_uav_texture_as_srv: false,
            force_zero_initialized_variables: false,
            entry_point: None,
        }
    }
}

impl spirv::Parse<Target> for spirv::Ast<Target> {
    fn parse(module: &spirv::Module) -> Result<Self, ErrorCode> {
        let compiler = {
            let mut compiler = ptr::null_mut();
            unsafe {
                check!(br::sc_internal_compiler_hlsl_new(
                    &mut compiler,
                    module.words.as_ptr() as *const u32,
                    module.words.len() as usize,
                ));
            }

            compiler::Compiler {
                sc_compiler: compiler,
                target_data: (),
                has_been_compiled: false,
            }
        };

        Ok(spirv::Ast {
            compiler,
            target_type: PhantomData,
        })
    }
}

impl spirv::Compile<Target> for spirv::Ast<Target> {
    type CompilerOptions = CompilerOptions;

    /// Set HLSL compiler specific compilation settings.
    fn set_compiler_options(&mut self, options: &CompilerOptions) -> Result<(), ErrorCode> {
        if let Some((name, model)) = &options.entry_point {
            let name_raw = CString::new(name.as_str()).map_err(|_| ErrorCode::Unhandled)?;
            let model = model.as_raw();
            unsafe {
                check!(br::sc_internal_compiler_set_entry_point(
                    self.compiler.sc_compiler,
                    name_raw.as_ptr(),
                    model,
                ));
            }
        };
        let raw_options = br::ScHlslCompilerOptions {
            shader_model: options.shader_model.as_raw(),
            point_size_compat: options.point_size_compat,
            point_coord_compat: options.point_coord_compat,
            vertex_invert_y: options.vertex.invert_y,
            vertex_transform_clip_space: options.vertex.transform_clip_space,
            force_storage_buffer_as_uav: options.force_storage_buffer_as_uav,
            nonwritable_uav_texture_as_srv: options.nonwritable_uav_texture_as_srv,
            force_zero_initialized_variables: options.force_zero_initialized_variables,
        };
        unsafe {
            check!(br::sc_internal_compiler_hlsl_set_options(
                self.compiler.sc_compiler,
                &raw_options,
            ));
        }

        Ok(())
    }

    /// Generate HLSL shader from the AST.
    fn compile(&mut self) -> Result<String, ErrorCode> {
        self.compiler.compile()
    }
}

impl spirv::Ast<Target> {
    ///
    pub fn set_root_constant_layout(&mut self, layout: Vec<RootConstant>) -> Result<(), ErrorCode> {
        unsafe {
            check!(br::sc_internal_compiler_hlsl_set_root_constant_layout(
                self.compiler.sc_compiler,
                layout.as_ptr(),
                layout.len() as _,
            ));
        }

        Ok(())
    }
}
