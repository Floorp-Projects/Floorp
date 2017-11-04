//! A DEFLATE-based stream compression/decompression library
//!
//! This library is meant to supplement/replace the standard distributon's
//! libflate library by providing a streaming encoder/decoder rather than purely
//! an in-memory encoder/decoder.
//!
//! Like with [`libflate`], flate2 is based on [`miniz.c`][1]
//!
//! [1]: https://code.google.com/p/miniz/
//! [`libflate`]: https://docs.rs/crate/libflate/
//!
//! # Organization
//!
//! This crate consists mainly of two modules, [`read`] and [`write`]. Each
//! module contains a number of types used to encode and decode various streams
//! of data. All types in the [`write`] module work on instances of [`Write`],
//! whereas all types in the [`read`] module work on instances of [`Read`].
//!
//! ```
//! use flate2::write::GzEncoder;
//! use flate2::Compression;
//! use std::io;
//! use std::io::prelude::*;
//!
//! # fn main() { let _ = run(); }
//! # fn run() -> io::Result<()> {
//! let mut encoder = GzEncoder::new(Vec::new(), Compression::Default);
//! encoder.write(b"Example")?;
//! # Ok(())
//! # }
//! ```
//!
//!
//! Other various types are provided at the top-level of the crate for
//! management and dealing with encoders/decoders.
//!
//! [`read`]: read/index.html
//! [`write`]: write/index.html
//! [`Read`]: https://doc.rust-lang.org/std/io/trait.Read.html
//! [`Write`]: https://doc.rust-lang.org/std/io/trait.Write.html
//!
//! # Helper traits
//!
//! There are two helper traits provided: [`FlateReadExt`] and [`FlateWriteExt`].
//! These provide convenience methods for creating a decoder/encoder out of an
//! already existing stream to chain construction.
//!
//! [`FlateReadExt`]: trait.FlateReadExt.html
//! [`FlateWriteExt`]: trait.FlateWriteExt.html
//!
//! ```
//! use flate2::{FlateReadExt, Compression};
//! use std::io::prelude::*;
//! use std::io;
//! use std::fs::File;
//!
//! # fn main() {
//! #    println!("{}", run().unwrap());
//! # }
//! #
//! // Read contents of file with a compression stream, then decompress with GZ
//!
//! # fn run() -> io::Result<String> {
//! let f = File::open("examples/hello_world.txt")?;
//!
//! //gz_encode method comes from FlateReadExt and applies to a std::fs::File
//! let data = f.gz_encode(Compression::Default);
//! let mut buffer = String::new();
//!
//! //gz_decode method comes from FlateReadExt and applies to a &[u8]
//! &data.gz_decode()?.read_to_string(&mut buffer)?;
//! # Ok(buffer)
//! # }
//! ```
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

#[cfg(feature = "tokio")]
extern crate futures;
extern crate libc;
#[cfg(test)]
extern crate quickcheck;
#[cfg(test)]
extern crate rand;
#[cfg(feature = "tokio")]
#[macro_use]
extern crate tokio_io;

use std::io::prelude::*;
use std::io;

pub use gz::Builder as GzBuilder;
pub use gz::Header as GzHeader;
pub use mem::{Compress, DataError, Decompress, Flush, Status};
pub use crc::{Crc, CrcReader};

mod bufreader;
mod crc;
mod deflate;
mod ffi;
mod gz;
mod zio;
mod mem;
mod zlib;

/// Types which operate over [`Read`] streams, both encoders and decoders for
/// various formats.
///
/// [`Read`]: https://doc.rust-lang.org/std/io/trait.Read.html
pub mod read {
    pub use deflate::read::DeflateEncoder;
    pub use deflate::read::DeflateDecoder;
    pub use zlib::read::ZlibEncoder;
    pub use zlib::read::ZlibDecoder;
    pub use gz::read::GzEncoder;
    pub use gz::read::GzDecoder;
    pub use gz::read::MultiGzDecoder;
}

/// Types which operate over [`Write`] streams, both encoders and decoders for
/// various formats.
///
/// [`Write`]: https://doc.rust-lang.org/std/io/trait.Write.html
pub mod write {
    pub use deflate::write::DeflateEncoder;
    pub use deflate::write::DeflateDecoder;
    pub use zlib::write::ZlibEncoder;
    pub use zlib::write::ZlibDecoder;
    pub use gz::write::GzEncoder;
}

