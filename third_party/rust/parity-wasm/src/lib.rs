//! WebAssembly format library
#![warn(missing_docs)]

#![cfg_attr(not(feature = "std"), no_std)]

#[macro_use]
extern crate alloc;

pub mod elements;
pub mod builder;
mod io;

pub use elements::{
	Error as SerializationError,
	deserialize_buffer,
	serialize,
	peek_size,
};

#[cfg(feature = "std")]
pub use elements::{
	deserialize_file,
	serialize_to_file,
};
