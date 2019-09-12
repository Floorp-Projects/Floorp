//! A DEFLATE-based stream compression/decompression library
//!
//! This library is meant to supplement/replace the
//! `flate` library that was previously part of the standard rust distribution
//! providing a streaming encoder/decoder rather than purely
//! an in-memory encoder/decoder.
//!
//! Like with [`flate`], flate2 is based on [`miniz.c`][1]
//!
//! [1]: https://github.com/richgel999/miniz
//! [`flate`]: https://github.com/rust-lang/rust/tree/1.19.0/src/libflate
//!
//! # Organization
//!
//! This crate consists mainly of three modules, [`read`], [`write`], and
//! [`bufread`]. Each module contains a number of types used to encode and
//! decode various streams of data.
//!
//! All types in the [`write`] module work on instances of [`Write`][write],
//! whereas all types in the [`read`] module work on instances of
//! [`Read`][read] and [`bufread`] works with [`BufRead`][bufread]. If you
//! are decoding directly from a `&[u8]`, use the [`bufread`] types.
//!
//! ```
//! use flate2::write::GzEncoder;
//! use flate2::Compression;
//! use std::io;
//! use std::io::prelude::*;
//!
//! # fn main() { let _ = run(); }
//! # fn run() -> io::Result<()> {
//! let mut encoder = GzEncoder::new(Vec::new(), Compression::default());
//! encoder.write_all(b"Example")?;
//! # Ok(())
//! # }
//! ```
//!
//!
//! Other various types are provided at the top-level of the crate for
//! management and dealing with encoders/decoders. Also note that types which
//! operate over a specific trait often implement the mirroring trait as well.
//! For example a `flate2::read::DeflateDecoder<T>` *also* implements the
//! `Write` trait if `T: Write`. That is, the "dual trait" is forwarded directly
//! to the underlying object if available.
//!
//! [`read`]: read/index.html
//! [`bufread`]: bufread/index.html
//! [`write`]: write/index.html
//! [read]: https://doc.rust-lang.org/std/io/trait.Read.html
//! [write]: https://doc.rust-lang.org/std/io/trait.Write.html
//! [bufread]: https://doc.rust-lang.org/std/io/trait.BufRead.html
//!
//! # Async I/O
//!
//! This crate optionally can support async I/O streams with the [Tokio stack] via
//! the `tokio` feature of this crate:
//!
//! [Tokio stack]: https://tokio.rs/
//!
//! ```toml
//! flate2 = { version = "0.2", features = ["tokio"] }
//! ```
//!
//! All methods are internally capable of working with streams that may return
//! [`ErrorKind::WouldBlock`] when they're not ready to perform the particular
//! operation.
//!
//! [`ErrorKind::WouldBlock`]: https://doc.rust-lang.org/std/io/enum.ErrorKind.html
//!
//! Note that care needs to be taken when using these objects, however. The
//! Tokio runtime, in particular, requires that data is fully flushed before
//! dropping streams. For compatibility with blocking streams all streams are
//! flushed/written when they are dropped, and this is not always a suitable
//! time to perform I/O. If I/O streams are flushed before drop, however, then
//! these operations will be a noop.
#![doc(html_root_url = "https://docs.rs/flate2/0.2")]
#![deny(missing_docs)]
#![deny(missing_debug_implementations)]
#![allow(trivial_numeric_casts)]
#![cfg_attr(test, deny(warnings))]

extern crate crc32fast;
#[cfg(feature = "tokio")]
extern crate futures;
#[cfg(not(any(
    all(not(feature = "zlib"), feature = "rust_backend"),
    all(target_arch = "wasm32", not(target_os = "emscripten"))
)))]
extern crate libc;
#[cfg(test)]
extern crate quickcheck;
#[cfg(test)]
extern crate rand;
#[cfg(feature = "tokio")]
extern crate tokio_io;

pub use crc::{Crc, CrcReader, CrcWriter};
pub use gz::GzBuilder;
pub use gz::GzHeader;
pub use mem::{Compress, CompressError, Decompress, DecompressError, Status};
pub use mem::{FlushCompress, FlushDecompress};

