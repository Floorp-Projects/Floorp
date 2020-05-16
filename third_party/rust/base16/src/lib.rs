//! This is a base16 (e.g. hexadecimal) encoding and decoding library with an
//! emphasis on performance. The API is very similar and inspired by the base64
//! crate's API, however it's less complex (base16 is much more simple than
//! base64).
//!
//! # Encoding
//!
//! The config options at the moment are limited to the output case (upper vs
//! lower).
//!
//! | Function                           | Output                       | Allocates               | Requires `alloc` feature |
//! | ---------------------------------- | ---------------------------- | ----------------------- | ------------------------ |
//! | [`encode_upper`], [`encode_lower`] | Returns a new `String`       | Always                  | Yes                      |
//! | [`encode_config`]                  | Returns a new `String`       | Always                  | Yes                      |
//! | [`encode_config_buf`]              | Appends to provided `String` | If buffer needs to grow | Yes                      |
//! | [`encode_config_slice`]            | Writes to provided `&[u8]`   | Never                   | No                       |
//!
//! # Decoding
//!
//! Note that there are no config options (In the future one might be added to
//! restrict the input character set, but it's not clear to me that this is
//! useful).
//!
//! | Function          | Output                        | Allocates               | Requires `alloc` feature |
//! | ----------------- | ----------------------------- | ----------------------- | ------------------------ |
//! | [`decode`]        | Returns a new `Vec<u8>`       | Always                  | Yes                      |
//! | [`decode_slice`]  | Writes to provided `&[u8]`    | Never                   | No                       |
//! | [`decode_buf`]    | Appends to provided `Vec<u8>` | If buffer needs to grow | Yes                      |
//!
//! # Features
//!
//! This crate has two features, both are enabled by default and exist to allow
//! users in `no_std` environments to disable various portions of .
//!
//! - The `"alloc"` feature, which is on by default, adds a number of helpful
//!   functions that require use of the [`alloc`][alloc_crate] crate, but not the
//!   rest of `std`.
//!     - This is `no_std` compatible.
//!     - Each function should list whether or not it requires this feature
//!       under the `Availability` of its documentation.
//!
//! - The `"std"` feature, which is on by default, enables the `"alloc"`
//!   feature, and additionally makes [`DecodeError`] implement the
//!   `std::error::Error` trait.
//!
//!     - Frustratingly, this trait is in `std` (and not in `core` or `alloc`),
//!       but not implementing it would be quite annoying for some users, so
//!       it's kept, even though it's what prevents us from being `no_std`
//!       compatible in all configurations.
//!
//! [alloc_crate]: https://doc.rust-lang.org/alloc/index.html

#![cfg_attr(not(feature = "std"), no_std)]
#![deny(missing_docs)]

#[cfg(feature = "alloc")]
extern crate alloc;

#[cfg(feature = "alloc")]
use alloc::{vec::Vec, string::String};

/// Configuration options for encoding. Just specifies whether or not output
/// should be uppercase or lowercase.
#[derive(Debug, Clone, Copy, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum EncConfig {
    /// Encode using lower case characters for hex values >= 10
    EncodeLower,
    /// Encode using upper case characters for hex values >= 10
    EncodeUpper,
}

pub use EncConfig::*;

#[inline]
fn encoded_size(source_len: usize) -> usize {
    const USIZE_TOP_BIT: usize = 1usize << (core::mem::size_of::<usize>() * 8 - 1);
    if (source_len & USIZE_TOP_BIT) != 0 {
        usize_overflow(source_len)
    }
    source_len << 1
}

#[inline]
fn encode_slice_raw(src: &[u8], cfg: EncConfig, dst: &mut [u8]) {
    let lut = if cfg == EncodeLower { HEX_LOWER } else { HEX_UPPER };
    debug_assert!(dst.len() == encoded_size(src.len()));
    dst.chunks_exact_mut(2).zip(src.iter().copied()).for_each(|(d, sb)| {
        d[0] = lut[(sb >> 4) as usize];
        d[1] = lut[(sb & 0xf) as usize];
    })
}

