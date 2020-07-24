use crate::pso;
use std::fmt;

/// A clear color union, which can be either f32, i32, or u32.
#[repr(C)]
#[derive(Clone, Copy)]
pub union ClearColor {
    /// `f32` variant
    pub float32: [f32; 4],
    /// `i32` variant
    pub sint32: [i32; 4],
    /// `u32` variant
    pub uint32: [u32; 4],
}

impl fmt::Debug for ClearColor {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln![f, "ClearColor"]
    }
}

/// A combination of depth and stencil clear values.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct ClearDepthStencil {
    /// Depth value
    pub depth: f32,
    /// Stencil value
    pub stencil: u32,
}

/// A set of clear values for a single attachment.
#[repr(C)]
#[derive(Clone, Copy)]
pub union ClearValue {
    /// Clear color
    pub color: ClearColor,
    /// Clear depth and stencil
    pub depth_stencil: ClearDepthStencil,
    _align: [u32; 4],
}

impl fmt::Debug for ClearValue {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("ClearValue")
            .field("color", unsafe { &self.color.uint32 })
            .field("depth_stencil", unsafe { &self.depth_stencil })
            .finish()
    }
}

/// Attachment clear description for the current subpass.
#[derive(Clone, Copy, Debug)]
pub enum AttachmentClear {
    /// Clear color attachment.
    Color {
        /// Index inside the `SubpassDesc::colors` array.
        index: usize,
        /// Value to clear with.
        value: ClearColor,
    },
    /// Clear depth-stencil attachment.
    DepthStencil {
        /// Depth value to clear with.
        depth: Option<pso::DepthValue>,
        /// Stencil value to clear with.
        stencil: Option<pso::StencilValue>,
    },
}
