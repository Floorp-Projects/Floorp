//! Output Merger (OM) stage description.
//! The final stage in a pipeline that creates pixel colors from
//! the input shader results, depth/stencil information, etc.

use crate::pso::{graphics::StencilValue, State};

/// A pixel-wise comparison function.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Comparison {
    /// `false`
    Never = 0,
    /// `x < y`
    Less = 1,
    /// `x == y`
    Equal = 2,
    /// `x <= y`
    LessEqual = 3,
    /// `x > y`
    Greater = 4,
    /// `x != y`
    NotEqual = 5,
    /// `x >= y`
    GreaterEqual = 6,
    /// `true`
    Always = 7,
}

bitflags!(
    /// Target output color mask.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct ColorMask: u8 {
        /// Red mask
        const RED     = 0x1;
        /// Green mask
        const GREEN   = 0x2;
        /// Blue mask
        const BLUE    = 0x4;
        /// Alpha channel mask
        const ALPHA   = 0x8;
        /// Mask for RGB channels
        const COLOR   = 0x7;
        /// Mask all channels
        const ALL     = 0xF;
        /// Mask no channels.
        const NONE    = 0x0;
    }
);

impl Default for ColorMask {
    fn default() -> Self {
        Self::ALL
    }
}

/// Defines the possible blending factors.
/// During blending, the source or destination fragment may be
/// multiplied by a factor to produce the final result.
#[allow(missing_docs)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Factor {
    Zero = 0,
    One = 1,
    SrcColor = 2,
    OneMinusSrcColor = 3,
    DstColor = 4,
    OneMinusDstColor = 5,
    SrcAlpha = 6,
    OneMinusSrcAlpha = 7,
    DstAlpha = 8,
    OneMinusDstAlpha = 9,
    ConstColor = 10,
    OneMinusConstColor = 11,
    ConstAlpha = 12,
    OneMinusConstAlpha = 13,
    SrcAlphaSaturate = 14,
    Src1Color = 15,
    OneMinusSrc1Color = 16,
    Src1Alpha = 17,
    OneMinusSrc1Alpha = 18,
}

/// Blending operations.
#[allow(missing_docs)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum BlendOp {
    /// Adds source and destination.
    /// Source and destination are multiplied by factors before addition.
    Add { src: Factor, dst: Factor },
    /// Subtracts destination from source.
    /// Source and destination are multiplied by factors before subtraction.
    Sub { src: Factor, dst: Factor },
    /// Subtracts source from destination.
    /// Source and destination are multiplied by factors before subtraction.
    RevSub { src: Factor, dst: Factor },
    /// Component-wise minimum value of source and destination.
    Min,
    /// Component-wise maximum value of source and destination.
    Max,
}

impl BlendOp {
    /// Replace the destination value with the source.
    pub const REPLACE: Self = BlendOp::Add {
        src: Factor::One,
        dst: Factor::Zero,
    };
    /// Add the source and destination together.
    pub const ADD: Self = BlendOp::Add {
        src: Factor::One,
        dst: Factor::One,
    };
    /// Alpha blend the source and destination together.
    pub const ALPHA: Self = BlendOp::Add {
        src: Factor::SrcAlpha,
        dst: Factor::OneMinusSrcAlpha,
    };
    /// Alpha blend a premultiplied-alpha source with the destination.
    pub const PREMULTIPLIED_ALPHA: Self = BlendOp::Add {
        src: Factor::One,
        dst: Factor::OneMinusSrcAlpha,
    };
}

/// Specifies whether to use blending, and if so,
/// which operations to use for color and alpha channels.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BlendState {
    /// The blend operation to use for the color channels.
    pub color: BlendOp,
    /// The blend operation to use for the alpha channel.
    pub alpha: BlendOp,
}

impl BlendState {
    /// Replace the color.
    pub const REPLACE: Self = BlendState {
        color: BlendOp::REPLACE,
        alpha: BlendOp::REPLACE,
    };
    /// Additive blending.
    pub const ADD: Self = BlendState {
        color: BlendOp::ADD,
        alpha: BlendOp::ADD,
    };
    /// Multiplicative blending.
    pub const MULTIPLY: Self = BlendState {
        color: BlendOp::Add {
            src: Factor::Zero,
            dst: Factor::SrcColor,
        },
        alpha: BlendOp::Add {
            src: Factor::Zero,
            dst: Factor::SrcAlpha,
        },
    };
    /// Alpha blending.
    pub const ALPHA: Self = BlendState {
        color: BlendOp::ALPHA,
        alpha: BlendOp::PREMULTIPLIED_ALPHA,
    };
    /// Pre-multiplied alpha blending.
    pub const PREMULTIPLIED_ALPHA: Self = BlendState {
        color: BlendOp::PREMULTIPLIED_ALPHA,
        alpha: BlendOp::PREMULTIPLIED_ALPHA,
    };
}

/// PSO color target descriptor.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct ColorBlendDesc {
    /// Color write mask.
    pub mask: ColorMask,
    /// Blend state, if enabled.
    pub blend: Option<BlendState>,
}

