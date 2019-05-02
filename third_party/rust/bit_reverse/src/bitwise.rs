use super::core;

/// Computes bit reversal by going bit by bit and setting the reverse position bit for the output.
pub trait BitwiseReverse<T> {
    /// Swaps the bits such that bit i is now bit N-i, where N is the length of the T in bits.
    fn swap_bits(self) -> T;
}

macro_rules! doit_bitwise { ($($ty:ty),*) => ($(
    impl BitwiseReverse<$ty> for $ty {
        // This algorithm uses the reverse variable as a like a stack to reverse the value.
        // The lesser significant bits are pushed onto the reverse variable and then the variable
        // is shifted down to make room for more significant bits. This algorithm has a shortcut,
        // that if there aren't anymore 1s to push onto the reverse variable the algorithm ends
        // early and shift the reverse to the correct position.
        #[inline]
        fn swap_bits(self) -> $ty {
            let mut v = self;

            // By initializing the reversal to value, we have already loaded the largest
            // significant bit into the correct location.
            let mut r = self;

            // Compute how many bits are left to shift at the end of the algorithm.
            let mut s = 8 * core::mem::size_of::<$ty>() - 1;

            v >>= 1;
            while v != 0 {  // Quit early if there are no more 1s to shift in
                r <<= 1;    // Make room for the next significant bit
                r |= v & 1; // Add the bit to the reverse variable
                v >>= 1;    // Go to the next significant bit
                s -= 1;     // Decrement the leftover bit count
            }

            // Shift the reversal to the correct position and return the reversal
            return r << s;
        }
    })*)
}

doit_bitwise!(u8, u16, u32, u64, usize);
doit_signed!(BitwiseReverse);
test_suite!();