#[cfg(feature = "alloc")]
#[inline]
fn encode_to_string(bytes: &[u8], cfg: EncConfig) -> String {
    let size = encoded_size(bytes.len());
    let mut buf: Vec<u8> = Vec::with_capacity(size);
    unsafe { buf.set_len(size); }
    encode_slice_raw(bytes, cfg, &mut buf);
    debug_assert!(core::str::from_utf8(&buf).is_ok());
    unsafe { String::from_utf8_unchecked(buf) }
}

#[cfg(feature = "alloc")]
#[inline]
unsafe fn grow_vec_uninitialized(v: &mut Vec<u8>, grow_by: usize) -> usize {
    v.reserve(grow_by);
    let initial_len = v.len();
    let new_len = initial_len + grow_by;
    debug_assert!(new_len <= v.capacity());
    v.set_len(new_len);
    initial_len
}

/// Encode bytes as base16, using lower case characters for nibbles between 10
/// and 15 (`a` through `f`).
///
/// This is equivalent to `base16::encode_config(bytes, base16::EncodeUpper)`.
///
/// # Example
///
/// ```
/// assert_eq!(base16::encode_lower(b"Hello World"), "48656c6c6f20576f726c64");
/// assert_eq!(base16::encode_lower(&[0xff, 0xcc, 0xaa]), "ffccaa");
/// ```
///
/// # Availability
///
/// This function is only available when the `alloc` feature is enabled, as it
/// needs to produce a String.
#[cfg(feature = "alloc")]
#[inline]
pub fn encode_lower<T: ?Sized + AsRef<[u8]>>(input: &T) -> String {
    encode_to_string(input.as_ref(), EncodeLower)
}

/// Encode bytes as base16, using upper case characters for nibbles between
/// 10 and 15 (`A` through `F`).
///
/// This is equivalent to `base16::encode_config(bytes, base16::EncodeUpper)`.
///
/// # Example
///
/// ```
/// assert_eq!(base16::encode_upper(b"Hello World"), "48656C6C6F20576F726C64");
/// assert_eq!(base16::encode_upper(&[0xff, 0xcc, 0xaa]), "FFCCAA");
/// ```
///
/// # Availability
///
/// This function is only available when the `alloc` feature is enabled, as it
/// needs to produce a `String`.
#[cfg(feature = "alloc")]
#[inline]
pub fn encode_upper<T: ?Sized + AsRef<[u8]>>(input: &T) -> String {
    encode_to_string(input.as_ref(), EncodeUpper)
}


/// Encode `input` into a string using the listed config. The resulting string
/// contains `input.len() * 2` bytes.
///
/// # Example
///
/// ```
/// let data = [1, 2, 3, 0xaa, 0xbb, 0xcc];
/// assert_eq!(base16::encode_config(&data, base16::EncodeLower), "010203aabbcc");
/// assert_eq!(base16::encode_config(&data, base16::EncodeUpper), "010203AABBCC");
/// ```
///
/// # Availability
///
/// This function is only available when the `alloc` feature is enabled, as it
/// needs to produce a `String`.
#[cfg(feature = "alloc")]
#[inline]
pub fn encode_config<T: ?Sized + AsRef<[u8]>>(input: &T, cfg: EncConfig) -> String {
    encode_to_string(input.as_ref(), cfg)
}

