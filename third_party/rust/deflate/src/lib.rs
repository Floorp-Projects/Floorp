//! An implementation an encoder using [DEFLATE](http://www.gzip.org/zlib/rfc-deflate.html)
//! compression algorightm in pure rust.
//!
//! This library provides functions to compress data using the DEFLATE algorithm,
//! optionally wrapped using the [zlib](https://tools.ietf.org/html/rfc1950) or
//! [gzip](http://www.gzip.org/zlib/rfc-gzip.html) formats.
//! The current implementation is still a bit lacking speed-wise compared to C-libraries
//! like zlib and miniz.
//!
//! The deflate algorithm is an older compression algorithm that is still widely used today,
//! by e.g html headers, the `.png` inage format, the unix `gzip` program and commonly in `.zip`
//! files. The `zlib` and `gzip` formats are wrappers around DEFLATE-compressed data, containing
//! some extra metadata and a checksum to validate the integrity of the raw data.
//!
//! The deflate algorithm does not perform as well as newer algorhitms used in file formats such as
//! `.7z`, `.rar`, `.xz` and `.bz2`, and is thus not the ideal choice for applications where
//! the `DEFLATE` format (with or without wrappers) is not required.
//!
//! Support for the gzip wrapper (the wrapper that is used in `.gz` files) is disabled by default,
//! but can be enabled with the `gzip` feature.
//!
//! As this library is still in development, the compression output may change slightly
//! between versions.
//!
//!
//! # Examples:
//! ## Simple compression function:
//! ``` rust
//! use deflate::deflate_bytes;
//!
//! let data = b"Some data";
//! let compressed = deflate_bytes(data);
//! # let _ = compressed;
//! ```
//!
//! ## Using a writer:
//! ``` rust
//! use std::io::Write;
//!
//! use deflate::Compression;
//! use deflate::write::ZlibEncoder;
//!
//! let data = b"This is some test data";
//! let mut encoder = ZlibEncoder::new(Vec::new(), Compression::Default);
//! encoder.write_all(data).expect("Write error!");
//! let compressed_data = encoder.finish().expect("Failed to finish compression!");
//! # let _ = compressed_data;
//! ```

#![cfg_attr(all(feature = "benchmarks", test), feature(test))]

#[cfg(all(test, feature = "benchmarks"))]
extern crate test as test_std;

#[cfg(test)]
extern crate flate2;
// #[cfg(test)]
// extern crate inflate;

extern crate adler32;
extern crate byteorder;
#[cfg(feature = "gzip")]
extern crate gzip_header;

mod compression_options;
mod huffman_table;
mod lz77;
mod lzvalue;
mod chained_hash_table;
mod length_encode;
mod output_writer;
mod stored_block;
mod huffman_lengths;
mod zlib;
mod checksum;
mod bit_reverse;
mod bitstream;
mod encoder_state;
mod matching;
mod input_buffer;
mod deflate_state;
mod compress;
mod rle;
mod writer;
#[cfg(test)]
mod test_utils;

use std::io::Write;
use std::io;

use byteorder::BigEndian;
#[cfg(feature = "gzip")]
use gzip_header::GzBuilder;
#[cfg(feature = "gzip")]
use gzip_header::Crc;
#[cfg(feature = "gzip")]
use byteorder::LittleEndian;

use checksum::RollingChecksum;
use deflate_state::DeflateState;

pub use compression_options::{CompressionOptions, SpecialOptions, Compression};
use compress::Flush;
pub use lz77::MatchingType;

use writer::compress_until_done;

/// Encoders implementing a `Write` interface.
pub mod write {
    pub use writer::{DeflateEncoder, ZlibEncoder};
    #[cfg(feature = "gzip")]
    pub use writer::gzip::GzEncoder;
}


