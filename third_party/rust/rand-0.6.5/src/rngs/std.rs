// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The standard RNG

use {RngCore, CryptoRng, Error, SeedableRng};
use rand_hc::Hc128Rng;

/// The standard RNG. The PRNG algorithm in `StdRng` is chosen to be efficient
/// on the current platform, to be statistically strong and unpredictable
/// (meaning a cryptographically secure PRNG).
///
/// The current algorithm used on all platforms is [HC-128], found in the
/// [rand_hc] crate.
///
/// Reproducibility of output from this generator is however not required, thus
/// future library versions may use a different internal generator with
/// different output. Further, this generator may not be portable and can
/// produce different output depending on the architecture. If you require
/// reproducible output, use a named RNG, for example [`ChaChaRng`] from the
/// [rand_chacha] crate.
///
/// [HC-128]: rand_hc::Hc128Rng
/// [`ChaChaRng`]: rand_chacha::ChaChaRng
/// [rand_hc]: https://crates.io/crates/rand_hc
/// [rand_chacha]: https://crates.io/crates/rand_chacha
#[derive(Clone, Debug)]
pub struct StdRng(Hc128Rng);

impl RngCore for StdRng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest);
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for StdRng {
    type Seed = <Hc128Rng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        StdRng(Hc128Rng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        Hc128Rng::from_rng(rng).map(StdRng)
    }
}

impl CryptoRng for StdRng {}


#[cfg(test)]
mod test {
    use {RngCore, SeedableRng};
    use rngs::StdRng;

    #[test]
    fn test_stdrng_construction() {
        let seed = [1,0,0,0, 23,0,0,0, 200,1,0,0, 210,30,0,0,
                    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
        let mut rng1 = StdRng::from_seed(seed);
        assert_eq!(rng1.next_u64(), 15759097995037006553);

        let mut rng2 = StdRng::from_rng(rng1).unwrap();
        assert_eq!(rng2.next_u64(), 6766915756997287454);
    }
}
