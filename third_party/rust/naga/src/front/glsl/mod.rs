//! Front end for consuming the OpenGl Shading language (GLSL).
//!
//! To begin take a look at the documentation for the [`Parser`](Parser).
//!
//! # Supported versions
//! Currently only the versions 450 and 460 are supported and 440 is partially
//! supported, furthermore the vulkan flavor is assumed.

pub use ast::{Precision, Profile};
pub use error::{Error, ErrorKind, ExpectedToken};
pub use token::TokenValue;

use crate::{FastHashMap, FastHashSet, Handle, Module, ShaderStage, Span, Type};
use ast::{EntryArg, FunctionDeclaration, GlobalLookup};
use parser::ParsingContext;

mod ast;
mod builtins;
mod constants;
mod context;
mod error;
mod functions;
mod lex;
mod offset;
mod parser;
#[cfg(test)]
mod parser_tests;
mod token;
mod types;
mod variables;

type Result<T> = std::result::Result<T, Error>;

/// Per shader options passed to [`parse`](Parser::parse)
///
/// The [`From`](From) trait is implemented for [`ShaderStage`](ShaderStage) to
/// provide a quick way to create a Options instance.
/// ```rust
/// # use naga::ShaderStage;
/// # use naga::front::glsl::Options;
/// Options::from(ShaderStage::Vertex);
/// ```
#[derive(Debug)]
pub struct Options {
    /// The shader stage in the pipeline.
    pub stage: ShaderStage,
    /// Preprocesor definitions to be used, akin to having
    /// ```glsl
    /// #define key value
    /// ```
    /// for each key value pair in the map.
    pub defines: FastHashMap<String, String>,
}

impl From<ShaderStage> for Options {
    fn from(stage: ShaderStage) -> Self {
        Options {
            stage,
            defines: FastHashMap::default(),
        }
    }
}

/// Additional information about the glsl shader
///
/// Stores additional information about the glsl shader which might not be
/// stored in the shader [`Module`](Module).
#[derive(Debug)]
pub struct ShaderMetadata {
    /// The glsl version specified in the shader trought the use of the
    /// `#version` preprocessor directive.
    pub version: u16,
    /// The glsl profile specified in the shader trought the use of the
    /// `#version` preprocessor directive.
    pub profile: Profile,
    /// The shader stage in the pipeline, passed to the [`parse`](Parser::parse)
    /// method via the [`Options`](Options) struct.
    pub stage: ShaderStage,

    /// The workgroup size for compute shaders, defaults to `[1; 3]` for
    /// compute shaders and `[0; 3]` for non compute shaders.
    pub workgroup_size: [u32; 3],
    /// Wether or not early fragment tests where requested by the shader,
    /// defaults to `false`.
    pub early_fragment_tests: bool,

    /// The shader can request extensions via the
    /// `#extension` preprocessor directive, in the directive a behavior
    /// parameter is used to control whether the extension should be disabled,
    /// warn on usage, enabled if possible or required.
    ///
    /// This field only stores extensions which were required or requested to
    /// be enabled if possible and they are supported.
    pub extensions: FastHashSet<String>,
}

impl ShaderMetadata {
    fn reset(&mut self, stage: ShaderStage) {
        self.version = 0;
        self.profile = Profile::Core;
        self.stage = stage;
        self.workgroup_size = [if stage == ShaderStage::Compute { 1 } else { 0 }; 3];
        self.early_fragment_tests = false;
        self.extensions.clear();
    }
}

impl Default for ShaderMetadata {
    fn default() -> Self {
        ShaderMetadata {
            version: 0,
            profile: Profile::Core,
            stage: ShaderStage::Vertex,
            workgroup_size: [0; 3],
            early_fragment_tests: false,
            extensions: FastHashSet::default(),
        }
    }
}

/// The `Parser` is the central structure of the glsl frontend.
///
/// To instantiate a new `Parser` the [`Default`](Default) trait is used, so a
/// call to the associated function [`Parser::default`](Parser::default) will
/// return a new `Parser` instance.
///
/// To parse a shader simply call the [`parse`](Parser::parse) method with a
/// [`Options`](Options) struct and a [`&str`](str) holding the glsl code.
///
/// The `Parser` also provides the [`metadata`](Parser::metadata) to get some
/// further information about the previously parsed shader, like version and
/// extensions used (see the documentation for
/// [`ShaderMetadata`](ShaderMetadata) to see all the returned information)
///
/// # Example usage
/// ```rust
/// use naga::ShaderStage;
/// use naga::front::glsl::{Parser, Options};
///
/// let glsl = r#"
///     #version 450 core
///
///     void main() {}
/// "#;
///
/// let mut parser = Parser::default();
/// let options = Options::from(ShaderStage::Vertex);
/// parser.parse(&options, glsl);
/// ```
///
/// # Reusability
///
/// If there's a need to parse more than one shader reusing the same `Parser`
/// instance may be beneficial since internal allocations will be reused.
///
/// Calling the [`parse`](Parser::parse) method multiple times will reset the
/// `Parser` so no extra care is needed when reusing.
#[derive(Debug, Default)]
pub struct Parser {
    meta: ShaderMetadata,

    lookup_function: FastHashMap<String, FunctionDeclaration>,
    lookup_type: FastHashMap<String, Handle<Type>>,

    global_variables: Vec<(String, GlobalLookup)>,

    entry_args: Vec<EntryArg>,

    errors: Vec<Error>,

    module: Module,
}

impl Parser {
    fn reset(&mut self, stage: ShaderStage) {
        self.meta.reset(stage);

        self.lookup_function.clear();
        self.lookup_type.clear();
        self.global_variables.clear();
        self.entry_args.clear();

        // This is necessary because if the last parsing errored out, the module
        // wouldn't have been swapped
        self.module = Module::default();
    }

    /// Parses a shader either outputting a shader [`Module`](Module) or a list
    /// of [`Error`](Error)s.
    ///
    /// Multiple calls using the same `Parser` and different shaders are supported.
    pub fn parse(
        &mut self,
        options: &Options,
        source: &str,
    ) -> std::result::Result<Module, Vec<Error>> {
        self.reset(options.stage);

        let lexer = lex::Lexer::new(source, &options.defines);
        let mut ctx = ParsingContext::new(lexer);

        if let Err(e) = ctx.parse(self) {
            self.errors.push(e);
        }

        if self.errors.is_empty() {
            let mut module = Module::default();
            std::mem::swap(&mut self.module, &mut module);
            Ok(module)
        } else {
            let mut errors = Vec::new();
            std::mem::swap(&mut self.errors, &mut errors);
            Err(errors)
        }
    }

    /// Returns additional information about the parsed shader which might not be
    /// stored in the [`Module`](Module), see the documentation for
    /// [`ShaderMetadata`](ShaderMetadata) for more information about the
    /// returned data.
    ///
    /// # Notes
    ///
    /// Following an unsuccessful parsing the state of the returned infomration
    /// is undefined, it might contain only partial information about the
    /// current shader, the previous shader or both.
    pub fn metadata(&self) -> &ShaderMetadata {
        &self.meta
    }
}
