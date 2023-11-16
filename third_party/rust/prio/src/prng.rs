// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! Tool for generating pseudorandom field elements.
//!
//! NOTE: The public API for this module is a work in progress.

use crate::field::{FieldElement, FieldElementExt};
#[cfg(feature = "crypto-dependencies")]
use crate::vdaf::xof::SeedStreamAes128;
#[cfg(feature = "crypto-dependencies")]
use getrandom::getrandom;
use rand_core::RngCore;

use std::marker::PhantomData;
use std::ops::ControlFlow;

const BUFFER_SIZE_IN_ELEMENTS: usize = 32;

/// Errors propagated by methods in this module.
#[derive(Debug, thiserror::Error)]
pub enum PrngError {
    /// Failure when calling getrandom().
    #[error("getrandom: {0}")]
    GetRandom(#[from] getrandom::Error),
}

/// This type implements an iterator that generates a pseudorandom sequence of field elements. The
/// sequence is derived from a XOF's key stream.
#[derive(Debug)]
pub(crate) struct Prng<F, S> {
    phantom: PhantomData<F>,
    seed_stream: S,
    buffer: Vec<u8>,
    buffer_index: usize,
}

#[cfg(feature = "crypto-dependencies")]
impl<F: FieldElement> Prng<F, SeedStreamAes128> {
    /// Create a [`Prng`] from a seed for Prio 2. The first 16 bytes of the seed and the last 16
    /// bytes of the seed are used, respectively, for the key and initialization vector for AES128
    /// in CTR mode.
    pub(crate) fn from_prio2_seed(seed: &[u8; 32]) -> Self {
        let seed_stream = SeedStreamAes128::new(&seed[..16], &seed[16..]);
        Self::from_seed_stream(seed_stream)
    }

