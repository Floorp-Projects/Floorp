//! OpenGL shading language backend
//!
//! The main structure is [`Writer`](Writer), it maintains internal state that is used
//! to output a [`Module`](crate::Module) into glsl
//!
//! # Supported versions
//! ### Core
//! - 330
//! - 400
//! - 410
//! - 420
//! - 430
//! - 450
//! - 460
//!
//! ### ES
//! - 300
//! - 310
//!

// GLSL is mostly a superset of C but it also removes some parts of it this is a list of relevant
// aspects for this backend.
//
// The most notable change is the introduction of the version preprocessor directive that must
// always be the first line of a glsl file and is written as
// `#version number profile`
// `number` is the version itself (i.e. 300) and `profile` is the
// shader profile we only support "core" and "es", the former is used in desktop applications and
// the later is used in embedded contexts, mobile devices and browsers. Each one as it's own
// versions (at the time of writing this the latest version for "core" is 460 and for "es" is 320)
//
// Other important preprocessor addition is the extension directive which is written as
// `#extension name: behaviour`
// Extensions provide increased features in a plugin fashion but they aren't required to be
// supported hence why they are called extensions, that's why `behaviour` is used it specifies
// wether the extension is strictly required or if it should only be enabled if needed. In our case
// when we use extensions we set behaviour to `require` always.
//
// The only thing that glsl removes that makes a difference are pointers.
//
// Additions that are relevant for the backend are the discard keyword, the introduction of
// vector, matrices, samplers, image types and functions that provide common shader operations

pub use features::Features;

use crate::{
    back,
    proc::{self, NameKey},
    valid, Handle, ShaderStage, TypeInner,
};
use features::FeaturesManager;
use std::{
    cmp::Ordering,
    fmt,
    fmt::{Error as FmtError, Write},
};
use thiserror::Error;

/// Contains the features related code and the features querying method
mod features;
/// Contains a constant with a slice of all the reserved keywords RESERVED_KEYWORDS
mod keywords;

/// List of supported core glsl versions
pub const SUPPORTED_CORE_VERSIONS: &[u16] = &[330, 400, 410, 420, 430, 440, 450];
/// List of supported es glsl versions
pub const SUPPORTED_ES_VERSIONS: &[u16] = &[300, 310, 320];

pub type BindingMap = std::collections::BTreeMap<crate::ResourceBinding, u8>;

impl crate::AtomicFunction {
    fn to_glsl(self) -> &'static str {
        match self {
            Self::Add | Self::Subtract => "Add",
            Self::And => "And",
            Self::InclusiveOr => "Or",
            Self::ExclusiveOr => "Xor",
            Self::Min => "Min",
            Self::Max => "Max",
            Self::Exchange { compare: None } => "Exchange",
            Self::Exchange { compare: Some(_) } => "", //TODO
        }
    }
}

/// glsl version
#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum Version {
    /// `core` glsl
    Desktop(u16),
    /// `es` glsl
    Embedded(u16),
}

impl Version {
    /// Returns true if self is `Version::Embedded` (i.e. is a es version)
    fn is_es(&self) -> bool {
        match *self {
            Version::Desktop(_) => false,
            Version::Embedded(_) => true,
        }
    }

    /// Checks the list of currently supported versions and returns true if it contains the
    /// specified version
    ///
    /// # Notes
    /// As an invalid version number will never be added to the supported version list
    /// so this also checks for version validity
    fn is_supported(&self) -> bool {
        match *self {
            Version::Desktop(v) => SUPPORTED_CORE_VERSIONS.contains(&v),
            Version::Embedded(v) => SUPPORTED_ES_VERSIONS.contains(&v),
        }
    }

    /// Checks if the version supports all of the explicit layouts:
    /// - `location=` qualifiers for bindings
    /// - `binding=` qualifiers for resources
    ///
    /// Note: `location=` for vertex inputs and fragment outputs is supported
    /// unconditionally for GLES 300.
    fn supports_explicit_locations(&self) -> bool {
        *self >= Version::Embedded(310) || *self >= Version::Desktop(410)
    }

    fn supports_early_depth_test(&self) -> bool {
        *self >= Version::Desktop(130) || *self >= Version::Embedded(310)
    }

    fn supports_std430_layout(&self) -> bool {
        *self >= Version::Desktop(430) || *self >= Version::Embedded(310)
    }
}

impl PartialOrd for Version {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        match (*self, *other) {
            (Version::Desktop(x), Version::Desktop(y)) => Some(x.cmp(&y)),
            (Version::Embedded(x), Version::Embedded(y)) => Some(x.cmp(&y)),
            _ => None,
        }
    }
}

impl fmt::Display for Version {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Version::Desktop(v) => write!(f, "{} core", v),
            Version::Embedded(v) => write!(f, "{} es", v),
        }
    }
}

bitflags::bitflags! {
    #[cfg_attr(feature = "serialize", derive(serde::Serialize))]
    #[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
    pub struct WriterFlags: u32 {
        /// Flip output Y and extend Z from (0,1) to (-1,1).
        const ADJUST_COORDINATE_SPACE = 0x1;
        /// Supports GL_EXT_texture_shadow_lod on the host, which provides
        /// additional functions on shadows and arrays of shadows.
        const TEXTURE_SHADOW_LOD = 0x2;
    }
}

/// Structure that contains the configuration used in the [`Writer`](Writer)
#[derive(Debug, Clone)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub struct Options {
    /// The glsl version to be used
    pub version: Version,
    /// Configuration flags for the writer.
    pub writer_flags: WriterFlags,
    /// Map of resources association to binding locations.
    pub binding_map: BindingMap,
}

impl Default for Options {
    fn default() -> Self {
        Options {
            version: Version::Embedded(310),
            writer_flags: WriterFlags::ADJUST_COORDINATE_SPACE,
            binding_map: BindingMap::default(),
        }
    }
}

// A subset of options that are meant to be changed per pipeline.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub struct PipelineOptions {
    /// The stage of the entry point
    pub shader_stage: ShaderStage,
    /// The name of the entry point
    ///
    /// If no entry point that matches is found a error will be thrown while creating a new instance
    /// of [`Writer`](struct.Writer.html)
    pub entry_point: String,
}

/// Structure that contains a reflection info
pub struct ReflectionInfo {
    pub texture_mapping: crate::FastHashMap<String, TextureMapping>,
    pub uniforms: crate::FastHashMap<Handle<crate::GlobalVariable>, String>,
}

/// Structure that connects a texture to a sampler or not
///
/// glsl pre vulkan has no concept of separate textures and samplers instead everything is a
/// `gsamplerN` where `g` is the scalar type and `N` is the dimension, but naga uses separate textures
/// and samplers in the IR so the backend produces a [`HashMap`](crate::FastHashMap) with the texture name
/// as a key and a [`TextureMapping`](TextureMapping) as a value this way the user knows where to bind.
///
/// [`Storage`](crate::ImageClass::Storage) images produce `gimageN` and don't have an associated sampler
/// so the [`sampler`](Self::sampler) field will be [`None`](std::option::Option::None)
#[derive(Debug, Clone)]
pub struct TextureMapping {
    /// Handle to the image global variable
    pub texture: Handle<crate::GlobalVariable>,
    /// Handle to the associated sampler global variable if it exists
    pub sampler: Option<Handle<crate::GlobalVariable>>,
}

/// Helper structure that generates a number
#[derive(Default)]
struct IdGenerator(u32);

impl IdGenerator {
    /// Generates a number that's guaranteed to be unique for this `IdGenerator`
    fn generate(&mut self) -> u32 {
        // It's just an increasing number but it does the job
        let ret = self.0;
        self.0 += 1;
        ret
    }
}

/// Helper wrapper used to get a name for a varying
///
/// Varying have different naming schemes depending on their binding:
/// - Varyings with builtin bindings get the from [`glsl_built_in`](glsl_built_in).
/// - Varyings with location bindings are named `_S_location_X` where `S` is a
///   prefix identifying which pipeline stage the varying connects, and `X` is
///   the location.
struct VaryingName<'a> {
    binding: &'a crate::Binding,
    stage: ShaderStage,
    output: bool,
}
impl fmt::Display for VaryingName<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self.binding {
            crate::Binding::Location { location, .. } => {
                let prefix = match (self.stage, self.output) {
                    (ShaderStage::Compute, _) => unreachable!(),
                    // pipeline to vertex
                    (ShaderStage::Vertex, false) => "p2vs",
                    // vertex to fragment
                    (ShaderStage::Vertex, true) | (ShaderStage::Fragment, false) => "vs2fs",
                    // fragment to pipeline
                    (ShaderStage::Fragment, true) => "fs2p",
                };
                write!(f, "_{}_location{}", prefix, location,)
            }
            crate::Binding::BuiltIn(built_in) => {
                write!(f, "{}", glsl_built_in(built_in, self.output))
            }
        }
    }
}

/// Shorthand result used internally by the backend
type BackendResult<T = ()> = Result<T, Error>;

/// A glsl compilation error.
#[derive(Debug, Error)]
pub enum Error {
    /// A error occurred while writing to the output
    #[error("Format error")]
    FmtError(#[from] FmtError),
    /// The specified [`Version`](Version) doesn't have all required [`Features`](super)
    ///
    /// Contains the missing [`Features`](Features)
    #[error("The selected version doesn't support {0:?}")]
    MissingFeatures(Features),
    /// [`StorageClass::PushConstant`](crate::StorageClass::PushConstant) was used and isn't
    /// supported in the glsl backend
    #[error("Push constants aren't supported")]
    PushConstantNotSupported,
    /// The specified [`Version`](Version) isn't supported
    #[error("The specified version isn't supported")]
    VersionNotSupported,
    /// The entry point couldn't be found
    #[error("The requested entry point couldn't be found")]
    EntryPointNotFound,
    /// A call was made to an unsupported external
    #[error("A call was made to an unsupported external: {0}")]
    UnsupportedExternal(String),
    /// A scalar with an unsupported width was requested
    #[error("A scalar with an unsupported width was requested: {0:?} {1:?}")]
    UnsupportedScalar(crate::ScalarKind, crate::Bytes),
    /// A image was used with multiple samplers, this isn't supported
    #[error("A image was used with multiple samplers")]
    ImageMultipleSamplers,
    #[error("{0}")]
    Custom(String),
}

/// Binary operation with a different logic on the GLSL side
enum BinaryOperation {
    /// Vector comparison should use the function like `greaterThan()`, etc.
    VectorCompare,
    /// GLSL `%` is SPIR-V `OpUMod/OpSMod` and `mod()` is `OpFMod`, but [`BinaryOperator::Modulo`](crate::BinaryOperator::Modulo) is `OpFRem`
    Modulo,
    /// Any plain operation. No additional logic required
    Other,
}

/// Main structure of the glsl backend responsible for all code generation
pub struct Writer<'a, W> {
    // Inputs
    /// The module being written
    module: &'a crate::Module,
    /// The module analysis.
    info: &'a valid::ModuleInfo,
    /// The output writer
    out: W,
    /// User defined configuration to be used
    options: &'a Options,

    // Internal State
    /// Features manager used to store all the needed features and write them
    features: FeaturesManager,
    namer: proc::Namer,
    /// A map with all the names needed for writing the module
    /// (generated by a [`Namer`](crate::proc::Namer))
    names: crate::FastHashMap<NameKey, String>,
    /// A map with all the names needed for reflections
    reflection_names_uniforms: crate::FastHashMap<Handle<crate::Type>, String>,
    /// A map with the names of global variables needed for reflections
    reflection_names_globals: crate::FastHashMap<Handle<crate::GlobalVariable>, String>,
    /// The selected entry point
    entry_point: &'a crate::EntryPoint,
    /// The index of the selected entry point
    entry_point_idx: proc::EntryPointIndex,
    /// Used to generate a unique number for blocks
    block_id: IdGenerator,
    /// Set of expressions that have associated temporary variables
    named_expressions: crate::NamedExpressions,
}

