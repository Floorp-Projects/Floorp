# BitReader

BitReader is a helper type to extract strings of bits from a slice of bytes.

Here is how you read first a single bit, then three bits and finally four bits from a byte buffer:

    use bitreader::BitReader;

    let slice_of_u8 = &[0b1000_1111];
    let mut reader = BitReader::new(slice_of_u8);

    // You obviously should use try! or some other error handling mechanism here
    let a_single_bit = reader.read_u8(1).unwrap(); // 1
    let more_bits = reader.read_u8(3).unwrap(); // 0
    let last_bits_of_byte = reader.read_u8(4).unwrap(); // 0b1111

You can naturally read bits from longer buffer of data than just a single byte.

As you read bits, the internal cursor of BitReader moves on along the stream of bits. Little endian format is assumed when reading the multi-byte values. BitReader supports reading maximum of 64 bits at a time (with read_u64). Reading signed values directly is not supported at the moment.