    /// Create a [`Prng`] from a randomly generated seed.
    pub(crate) fn new() -> Result<Self, PrngError> {
        let mut seed = [0; 32];
        getrandom(&mut seed)?;
        Ok(Self::from_prio2_seed(&seed))
    }
}

impl<F, S> Prng<F, S>
where
    F: FieldElement,
    S: RngCore,
{
    pub(crate) fn from_seed_stream(mut seed_stream: S) -> Self {
        let mut buffer = vec![0; BUFFER_SIZE_IN_ELEMENTS * F::ENCODED_SIZE];
        seed_stream.fill_bytes(&mut buffer);

        Self {
            phantom: PhantomData::<F>,
            seed_stream,
            buffer,
            buffer_index: 0,
        }
    }

    pub(crate) fn get(&mut self) -> F {
        loop {
            // Seek to the next chunk of the buffer that encodes an element of F.
            for i in (self.buffer_index..self.buffer.len()).step_by(F::ENCODED_SIZE) {
                let j = i + F::ENCODED_SIZE;

                if j > self.buffer.len() {
                    break;
                }

                self.buffer_index = j;

                match F::from_random_rejection(&self.buffer[i..j]) {
                    ControlFlow::Break(x) => return x,
                    ControlFlow::Continue(()) => continue, // reject this sample
                }
            }

            // Refresh buffer with the next chunk of XOF output, filling the front of the buffer
            // with the leftovers. This ensures continuity of the seed stream after converting the
            // `Prng` to a new field type via `into_new_field()`.
            let left_over = self.buffer.len() - self.buffer_index;
            self.buffer.copy_within(self.buffer_index.., 0);
            self.seed_stream.fill_bytes(&mut self.buffer[left_over..]);
            self.buffer_index = 0;
        }
    }

    /// Convert this object into a field element generator for a different field.
    #[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
    pub(crate) fn into_new_field<F1: FieldElement>(self) -> Prng<F1, S> {
        Prng {
            phantom: PhantomData,
            seed_stream: self.seed_stream,
            buffer: self.buffer,
            buffer_index: self.buffer_index,
        }
    }
}

impl<F, S> Iterator for Prng<F, S>
where
    F: FieldElement,
    S: RngCore,
{
    type Item = F;

    fn next(&mut self) -> Option<F> {
        Some(self.get())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        codec::Decode,
        field::{Field64, FieldPrio2},
        vdaf::xof::{Seed, SeedStreamSha3, Xof, XofShake128},
    };
    #[cfg(feature = "prio2")]
    use base64::{engine::Engine, prelude::BASE64_STANDARD};
    #[cfg(feature = "prio2")]
    use sha2::{Digest, Sha256};
    use std::convert::TryInto;

    #[test]
    fn secret_sharing_interop() {
        let seed = [
            0xcd, 0x85, 0x5b, 0xd4, 0x86, 0x48, 0xa4, 0xce, 0x52, 0x5c, 0x36, 0xee, 0x5a, 0x71,
            0xf3, 0x0f, 0x66, 0x80, 0xd3, 0x67, 0x53, 0x9a, 0x39, 0x6f, 0x12, 0x2f, 0xad, 0x94,
            0x4d, 0x34, 0xcb, 0x58,
        ];

        let reference = [
            0xd0056ec5, 0xe23f9c52, 0x47e4ddb4, 0xbe5dacf6, 0x4b130aba, 0x530c7a90, 0xe8fc4ee5,
            0xb0569cb7, 0x7774cd3c, 0x7f24e6a5, 0xcc82355d, 0xc41f4f13, 0x67fe193c, 0xc94d63a4,
            0x5d7b474c, 0xcc5c9f5f, 0xe368e1d5, 0x020fa0cf, 0x9e96aa2a, 0xe924137d, 0xfa026ab9,
            0x8ebca0cc, 0x26fc58a5, 0x10a7b173, 0xb9c97291, 0x53ef0e28, 0x069cfb8e, 0xe9383cae,
            0xacb8b748, 0x6f5b9d49, 0x887d061b, 0x86db0c58,
        ];

        let share2 = extract_share_from_seed::<FieldPrio2>(reference.len(), &seed);

        assert_eq!(share2, reference);
    }

    /// takes a seed and hash as base64 encoded strings
    #[cfg(feature = "prio2")]
    fn random_data_interop(seed_base64: &str, hash_base64: &str, len: usize) {
        let seed = BASE64_STANDARD.decode(seed_base64).unwrap();
        let random_data = extract_share_from_seed::<FieldPrio2>(len, &seed);

        let random_bytes = FieldPrio2::slice_into_byte_vec(&random_data);

        let mut hasher = Sha256::new();
        hasher.update(&random_bytes);
        let digest = hasher.finalize();
        assert_eq!(BASE64_STANDARD.encode(digest), hash_base64);
    }

    #[test]
    #[cfg(feature = "prio2")]
    fn test_hash_interop() {
        random_data_interop(
            "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
            "RtzeQuuiWdD6bW2ZTobRELDmClz1wLy3HUiKsYsITOI=",
            100_000,
        );

        // zero seed
        random_data_interop(
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
            "3wHQbSwAn9GPfoNkKe1qSzWdKnu/R+hPPyRwwz6Di+w=",
            100_000,
        );
        // 0, 1, 2 ... seed
        random_data_interop(
            "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
            "RtzeQuuiWdD6bW2ZTobRELDmClz1wLy3HUiKsYsITOI=",
            100_000,
        );
        // one arbirtary fixed seed
        random_data_interop(
            "rkLrnVcU8ULaiuXTvR3OKrfpMX0kQidqVzta1pleKKg=",
            "b1fMXYrGUNR3wOZ/7vmUMmY51QHoPDBzwok0fz6xC0I=",
            100_000,
        );
        // all bits set seed
        random_data_interop(
            "//////////////////////////////////////////8=",
            "iBiDaqLrv7/rX/+vs6akPiprGgYfULdh/XhoD61HQXA=",
            100_000,
        );
    }

    fn extract_share_from_seed<F: FieldElement>(length: usize, seed: &[u8]) -> Vec<F> {
        assert_eq!(seed.len(), 32);
        Prng::from_prio2_seed(seed.try_into().unwrap())
            .take(length)
            .collect()
    }

    #[test]
    fn rejection_sampling_test_vector() {
        // These constants were found in a brute-force search, and they test that the XOF performs
        // rejection sampling correctly when the raw output exceeds the prime modulus.
        let seed = Seed::get_decoded(&[
            0x29, 0xb2, 0x98, 0x64, 0xb4, 0xaa, 0x4e, 0x07, 0x2a, 0x44, 0x49, 0x24, 0xf6, 0x74,
            0x0a, 0x3d,
        ])
        .unwrap();
        let expected = Field64::from(2035552711764301796);

        let seed_stream = XofShake128::seed_stream(&seed, b"", b"");
        let mut prng = Prng::<Field64, _>::from_seed_stream(seed_stream);
        let actual = prng.nth(33236).unwrap();
        assert_eq!(actual, expected);

        #[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
        {
            let mut seed_stream = XofShake128::seed_stream(&seed, b"", b"");
            let mut actual = <Field64 as FieldElement>::zero();
            for _ in 0..=33236 {
                actual = <Field64 as crate::idpf::IdpfValue>::generate(&mut seed_stream, &());
            }
            assert_eq!(actual, expected);
        }
    }

    // Test that the `Prng`'s internal buffer properly copies the end of the buffer to the front
    // once it reaches the end.
    #[test]
    fn left_over_buffer_back_fill() {
        let seed = Seed::generate().unwrap();

        let mut prng: Prng<Field64, SeedStreamSha3> =
            Prng::from_seed_stream(XofShake128::seed_stream(&seed, b"", b""));

        // Construct a `Prng` with a longer-than-usual buffer.
        let mut prng_weird_buffer_size: Prng<Field64, SeedStreamSha3> =
            Prng::from_seed_stream(XofShake128::seed_stream(&seed, b"", b""));
        let mut extra = [0; 7];
        prng_weird_buffer_size.seed_stream.fill_bytes(&mut extra);
        prng_weird_buffer_size.buffer.extend_from_slice(&extra);

        // Check that the next several outputs match. We need to check enough outputs to ensure
        // that we have to refill the buffer.
        for _ in 0..BUFFER_SIZE_IN_ELEMENTS * 2 {
            assert_eq!(prng.next().unwrap(), prng_weird_buffer_size.next().unwrap());
        }
    }

    #[cfg(feature = "experimental")]
    #[test]
    fn into_new_field() {
        let seed = Seed::generate().unwrap();
        let want: Prng<Field64, SeedStreamSha3> =
            Prng::from_seed_stream(XofShake128::seed_stream(&seed, b"", b""));
        let want_buffer = want.buffer.clone();

        let got: Prng<FieldPrio2, _> = want.into_new_field();
        assert_eq!(got.buffer_index, 0);
        assert_eq!(got.buffer, want_buffer);
    }
}