impl<'a, W: Write> Writer<'a, W> {
    /// Creates a new [`Writer`](Writer) instance
    ///
    /// # Errors
    /// - If the version specified isn't supported (or invalid)
    /// - If the entry point couldn't be found on the module
    /// - If the version specified doesn't support some used features
    pub fn new(
        out: W,
        module: &'a crate::Module,
        info: &'a valid::ModuleInfo,
        options: &'a Options,
        pipeline_options: &'a PipelineOptions,
    ) -> Result<Self, Error> {
        // Check if the requested version is supported
        if !options.version.is_supported() {
            log::error!("Version {}", options.version);
            return Err(Error::VersionNotSupported);
        }

        // Try to find the entry point and corresponding index
        let ep_idx = module
            .entry_points
            .iter()
            .position(|ep| {
                pipeline_options.shader_stage == ep.stage && pipeline_options.entry_point == ep.name
            })
            .ok_or(Error::EntryPointNotFound)?;

        // Generate a map with names required to write the module
        let mut names = crate::FastHashMap::default();
        let mut namer = proc::Namer::default();
        namer.reset(module, keywords::RESERVED_KEYWORDS, &["gl_"], &mut names);

        // Build the instance
        let mut this = Self {
            module,
            info,
            out,
            options,
            namer,
            features: FeaturesManager::new(),
            names,
            reflection_names_uniforms: crate::FastHashMap::default(),
            reflection_names_globals: crate::FastHashMap::default(),
            entry_point: &module.entry_points[ep_idx],
            entry_point_idx: ep_idx as u16,

            block_id: IdGenerator::default(),
            named_expressions: crate::NamedExpressions::default(),
        };

        // Find all features required to print this module
        this.collect_required_features()?;

        Ok(this)
    }

    /// Writes the [`Module`](crate::Module) as glsl to the output
    ///
    /// # Notes
    /// If an error occurs while writing, the output might have been written partially
    ///
    /// # Panics
    /// Might panic if the module is invalid
    pub fn write(&mut self) -> Result<ReflectionInfo, Error> {
        // We use `writeln!(self.out)` throughout the write to add newlines
        // to make the output more readable

        let es = self.options.version.is_es();

        // Write the version (It must be the first thing or it isn't a valid glsl output)
        writeln!(self.out, "#version {}", self.options.version)?;
        // Write all the needed extensions
        //
        // This used to be the last thing being written as it allowed to search for features while
        // writing the module saving some loops but some older versions (420 or less) required the
        // extensions to appear before being used, even though extensions are part of the
        // preprocessor not the processor ¯\_(ツ)_/¯
        self.features.write(self.options.version, &mut self.out)?;

        // Write the additional extensions
        if self
            .options
            .writer_flags
            .contains(WriterFlags::TEXTURE_SHADOW_LOD)
        {
            // https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_shadow_lod.txt
            writeln!(self.out, "#extension GL_EXT_texture_shadow_lod : require")?;
        }

        // glsl es requires a precision to be specified for floats and ints
        // TODO: Should this be user configurable?
        if es {
            writeln!(self.out)?;
            writeln!(self.out, "precision highp float;")?;
            writeln!(self.out, "precision highp int;")?;
            writeln!(self.out)?;
        }

        if self.entry_point.stage == ShaderStage::Compute {
            let workgroup_size = self.entry_point.workgroup_size;
            writeln!(
                self.out,
                "layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;",
                workgroup_size[0], workgroup_size[1], workgroup_size[2]
            )?;
            writeln!(self.out)?;
        }

        // Enable early depth tests if needed
        if let Some(depth_test) = self.entry_point.early_depth_test {
            // If early depth test is supported for this version of GLSL
            if self.options.version.supports_early_depth_test() {
                writeln!(self.out, "layout(early_fragment_tests) in;")?;

                if let Some(conservative) = depth_test.conservative {
                    use crate::ConservativeDepth as Cd;

                    let depth = match conservative {
                        Cd::GreaterEqual => "greater",
                        Cd::LessEqual => "less",
                        Cd::Unchanged => "unchanged",
                    };
                    writeln!(self.out, "layout (depth_{}) out float gl_FragDepth;", depth)?;
                }
                writeln!(self.out)?;
            } else {
                log::warn!(
                    "Early depth testing is not supported for this version of GLSL: {}",
                    self.options.version
                );
            }
        }

        // Write all structs
        //
        // This are always ordered because of the IR is structured in a way that you can't make a
        // struct without adding all of it's members first
        for (handle, ty) in self.module.types.iter() {
            if let TypeInner::Struct { ref members, .. } = ty.inner {
                // No needed to write a struct that also should be written as a global variable
                let is_global_struct = self
                    .module
                    .global_variables
                    .iter()
                    .any(|e| e.1.ty == handle);

                if !is_global_struct {
                    self.write_struct(false, handle, members)?
                }
            }
        }

        let ep_info = self.info.get_entry_point(self.entry_point_idx as usize);

        // Write the globals
        //
        // We filter all globals that aren't used by the selected entry point as they might be
        // interfere with each other (i.e. two globals with the same location but different with
        // different classes)
        for (handle, global) in self.module.global_variables.iter() {
            if ep_info[handle].is_empty() {
                continue;
            }

            match self.module.types[global.ty].inner {
                // We treat images separately because they might require
                // writing the storage format
                TypeInner::Image {
                    mut dim,
                    arrayed,
                    class,
                } => {
                    // Gather the storage format if needed
                    let storage_format_access = match self.module.types[global.ty].inner {
                        TypeInner::Image {
                            class: crate::ImageClass::Storage { format, access },
                            ..
                        } => Some((format, access)),
                        _ => None,
                    };

                    if dim == crate::ImageDimension::D1 && es {
                        dim = crate::ImageDimension::D2
                    }

                    // Gether the location if needed
                    let layout_binding = if self.options.version.supports_explicit_locations() {
                        let br = global.binding.as_ref().unwrap();
                        self.options.binding_map.get(br).cloned()
                    } else {
                        None
                    };

                    // Write all the layout qualifiers
                    if layout_binding.is_some() || storage_format_access.is_some() {
                        write!(self.out, "layout(")?;
                        if let Some(binding) = layout_binding {
                            write!(self.out, "binding = {}", binding)?;
                        }
                        if let Some((format, _)) = storage_format_access {
                            let format_str = glsl_storage_format(format);
                            let separator = match layout_binding {
                                Some(_) => ",",
                                None => "",
                            };
                            write!(self.out, "{}{}", separator, format_str)?;
                        }
                        write!(self.out, ") ")?;
                    }

                    if let Some((_, access)) = storage_format_access {
                        self.write_storage_access(access)?;
                    }

                    // All images in glsl are `uniform`
                    // The trailing space is important
                    write!(self.out, "uniform ")?;

                    // write the type
                    //
                    // This is way we need the leading space because `write_image_type` doesn't add
                    // any spaces at the beginning or end
                    self.write_image_type(dim, arrayed, class)?;

                    // Finally write the name and end the global with a `;`
                    // The leading space is important
                    let global_name = self.get_global_name(handle, global);
                    writeln!(self.out, " {};", global_name)?;
                    writeln!(self.out)?;

                    self.reflection_names_globals.insert(handle, global_name);
                }
                // glsl has no concept of samplers so we just ignore it
                TypeInner::Sampler { .. } => continue,
                // All other globals are written by `write_global`
                _ => {
                    if !ep_info[handle].is_empty() {
                        self.write_global(handle, global)?;
                        // Add a newline (only for readability)
                        writeln!(self.out)?;
                    }
                }
            }
        }

        for arg in self.entry_point.function.arguments.iter() {
            self.write_varying(arg.binding.as_ref(), arg.ty, false)?;
        }
        if let Some(ref result) = self.entry_point.function.result {
            self.write_varying(result.binding.as_ref(), result.ty, true)?;
        }
        writeln!(self.out)?;

        // Write all regular functions
        for (handle, function) in self.module.functions.iter() {
            // Check that the function doesn't use globals that aren't supported
            // by the current entry point
            if !ep_info.dominates_global_use(&self.info[handle]) {
                continue;
            }

            let fun_info = &self.info[handle];

            // Write the function
            self.write_function(back::FunctionType::Function(handle), function, fun_info)?;

            writeln!(self.out)?;
        }

        self.write_function(
            back::FunctionType::EntryPoint(self.entry_point_idx),
            &self.entry_point.function,
            ep_info,
        )?;

        // Add newline at the end of file
        writeln!(self.out)?;

        // Collect all relection info and return it to the user
        self.collect_reflection_info()
    }

    fn write_array_size(&mut self, size: crate::ArraySize) -> BackendResult {
        write!(self.out, "[")?;

        // Write the array size
        // Writes nothing if `ArraySize::Dynamic`
        // Panics if `ArraySize::Constant` has a constant that isn't an sint or uint
        match size {
            crate::ArraySize::Constant(const_handle) => {
                match self.module.constants[const_handle].inner {
                    crate::ConstantInner::Scalar {
                        width: _,
                        value: crate::ScalarValue::Uint(size),
                    } => write!(self.out, "{}", size)?,
                    crate::ConstantInner::Scalar {
                        width: _,
                        value: crate::ScalarValue::Sint(size),
                    } => write!(self.out, "{}", size)?,
                    _ => unreachable!(),
                }
            }
            crate::ArraySize::Dynamic => (),
        }

        write!(self.out, "]")?;
        Ok(())
    }

    /// Helper method used to write value types
    ///
    /// # Notes
    /// Adds no trailing or leading whitespace
    ///
    /// # Panics
    /// - If type is either a image, a sampler, a pointer, or a struct
    /// - If it's an Array with a [`ArraySize::Constant`](crate::ArraySize::Constant) with a
    /// constant that isn't a [`Scalar`](crate::ConstantInner::Scalar) or if the
    /// scalar value isn't an [`Sint`](crate::ScalarValue::Sint) or [`Uint`](crate::ScalarValue::Uint)
    fn write_value_type(&mut self, inner: &TypeInner) -> BackendResult {
        match *inner {
            // Scalars are simple we just get the full name from `glsl_scalar`
            TypeInner::Scalar { kind, width }
            | TypeInner::Atomic { kind, width }
            | TypeInner::ValuePointer {
                size: None,
                kind,
                width,
                class: _,
            } => write!(self.out, "{}", glsl_scalar(kind, width)?.full)?,
            // Vectors are just `gvecN` where `g` is the scalar prefix and `N` is the vector size
            TypeInner::Vector { size, kind, width }
            | TypeInner::ValuePointer {
                size: Some(size),
                kind,
                width,
                class: _,
            } => write!(
                self.out,
                "{}vec{}",
                glsl_scalar(kind, width)?.prefix,
                size as u8
            )?,
            // Matrices are written with `gmatMxN` where `g` is the scalar prefix (only floats and
            // doubles are allowed), `M` is the columns count and `N` is the rows count
            //
            // glsl supports a matrix shorthand `gmatN` where `N` = `M` but it doesn't justify the
            // extra branch to write matrices this way
            TypeInner::Matrix {
                columns,
                rows,
                width,
            } => write!(
                self.out,
                "{}mat{}x{}",
                glsl_scalar(crate::ScalarKind::Float, width)?.prefix,
                columns as u8,
                rows as u8
            )?,
            // GLSL arrays are written as `type name[size]`
            // Current code is written arrays only as `[size]`
            // Base `type` and `name` should be written outside
            TypeInner::Array { size, .. } => self.write_array_size(size)?,
            // Panic if either Image, Sampler, Pointer, or a Struct is being written
            //
            // Write all variants instead of `_` so that if new variants are added a
            // no exhaustiveness error is thrown
            TypeInner::Pointer { .. }
            | TypeInner::Struct { .. }
            | TypeInner::Image { .. }
            | TypeInner::Sampler { .. } => {
                return Err(Error::Custom(format!("Unable to write type {:?}", inner)))
            }
        }

        Ok(())
    }

