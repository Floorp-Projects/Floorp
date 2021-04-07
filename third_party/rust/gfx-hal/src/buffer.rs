//! Memory buffers.
//!
//! Buffers interpret memory slices as linear contiguous data array.
//!
//! They can be used as shader resources, vertex buffers, index buffers or for
//! specifying the action commands for indirect execution.

use crate::{device::OutOfMemory, format::Format};

/// An offset inside a buffer, in bytes.
pub type Offset = u64;

/// An stride between elements inside a buffer, in bytes.
pub type Stride = u32;

/// A subrange of the buffer.
#[derive(Clone, Debug, Default, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct SubRange {
    /// Offset to the subrange.
    pub offset: Offset,
    /// Size of the subrange, or None for the remaining size of the buffer.
    pub size: Option<Offset>,
}

impl SubRange {
    /// Whole buffer subrange.
    pub const WHOLE: Self = SubRange {
        offset: 0,
        size: None,
    };

    /// Return the stored size, if present, or computed size based on the limit.
    pub fn size_to(&self, limit: Offset) -> Offset {
        self.size.unwrap_or(limit - self.offset)
    }
}

/// Buffer state.
pub type State = Access;

/// Error creating a buffer.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum CreationError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] OutOfMemory),
    /// Requested buffer usage is not supported.
    ///
    /// Older GL version don't support constant buffers or multiple usage flags.
    #[error("Unsupported usage: {0:?}")]
    UnsupportedUsage(Usage),
}

/// Error creating a buffer view.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum ViewCreationError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] OutOfMemory),
    /// Buffer view format is not supported.
    #[error("Unsupported format: {0:?}")]
    UnsupportedFormat(Option<Format>),
}

bitflags!(
    /// Buffer usage flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Usage: u32 {
        ///
        const TRANSFER_SRC  = 0x1;
        ///
        const TRANSFER_DST = 0x2;
        ///
        const UNIFORM_TEXEL = 0x4;
        ///
        const STORAGE_TEXEL = 0x8;
        ///
        const UNIFORM = 0x10;
        ///
        const STORAGE = 0x20;
        ///
        const INDEX = 0x40;
        ///
        const VERTEX = 0x80;
        ///
        const INDIRECT = 0x100;
    }
);

bitflags!(
    /// Buffer create flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct CreateFlags: u32 {
        /// Specifies the view will be backed using sparse memory binding.
        const SPARSE_BINDING = 0x0000_0001;
        /// Specifies the view can be partially backed with sparse memory binding.
        /// Must have `SPARSE_BINDING` enabled.
        const SPARSE_RESIDENCY = 0x0000_0002;
        /// Specifies the view will be backed using sparse memory binding with memory bindings that
        /// might alias other data. Must have `SPARSE_BINDING` enabled.
        const SPARSE_ALIASED = 0x0000_0004;
    }
);

impl Usage {
    /// Returns if the buffer can be used in transfer operations.
    pub fn can_transfer(&self) -> bool {
        self.intersects(Usage::TRANSFER_SRC | Usage::TRANSFER_DST)
    }
}

bitflags!(
    /// Buffer access flags.
    ///
    /// Access of buffers by the pipeline or shaders.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Access: u32 {
        /// Read commands instruction for indirect execution.
        const INDIRECT_COMMAND_READ = 0x1;
        /// Read index values for indexed draw commands.
        ///
        /// See [`draw_indexed`](../command/trait.RawCommandBuffer.html#tymethod.draw_indexed)
        /// and [`draw_indexed_indirect`](../command/trait.RawCommandBuffer.html#tymethod.draw_indexed_indirect).
        const INDEX_BUFFER_READ = 0x2;
        /// Read vertices from vertex buffer for draw commands in the [`VERTEX_INPUT`](
        /// ../pso/struct.PipelineStage.html#associatedconstant.VERTEX_INPUT) stage.
        const VERTEX_BUFFER_READ = 0x4;
        ///
        const UNIFORM_READ = 0x8;
        ///
        const SHADER_READ = 0x20;
        ///
        const SHADER_WRITE = 0x40;
        ///
        const TRANSFER_READ = 0x800;
        ///
        const TRANSFER_WRITE = 0x1000;
        ///
        const HOST_READ = 0x2000;
        ///
        const HOST_WRITE = 0x4000;
        ///
        const MEMORY_READ = 0x8000;
        ///
        const MEMORY_WRITE = 0x10000;
    }
);
