//! The deflate module provides tools for applying the permessage-deflate extension.

extern crate libz_sys as ffi;
extern crate libc;

mod context;
mod extension;

pub use self::extension::{DeflateHandler, DeflateBuilder, DeflateSettings};






