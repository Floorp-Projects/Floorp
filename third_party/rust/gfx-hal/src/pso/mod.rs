//! Raw Pipeline State Objects
//!
//! This module contains items used to create and manage Pipelines.

use crate::{device, pass, Backend};

mod compute;
mod descriptor;
mod graphics;
mod input_assembler;
mod output_merger;
mod specialization;

pub use self::{
    compute::*, descriptor::*, graphics::*, input_assembler::*, output_merger::*, specialization::*,
};

/// Error types happening upon PSO creation on the device side.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum CreationError {
    /// Unknown other error.
    #[error("Implementation specific error occurred")]
    Other,
    /// Shader module creation error.
    #[error("{0:?} shader creation failed: {1:}")]
    ShaderCreationError(ShaderStageFlags, String),
    /// Unsupported pipeline on hardware or implementation. Example: mesh shaders on DirectX 11.
    #[error("Pipeline kind is not supported")]
    UnsupportedPipeline,
    /// Invalid subpass (not part of renderpass).
    #[error("Invalid subpass: {0:?}")]
    InvalidSubpass(pass::SubpassId),
    /// The shader is missing an entry point.
    #[error("Invalid entry point: {0:}")]
    MissingEntryPoint(String),
    /// The specialization values are incorrect.
    #[error("Specialization failed: {0:}")]
    InvalidSpecialization(String),
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] device::OutOfMemory),
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
        /// Task shader stage.
        const TASK_SHADER = 0x80000;
        /// Mesh shader stage.
        const MESH_SHADER = 0x100000;
    }
);

bitflags!(
    /// Combination of different shader pipeline stages.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    #[derive(Default)]
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
        /// Task shader stage.
        const TASK     = 0x40;
        /// Mesh shader stage.
        const MESH     = 0x80;
        /// All graphics pipeline shader stages.
        const GRAPHICS = Self::VERTEX.bits | Self::HULL.bits |
            Self::DOMAIN.bits | Self::GEOMETRY.bits | Self::FRAGMENT.bits;
        /// All shader stages (matches Vulkan).
        const ALL      = 0x7FFFFFFF;
    }
);

impl From<naga::ShaderStage> for ShaderStageFlags {
    fn from(stage: naga::ShaderStage) -> Self {
        use naga::ShaderStage as Ss;
        match stage {
            Ss::Vertex => Self::VERTEX,
            Ss::Fragment => Self::FRAGMENT,
            Ss::Compute => Self::COMPUTE,
        }
    }
}

/// Shader entry point.
#[derive(Debug)]
pub struct EntryPoint<'a, B: Backend> {
    /// Entry point name.
    pub entry: &'a str,
    /// Reference to the shader module containing this entry point.
    pub module: &'a B::ShaderModule,
    /// Specialization constants to be used when creating the pipeline.
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