    /// Helper method used to write non image/sampler types
    ///
    /// # Notes
    /// Adds no trailing or leading whitespace
    ///
    /// # Panics
    /// - If type is either a image or sampler
    /// - If it's an Array with a [`ArraySize::Constant`](crate::ArraySize::Constant) with a
    /// constant that isn't a [`Scalar`](crate::ConstantInner::Scalar) or if the
    /// scalar value isn't an [`Sint`](crate::ScalarValue::Sint) or [`Uint`](crate::ScalarValue::Uint)
    fn write_type(&mut self, ty: Handle<crate::Type>) -> BackendResult {
        match self.module.types[ty].inner {
            // glsl has no pointer types so just write types as normal and loads are skipped
            TypeInner::Pointer { base, .. } => self.write_type(base),
            TypeInner::Struct {
                top_level: true,
                ref members,
                span: _,
            } => self.write_struct(true, ty, members),
            // glsl structs are written as just the struct name if it isn't a block
            TypeInner::Struct { .. } => {
                // Get the struct name
                let name = &self.names[&NameKey::Type(ty)];
                write!(self.out, "{}", name)?;
                Ok(())
            }
            // glsl array has the size separated from the base type
            TypeInner::Array { base, .. } => self.write_type(base),
            ref other => self.write_value_type(other),
        }
    }

    /// Helper method to write a image type
    ///
    /// # Notes
    /// Adds no leading or trailing whitespace
    fn write_image_type(
        &mut self,
        dim: crate::ImageDimension,
        arrayed: bool,
        class: crate::ImageClass,
    ) -> BackendResult {
        // glsl images consist of four parts the scalar prefix, the image "type", the dimensions
        // and modifiers
        //
        // There exists two image types
        // - sampler - for sampled images
        // - image - for storage images
        //
        // There are three possible modifiers that can be used together and must be written in
        // this order to be valid
        // - MS - used if it's a multisampled image
        // - Array - used if it's an image array
        // - Shadow - used if it's a depth image
        use crate::ImageClass as Ic;

        let (base, kind, ms, comparison) = match class {
            Ic::Sampled { kind, multi: true } => ("sampler", kind, "MS", ""),
            Ic::Sampled { kind, multi: false } => ("sampler", kind, "", ""),
            Ic::Depth { multi: true } => ("sampler", crate::ScalarKind::Float, "MS", ""),
            Ic::Depth { multi: false } => ("sampler", crate::ScalarKind::Float, "", "Shadow"),
            Ic::Storage { format, .. } => ("image", format.into(), "", ""),
        };

        write!(
            self.out,
            "highp {}{}{}{}{}{}",
            glsl_scalar(kind, 4)?.prefix,
            base,
            glsl_dimension(dim),
            ms,
            if arrayed { "Array" } else { "" },
            comparison
        )?;

        Ok(())
    }

    /// Helper method used to write non images/sampler globals
    ///
    /// # Notes
    /// Adds a newline
    ///
    /// # Panics
    /// If the global has type sampler
    fn write_global(
        &mut self,
        handle: Handle<crate::GlobalVariable>,
        global: &crate::GlobalVariable,
    ) -> BackendResult {
        if self.options.version.supports_explicit_locations() {
            if let Some(ref br) = global.binding {
                match self.options.binding_map.get(br) {
                    Some(binding) => {
                        let layout = match global.class {
                            crate::StorageClass::Storage { .. } => {
                                if self.options.version.supports_std430_layout() {
                                    "std430, "
                                } else {
                                    "std140, "
                                }
                            }
                            crate::StorageClass::Uniform => "std140, ",
                            _ => "",
                        };
                        write!(self.out, "layout({}binding = {}) ", layout, binding)?
                    }
                    None => {
                        log::debug!("unassigned binding for {:?}", global.name);
                        if let crate::StorageClass::Storage { .. } = global.class {
                            if self.options.version.supports_std430_layout() {
                                write!(self.out, "layout(std430) ")?
                            }
                        }
                    }
                }
            }
        }

        if let crate::StorageClass::Storage { access } = global.class {
            self.write_storage_access(access)?;
        }

        // Write the storage class
        // Trailing space is important
        if let Some(storage_class) = glsl_storage_class(global.class) {
            write!(self.out, "{} ", storage_class)?;
        } else if let TypeInner::Struct {
            top_level: true, ..
        } = self.module.types[global.ty].inner
        {
            write!(self.out, "struct ")?;
        }

        // Write the type
        // `write_type` adds no leading or trailing spaces
        self.write_type(global.ty)?;

        // Finally write the global name and end the global with a `;` and a newline
        // Leading space is important
        write!(self.out, " ")?;
        self.write_global_name(handle, global)?;
        if let TypeInner::Array { size, .. } = self.module.types[global.ty].inner {
            self.write_array_size(size)?;
        }

        if is_value_init_supported(self.module, global.ty) {
            write!(self.out, " = ")?;
            if let Some(init) = global.init {
                self.write_constant(init)?;
            } else {
                self.write_zero_init_value(global.ty)?;
            }
        }
        writeln!(self.out, ";")?;

        Ok(())
    }

    /// Helper method used to get a name for a global
    ///
    /// Globals have different naming schemes depending on their binding:
    /// - Globals without bindings use the name from the [`Namer`](crate::proc::Namer)
    /// - Globals with resource binding are named `_group_X_binding_Y` where `X`
    ///   is the group and `Y` is the binding
    fn get_global_name(
        &self,
        handle: Handle<crate::GlobalVariable>,
        global: &crate::GlobalVariable,
    ) -> String {
        match global.binding {
            Some(ref br) => {
                format!("_group_{}_binding_{}", br.group, br.binding)
            }
            None => self.names[&NameKey::GlobalVariable(handle)].clone(),
        }
    }

    /// Helper method used to write a name for a global without additional heap allocation
    fn write_global_name(
        &mut self,
        handle: Handle<crate::GlobalVariable>,
        global: &crate::GlobalVariable,
    ) -> BackendResult {
        match global.binding {
            Some(ref br) => write!(self.out, "_group_{}_binding_{}", br.group, br.binding)?,
            None => write!(
                self.out,
                "{}",
                &self.names[&NameKey::GlobalVariable(handle)]
            )?,
        }

        Ok(())
    }

    /// Writes the varying declaration.
    fn write_varying(
        &mut self,
        binding: Option<&crate::Binding>,
        ty: Handle<crate::Type>,
        output: bool,
    ) -> Result<(), Error> {
        match self.module.types[ty].inner {
            crate::TypeInner::Struct { ref members, .. } => {
                for member in members {
                    self.write_varying(member.binding.as_ref(), member.ty, output)?;
                }
            }
            _ => {
                let (location, interpolation, sampling) = match binding {
                    Some(&crate::Binding::Location {
                        location,
                        interpolation,
                        sampling,
                    }) => (location, interpolation, sampling),
                    _ => return Ok(()),
                };

                // Write the interpolation modifier if needed
                //
                // We ignore all interpolation and auxiliary modifiers that aren't used in fragment
                // shaders' input globals or vertex shaders' output globals.
                let emit_interpolation_and_auxiliary = match self.entry_point.stage {
                    ShaderStage::Vertex => output,
                    ShaderStage::Fragment => !output,
                    _ => false,
                };

                // Write the I/O locations, if allowed
                if self.options.version.supports_explicit_locations()
                    || !emit_interpolation_and_auxiliary
                {
                    write!(self.out, "layout(location = {}) ", location)?;
                }

                // Write the interpolation qualifier.
                if let Some(interp) = interpolation {
                    if emit_interpolation_and_auxiliary {
                        write!(self.out, "{} ", glsl_interpolation(interp))?;
                    }
                }

                // Write the sampling auxiliary qualifier.
                //
                // Before GLSL 4.2, the `centroid` and `sample` qualifiers were required to appear
                // immediately before the `in` / `out` qualifier, so we'll just follow that rule
                // here, regardless of the version.
                if let Some(sampling) = sampling {
                    if emit_interpolation_and_auxiliary {
                        if let Some(qualifier) = glsl_sampling(sampling) {
                            write!(self.out, "{} ", qualifier)?;
                        }
                    }
                }

                // Write the input/output qualifier.
                write!(self.out, "{} ", if output { "out" } else { "in" })?;

                // Write the type
                // `write_type` adds no leading or trailing spaces
                self.write_type(ty)?;

                // Finally write the global name and end the global with a `;` and a newline
                // Leading space is important
                let vname = VaryingName {
                    binding: &crate::Binding::Location {
                        location,
                        interpolation: None,
                        sampling: None,
                    },
                    stage: self.entry_point.stage,
                    output,
                };
                writeln!(self.out, " {};", vname)?;
            }
        }
        Ok(())
    }

    /// Helper method used to write functions (both entry points and regular functions)
    ///
    /// # Notes
    /// Adds a newline
    fn write_function(
        &mut self,
        ty: back::FunctionType,
        func: &crate::Function,
        info: &valid::FunctionInfo,
    ) -> BackendResult {
        // Create a function context for the function being written
        let ctx = back::FunctionCtx {
            ty,
            info,
            expressions: &func.expressions,
            named_expressions: &func.named_expressions,
        };

        self.named_expressions.clear();

        // Write the function header
        //
        // glsl headers are the same as in c:
        // `ret_type name(args)`
        // `ret_type` is the return type
        // `name` is the function name
        // `args` is a comma separated list of `type name`
        //  | - `type` is the argument type
        //  | - `name` is the argument name

        // Start by writing the return type if any otherwise write void
        // This is the only place where `void` is a valid type
        // (though it's more a keyword than a type)
        if let back::FunctionType::EntryPoint(_) = ctx.ty {
            write!(self.out, "void")?;
        } else if let Some(ref result) = func.result {
            self.write_type(result.ty)?;
        } else {
            write!(self.out, "void")?;
        }

        // Write the function name and open parentheses for the argument list
        let function_name = match ctx.ty {
            back::FunctionType::Function(handle) => &self.names[&NameKey::Function(handle)],
            back::FunctionType::EntryPoint(_) => "main",
        };
        write!(self.out, " {}(", function_name)?;

        // Write the comma separated argument list
        //
        // We need access to `Self` here so we use the reference passed to the closure as an
        // argument instead of capturing as that would cause a borrow checker error
        let arguments = match ctx.ty {
            back::FunctionType::EntryPoint(_) => &[][..],
            back::FunctionType::Function(_) => &func.arguments,
        };
        let arguments: Vec<_> = arguments
            .iter()
            .filter(|arg| match self.module.types[arg.ty].inner {
                TypeInner::Sampler { .. } => false,
                _ => true,
            })
            .collect();
        self.write_slice(&arguments, |this, i, arg| {
            // Write the argument type
            match this.module.types[arg.ty].inner {
                // We treat images separately because they might require
                // writing the storage format
                TypeInner::Image {
                    dim,
                    arrayed,
                    class,
                } => {
                    // Write the storage format if needed
                    if let TypeInner::Image {
                        class: crate::ImageClass::Storage { format, .. },
                        ..
                    } = this.module.types[arg.ty].inner
                    {
                        write!(this.out, "layout({}) ", glsl_storage_format(format))?;
                    }

                    // write the type
                    //
                    // This is way we need the leading space because `write_image_type` doesn't add
                    // any spaces at the beginning or end
                    this.write_image_type(dim, arrayed, class)?;
                }
                TypeInner::Pointer { base, .. } => {
                    // write parameter qualifiers
                    write!(this.out, "inout ")?;
                    this.write_type(base)?;
                }
                // All other types are written by `write_type`
                _ => {
                    this.write_type(arg.ty)?;
                }
            }

            // Write the argument name
            // The leading space is important
            write!(this.out, " {}", &this.names[&ctx.argument_key(i)])?;

            Ok(())
        })?;

        // Close the parentheses and open braces to start the function body
        writeln!(self.out, ") {{")?;

        // Compose the function arguments from globals, in case of an entry point.
        if let back::FunctionType::EntryPoint(ep_index) = ctx.ty {
            let stage = self.module.entry_points[ep_index as usize].stage;
            for (index, arg) in func.arguments.iter().enumerate() {
                write!(self.out, "{}", back::INDENT)?;
                self.write_type(arg.ty)?;
                let name = &self.names[&NameKey::EntryPointArgument(ep_index, index as u32)];
                write!(self.out, " {}", name)?;
                write!(self.out, " = ")?;
                match self.module.types[arg.ty].inner {
                    crate::TypeInner::Struct { ref members, .. } => {
                        self.write_type(arg.ty)?;
                        write!(self.out, "(")?;
                        for (index, member) in members.iter().enumerate() {
                            let varying_name = VaryingName {
                                binding: member.binding.as_ref().unwrap(),
                                stage,
                                output: false,
                            };
                            if index != 0 {
                                write!(self.out, ", ")?;
                            }
                            write!(self.out, "{}", varying_name)?;
                        }
                        writeln!(self.out, ");")?;
                    }
                    _ => {
                        let varying_name = VaryingName {
                            binding: arg.binding.as_ref().unwrap(),
                            stage,
                            output: false,
                        };
                        writeln!(self.out, "{};", varying_name)?;
                    }
                }
            }
        }

        // Write all function locals
        // Locals are `type name (= init)?;` where the init part (including the =) are optional
        //
        // Always adds a newline
        for (handle, local) in func.local_variables.iter() {
            // Write indentation (only for readability) and the type
            // `write_type` adds no trailing space
            write!(self.out, "{}", back::INDENT)?;
            self.write_type(local.ty)?;

            // Write the local name
            // The leading space is important
            write!(self.out, " {}", self.names[&ctx.name_key(handle)])?;
            // Write size for array type
            if let TypeInner::Array { size, .. } = self.module.types[local.ty].inner {
                self.write_array_size(size)?;
            }
            // Write the local initializer if needed
            if let Some(init) = local.init {
                // Put the equal signal only if there's a initializer
                // The leading and trailing spaces aren't needed but help with readability
                write!(self.out, " = ")?;

                // Write the constant
                // `write_constant` adds no trailing or leading space/newline
                self.write_constant(init)?;
            } else if is_value_init_supported(self.module, local.ty) {
                write!(self.out, " = ")?;
                self.write_zero_init_value(local.ty)?;
            }

            // Finish the local with `;` and add a newline (only for readability)
            writeln!(self.out, ";")?
        }

        // Write the function body (statement list)
        for sta in func.body.iter() {
            // Write a statement, the indentation should always be 1 when writing the function body
            // `write_stmt` adds a newline
            self.write_stmt(sta, &ctx, back::Level(1))?;
        }

        // Close braces and add a newline
        writeln!(self.out, "}}")?;

        Ok(())
    }

