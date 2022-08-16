//! impl Fallible collections on allocation errors, quite as describe
//! in [RFC 2116](https://github.com/rust-lang/rfcs/blob/master/text/2116-alloc-me-maybe.md)
//! This was used in the turbofish OS hobby project to mitigate the
//! the lack of faillible allocation in rust.
//!
//! The `Try*` types in this module are thin wrappers around the stdlib types to add
//! support for fallible allocation. The API differences from the stdlib types ensure
//! that all operations which allocate return a `Result`. For the most part, this simply
//! means adding a `Result` return value to functions which return nothing or a
//! non-`Result` value. However, these types implement some traits whose API cannot
//! communicate failure, but which do require allocation, so it is important that these
//! wrapper types do not implement these traits.
//!
//! Specifically, these types must not implement any of the following traits:
//! - Clone
//! - Extend
//! - From
//! - FromIterator
//!
//! This list may not be exhaustive. Exercise caution when implementing
//! any new traits to ensure they won't potentially allocate in a way that
//! can't return a Result to indicate allocation failure.

#![cfg_attr(not(test), no_std)]
#![cfg_attr(all(feature = "unstable", not(feature = "rust_1_57")), feature(try_reserve))]
#![cfg_attr(feature = "unstable", feature(min_specialization))]
#![cfg_attr(feature = "unstable", feature(allocator_api))]
#![cfg_attr(feature = "unstable", feature(dropck_eyepatch))]
#![cfg_attr(feature = "unstable", feature(ptr_internals))]
#![cfg_attr(feature = "unstable", feature(core_intrinsics))]
#![cfg_attr(all(feature = "unstable", not(feature = "rust_1_57")), feature(maybe_uninit_ref))]
#![cfg_attr(feature = "unstable", feature(maybe_uninit_slice))]
#![cfg_attr(feature = "unstable", feature(maybe_uninit_extra))]
#![cfg_attr(feature = "unstable", feature(maybe_uninit_uninit_array))]
extern crate alloc;
#[cfg(feature = "std")]
extern crate std;

pub mod boxed;
pub use boxed::*;
#[macro_use]
pub mod vec;
pub use vec::*;
pub mod rc;
pub use rc::*;
pub mod arc;
pub use arc::*;
#[cfg(feature = "unstable")]
pub mod btree;
#[cfg(not(feature = "unstable"))]
pub mod hashmap;
#[cfg(not(feature = "unstable"))]
pub use hashmap::*;
#[macro_use]
pub mod format;
pub mod try_clone;

#[cfg(all(feature = "unstable", not(feature = "rust_1_57")))]
pub use alloc::collections::TryReserveError;
#[cfg(not(all(feature = "unstable", not(feature = "rust_1_57"))))]
pub use hashbrown::TryReserveError;

#[cfg(feature = "std_io")]
pub use vec::std_io::*;

/// trait for trying to clone an elem, return an error instead of
/// panic if allocation failed
/// # Examples
///
/// ```
/// use fallible_collections::TryClone;
/// let mut vec = vec![42, 100];
/// assert_eq!(vec.try_clone().unwrap(), vec)
/// ```
pub trait TryClone {
    /// try clone method, (Self must be sized because of Result
    /// constraint)
    fn try_clone(&self) -> Result<Self, TryReserveError>
    where
        Self: core::marker::Sized;
}

#[cfg(feature = "rust_1_57")]
fn make_try_reserve_error(len: usize, additional: usize, elem_size: usize, align: usize) -> hashbrown::TryReserveError {
    if let Some(size) = len.checked_add(additional).and_then(|l| l.checked_mul(elem_size)) {
        if let Ok(layout) = alloc::alloc::Layout::from_size_align(size, align) {
            return TryReserveError::AllocError { layout }
        }
    }
    TryReserveError::CapacityOverflow
}
