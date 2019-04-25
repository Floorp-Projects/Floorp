// Copyright 2014-2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A [DEFLATE](http://www.gzip.org/zlib/rfc-deflate.html) decoder written in rust.
//!
//! This library provides functionality to decompress data compressed with the DEFLATE algorithm,
//! both with and without a [zlib](https://tools.ietf.org/html/rfc1950) header/trailer.
//!
//! # Examples
//! The easiest way to get `std::Vec<u8>` containing the decompressed bytes is to use either
//! `inflate::inflate_bytes` or `inflate::inflate_bytes_zlib` (depending on whether
//! the encoded data has zlib headers and trailers or not). The following example
//! decodes the DEFLATE encoded string "Hello, world" and prints it:
//!
//! ```rust
//! use inflate::inflate_bytes;
//! use std::str::from_utf8;
//!
//! let encoded = [243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
//! let decoded = inflate_bytes(&encoded).unwrap();
//! println!("{}", from_utf8(&decoded).unwrap()); // prints "Hello, world"
//! ```
//!
//! If you need more flexibility, then the library also provides an implementation
//! of `std::io::Writer` in `inflate::writer`. Below is an example using an
//! `inflate::writer::InflateWriter` to decode the DEFLATE encoded string "Hello, world":
//!
//! ```rust
//! use inflate::InflateWriter;
//! use std::io::Write;
//! use std::str::from_utf8;
//!
//! let encoded = [243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
//! let mut decoder = InflateWriter::new(Vec::new());
//! decoder.write(&encoded).unwrap();
//! let decoded = decoder.finish().unwrap();
//! println!("{}", from_utf8(&decoded).unwrap()); // prints "Hello, world"
//! ```
//!
//! Finally, if you need even more flexibility, or if you only want to depend on
//! `core`, you can use the `inflate::InflateStream` API. The below example
//! decodes an array of DEFLATE encoded bytes:
//!
//! ```rust
//! use inflate::InflateStream;
//!
//! let data = [0x73, 0x49, 0x4d, 0xcb, 0x49, 0x2c, 0x49, 0x55, 0x00, 0x11, 0x00];
//! let mut inflater = InflateStream::new();
//! let mut out = Vec::<u8>::new();
//! let mut n = 0;
//! while n < data.len() {
//!     let res = inflater.update(&data[n..]);
//!     if let Ok((num_bytes_read, result)) = res {
//!         n += num_bytes_read;
//!         out.extend(result.iter().cloned());
//!     } else {
//!         res.unwrap();
//!     }
//! }
//! ```

extern crate adler32;

use std::cmp;
use std::slice;

mod checksum;
use checksum::{Checksum, adler32_from_bytes};

mod writer;
pub use self::writer::{InflateWriter};

mod utils;
pub use self::utils::{inflate_bytes, inflate_bytes_zlib, inflate_bytes_zlib_no_checksum};

mod reader;
pub use self::reader::{DeflateDecoder, DeflateDecoderBuf};