    /// Helper method that writes a list of comma separated `T` with a writer function `F`
    ///
    /// The writer function `F` receives a mutable reference to `self` that if needed won't cause
    /// borrow checker issues (using for example a closure with `self` will cause issues), the
    /// second argument is the 0 based index of the element on the list, and the last element is
    /// a reference to the element `T` being written
    ///
    /// # Notes
    /// - Adds no newlines or leading/trailing whitespace
    /// - The last element won't have a trailing `,`
    fn write_slice<T, F: FnMut(&mut Self, u32, &T) -> BackendResult>(
        &mut self,
        data: &[T],
        mut f: F,
    ) -> BackendResult {
        // Loop trough `data` invoking `f` for each element
        for (i, item) in data.iter().enumerate() {
            f(self, i as u32, item)?;

            // Only write a comma if isn't the last element
            if i != data.len().saturating_sub(1) {
                // The leading space is for readability only
                write!(self.out, ", ")?;
            }
        }

        Ok(())
    }

    /// Helper method used to write constants
    ///
    /// # Notes
    /// Adds no newlines or leading/trailing whitespace
    fn write_constant(&mut self, handle: Handle<crate::Constant>) -> BackendResult {
        use crate::ScalarValue as Sv;

        match self.module.constants[handle].inner {
            crate::ConstantInner::Scalar {
                width: _,
                ref value,
            } => match *value {
                // Signed integers don't need anything special
                Sv::Sint(int) => write!(self.out, "{}", int)?,
                // Unsigned integers need a `u` at the end
                //
                // While `core` doesn't necessarily need it, it's allowed and since `es` needs it we
                // always write it as the extra branch wouldn't have any benefit in readability
                Sv::Uint(int) => write!(self.out, "{}u", int)?,
                // Floats are written using `Debug` instead of `Display` because it always appends the
                // decimal part even it's zero which is needed for a valid glsl float constant
                Sv::Float(float) => write!(self.out, "{:?}", float)?,
                // Booleans are either `true` or `false` so nothing special needs to be done
                Sv::Bool(boolean) => write!(self.out, "{}", boolean)?,
            },
            // Composite constant are created using the same syntax as compose
            // `type(components)` where `components` is a comma separated list of constants
            crate::ConstantInner::Composite { ty, ref components } => {
                self.write_type(ty)?;
                write!(self.out, "(")?;

                // Write the comma separated constants
                self.write_slice(components, |this, _, arg| this.write_constant(*arg))?;

                write!(self.out, ")")?
            }
        }

        Ok(())
    }

    /// Helper method used to write structs
    ///
    /// # Notes
    /// Ends in a newline
    fn write_struct(
        &mut self,
        block: bool,
        handle: Handle<crate::Type>,
        members: &[crate::StructMember],
    ) -> BackendResult {
        // glsl structs are written as in C
        // `struct name() { members };`
        //  | `struct` is a keyword
        //  | `name` is the struct name
        //  | `members` is a semicolon separated list of `type name`
        //      | `type` is the member type
        //      | `name` is the member name
        let name = &self.names[&NameKey::Type(handle)];

        // If struct is a block we need to write `block_name { members }` where `block_name` must be
        // unique between blocks and structs so we add `_block_ID` where `ID` is a `IdGenerator`
        // generated number so it's unique and `members` are the same as in a struct
        if block {
            // Write the block name, it's just the struct name appended with `_block_ID`
            let stage_postfix = match self.entry_point.stage {
                ShaderStage::Vertex => "Vs",
                ShaderStage::Fragment => "Fs",
                ShaderStage::Compute => "Cs",
            };
            let block_name = format!(
                "{}_block_{}{}",
                name,
                self.block_id.generate(),
                stage_postfix
            );
            writeln!(self.out, "{} {{", block_name)?;

            self.reflection_names_uniforms.insert(handle, block_name);
        } else {
            writeln!(self.out, "struct {} {{", name)?;
        }

        for (idx, member) in members.iter().enumerate() {
            // The indentation is only for readability
            write!(self.out, "{}", back::INDENT)?;

            match self.module.types[member.ty].inner {
                TypeInner::Array {
                    base,
                    size,
                    stride: _,
                } => {
                    self.write_type(base)?;
                    write!(
                        self.out,
                        " {}",
                        &self.names[&NameKey::StructMember(handle, idx as u32)]
                    )?;
                    // Write [size]
                    self.write_array_size(size)?;
                    // Newline is important
                    writeln!(self.out, ";")?;
                }
                _ => {
                    // Write the member type
                    // Adds no trailing space
                    self.write_type(member.ty)?;

                    // Write the member name and put a semicolon
                    // The leading space is important
                    // All members must have a semicolon even the last one
                    writeln!(
                        self.out,
                        " {};",
                        &self.names[&NameKey::StructMember(handle, idx as u32)]
                    )?;
                }
            }
        }

        write!(self.out, "}}")?;

        if !block {
            writeln!(self.out, ";")?;
            // Add a newline for readability
            writeln!(self.out)?;
        }

        Ok(())
    }