/// Types which operate over [`BufRead`] streams, both encoders and decoders for
/// various formats.
///
/// [`BufRead`]: https://doc.rust-lang.org/std/io/trait.BufRead.html
pub mod bufread {
    pub use deflate::bufread::DeflateEncoder;
    pub use deflate::bufread::DeflateDecoder;
    pub use zlib::bufread::ZlibEncoder;
    pub use zlib::bufread::ZlibDecoder;
    pub use gz::bufread::GzEncoder;
    pub use gz::bufread::GzDecoder;
    pub use gz::bufread::MultiGzDecoder;
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
}

/// When compressing data, the compression level can be specified by a value in
/// this enum.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Compression {
    /// No compression is to be performed, this may actually inflate data
    /// slightly when encoding.
    None = 0,
    /// Optimize for the best speed of encoding.
    Fast = 1,
    /// Optimize for the size of data being encoded.
    Best = 9,
    /// Choose the default compression, a balance between speed and size.
    Default = 6,
}

/// Default to Compression::Default.
impl Default for Compression {
    fn default() -> Compression {
        Compression::Default
    }
}

/// A helper trait to create encoder/decoders with method syntax.
pub trait FlateReadExt: Read + Sized {
    /// Consume this reader to create a compression stream at the specified
    /// compression level.
    fn gz_encode(self, lvl: Compression) -> read::GzEncoder<Self> {
        read::GzEncoder::new(self, lvl)
    }

    /// Consume this reader to create a decompression stream of this stream.
    fn gz_decode(self) -> io::Result<read::GzDecoder<Self>> {
        read::GzDecoder::new(self)
    }

    /// Consume this reader to create a compression stream at the specified
    /// compression level.
    fn zlib_encode(self, lvl: Compression) -> read::ZlibEncoder<Self> {
        read::ZlibEncoder::new(self, lvl)
    }

    /// Consume this reader to create a decompression stream of this stream.
    fn zlib_decode(self) -> read::ZlibDecoder<Self> {
        read::ZlibDecoder::new(self)
    }

    /// Consume this reader to create a compression stream at the specified
    /// compression level.
    fn deflate_encode(self, lvl: Compression) -> read::DeflateEncoder<Self> {
        read::DeflateEncoder::new(self, lvl)
    }

    /// Consume this reader to create a decompression stream of this stream.
    fn deflate_decode(self) -> read::DeflateDecoder<Self> {
        read::DeflateDecoder::new(self)
    }
}

/// A helper trait to create encoder/decoders with method syntax.
pub trait FlateWriteExt: Write + Sized {
    /// Consume this writer to create a compression stream at the specified
    /// compression level.
    fn gz_encode(self, lvl: Compression) -> write::GzEncoder<Self> {
        write::GzEncoder::new(self, lvl)
    }

    // TODO: coming soon to a theater near you!
    // /// Consume this writer to create a decompression stream of this stream.
    // fn gz_decode(self) -> IoResult<write::GzDecoder<Self>> {
    //     write::GzDecoder::new(self)
    // }

    /// Consume this writer to create a compression stream at the specified
    /// compression level.
    fn zlib_encode(self, lvl: Compression) -> write::ZlibEncoder<Self> {
        write::ZlibEncoder::new(self, lvl)
    }

    /// Consume this writer to create a decompression stream of this stream.
    fn zlib_decode(self) -> write::ZlibDecoder<Self> {
        write::ZlibDecoder::new(self)
    }

    /// Consume this writer to create a compression stream at the specified
    /// compression level.
    fn deflate_encode(self, lvl: Compression) -> write::DeflateEncoder<Self> {
        write::DeflateEncoder::new(self, lvl)
    }

    /// Consume this writer to create a decompression stream of this stream.
    fn deflate_decode(self) -> write::DeflateDecoder<Self> {
        write::DeflateDecoder::new(self)
    }
}

impl<T: Read> FlateReadExt for T {}
impl<T: Write> FlateWriteExt for T {}

#[cfg(test)]
mod test {
    use std::io::prelude::*;
    use {Compression, FlateReadExt};

    #[test]
    fn crazy() {
        let rdr = &mut b"foobar";
        let mut res = Vec::new();
        rdr.gz_encode(Compression::Default)
            .deflate_encode(Compression::Default)
            .zlib_encode(Compression::Default)
            .zlib_decode()
            .deflate_decode()
            .gz_decode()
            .unwrap()
            .read_to_end(&mut res)
            .unwrap();
        assert_eq!(res, b"foobar");
    }
}
