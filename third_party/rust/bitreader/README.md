# BitReader

BitReader is a helper type to extract strings of bits from a slice of bytes.

[![Published Package](https://img.shields.io/crates/v/bitreader.svg)](https://crates.io/crates/bitreader)
[![Documentation](https://docs.rs/bitreader/badge.svg)](https://docs.rs/bitreader)
[![Build Status](https://travis-ci.org/irauta/bitreader.svg?branch=master)](https://travis-ci.org/irauta/bitreader)

Here is how you read first a single bit, then three bits and finally four bits from a byte buffer:

    use bitreader::BitReader;

    let slice_of_u8 = &[0b1000_1111];
    let mut reader = BitReader::new(slice_of_u8);

    // You obviously should use try! or some other error handling mechanism here
    let a_single_bit = reader.read_u8(1).unwrap(); // 1
    let more_bits = reader.read_u8(3).unwrap(); // 0
    let last_bits_of_byte = reader.read_u8(4).unwrap(); // 0b1111

You can naturally read bits from longer buffer of data than just a single byte.

As you read bits, the internal cursor of BitReader moves on along the stream of bits. Big endian format is assumed when reading the multi-byte values. BitReader supports reading maximum of 64 bits at a time (with read_u64).

## License

Licensed under the Apache License, Version 2.0 or the MIT license, at your option.