    /// Helper method used to write statements
    ///
    /// # Notes
    /// Always adds a newline
    fn write_stmt(
        &mut self,
        sta: &crate::Statement,
        ctx: &back::FunctionCtx,
        level: back::Level,
    ) -> BackendResult {
        use crate::Statement;

        match *sta {
            // This is where we can generate intermediate constants for some expression types.
            Statement::Emit(ref range) => {
                for handle in range.clone() {
                    let info = &ctx.info[handle];
                    let ptr_class = info.ty.inner_with(&self.module.types).pointer_class();
                    let expr_name = if ptr_class.is_some() {
                        // GLSL can't save a pointer-valued expression in a variable,
                        // but we shouldn't ever need to: they should never be named expressions,
                        // and none of the expression types flagged by bake_ref_count can be pointer-valued.
                        None
                    } else if let Some(name) = ctx.named_expressions.get(&handle) {
                        // Front end provides names for all variables at the start of writing.
                        // But we write them to step by step. We need to recache them
                        // Otherwise, we could accidentally write variable name instead of full expression.
                        // Also, we use sanitized names! It defense backend from generating variable with name from reserved keywords.
                        Some(self.namer.call(name))
                    } else {
                        let min_ref_count = ctx.expressions[handle].bake_ref_count();
                        if min_ref_count <= info.ref_count {
                            Some(format!("{}{}", super::BAKE_PREFIX, handle.index()))
                        } else {
                            None
                        }
                    };

                    if let Some(name) = expr_name {
                        write!(self.out, "{}", level)?;
                        self.write_named_expr(handle, name, ctx)?;
                    }
                }
            }
            // Blocks are simple we just need to write the block statements between braces
            // We could also just print the statements but this is more readable and maps more
            // closely to the IR
            Statement::Block(ref block) => {
                write!(self.out, "{}", level)?;
                writeln!(self.out, "{{")?;
                for sta in block.iter() {
                    // Increase the indentation to help with readability
                    self.write_stmt(sta, ctx, level.next())?
                }
                writeln!(self.out, "{}}}", level)?
            }
            // Ifs are written as in C:
            // ```
            // if(condition) {
            //  accept
            // } else {
            //  reject
            // }
            // ```
            Statement::If {
                condition,
                ref accept,
                ref reject,
            } => {
                write!(self.out, "{}", level)?;
                write!(self.out, "if (")?;
                self.write_expr(condition, ctx)?;
                writeln!(self.out, ") {{")?;

                for sta in accept {
                    // Increase indentation to help with readability
                    self.write_stmt(sta, ctx, level.next())?;
                }

                // If there are no statements in the reject block we skip writing it
                // This is only for readability
                if !reject.is_empty() {
                    writeln!(self.out, "{}}} else {{", level)?;

                    for sta in reject {
                        // Increase indentation to help with readability
                        self.write_stmt(sta, ctx, level.next())?;
                    }
                }

                writeln!(self.out, "{}}}", level)?
            }
            // Switch are written as in C:
            // ```
            // switch (selector) {
            //      // Fallthrough
            //      case label:
            //          block
            //      // Non fallthrough
            //      case label:
            //          block
            //          break;
            //      default:
            //          block
            //  }
            //  ```
            //  Where the `default` case happens isn't important but we put it last
            //  so that we don't need to print a `break` for it
            Statement::Switch {
                selector,
                ref cases,
            } => {
                // Start the switch
                write!(self.out, "{}", level)?;
                write!(self.out, "switch(")?;
                self.write_expr(selector, ctx)?;
                writeln!(self.out, ") {{")?;
                let type_postfix = match *ctx.info[selector].ty.inner_with(&self.module.types) {
                    crate::TypeInner::Scalar {
                        kind: crate::ScalarKind::Uint,
                        ..
                    } => "u",
                    _ => "",
                };

                // Write all cases
                let l2 = level.next();
                for case in cases {
                    match case.value {
                        crate::SwitchValue::Integer(value) => {
                            writeln!(self.out, "{}case {}{}:", l2, value, type_postfix)?
                        }
                        crate::SwitchValue::Default => writeln!(self.out, "{}default:", l2)?,
                    }

                    for sta in case.body.iter() {
                        self.write_stmt(sta, ctx, l2.next())?;
                    }

                    // Write fallthrough comment if the case is fallthrough,
                    // otherwise write a break, if the case is not already
                    // broken out of at the end of its body.
                    if case.fall_through {
                        writeln!(self.out, "{}/* fallthrough */", l2.next())?;
                    } else if case.body.last().map_or(true, |s| !s.is_terminator()) {
                        writeln!(self.out, "{}break;", l2.next())?;
                    }
                }

                writeln!(self.out, "{}}}", level)?
            }
            // Loops in naga IR are based on wgsl loops, glsl can emulate the behaviour by using a
            // while true loop and appending the continuing block to the body resulting on:
            // ```
            // bool loop_init = true;
            // while(true) {
            //  if (!loop_init) { <continuing> }
            //  loop_init = false;
            //  <body>
            // }
            // ```
            Statement::Loop {
                ref body,
                ref continuing,
            } => {
                if !continuing.is_empty() {
                    let gate_name = self.namer.call("loop_init");
                    writeln!(self.out, "{}bool {} = true;", level, gate_name)?;
                    writeln!(self.out, "{}while(true) {{", level)?;
                    writeln!(self.out, "{}if (!{}) {{", level.next(), gate_name)?;
                    for sta in continuing {
                        self.write_stmt(sta, ctx, level.next())?;
                    }
                    writeln!(self.out, "{}}}", level.next())?;
                    writeln!(self.out, "{}{} = false;", level.next(), gate_name)?;
                } else {
                    writeln!(self.out, "{}while(true) {{", level)?;
                }
                for sta in body {
                    self.write_stmt(sta, ctx, level.next())?;
                }
                writeln!(self.out, "{}}}", level)?
            }
            // Break, continue and return as written as in C
            // `break;`
            Statement::Break => {
                write!(self.out, "{}", level)?;
                writeln!(self.out, "break;")?
            }
            // `continue;`
            Statement::Continue => {
                write!(self.out, "{}", level)?;
                writeln!(self.out, "continue;")?
            }
            // `return expr;`, `expr` is optional
            Statement::Return { value } => {
                write!(self.out, "{}", level)?;
                match ctx.ty {
                    back::FunctionType::Function(_) => {
                        write!(self.out, "return")?;
                        // Write the expression to be returned if needed
                        if let Some(expr) = value {
                            write!(self.out, " ")?;
                            self.write_expr(expr, ctx)?;
                        }
                        writeln!(self.out, ";")?;
                    }
                    back::FunctionType::EntryPoint(ep_index) => {
                        let ep = &self.module.entry_points[ep_index as usize];
                        if let Some(ref result) = ep.function.result {
                            let value = value.unwrap();
                            match self.module.types[result.ty].inner {
                                crate::TypeInner::Struct { ref members, .. } => {
                                    let temp_struct_name = match ctx.expressions[value] {
                                        crate::Expression::Compose { .. } => {
                                            let return_struct = "_tmp_return";
                                            write!(
                                                self.out,
                                                "{} {} = ",
                                                &self.names[&NameKey::Type(result.ty)],
                                                return_struct
                                            )?;
                                            self.write_expr(value, ctx)?;
                                            writeln!(self.out, ";")?;
                                            write!(self.out, "{}", level)?;
                                            Some(return_struct)
                                        }
                                        _ => None,
                                    };

                                    for (index, member) in members.iter().enumerate() {
                                        // TODO: handle builtin in better way
                                        if let Some(crate::Binding::BuiltIn(builtin)) =
                                            member.binding
                                        {
                                            match builtin {
                                                crate::BuiltIn::ClipDistance
                                                | crate::BuiltIn::CullDistance
                                                | crate::BuiltIn::PointSize => {
                                                    if self.options.version.is_es() {
                                                        continue;
                                                    }
                                                }
                                                _ => {}
                                            }
                                        }

                                        let varying_name = VaryingName {
                                            binding: member.binding.as_ref().unwrap(),
                                            stage: ep.stage,
                                            output: true,
                                        };
                                        write!(self.out, "{} = ", varying_name)?;

                                        if let Some(struct_name) = temp_struct_name {
                                            write!(self.out, "{}", struct_name)?;
                                        } else {
                                            self.write_expr(value, ctx)?;
                                        }

                                        // Write field name
                                        writeln!(
                                            self.out,
                                            ".{};",
                                            &self.names
                                                [&NameKey::StructMember(result.ty, index as u32)]
                                        )?;
                                        write!(self.out, "{}", level)?;
                                    }
                                }
                                _ => {
                                    let name = VaryingName {
                                        binding: result.binding.as_ref().unwrap(),
                                        stage: ep.stage,
                                        output: true,
                                    };
                                    write!(self.out, "{} = ", name)?;
                                    self.write_expr(value, ctx)?;
                                    writeln!(self.out, ";")?;
                                    write!(self.out, "{}", level)?;
                                }
                            }
                        }

                        if let back::FunctionType::EntryPoint(ep_index) = ctx.ty {
                            if self.module.entry_points[ep_index as usize].stage
                                == crate::ShaderStage::Vertex
                                && self
                                    .options
                                    .writer_flags
                                    .contains(WriterFlags::ADJUST_COORDINATE_SPACE)
                            {
                                writeln!(
                                    self.out,
                                    "gl_Position.yz = vec2(-gl_Position.y, gl_Position.z * 2.0 - gl_Position.w);",
                                )?;
                                write!(self.out, "{}", level)?;
                            }
                        }
                        writeln!(self.out, "return;")?;
                    }
                }
            }
            // This is one of the places were glsl adds to the syntax of C in this case the discard
            // keyword which ceases all further processing in a fragment shader, it's called OpKill
            // in spir-v that's why it's called `Statement::Kill`
            Statement::Kill => writeln!(self.out, "{}discard;", level)?,
            // Issue an execution or a memory barrier.
            Statement::Barrier(flags) => {
                if flags.is_empty() {
                    writeln!(self.out, "{}barrier();", level)?;
                } else {
                    writeln!(self.out, "{}groupMemoryBarrier();", level)?;
                }
            }
            // Stores in glsl are just variable assignments written as `pointer = value;`
            Statement::Store { pointer, value } => {
                write!(self.out, "{}", level)?;
                self.write_expr(pointer, ctx)?;
                write!(self.out, " = ")?;
                self.write_expr(value, ctx)?;
                writeln!(self.out, ";")?
            }
            // Stores a value into an image.
            Statement::ImageStore {
                image,
                coordinate,
                array_index,
                value,
            } => {
                write!(self.out, "{}", level)?;
                // This will only panic if the module is invalid
                let dim = match *ctx.info[image].ty.inner_with(&self.module.types) {
                    TypeInner::Image { dim, .. } => dim,
                    _ => unreachable!(),
                };

                write!(self.out, "imageStore(")?;
                self.write_expr(image, ctx)?;
                write!(self.out, ", ")?;
                self.write_texture_coordinates(coordinate, array_index, dim, ctx)?;
                write!(self.out, ", ")?;
                self.write_expr(value, ctx)?;
                writeln!(self.out, ");")?;
            }
            // A `Call` is written `name(arguments)` where `arguments` is a comma separated expressions list
            Statement::Call {
                function,
                ref arguments,
                result,
            } => {
                write!(self.out, "{}", level)?;
                if let Some(expr) = result {
                    let name = format!("{}{}", super::BAKE_PREFIX, expr.index());
                    let result = self.module.functions[function].result.as_ref().unwrap();
                    self.write_type(result.ty)?;
                    write!(self.out, " {} = ", name)?;
                    self.named_expressions.insert(expr, name);
                }
                write!(self.out, "{}(", &self.names[&NameKey::Function(function)])?;
                let arguments: Vec<_> = arguments
                    .iter()
                    .enumerate()
                    .filter_map(|(i, arg)| {
                        let arg_ty = self.module.functions[function].arguments[i].ty;
                        match self.module.types[arg_ty].inner {
                            TypeInner::Sampler { .. } => None,
                            _ => Some(*arg),
                        }
                    })
                    .collect();
                self.write_slice(&arguments, |this, _, arg| this.write_expr(*arg, ctx))?;
                writeln!(self.out, ");")?
            }
            Statement::Atomic {
                pointer,
                ref fun,
                value,
                result,
            } => {
                write!(self.out, "{}", level)?;
                let res_name = format!("{}{}", super::BAKE_PREFIX, result.index());
                let res_ty = ctx.info[result].ty.inner_with(&self.module.types);
                self.write_value_type(res_ty)?;
                write!(self.out, " {} = ", res_name)?;
                self.named_expressions.insert(result, res_name);

                let fun_str = fun.to_glsl();
                write!(self.out, "atomic{}(", fun_str)?;
                self.write_expr(pointer, ctx)?;
                write!(self.out, ", ")?;
                // handle the special cases
                match *fun {
                    crate::AtomicFunction::Subtract => {
                        // we just wrote `InterlockedAdd`, so negate the argument
                        write!(self.out, "-")?;
                    }
                    crate::AtomicFunction::Exchange { compare: Some(_) } => {
                        return Err(Error::Custom(
                            "atomic CompareExchange is not implemented".to_string(),
                        ));
                    }
                    _ => {}
                }
                self.write_expr(value, ctx)?;
                writeln!(self.out, ");")?;
            }
        }

        Ok(())
    }

