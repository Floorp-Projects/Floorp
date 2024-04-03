// This library is patched, this considered first-party. Ignore warnings
// as if it were third-party.
#![allow(warnings)]

#[macro_use]
mod macros;
mod error;
mod rure;

pub use crate::error::*;
pub use crate::rure::*;
