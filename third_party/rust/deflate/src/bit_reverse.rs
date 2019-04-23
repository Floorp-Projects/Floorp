/// Reverse the first length bits of n.
/// (Passing more than 16 as length will produce garbage.
pub fn reverse_bits(mut n: u16, length: u8) -> u16 {
    debug_assert!(length <= 16);
    // Borrowed from http://aggregate.org/MAGIC/#Bit%20Reversal
    n = ((n & 0xaaaa) >> 1) | ((n & 0x5555) << 1);
    n = ((n & 0xcccc) >> 2) | ((n & 0x3333) << 2);
    n = ((n & 0xf0f0) >> 4) | ((n & 0x0f0f) << 4);
    n = ((n & 0xff00) >> 8) | ((n & 0x00ff) << 8);
    n >> (16 - length)
}

#[cfg(test)]
mod test {
    use super::reverse_bits;
    #[test]
    fn test_bit_reverse() {
        assert_eq!(reverse_bits(0b0111_0100, 8), 0b0010_1110);
        assert_eq!(
            reverse_bits(0b1100_1100_1100_1100, 16),
            0b0011_0011_0011_0011
        );
        // Check that we ignore >16 length
        // We no longer check for this.
        // assert_eq!(reverse_bits(0b11001100_11001100, 32), 0b0011001100110011);
    }
}