    /// Helper method to write expressions
    ///
    /// # Notes
    /// Doesn't add any newlines or leading/trailing spaces
    fn write_expr(
        &mut self,
        expr: Handle<crate::Expression>,
        ctx: &back::FunctionCtx<'_>,
    ) -> BackendResult {
        use crate::Expression;

        if let Some(name) = self.named_expressions.get(&expr) {
            write!(self.out, "{}", name)?;
            return Ok(());
        }

        match ctx.expressions[expr] {
            // `Access` is applied to arrays, vectors and matrices and is written as indexing
            Expression::Access { base, index } => {
                self.write_expr(base, ctx)?;
                write!(self.out, "[")?;
                self.write_expr(index, ctx)?;
                write!(self.out, "]")?
            }
            // `AccessIndex` is the same as `Access` except that the index is a constant and it can
            // be applied to structs, in this case we need to find the name of the field at that
            // index and write `base.field_name`
            Expression::AccessIndex { base, index } => {
                self.write_expr(base, ctx)?;

                let base_ty_res = &ctx.info[base].ty;
                let mut resolved = base_ty_res.inner_with(&self.module.types);
                let base_ty_handle = match *resolved {
                    TypeInner::Pointer { base, class: _ } => {
                        resolved = &self.module.types[base].inner;
                        Some(base)
                    }
                    _ => base_ty_res.handle(),
                };

                match *resolved {
                    TypeInner::Vector { .. } => {
                        // Write vector access as a swizzle
                        write!(self.out, ".{}", back::COMPONENTS[index as usize])?
                    }
                    TypeInner::Matrix { .. }
                    | TypeInner::Array { .. }
                    | TypeInner::ValuePointer { .. } => write!(self.out, "[{}]", index)?,
                    TypeInner::Struct { .. } => {
                        // This will never panic in case the type is a `Struct`, this is not true
                        // for other types so we can only check while inside this match arm
                        let ty = base_ty_handle.unwrap();

                        write!(
                            self.out,
                            ".{}",
                            &self.names[&NameKey::StructMember(ty, index)]
                        )?
                    }
                    ref other => return Err(Error::Custom(format!("Cannot index {:?}", other))),
                }
            }
            // Constants are delegated to `write_constant`
            Expression::Constant(constant) => self.write_constant(constant)?,
            // `Splat` needs to actually write down a vector, it's not always inferred in GLSL.
            Expression::Splat { size: _, value } => {
                let resolved = ctx.info[expr].ty.inner_with(&self.module.types);
                self.write_value_type(resolved)?;
                write!(self.out, "(")?;
                self.write_expr(value, ctx)?;
                write!(self.out, ")")?
            }
            // `Swizzle` adds a few letters behind the dot.
            Expression::Swizzle {
                size,
                vector,
                pattern,
            } => {
                self.write_expr(vector, ctx)?;
                write!(self.out, ".")?;
                for &sc in pattern[..size as usize].iter() {
                    self.out.write_char(back::COMPONENTS[sc as usize])?;
                }
            }
            // `Compose` is pretty simple we just write `type(components)` where `components` is a
            // comma separated list of expressions
            Expression::Compose { ty, ref components } => {
                self.write_type(ty)?;

                let resolved = ctx.info[expr].ty.inner_with(&self.module.types);
                if let TypeInner::Array { size, .. } = *resolved {
                    self.write_array_size(size)?;
                }

                write!(self.out, "(")?;
                self.write_slice(components, |this, _, arg| this.write_expr(*arg, ctx))?;
                write!(self.out, ")")?
            }
            // Function arguments are written as the argument name
            Expression::FunctionArgument(pos) => {
                write!(self.out, "{}", &self.names[&ctx.argument_key(pos)])?
            }
            // Global variables need some special work for their name but
            // `get_global_name` does the work for us
            Expression::GlobalVariable(handle) => {
                let global = &self.module.global_variables[handle];
                self.write_global_name(handle, global)?
            }
            // A local is written as it's name
            Expression::LocalVariable(handle) => {
                write!(self.out, "{}", self.names[&ctx.name_key(handle)])?
            }
            // glsl has no pointers so there's no load operation, just write the pointer expression
            Expression::Load { pointer } => self.write_expr(pointer, ctx)?,
            // `ImageSample` is a bit complicated compared to the rest of the IR.
            //
            // First there are three variations depending wether the sample level is explicitly set,
            // if it's automatic or it it's bias:
            // `texture(image, coordinate)` - Automatic sample level
            // `texture(image, coordinate, bias)` - Bias sample level
            // `textureLod(image, coordinate, level)` - Zero or Exact sample level
            //
            // Furthermore if `depth_ref` is some we need to append it to the coordinate vector
            Expression::ImageSample {
                image,
                sampler: _, //TODO?
                coordinate,
                array_index,
                offset,
                level,
                depth_ref,
            } => {
                let dim = match *ctx.info[image].ty.inner_with(&self.module.types) {
                    TypeInner::Image { dim, .. } => dim,
                    _ => unreachable!(),
                };

                if dim == crate::ImageDimension::Cube
                    && array_index.is_some()
                    && depth_ref.is_some()
                {
                    match level {
                        crate::SampleLevel::Zero
                        | crate::SampleLevel::Exact(_)
                        | crate::SampleLevel::Gradient { .. }
                        | crate::SampleLevel::Bias(_) => {
                            return Err(Error::Custom(String::from(
                                "gsamplerCubeArrayShadow isn't supported in textureGrad, \
                                 textureLod or texture with bias",
                            )))
                        }
                        crate::SampleLevel::Auto => {}
                    }
                }

                // textureLod on sampler2DArrayShadow and samplerCubeShadow does not exist in GLSL.
                // To emulate this, we will have to use textureGrad with a constant gradient of 0.
                let workaround_lod_array_shadow_as_grad = (array_index.is_some()
                    || dim == crate::ImageDimension::Cube)
                    && depth_ref.is_some()
                    && !self
                        .options
                        .writer_flags
                        .contains(WriterFlags::TEXTURE_SHADOW_LOD);

                //Write the function to be used depending on the sample level
                let fun_name = match level {
                    crate::SampleLevel::Auto | crate::SampleLevel::Bias(_) => "texture",
                    crate::SampleLevel::Zero | crate::SampleLevel::Exact(_) => {
                        if workaround_lod_array_shadow_as_grad {
                            "textureGrad"
                        } else {
                            "textureLod"
                        }
                    }
                    crate::SampleLevel::Gradient { .. } => "textureGrad",
                };
                let offset_name = match offset {
                    Some(_) => "Offset",
                    None => "",
                };

                write!(self.out, "{}{}(", fun_name, offset_name)?;

                // Write the image that will be used
                self.write_expr(image, ctx)?;
                // The space here isn't required but it helps with readability
                write!(self.out, ", ")?;

                // We need to get the coordinates vector size to later build a vector that's `size + 1`
                // if `depth_ref` is some, if it isn't a vector we panic as that's not a valid expression
                let mut coord_dim = match *ctx.info[coordinate].ty.inner_with(&self.module.types) {
                    TypeInner::Vector { size, .. } => size as u8,
                    TypeInner::Scalar { .. } => 1,
                    _ => unreachable!(),
                };

                if array_index.is_some() {
                    coord_dim += 1;
                }
                let cube_array_shadow = coord_dim == 4;
                if depth_ref.is_some() && !cube_array_shadow {
                    coord_dim += 1;
                }

                let tex_1d_hack = dim == crate::ImageDimension::D1 && self.options.version.is_es();
                let is_vec = tex_1d_hack || coord_dim != 1;
                // Compose a new texture coordinates vector
                if is_vec {
                    write!(self.out, "vec{}(", coord_dim + tex_1d_hack as u8)?;
                }
                self.write_expr(coordinate, ctx)?;
                if tex_1d_hack {
                    write!(self.out, ", 0.0")?;
                }
                if let Some(expr) = array_index {
                    write!(self.out, ", ")?;
                    self.write_expr(expr, ctx)?;
                }
                if !cube_array_shadow {
                    if let Some(expr) = depth_ref {
                        write!(self.out, ", ")?;
                        self.write_expr(expr, ctx)?;
                    }
                }
                if is_vec {
                    write!(self.out, ")")?;
                }

                if cube_array_shadow {
                    if let Some(expr) = depth_ref {
                        write!(self.out, ", ")?;
                        self.write_expr(expr, ctx)?;
                    }
                }

                match level {
                    // Auto needs no more arguments
                    crate::SampleLevel::Auto => (),
                    // Zero needs level set to 0
                    crate::SampleLevel::Zero => {
                        if workaround_lod_array_shadow_as_grad {
                            write!(self.out, ", vec2(0,0), vec2(0,0)")?;
                        } else {
                            write!(self.out, ", 0.0")?;
                        }
                    }
                    // Exact and bias require another argument
                    crate::SampleLevel::Exact(expr) => {
                        if workaround_lod_array_shadow_as_grad {
                            log::warn!("Unable to `textureLod` a shadow array, ignoring the LOD");
                            write!(self.out, ", vec2(0,0), vec2(0,0)")?;
                        } else {
                            write!(self.out, ", ")?;
                            self.write_expr(expr, ctx)?;
                        }
                    }
                    crate::SampleLevel::Bias(expr) => {
                        write!(self.out, ", ")?;
                        self.write_expr(expr, ctx)?;
                    }
                    crate::SampleLevel::Gradient { x, y } => {
                        write!(self.out, ", ")?;
                        self.write_expr(x, ctx)?;
                        write!(self.out, ", ")?;
                        self.write_expr(y, ctx)?;
                    }
                }

                if let Some(constant) = offset {
                    write!(self.out, ", ")?;
                    if tex_1d_hack {
                        write!(self.out, "ivec2(")?;
                    }
                    self.write_constant(constant)?;
                    if tex_1d_hack {
                        write!(self.out, ", 0)")?;
                    }
                }

                // End the function
                write!(self.out, ")")?
            }
            // `ImageLoad` is also a bit complicated.
            // There are two functions one for sampled
            // images another for storage images, the former uses `texelFetch` and the latter uses
            // `imageLoad`.
            // Furthermore we have `index` which is always `Some` for sampled images
            // and `None` for storage images, so we end up with two functions:
            // `texelFetch(image, coordinate, index)` - for sampled images
            // `imageLoad(image, coordinate)` - for storage images
            Expression::ImageLoad {
                image,
                coordinate,
                array_index,
                index,
            } => {
                // This will only panic if the module is invalid
                let (dim, class) = match *ctx.info[image].ty.inner_with(&self.module.types) {
                    TypeInner::Image {
                        dim,
                        arrayed: _,
                        class,
                    } => (dim, class),
                    _ => unreachable!(),
                };

                let fun_name = match class {
                    crate::ImageClass::Sampled { .. } => "texelFetch",
                    crate::ImageClass::Storage { .. } => "imageLoad",
                    // TODO: Is there even a function for this?
                    crate::ImageClass::Depth { multi: _ } => {
                        return Err(Error::Custom("TODO: depth sample loads".to_string()))
                    }
                };

                write!(self.out, "{}(", fun_name)?;
                self.write_expr(image, ctx)?;
                write!(self.out, ", ")?;
                self.write_texture_coordinates(coordinate, array_index, dim, ctx)?;

                if let Some(index_expr) = index {
                    write!(self.out, ", ")?;
                    self.write_expr(index_expr, ctx)?;
                }
                write!(self.out, ")")?;
            }
            // Query translates into one of the:
            // - textureSize/imageSize
            // - textureQueryLevels
            // - textureSamples/imageSamples
            Expression::ImageQuery { image, query } => {
                use crate::ImageClass;

                // This will only panic if the module is invalid
                let (dim, class) = match *ctx.info[image].ty.inner_with(&self.module.types) {
                    TypeInner::Image {
                        dim,
                        arrayed: _,
                        class,
                    } => (dim, class),
                    _ => unreachable!(),
                };
                let components = match dim {
                    crate::ImageDimension::D1 => 1,
                    crate::ImageDimension::D2 => 2,
                    crate::ImageDimension::D3 => 3,
                    crate::ImageDimension::Cube => 2,
                };
                match query {
                    crate::ImageQuery::Size { level } => {
                        match class {
                            ImageClass::Sampled { .. } | ImageClass::Depth { .. } => {
                                write!(self.out, "textureSize(")?;
                                self.write_expr(image, ctx)?;
                                write!(self.out, ", ")?;
                                if let Some(expr) = level {
                                    self.write_expr(expr, ctx)?;
                                } else {
                                    write!(self.out, "0")?;
                                }
                            }
                            ImageClass::Storage { .. } => {
                                write!(self.out, "imageSize(")?;
                                self.write_expr(image, ctx)?;
                            }
                        }
                        write!(self.out, ")")?;
                        if components != 1 || self.options.version.is_es() {
                            write!(self.out, ".{}", &"xyz"[..components])?;
                        }
                    }
                    crate::ImageQuery::NumLevels => {
                        write!(self.out, "textureQueryLevels(",)?;
                        self.write_expr(image, ctx)?;
                        write!(self.out, ")",)?;
                    }
                    crate::ImageQuery::NumLayers => {
                        let fun_name = match class {
                            ImageClass::Sampled { .. } | ImageClass::Depth { .. } => "textureSize",
                            ImageClass::Storage { .. } => "imageSize",
                        };
                        write!(self.out, "{}(", fun_name)?;
                        self.write_expr(image, ctx)?;
                        if components != 1 || self.options.version.is_es() {
                            write!(self.out, ", 0).{}", back::COMPONENTS[components])?;
                        }
                    }
                    crate::ImageQuery::NumSamples => {
                        // assumes ARB_shader_texture_image_samples
                        let fun_name = match class {
                            ImageClass::Sampled { .. } | ImageClass::Depth { .. } => {
                                "textureSamples"
                            }
                            ImageClass::Storage { .. } => "imageSamples",
                        };
                        write!(self.out, "{}(", fun_name)?;
                        self.write_expr(image, ctx)?;
                        write!(self.out, ")",)?;
                    }
                }
            }
            // `Unary` is pretty straightforward
            // "-" - for `Negate`
            // "~" - for `Not` if it's an integer
            // "!" - for `Not` if it's a boolean
            //
            // We also wrap the everything in parentheses to avoid precedence issues
            Expression::Unary { op, expr } => {
                use crate::{ScalarKind as Sk, UnaryOperator as Uo};

                write!(
                    self.out,
                    "({} ",
                    match op {
                        Uo::Negate => "-",
                        Uo::Not => match *ctx.info[expr].ty.inner_with(&self.module.types) {
                            TypeInner::Scalar { kind: Sk::Sint, .. } => "~",
                            TypeInner::Scalar { kind: Sk::Uint, .. } => "~",
                            TypeInner::Scalar { kind: Sk::Bool, .. } => "!",
                            ref other =>
                                return Err(Error::Custom(format!(
                                    "Cannot apply not to type {:?}",
                                    other
                                ))),
                        },
                    }
                )?;

                self.write_expr(expr, ctx)?;

                write!(self.out, ")")?
            }
            // `Binary` we just write `left op right`, except when dealing with
            // comparison operations on vectors as they are implemented with
            // builtin functions.
            // Once again we wrap everything in parentheses to avoid precedence issues
            Expression::Binary { op, left, right } => {
                // Holds `Some(function_name)` if the binary operation is
                // implemented as a function call
                use crate::{BinaryOperator as Bo, ScalarKind as Sk, TypeInner as Ti};

                let left_inner = ctx.info[left].ty.inner_with(&self.module.types);
                let right_inner = ctx.info[right].ty.inner_with(&self.module.types);

                let function = match (left_inner, right_inner) {
                    (
                        &Ti::Vector {
                            kind: left_kind, ..
                        },
                        &Ti::Vector {
                            kind: right_kind, ..
                        },
                    ) => match op {
                        Bo::Less
                        | Bo::LessEqual
                        | Bo::Greater
                        | Bo::GreaterEqual
                        | Bo::Equal
                        | Bo::NotEqual => BinaryOperation::VectorCompare,
                        Bo::Modulo => match (left_kind, right_kind) {
                            (Sk::Float, _) | (_, Sk::Float) => match op {
                                Bo::Modulo => BinaryOperation::Modulo,
                                _ => BinaryOperation::Other,
                            },
                            _ => BinaryOperation::Other,
                        },
                        _ => BinaryOperation::Other,
                    },
                    _ => match (left_inner.scalar_kind(), right_inner.scalar_kind()) {
                        (Some(Sk::Float), _) | (_, Some(Sk::Float)) => match op {
                            Bo::Modulo => BinaryOperation::Modulo,
                            _ => BinaryOperation::Other,
                        },
                        _ => BinaryOperation::Other,
                    },
                };

                match function {
                    BinaryOperation::VectorCompare => {
                        let op_str = match op {
                            Bo::Less => "lessThan(",
                            Bo::LessEqual => "lessThanEqual(",
                            Bo::Greater => "greaterThan(",
                            Bo::GreaterEqual => "greaterThanEqual(",
                            Bo::Equal => "equal(",
                            Bo::NotEqual => "notEqual(",
                            _ => unreachable!(),
                        };
                        write!(self.out, "{}", op_str)?;
                        self.write_expr(left, ctx)?;
                        write!(self.out, ", ")?;
                        self.write_expr(right, ctx)?;
                        write!(self.out, ")")?;
                    }
                    BinaryOperation::Modulo => {
                        write!(self.out, "(")?;

                        // write `e1 - e2 * trunc(e1 / e2)`
                        self.write_expr(left, ctx)?;
                        write!(self.out, " - ")?;
                        self.write_expr(right, ctx)?;
                        write!(self.out, " * ")?;
                        write!(self.out, "trunc(")?;
                        self.write_expr(left, ctx)?;
                        write!(self.out, " / ")?;
                        self.write_expr(right, ctx)?;
                        write!(self.out, ")")?;

                        write!(self.out, ")")?;
                    }
                    BinaryOperation::Other => {
                        write!(self.out, "(")?;

                        self.write_expr(left, ctx)?;
                        write!(self.out, " {} ", super::binary_operation_str(op))?;
                        self.write_expr(right, ctx)?;

                        write!(self.out, ")")?;
                    }
                }
            }
            // `Select` is written as `condition ? accept : reject`
            // We wrap everything in parentheses to avoid precedence issues
            Expression::Select {
                condition,
                accept,
                reject,
            } => {
                let cond_ty = ctx.info[condition].ty.inner_with(&self.module.types);
                let vec_select = if let TypeInner::Vector { .. } = *cond_ty {
                    true
                } else {
                    false
                };

                // TODO: Boolean mix on desktop required GL_EXT_shader_integer_mix
                if vec_select {
                    // Glsl defines that for mix when the condition is a boolean the first element
                    // is picked if condition is false and the second if condition is true
                    write!(self.out, "mix(")?;
                    self.write_expr(reject, ctx)?;
                    write!(self.out, ", ")?;
                    self.write_expr(accept, ctx)?;
                    write!(self.out, ", ")?;
                    self.write_expr(condition, ctx)?;
                } else {
                    write!(self.out, "(")?;
                    self.write_expr(condition, ctx)?;
                    write!(self.out, " ? ")?;
                    self.write_expr(accept, ctx)?;
                    write!(self.out, " : ")?;
                    self.write_expr(reject, ctx)?;
                }

                write!(self.out, ")")?
            }
            // `Derivative` is a function call to a glsl provided function
            Expression::Derivative { axis, expr } => {
                use crate::DerivativeAxis as Da;

                write!(
                    self.out,
                    "{}(",
                    match axis {
                        Da::X => "dFdx",
                        Da::Y => "dFdy",
                        Da::Width => "fwidth",
                    }
                )?;
                self.write_expr(expr, ctx)?;
                write!(self.out, ")")?
            }
            // `Relational` is a normal function call to some glsl provided functions
            Expression::Relational { fun, argument } => {
                use crate::RelationalFunction as Rf;

                let fun_name = match fun {
                    // There's no specific function for this but we can invert the result of `isinf`
                    Rf::IsFinite => "!isinf",
                    Rf::IsInf => "isinf",
                    Rf::IsNan => "isnan",
                    // There's also no function for this but we can invert `isnan`
                    Rf::IsNormal => "!isnan",
                    Rf::All => "all",
                    Rf::Any => "any",
                };
                write!(self.out, "{}(", fun_name)?;

                self.write_expr(argument, ctx)?;

                write!(self.out, ")")?
            }
            Expression::Math {
                fun,
                arg,
                arg1,
                arg2,
                arg3,
            } => {
                use crate::MathFunction as Mf;

                let fun_name = match fun {
                    // comparison
                    Mf::Abs => "abs",
                    Mf::Min => "min",
                    Mf::Max => "max",
                    Mf::Clamp => "clamp",
                    // trigonometry
                    Mf::Cos => "cos",
                    Mf::Cosh => "cosh",
                    Mf::Sin => "sin",
                    Mf::Sinh => "sinh",
                    Mf::Tan => "tan",
                    Mf::Tanh => "tanh",
                    Mf::Acos => "acos",
                    Mf::Asin => "asin",
                    Mf::Atan => "atan",
                    Mf::Asinh => "asinh",
                    Mf::Acosh => "acosh",
                    Mf::Atanh => "atanh",
                    // glsl doesn't have atan2 function
                    // use two-argument variation of the atan function
                    Mf::Atan2 => "atan",
                    // decomposition
                    Mf::Ceil => "ceil",
                    Mf::Floor => "floor",
                    Mf::Round => "roundEven",
                    Mf::Fract => "fract",
                    Mf::Trunc => "trunc",
                    Mf::Modf => "modf",
                    Mf::Frexp => "frexp",
                    Mf::Ldexp => "ldexp",
                    // exponent
                    Mf::Exp => "exp",
                    Mf::Exp2 => "exp2",
                    Mf::Log => "log",
                    Mf::Log2 => "log2",
                    Mf::Pow => "pow",
                    // geometry
                    Mf::Dot => "dot",
                    Mf::Outer => "outerProduct",
                    Mf::Cross => "cross",
                    Mf::Distance => "distance",
                    Mf::Length => "length",
                    Mf::Normalize => "normalize",
                    Mf::FaceForward => "faceforward",
                    Mf::Reflect => "reflect",
                    Mf::Refract => "refract",
                    // computational
                    Mf::Sign => "sign",
                    Mf::Fma => "fma",
                    Mf::Mix => "mix",
                    Mf::Step => "step",
                    Mf::SmoothStep => "smoothstep",
                    Mf::Sqrt => "sqrt",
                    Mf::InverseSqrt => "inversesqrt",
                    Mf::Inverse => "inverse",
                    Mf::Transpose => "transpose",
                    Mf::Determinant => "determinant",
                    // bits
                    Mf::CountOneBits => "bitCount",
                    Mf::ReverseBits => "bitfieldReverse",
                    Mf::ExtractBits => "bitfieldExtract",
                    Mf::InsertBits => "bitfieldInsert",
                    // data packing
                    Mf::Pack4x8snorm => "packSnorm4x8",
                    Mf::Pack4x8unorm => "packUnorm4x8",
                    Mf::Pack2x16snorm => "packSnorm2x16",
                    Mf::Pack2x16unorm => "packUnorm2x16",
                    Mf::Pack2x16float => "packHalf2x16",
                    // data unpacking
                    Mf::Unpack4x8snorm => "unpackSnorm4x8",
                    Mf::Unpack4x8unorm => "unpackUnorm4x8",
                    Mf::Unpack2x16snorm => "unpackSnorm2x16",
                    Mf::Unpack2x16unorm => "unpackUnorm2x16",
                    Mf::Unpack2x16float => "unpackHalf2x16",
                };

                let extract_bits = fun == Mf::ExtractBits;
                let insert_bits = fun == Mf::InsertBits;

                write!(self.out, "{}(", fun_name)?;
                self.write_expr(arg, ctx)?;
                if let Some(arg) = arg1 {
                    write!(self.out, ", ")?;
                    if extract_bits {
                        write!(self.out, "int(")?;
                        self.write_expr(arg, ctx)?;
                        write!(self.out, ")")?;
                    } else {
                        self.write_expr(arg, ctx)?;
                    }
                }
                if let Some(arg) = arg2 {
                    write!(self.out, ", ")?;
                    if extract_bits || insert_bits {
                        write!(self.out, "int(")?;
                        self.write_expr(arg, ctx)?;
                        write!(self.out, ")")?;
                    } else {
                        self.write_expr(arg, ctx)?;
                    }
                }
                if let Some(arg) = arg3 {
                    write!(self.out, ", ")?;
                    if insert_bits {
                        write!(self.out, "int(")?;
                        self.write_expr(arg, ctx)?;
                        write!(self.out, ")")?;
                    } else {
                        self.write_expr(arg, ctx)?;
                    }
                }
                write!(self.out, ")")?
            }
            // `As` is always a call.
            // If `convert` is true the function name is the type
            // Else the function name is one of the glsl provided bitcast functions
            Expression::As {
                expr,
                kind: target_kind,
                convert,
            } => {
                let inner = ctx.info[expr].ty.inner_with(&self.module.types);
                match convert {
                    Some(width) => {
                        // this is similar to `write_type`, but with the target kind
                        let scalar = glsl_scalar(target_kind, width)?;
                        match *inner {
                            TypeInner::Vector { size, .. } => {
                                write!(self.out, "{}vec{}", scalar.prefix, size as u8)?
                            }
                            _ => write!(self.out, "{}", scalar.full)?,
                        }

                        write!(self.out, "(")?;
                        self.write_expr(expr, ctx)?;
                        write!(self.out, ")")?
                    }
                    None => {
                        use crate::ScalarKind as Sk;

                        let source_kind = inner.scalar_kind().unwrap();
                        let conv_op = match (source_kind, target_kind) {
                            (Sk::Float, Sk::Sint) => "floatBitsToInt",
                            (Sk::Float, Sk::Uint) => "floatBitsToUInt",
                            (Sk::Sint, Sk::Float) => "intBitsToFloat",
                            (Sk::Uint, Sk::Float) => "uintBitsToFloat",
                            // There is no way to bitcast between Uint/Sint in glsl. Use constructor conversion
                            (Sk::Uint, Sk::Sint) => "int",
                            (Sk::Sint, Sk::Uint) => "uint",

                            (Sk::Bool, Sk::Sint) => "int",
                            (Sk::Bool, Sk::Uint) => "uint",
                            (Sk::Bool, Sk::Float) => "float",

                            (Sk::Sint, Sk::Bool) => "bool",
                            (Sk::Uint, Sk::Bool) => "bool",
                            (Sk::Float, Sk::Bool) => "bool",

                            // No conversion needed
                            (Sk::Sint, Sk::Sint) => "",
                            (Sk::Uint, Sk::Uint) => "",
                            (Sk::Float, Sk::Float) => "",
                            (Sk::Bool, Sk::Bool) => "",
                        };
                        write!(self.out, "{}", conv_op)?;
                        if !conv_op.is_empty() {
                            write!(self.out, "(")?;
                        }
                        self.write_expr(expr, ctx)?;
                        if !conv_op.is_empty() {
                            write!(self.out, ")")?
                        }
                    }
                }
            }
            // These expressions never show up in `Emit`.
            Expression::CallResult(_) | Expression::AtomicResult { .. } => unreachable!(),
            // `ArrayLength` is written as `expr.length()` and we convert it to a uint
            Expression::ArrayLength(expr) => {
                write!(self.out, "uint(")?;
                self.write_expr(expr, ctx)?;
                write!(self.out, ".length())")?
            }
        }

        Ok(())
    }

