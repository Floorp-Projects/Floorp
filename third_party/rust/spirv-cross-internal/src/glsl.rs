use crate::bindings as br;
use crate::ptr_util::read_into_vec_from_ptr;
use crate::{compiler, spirv, ErrorCode};
use std::marker::PhantomData;
use std::ptr;

/// A GLSL target.
#[derive(Debug, Clone)]
pub enum Target {}

pub struct TargetData {
    combined_image_samplers_built: bool,
}

impl spirv::Target for Target {
    type Data = TargetData;
}

#[allow(non_snake_case, non_camel_case_types)]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum Version {
    V1_10,
    V1_20,
    V1_30,
    V1_40,
    V1_50,
    V3_30,
    V4_00,
    V4_10,
    V4_20,
    V4_30,
    V4_40,
    V4_50,
    V4_60,
    V1_00Es,
    V3_00Es,
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

/// GLSL compiler options.
#[derive(Debug, Clone)]
pub struct CompilerOptions {
    pub version: Version,
    pub vertex: CompilerVertexOptions,
}

impl CompilerOptions {
    fn as_raw(&self) -> br::ScGlslCompilerOptions {
        use self::Version::*;
        let (version, es) = match self.version {
            V1_10 => (1_10, false),
            V1_20 => (1_20, false),
            V1_30 => (1_30, false),
            V1_40 => (1_40, false),
            V1_50 => (1_50, false),
            V3_30 => (3_30, false),
            V4_00 => (4_00, false),
            V4_10 => (4_10, false),
            V4_20 => (4_20, false),
            V4_30 => (4_30, false),
            V4_40 => (4_40, false),
            V4_50 => (4_50, false),
            V4_60 => (4_60, false),
            V1_00Es => (1_00, true),
            V3_00Es => (3_00, true),
        };
        br::ScGlslCompilerOptions {
            vertex_invert_y: self.vertex.invert_y,
            vertex_transform_clip_space: self.vertex.transform_clip_space,
            version,
            es,
        }
    }
}

impl Default for CompilerOptions {
    fn default() -> CompilerOptions {
        CompilerOptions {
            version: Version::V4_50,
            vertex: CompilerVertexOptions::default(),
        }
    }
}

impl spirv::Parse<Target> for spirv::Ast<Target> {
    fn parse(module: &spirv::Module) -> Result<Self, ErrorCode> {
        let compiler = {
            let mut compiler = ptr::null_mut();
            unsafe {
                check!(br::sc_internal_compiler_glsl_new(
                    &mut compiler,
                    module.words.as_ptr() as *const u32,
                    module.words.len() as usize,
                ));
            }

            compiler::Compiler {
                sc_compiler: compiler,
                target_data: TargetData {
                    combined_image_samplers_built: false,
                },
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

    /// Set GLSL compiler specific compilation settings.
    fn set_compiler_options(&mut self, options: &CompilerOptions) -> Result<(), ErrorCode> {
        let raw_options = options.as_raw();
        unsafe {
            check!(br::sc_internal_compiler_glsl_set_options(
                self.compiler.sc_compiler,
                &raw_options,
            ));
        }

        Ok(())
    }

    /// Generate GLSL shader from the AST.
    fn compile(&mut self) -> Result<String, ErrorCode> {
        self.build_combined_image_samplers()?;
        self.compiler.compile()
    }
}

impl spirv::Ast<Target> {
    pub fn build_combined_image_samplers(&mut self) -> Result<(), ErrorCode> {
        unsafe {
            if !self.compiler.target_data.combined_image_samplers_built {
                check!(br::sc_internal_compiler_glsl_build_combined_image_samplers(
                    self.compiler.sc_compiler
                ));
                self.compiler.target_data.combined_image_samplers_built = true
            }
        }

        Ok(())
    }

    pub fn get_combined_image_samplers(
        &mut self,
    ) -> Result<Vec<spirv::CombinedImageSampler>, ErrorCode> {
        self.build_combined_image_samplers()?;
        unsafe {
            let mut samplers_raw: *const br::ScCombinedImageSampler = std::ptr::null();
            let mut samplers_raw_length: usize = 0;

            check!(br::sc_internal_compiler_glsl_get_combined_image_samplers(
                self.compiler.sc_compiler,
                &mut samplers_raw as _,
                &mut samplers_raw_length as _,
            ));

            let samplers = read_into_vec_from_ptr(samplers_raw, samplers_raw_length)
                .iter()
                .map(|sc| spirv::CombinedImageSampler {
                    combined_id: sc.combined_id,
                    image_id: sc.image_id,
                    sampler_id: sc.sampler_id,
                })
                .collect();

            Ok(samplers)
        }
    }
}
