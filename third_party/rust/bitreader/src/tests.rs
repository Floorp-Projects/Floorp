// Copyright 2015 Ilkka Rauta
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use super::*;

#[test]
fn read_buffer() {
    let bytes = &[
        0b1011_0101, 0b0110_1010, 0b1010_1100, 0b1001_1001,
        0b1001_1001, 0b1001_1001, 0b1001_1001, 0b1110_0111,
    ];

    let mut reader = BitReader::new(bytes);

    assert_eq!(reader.read_u8(1).unwrap(), 0b1);
    assert_eq!(reader.read_u8(1).unwrap(), 0b0);
    assert_eq!(reader.read_u8(2).unwrap(), 0b11);

    assert_eq!(reader.read_u8(4).unwrap(), 0b0101);

    assert!(reader.is_aligned(1));

    assert_eq!(reader.read_u8(3).unwrap(), 0b11);
    assert_eq!(reader.read_u16(10).unwrap(), 0b01_0101_0101);
    assert_eq!(reader.read_u8(3).unwrap(), 0b100);

    assert!(reader.is_aligned(1));

    assert_eq!(reader.read_u32(32).unwrap(), 0b1001_1001_1001_1001_1001_1001_1001_1001);

    assert_eq!(reader.read_u8(4).unwrap(), 0b1110);
    assert_eq!(reader.read_u8(3).unwrap(), 0b011);
    assert_eq!(reader.read_u8(1).unwrap(), 0b1);

    // Could also be 8 at this point!
    assert!(reader.is_aligned(4));
}

#[test]
fn try_all_sizes() {
    let bytes = &[
        0x4a, 0x1e, 0x39, 0xbb, 0xd0, 0x07, 0xca, 0x9a,
        0xa6, 0xba, 0x25, 0x52, 0x6f, 0x0a, 0x6a, 0xba,
    ];

    let mut reader = BitReader::new(bytes);
    assert_eq!(reader.read_u64(64).unwrap(), 0x4a1e39bbd007ca9a);
    assert_eq!(reader.read_u64(64).unwrap(), 0xa6ba25526f0a6aba);

    let mut reader = BitReader::new(bytes);
    assert_eq!(reader.read_u32(32).unwrap(), 0x4a1e39bb);
    assert_eq!(reader.read_u32(32).unwrap(), 0xd007ca9a);
    assert_eq!(reader.read_u32(32).unwrap(), 0xa6ba2552);
    assert_eq!(reader.read_u32(32).unwrap(), 0x6f0a6aba);

    let mut reader = BitReader::new(bytes);
    assert_eq!(reader.read_u16(16).unwrap(), 0x4a1e);
    assert_eq!(reader.read_u16(16).unwrap(), 0x39bb);
    assert_eq!(reader.read_u16(16).unwrap(), 0xd007);
    assert_eq!(reader.read_u16(16).unwrap(), 0xca9a);
    assert_eq!(reader.read_u16(16).unwrap(), 0xa6ba);
    assert_eq!(reader.read_u16(16).unwrap(), 0x2552);
    assert_eq!(reader.read_u16(16).unwrap(), 0x6f0a);
    assert_eq!(reader.read_u16(16).unwrap(), 0x6aba);

    let mut reader = BitReader::new(&bytes[..]);
    for byte in bytes {
        assert_eq!(reader.read_u8(8).unwrap(), *byte);
    }
}

#[test]
fn skipping_and_zero_reads() {
    let bytes = &[
        0b1011_0101, 0b1110_1010, 0b1010_1100,
    ];

    let mut reader = BitReader::new(bytes);

    assert_eq!(reader.read_u8(4).unwrap(), 0b1011);
    // Reading zero bits should be a no-op
    assert_eq!(reader.read_u8(0).unwrap(), 0b0);
    assert_eq!(reader.read_u8(4).unwrap(), 0b0101);
    reader.skip(3).unwrap(); // 0b111
    assert_eq!(reader.read_u16(10).unwrap(), 0b0101010101);
    assert_eq!(reader.read_u8(3).unwrap(), 0b100);
}

#[test]
fn errors() {
    let bytes = &[
        0b1011_0101, 0b1110_1010, 0b1010_1100,
    ];

    let mut reader = BitReader::new(bytes);
    assert_eq!(reader.read_u8(4).unwrap(), 0b1011);
    assert_eq!(reader.read_u8(9).unwrap_err(), BitReaderError::TooManyBitsForType {
        position: 4,
        requested: 9,
        allowed: 8
    });
    // If an error happens, it should be possible to resume as if nothing had happened
    assert_eq!(reader.read_u8(4).unwrap(), 0b0101);

    let mut reader = BitReader::new(bytes);
    assert_eq!(reader.read_u8(4).unwrap(), 0b1011);
    // Same with this error
    assert_eq!(reader.read_u32(21).unwrap_err(), BitReaderError::NotEnoughData {
        position: 4,
        length: (bytes.len() * 8) as u64,
        requested: 21
    });
    assert_eq!(reader.read_u8(4).unwrap(), 0b0101);
}

#[test]
fn signed_values() {
    let from = -2048;
    let to = 2048;
    for x in from..to {
        let bytes = &[
            (x >> 8) as u8,
            x as u8,
        ];
        let mut reader = BitReader::new(bytes);
        assert_eq!(reader.read_u8(4).unwrap(), if x < 0 { 0b1111 } else { 0 });
        assert_eq!(reader.read_i16(12).unwrap(), x);
    }
}