    fn write_texture_coordinates(
        &mut self,
        coordinate: Handle<crate::Expression>,
        array_index: Option<Handle<crate::Expression>>,
        dim: crate::ImageDimension,
        ctx: &back::FunctionCtx,
    ) -> Result<(), Error> {
        use crate::ImageDimension as IDim;

        match array_index {
            Some(layer_expr) => {
                let tex_coord_type = match dim {
                    IDim::D1 => "ivec2",
                    IDim::D2 => "ivec3",
                    IDim::D3 => "ivec4",
                    IDim::Cube => "ivec4",
                };
                write!(self.out, "{}(", tex_coord_type)?;
                self.write_expr(coordinate, ctx)?;
                write!(self.out, ", ")?;
                self.write_expr(layer_expr, ctx)?;
                write!(self.out, ")")?;
            }
            None => {
                let tex_1d_hack = dim == IDim::D1 && self.options.version.is_es();
                if tex_1d_hack {
                    write!(self.out, "ivec2(")?;
                }
                self.write_expr(coordinate, ctx)?;
                if tex_1d_hack {
                    write!(self.out, ", 0.0)")?;
                }
            }
        }
        Ok(())
    }

    fn write_named_expr(
        &mut self,
        handle: Handle<crate::Expression>,
        name: String,
        ctx: &back::FunctionCtx,
    ) -> BackendResult {
        match ctx.info[handle].ty {
            proc::TypeResolution::Handle(ty_handle) => match self.module.types[ty_handle].inner {
                TypeInner::Struct { .. } => {
                    let ty_name = &self.names[&NameKey::Type(ty_handle)];
                    write!(self.out, "{}", ty_name)?;
                }
                _ => {
                    self.write_type(ty_handle)?;
                }
            },
            proc::TypeResolution::Value(ref inner) => {
                self.write_value_type(inner)?;
            }
        }

        let base_ty_res = &ctx.info[handle].ty;
        let resolved = base_ty_res.inner_with(&self.module.types);

        write!(self.out, " {}", name)?;
        if let TypeInner::Array { size, .. } = *resolved {
            self.write_array_size(size)?;
        }
        write!(self.out, " = ")?;
        self.write_expr(handle, ctx)?;
        writeln!(self.out, ";")?;
        self.named_expressions.insert(handle, name);

        Ok(())
    }

