/// Computes bit reversal by using a divide and conquer approach. Pairs of bits are swapped.
/// Then neighboring bit pairs are swapped. Each time swapping the next largest group of bits.
/// This is done until the entire data has been bit reversed.
pub trait ParallelReverse<T> {
    /// Swaps the bits such that bit i is now bit N-i, where N is the length of the T in bits.
    fn swap_bits(self) -> T;
}

macro_rules! doit_parallel { ($($ty:ty),*) => ($(
    impl ParallelReverse<$ty> for $ty {
        #[inline]
        fn swap_bits(self) -> $ty {
            let mut v = self;
            // Swap odd and even bits
            v = ((v >> 1) & (0x5555555555555555 as $ty)) | ((v & (0x5555555555555555 as $ty)) << 1);
            // Swap consecutive pairs
            v = ((v >> 2) & (0x3333333333333333 as $ty)) | ((v & (0x3333333333333333 as $ty)) << 2);
            // Swap nibbles
            v = ((v >> 4) & (0x0F0F0F0F0F0F0F0F as $ty)) | ((v & (0x0F0F0F0F0F0F0F0F as $ty)) << 4);

            return v.swap_bytes();
        }
    })*)
}

doit_parallel!(u8, u16, u32, u64, usize);
doit_signed!(ParallelReverse);
test_suite!();
