extern crate byteorder;

use std::{fmt, error, str};

use byteorder::{BigEndian, ByteOrder};

mod tables;

mod line_wrap;
use line_wrap::{line_wrap_parameters, line_wrap};

/// Available encoding character sets
#[derive(Clone, Copy, Debug)]
pub enum CharacterSet {
    /// The standard character set (uses `+` and `/`)
    Standard,
    /// The URL safe character set (uses `-` and `_`)
    UrlSafe
}

impl CharacterSet {
    fn encode_table(&self) -> &'static [u8; 64] {
        match *self {
            CharacterSet::Standard => tables::STANDARD_ENCODE,
            CharacterSet::UrlSafe => tables::URL_SAFE_ENCODE
        }
    }

    fn decode_table(&self) -> &'static [u8; 256] {
        match *self {
            CharacterSet::Standard => tables::STANDARD_DECODE,
            CharacterSet::UrlSafe => tables::URL_SAFE_DECODE
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub enum LineEnding {
    LF,
    CRLF,
}

impl LineEnding {
    fn len(&self) -> usize {
        match *self {
            LineEnding::LF => 1,
            LineEnding::CRLF => 2
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub enum LineWrap {
    NoWrap,
    // wrap length is always > 0
    Wrap(usize, LineEnding)
}

/// Contains configuration parameters for base64 encoding
#[derive(Clone, Copy, Debug)]
pub struct Config {
    /// Character set to use
    char_set: CharacterSet,
    /// True to pad output with `=` characters
    pad: bool,
    /// Remove whitespace before decoding, at the cost of an allocation
    strip_whitespace: bool,
    /// ADT signifying whether to linewrap output, and if so by how many characters and with what ending
    line_wrap: LineWrap,
}

impl Config {
    pub fn new(char_set: CharacterSet,
               pad: bool,
               strip_whitespace: bool,
               input_line_wrap: LineWrap) -> Config {
        let line_wrap = match input_line_wrap  {
            LineWrap::Wrap(0, _) => LineWrap::NoWrap,
            _ => input_line_wrap,
        };

        Config {
            char_set: char_set,
            pad: pad,
            strip_whitespace: strip_whitespace,
            line_wrap: line_wrap,
        }
    }
}

pub static STANDARD: Config = Config {
    char_set: CharacterSet::Standard,
    pad: true,
    strip_whitespace: false,
    line_wrap: LineWrap::NoWrap,
};

pub static MIME: Config = Config {
    char_set: CharacterSet::Standard,
    pad: true,
    strip_whitespace: true,
    line_wrap: LineWrap::Wrap(76, LineEnding::CRLF),
};

pub static URL_SAFE: Config = Config {
    char_set: CharacterSet::UrlSafe,
    pad: true,
    strip_whitespace: false,
    line_wrap: LineWrap::NoWrap,
};

pub static URL_SAFE_NO_PAD: Config = Config {
    char_set: CharacterSet::UrlSafe,
    pad: false,
    strip_whitespace: false,
    line_wrap: LineWrap::NoWrap,
};

#[derive(Debug, PartialEq, Eq)]
pub enum DecodeError {
    InvalidByte(usize, u8),
    InvalidLength,
}

impl fmt::Display for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            DecodeError::InvalidByte(index, byte) =>
                write!(f, "Invalid byte {}, offset {}.", byte, index),
            DecodeError::InvalidLength =>
                write!(f, "Encoded text cannot have a 6-bit remainder.")
        }
    }
}

impl error::Error for DecodeError {
    fn description(&self) -> &str {
        match *self {
            DecodeError::InvalidByte(_, _) => "invalid byte",
            DecodeError::InvalidLength => "invalid length"
        }
    }

    fn cause(&self) -> Option<&error::Error> {
        None
    }
}

///Encode arbitrary octets as base64.
///Returns a String.
///Convenience for `encode_config(input, base64::STANDARD);`.
///
///# Example
///
///```rust
///extern crate base64;
///
///fn main() {
///    let b64 = base64::encode(b"hello world");
///    println!("{}", b64);
///}
///```
pub fn encode<T: ?Sized + AsRef<[u8]>>(input: &T) -> String {
    encode_config(input, STANDARD)
}

///Decode from string reference as octets.
///Returns a Result containing a Vec<u8>.
///Convenience `decode_config(input, base64::STANDARD);`.
///
///# Example
///
///```rust
///extern crate base64;
///
///fn main() {
///    let bytes = base64::decode("aGVsbG8gd29ybGQ=").unwrap();
///    println!("{:?}", bytes);
///}
///```
pub fn decode<T: ?Sized + AsRef<[u8]>>(input: &T) -> Result<Vec<u8>, DecodeError> {
    decode_config(input, STANDARD)
}

///Encode arbitrary octets as base64.
///Returns a String.
///
///# Example
///
///```rust
///extern crate base64;
///
///fn main() {
///    let b64 = base64::encode_config(b"hello world~", base64::STANDARD);
///    println!("{}", b64);
///
///    let b64_url = base64::encode_config(b"hello internet~", base64::URL_SAFE);
///    println!("{}", b64_url);
///}
///```
pub fn encode_config<T: ?Sized + AsRef<[u8]>>(input: &T, config: Config) -> String {
    let mut buf = match encoded_size(input.as_ref().len(), &config) {
        Some(n) => String::with_capacity(n),
        None => panic!("integer overflow when calculating buffer size")
    };

    encode_config_buf(input, config, &mut buf);

    buf
}

/// calculate the base64 encoded string size, including padding
fn encoded_size(bytes_len: usize, config: &Config) -> Option<usize> {
    let rem = bytes_len % 3;

    let complete_input_chunks = bytes_len / 3;
    let complete_chunk_output = complete_input_chunks.checked_mul(4);

    let encoded_len_no_wrap = if rem > 0 {
        if config.pad {
            complete_chunk_output.and_then(|c| c.checked_add(4))
        } else {
            let encoded_rem = match rem {
                1 => 2,
                2 => 3,
                _ => panic!("Impossible remainder")
            };
            complete_chunk_output.and_then(|c| c.checked_add(encoded_rem))
        }
    } else {
        complete_chunk_output
    };

    encoded_len_no_wrap.map(|e| {
        match config.line_wrap {
            LineWrap::NoWrap => e,
            LineWrap::Wrap(line_len, line_ending) => {
                line_wrap_parameters(e, line_len, line_ending).total_len
            }
        }
    })
}

///Encode arbitrary octets as base64.
///Writes into the supplied buffer to avoid allocations.
///
///# Example
///
///```rust
///extern crate base64;
///
///fn main() {
///    let mut buf = String::new();
///    base64::encode_config_buf(b"hello world~", base64::STANDARD, &mut buf);
///    println!("{}", buf);
///
///    buf.clear();
///    base64::encode_config_buf(b"hello internet~", base64::URL_SAFE, &mut buf);
///    println!("{}", buf);
///}
///```
pub fn encode_config_buf<T: ?Sized + AsRef<[u8]>>(input: &T, config: Config, buf: &mut String) {
    let input_bytes = input.as_ref();

    let encoded_size = encoded_size(input_bytes.len(), &config)
        .expect("usize overflow when calculating buffer size");

    let orig_buf_len = buf.len();

    // we're only going to insert valid utf8
    let mut buf_bytes;
    unsafe {
        buf_bytes = buf.as_mut_vec();
    }

    buf_bytes.resize(orig_buf_len.checked_add(encoded_size)
                         .expect("usize overflow when calculating expanded buffer size"), 0);

    let mut b64_output = &mut buf_bytes[orig_buf_len..];

    let encoded_bytes = encode_with_padding(input_bytes, b64_output, config.char_set.encode_table(),
                                            config.pad);

    if let LineWrap::Wrap(line_len, line_end) = config.line_wrap {
        line_wrap(b64_output, encoded_bytes, line_len, line_end);
    }
}

/// Encode input bytes and pad if configured.
/// `output` must be long enough to hold the encoded `input` with padding.
/// Returns the number of bytes written.
fn encode_with_padding(input: &[u8], output: &mut [u8], encode_table: &[u8; 64], pad: bool) -> usize {
    let b64_bytes_written = encode_to_slice(input, output, encode_table);

    let padding_bytes = if pad {
        add_padding(input.len(), &mut output[b64_bytes_written..])
    } else {
        0
    };

    b64_bytes_written.checked_add(padding_bytes)
        .expect("usize overflow when calculating b64 length")
}

/// Encode input bytes to utf8 base64 bytes. Does not pad or line wrap.
/// `output` must be long enough to hold the encoded `input` without padding or line wrapping.
/// Returns the number of bytes written.
#[inline]
fn encode_to_slice(input: &[u8], output: &mut [u8], encode_table: &[u8; 64]) -> usize {
    let mut input_index: usize = 0;

    const BLOCKS_PER_FAST_LOOP: usize = 4;
    const LOW_SIX_BITS: u64 = 0x3F;

    // we read 8 bytes at a time (u64) but only actually consume 6 of those bytes. Thus, we need
    // 2 trailing bytes to be available to read..
    let last_fast_index = input.len().saturating_sub(BLOCKS_PER_FAST_LOOP * 6 + 2);
    let mut output_index = 0;

    if last_fast_index > 0 {
        while input_index <= last_fast_index {
            // Major performance wins from letting the optimizer do the bounds check once, mostly
            // on the output side
            let input_chunk = &input[input_index..(input_index + (BLOCKS_PER_FAST_LOOP * 6 + 2))];
            let mut output_chunk = &mut output[output_index..(output_index + BLOCKS_PER_FAST_LOOP * 8)];

            // Hand-unrolling for 32 vs 16 or 8 bytes produces yields performance about equivalent
            // to unsafe pointer code on a Xeon E5-1650v3. 64 byte unrolling was slightly better for
            // large inputs but significantly worse for 50-byte input, unsurprisingly. I suspect
            // that it's a not uncommon use case to encode smallish chunks of data (e.g. a 64-byte
            // SHA-512 digest), so it would be nice if that fit in the unrolled loop at least once.
            // Plus, single-digit percentage performance differences might well be quite different
            // on different hardware.

            let input_u64 = BigEndian::read_u64(&input_chunk[0..]);

            output_chunk[0] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[1] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[2] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[3] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[4] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[5] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[6] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[7] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            let input_u64 = BigEndian::read_u64(&input_chunk[6..]);

            output_chunk[8] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[9] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[10] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[11] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[12] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[13] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[14] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[15] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            let input_u64 = BigEndian::read_u64(&input_chunk[12..]);

            output_chunk[16] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[17] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[18] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[19] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[20] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[21] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[22] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[23] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            let input_u64 = BigEndian::read_u64(&input_chunk[18..]);

            output_chunk[24] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[25] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[26] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[27] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[28] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[29] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[30] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[31] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            output_index += BLOCKS_PER_FAST_LOOP * 8;
            input_index += BLOCKS_PER_FAST_LOOP * 6;
        }
    }

    // Encode what's left after the fast loop.

    const LOW_SIX_BITS_U8: u8 = 0x3F;

    let rem = input.len() % 3;
    let start_of_rem = input.len() - rem;

    // start at the first index not handled by fast loop, which may be 0.

    while input_index < start_of_rem {
        let input_chunk = &input[input_index..(input_index + 3)];
        let mut output_chunk = &mut output[output_index..(output_index + 4)];

        output_chunk[0] = encode_table[(input_chunk[0] >> 2) as usize];
        output_chunk[1] = encode_table[((input_chunk[0] << 4 | input_chunk[1] >> 4) & LOW_SIX_BITS_U8) as usize];
        output_chunk[2] = encode_table[((input_chunk[1] << 2 | input_chunk[2] >> 6) & LOW_SIX_BITS_U8) as usize];
        output_chunk[3] = encode_table[(input_chunk[2] & LOW_SIX_BITS_U8) as usize];

        input_index += 3;
        output_index += 4;
    }

    if rem == 2 {
        output[output_index] = encode_table[(input[start_of_rem] >> 2) as usize];
        output[output_index + 1] = encode_table[((input[start_of_rem] << 4 | input[start_of_rem + 1] >> 4) & LOW_SIX_BITS_U8) as usize];
        output[output_index + 2] = encode_table[((input[start_of_rem + 1] << 2) & LOW_SIX_BITS_U8) as usize];
        output_index += 3;
    } else if rem == 1 {
        output[output_index] = encode_table[(input[start_of_rem] >> 2) as usize];
        output[output_index + 1] = encode_table[((input[start_of_rem] << 4) & LOW_SIX_BITS_U8) as usize];
        output_index += 2;
    }

    output_index
}

/// Write padding characters.
/// `output` is the slice where padding should be written, of length at least 2.
fn add_padding(input_len: usize, output: &mut[u8]) -> usize {
    let rem = input_len % 3;
    let mut bytes_written = 0;
    for _ in 0..((3 - rem) % 3) {
        output[bytes_written] = b'=';
        bytes_written += 1;
    }

    bytes_written
}

///Decode from string reference as octets.
///Returns a Result containing a Vec<u8>.
///
///# Example
///
///```rust
///extern crate base64;
///
///fn main() {
///    let bytes = base64::decode_config("aGVsbG8gd29ybGR+Cg==", base64::STANDARD).unwrap();
///    println!("{:?}", bytes);
///
///    let bytes_url = base64::decode_config("aGVsbG8gaW50ZXJuZXR-Cg==", base64::URL_SAFE).unwrap();
///    println!("{:?}", bytes_url);
///}
///```
pub fn decode_config<T: ?Sized + AsRef<[u8]>>(input: &T, config: Config) -> Result<Vec<u8>, DecodeError> {
    let mut buffer = Vec::<u8>::with_capacity(input.as_ref().len() * 4 / 3);

    decode_config_buf(input, config, &mut buffer).map(|_| buffer)
}

///Decode from string reference as octets.
///Writes into the supplied buffer to avoid allocation.
///Returns a Result containing an empty tuple, aka ().
///
///# Example
///
///```rust
///extern crate base64;
///
///fn main() {
///    let mut buffer = Vec::<u8>::new();
///    base64::decode_config_buf("aGVsbG8gd29ybGR+Cg==", base64::STANDARD, &mut buffer).unwrap();
///    println!("{:?}", buffer);
///
///    buffer.clear();
///
///    base64::decode_config_buf("aGVsbG8gaW50ZXJuZXR-Cg==", base64::URL_SAFE, &mut buffer).unwrap();
///    println!("{:?}", buffer);
///}
///```
pub fn decode_config_buf<T: ?Sized + AsRef<[u8]>>(input: &T,
                                                  config: Config,
                                                  buffer: &mut Vec<u8>)
                                                  -> Result<(), DecodeError> {
    let mut input_copy;
    let input_bytes = if config.strip_whitespace {
        input_copy = Vec::<u8>::with_capacity(input.as_ref().len());
        input_copy.extend(input.as_ref().iter().filter(|b| !b" \n\t\r\x0b\x0c".contains(b)));

        input_copy.as_ref()
    } else {
        input.as_ref()
    };

    let decode_table = &config.char_set.decode_table();

    // decode logic operates on chunks of 8 input bytes without padding
    const INPUT_CHUNK_LEN: usize = 8;
    const DECODED_CHUNK_LEN: usize = 6;
    // we read a u64 and write a u64, but a u64 of input only yields 6 bytes of output, so the last
    // 2 bytes of any output u64 should not be counted as written to (but must be available in a
    // slice).
    const DECODED_CHUNK_SUFFIX: usize = 2;

    let remainder_len = input_bytes.len() % INPUT_CHUNK_LEN;
    let trailing_bytes_to_skip = if remainder_len == 0 {
        // if input is a multiple of the chunk size, ignore the last chunk as it may have padding,
        // and the fast decode logic cannot handle padding
        INPUT_CHUNK_LEN
    } else {
        remainder_len
    };

    let length_of_full_chunks = input_bytes.len().saturating_sub(trailing_bytes_to_skip);

    let starting_output_index = buffer.len();
    // Resize to hold decoded output from fast loop. Need the extra two bytes because
    // we write a full 8 bytes for the last 6-byte decoded chunk and then truncate off the last two.
    let new_size = starting_output_index
        .checked_add(length_of_full_chunks / INPUT_CHUNK_LEN * DECODED_CHUNK_LEN)
        .and_then(|l| l.checked_add(DECODED_CHUNK_SUFFIX))
        .expect("Overflow when calculating output buffer length");

    buffer.resize(new_size, 0);

    {
        let mut output_index = 0;
        let mut input_index = 0;
        let buffer_slice = &mut buffer.as_mut_slice()[starting_output_index..];

        // how many u64's of input to handle at a time
        const CHUNKS_PER_FAST_LOOP_BLOCK: usize = 4;
        const INPUT_BLOCK_LEN: usize = CHUNKS_PER_FAST_LOOP_BLOCK * INPUT_CHUNK_LEN;
        // includes the trailing 2 bytes for the final u64 write
        const DECODED_BLOCK_LEN: usize = CHUNKS_PER_FAST_LOOP_BLOCK * DECODED_CHUNK_LEN +
            DECODED_CHUNK_SUFFIX;
        // the start index of the last block of data that is big enough to use the unrolled loop
        let last_block_start_index = length_of_full_chunks
            .saturating_sub(INPUT_CHUNK_LEN * CHUNKS_PER_FAST_LOOP_BLOCK);

        // manual unroll to CHUNKS_PER_FAST_LOOP_BLOCK of u64s to amortize slice bounds checks
        if last_block_start_index > 0 {
            while input_index <= last_block_start_index {
                let input_slice = &input_bytes[input_index..(input_index + INPUT_BLOCK_LEN)];
                let output_slice = &mut buffer_slice[output_index..(output_index + DECODED_BLOCK_LEN)];

                decode_chunk(&input_slice[0..], input_index, decode_table, &mut output_slice[0..])?;
                decode_chunk(&input_slice[8..], input_index + 8, decode_table, &mut output_slice[6..])?;
                decode_chunk(&input_slice[16..], input_index + 16, decode_table, &mut output_slice[12..])?;
                decode_chunk(&input_slice[24..], input_index + 24, decode_table, &mut output_slice[18..])?;

                input_index += INPUT_BLOCK_LEN;
                output_index += DECODED_BLOCK_LEN - DECODED_CHUNK_SUFFIX;
            }
        }

        // still pretty fast loop: 8 bytes at a time for whatever we didn't do in the faster loop.
        while input_index < length_of_full_chunks {
            decode_chunk(&input_bytes[input_index..(input_index + 8)], input_index, decode_table,
                         &mut buffer_slice[output_index..(output_index + 8)])?;

            output_index += DECODED_CHUNK_LEN;
            input_index += INPUT_CHUNK_LEN;
        }
    }

    // Truncate off the last two bytes from writing the last u64.
    // Unconditional because we added on the extra 2 bytes in the resize before the loop,
    // so it will never underflow.
    let new_len = buffer.len() - DECODED_CHUNK_SUFFIX;
    buffer.truncate(new_len);

    // handle leftovers (at most 8 bytes, decoded to 6).
    // Use a u64 as a stack-resident 8 byte buffer.
    let mut leftover_bits: u64 = 0;
    let mut morsels_in_leftover = 0;
    let mut padding_bytes = 0;
    let mut first_padding_index: usize = 0;
    for (i, b) in input_bytes[length_of_full_chunks..].iter().enumerate() {
        // '=' padding
        if *b == 0x3D {
            // There can be bad padding in a few ways:
            // 1 - Padding with non-padding characters after it
            // 2 - Padding after zero or one non-padding characters before it
            //     in the current quad.
            // 3 - More than two characters of padding. If 3 or 4 padding chars
            //     are in the same quad, that implies it will be caught by #2.
            //     If it spreads from one quad to another, it will be caught by
            //     #2 in the second quad.

            if i % 4 < 2 {
                // Check for case #2.
                let bad_padding_index = length_of_full_chunks + if padding_bytes > 0 {
                    // If we've already seen padding, report the first padding index.
                    // This is to be consistent with the faster logic above: it will report an error
                    // on the first padding character (since it doesn't expect to see anything but
                    // actual encoded data).
                    first_padding_index
                } else {
                    // haven't seen padding before, just use where we are now
                    i
                };
                return Err(DecodeError::InvalidByte(bad_padding_index, *b));
            }

            if padding_bytes == 0 {
                first_padding_index = i;
            }

            padding_bytes += 1;
            continue;
        }

        // Check for case #1.
        // To make '=' handling consistent with the main loop, don't allow
        // non-suffix '=' in trailing chunk either. Report error as first
        // erroneous padding.
        if padding_bytes > 0 {
            return Err(DecodeError::InvalidByte(
                length_of_full_chunks + first_padding_index, 0x3D));
        }

        // can use up to 8 * 6 = 48 bits of the u64, if last chunk has no padding.
        // To minimize shifts, pack the leftovers from left to right.
        let shift = 64 - (morsels_in_leftover + 1) * 6;
        // tables are all 256 elements, lookup with a u8 index always succeeds
        let morsel = decode_table[*b as usize];
        if morsel == tables::INVALID_VALUE {
            return Err(DecodeError::InvalidByte(length_of_full_chunks + i, *b));
        }

        leftover_bits |= (morsel as u64) << shift;
        morsels_in_leftover += 1;
    }

    let leftover_bits_ready_to_append = match morsels_in_leftover {
        0 => 0,
        1 => return Err(DecodeError::InvalidLength),
        2 => 8,
        3 => 16,
        4 => 24,
        5 => return Err(DecodeError::InvalidLength),
        6 => 32,
        7 => 40,
        8 => 48,
        _ => panic!("Impossible: must only have 0 to 4 input bytes in last quad")
    };

    let mut leftover_bits_appended_to_buf = 0;
    while leftover_bits_appended_to_buf < leftover_bits_ready_to_append {
        // `as` simply truncates the higher bits, which is what we want here
        let selected_bits = (leftover_bits >> (56 - leftover_bits_appended_to_buf)) as u8;
        buffer.push(selected_bits);

        leftover_bits_appended_to_buf += 8;
    };

    Ok(())
}

// yes, really inline (worth 30-50% speedup)
#[inline(always)]
fn decode_chunk(input: &[u8], index_at_start_of_input: usize, decode_table: &[u8; 256],
                output: &mut [u8]) -> Result<(), DecodeError> {
    let mut accum: u64;

    let morsel = decode_table[input[0] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input, input[0]));
    }
    accum = (morsel as u64) << 58;

    let morsel = decode_table[input[1] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input + 1, input[1]));
    }
    accum |= (morsel as u64) << 52;

    let morsel = decode_table[input[2] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input + 2, input[2]));
    }
    accum |= (morsel as u64) << 46;

    let morsel = decode_table[input[3] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input + 3, input[3]));
    }
    accum |= (morsel as u64) << 40;

    let morsel = decode_table[input[4] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input + 4, input[4]));
    }
    accum |= (morsel as u64) << 34;

    let morsel = decode_table[input[5] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input + 5, input[5]));
    }
    accum |= (morsel as u64) << 28;

    let morsel = decode_table[input[6] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input + 6, input[6]));
    }
    accum |= (morsel as u64) << 22;

    let morsel = decode_table[input[7] as usize];
    if morsel == tables::INVALID_VALUE {
        return Err(DecodeError::InvalidByte(index_at_start_of_input + 7, input[7]));
    }
    accum |= (morsel as u64) << 16;

    BigEndian::write_u64(output, accum);

    Ok(())
}

#[cfg(test)]
mod tests;
