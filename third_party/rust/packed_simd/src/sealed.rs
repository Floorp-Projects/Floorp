//! Sealed traits

/// A sealed trait, this is logically private to the crate
/// and will prevent implementations from outside the crate
pub trait Seal<T = ()> {}

/// Trait implemented by arrays that can be SIMD types.
pub trait SimdArray: Seal {
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
pub trait Shuffle<Lanes>: Seal<Lanes> {
    // Lanes is a `[u32; N]` where `N` is the number of vector lanes

    /// The result type of the shuffle.
    type Output;
}

/// This trait is implemented by all SIMD vector types.
pub trait Simd: Seal {
    /// Element type of the SIMD vector
    type Element;
    /// The number of elements in the SIMD vector.
    const LANES: usize;
    /// The type: `[u32; Self::N]`.
    type LanesType;
}

/// This trait is implemented by all mask types
pub trait Mask: Seal {
    fn test(&self) -> bool;
}
