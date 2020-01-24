use crate::bindings as br;
use crate::{compiler, spirv, ErrorCode};
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
#[derive(Debug, Clone)]
pub struct CompilerOptions {
    pub shader_model: ShaderModel,
    /// Support point size builtin but ignore the value.
    pub point_size_compat: bool,
    /// Support point coordinate builtin but ignore the value.
    pub point_coord_compat: bool,
    pub vertex: CompilerVertexOptions,
}

impl CompilerOptions {
    fn as_raw(&self) -> br::ScHlslCompilerOptions {
        br::ScHlslCompilerOptions {
            shader_model: self.shader_model.as_raw(),
            point_size_compat: self.point_size_compat,
            point_coord_compat: self.point_coord_compat,
            vertex_invert_y: self.vertex.invert_y,
            vertex_transform_clip_space: self.vertex.transform_clip_space,
        }
    }
}

impl Default for CompilerOptions {
    fn default() -> CompilerOptions {
        CompilerOptions {
            shader_model: ShaderModel::V3_0,
            point_size_compat: false,
            point_coord_compat: false,
            vertex: CompilerVertexOptions::default(),
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
        let raw_options = options.as_raw();
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
