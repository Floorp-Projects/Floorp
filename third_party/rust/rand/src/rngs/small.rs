// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A small fast RNG

use {RngCore, SeedableRng, Error};

#[cfg(all(all(rustc_1_26, not(target_os = "emscripten")), target_pointer_width = "64"))]
type Rng = ::rand_pcg::Pcg64Mcg;
#[cfg(not(all(all(rustc_1_26, not(target_os = "emscripten")), target_pointer_width = "64")))]
type Rng = ::rand_pcg::Pcg32;

/// An RNG recommended when small state, cheap initialization and good
/// performance are required. The PRNG algorithm in `SmallRng` is chosen to be
/// efficient on the current platform, **without consideration for cryptography
/// or security**. The size of its state is much smaller than for [`StdRng`].
///
/// Reproducibility of output from this generator is however not required, thus
/// future library versions may use a different internal generator with
/// different output. Further, this generator may not be portable and can
/// produce different output depending on the architecture. If you require
/// reproducible output, use a named RNG.
/// Refer to [The Book](https://rust-random.github.io/book/guide-rngs.html).
/// 
///
/// The current algorithm is [`Pcg64Mcg`][rand_pcg::Pcg64Mcg] on 64-bit platforms with Rust version
/// 1.26 and later, or [`Pcg32`][rand_pcg::Pcg32] otherwise. Both are found in
/// the [rand_pcg] crate.
///
/// # Examples
///
/// Initializing `SmallRng` with a random seed can be done using [`FromEntropy`]:
///
/// ```
/// # use rand::Rng;
/// use rand::FromEntropy;
/// use rand::rngs::SmallRng;
///
/// // Create small, cheap to initialize and fast RNG with a random seed.
/// // The randomness is supplied by the operating system.
/// let mut small_rng = SmallRng::from_entropy();
/// # let v: u32 = small_rng.gen();
/// ```
///
/// When initializing a lot of `SmallRng`'s, using [`thread_rng`] can be more
/// efficient:
///
/// ```
/// use std::iter;
/// use rand::{SeedableRng, thread_rng};
/// use rand::rngs::SmallRng;
///
/// // Create a big, expensive to initialize and slower, but unpredictable RNG.
/// // This is cached and done only once per thread.
/// let mut thread_rng = thread_rng();
/// // Create small, cheap to initialize and fast RNGs with random seeds.
/// // One can generally assume this won't fail.
/// let rngs: Vec<SmallRng> = iter::repeat(())
///     .map(|()| SmallRng::from_rng(&mut thread_rng).unwrap())
///     .take(10)
///     .collect();
/// ```
///
/// [`FromEntropy`]: crate::FromEntropy
/// [`StdRng`]: crate::rngs::StdRng
/// [`thread_rng`]: crate::thread_rng
/// [rand_pcg]: https://crates.io/crates/rand_pcg
#[derive(Clone, Debug)]
pub struct SmallRng(Rng);

impl RngCore for SmallRng {
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

impl SeedableRng for SmallRng {
    type Seed = <Rng as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        SmallRng(Rng::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        Rng::from_rng(rng).map(SmallRng)
    }
}
