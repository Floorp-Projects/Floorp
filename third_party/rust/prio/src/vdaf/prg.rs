// SPDX-License-Identifier: MPL-2.0

//! Implementations of PRGs specified in [[draft-irtf-cfrg-vdaf-01]].
//!
//! [draft-irtf-cfrg-vdaf-01]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/01/

use crate::vdaf::{CodecError, Decode, Encode};
#[cfg(feature = "crypto-dependencies")]
use aes::{
    cipher::{KeyIvInit, StreamCipher},
    Aes128,
};
#[cfg(feature = "crypto-dependencies")]
use cmac::{Cmac, Mac};
#[cfg(feature = "crypto-dependencies")]
use ctr::Ctr64BE;
#[cfg(feature = "crypto-dependencies")]
use std::fmt::Formatter;
use std::{
    fmt::Debug,
    io::{Cursor, Read},
};

/// Function pointer to fill a buffer with random bytes. Under normal operation,
/// `getrandom::getrandom()` will be used, but other implementations can be used to control
/// randomness when generating or verifying test vectors.
pub(crate) type RandSource = fn(&mut [u8]) -> Result<(), getrandom::Error>;

/// Input of [`Prg`].
#[derive(Clone, Debug, Eq)]
pub struct Seed<const L: usize>(pub(crate) [u8; L]);

impl<const L: usize> Seed<L> {
    /// Generate a uniform random seed.
    pub fn generate() -> Result<Self, getrandom::Error> {
        Self::from_rand_source(getrandom::getrandom)
    }

    pub(crate) fn from_rand_source(rand_source: RandSource) -> Result<Self, getrandom::Error> {
        let mut seed = [0; L];
        rand_source(&mut seed)?;
        Ok(Self(seed))
    }

    pub(crate) fn uninitialized() -> Self {
        Self([0; L])
    }

    pub(crate) fn xor_accumulate(&mut self, other: &Self) {
        for i in 0..L {
            self.0[i] ^= other.0[i]
        }
    }

    pub(crate) fn xor(&mut self, left: &Self, right: &Self) {
        for i in 0..L {
            self.0[i] = left.0[i] ^ right.0[i]
        }
    }
}

impl<const L: usize> AsRef<[u8; L]> for Seed<L> {
    fn as_ref(&self) -> &[u8; L] {
        &self.0
    }
}

impl<const L: usize> PartialEq for Seed<L> {
    fn eq(&self, other: &Self) -> bool {
        // Do constant-time compare.
        let mut r = 0;
        for (x, y) in (&self.0[..]).iter().zip(&other.0[..]) {
            r |= x ^ y;
        }
        r == 0
    }
}

impl<const L: usize> Encode for Seed<L> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&self.0[..]);
    }
}

impl<const L: usize> Decode for Seed<L> {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let mut seed = [0; L];
        bytes.read_exact(&mut seed)?;
        Ok(Seed(seed))
    }
}

/// A stream of pseudorandom bytes derived from a seed.
pub trait SeedStream {
    /// Fill `buf` with the next `buf.len()` bytes of output.
    fn fill(&mut self, buf: &mut [u8]);
}

/// A pseudorandom generator (PRG) with the interface specified in [[draft-irtf-cfrg-vdaf-01]].
///
/// [draft-irtf-cfrg-vdaf-01]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/01/
pub trait Prg<const L: usize>: Clone + Debug {
    /// The type of stream produced by this PRG.
    type SeedStream: SeedStream;

    /// Construct an instance of [`Prg`] with the given seed.
    fn init(seed_bytes: &[u8; L]) -> Self;

    /// Update the PRG state by passing in the next fragment of the info string. The final info
    /// string is assembled from the concatenation of sequence of fragments passed to this method.
    fn update(&mut self, data: &[u8]);

    /// Finalize the PRG state, producing a seed stream.
    fn into_seed_stream(self) -> Self::SeedStream;

    /// Finalize the PRG state, producing a seed.
    fn into_seed(self) -> Seed<L> {
        let mut new_seed = [0; L];
        let mut seed_stream = self.into_seed_stream();
        seed_stream.fill(&mut new_seed);
        Seed(new_seed)
    }

    /// Construct a seed stream from the given seed and info string.
    fn seed_stream(seed: &Seed<L>, info: &[u8]) -> Self::SeedStream {
        let mut prg = Self::init(seed.as_ref());
        prg.update(info);
        prg.into_seed_stream()
    }
}

