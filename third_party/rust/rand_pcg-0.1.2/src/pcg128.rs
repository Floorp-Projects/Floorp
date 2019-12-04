// Copyright 2018 Developers of the Rand project.
// Copyright 2017 Paul Dicker.
// Copyright 2014-2017 Melissa O'Neill and PCG Project contributors
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! PCG random number generators

// This is the default multiplier used by PCG for 64-bit state.
const MULTIPLIER: u128 = 0x2360_ED05_1FC6_5DA4_4385_DF64_9FCC_F645;

use core::fmt;
use core::mem::transmute;
use rand_core::{RngCore, SeedableRng, Error, le};

/// A PCG random number generator (XSL 128/64 (MCG) variant).
///
/// Permuted Congruential Generator with 128-bit state, internal Multiplicative
/// Congruential Generator, and 64-bit output via "xorshift low (bits),
/// random rotation" output function.
///
/// This is a 128-bit MCG with the PCG-XSL-RR output function.
/// Note that compared to the standard `pcg64` (128-bit LCG with PCG-XSL-RR
/// output function), this RNG is faster, also has a long cycle, and still has
/// good performance on statistical tests.
///
/// Note: this RNG is only available using Rust 1.26 or later.
#[derive(Clone)]
#[cfg_attr(feature="serde1", derive(Serialize,Deserialize))]
pub struct Mcg128Xsl64 {
    state: u128,
}

/// A friendly name for `Mcg128Xsl64`.
pub type Pcg64Mcg = Mcg128Xsl64;

impl Mcg128Xsl64 {
    /// Construct an instance compatible with PCG seed.
    ///
    /// Note that PCG specifies a default value for the parameter:
    ///
    /// - `state = 0xcafef00dd15ea5e5`
    pub fn new(state: u128) -> Self {
        // Force low bit to 1, as in C version (C++ uses `state | 3` instead).
        Mcg128Xsl64 { state: state | 1 }
    }
}

// Custom Debug implementation that does not expose the internal state
impl fmt::Debug for Mcg128Xsl64 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Mcg128Xsl64 {{}}")
    }
}

/// We use a single 126-bit seed to initialise the state and select a stream.
/// Two `seed` bits (lowest order of last byte) are ignored.
impl SeedableRng for Mcg128Xsl64 {
    type Seed = [u8; 16];

    fn from_seed(seed: Self::Seed) -> Self {
        // Read as if a little-endian u128 value:
        let mut seed_u64 = [0u64; 2];
        le::read_u64_into(&seed, &mut seed_u64);
        let state = (seed_u64[0] as u128) |
                    (seed_u64[1] as u128) << 64;
        Mcg128Xsl64::new(state)
    }
}

impl RngCore for Mcg128Xsl64 {
    #[inline]
    fn next_u32(&mut self) -> u32 {
        self.next_u64() as u32
    }

    #[inline]
    fn next_u64(&mut self) -> u64 {
        // prepare the LCG for the next round
        let state = self.state.wrapping_mul(MULTIPLIER);
        self.state = state;

        // Output function XSL RR ("xorshift low (bits), random rotation")
        // Constants are for 128-bit state, 64-bit output
        const XSHIFT: u32 = 64;     // (128 - 64 + 64) / 2
        const ROTATE: u32 = 122;    // 128 - 6

        let rot = (state >> ROTATE) as u32;
        let xsl = ((state >> XSHIFT) as u64) ^ (state as u64);
        xsl.rotate_right(rot)
    }

    #[inline]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        // specialisation of impls::fill_bytes_via_next; approx 3x faster
        let mut left = dest;
        while left.len() >= 8 {
            let (l, r) = {left}.split_at_mut(8);
            left = r;
            let chunk: [u8; 8] = unsafe {
                transmute(self.next_u64().to_le())
            };
            l.copy_from_slice(&chunk);
        }
        let n = left.len();
        if n > 0 {
            let chunk: [u8; 8] = unsafe {
                transmute(self.next_u64().to_le())
            };
            left.copy_from_slice(&chunk[..n]);
        }
    }

    #[inline]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        Ok(self.fill_bytes(dest))
    }
}
