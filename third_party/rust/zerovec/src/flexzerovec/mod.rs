// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! See [`FlexZeroVec`](crate::vecs::FlexZeroVec) for details.

pub(crate) mod owned;
pub(crate) mod slice;
pub(crate) mod vec;

#[cfg(feature = "databake")]
mod databake;

#[cfg(feature = "serde")]
mod serde;

pub use owned::FlexZeroVecOwned;
pub(crate) use slice::chunk_to_usize;
pub use slice::FlexZeroSlice;
pub use vec::FlexZeroVec;