static BIT_REV_U8: [u8; 256] = [
    0b0000_0000, 0b1000_0000, 0b0100_0000, 0b1100_0000,
    0b0010_0000, 0b1010_0000, 0b0110_0000, 0b1110_0000,
    0b0001_0000, 0b1001_0000, 0b0101_0000, 0b1101_0000,
    0b0011_0000, 0b1011_0000, 0b0111_0000, 0b1111_0000,

    0b0000_1000, 0b1000_1000, 0b0100_1000, 0b1100_1000,
    0b0010_1000, 0b1010_1000, 0b0110_1000, 0b1110_1000,
    0b0001_1000, 0b1001_1000, 0b0101_1000, 0b1101_1000,
    0b0011_1000, 0b1011_1000, 0b0111_1000, 0b1111_1000,

    0b0000_0100, 0b1000_0100, 0b0100_0100, 0b1100_0100,
    0b0010_0100, 0b1010_0100, 0b0110_0100, 0b1110_0100,
    0b0001_0100, 0b1001_0100, 0b0101_0100, 0b1101_0100,
    0b0011_0100, 0b1011_0100, 0b0111_0100, 0b1111_0100,

    0b0000_1100, 0b1000_1100, 0b0100_1100, 0b1100_1100,
    0b0010_1100, 0b1010_1100, 0b0110_1100, 0b1110_1100,
    0b0001_1100, 0b1001_1100, 0b0101_1100, 0b1101_1100,
    0b0011_1100, 0b1011_1100, 0b0111_1100, 0b1111_1100,


    0b0000_0010, 0b1000_0010, 0b0100_0010, 0b1100_0010,
    0b0010_0010, 0b1010_0010, 0b0110_0010, 0b1110_0010,
    0b0001_0010, 0b1001_0010, 0b0101_0010, 0b1101_0010,
    0b0011_0010, 0b1011_0010, 0b0111_0010, 0b1111_0010,

    0b0000_1010, 0b1000_1010, 0b0100_1010, 0b1100_1010,
    0b0010_1010, 0b1010_1010, 0b0110_1010, 0b1110_1010,
    0b0001_1010, 0b1001_1010, 0b0101_1010, 0b1101_1010,
    0b0011_1010, 0b1011_1010, 0b0111_1010, 0b1111_1010,

    0b0000_0110, 0b1000_0110, 0b0100_0110, 0b1100_0110,
    0b0010_0110, 0b1010_0110, 0b0110_0110, 0b1110_0110,
    0b0001_0110, 0b1001_0110, 0b0101_0110, 0b1101_0110,
    0b0011_0110, 0b1011_0110, 0b0111_0110, 0b1111_0110,

    0b0000_1110, 0b1000_1110, 0b0100_1110, 0b1100_1110,
    0b0010_1110, 0b1010_1110, 0b0110_1110, 0b1110_1110,
    0b0001_1110, 0b1001_1110, 0b0101_1110, 0b1101_1110,
    0b0011_1110, 0b1011_1110, 0b0111_1110, 0b1111_1110,


    0b0000_0001, 0b1000_0001, 0b0100_0001, 0b1100_0001,
    0b0010_0001, 0b1010_0001, 0b0110_0001, 0b1110_0001,
    0b0001_0001, 0b1001_0001, 0b0101_0001, 0b1101_0001,
    0b0011_0001, 0b1011_0001, 0b0111_0001, 0b1111_0001,

    0b0000_1001, 0b1000_1001, 0b0100_1001, 0b1100_1001,
    0b0010_1001, 0b1010_1001, 0b0110_1001, 0b1110_1001,
    0b0001_1001, 0b1001_1001, 0b0101_1001, 0b1101_1001,
    0b0011_1001, 0b1011_1001, 0b0111_1001, 0b1111_1001,

    0b0000_0101, 0b1000_0101, 0b0100_0101, 0b1100_0101,
    0b0010_0101, 0b1010_0101, 0b0110_0101, 0b1110_0101,
    0b0001_0101, 0b1001_0101, 0b0101_0101, 0b1101_0101,
    0b0011_0101, 0b1011_0101, 0b0111_0101, 0b1111_0101,

    0b0000_1101, 0b1000_1101, 0b0100_1101, 0b1100_1101,
    0b0010_1101, 0b1010_1101, 0b0110_1101, 0b1110_1101,
    0b0001_1101, 0b1001_1101, 0b0101_1101, 0b1101_1101,
    0b0011_1101, 0b1011_1101, 0b0111_1101, 0b1111_1101,


    0b0000_0011, 0b1000_0011, 0b0100_0011, 0b1100_0011,
    0b0010_0011, 0b1010_0011, 0b0110_0011, 0b1110_0011,
    0b0001_0011, 0b1001_0011, 0b0101_0011, 0b1101_0011,
    0b0011_0011, 0b1011_0011, 0b0111_0011, 0b1111_0011,

    0b0000_1011, 0b1000_1011, 0b0100_1011, 0b1100_1011,
    0b0010_1011, 0b1010_1011, 0b0110_1011, 0b1110_1011,
    0b0001_1011, 0b1001_1011, 0b0101_1011, 0b1101_1011,
    0b0011_1011, 0b1011_1011, 0b0111_1011, 0b1111_1011,

    0b0000_0111, 0b1000_0111, 0b0100_0111, 0b1100_0111,
    0b0010_0111, 0b1010_0111, 0b0110_0111, 0b1110_0111,
    0b0001_0111, 0b1001_0111, 0b0101_0111, 0b1101_0111,
    0b0011_0111, 0b1011_0111, 0b0111_0111, 0b1111_0111,

    0b0000_1111, 0b1000_1111, 0b0100_1111, 0b1100_1111,
    0b0010_1111, 0b1010_1111, 0b0110_1111, 0b1110_1111,
    0b0001_1111, 0b1001_1111, 0b0101_1111, 0b1101_1111,
    0b0011_1111, 0b1011_1111, 0b0111_1111, 0b1111_1111
];

#[derive(Clone, Copy)]
struct BitState {
    n: u8,
    v: u32,
}

#[derive(Clone)]
struct BitStream<'a> {
    bytes: slice::Iter<'a, u8>,
    used: usize,
    state: BitState,
}

#[cfg(debug)]
macro_rules! debug { ($($x:tt)*) => (println!($($x)*)) }
#[cfg(not(debug))]
macro_rules! debug { ($($x:tt)*) => (()) }

