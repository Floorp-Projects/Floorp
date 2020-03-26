// Copyright 2015 Ilkka Rauta
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

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

    assert_eq!(reader.position(), 4);
    assert_eq!(reader.remaining(), 60);

    assert_eq!(reader.read_u8(4).unwrap(), 0b0101);

    assert!(reader.is_aligned(1));

    assert_eq!(reader.read_u8(3).unwrap(), 0b11);
    assert_eq!(reader.read_u16(10).unwrap(), 0b01_0101_0101);
    assert_eq!(reader.read_u8(3).unwrap(), 0b100);

    assert_eq!(reader.position(), 24);
    assert_eq!(reader.remaining(), 40);

    assert!(reader.is_aligned(1));

    assert_eq!(reader.read_u32(32).unwrap(), 0b1001_1001_1001_1001_1001_1001_1001_1001);

    assert_eq!(reader.read_u8(4).unwrap(), 0b1110);
    assert_eq!(reader.read_u8(3).unwrap(), 0b011);
    assert_eq!(reader.read_bool().unwrap(), true);

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
        0b1011_0101, 0b1110_1010, 0b1010_1100, 0b0011_0101,
    ];

    let mut reader = BitReader::new(bytes);

    assert_eq!(reader.read_u8(4).unwrap(), 0b1011);
    // Reading zero bits should be a no-op
    assert_eq!(reader.read_u8(0).unwrap(), 0b0);
    assert_eq!(reader.read_u8(4).unwrap(), 0b0101);
    reader.skip(3).unwrap(); // 0b111
    assert_eq!(reader.read_u16(10).unwrap(), 0b0101010101);
    assert_eq!(reader.read_u8(3).unwrap(), 0b100);
    reader.skip(4).unwrap(); // 0b0011
    assert_eq!(reader.read_u32(2).unwrap(), 0b01);
    assert_eq!(reader.read_bool().unwrap(), false);
    assert_eq!(reader.read_bool().unwrap(), true);
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

#[test]
fn boolean_values() {
    let bytes: Vec<u8> = (0..16).collect();
    let mut reader = BitReader::new(&bytes);
    for v in &bytes {
        assert_eq!(reader.read_bool().unwrap(), false);
        reader.skip(3).unwrap();
        assert_eq!(reader.read_bool().unwrap(), v & 0x08 == 8);
        assert_eq!(reader.read_bool().unwrap(), v & 0x04 == 4);
        assert_eq!(reader.read_bool().unwrap(), v & 0x02 == 2);
        assert_eq!(reader.read_bool().unwrap(), v & 0x01 == 1);
    }
}

#[test]
fn read_slice() {
    let bytes = &[
        0b1111_0000, 0b0000_1111, 0b1111_0000,
        0b0000_1000, 0b0000_0100, 0b0000_0011,
        0b1111_1100, 0b0000_0011, 0b1101_1000,
    ];
    let mut reader = BitReader::new(bytes);
    assert_eq!(reader.read_u8(4).unwrap(), 0b1111);
    // Just some pattern that's definitely not in the bytes array
    let mut output = [0b1010_1101; 3];
    reader.read_u8_slice(&mut output).unwrap();
    assert_eq!(&output, &[0u8, 255u8, 0u8]);

    assert_eq!(reader.read_u8(1).unwrap(), 1);

    reader.read_u8_slice(&mut output[1..2]).unwrap();
    assert_eq!(&output, &[0u8, 0u8, 0u8]);

    assert_eq!(reader.read_u8(1).unwrap(), 1);

    output = [0b1010_1101; 3];
    reader.read_u8_slice(&mut output).unwrap();
    assert_eq!(&output, &[0u8, 255u8, 0u8]);

    reader.read_u8_slice(&mut output[0..1]).unwrap();
    assert_eq!(output[0], 0b1111_0110);

    assert_eq!(reader.read_u8(2).unwrap(), 0);
}

#[test]
fn read_slice_too_much() {
    let bytes = &[
        0b1111_1111, 0b1111_1111, 0b1111_1111, 0b1111_1111,
    ];
    let mut reader = BitReader::new(bytes);
    assert_eq!(reader.read_u8(1).unwrap(), 1);

    let mut output = [0u8; 4];
    let should_be_error = reader.read_u8_slice(&mut output);
    assert_eq!(should_be_error.unwrap_err(), BitReaderError::NotEnoughData {
        position: 1,
        length: (bytes.len() * 8) as u64,
        requested: (&output.len() * 8) as u64
    });
    assert_eq!(&output, &[0u8; 4]);
}