    /// Helper function that write string with default zero initialization for supported types
    fn write_zero_init_value(&mut self, ty: Handle<crate::Type>) -> BackendResult {
        let inner = &self.module.types[ty].inner;
        match *inner {
            TypeInner::Scalar { kind, .. } => {
                self.write_zero_init_scalar(kind)?;
            }
            TypeInner::Vector { size, kind, .. } => {
                self.write_value_type(inner)?;
                write!(self.out, "(")?;
                for _ in 1..(size as usize) {
                    self.write_zero_init_scalar(kind)?;
                    write!(self.out, ", ")?;
                }
                // write last parameter without comma and space
                self.write_zero_init_scalar(kind)?;
                write!(self.out, ")")?;
            }
            TypeInner::Matrix { columns, rows, .. } => {
                let number_of_components = (columns as usize) * (rows as usize);
                self.write_value_type(inner)?;
                write!(self.out, "(")?;
                for _ in 1..number_of_components {
                    // IR supports only float matrix
                    self.write_zero_init_scalar(crate::ScalarKind::Float)?;
                    write!(self.out, ", ")?;
                }
                // write last parameter without comma and space
                self.write_zero_init_scalar(crate::ScalarKind::Float)?;
                write!(self.out, ")")?;
            }
            _ => {} // TODO:
        }

        Ok(())
    }

    /// Helper function that write string with zero initialization for scalar
    fn write_zero_init_scalar(&mut self, kind: crate::ScalarKind) -> BackendResult {
        match kind {
            crate::ScalarKind::Bool => write!(self.out, "false")?,
            crate::ScalarKind::Uint => write!(self.out, "0u")?,
            crate::ScalarKind::Float => write!(self.out, "0.0")?,
            crate::ScalarKind::Sint => write!(self.out, "0")?,
        }

        Ok(())
    }

    /// Helper function that return the glsl storage access string of [`StorageAccess`](crate::StorageAccess)
    ///
    /// glsl allows adding both `readonly` and `writeonly` but this means that
    /// they can only be used to query information about the resource which isn't what
    /// we want here so when storage access is both `LOAD` and `STORE` add no modifiers
    fn write_storage_access(&mut self, storage_access: crate::StorageAccess) -> BackendResult {
        if !storage_access.contains(crate::StorageAccess::STORE) {
            write!(self.out, "readonly ")?;
        }
        if !storage_access.contains(crate::StorageAccess::LOAD) {
            write!(self.out, "writeonly ")?;
        }
        Ok(())
    }

    /// Helper method used to produce the reflection info that's returned to the user
    ///
    /// It takes an iterator of [`Function`](crate::Function) references instead of
    /// [`Handle`](crate::arena::Handle) because [`EntryPoint`](crate::EntryPoint) isn't in any
    /// [`Arena`](crate::arena::Arena) and we need to traverse it
    fn collect_reflection_info(&self) -> Result<ReflectionInfo, Error> {
        use std::collections::hash_map::Entry;
        let info = self.info.get_entry_point(self.entry_point_idx as usize);
        let mut mappings = crate::FastHashMap::default();
        let mut uniforms = crate::FastHashMap::default();

        for sampling in info.sampling_set.iter() {
            let tex_name = self.reflection_names_globals[&sampling.image].clone();

            match mappings.entry(tex_name) {
                Entry::Vacant(v) => {
                    v.insert(TextureMapping {
                        texture: sampling.image,
                        sampler: Some(sampling.sampler),
                    });
                }
                Entry::Occupied(e) => {
                    if e.get().sampler != Some(sampling.sampler) {
                        log::error!("Conflicting samplers for {}", e.key());
                        return Err(Error::ImageMultipleSamplers);
                    }
                }
            }
        }

        for (handle, var) in self.module.global_variables.iter() {
            if info[handle].is_empty() {
                continue;
            }
            match self.module.types[var.ty].inner {
                crate::TypeInner::Struct { .. } => match var.class {
                    crate::StorageClass::Uniform | crate::StorageClass::Storage { .. } => {
                        let name = self.reflection_names_uniforms[&var.ty].clone();
                        uniforms.insert(handle, name);
                    }
                    _ => (),
                },
                _ => continue,
            }
        }

        Ok(ReflectionInfo {
            texture_mapping: mappings,
            uniforms,
        })
    }
}

/// Structure returned by [`glsl_scalar`](glsl_scalar)
///
/// It contains both a prefix used in other types and the full type name
struct ScalarString<'a> {
    /// The prefix used to compose other types
    prefix: &'a str,
    /// The name of the scalar type
    full: &'a str,
}

/// Helper function that returns scalar related strings
///
/// Check [`ScalarString`](ScalarString) for the information provided
///
/// # Errors
/// If a [`Float`](crate::ScalarKind::Float) with an width that isn't 4 or 8
fn glsl_scalar(
    kind: crate::ScalarKind,
    width: crate::Bytes,
) -> Result<ScalarString<'static>, Error> {
    use crate::ScalarKind as Sk;

    Ok(match kind {
        Sk::Sint => ScalarString {
            prefix: "i",
            full: "int",
        },
        Sk::Uint => ScalarString {
            prefix: "u",
            full: "uint",
        },
        Sk::Float => match width {
            4 => ScalarString {
                prefix: "",
                full: "float",
            },
            8 => ScalarString {
                prefix: "d",
                full: "double",
            },
            _ => return Err(Error::UnsupportedScalar(kind, width)),
        },
        Sk::Bool => ScalarString {
            prefix: "b",
            full: "bool",
        },
    })
}

/// Helper function that returns the glsl variable name for a builtin
fn glsl_built_in(built_in: crate::BuiltIn, output: bool) -> &'static str {
    use crate::BuiltIn as Bi;

    match built_in {
        Bi::Position => {
            if output {
                "gl_Position"
            } else {
                "gl_FragCoord"
            }
        }
        Bi::ViewIndex => "gl_ViewIndex",
        // vertex
        Bi::BaseInstance => "uint(gl_BaseInstance)",
        Bi::BaseVertex => "uint(gl_BaseVertex)",
        Bi::ClipDistance => "gl_ClipDistance",
        Bi::CullDistance => "gl_CullDistance",
        Bi::InstanceIndex => "uint(gl_InstanceID)",
        Bi::PointSize => "gl_PointSize",
        Bi::VertexIndex => "uint(gl_VertexID)",
        // fragment
        Bi::FragDepth => "gl_FragDepth",
        Bi::FrontFacing => "gl_FrontFacing",
        Bi::PrimitiveIndex => "uint(gl_PrimitiveID)",
        Bi::SampleIndex => "gl_SampleID",
        Bi::SampleMask => {
            if output {
                "gl_SampleMask"
            } else {
                "gl_SampleMaskIn"
            }
        }
        // compute
        Bi::GlobalInvocationId => "gl_GlobalInvocationID",
        Bi::LocalInvocationId => "gl_LocalInvocationID",
        Bi::LocalInvocationIndex => "gl_LocalInvocationIndex",
        Bi::WorkGroupId => "gl_WorkGroupID",
        Bi::WorkGroupSize => "gl_WorkGroupSize",
        Bi::NumWorkGroups => "gl_NumWorkGroups",
    }
}

/// Helper function that returns the string corresponding to the storage class
fn glsl_storage_class(class: crate::StorageClass) -> Option<&'static str> {
    use crate::StorageClass as Sc;

    match class {
        Sc::Function => None,
        Sc::Private => None,
        Sc::Storage { .. } => Some("buffer"),
        Sc::Uniform => Some("uniform"),
        Sc::Handle => Some("uniform"),
        Sc::WorkGroup => Some("shared"),
        Sc::PushConstant => None,
    }
}

/// Helper function that returns the string corresponding to the glsl interpolation qualifier
fn glsl_interpolation(interpolation: crate::Interpolation) -> &'static str {
    use crate::Interpolation as I;

    match interpolation {
        I::Perspective => "smooth",
        I::Linear => "noperspective",
        I::Flat => "flat",
    }
}

/// Return the GLSL auxiliary qualifier for the given sampling value.
fn glsl_sampling(sampling: crate::Sampling) -> Option<&'static str> {
    use crate::Sampling as S;

    match sampling {
        S::Center => None,
        S::Centroid => Some("centroid"),
        S::Sample => Some("sample"),
    }
}

/// Helper function that returns the glsl dimension string of [`ImageDimension`](crate::ImageDimension)
fn glsl_dimension(dim: crate::ImageDimension) -> &'static str {
    use crate::ImageDimension as IDim;

    match dim {
        IDim::D1 => "1D",
        IDim::D2 => "2D",
        IDim::D3 => "3D",
        IDim::Cube => "Cube",
    }
}

/// Helper function that returns the glsl storage format string of [`StorageFormat`](crate::StorageFormat)
fn glsl_storage_format(format: crate::StorageFormat) -> &'static str {
    use crate::StorageFormat as Sf;

    match format {
        Sf::R8Unorm => "r8",
        Sf::R8Snorm => "r8_snorm",
        Sf::R8Uint => "r8ui",
        Sf::R8Sint => "r8i",
        Sf::R16Uint => "r16ui",
        Sf::R16Sint => "r16i",
        Sf::R16Float => "r16f",
        Sf::Rg8Unorm => "rg8",
        Sf::Rg8Snorm => "rg8_snorm",
        Sf::Rg8Uint => "rg8ui",
        Sf::Rg8Sint => "rg8i",
        Sf::R32Uint => "r32ui",
        Sf::R32Sint => "r32i",
        Sf::R32Float => "r32f",
        Sf::Rg16Uint => "rg16ui",
        Sf::Rg16Sint => "rg16i",
        Sf::Rg16Float => "rg16f",
        Sf::Rgba8Unorm => "rgba8ui",
        Sf::Rgba8Snorm => "rgba8_snorm",
        Sf::Rgba8Uint => "rgba8ui",
        Sf::Rgba8Sint => "rgba8i",
        Sf::Rgb10a2Unorm => "rgb10_a2ui",
        Sf::Rg11b10Float => "r11f_g11f_b10f",
        Sf::Rg32Uint => "rg32ui",
        Sf::Rg32Sint => "rg32i",
        Sf::Rg32Float => "rg32f",
        Sf::Rgba16Uint => "rgba16ui",
        Sf::Rgba16Sint => "rgba16i",
        Sf::Rgba16Float => "rgba16f",
        Sf::Rgba32Uint => "rgba32ui",
        Sf::Rgba32Sint => "rgba32i",
        Sf::Rgba32Float => "rgba32f",
    }
}

fn is_value_init_supported(module: &crate::Module, ty: Handle<crate::Type>) -> bool {
    match module.types[ty].inner {
        TypeInner::Scalar { .. } | TypeInner::Vector { .. } | TypeInner::Matrix { .. } => true,
        _ => false,
    }
}