impl<'a> BitStream<'a> {
    fn new(bytes: &'a [u8], state: BitState) -> BitStream<'a> {
        BitStream {
            bytes: bytes.iter(),
            used: 0,
            state: state,
        }
    }

    fn use_byte(&mut self) -> bool {
        match self.bytes.next() {
            Some(&b) => {
                self.state.v |= (b as u32) << self.state.n;
                self.state.n += 8;
                self.used += 1;
                true
            }
            None => false,
        }
    }

    fn need(&mut self, n: u8) -> bool {
        if self.state.n < n {
            if !self.use_byte() {
                return false;
            }
            if n > 8 && self.state.n < n {
                assert!(n <= 16);
                if !self.use_byte() {
                    return false;
                }
            }
        }
        true
    }

    fn take16(&mut self, n: u8) -> Option<u16> {
        if self.need(n) {
            self.state.n -= n;
            let v = self.state.v & ((1 << n) - 1);
            self.state.v >>= n;
            Some(v as u16)
        } else {
            None
        }
    }

    fn take(&mut self, n: u8) -> Option<u8> {
        assert!(n <= 8);
        self.take16(n).map(|v: u16| v as u8)
    }

    fn fill(&mut self) -> BitState {
        while self.state.n + 8 <= 32 && self.use_byte() {}
        self.state
    }

    fn align_byte(&mut self) {
        if self.state.n > 0 {
            let n = self.state.n % 8;
            self.take(n);
        }
    }

    fn trailing_bytes(&mut self) -> (u8, [u8; 4]) {
        let mut len = 0;
        let mut bytes = [0; 4];
        self.align_byte();
        while self.state.n >= 8 {
            bytes[len as usize] = self.state.v as u8;
            len += 1;
            self.state.n -= 8;
            self.state.v >>= 8;
        }
        (len, bytes)
    }
}

/// Generate huffman codes from the given set of lengths and run `$cb` on them except the first
/// code for each length.
///
/// See also the [deflate specification](http://www.gzip.org/zlib/rfc-deflate.html#huffman)
/// for an explanation of the algorithm.
macro_rules! with_codes (($clens:expr, $max_bits:expr => $code_ty:ty, $cb:expr) => ({
    // Count the number of codes for each bit length.
    let mut bl_count = [0 as $code_ty; ($max_bits+1)];
    for &bits in $clens.iter() {
        if bits != 0 {
            // This should be safe from overflow as the number of lengths read from the input
            // is bounded by the number of bits the number of lengths is represented by in the
            // deflate compressed data.
            bl_count[bits as usize] += 1;
        }
    }

    // Compute the first code value for each bit length.
    let mut next_code = [0 as $code_ty; ($max_bits+1)];
    let mut code = 0 as $code_ty;
    // TODO use range_inclusive as soon as it is stable
    //for bits in range_inclusive(1, $max_bits) {
    for bits in 1..$max_bits + 1 {
        code = try!(
            code.checked_add(bl_count[bits as usize - 1])
                .ok_or_else(|| "Error generating huffman codes: Invalid set of code lengths")
        ) << 1;
        next_code[bits as usize] = code;
    }

    // Compute the rest of the codes
    for (i, &bits) in $clens.iter().enumerate() {
        if bits != 0 {
            let code = next_code[bits as usize];
            // If there is an overflow here, the given set of code lengths won't allow enough
            // unique codes to be generated.
            let new_code = try!(
                code.checked_add(1)
                    .ok_or_else(|| "Error generating huffman codes: Invalid set of code lengths!")
            );
            next_code[bits as usize] = new_code;
            match $cb(i as $code_ty, code, bits) {
                Ok(()) => (),
                Err(err) => return Err(err)
            }
        }
    }
}));

struct CodeLengthReader {
    patterns: Box<[u8; 128]>,
    clens: Box<[u8; 19]>,
    result: Vec<u8>,
    num_lit: u16,
    num_dist: u8,
}

impl CodeLengthReader {
    fn new(clens: Box<[u8; 19]>, num_lit: u16, num_dist: u8) -> Result<CodeLengthReader, String> {
        // Fill in the 7-bit patterns that match each code.
        let mut patterns = Box::new([0xffu8; 128]);
        with_codes!(clens, 7 => u8, |i: u8, code: u8, bits| -> _ {
            /*let base = match BIT_REV_U8.get((code << (8 - bits)) as usize) {
                Some(&base) => base,
                None => return Err("invalid length code".to_owned())
            }*/
            let base = BIT_REV_U8[(code << (8 - bits)) as usize];
            for rest in 0u8 .. 1u8 << (7 - bits) {
                patterns[(base | (rest << bits)) as usize] = i;
            }
            Ok(())
        });

        Ok(CodeLengthReader {
            patterns: patterns,
            clens: clens,
            result: Vec::with_capacity(num_lit as usize + num_dist as usize),
            num_lit: num_lit,
            num_dist: num_dist,
        })
    }

