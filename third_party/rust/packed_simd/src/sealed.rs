//! Sealed traits

/// Trait implemented by arrays that can be SIMD types.
#[doc(hidden)]
pub trait SimdArray {
    /// The type of the #[repr(simd)] type.
    type Tuple: Copy + Clone;
    /// The element type of the vector.
    type T;
    /// The number of elements in the array.
    const N: usize;
    /// The type: `[u32; Self::N]`.
    type NT;
}

/// This traits is used to constraint the arguments
/// and result type of the portable shuffles.
#[doc(hidden)]
pub trait Shuffle<Lanes> {
    // Lanes is a `[u32; N]` where `N` is the number of vector lanes

    /// The result type of the shuffle.
    type Output;
}

/// This trait is implemented by all SIMD vector types.
#[doc(hidden)]
pub trait Simd {
    /// Element type of the SIMD vector
    type Element;
    /// The number of elements in the SIMD vector.
    const LANES: usize;
    /// The type: `[u32; Self::N]`.
    type LanesType;
}

/// This trait is implemented by all mask types
#[doc(hidden)]
pub trait Mask {
    fn test(&self) -> bool;
}
