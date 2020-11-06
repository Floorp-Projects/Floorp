//! GPU descriptor allocator
//!

#![warn(
    missing_docs,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications
)]
#[allow(clippy::new_without_default)]
mod allocator;
mod counts;

pub use crate::{allocator::*, counts::*};
