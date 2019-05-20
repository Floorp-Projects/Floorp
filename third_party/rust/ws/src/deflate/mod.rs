//! The deflate module provides tools for applying the permessage-deflate extension.

extern crate libc;
extern crate libz_sys as ffi;

mod context;
mod extension;

pub use self::extension::{DeflateBuilder, DeflateHandler, DeflateSettings};
