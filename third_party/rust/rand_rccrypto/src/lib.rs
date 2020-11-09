/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use rand;
pub use rand_core;
use rand_core::{impls, CryptoRng, Error, RngCore};

pub struct RcCryptoRng;

impl RngCore for RcCryptoRng {
    fn next_u32(&mut self) -> u32 {
        impls::next_u32_via_fill(self)
    }

    fn next_u64(&mut self) -> u64 {
        impls::next_u64_via_fill(self)
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.try_fill_bytes(dest).unwrap()
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        rc_crypto::rand::fill(dest).map_err(Error::new)
    }
}

// NSS's `PK11_GenerateRandom` is considered a CSPRNG.
impl CryptoRng for RcCryptoRng {}
