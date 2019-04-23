//! This module contains the various options to tweak how compression is performed.
//!
//! Note that due to the nature of the `DEFLATE` format, lower compression levels
//! may for some data compress better than higher compression levels.
//!
//! For applications where a maximum level of compression (irrespective of compression
//! speed) is required, consider using the [`Zopfli`](https://crates.io/crates/zopfli)
//! compressor, which uses a specialised (but slow) algorithm to figure out the maximum
//! of compression for the provided data.
//!
use lz77::MatchingType;
use std::convert::From;

pub const HIGH_MAX_HASH_CHECKS: u16 = 1768;
pub const HIGH_LAZY_IF_LESS_THAN: u16 = 128;
/// The maximum number of hash checks that make sense as this is the length
/// of the hash chain.
pub const MAX_HASH_CHECKS: u16 = 32 * 1024;
pub const DEFAULT_MAX_HASH_CHECKS: u16 = 128;
pub const DEFAULT_LAZY_IF_LESS_THAN: u16 = 32;

/// An enum describing the level of compression to be used by the encoder
///
/// Higher compression ratios will take longer to encode.
///
/// This is a simplified interface to specify a compression level.
///
/// [See also `CompressionOptions`](./struct.CompressionOptions.html) which provides for
/// tweaking the settings more finely.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum Compression {
    /// Fast minimal compression (`CompressionOptions::fast()`).
    Fast,
    /// Default level (`CompressionOptions::default()`).
    Default,
    /// Higher compression level (`CompressionOptions::high()`).
    ///
    /// Best in this context isn't actually the highest possible level
    /// the encoder can do, but is meant to emulate the `Best` setting in the `Flate2`
    /// library.
    Best,
}

impl Default for Compression {
    fn default() -> Compression {
        Compression::Default
    }
}

/// Enum allowing some special options (not implemented yet)!
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum SpecialOptions {
    /// Compress normally.
    Normal,
    /// Force fixed huffman tables. (Unimplemented!).
    _ForceFixed,
    /// Force stored (uncompressed) blocks only. (Unimplemented!).
    _ForceStored,
}

impl Default for SpecialOptions {
    fn default() -> SpecialOptions {
        SpecialOptions::Normal
    }
}

pub const DEFAULT_OPTIONS: CompressionOptions = CompressionOptions {
    max_hash_checks: DEFAULT_MAX_HASH_CHECKS,
    lazy_if_less_than: DEFAULT_LAZY_IF_LESS_THAN,
    matching_type: MatchingType::Lazy,
    special: SpecialOptions::Normal,
};

/// A struct describing the options for a compressor or compression function.
///
/// These values are not stable and still subject to change!
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct CompressionOptions {
    /// The maximum number of checks to make in the hash table for matches.
    ///
    /// Higher numbers mean slower, but better compression. Very high (say `>1024`) values
    /// will impact compression speed a lot. The maximum match length is 2^15, so values higher than
    /// this won't make any difference, and will be truncated to 2^15 by the compression
    /// function/writer.
    ///
    /// Default value: `128`
    pub max_hash_checks: u16,
    // pub _window_size: u16,
    /// Only lazy match if we have a length less than this value.
    ///
    /// Higher values degrade compression slightly, but improve compression speed.
    ///
    /// * `0`: Never lazy match. (Same effect as setting MatchingType to greedy, but may be slower).
    /// * `1...257`: Only check for a better match if the first match was shorter than this value.
    /// * `258`: Always lazy match.
    ///
    /// As the maximum length of a match is `258`, values higher than this will have
    /// no further effect.
    ///
    /// * Default value: `32`
    pub lazy_if_less_than: u16,

    // pub _decent_match: u16,
    /// Whether to use lazy or greedy matching.
    ///
    /// Lazy matching will provide better compression, at the expense of compression speed.
    ///
    /// As a special case, if max_hash_checks is set to 0, and matching_type is set to lazy,
    /// compression using only run-length encoding (i.e maximum match distance of 1) is performed.
    /// (This may be changed in the future but is defined like this at the moment to avoid API
    /// breakage.
    ///
    /// [See `MatchingType`](./enum.MatchingType.html)
    ///
    /// * Default value: `MatchingType::Lazy`
    pub matching_type: MatchingType,
    /// Force fixed/stored blocks (Not implemented yet).
    /// * Default value: `SpecialOptions::Normal`
    pub special: SpecialOptions,
}

// Some standard profiles for the compression options.
// Ord should be implemented at some point, but won't yet until the struct is stabilised.
impl CompressionOptions {
    /// Returns compression settings rouhgly corresponding to the `HIGH(9)` setting in miniz.
    pub fn high() -> CompressionOptions {
        CompressionOptions {
            max_hash_checks: HIGH_MAX_HASH_CHECKS,
            lazy_if_less_than: HIGH_LAZY_IF_LESS_THAN,
            matching_type: MatchingType::Lazy,
            special: SpecialOptions::Normal,
        }
    }

    /// Returns  a fast set of compression settings
    ///
    /// Ideally this should roughly correspond to the `FAST(1)` setting in miniz.
    /// However, that setting makes miniz use a somewhat different algorhithm,
    /// so currently hte fast level in this library is slower and better compressing
    /// than the corresponding level in miniz.
    pub fn fast() -> CompressionOptions {
        CompressionOptions {
            max_hash_checks: 1,
            lazy_if_less_than: 0,
            matching_type: MatchingType::Greedy,
            special: SpecialOptions::Normal,
        }
    }

    /// Returns a set of compression settings that makes the compressor only compress using
    /// huffman coding. (Ignoring any length/distance matching)
    ///
    /// This will normally have the worst compression ratio (besides only using uncompressed data),
    /// but may be the fastest method in some cases.
    pub fn huffman_only() -> CompressionOptions {
        CompressionOptions {
            max_hash_checks: 0,
            lazy_if_less_than: 0,
            matching_type: MatchingType::Greedy,
            special: SpecialOptions::Normal,
        }
    }

    /// Returns a set of compression settings that makes the compressor compress only using
    /// run-length encoding (i.e only looking for matches one byte back).
    ///
    /// This is very fast, but tends to compress worse than looking for more matches using hash
    /// chains that the slower settings do.
    /// Works best on data that has runs of equivialent bytes, like binary or simple images,
    /// less good for text.
    pub fn rle() -> CompressionOptions {
        CompressionOptions {
            max_hash_checks: 0,
            lazy_if_less_than: 0,
            matching_type: MatchingType::Lazy,
            special: SpecialOptions::Normal,
        }
    }
}

impl Default for CompressionOptions {
    /// Returns the options describing the default compression level.
    fn default() -> CompressionOptions {
        DEFAULT_OPTIONS
    }
}

impl From<Compression> for CompressionOptions {
    fn from(compression: Compression) -> CompressionOptions {
        match compression {
            Compression::Fast => CompressionOptions::fast(),
            Compression::Default => CompressionOptions::default(),
            Compression::Best => CompressionOptions::high(),
        }
    }
}
