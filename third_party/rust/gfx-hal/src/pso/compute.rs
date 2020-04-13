//! Compute pipeline descriptor.

use crate::{
    pso::{BasePipeline, EntryPoint, PipelineCreationFlags},
    Backend,
};

/// A description of the data needed to construct a compute pipeline.
#[derive(Debug)]
pub struct ComputePipelineDesc<'a, B: Backend> {
    /// The shader entry point that performs the computation.
    pub shader: EntryPoint<'a, B>,
    /// Pipeline layout.
    pub layout: &'a B::PipelineLayout,
    /// Any flags necessary for the pipeline creation.
    pub flags: PipelineCreationFlags,
    /// The parent pipeline to this one, if any.
    pub parent: BasePipeline<'a, B::ComputePipeline>,
}

impl<'a, B: Backend> ComputePipelineDesc<'a, B> {
    /// Create a new empty PSO descriptor.
    pub fn new(shader: EntryPoint<'a, B>, layout: &'a B::PipelineLayout) -> Self {
        ComputePipelineDesc {
            shader,
            layout,
            flags: PipelineCreationFlags::empty(),
            parent: BasePipeline::None,
        }
    }
}
