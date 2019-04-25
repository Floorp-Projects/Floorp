//! This module contains functionality for generating a [zlib](https://tools.ietf.org/html/rfc1950)
//! header.
//!
//! The Zlib header contains some metadata (a window size and a compression level), and optionally
//! a block of data serving as an extra dictionary for the compressor/decompressor.
//! The dictionary is not implemented in this library.
//! The data in the header aside from the dictionary doesn't actually have any effect on the
//! decompressed data, it only offers some hints for the decompressor on how the data was
//! compressed.

use std::io::{Write, Result};

// CM = 8 means to use the DEFLATE compression method.
const DEFAULT_CM: u8 = 8;
// CINFO = 7 Indicates a 32k window size.
const DEFAULT_CINFO: u8 = 7 << 4;
const DEFAULT_CMF: u8 = DEFAULT_CM | DEFAULT_CINFO;

// No dict by default.
#[cfg(test)]
const DEFAULT_FDICT: u8 = 0;
// FLEVEL = 0 means fastest compression algorithm.
const _DEFAULT_FLEVEL: u8 = 0 << 7;

// The 16-bit value consisting of CMF and FLG must be divisible by this to be valid.
const FCHECK_DIVISOR: u8 = 31;

#[allow(dead_code)]
#[repr(u8)]
pub enum CompressionLevel {
    Fastest = 0 << 6,
    Fast = 1 << 6,
    Default = 2 << 6,
    Maximum = 3 << 6,
}

/// Generate FCHECK from CMF and FLG (without FCKECH )so that they are correct according to the
/// specification, i.e (CMF*256 + FCHK) % 31 = 0.
/// Returns flg with the FCHKECK bits added (any existing FCHECK bits are ignored).
fn add_fcheck(cmf: u8, flg: u8) -> u8 {
    let rem = ((usize::from(cmf) * 256) + usize::from(flg)) % usize::from(FCHECK_DIVISOR);

    // Clear existing FCHECK if any
    let flg = flg & 0b11100000;

    // Casting is safe as rem can't overflow since it is a value mod 31
    // We can simply add the value to flg as (31 - rem) will never be above 2^5
    flg + (FCHECK_DIVISOR - rem as u8)
}

/// Write a zlib header with an empty dictionary to the writer using the specified
/// compression level preset.
pub fn write_zlib_header<W: Write>(writer: &mut W, level: CompressionLevel) -> Result<()> {
    writer.write_all(&get_zlib_header(level))
}

/// Get the zlib header for the `CompressionLevel` level using the default window size and no
/// dictionary.
pub fn get_zlib_header(level: CompressionLevel) -> [u8; 2] {
    let cmf = DEFAULT_CMF;
    [cmf, add_fcheck(cmf, level as u8)]
}

#[cfg(test)]
mod test {
    use super::DEFAULT_CMF;
    use super::*;

    #[test]
    fn test_gen_fcheck() {
        let cmf = DEFAULT_CMF;
        let flg = super::add_fcheck(
            DEFAULT_CMF,
            CompressionLevel::Default as u8 | super::DEFAULT_FDICT,
        );
        assert_eq!(((usize::from(cmf) * 256) + usize::from(flg)) % 31, 0);
    }

    #[test]
    fn test_header() {
        let header = get_zlib_header(CompressionLevel::Fastest);
        assert_eq!(
            ((usize::from(header[0]) * 256) + usize::from(header[1])) % 31,
            0
        );
    }
}