/// Encode `input` into the end of the provided buffer. Returns the number of
/// bytes that were written.
///
/// Only allocates when `dst.size() + (input.len() * 2) >= dst.capacity()`.
///
/// # Example
///
/// ```
/// let messages = &["Taako, ", "Merle, ", "Magnus"];
/// let mut buffer = String::new();
/// for msg in messages {
///     let bytes_written = base16::encode_config_buf(msg.as_bytes(),
///                                                   base16::EncodeUpper,
///                                                   &mut buffer);
///     assert_eq!(bytes_written, msg.len() * 2);
/// }
/// assert_eq!(buffer, "5461616B6F2C204D65726C652C204D61676E7573");
/// ```
/// # Availability
///
/// This function is only available when the `alloc` feature is enabled, as it
/// needs write to a `String`.
#[cfg(feature = "alloc")]
#[inline]
pub fn encode_config_buf<T: ?Sized + AsRef<[u8]>>(input: &T,
                                                  cfg: EncConfig,
                                                  dst: &mut String) -> usize {
    let src = input.as_ref();
    let bytes_to_write = encoded_size(src.len());
    // Swap the string out while we work on it, so that if we panic, we don't
    // leave behind garbage (we do clear the string if we panic, but that's
    // better than UB)
    let mut buf = core::mem::replace(dst, String::new()).into_bytes();
    let cur_size = unsafe { grow_vec_uninitialized(&mut buf, bytes_to_write) };

    encode_slice_raw(src, cfg, &mut buf[cur_size..]);

    debug_assert!(core::str::from_utf8(&buf).is_ok());
    // Put `buf` back into `dst`.
    *dst = unsafe { String::from_utf8_unchecked(buf) };

    bytes_to_write
}

/// Write bytes as base16 into the provided output buffer. Never allocates.
///
/// This is useful if you wish to avoid allocation entirely (e.g. your
/// destination buffer is on the stack), or control it precisely.
///
/// # Panics
///
/// Panics if the desination buffer is insufficiently large.
///
/// # Example
///
/// ```
/// # extern crate core as std;
/// // Writing to a statically sized buffer on the stack.
/// let message = b"Wu-Tang Killa Bees";
/// let mut buffer = [0u8; 1024];
///
/// let wrote = base16::encode_config_slice(message,
///                                         base16::EncodeLower,
///                                         &mut buffer);
///
/// assert_eq!(message.len() * 2, wrote);
/// assert_eq!(std::str::from_utf8(&buffer[..wrote]).unwrap(),
///            "57752d54616e67204b696c6c612042656573");
///
/// // Appending to an existing buffer is possible too.
/// let wrote2 = base16::encode_config_slice(b": The Swarm",
///                                          base16::EncodeLower,
///                                          &mut buffer[wrote..]);
/// let write_end = wrote + wrote2;
/// assert_eq!(std::str::from_utf8(&buffer[..write_end]).unwrap(),
///            "57752d54616e67204b696c6c6120426565733a2054686520537761726d");
/// ```
/// # Availability
///
/// This function is available whether or not the `alloc` feature is enabled.
#[inline]
pub fn encode_config_slice<T: ?Sized + AsRef<[u8]>>(input: &T,
                                                    cfg: EncConfig,
                                                    dst: &mut [u8]) -> usize {
    let src = input.as_ref();
    let need_size = encoded_size(src.len());
    if dst.len() < need_size {
        dest_too_small_enc(dst.len(), need_size);
    }
    encode_slice_raw(src, cfg, &mut dst[..need_size]);
    need_size
}

/// Encode a single character as hex, returning a tuple containing the two
/// encoded bytes in big-endian order -- the order the characters would be in
/// when written out (e.g. the top nibble is the first item in the tuple)
///
/// # Example
/// ```
/// assert_eq!(base16::encode_byte(0xff, base16::EncodeLower), [b'f', b'f']);
/// assert_eq!(base16::encode_byte(0xa0, base16::EncodeUpper), [b'A', b'0']);
/// assert_eq!(base16::encode_byte(3, base16::EncodeUpper), [b'0', b'3']);
/// ```
/// # Availability
///
/// This function is available whether or not the `alloc` feature is enabled.
#[inline]
pub fn encode_byte(byte: u8, cfg: EncConfig) -> [u8; 2] {
    let lut = if cfg == EncodeLower { HEX_LOWER } else { HEX_UPPER };
    let lo = lut[(byte & 15) as usize];
    let hi = lut[(byte >> 4) as usize];
    [hi, lo]
}