    fn read(&mut self, stream: &mut BitStream) -> Result<bool, String> {
        let total_len = self.num_lit as usize + self.num_dist as usize;
        while self.result.len() < total_len {
            if !stream.need(7) {
                return Ok(false);
            }
            let save = stream.clone();
            macro_rules! take (($n:expr) => (match stream.take($n) {
                Some(v) => v,
                None => {
                    *stream = save;
                    return Ok(false);
                }
            }));
            let code = self.patterns[(stream.state.v & 0x7f) as usize];
            stream.take(match self.clens.get(code as usize) {
                Some(&len) => len,
                None => return Err("invalid length code".to_owned()),
            });
            match code {
                0...15 => self.result.push(code),
                16 => {
                    let last = match self.result.last() {
                        Some(&v) => v,
                        // 16 appeared before anything else
                        None => return Err("invalid length code".to_owned()),
                    };
                    for _ in 0..3 + take!(2) {
                        self.result.push(last);
                    }
                }
                17 => {
                    for _ in 0..3 + take!(3) {
                        self.result.push(0);
                    }
                }
                18 => {
                    for _ in 0..11 + take!(7) {
                        self.result.push(0);
                    }
                }
                _ => unreachable!(),
            }
        }
        Ok(true)
    }

    fn to_lit_and_dist(&self) -> Result<(DynHuffman16, DynHuffman16), String> {
        let num_lit = self.num_lit as usize;
        let lit = try!(DynHuffman16::new(&self.result[..num_lit]));
        let dist = try!(DynHuffman16::new(&self.result[num_lit..]));
        Ok((lit, dist))
    }
}

struct Trie8bit<T> {
    data: [T; 16],
    children: [Option<Box<[T; 16]>>; 16],
}

struct DynHuffman16 {
    patterns: Box<[u16; 256]>,
    rest: Vec<Trie8bit<u16>>,
}

impl DynHuffman16 {
    fn new(clens: &[u8]) -> Result<DynHuffman16, String> {
        // Fill in the 8-bit patterns that match each code.
        // Longer patterns go into the trie.
        let mut patterns = Box::new([0xffffu16; 256]);
        let mut rest = Vec::new();
        with_codes!(clens, 15 => u16, |i: u16, code: u16, bits: u8| -> _ {
            let entry = i | ((bits as u16) << 12);
            if bits <= 8 {
                let base = match BIT_REV_U8.get((code << (8 - bits)) as usize) {
                    Some(&v) => v,
                    None => return Err("invalid length code".to_owned())
                };
                for rest in 0u8 .. 1 << (8 - bits) {
                    patterns[(base | (rest << (bits & 7))) as usize] = entry;
                }
            } else {
                let low = match BIT_REV_U8.get((code >> (bits - 8)) as usize) {
                    Some(&v) => v,
                    None => return Err("invalid length code".to_owned())
                };
                let high = BIT_REV_U8[((code << (16 - bits)) & 0xff) as usize];
                let (min_bits, idx) = if patterns[low as usize] != 0xffff {
                    let bits_prev = (patterns[low as usize] >> 12) as u8;
                    (cmp::min(bits_prev, bits), patterns[low as usize] & 0x7ff)
                } else {
                    rest.push(Trie8bit {
                        data: [0xffff; 16],
                        children: [
                            None, None, None, None,
                            None, None, None, None,
                            None, None, None, None,
                            None, None, None, None
                        ]
                    });
                    (bits, (rest.len() - 1) as u16)
                };
                patterns[low as usize] = idx | 0x800 | ((min_bits as u16) << 12);
                let trie_entry = match rest.get_mut(idx as usize) {
                    Some(v) => v,
                    None => return Err("invalid huffman code".to_owned())
                };
                if bits <= 12 {
                    for rest in 0u8 .. 1 << (12 - bits) {
                        trie_entry.data[(high | (rest << (bits - 8))) as usize] = entry;
                    }
                } else {
                    let child = &mut trie_entry.children[(high & 0xf) as usize];
                    if child.is_none() {
                        *child = Some(Box::new([0xffff; 16]));
                    }
                    let child = &mut **child.as_mut().unwrap();
                    let high_top = high >> 4;
                    for rest in 0u8 .. 1 << (16 - bits) {
                        child[(high_top | (rest << (bits - 12))) as usize] = entry;
                    }
                }
            }
            Ok(())
        });
        debug!("=== DYN HUFFMAN ===");
        for _i in 0..256 {
            debug!("{:08b} {:04x}", _i, patterns[BIT_REV_U8[_i] as usize]);
        }
        debug!("===================");
        Ok(DynHuffman16 {
            patterns: patterns,
            rest: rest,
        })
    }

    fn read<'a>(&self, stream: &mut BitStream<'a>) -> Result<Option<(BitStream<'a>, u16)>, String> {
        let has8 = stream.need(8);
        let entry = self.patterns[(stream.state.v & 0xff) as usize];
        let bits = (entry >> 12) as u8;

        Ok(if !has8 {
            if bits <= stream.state.n {
                let save = stream.clone();
                stream.state.n -= bits;
                stream.state.v >>= bits;
                Some((save, entry & 0xfff))
            } else {
                None
            }
        } else if bits <= 8 {
            let save = stream.clone();
            stream.state.n -= bits;
            stream.state.v >>= bits;
            Some((save, entry & 0xfff))
        } else {
            let has16 = stream.need(16);
            let trie = match self.rest.get((entry & 0x7ff) as usize) {
                Some(trie) => trie,
                None => return Err("invalid entry in stream".to_owned()),
            };
            let idx = stream.state.v >> 8;
            let trie_entry = match trie.children[(idx & 0xf) as usize] {
                Some(ref child) => child[((idx >> 4) & 0xf) as usize],
                None => trie.data[(idx & 0xf) as usize],
            };
            let trie_bits = (trie_entry >> 12) as u8;
            if has16 || trie_bits <= stream.state.n {
                let save = stream.clone();
                stream.state.n -= trie_bits;
                stream.state.v >>= trie_bits;
                Some((save, trie_entry & 0xfff))
            } else {
                None
            }
        })
    }
}