fn compress_data_dynamic<RC: RollingChecksum, W: Write>(
    input: &[u8],
    writer: &mut W,
    mut checksum: RC,
    compression_options: CompressionOptions,
) -> io::Result<()> {
    checksum.update_from_slice(input);
    // We use a box here to avoid putting the buffers on the stack
    // It's done here rather than in the structs themselves for now to
    // keep the data close in memory.
    let mut deflate_state = Box::new(DeflateState::new(compression_options, writer));
    compress_until_done(input, &mut deflate_state, Flush::Finish)
}

/// Compress the given slice of bytes with DEFLATE compression.
///
/// Returns a `Vec<u8>` of the compressed data.
///
/// # Examples
///
/// ```
/// use deflate::{deflate_bytes_conf, Compression};
///
/// let data = b"This is some test data";
/// let compressed_data = deflate_bytes_conf(data, Compression::Best);
/// # let _ = compressed_data;
/// ```
pub fn deflate_bytes_conf<O: Into<CompressionOptions>>(input: &[u8], options: O) -> Vec<u8> {
    let mut writer = Vec::with_capacity(input.len() / 3);
    compress_data_dynamic(
        input,
        &mut writer,
        checksum::NoChecksum::new(),
        options.into(),
    ).expect("Write error!");
    writer
}

/// Compress the given slice of bytes with DEFLATE compression using the default compression
/// level.
///
/// Returns a `Vec<u8>` of the compressed data.
///
/// # Examples
///
/// ```
/// use deflate::deflate_bytes;
///
/// let data = b"This is some test data";
/// let compressed_data = deflate_bytes(data);
/// # let _ = compressed_data;
/// ```
pub fn deflate_bytes(input: &[u8]) -> Vec<u8> {
    deflate_bytes_conf(input, Compression::Default)
}

/// Compress the given slice of bytes with DEFLATE compression, including a zlib header and trailer.
///
/// Returns a `Vec<u8>` of the compressed data.
///
/// Zlib dictionaries are not yet suppored.
///
/// # Examples
///
/// ```
/// use deflate::{deflate_bytes_zlib_conf, Compression};
///
/// let data = b"This is some test data";
/// let compressed_data = deflate_bytes_zlib_conf(data, Compression::Best);
/// # let _ = compressed_data;
/// ```
pub fn deflate_bytes_zlib_conf<O: Into<CompressionOptions>>(input: &[u8], options: O) -> Vec<u8> {
    use byteorder::WriteBytesExt;
    let mut writer = Vec::with_capacity(input.len() / 3);
    // Write header
    zlib::write_zlib_header(&mut writer, zlib::CompressionLevel::Default)
        .expect("Write error when writing zlib header!");

    let mut checksum = checksum::Adler32Checksum::new();
    compress_data_dynamic(input, &mut writer, &mut checksum, options.into())
        .expect("Write error when writing compressed data!");

    let hash = checksum.current_hash();

    writer
        .write_u32::<BigEndian>(hash)
        .expect("Write error when writing checksum!");
    writer
}

/// Compress the given slice of bytes with DEFLATE compression, including a zlib header and trailer,
/// using the default compression level.
///
/// Returns a Vec<u8> of the compressed data.
///
/// Zlib dictionaries are not yet suppored.
///
/// # Examples
///
/// ```
/// use deflate::deflate_bytes_zlib;
///
/// let data = b"This is some test data";
/// let compressed_data = deflate_bytes_zlib(data);
/// # let _ = compressed_data;
/// ```
pub fn deflate_bytes_zlib(input: &[u8]) -> Vec<u8> {
    deflate_bytes_zlib_conf(input, Compression::Default)
}

