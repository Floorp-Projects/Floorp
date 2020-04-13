//! Raw Pipeline State Objects
//!
//! This module contains items used to create and manage Pipelines.

use crate::{device, pass, Backend};
use std::{fmt, io, slice};

mod compute;
mod descriptor;
mod graphics;
mod input_assembler;
mod output_merger;
mod specialization;

pub use self::{
    compute::*,
    descriptor::*,
    graphics::*,
    input_assembler::*,
    output_merger::*,
    specialization::*,
};

/// Error types happening upon PSO creation on the device side.
#[derive(Clone, Debug, PartialEq)]
pub enum CreationError {
    /// Unknown other error.
    Other,
    /// Invalid subpass (not part of renderpass).
    InvalidSubpass(pass::SubpassId),
    /// Shader compilation error.
    Shader(device::ShaderError),
    /// Out of either host or device memory.
    OutOfMemory(device::OutOfMemory),
}

impl From<device::OutOfMemory> for CreationError {
    fn from(err: device::OutOfMemory) -> Self {
        CreationError::OutOfMemory(err)
    }
}

impl std::fmt::Display for CreationError {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CreationError::OutOfMemory(err) => write!(fmt, "Failed to create pipeline: {}", err),
            CreationError::Other => write!(fmt, "Failed to create pipeline: Unsupported usage: Implementation specific error occurred"),
            CreationError::InvalidSubpass(subpass) => write!(fmt, "Failed to create pipeline: Invalid subpass: {}", subpass),
            CreationError::Shader(err) => write!(fmt, "Failed to create pipeline: {}", err),
        }
    }
}

impl std::error::Error for CreationError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            CreationError::OutOfMemory(err) => Some(err),
            CreationError::Shader(err) => Some(err),
            CreationError::InvalidSubpass(_) => None,
            CreationError::Other => None,
        }
    }
}

bitflags!(
    /// Stages of the logical pipeline.
    ///
    /// The pipeline is structured by the ordering of the flags.
    /// Some stages are queue type dependent.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct PipelineStage: u32 {
        /// Beginning of the command queue.
        const TOP_OF_PIPE = 0x1;
        /// Indirect data consumption.
        const DRAW_INDIRECT = 0x2;
        /// Vertex data consumption.
        const VERTEX_INPUT = 0x4;
        /// Vertex shader execution.
        const VERTEX_SHADER = 0x8;
        /// Hull shader execution.
        const HULL_SHADER = 0x10;
        /// Domain shader execution.
        const DOMAIN_SHADER = 0x20;
        /// Geometry shader execution.
        const GEOMETRY_SHADER = 0x40;
        /// Fragment shader execution.
        const FRAGMENT_SHADER = 0x80;
        /// Stage of early depth and stencil test.
        const EARLY_FRAGMENT_TESTS = 0x100;
        /// Stage of late depth and stencil test.
        const LATE_FRAGMENT_TESTS = 0x200;
        /// Stage of final color value calculation.
        const COLOR_ATTACHMENT_OUTPUT = 0x400;
        /// Compute shader execution,
        const COMPUTE_SHADER = 0x800;
        /// Copy/Transfer command execution.
        const TRANSFER = 0x1000;
        /// End of the command queue.
        const BOTTOM_OF_PIPE = 0x2000;
        /// Read/Write access from host.
        /// (Not a real pipeline stage)
        const HOST = 0x4000;
    }
);

bitflags!(
    /// Combination of different shader pipeline stages.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct ShaderStageFlags: u32 {
        /// Vertex shader stage.
        const VERTEX   = 0x1;
        /// Hull (tessellation) shader stage.
        const HULL     = 0x2;
        /// Domain (tessellation) shader stage.
        const DOMAIN   = 0x4;
        /// Geometry shader stage.
        const GEOMETRY = 0x8;
        /// Fragment shader stage.
        const FRAGMENT = 0x10;
        /// Compute shader stage.
        const COMPUTE  = 0x20;
        /// All graphics pipeline shader stages.
        const GRAPHICS = Self::VERTEX.bits | Self::HULL.bits |
            Self::DOMAIN.bits | Self::GEOMETRY.bits | Self::FRAGMENT.bits;
        /// All shader stages (matches Vulkan).
        const ALL      = 0x7FFFFFFF;
    }
);

// Note: this type is only needed for backends, not used anywhere within gfx_hal.
/// Which program stage this shader represents.
#[allow(missing_docs)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum Stage {
    Vertex,
    Hull,
    Domain,
    Geometry,
    Fragment,
    Compute,
}