enum State {
    ZlibMethodAndFlags, // CMF
    ZlibFlags(/* CMF */ u8), // FLG,
    Bits(BitsNext, BitState),
    LenDist((BitsNext, BitState), /* len */ u16, /* dist */ u16),
    Uncompressed(/* len */ u16),
    CheckCRC(/* len */ u8, /* bytes */ [u8; 4]),
    Finished
}

use self::State::*;

enum BitsNext {
    BlockHeader,
    BlockUncompressedLen,
    BlockUncompressedNlen(/* len */ u16),
    BlockDynHlit,
    BlockDynHdist(/* hlit */ u8),
    BlockDynHclen(/* hlit */ u8, /* hdist */ u8),
    BlockDynClenCodeLengths(/* hlit */ u8, /* hdist */ u8, /* hclen */ u8, /* idx */ u8, /* clens */ Box<[u8; 19]>),
    BlockDynCodeLengths(CodeLengthReader),
    BlockDyn(/* lit/len */ DynHuffman16, /* dist */ DynHuffman16, /* prev_len */ u16)
}

use self::BitsNext::*;

pub struct InflateStream {
    buffer: Vec<u8>,
    pos: u16,
    state: Option<State>,
    final_block: bool,
    checksum: Checksum,
    read_checksum: Option<u32>,
}

impl InflateStream {
    #[allow(dead_code)]
    /// Create a new stream for decoding raw deflate encoded data.
    pub fn new() -> InflateStream {
        let state = Bits(BlockHeader, BitState { n: 0, v: 0 });
        let buffer = Vec::with_capacity(32 * 1024);
        InflateStream::with_state_and_buffer(state, buffer, Checksum::none())
    }

    /// Create a new stream for decoding deflate encoded data with a zlib header and footer
    pub fn from_zlib() -> InflateStream {
        InflateStream::with_state_and_buffer(ZlibMethodAndFlags, Vec::new(), Checksum::zlib())
    }

    /// Create a new stream for decoding deflate encoded data with a zlib header and footer
    ///
    /// This version creates a decoder that does not checksum the data to validate it with the
    /// checksum provided with the zlib wrapper.
    pub fn from_zlib_no_checksum() -> InflateStream {
        InflateStream::with_state_and_buffer(ZlibMethodAndFlags, Vec::new(), Checksum::none())
    }

    pub fn reset(&mut self) {
        self.buffer.clear();
        self.pos = 0;
        self.state = Some(Bits(BlockHeader, BitState { n: 0, v: 0 }));
        self.final_block = false;
    }

    pub fn reset_to_zlib(&mut self) {
        self.reset();
        self.state = Some(ZlibMethodAndFlags);
    }

    fn with_state_and_buffer(state: State, buffer: Vec<u8>, checksum: Checksum)
                             -> InflateStream {
        InflateStream {
            buffer: buffer,
            pos: 0,
            state: Some(state),
            final_block: false,
            checksum: checksum,
            read_checksum: None,
        }
    }

    fn run_len_dist(&mut self, len: u16, dist: u16) -> Result<Option<u16>, String> {
        debug!("RLE -{}; {} (cap={} len={})", dist, len,
               self.buffer.capacity(), self.buffer.len());
        if dist < 1 {
            return Err("invalid run length in stream".to_owned());
        }
        // `buffer_size` is used for validating `unsafe` below, handle with care
        let buffer_size = self.buffer.capacity() as u16;
        let len = if self.pos < dist {
            // Handle copying from ahead, until we hit the end reading.
            let pos_end = self.pos + len;
            let (pos_end, left) = if pos_end < dist {
                (pos_end, 0)
            } else {
                (dist, pos_end - dist)
            };
            if dist > buffer_size {
                return Err("run length distance is bigger than the window size".to_owned());
            }
            let forward = buffer_size - dist;
            if pos_end + forward > self.buffer.len() as u16 {
                return Err("invalid run length in stream".to_owned());
            }

            for i in self.pos as usize..pos_end as usize {
                self.buffer[i] = self.buffer[i + forward as usize];
            }
            self.pos = pos_end;
            left
        } else {
            len
        };
        // Handle copying from before, until we hit the end writing.
        let pos_end = self.pos + len;
        let (pos_end, left) = if pos_end <= buffer_size {
            (pos_end, None)
        } else {
            (buffer_size, Some(pos_end - buffer_size))
        };

        if self.pos < dist && pos_end > self.pos {
            return Err("invalid run length in stream".to_owned());
        }

        if self.buffer.len() < pos_end as usize {
            // ensure the buffer length will not exceed the amount of allocated memory
            assert!(pos_end <= buffer_size);
            // ensure that the uninitialized chunk of memory will be fully overwritten
            assert!(self.pos as usize <= self.buffer.len());
            unsafe {
                self.buffer.set_len(pos_end as usize);
            }
        }
        assert!(dist > 0); // validation against reading uninitialized memory
        for i in self.pos as usize..pos_end as usize {
            self.buffer[i] = self.buffer[i - dist as usize];
        }
        self.pos = pos_end;
        Ok(left)
    }

