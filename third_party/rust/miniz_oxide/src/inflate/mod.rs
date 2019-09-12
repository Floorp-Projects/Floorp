//! This module contains functionality for decompression.

use std::io::Cursor;
use std::usize;

pub mod core;
mod output_buffer;
pub mod stream;
use self::core::*;

const TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS: i32 = -4;
const TINFL_STATUS_BAD_PARAM: i32 = -3;
const TINFL_STATUS_ADLER32_MISMATCH: i32 = -2;
const TINFL_STATUS_FAILED: i32 = -1;
const TINFL_STATUS_DONE: i32 = 0;
const TINFL_STATUS_NEEDS_MORE_INPUT: i32 = 1;
const TINFL_STATUS_HAS_MORE_OUTPUT: i32 = 2;

/// Return status codes.
#[repr(i8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum TINFLStatus {
    /// More input data was expected, but the caller indicated that there was more data, so the
    /// input stream is likely truncated.
    FailedCannotMakeProgress = TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS as i8,
    /// One or more of the input parameters were invalid.
    BadParam = TINFL_STATUS_BAD_PARAM as i8,
    /// The decompression went fine, but the adler32 checksum did not match the one
    /// provided in the header.
    Adler32Mismatch = TINFL_STATUS_ADLER32_MISMATCH as i8,
    /// Failed to decompress due to invalid data.
    Failed = TINFL_STATUS_FAILED as i8,
    /// Finished decomression without issues.
    Done = TINFL_STATUS_DONE as i8,
    /// The decompressor needs more input data to continue decompressing.
    NeedsMoreInput = TINFL_STATUS_NEEDS_MORE_INPUT as i8,
    /// There is still pending data that didn't fit in the output buffer.
    HasMoreOutput = TINFL_STATUS_HAS_MORE_OUTPUT as i8,
}

impl TINFLStatus {
    pub fn from_i32(value: i32) -> Option<TINFLStatus> {
        use self::TINFLStatus::*;
        match value {
            TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS => Some(FailedCannotMakeProgress),
            TINFL_STATUS_BAD_PARAM => Some(BadParam),
            TINFL_STATUS_ADLER32_MISMATCH => Some(Adler32Mismatch),
            TINFL_STATUS_FAILED => Some(Failed),
            TINFL_STATUS_DONE => Some(Done),
            TINFL_STATUS_NEEDS_MORE_INPUT => Some(NeedsMoreInput),
            TINFL_STATUS_HAS_MORE_OUTPUT => Some(HasMoreOutput),
            _ => None,
        }
    }
}

/// Decompress the deflate-encoded data in `input` to a vector.
///
/// Returns a status and an integer representing where the decompressor failed on failure.
#[inline]
pub fn decompress_to_vec(input: &[u8]) -> Result<Vec<u8>, TINFLStatus> {
    decompress_to_vec_inner(input, 0)
}

/// Decompress the deflate-encoded data (with a zlib wrapper) in `input` to a vector.
///
/// Returns a status and an integer representing where the decompressor failed on failure.
#[inline]
pub fn decompress_to_vec_zlib(input: &[u8]) -> Result<Vec<u8>, TINFLStatus> {
    decompress_to_vec_inner(input, inflate_flags::TINFL_FLAG_PARSE_ZLIB_HEADER)
}

fn decompress_to_vec_inner(input: &[u8], flags: u32) -> Result<Vec<u8>, TINFLStatus> {
    let flags = flags | inflate_flags::TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
    let mut ret: Vec<u8> = vec![0; input.len() * 2];

    let mut decomp = Box::<DecompressorOxide>::default();

    let mut in_pos = 0;
    let mut out_pos = 0;
    loop {
        let (status, in_consumed, out_consumed) = {
            // Wrap the whole output slice so we know we have enough of the
            // decompressed data for matches.
            let mut c = Cursor::new(ret.as_mut_slice());
            c.set_position(out_pos as u64);
            decompress(&mut decomp, &input[in_pos..], &mut c, flags)
        };
        in_pos += in_consumed;
        out_pos += out_consumed;

        match status {
            TINFLStatus::Done => {
                ret.truncate(out_pos);
                return Ok(ret);
            }

            TINFLStatus::HasMoreOutput => {
                // We need more space so resize the buffer.
                ret.resize(ret.len() + out_pos, 0);
            }

            _ => return Err(status),
        }
    }
}

#[cfg(test)]
mod test {
    use super::decompress_to_vec_zlib;

    #[test]
    fn decompress_vec() {
        let encoded = [
            120, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201, 76, 82, 4, 0, 27, 101, 4, 19,
        ];
        let res = decompress_to_vec_zlib(&encoded[..]).unwrap();
        assert_eq!(res.as_slice(), &b"Hello, zlib!"[..]);
    }
}
