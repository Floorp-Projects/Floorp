//! A DEFLATE-based stream compression/decompression library
//!
//! This library is meant to supplement/replace the standard distributon's
//! libflate library by providing a streaming encoder/decoder rather than purely
//! an in-memory encoder/decoder.
//!
//! Like with libflate, flate2 is based on [`miniz.c`][1]
//!
//! [1]: https://code.google.com/p/miniz/
//!
//! # Organization
//!
//! This crate consists mainly of two modules, `read` and `write`. Each
//! module contains a number of types used to encode and decode various streams
//! of data. All types in the `write` module work on instances of `Write`,
//! whereas all types in the `read` module work on instances of `Read`.
//!
//! Other various types are provided at the top-level of the crate for
//! management and dealing with encoders/decoders.
//!
//! # Helper traits
//!
//! There are two helper traits provided: `FlateReader` and `FlateWriter`.
//! These provide convenience methods for creating a decoder/encoder out of an
//! already existing stream to chain construction.
//!
//! # Async I/O
//!
//! This crate optionally can support async I/O streams with the Tokio stack via
//! the `tokio` feature of this crate:
//!
//! ```toml
//! flate2 = { version = "0.2", features = ["tokio"] }
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

#![doc(html_root_url = "https://docs.rs/flate2/0.2")]
#![deny(missing_docs)]
#![allow(trivial_numeric_casts)]
#![cfg_attr(test, deny(warnings))]

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

use std::io::prelude::*;
use std::io;

pub use gz::Builder as GzBuilder;
pub use gz::Header as GzHeader;
pub use mem::{Compress, Decompress, DataError, Status, Flush};
pub use crc::{Crc, CrcReader};

mod bufreader;
mod crc;
mod deflate;
mod ffi;
mod gz;
mod zio;
mod mem;
mod zlib;

/// Types which operate over `Read` streams, both encoders and decoders for
/// various formats.
pub mod read {
    pub use deflate::EncoderReader as DeflateEncoder;
    pub use deflate::DecoderReader as DeflateDecoder;
    pub use zlib::EncoderReader as ZlibEncoder;
    pub use zlib::DecoderReader as ZlibDecoder;
    pub use gz::EncoderReader as GzEncoder;
    pub use gz::DecoderReader as GzDecoder;
    pub use gz::MultiDecoderReader as MultiGzDecoder;
}

/// Types which operate over `Write` streams, both encoders and decoders for
/// various formats.
pub mod write {
    pub use deflate::EncoderWriter as DeflateEncoder;
    pub use deflate::DecoderWriter as DeflateDecoder;
    pub use zlib::EncoderWriter as ZlibEncoder;
    pub use zlib::DecoderWriter as ZlibDecoder;
    pub use gz::EncoderWriter as GzEncoder;
}

/// Types which operate over `BufRead` streams, both encoders and decoders for
/// various formats.
pub mod bufread {
    pub use deflate::EncoderReaderBuf as DeflateEncoder;
    pub use deflate::DecoderReaderBuf as DeflateDecoder;
    pub use zlib::EncoderReaderBuf as ZlibEncoder;
    pub use zlib::DecoderReaderBuf as ZlibDecoder;
    pub use gz::EncoderReaderBuf as GzEncoder;
    pub use gz::DecoderReaderBuf as GzDecoder;
    pub use gz::MultiDecoderReaderBuf as MultiGzDecoder;
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
#[derive(Copy, Clone)]
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
    use {FlateReadExt, Compression};

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