/// Compress the given slice of bytes with DEFLATE compression, including a gzip header and trailer
/// using the given gzip header and compression options.
///
/// Returns a `Vec<u8>` of the compressed data.
///
///
/// # Examples
///
/// ```
/// extern crate gzip_header;
/// extern crate deflate;
///
/// # fn main() {
/// use deflate::{deflate_bytes_gzip_conf, Compression};
/// use gzip_header::GzBuilder;
///
/// let data = b"This is some test data";
/// let compressed_data = deflate_bytes_gzip_conf(data, Compression::Best, GzBuilder::new());
/// # let _ = compressed_data;
/// # }
/// ```
#[cfg(feature = "gzip")]
pub fn deflate_bytes_gzip_conf<O: Into<CompressionOptions>>(
    input: &[u8],
    options: O,
    gzip_header: GzBuilder,
) -> Vec<u8> {
    use byteorder::WriteBytesExt;
    let mut writer = Vec::with_capacity(input.len() / 3);

    // Write header
    writer
        .write_all(&gzip_header.into_header())
        .expect("Write error when writing header!");
    let mut checksum = checksum::NoChecksum::new();
    compress_data_dynamic(input, &mut writer, &mut checksum, options.into())
        .expect("Write error when writing compressed data!");

    let mut crc = Crc::new();
    crc.update(input);

    writer
        .write_u32::<LittleEndian>(crc.sum())
        .expect("Write error when writing checksum!");
    writer
        .write_u32::<LittleEndian>(crc.amt_as_u32())
        .expect("Write error when writing amt!");
    writer
}

/// Compress the given slice of bytes with DEFLATE compression, including a gzip header and trailer,
/// using the default compression level, and a gzip header with default values.
///
/// Returns a `Vec<u8>` of the compressed data.
///
///
/// # Examples
///
/// ```
/// use deflate::deflate_bytes_gzip;
/// let data = b"This is some test data";
/// let compressed_data = deflate_bytes_gzip(data);
/// # let _ = compressed_data;
/// ```
#[cfg(feature = "gzip")]
pub fn deflate_bytes_gzip(input: &[u8]) -> Vec<u8> {
    deflate_bytes_gzip_conf(input, Compression::Default, GzBuilder::new())
}

#[cfg(test)]
mod test {
    use super::*;
    use std::io::Write;

    use test_utils::{get_test_data, decompress_to_end, decompress_zlib};
    #[cfg(feature = "gzip")]
    use test_utils::decompress_gzip;

    type CO = CompressionOptions;

    /// Write data to the writer in chunks of chunk_size.
    fn chunked_write<W: Write>(mut writer: W, data: &[u8], chunk_size: usize) {
        for chunk in data.chunks(chunk_size) {
            writer.write_all(&chunk).unwrap();
        }
    }

    #[test]
    fn dynamic_string_mem() {
        let test_data = String::from("                    GNU GENERAL PUBLIC LICENSE").into_bytes();
        let compressed = deflate_bytes(&test_data);

        assert!(compressed.len() < test_data.len());

        let result = decompress_to_end(&compressed);
        assert_eq!(test_data, result);
    }

    #[test]
    fn dynamic_string_file() {
        let input = get_test_data();
        let compressed = deflate_bytes(&input);

        let result = decompress_to_end(&compressed);
        for (n, (&a, &b)) in input.iter().zip(result.iter()).enumerate() {
            if a != b {
                println!("First difference at {}, input: {}, output: {}", n, a, b);
                println!(
                    "input: {:?}, output: {:?}",
                    &input[n - 3..n + 3],
                    &result[n - 3..n + 3]
                );
                break;
            }
        }
        // Not using assert_eq here deliberately to avoid massive amounts of output spam
        assert!(input == result);
        // Check that we actually managed to compress the input
        assert!(compressed.len() < input.len());
    }

    #[test]
    fn file_rle() {
        let input = get_test_data();
        let compressed = deflate_bytes_conf(&input, CO::rle());

        let result = decompress_to_end(&compressed);
        assert!(input == result);
    }

    #[test]
    fn file_zlib() {
        let test_data = get_test_data();

        let compressed = deflate_bytes_zlib(&test_data);
        // {
        //     use std::fs::File;
        //     use std::io::Write;
        //     let mut f = File::create("out.zlib").unwrap();
        //     f.write_all(&compressed).unwrap();
        // }

        println!("file_zlib compressed(default) length: {}", compressed.len());

        let result = decompress_zlib(&compressed);

        assert!(&test_data == &result);
        assert!(compressed.len() < test_data.len());
    }

