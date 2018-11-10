//! Target "triple" support.

#![deny(missing_docs, trivial_numeric_casts, unused_extern_crates)]
#![warn(unused_import_braces)]
#![cfg_attr(
    feature = "cargo-clippy",
    warn(
        float_arithmetic,
        mut_mut,
        nonminimal_bool,
        option_map_unwrap_or,
        option_map_unwrap_or_else,
        print_stdout,
        unicode_not_nfc,
        use_self
    )
)]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(not(feature = "std"), feature(alloc))]

#[cfg(not(feature = "std"))]
extern crate alloc;

extern crate failure;
#[macro_use]
extern crate failure_derive;

#[cfg(not(feature = "std"))]
mod std {
    pub use alloc::{borrow, string};
    pub use core::*;
}

mod host;
mod parse_error;
mod targets;
#[macro_use]
mod triple;

pub use host::HOST;
pub use parse_error::ParseError;
pub use targets::{Architecture, BinaryFormat, Environment, OperatingSystem, Vendor};
pub use triple::{CallingConvention, Endianness, PointerWidth, Triple};