/// The PRG based on AES128 as specified in [[draft-irtf-cfrg-vdaf-01]].
///
/// [draft-irtf-cfrg-vdaf-01]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/01/
#[derive(Clone, Debug)]
#[cfg(feature = "crypto-dependencies")]
pub struct PrgAes128(Cmac<Aes128>);

#[cfg(feature = "crypto-dependencies")]
impl Prg<16> for PrgAes128 {
    type SeedStream = SeedStreamAes128;

    fn init(seed_bytes: &[u8; 16]) -> Self {
        Self(Cmac::new_from_slice(seed_bytes).unwrap())
    }

    fn update(&mut self, data: &[u8]) {
        self.0.update(data);
    }

    fn into_seed_stream(self) -> SeedStreamAes128 {
        let key = self.0.finalize().into_bytes();
        SeedStreamAes128::new(&key, &[0; 16])
    }
}

/// The key stream produced by AES128 in CTR-mode.
#[cfg(feature = "crypto-dependencies")]
pub struct SeedStreamAes128(Ctr64BE<Aes128>);

#[cfg(feature = "crypto-dependencies")]
impl SeedStreamAes128 {
    pub(crate) fn new(key: &[u8], iv: &[u8]) -> Self {
        SeedStreamAes128(Ctr64BE::<Aes128>::new(key.into(), iv.into()))
    }
}

#[cfg(feature = "crypto-dependencies")]
impl SeedStream for SeedStreamAes128 {
    fn fill(&mut self, buf: &mut [u8]) {
        buf.fill(0);
        self.0.apply_keystream(buf);
    }
}

#[cfg(feature = "crypto-dependencies")]
impl Debug for SeedStreamAes128 {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        // Ctr64BE<Aes128> does not implement Debug, but [`ctr::CtrCore`][1] does, and we get that
        // with [`cipher::StreamCipherCoreWrapper::get_core`][2].
        //
        // [1]: https://docs.rs/ctr/latest/ctr/struct.CtrCore.html
        // [2]: https://docs.rs/cipher/latest/cipher/struct.StreamCipherCoreWrapper.html
        self.0.get_core().fmt(f)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{field::Field128, prng::Prng};
    use serde::{Deserialize, Serialize};
    use std::convert::TryInto;

    #[derive(Deserialize, Serialize)]
    struct PrgTestVector {
        #[serde(with = "hex")]
        seed: Vec<u8>,
        #[serde(with = "hex")]
        info: Vec<u8>,
        length: usize,
        #[serde(with = "hex")]
        derived_seed: Vec<u8>,
        #[serde(with = "hex")]
        expanded_vec_field128: Vec<u8>,
    }

    // Test correctness of dervied methods.
    fn test_prg<P, const L: usize>()
    where
        P: Prg<L>,
    {
        let seed = Seed::generate().unwrap();
        let info = b"info string";

        let mut prg = P::init(seed.as_ref());
        prg.update(info);

        let mut want: Seed<L> = Seed::uninitialized();
        prg.clone().into_seed_stream().fill(&mut want.0[..]);
        let got = prg.clone().into_seed();
        assert_eq!(got, want);

        let mut want = [0; 45];
        prg.clone().into_seed_stream().fill(&mut want);
        let mut got = [0; 45];
        P::seed_stream(&seed, info).fill(&mut got);
        assert_eq!(got, want);
    }

    #[test]
    fn prg_aes128() {
        let t: PrgTestVector =
            serde_json::from_str(include_str!("test_vec/01/PrgAes128.json")).unwrap();
        let mut prg = PrgAes128::init(&t.seed.try_into().unwrap());
        prg.update(&t.info);

        assert_eq!(
            prg.clone().into_seed(),
            Seed(t.derived_seed.try_into().unwrap())
        );

        let mut bytes = std::io::Cursor::new(t.expanded_vec_field128.as_slice());
        let mut want = Vec::with_capacity(t.length);
        while (bytes.position() as usize) < t.expanded_vec_field128.len() {
            want.push(Field128::decode(&mut bytes).unwrap())
        }
        let got: Vec<Field128> = Prng::from_seed_stream(prg.clone().into_seed_stream())
            .take(t.length)
            .collect();
        assert_eq!(got, want);

        test_prg::<PrgAes128, 16>();
    }
}
