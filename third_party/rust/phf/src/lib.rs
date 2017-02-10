//! Compile time optimized maps and sets.
//!
//! PHF data structures can be generated via the syntax extensions in the
//! `phf_macros` crate or via code generation in the `phf_codegen` crate. See
//! the documentation of those crates for more details.
#![doc(html_root_url="https://docs.rs/phf/0.7.20")]
#![warn(missing_docs)]
#![cfg_attr(feature = "core", no_std)]

#[cfg(not(feature = "core"))]
extern crate std as core;

extern crate phf_shared;

use core::ops::Deref;

pub use phf_shared::PhfHash;
#[doc(inline)]
pub use map::Map;
#[doc(inline)]
pub use set::Set;
#[doc(inline)]
pub use ordered_map::OrderedMap;
#[doc(inline)]
pub use ordered_set::OrderedSet;

pub mod map;
pub mod set;
pub mod ordered_map;
pub mod ordered_set;

// WARNING: this is not considered part of phf's public API and is subject to
// change at any time.
//
// Basically Cow, but with the Owned version conditionally compiled
#[doc(hidden)]
pub enum Slice<T: 'static> {
    Static(&'static [T]),
    #[cfg(not(feature = "core"))]
    Dynamic(Vec<T>),
}

impl<T> Deref for Slice<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        match *self {
            Slice::Static(t) => t,
            #[cfg(not(feature = "core"))]
            Slice::Dynamic(ref t) => t,
        }
    }
}