/// Convenience wrapper for `base16::encode_byte(byte, base16::EncodeLower)`
///
/// See also `base16::encode_byte_u`.
///
/// # Example
/// ```
/// assert_eq!(base16::encode_byte_l(0xff), [b'f', b'f']);
/// assert_eq!(base16::encode_byte_l(30), [b'1', b'e']);
/// assert_eq!(base16::encode_byte_l(0x2d), [b'2', b'd']);
/// ```
/// # Availability
///
/// This function is available whether or not the `alloc` feature is enabled.
#[inline]
pub fn encode_byte_l(byte: u8) -> [u8; 2] {
    encode_byte(byte, EncodeLower)
}

/// Convenience wrapper for `base16::encode_byte(byte, base16::EncodeUpper)`
///
/// See also `base16::encode_byte_l`.
///
/// # Example
/// ```
/// assert_eq!(base16::encode_byte_u(0xff), [b'F', b'F']);
/// assert_eq!(base16::encode_byte_u(30), [b'1', b'E']);
/// assert_eq!(base16::encode_byte_u(0x2d), [b'2', b'D']);
/// ```
/// # Availability
///
/// This function is available whether or not the `alloc` feature is enabled.
#[inline]
pub fn encode_byte_u(byte: u8) -> [u8; 2] {
    encode_byte(byte, EncodeUpper)
}

/// Represents a problem with the data we want to decode.
///
/// This implements `std::error::Error` and `Display` if the `std`
/// feature is enabled, but only `Display` if it is not.
#[derive(Debug, PartialEq, Eq, Clone)]
pub enum DecodeError {
    /// An invalid byte was found in the input (bytes must be `[0-9a-fA-F]`)
    InvalidByte {
        /// The index at which the problematic byte was found.
        index: usize,
        /// The byte that we cannot decode.
        byte: u8
    },
    /// The length of the input not a multiple of two
    InvalidLength {
        /// The input length.
        length: usize
    },
}

#[cold]
fn invalid_length(length: usize) -> DecodeError {
    DecodeError::InvalidLength { length }
}

#[cold]
fn invalid_byte(index: usize, src: &[u8]) -> DecodeError {
    DecodeError::InvalidByte { index, byte: src[index] }
}