    #[test]
    fn zlib_short() {
        let test_data = [10, 10, 10, 10, 10, 55];
        roundtrip_zlib(&test_data, CO::default());
    }

    #[test]
    fn zlib_last_block() {
        let mut test_data = vec![22; 32768];
        test_data.extend(&[5, 2, 55, 11, 12]);
        roundtrip_zlib(&test_data, CO::default());
    }

    #[test]
    fn deflate_short() {
        let test_data = [10, 10, 10, 10, 10, 55];
        let compressed = deflate_bytes(&test_data);

        let result = decompress_to_end(&compressed);
        assert_eq!(&test_data, result.as_slice());
        // If block type and compression is selected correctly, this should only take 5 bytes.
        assert_eq!(compressed.len(), 5);
    }

    #[cfg(feature = "gzip")]
    #[test]
    fn gzip() {
        let data = get_test_data();
        let comment = b"Test";
        let compressed = deflate_bytes_gzip_conf(
            &data,
            Compression::Default,
            GzBuilder::new().comment(&comment[..]),
        );
        let (dec, decompressed) = decompress_gzip(&compressed);
        assert_eq!(dec.header().comment().unwrap(), comment);
        assert!(data == decompressed);
    }

    fn chunk_test(chunk_size: usize, level: CompressionOptions) {
        let mut compressed = Vec::with_capacity(32000);
        let data = get_test_data();
        {
            let mut compressor = write::ZlibEncoder::new(&mut compressed, level);
            chunked_write(&mut compressor, &data, chunk_size);
            compressor.finish().unwrap();
        }
        let compressed2 = deflate_bytes_zlib_conf(&data, level);
        let res = decompress_zlib(&compressed);
        assert!(res == data);
        assert_eq!(compressed.len(), compressed2.len());
        assert!(compressed == compressed2);
    }

    fn writer_chunks_level(level: CompressionOptions) {
        use input_buffer::BUFFER_SIZE;
        let ct = |n| chunk_test(n, level);
        ct(1);
        ct(50);
        ct(400);
        ct(32768);
        ct(BUFFER_SIZE);
        ct(50000);
        ct((32768 * 2) + 258);
    }

    #[ignore]
    #[test]
    /// Test the writer by inputing data in one chunk at the time.
    fn zlib_writer_chunks() {
        writer_chunks_level(CompressionOptions::default());
        writer_chunks_level(CompressionOptions::fast());
        writer_chunks_level(CompressionOptions::rle());
    }

    /// Check that the frequency values don't overflow.
    #[test]
    fn frequency_overflow() {
        let _ = deflate_bytes_conf(
            &vec![5; 100000],
            compression_options::CompressionOptions::default(),
        );
    }

    fn roundtrip_zlib(data: &[u8], level: CompressionOptions) {
        let compressed = deflate_bytes_zlib_conf(data, level);
        let res = decompress_zlib(&compressed);
        if data.len() <= 32 {
            assert_eq!(res, data, "Failed with level: {:?}", level);
        } else {
            assert!(res == data, "Failed with level: {:?}", level);
        }
    }

    fn check_zero(level: CompressionOptions) {
        roundtrip_zlib(&[], level);
    }

    /// Compress with an empty slice.
    #[test]
    fn empty_input() {
        check_zero(CompressionOptions::default());
        check_zero(CompressionOptions::fast());
        check_zero(CompressionOptions::rle());
    }

    #[test]
    fn one_and_two_values() {
        let one = &[1][..];
        roundtrip_zlib(one, CO::rle());
        roundtrip_zlib(one, CO::fast());
        roundtrip_zlib(one, CO::default());
        let two = &[5, 6, 7, 8][..];
        roundtrip_zlib(two, CO::rle());
        roundtrip_zlib(two, CO::fast());
        roundtrip_zlib(two, CO::default());
    }


}