impl ColorBlendDesc {
    /// Empty blend descriptor just writes out the color without blending.
    // this can be used because `Default::default()` isn't a const function...
    pub const EMPTY: Self = ColorBlendDesc {
        mask: ColorMask::ALL,
        blend: None,
    };
}

/// Depth test state.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DepthTest {
    /// Comparison function to use.
    pub fun: Comparison,
    /// Specify whether to write to the depth buffer or not.
    pub write: bool,
}

impl DepthTest {
    /// A depth test that always fails.
    pub const FAIL: Self = DepthTest {
        fun: Comparison::Never,
        write: false,
    };
    /// A depth test that always succeeds but doesn't
    /// write to the depth buffer
    // DOC TODO: Not a terribly helpful description there...
    pub const PASS_TEST: Self = DepthTest {
        fun: Comparison::Always,
        write: false,
    };
    /// A depth test that always succeeds and writes its result
    /// to the depth buffer.
    pub const PASS_WRITE: Self = DepthTest {
        fun: Comparison::Always,
        write: true,
    };
}

/// The operation to use for stencil masking.
#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum StencilOp {
    /// Keep the current value in the stencil buffer (no change).
    Keep = 0,
    /// Set the value in the stencil buffer to zero.
    Zero = 1,
    /// Set the stencil buffer value to `reference` from `StencilFace`.
    Replace = 2,
    /// Increment the stencil buffer value, clamping to its maximum value.
    IncrementClamp = 3,
    /// Decrement the stencil buffer value, clamping to its minimum value.
    DecrementClamp = 4,
    /// Bitwise invert the current value in the stencil buffer.
    Invert = 5,
    /// Increment the stencil buffer value, wrapping around to 0 on overflow.
    IncrementWrap = 6,
    /// Decrement the stencil buffer value, wrapping around to the maximum value on overflow.
    DecrementWrap = 7,
}

/// Complete stencil state for a given side of a face.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct StencilFace {
    /// Comparison function to use to determine if the stencil test passes.
    pub fun: Comparison,
    /// What operation to do if the stencil test fails.
    pub op_fail: StencilOp,
    /// What operation to do if the stencil test passes but the depth test fails.
    pub op_depth_fail: StencilOp,
    /// What operation to do if both the depth and stencil test pass.
    pub op_pass: StencilOp,
}

impl Default for StencilFace {
    fn default() -> StencilFace {
        StencilFace {
            fun: Comparison::Never,
            op_fail: StencilOp::Keep,
            op_depth_fail: StencilOp::Keep,
            op_pass: StencilOp::Keep,
        }
    }
}

/// A generic struct holding the properties of two sides of a polygon.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Sided<T> {
    /// Information about the front face.
    pub front: T,
    /// Information about the back face.
    pub back: T,
}

impl<T: Copy> Sided<T> {
    /// Create a new `Sided` structure with both `front` and `back` holding
    /// the same value.
    pub fn new(value: T) -> Self {
        Sided {
            front: value,
            back: value,
        }
    }
}

/// Pair of stencil values that could be either baked into a graphics pipeline
/// or provided dynamically.
pub type StencilValues = State<Sided<StencilValue>>;

/// Defines a stencil test. Stencil testing is an operation
/// performed to cull fragments;
/// the new fragment is tested against the value held in the
/// stencil buffer, and if the test fails the fragment is
/// discarded.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct StencilTest {
    /// Operations for stencil faces.
    pub faces: Sided<StencilFace>,
    /// Masks that are ANDd with both the stencil buffer value and the reference value when they
    /// are read before doing the stencil test.
    pub read_masks: StencilValues,
    /// Mask that are ANDd with the stencil value before writing to the stencil buffer.
    pub write_masks: StencilValues,
    /// The reference values used for stencil tests.
    pub reference_values: StencilValues,
}

impl Default for StencilTest {
    fn default() -> Self {
        StencilTest {
            faces: Sided::default(),
            read_masks: State::Static(Sided::new(!0)),
            write_masks: State::Static(Sided::new(!0)),
            reference_values: State::Static(Sided::new(0)),
        }
    }
}

/// PSO depth-stencil target descriptor.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DepthStencilDesc {
    /// Optional depth testing/writing.
    pub depth: Option<DepthTest>,
    /// Enable depth bounds testing.
    pub depth_bounds: bool,
    /// Stencil test/write.
    pub stencil: Option<StencilTest>,
}

impl DepthStencilDesc {
    /// Returns true if the descriptor assumes the depth attachment.
    pub fn uses_depth(&self) -> bool {
        self.depth.is_some() || self.depth_bounds
    }
    /// Returns true if the descriptor assumes the stencil attachment.
    pub fn uses_stencil(&self) -> bool {
        self.stencil.is_some()
    }
}

bitflags!(
    /// Face.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Face: u32 {
        /// Empty face. TODO: remove when constexpr are stabilized to use empty()
        const NONE = 0x0;
        /// Front face.
        const FRONT = 0x1;
        /// Back face.
        const BACK = 0x2;
    }
);