    fn next_state(&mut self, data: &[u8]) -> Result<usize, String> {
        macro_rules! ok_bytes (($n:expr, $state:expr) => ({
            self.state = Some($state);
            Ok($n)
        }));
        let debug_byte = |_i, _b| debug!("[{:04x}] {:02x}", _i, _b);
        macro_rules! push_or (($b:expr, $ret:expr) => (if self.pos < self.buffer.capacity() as u16 {
            let b = $b;
            debug_byte(self.pos, b);
            if (self.pos as usize) < self.buffer.len() {
                self.buffer[self.pos as usize] = b;
            } else {
                assert_eq!(self.pos as usize, self.buffer.len());
                self.buffer.push(b);
            }
            self.pos += 1;
        } else {
            return $ret;
        }));
        macro_rules! run_len_dist (($len:expr, $dist:expr => ($bytes:expr, $next:expr, $state:expr)) => ({
            let dist = $dist;
            let left = try!(self.run_len_dist($len, dist));
            if let Some(len) = left {
                return ok_bytes!($bytes, LenDist(($next, $state), len, dist));
            }
        }));
        match self.state.take().unwrap() {
            ZlibMethodAndFlags => {
                let b = match data.get(0) {
                    Some(&x) => x,
                    None => {
                        self.state = Some(ZlibMethodAndFlags);
                        return Ok(0);
                    }
                };
                let (method, info) = (b & 0xF, b >> 4);
                debug!("ZLIB CM=0x{:x} CINFO=0x{:x}", method, info);

                // CM = 8 (DEFLATE) is the only method defined by the ZLIB specification.
                match method {
                    8 => {/* DEFLATE */}
                    _ => return Err(format!("unknown ZLIB method CM=0x{:x}", method))
                }

                if info > 7 {
                    return Err(format!("invalid ZLIB info CINFO=0x{:x}", info));
                }

                self.buffer = Vec::with_capacity(1 << (8 + info));

                ok_bytes!(1, ZlibFlags(b))
            }
            ZlibFlags(cmf) => {
                let b = match data.get(0) {
                    Some(&x) => x,
                    None => {
                        self.state = Some(ZlibFlags(cmf));
                        return Ok(0);
                    }
                };
                let (_check, dict, _level) = (b & 0x1F, (b & 0x20) != 0, b >> 6);
                debug!("ZLIB FCHECK=0x{:x} FDICT={} FLEVEL=0x{:x}", _check, dict, _level);

                if (((cmf as u16) << 8) | b as u16) % 31 != 0 {
                    return Err(format!("invalid ZLIB checksum CMF=0x{:x} FLG=0x{:x}", cmf, b));
                }

                if dict {
                    return Err("unimplemented ZLIB FDICT=1".into());
                }

                ok_bytes!(1, Bits(BlockHeader, BitState { n: 0, v: 0 }))
            }
            Bits(next, state) => {
                let mut stream = BitStream::new(data, state);
                macro_rules! ok_state (($state:expr) => ({self.state = Some($state); Ok(stream.used)}));
                macro_rules! ok (($next:expr) => (ok_state!(Bits($next, stream.fill()))));
                macro_rules! take (
                    ($n:expr => $next:expr) => (match stream.take($n) {
                        Some(v) => v,
                        None => return ok!($next)
                    });
                    ($n:expr) => (take!($n => next))
                );
                macro_rules! take16 (
                    ($n:expr => $next:expr) => (match stream.take16($n) {
                        Some(v) => v,
                        None => return ok!($next)
                    });
                    ($n:expr) => (take16!($n => next))
                );
                macro_rules! len_dist (
                    ($len:expr, $code:expr, $bits:expr => $next_early:expr, $next:expr) => ({
                        let dist = 1 + if $bits == 0 { 0 } else { // new_base
                            2 << $bits
                        } + (($code as u16 - if $bits == 0 { 0 } else { // old_base
                            $bits * 2 + 2
                        }) << $bits) + take16!($bits => $next_early) as u16;
                        run_len_dist!($len, dist => (stream.used, $next, stream.state));
                    });
                    ($len:expr, $code:expr, $bits:expr) => (
                        len_dist!($len, $code, $bits => next, next)
                    )
                );
                match next {
                    BlockHeader => {
                        if self.final_block {
                            let (len, bytes) = stream.trailing_bytes();
                            return ok_state!(CheckCRC(len, bytes));
                        }
                        let h = take!(3);
                        let (final_, block_type) = ((h & 1) != 0, (h >> 1) & 0b11);

                        self.final_block = final_;

                        match block_type {
                            0 => {
                                // Skip to the next byte for an uncompressed block.
                                stream.align_byte();
                                ok!(BlockUncompressedLen)
                            }
                            1 => {
                                // Unwrap is safe because the data is valid.
                                let lit = DynHuffman16::new(&[
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 0-15
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 16-31
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 32-47
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 48-63
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 64-79
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 80-95
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 96-101
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 112-127
                                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, // 128-143
                                    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 144-159
                                    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 160-175
                                    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 176-191
                                    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 192-207
                                    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 208-223
                                    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 224-239
                                    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 240-255
                                    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // 256-271
                                    7, 7, 7, 7, 7, 7, 7, 7, // 272-279
                                    8, 8, 8, 8, 8, 8, 8, 8, // 280-287
                                ]).unwrap();
                                let dist = DynHuffman16::new(&[
                                    5, 5, 5, 5, 5, 5, 5, 5,
                                    5, 5, 5, 5, 5, 5, 5, 5,
                                    5, 5, 5, 5, 5, 5, 5, 5,
                                    5, 5, 5, 5, 5, 5, 5, 5
                                ]).unwrap();
                                ok!(BlockDyn(lit, dist, 0))
                            }
                            2 => ok!(BlockDynHlit),
                            _ => {
                                Err(format!("unimplemented DEFLATE block type 0b{:?}",
                                             block_type))
                            }
                        }
                    }
                    BlockUncompressedLen => {
                        let len = take16!(16);
                        ok_state!(Bits(BlockUncompressedNlen(len), stream.state))
                    }
                    BlockUncompressedNlen(len) => {
                        let nlen = take16!(16);
                        assert_eq!(stream.state.n, 0);
                        if !len != nlen {
                            return Err(format!("invalid uncompressed block len: LEN: {:04x} NLEN: {:04x}", len, nlen));
                        }
                        ok_state!(Uncompressed(len))
                    }
                    BlockDynHlit => ok!(BlockDynHdist(take!(5) + 1)),
                    BlockDynHdist(hlit) => ok!(BlockDynHclen(hlit, take!(5) + 1)),
                    BlockDynHclen(hlit, hdist) => {
                        ok!(BlockDynClenCodeLengths(hlit, hdist, take!(4) + 4, 0, Box::new([0; 19])))
                    }
                    BlockDynClenCodeLengths(hlit, hdist, hclen, i, mut clens) => {
                        let v =
                            match stream.take(3) {
                                Some(v) => v,
                                None => return ok!(BlockDynClenCodeLengths(hlit, hdist, hclen, i, clens)),
                            };
                        clens[[16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15][i as usize]] = v;
                        if i < hclen - 1 {
                            ok!(BlockDynClenCodeLengths(hlit, hdist, hclen, i + 1, clens))
                        } else {
                            ok!(BlockDynCodeLengths(try!(CodeLengthReader::new(clens, hlit as u16 + 256, hdist))))
                        }
                    }
                    BlockDynCodeLengths(mut reader) => {
                        let finished = try!(reader.read(&mut stream));
                        if finished {
                            let (lit, dist) = try!(reader.to_lit_and_dist());
                            ok!(BlockDyn(lit, dist, 0))
                        } else {
                            ok!(BlockDynCodeLengths(reader))
                        }
                    }
                    BlockDyn(huff_lit_len, huff_dist, mut prev_len) => {
                        macro_rules! next (($save_len:expr) => (BlockDyn(huff_lit_len, huff_dist, $save_len)));
                        loop {
                            let len = if prev_len != 0 {
                                let len = prev_len;
                                prev_len = 0;
                                len
                            } else {
                                let (save, code16) = match try!(huff_lit_len.read(&mut stream)) {
                                    Some(data) => data,
                                    None => return ok!(next!(0)),
                                };
                                let code = code16 as u8;
                                debug!("{:09b}", code16);
                                match code16 {
                                    0...255 => {
                                        push_or!(code, ok!({stream = save; next!(0)}));
                                        continue;
                                    }
                                    256...285 => {}
                                    _ => return Err(format!("bad DEFLATE len code {}", code)),
                                }

                                macro_rules! len (($code:expr, $bits:expr) => (
                                    3 + if $bits == 0 { 0 } else { // new_base
                                        4 << $bits
                                    } + ((if $code == 29 {
                                        256
                                    } else {
                                        $code as u16
                                    } - if $bits == 0 { 0 } else { // old_base
                                        $bits * 4 + 4
                                    } - 1) << $bits) + take!($bits => {stream = save; next!(0)}) as u16
                                ));
                                match code {
                                    0 => {
                                        return if self.final_block {
                                            let (len, bytes) = stream.trailing_bytes();
                                            ok_state!(CheckCRC(len, bytes))
                                        } else {
                                            ok!(BlockHeader)
                                        }
                                    }
                                    1...8 => len!(code, 0),
                                    9...12 => len!(code, 1),
                                    13...16 => len!(code, 2),
                                    17...20 => len!(code, 3),
                                    21...24 => len!(code, 4),
                                    25...28 => len!(code, 5),
                                    29 => len!(29, 0),
                                    _ => return Err(format!("bad DEFLATE len code {}", code as u16 + 256)),
                                }
                            };

                            let (save, dist_code) = match try!(huff_dist.read(&mut stream)) {
                                Some(data) => data,
                                None => return ok!(next!(len)),
                            };
                            debug!("  {:05b}", dist_code);
                            macro_rules! len_dist_case (($bits:expr) => (
                                len_dist!(len, dist_code, $bits => {stream = save; next!(len)}, next!(0))
                            ));
                            match dist_code {
                                0...3 => len_dist_case!(0),
                                4...5 => len_dist_case!(1),
                                6...7 => len_dist_case!(2),
                                8...9 => len_dist_case!(3),
                                10...11 => len_dist_case!(4),
                                12...13 => len_dist_case!(5),
                                14...15 => len_dist_case!(6),
                                16...17 => len_dist_case!(7),
                                18...19 => len_dist_case!(8),
                                20...21 => len_dist_case!(9),
                                22...23 => len_dist_case!(10),
                                24...25 => len_dist_case!(11),
                                26...27 => len_dist_case!(12),
                                28...29 => len_dist_case!(13),
                                _ => return Err(format!("bad DEFLATE dist code {}", dist_code)),
                            }
                        }
                    }
                }
            }
            LenDist((next, state), len, dist) => {
                run_len_dist!(len, dist => (0, next, state));
                ok_bytes!(0, Bits(next, state))
            }
            Uncompressed(mut len) => {
                for (i, &b) in data.iter().enumerate() {
                    if len == 0 {
                        return ok_bytes!(i, Bits(BlockHeader, BitState { n: 0, v: 0 }));
                    }
                    push_or!(b, ok_bytes!(i, Uncompressed(len)));
                    len -= 1;
                }
                ok_bytes!(data.len(), Uncompressed(len))
            }
            CheckCRC(mut len, mut bytes) => {
                if self.checksum.is_none() {
                    // TODO: inform caller of unused bytes
                    return ok_bytes!(0, Finished);
                }

                // Get the checksum value from the end of the stream.
                let mut used = 0;
                while len < 4 && used < data.len() {
                    bytes[len as usize] = data[used];
                    len += 1;
                    used += 1;
                }
                if len < 4 {
                    return ok_bytes!(used, CheckCRC(len, bytes));
                }

                self.read_checksum = Some(adler32_from_bytes(&bytes));
                ok_bytes!(used, Finished)
            }
            Finished => {
                // TODO: inform caller of unused bytes
                ok_bytes!(data.len(), Finished)
            }
        }
    }