impl core::fmt::Display for DecodeError {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        match *self {
            DecodeError::InvalidByte { index, byte } => {
                write!(f, "Invalid byte `b{:?}`, at index {}.",
                       byte as char, index)
            }
            DecodeError::InvalidLength { length } =>
                write!(f, "Base16 data cannot have length {} (must be even)",
                       length),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for DecodeError {
    fn description(&self) -> &str {
        match *self {
            DecodeError::InvalidByte { .. } => "Illegal byte in base16 data",
            DecodeError::InvalidLength { .. } => "Illegal length for base16 data",
        }
    }

    fn cause(&self) -> Option<&dyn std::error::Error> {
        None
    }
}

#[inline]
fn decode_slice_raw(src: &[u8], dst: &mut[u8]) -> Result<(), usize> {
    // checked in caller.
    debug_assert!(src.len() / 2 == dst.len());
    debug_assert!((src.len() & 1) == 0);
    src.chunks_exact(2).enumerate().zip(dst.iter_mut()).try_for_each(|((si, s), d)| {
        let r0 = DECODE_LUT[s[0] as usize];
        let r1 = DECODE_LUT[s[1] as usize];
        if (r0 | r1) >= 0 {
            *d = ((r0 << 4) | r1) as u8;
            Ok(())
        } else {
            Err(si * 2)
        }
    }).map_err(|bad_idx| raw_decode_err(bad_idx, src))
}

#[cold]
#[inline(never)]
fn raw_decode_err(idx: usize, src: &[u8]) -> usize {
    let b0 = src[idx];
    if decode_byte(b0).is_none() {
        idx
    } else {
        idx + 1
    }
}

/// Decode bytes from base16, and return a new `Vec<u8>` containing the results.
///
/// # Example
///
/// ```
/// assert_eq!(&base16::decode("48656c6c6f20576f726c64".as_bytes()).unwrap(),
///            b"Hello World");
/// assert_eq!(&base16::decode(b"deadBEEF").unwrap(),
///            &[0xde, 0xad, 0xbe, 0xef]);
/// // Error cases:
/// assert_eq!(base16::decode(b"Not Hexadecimal!"),
///            Err(base16::DecodeError::InvalidByte { byte: b'N', index: 0 }));
/// assert_eq!(base16::decode(b"a"),
///            Err(base16::DecodeError::InvalidLength { length: 1 }));
/// ```
/// # Availability
///
/// This function is only available when the `alloc` feature is enabled, as it
/// needs to produce a Vec.
#[cfg(feature = "alloc")]
#[inline]
pub fn decode<T: ?Sized + AsRef<[u8]>>(input: &T) -> Result<Vec<u8>, DecodeError> {
    let src = input.as_ref();
    if (src.len() & 1) != 0 {
        return Err(invalid_length(src.len()));
    }
    let need_size = src.len() >> 1;
    let mut dst = Vec::with_capacity(need_size);
    unsafe { dst.set_len(need_size); }
    match decode_slice_raw(src, &mut dst) {
        Ok(()) => Ok(dst),
        Err(index) => Err(invalid_byte(index, src))
    }
}


/// Decode bytes from base16, and appends into the provided buffer. Only
/// allocates if the buffer could not fit the data. Returns the number of bytes
/// written.
///
/// In the case of an error, the buffer should remain the same size.
///
/// # Example
///
/// ```
/// # extern crate core as std;
/// # extern crate alloc;
/// # use alloc::vec::Vec;
/// let mut result = Vec::new();
/// assert_eq!(base16::decode_buf(b"4d61646f6b61", &mut result).unwrap(), 6);
/// assert_eq!(base16::decode_buf(b"486F6D757261", &mut result).unwrap(), 6);
/// assert_eq!(std::str::from_utf8(&result).unwrap(), "MadokaHomura");
/// ```
/// # Availability
///
/// This function is only available when the `alloc` feature is enabled, as it
/// needs to write to a Vec.
#[cfg(feature = "alloc")]
#[inline]
pub fn decode_buf<T: ?Sized + AsRef<[u8]>>(input: &T, v: &mut Vec<u8>) -> Result<usize, DecodeError> {
    let src = input.as_ref();
    if (src.len() & 1) != 0 {
        return Err(invalid_length(src.len()));
    }
    // Swap the vec out while we work on it, so that if we panic, we don't leave
    // behind garbage (this will end up cleared if we panic, but that's better
    // than UB)
    let mut work = core::mem::replace(v, Vec::default());
    let need_size = src.len() >> 1;
    let current_size = unsafe {
        grow_vec_uninitialized(&mut work, need_size)
    };
    match decode_slice_raw(src, &mut work[current_size..]) {
        Ok(()) => {
            // Swap back
            core::mem::swap(v, &mut work);
            Ok(need_size)
        }
        Err(index) => {
            work.truncate(current_size);
            // Swap back
            core::mem::swap(v, &mut work);
            Err(invalid_byte(index, src))
        }
    }
}

/// Decode bytes from base16, and write into the provided buffer. Never
/// allocates.
///
/// In the case of a decoder error, the output is not specified, but in practice
/// will remain untouched for an `InvalidLength` error, and will contain the
/// decoded input up to the problem byte in the case of an InvalidByte error.
///
/// # Panics
///
/// Panics if the provided buffer is not large enough for the input.
///
/// # Example
/// ```
/// let msg = "476f6f642072757374206c6962726172696573207573652073696c6c79206578616d706c6573";
/// let mut buf = [0u8; 1024];
/// assert_eq!(base16::decode_slice(&msg[..], &mut buf).unwrap(), 38);
/// assert_eq!(&buf[..38], b"Good rust libraries use silly examples".as_ref());
///
/// let msg2 = b"2E20416C736F2C20616E696D65207265666572656e636573";
/// assert_eq!(base16::decode_slice(&msg2[..], &mut buf[38..]).unwrap(), 24);
/// assert_eq!(&buf[38..62], b". Also, anime references".as_ref());
/// ```
/// # Availability
///
/// This function is available whether or not the `alloc` feature is enabled.
#[inline]
pub fn decode_slice<T: ?Sized + AsRef<[u8]>>(input: &T, out: &mut [u8]) -> Result<usize, DecodeError> {
    let src = input.as_ref();
    if (src.len() & 1) != 0 {
        return Err(invalid_length(src.len()));
    }
    let need_size = src.len() >> 1;
    if out.len() < need_size {
        dest_too_small_dec(out.len(), need_size);
    }
    match decode_slice_raw(src, &mut out[..need_size]) {
        Ok(()) => Ok(need_size),
        Err(index) => Err(invalid_byte(index, src))
    }
}

/// Decode a single character as hex.
///
/// Returns `None` for values outside the ASCII hex range.
///
/// # Example
/// ```
/// assert_eq!(base16::decode_byte(b'a'), Some(10));
/// assert_eq!(base16::decode_byte(b'B'), Some(11));
/// assert_eq!(base16::decode_byte(b'0'), Some(0));
/// assert_eq!(base16::decode_byte(b'q'), None);
/// assert_eq!(base16::decode_byte(b'x'), None);
/// ```
/// # Availability
///
/// This function is available whether or not the `alloc` feature is enabled.
#[inline]
pub fn decode_byte(c: u8) -> Option<u8> {
    if c.wrapping_sub(b'0') <= 9 {
        Some(c.wrapping_sub(b'0'))
    } else if c.wrapping_sub(b'a') < 6 {
        Some(c.wrapping_sub(b'a') + 10)
    } else if c.wrapping_sub(b'A') < 6 {
        Some(c.wrapping_sub(b'A') + 10)
    } else {
        None
    }
}
static HEX_UPPER: [u8; 16] = *b"0123456789ABCDEF";
static HEX_LOWER: [u8; 16] = *b"0123456789abcdef";
static DECODE_LUT: [i8; 256] = [
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,
        6,  7,  8,  9, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1
];
// Outlined assertions.
#[inline(never)]
#[cold]
fn usize_overflow(len: usize) -> ! {
    panic!("usize overflow when computing size of destination: {}", len);
}

#[cold]
#[inline(never)]
fn dest_too_small_enc(dst_len: usize, need_size: usize) -> ! {
    panic!("Destination is not large enough to encode input: {} < {}", dst_len, need_size);
}

#[cold]
#[inline(never)]
fn dest_too_small_dec(dst_len: usize, need_size: usize) -> ! {
    panic!("Destination buffer not large enough for decoded input {} < {}", dst_len, need_size);
}

// encoded_size smoke tests
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    #[should_panic]
    #[cfg(pointer_size )]
    fn test_encoded_size_panic_top_bit() {
        #[cfg(target_pointer_width = "64")]
        let usz = 0x8000_0000_0000_0000usize;
        #[cfg(target_pointer_width = "32")]
        let usz = 0x8000_0000usize;
        let _ = encoded_size(usz);
    }

    #[test]
    #[should_panic]
    fn test_encoded_size_panic_max() {
        let _ = encoded_size(usize::max_value());
    }

    #[test]
    fn test_encoded_size_allows_almost_max() {
        #[cfg(target_pointer_width = "64")]
        let usz = 0x7fff_ffff_ffff_ffffusize;
        #[cfg(target_pointer_width = "32")]
        let usz = 0x7fff_ffffusize;
        assert_eq!(encoded_size(usz), usz * 2);
    }
}
