// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The xorshift random number generator.

#![doc(html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk.png",
       html_favicon_url = "https://www.rust-lang.org/favicon.ico",
       html_root_url = "https://rust-random.github.io/rand/")]

#![deny(missing_docs)]
#![deny(missing_debug_implementations)]

#![no_std]

extern crate rand_core;

#[cfg(feature="serde1")] extern crate serde;
#[cfg(feature="serde1")] #[macro_use] extern crate serde_derive;

use core::num::Wrapping as w;
use core::{fmt, slice};
use rand_core::{RngCore, SeedableRng, Error, impls, le};

/// An Xorshift random number generator.
///
/// The Xorshift[^1] algorithm is not suitable for cryptographic purposes
/// but is very fast. If you do not know for sure that it fits your
/// requirements, use a more secure one such as `StdRng` or `OsRng`.
///
/// [^1]: Marsaglia, George (July 2003).
///       ["Xorshift RNGs"](https://www.jstatsoft.org/v08/i14/paper).
///       *Journal of Statistical Software*. Vol. 8 (Issue 14).
#[derive(Clone)]
#[cfg_attr(feature="serde1", derive(Serialize,Deserialize))]
pub struct XorShiftRng {
    x: w<u32>,
    y: w<u32>,
    z: w<u32>,
    w: w<u32>,
}

// Custom Debug implementation that does not expose the internal state
impl fmt::Debug for XorShiftRng {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "XorShiftRng {{}}")
    }
}

impl RngCore for XorShiftRng {
    #[inline]
    fn next_u32(&mut self) -> u32 {
        let x = self.x;
        let t = x ^ (x << 11);
        self.x = self.y;
        self.y = self.z;
        self.z = self.w;
        let w_ = self.w;
        self.w = w_ ^ (w_ >> 19) ^ (t ^ (t >> 8));
        self.w.0
    }

    #[inline]
    fn next_u64(&mut self) -> u64 {
        impls::next_u64_via_u32(self)
    }

    #[inline]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        impls::fill_bytes_via_next(self, dest)
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        Ok(self.fill_bytes(dest))
    }
}

impl SeedableRng for XorShiftRng {
    type Seed = [u8; 16];

    fn from_seed(seed: Self::Seed) -> Self {
        let mut seed_u32 = [0u32; 4];
        le::read_u32_into(&seed, &mut seed_u32);

        // Xorshift cannot be seeded with 0 and we cannot return an Error, but
        // also do not wish to panic (because a random seed can legitimately be
        // 0); our only option is therefore to use a preset value.
        if seed_u32.iter().all(|&x| x == 0) {
            seed_u32 = [0xBAD_5EED, 0xBAD_5EED, 0xBAD_5EED, 0xBAD_5EED];
        }

        XorShiftRng {
            x: w(seed_u32[0]),
            y: w(seed_u32[1]),
            z: w(seed_u32[2]),
            w: w(seed_u32[3]),
        }
    }

    fn from_rng<R: RngCore>(mut rng: R) -> Result<Self, Error> {
        let mut seed_u32 = [0u32; 4];
        loop {
            unsafe {
                let ptr = seed_u32.as_mut_ptr() as *mut u8;

                let slice = slice::from_raw_parts_mut(ptr, 4 * 4);
                rng.try_fill_bytes(slice)?;
            }
            if !seed_u32.iter().all(|&x| x == 0) { break; }
        }

        Ok(XorShiftRng {
            x: w(seed_u32[0]),
            y: w(seed_u32[1]),
            z: w(seed_u32[2]),
            w: w(seed_u32[3]),
        })
    }
}
