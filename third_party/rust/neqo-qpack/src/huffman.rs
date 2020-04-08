// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::pedantic)]

use crate::huffman_decode_helper::{HuffmanDecodeTable, HUFFMAN_DECODE_ROOT};
use crate::huffman_table::HUFFMAN_TABLE;
use crate::{Error, Res};
use std::convert::TryFrom;

#[derive(Default)]
pub struct Huffman {
    // we read whole bytes from an input and stored them in incoming_bytes.
    // Some bits will be transfer to decoding_byte and incoming_bits_left is number of bits still
    // left in incoming_bytes.
    input_byte: u8,
    input_bits_left: u8,
    // byte used for decoding
    decoding_byte: u8,
    // bits left in decoding_byte that are not decoded yet.
    decoding_bits_left: u8,
}

impl Huffman {
    pub fn decode(&mut self, input: &[u8]) -> Res<Vec<u8>> {
        let mut output: Vec<u8> = Vec::new();
        let mut read: usize = 0; // bytes read from the input.
        let len = input.len();

        while self.has_more_data(len, read) {
            if let Some(c) =
                self.decode_huffman_character(HUFFMAN_DECODE_ROOT, input, len, &mut read)?
            {
                output.push(c);
            }
        }

        if self.decoding_bits_left > 7 {
            return Err(Error::DecompressionFailed);
        }
        if self.decoding_bits_left > 0 {
            let mask: u8 = ((1 << self.decoding_bits_left) - 1) << (8 - self.decoding_bits_left);
            let bits: u8 = self.decoding_byte & mask;
            if bits != mask {
                return Err(Error::DecompressionFailed);
            }
        }
        Ok(output)
    }

    fn has_more_data(&self, len: usize, read: usize) -> bool {
        len > read || self.input_bits_left > 0
    }

    fn extract_byte(&mut self, input: &[u8], len: usize, read: &mut usize) {
        // if self.decoding_bits_left > 0 the 'left' bits will be in proper place and the rest will be 0.
        // for self.decoding_bits_left == 0 we need to do it here.
        if self.decoding_bits_left == 0 {
            self.decoding_byte = 0x00;
        }

        let from_current = std::cmp::min(8 - self.decoding_bits_left, self.input_bits_left);
        if from_current > 0 {
            let mask = (1 << from_current) - 1;
            let bits = (self.input_byte >> (self.input_bits_left - from_current)) & mask;
            self.decoding_byte |= bits << (8 - self.decoding_bits_left - from_current);
            self.decoding_bits_left += from_current;
            self.input_bits_left -= from_current;
        }
        if self.decoding_bits_left < 8 && *read < len {
            // get bits from the next byte.
            self.input_byte = input[*read];
            *read += 1;
            self.decoding_byte |= self.input_byte >> self.decoding_bits_left;
            self.input_bits_left = self.decoding_bits_left;
            self.decoding_bits_left = 8;
        }
    }

    fn decode_huffman_character(
        &mut self,
        table: &HuffmanDecodeTable,
        input: &[u8],
        len: usize,
        read: &mut usize,
    ) -> Res<Option<u8>> {
        self.extract_byte(input, len, read);

        if table.index_has_a_next_table(self.decoding_byte) {
            if !self.has_more_data(len, *read) {
                // This is the last bit and it is padding.
                return Ok(None);
            }

            self.decoding_bits_left = 0;
            return self.decode_huffman_character(
                table.next_table(self.decoding_byte),
                input,
                len,
                read,
            );
        }

        let entry = table.entry(self.decoding_byte);
        if entry.val == 256 {
            return Err(Error::DecompressionFailed);
        }

        if entry.prefix_len > self.decoding_bits_left {
            assert!(!self.has_more_data(len, *read));
            // This is the last bit and it is padding.
            return Ok(None);
        }
        let c = u8::try_from(entry.val).unwrap();

        self.decoding_bits_left -= entry.prefix_len;
        if self.decoding_bits_left > 0 {
            self.decoding_byte <<= entry.prefix_len;
        }
        Ok(Some(c))
    }
}

#[must_use]
pub fn encode_huffman(input: &[u8]) -> Vec<u8> {
    let mut output: Vec<u8> = Vec::new();
    let mut left: u8 = 8;
    let mut saved: u8 = 0;
    for c in input {
        let mut e = HUFFMAN_TABLE[*c as usize];

        // Fill the previous byte
        if e.len < left {
            let b = u8::try_from(e.val & 0xFF).unwrap();
            saved |= b << (left - e.len);
            left -= e.len;
            e.len = 0;
        } else {
            let v: u8 = (e.val >> (e.len - left)) as u8;
            saved |= v;
            output.push(saved);
            e.len -= left;
            left = 8;
            saved = 0;
        }

        while e.len >= 8 {
            let v: u8 = (e.val >> (e.len - 8)) as u8;
            output.push(v);
            e.len -= 8;
        }

        if e.len > 0 {
            saved = ((e.val & ((1 << e.len) - 1)) as u8) << (8 - e.len);
            left = 8 - e.len;
        }
    }

    if left < 8 {
        let v: u8 = (1 << left) - 1;
        saved |= v;
        output.push(saved);
    }

    output
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestElement {
        pub val: &'static [u8],
        pub res: &'static [u8],
    }
    const TEST_CASES: &[TestElement] = &[
        TestElement {
            val: b"www.example.com",
            res: &[
                0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff,
            ],
        },
        TestElement {
            val: b"no-cache",
            res: &[0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf],
        },
        TestElement {
            val: b"custom-key",
            res: &[0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f],
        },
        TestElement {
            val: b"custom-value",
            res: &[0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf],
        },
        TestElement {
            val: b"private",
            res: &[0xae, 0xc3, 0x77, 0x1a, 0x4b],
        },
        TestElement {
            val: b"Mon, 21 Oct 2013 20:13:21 GMT",
            res: &[
                0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b,
                0x81, 0x66, 0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff,
            ],
        },
        TestElement {
            val: b"https://www.example.com",
            res: &[
                0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f, 0x0b, 0x97, 0xc8, 0xe9, 0xae, 0x82,
                0xae, 0x43, 0xd3,
            ],
        },
        TestElement {
            val: b"Mon, 21 Oct 2013 20:13:22 GMT",
            res: &[
                0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b,
                0x81, 0x66, 0xe0, 0x84, 0xa6, 0x2d, 0x1b, 0xff,
            ],
        },
        TestElement {
            val: b"gzip",
            res: &[0x9b, 0xd9, 0xab],
        },
        TestElement {
            val: b"foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1",
            res: &[
                0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2, 0xe6, 0xc7, 0xb3, 0x35, 0xdf, 0xdf, 0xcd, 0x5b,
                0x39, 0x60, 0xd5, 0xaf, 0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27, 0x0f, 0xb5,
                0x29, 0x1f, 0x95, 0x87, 0x31, 0x60, 0x65, 0xc0, 0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06,
                0x3d, 0x50, 0x07,
            ],
        },
        TestElement {
            val: b"<?\\ >",
            res: &[0xff, 0xf9, 0xfe, 0x7f, 0xff, 0x05, 0x3f, 0xef],
        },
    ];

    #[test]
    fn test_encoder() {
        for e in TEST_CASES {
            let out = encode_huffman(e.val);
            assert_eq!(out[..], *e.res);
        }
    }

    #[test]
    fn test_decoder() {
        for e in TEST_CASES {
            let res = Huffman::default().decode(e.res);
            assert!(res.is_ok());
            assert_eq!(res.unwrap()[..], *e.val);
        }
    }
}
