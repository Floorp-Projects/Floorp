// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_common::Decoder;
use std::convert::TryFrom;
use std::ops::Range;

/// An implementation of a xoshiro256** pseudorandom generator.
pub struct Random {
    state: [u64; 4],
}

impl Random {
    pub fn new(seed: [u8; 32]) -> Self {
        assert!(seed.iter().any(|&x| x != 0));
        let mut dec = Decoder::from(&seed);
        Self {
            state: [
                dec.decode_uint(8).unwrap(),
                dec.decode_uint(8).unwrap(),
                dec.decode_uint(8).unwrap(),
                dec.decode_uint(8).unwrap(),
            ],
        }
    }

    pub fn random(&mut self) -> u64 {
        let result = (self.state[1].overflowing_mul(5).0)
            .rotate_right(7)
            .overflowing_mul(9)
            .0;
        let t = self.state[1] << 17;

        self.state[2] ^= self.state[0];
        self.state[3] ^= self.state[1];
        self.state[1] ^= self.state[2];
        self.state[0] ^= self.state[3];

        self.state[2] ^= t;
        self.state[3] = self.state[3].rotate_right(45);

        result
    }

    /// Generate a random value from the range.
    /// If the range is empty or inverted (`range.start > range.end`), then
    /// this returns the value of `range.start` without generating any random values.
    pub fn random_from(&mut self, range: Range<u64>) -> u64 {
        let max = range.end.saturating_sub(range.start);
        if max == 0 {
            return range.start;
        }

        let shift = (max - 1).leading_zeros();
        assert_ne!(max, 0);
        loop {
            let r = self.random() >> shift;
            if r < max {
                return range.start + r;
            }
        }
    }

    /// Get the seed necessary to continue from this point.
    pub fn seed_str(&self) -> String {
        format!(
            "{:8x}{:8x}{:8x}{:8x}",
            self.state[0], self.state[1], self.state[2], self.state[3],
        )
    }
}

impl Default for Random {
    fn default() -> Self {
        let buf = neqo_crypto::random(32);
        Random::new(<[u8; 32]>::try_from(&buf[..]).unwrap())
    }
}