mod bufreader;
mod crc;
mod deflate;
mod ffi;
mod gz;
mod mem;
mod zio;
mod zlib;

/// Types which operate over [`Read`] streams, both encoders and decoders for
/// various formats.
///
/// [`Read`]: https://doc.rust-lang.org/std/io/trait.Read.html
pub mod read {
    pub use deflate::read::DeflateDecoder;
    pub use deflate::read::DeflateEncoder;
    pub use gz::read::GzDecoder;
    pub use gz::read::GzEncoder;
    pub use gz::read::MultiGzDecoder;
    pub use zlib::read::ZlibDecoder;
    pub use zlib::read::ZlibEncoder;
}

/// Types which operate over [`Write`] streams, both encoders and decoders for
/// various formats.
///
/// [`Write`]: https://doc.rust-lang.org/std/io/trait.Write.html
pub mod write {
    pub use deflate::write::DeflateDecoder;
    pub use deflate::write::DeflateEncoder;
    pub use gz::write::GzDecoder;
    pub use gz::write::GzEncoder;
    pub use zlib::write::ZlibDecoder;
    pub use zlib::write::ZlibEncoder;
}

/// Types which operate over [`BufRead`] streams, both encoders and decoders for
/// various formats.
///
/// [`BufRead`]: https://doc.rust-lang.org/std/io/trait.BufRead.html
pub mod bufread {
    pub use deflate::bufread::DeflateDecoder;
    pub use deflate::bufread::DeflateEncoder;
    pub use gz::bufread::GzDecoder;
    pub use gz::bufread::GzEncoder;
    pub use gz::bufread::MultiGzDecoder;
    pub use zlib::bufread::ZlibDecoder;
    pub use zlib::bufread::ZlibEncoder;
}

fn _assert_send_sync() {
    fn _assert_send_sync<T: Send + Sync>() {}

    _assert_send_sync::<read::DeflateEncoder<&[u8]>>();
    _assert_send_sync::<read::DeflateDecoder<&[u8]>>();
    _assert_send_sync::<read::ZlibEncoder<&[u8]>>();
    _assert_send_sync::<read::ZlibDecoder<&[u8]>>();
    _assert_send_sync::<read::GzEncoder<&[u8]>>();
    _assert_send_sync::<read::GzDecoder<&[u8]>>();
    _assert_send_sync::<read::MultiGzDecoder<&[u8]>>();
    _assert_send_sync::<write::DeflateEncoder<Vec<u8>>>();
    _assert_send_sync::<write::DeflateDecoder<Vec<u8>>>();
    _assert_send_sync::<write::ZlibEncoder<Vec<u8>>>();
    _assert_send_sync::<write::ZlibDecoder<Vec<u8>>>();
    _assert_send_sync::<write::GzEncoder<Vec<u8>>>();
    _assert_send_sync::<write::GzDecoder<Vec<u8>>>();
}

/// When compressing data, the compression level can be specified by a value in
/// this enum.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct Compression(u32);

impl Compression {
    /// Creates a new description of the compression level with an explicitly
    /// specified integer.
    ///
    /// The integer here is typically on a scale of 0-9 where 0 means "no
    /// compression" and 9 means "take as long as you'd like".
    pub fn new(level: u32) -> Compression {
        Compression(level)
    }

    /// No compression is to be performed, this may actually inflate data
    /// slightly when encoding.
    pub fn none() -> Compression {
        Compression(0)
    }

    /// Optimize for the best speed of encoding.
    pub fn fast() -> Compression {
        Compression(1)
    }

    /// Optimize for the size of data being encoded.
    pub fn best() -> Compression {
        Compression(9)
    }

    /// Returns an integer representing the compression level, typically on a
    /// scale of 0-9
    pub fn level(&self) -> u32 {
        self.0
    }
}

impl Default for Compression {
    fn default() -> Compression {
        Compression(6)
    }
}

#[cfg(test)]
fn random_bytes() -> impl Iterator<Item = u8> {
    use rand::Rng;
    use std::iter;

    iter::repeat(()).map(|_| rand::thread_rng().gen())
}