    /// Try to uncompress/decode the data in `data`.
    ///
    /// On success, returns how many bytes of the input data was decompressed, and a reference to
    /// the buffer containing the decompressed data.
    ///
    /// This function may not uncompress all the provided data in one call, so it has to be called
    /// repeatedly with the data that hasn't been decompressed yet as an input until the number of
    /// bytes decoded returned is 0. (See the [top level crate documentation](index.html)
    /// for an example.)
    ///
    /// # Errors
    /// If invalid input data is encountered, a string describing what went wrong is returned.
    pub fn update<'a>(&'a mut self, mut data: &[u8]) -> Result<(usize, &'a [u8]), String> {
        let original_size = data.len();
        let original_pos = self.pos as usize;
        let mut empty = false;
        while !empty &&
              ((self.pos as usize) < self.buffer.capacity() || self.buffer.capacity() == 0) {
            // next_state must be called at least once after the data is empty.
            empty = data.is_empty();
            match self.next_state(data) {
                Ok(n) => {
                    data = &data[n..];
                }
                Err(m) => return Err(m),
            }
        }
        let output = &self.buffer[original_pos..self.pos as usize];
        if self.pos as usize >= self.buffer.capacity() {
            self.pos = 0;
        }

        // Update the checksum..
        self.checksum.update(output);
        // and validate if we are done decoding.
        if let Some(c) = self.read_checksum {
            try!(self.checksum.check(c));
        }

        Ok((original_size - data.len(), output))
    }

    /// Returns the calculated checksum value of the currently decoded data.
    ///
    /// Will return 0 for cases where the checksum is not validated.
    pub fn current_checksum(&self) -> u32 {
        self.checksum.current_value()
    }
}