impl From<Stage> for ShaderStageFlags {
    fn from(stage: Stage) -> Self {
        match stage {
            Stage::Vertex => ShaderStageFlags::VERTEX,
            Stage::Hull => ShaderStageFlags::HULL,
            Stage::Domain => ShaderStageFlags::DOMAIN,
            Stage::Geometry => ShaderStageFlags::GEOMETRY,
            Stage::Fragment => ShaderStageFlags::FRAGMENT,
            Stage::Compute => ShaderStageFlags::COMPUTE,
        }
    }
}

impl fmt::Display for Stage {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match self {
            Stage::Vertex => "vertex",
            Stage::Hull => "hull",
            Stage::Domain => "domain",
            Stage::Geometry => "geometry",
            Stage::Fragment => "fragment",
            Stage::Compute => "compute",
        })
    }
}

/// Shader entry point.
#[derive(Debug)]
pub struct EntryPoint<'a, B: Backend> {
    /// Entry point name.
    pub entry: &'a str,
    /// Shader module reference.
    pub module: &'a B::ShaderModule,
    /// Specialization.
    pub specialization: Specialization<'a>,
}

impl<'a, B: Backend> Clone for EntryPoint<'a, B> {
    fn clone(&self) -> Self {
        EntryPoint {
            entry: self.entry,
            module: self.module,
            specialization: self.specialization.clone(),
        }
    }
}

bitflags!(
    /// Pipeline creation flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct PipelineCreationFlags: u32 {
        /// Disable pipeline optimizations.
        ///
        /// May speedup pipeline creation.
        const DISABLE_OPTIMIZATION = 0x1;
        /// Allow derivatives (children) of the pipeline.
        ///
        /// Must be set when pipelines set the pipeline as base.
        const ALLOW_DERIVATIVES = 0x2;
    }
);

/// A reference to a parent pipeline.  The assumption is that
/// a parent and derivative/child pipeline have most settings
/// in common, and one may be switched for another more quickly
/// than entirely unrelated pipelines would be.
#[derive(Debug)]
pub enum BasePipeline<'a, P: 'a> {
    /// Referencing an existing pipeline as parent.
    Pipeline(&'a P),
    /// A pipeline in the same create pipelines call.
    ///
    /// The index of the parent must be lower than the index of the child.
    Index(usize),
    /// No parent pipeline exists.
    None,
}

/// Pipeline state which may be static or dynamic.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum State<T> {
    /// Static state that cannot be altered.
    Static(T),
    /// Dynamic state set through a command buffer.
    Dynamic,
}

impl<T> State<T> {
    /// Returns the static value or a default.
    pub fn static_or(self, default: T) -> T {
        match self {
            State::Static(v) => v,
            State::Dynamic => default,
        }
    }

    /// Whether the state is static.
    pub fn is_static(self) -> bool {
        match self {
            State::Static(_) => true,
            State::Dynamic => false,
        }
    }

    /// Whether the state is dynamic.
    pub fn is_dynamic(self) -> bool {
        !self.is_static()
    }
}

/// Safely read SPIR-V
///
/// Converts to native endianness and returns correctly aligned storage without unnecessary
/// copying. Returns an `InvalidData` error if the input is trivially not SPIR-V.
///
/// This function can also be used to convert an already in-memory `&[u8]` to a valid `Vec<u32>`,
/// but prefer working with `&[u32]` from the start whenever possible.
///
/// # Examples
/// ```no_run
/// let mut file = std::fs::File::open("/path/to/shader.spv").unwrap();
/// let words = gfx_hal::pso::read_spirv(&mut file).unwrap();
/// ```
/// ```
/// const SPIRV: &[u8] = &[
///     0x03, 0x02, 0x23, 0x07, // ...
/// ];
/// let words = gfx_hal::pso::read_spirv(std::io::Cursor::new(&SPIRV[..])).unwrap();
/// ```
pub fn read_spirv<R: io::Read + io::Seek>(mut x: R) -> io::Result<Vec<u32>> {
    let size = x.seek(io::SeekFrom::End(0))?;
    if size % 4 != 0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "input length not divisible by 4",
        ));
    }
    if size > usize::max_value() as u64 {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "input too long"));
    }
    let words = (size / 4) as usize;
    let mut result = Vec::<u32>::with_capacity(words);
    x.seek(io::SeekFrom::Start(0))?;
    unsafe {
        // Writing all bytes through a pointer with less strict alignment when our type has no
        // invalid bitpatterns is safe.
        x.read_exact(slice::from_raw_parts_mut(
            result.as_mut_ptr() as *mut u8,
            words * 4,
        ))?;
        result.set_len(words);
    }
    const MAGIC_NUMBER: u32 = 0x07230203;
    if result.len() > 0 && result[0] == MAGIC_NUMBER.swap_bytes() {
        for word in &mut result {
            *word = word.swap_bytes();
        }
    }
    if result.len() == 0 || result[0] != MAGIC_NUMBER {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "input missing SPIR-V magic number",
        ));
    }
    Ok(result)
}
