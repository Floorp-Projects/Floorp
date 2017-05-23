//! Bzip compression for Rust
//!
//! This library contains bindings to libbz2 to support bzip compression and
//! decompression for Rust. The streams offered in this library are primarily
//! found in the `reader` and `writer` modules. Both compressors and
//! decompressors are available in each module depending on what operation you
//! need.
//!
//! Access to the raw decompression/compression stream is also provided through
//! the `raw` module which has a much closer interface to libbz2.
//!
//! # Example
//!
//! ```
//! use std::io::prelude::*;
//! use bzip2::Compression;
//! use bzip2::read::{BzEncoder, BzDecoder};
//!
//! // Round trip some bytes from a byte source, into a compressor, into a
//! // decompressor, and finally into a vector.
//! let data = "Hello, World!".as_bytes();
//! let compressor = BzEncoder::new(data, Compression::Best);
//! let mut decompressor = BzDecoder::new(compressor);
//!
//! let mut contents = String::new();
//! decompressor.read_to_string(&mut contents).unwrap();
//! assert_eq!(contents, "Hello, World!");
//! ```
//!
//! # Async I/O
//!
//! This crate optionally can support async I/O streams with the Tokio stack via
//! the `tokio` feature of this crate:
//!
//! ```toml
//! bzip2 = { version = "0.3", features = ["tokio"] }
//! ```
//!
//! All methods are internally capable of working with streams that may return
//! `ErrorKind::WouldBlock` when they're not ready to perform the particular
//! operation.
//!
//! Note that care needs to be taken when using these objects, however. The
//! Tokio runtime, in particular, requires that data is fully flushed before
//! dropping streams. For compatibility with blocking streams all streams are
//! flushed/written when they are dropped, and this is not always a suitable
//! time to perform I/O. If I/O streams are flushed before drop, however, then
//! these operations will be a noop.

#![deny(missing_docs, warnings)]
#![doc(html_root_url = "https://docs.rs/bzip2/0.3")]

extern crate bzip2_sys as ffi;
extern crate libc;
#[cfg(test)]
extern crate rand;
#[cfg(test)]
extern crate quickcheck;
#[cfg(feature = "tokio")]
#[macro_use]
extern crate tokio_io;
#[cfg(feature = "tokio")]
extern crate futures;

pub use mem::{Compress, Decompress, Action, Status, Error};

mod mem;

pub mod bufread;
pub mod read;
pub mod write;

/// When compressing data, the compression level can be specified by a value in
/// this enum.
#[derive(Copy, Clone, Debug)]
pub enum Compression {
    /// Optimize for the best speed of encoding.
    Fastest = 1,
    /// Optimize for the size of data being encoded.
    Best = 9,
    /// Choose the default compression, a balance between speed and size.
    Default = 6,
}

