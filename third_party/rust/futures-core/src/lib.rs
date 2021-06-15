//! Core traits and types for asynchronous operations in Rust.

#![cfg_attr(not(feature = "std"), no_std)]
#![warn(missing_docs, missing_debug_implementations, rust_2018_idioms, unreachable_pub)]
// It cannot be included in the published code because this lints have false positives in the minimum required version.
#![cfg_attr(test, warn(single_use_lifetimes))]
#![warn(clippy::all)]
#![doc(test(attr(deny(warnings), allow(dead_code, unused_assignments, unused_variables))))]

#[cfg(feature = "alloc")]
extern crate alloc;

pub mod future;
#[doc(hidden)]
pub use self::future::{FusedFuture, Future, TryFuture};

pub mod stream;
#[doc(hidden)]
pub use self::stream::{FusedStream, Stream, TryStream};

#[macro_use]
pub mod task;

// Not public API.
#[doc(hidden)]
pub mod __private {
    pub use core::task::Poll;
}
