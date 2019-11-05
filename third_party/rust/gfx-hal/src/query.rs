//! Queries are commands that can be submitted to a command buffer to record statistics or
//! other useful values as the command buffer is running. They are often intended for profiling
//! or other introspection, providing a mechanism for the command buffer to record data about its
//! operation as it is running.

use crate::device::OutOfMemory;
use crate::Backend;

/// A query identifier.
pub type Id = u32;

/// Query creation error.
#[derive(Clone, Debug, PartialEq)]
pub enum CreationError {
    /// Out of either host or device memory.
    OutOfMemory(OutOfMemory),

    /// Query type unsupported.
    Unsupported(Type),
}

impl From<OutOfMemory> for CreationError {
    fn from(error: OutOfMemory) -> Self {
        CreationError::OutOfMemory(error)
    }
}

/// A `Query` object has a particular identifier and saves its results to a given `QueryPool`.
/// It is passed as a parameter to the command buffer's query methods.
#[derive(Debug)]
pub struct Query<'a, B: Backend> {
    ///
    pub pool: &'a B::QueryPool,
    ///
    pub id: Id,
}

bitflags!(
    /// Query control flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct ControlFlags: u32 {
        /// Occlusion queries **must** return the exact sampler number.
        ///
        /// Requires `precise_occlusion_query` device feature.
        const PRECISE = 0x1;
    }
);

bitflags!(
    /// Query result flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct ResultFlags: u32 {
        /// Results will be written as an array of 64-bit unsigned integer values.
        const BITS_64 = 0x1;
        /// Wait for each query’s status to become available before retrieving its results.
        const WAIT = 0x2;
        /// Availability status accompanies the results.
        const WITH_AVAILABILITY = 0x4;
        /// Returning partial results is acceptable.
        const PARTIAL = 0x8;
    }
);

/// Type of queries in a query pool.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum Type {
    /// Occlusion query. Count the number of drawn samples between
    /// the start and end of the query command.
    Occlusion,
    /// Pipeline statistic query. Counts the number of pipeline stage
    /// invocations of the given types between the start and end of
    /// the query command.
    PipelineStatistics(PipelineStatistic),
    /// Timestamp query. Timestamps can be recorded to the
    /// query pool by calling `write_timestamp()`.
    Timestamp,
}

bitflags!(
    /// Pipeline statistic flags
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct PipelineStatistic: u32 {
        ///
        const INPUT_ASSEMBLY_VERTICES = 0x1;
        ///
        const INPUT_ASSEMBLY_PRIMITIVES = 0x2;
        ///
        const VERTEX_SHADER_INVOCATIONS = 0x4;
        ///
        const GEOMETRY_SHADER_INVOCATIONS = 0x8;
        ///
        const GEOMETRY_SHADER_PRIMITIVES = 0x10;
        ///
        const CLIPPING_INVOCATIONS = 0x20;
        ///
        const CLIPPING_PRIMITIVES = 0x40;
        ///
        const FRAGMENT_SHADER_INVOCATIONS = 0x80;
        ///
        const HULL_SHADER_PATCHES = 0x100;
        ///
        const DOMAIN_SHADER_INVOCATIONS = 0x200;
        ///
        const COMPUTE_SHADER_INVOCATIONS = 0x400;
    }
);
