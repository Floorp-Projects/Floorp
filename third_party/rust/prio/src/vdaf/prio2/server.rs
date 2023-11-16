// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! Primitives for the Prio2 server.
use crate::{
    field::{FftFriendlyFieldElement, FieldError},
    polynomial::poly_interpret_eval,
    prng::PrngError,
    vdaf::prio2::client::{unpack_proof, SerializeError},
};
use serde::{Deserialize, Serialize};

/// Possible errors from server operations
#[derive(Debug, thiserror::Error)]
pub enum ServerError {
    /// Unexpected Share Length
    #[allow(unused)]
    #[error("unexpected share length")]
    ShareLength,
    /// Finite field operation error
    #[error("finite field operation error")]
    Field(#[from] FieldError),
    /// Serialization/deserialization error
    #[error("serialization/deserialization error")]
    Serialize(#[from] SerializeError),
    /// Failure when calling getrandom().
    #[error("getrandom: {0}")]
    GetRandom(#[from] getrandom::Error),
    /// PRNG error.
    #[error("prng error: {0}")]
    Prng(#[from] PrngError),
}

/// Verification message for proof validation
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct VerificationMessage<F> {
    /// f evaluated at random point
    pub f_r: F,
    /// g evaluated at random point
    pub g_r: F,
    /// h evaluated at random point
    pub h_r: F,
}

/// Given a proof and evaluation point, this constructs the verification
/// message.
pub(crate) fn generate_verification_message<F: FftFriendlyFieldElement>(
    dimension: usize,
    eval_at: F,
    proof: &[F],
    is_first_server: bool,
) -> Result<VerificationMessage<F>, ServerError> {
    let unpacked = unpack_proof(proof, dimension)?;
    let n: usize = (dimension + 1).next_power_of_two();
    let proof_length = 2 * n;
    let mut fft_in = vec![F::zero(); proof_length];
    let mut fft_mem = vec![F::zero(); proof_length];

    // construct and evaluate polynomial f at the random point
    fft_in[0] = *unpacked.f0;
    fft_in[1..unpacked.data.len() + 1].copy_from_slice(unpacked.data);
    let f_r = poly_interpret_eval(&fft_in[..n], eval_at, &mut fft_mem);

    // construct and evaluate polynomial g at the random point
    fft_in[0] = *unpacked.g0;
    if is_first_server {
        for x in fft_in[1..unpacked.data.len() + 1].iter_mut() {
            *x -= F::one();
        }
    }
    let g_r = poly_interpret_eval(&fft_in[..n], eval_at, &mut fft_mem);

    // construct and evaluate polynomial h at the random point
    fft_in[0] = *unpacked.h0;
    fft_in[1] = unpacked.points_h_packed[0];
    for (x, chunk) in unpacked.points_h_packed[1..]
        .iter()
        .zip(fft_in[2..proof_length].chunks_exact_mut(2))
    {
        chunk[0] = F::zero();
        chunk[1] = *x;
    }
    let h_r = poly_interpret_eval(&fft_in, eval_at, &mut fft_mem);

    Ok(VerificationMessage { f_r, g_r, h_r })
}

/// Decides if the distributed proof is valid
pub(crate) fn is_valid_share<F: FftFriendlyFieldElement>(
    v1: &VerificationMessage<F>,
    v2: &VerificationMessage<F>,
) -> bool {
    // reconstruct f_r, g_r, h_r
    let f_r = v1.f_r + v2.f_r;
    let g_r = v1.g_r + v2.g_r;
    let h_r = v1.h_r + v2.h_r;
    // validity check
    f_r * g_r == h_r
}

#[cfg(test)]
mod test_util {
    use crate::{
        field::{merge_vector, FftFriendlyFieldElement},
        prng::Prng,
        vdaf::prio2::client::proof_length,
    };

    use super::{generate_verification_message, is_valid_share, ServerError, VerificationMessage};

    /// Main workhorse of the server.
    #[derive(Debug)]
    pub(crate) struct Server<F> {
        dimension: usize,
        is_first_server: bool,
        accumulator: Vec<F>,
    }

    impl<F: FftFriendlyFieldElement> Server<F> {
        /// Construct a new server instance
        ///
        /// Params:
        ///  * `dimension`: the number of elements in the aggregation vector.
        ///  * `is_first_server`: only one of the servers should have this true.
        pub fn new(dimension: usize, is_first_server: bool) -> Result<Server<F>, ServerError> {
            Ok(Server {
                dimension,
                is_first_server,
                accumulator: vec![F::zero(); dimension],
            })
        }

        /// Deserialize
        fn deserialize_share(&self, share: &[u8]) -> Result<Vec<F>, ServerError> {
            let len = proof_length(self.dimension);
            Ok(if self.is_first_server {
                F::byte_slice_into_vec(share)?
            } else {
                if share.len() != 32 {
                    return Err(ServerError::ShareLength);
                }

                Prng::from_prio2_seed(&share.try_into().unwrap())
                    .take(len)
                    .collect()
            })
        }

        /// Generate verification message from an encrypted share
        ///
        /// This decrypts the share of the proof and constructs the
        /// [`VerificationMessage`](struct.VerificationMessage.html).
        /// The `eval_at` field should be generate by
        /// [choose_eval_at](#method.choose_eval_at).
        pub fn generate_verification_message(
            &mut self,
            eval_at: F,
            share: &[u8],
        ) -> Result<VerificationMessage<F>, ServerError> {
            let share_field = self.deserialize_share(share)?;
            generate_verification_message(
                self.dimension,
                eval_at,
                &share_field,
                self.is_first_server,
            )
        }

        /// Add the content of the encrypted share into the accumulator
        ///
        /// This only changes the accumulator if the verification messages `v1` and
        /// `v2` indicate that the share passed validation.
        pub fn aggregate(
            &mut self,
            share: &[u8],
            v1: &VerificationMessage<F>,
            v2: &VerificationMessage<F>,
        ) -> Result<bool, ServerError> {
            let share_field = self.deserialize_share(share)?;
            let is_valid = is_valid_share(v1, v2);
            if is_valid {
                // Add to the accumulator. share_field also includes the proof
                // encoding, so we slice off the first dimension fields, which are
                // the actual data share.
                merge_vector(&mut self.accumulator, &share_field[..self.dimension])?;
            }

            Ok(is_valid)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        codec::Encode,
        field::{FieldElement, FieldPrio2},
        prng::Prng,
        vdaf::{
            prio2::{client::unpack_proof_mut, server::test_util::Server, Prio2},
            Client,
        },
    };
    use rand::{random, Rng};

    fn secret_share(share: &mut [FieldPrio2]) -> Vec<FieldPrio2> {
        let mut rng = rand::thread_rng();
        let mut share2 = vec![FieldPrio2::zero(); share.len()];
        for (f1, f2) in share.iter_mut().zip(share2.iter_mut()) {
            let f = FieldPrio2::from(rng.gen::<u32>());
            *f2 = f;
            *f1 -= f;
        }
        share2
    }

    #[test]
    fn test_validation() {
        let dim = 8;
        let proof_u32: Vec<u32> = vec![
            1, 0, 0, 0, 0, 0, 0, 0, 2052337230, 3217065186, 1886032198, 2533724497, 397524722,
            3820138372, 1535223968, 4291254640, 3565670552, 2447741959, 163741941, 335831680,
            2567182742, 3542857140, 124017604, 4201373647, 431621210, 1618555683, 267689149,
        ];

        let mut proof: Vec<FieldPrio2> = proof_u32.iter().map(|x| FieldPrio2::from(*x)).collect();
        let share2 = secret_share(&mut proof);
        let eval_at = FieldPrio2::from(12313);

        let v1 = generate_verification_message(dim, eval_at, &proof, true).unwrap();
        let v2 = generate_verification_message(dim, eval_at, &share2, false).unwrap();
        assert!(is_valid_share(&v1, &v2));
    }

    #[test]
    fn test_verification_message_serde() {
        let dim = 8;
        let proof_u32: Vec<u32> = vec![
            1, 0, 0, 0, 0, 0, 0, 0, 2052337230, 3217065186, 1886032198, 2533724497, 397524722,
            3820138372, 1535223968, 4291254640, 3565670552, 2447741959, 163741941, 335831680,
            2567182742, 3542857140, 124017604, 4201373647, 431621210, 1618555683, 267689149,
        ];

        let mut proof: Vec<FieldPrio2> = proof_u32.iter().map(|x| FieldPrio2::from(*x)).collect();
        let share2 = secret_share(&mut proof);
        let eval_at = FieldPrio2::from(12313);

        let v1 = generate_verification_message(dim, eval_at, &proof, true).unwrap();
        let v2 = generate_verification_message(dim, eval_at, &share2, false).unwrap();

        // serialize and deserialize the first verification message
        let serialized = serde_json::to_string(&v1).unwrap();
        let deserialized: VerificationMessage<FieldPrio2> =
            serde_json::from_str(&serialized).unwrap();

        assert!(is_valid_share(&deserialized, &v2));
    }

    #[derive(Debug, Clone, Copy, PartialEq)]
    enum Tweak {
        None,
        WrongInput,
        DataPartOfShare,
        ZeroTermF,
        ZeroTermG,
        ZeroTermH,
        PointsH,
        VerificationF,
        VerificationG,
        VerificationH,
    }

    fn tweaks(tweak: Tweak) {
        let dim = 123;

        let mut server1 = Server::<FieldPrio2>::new(dim, true).unwrap();
        let mut server2 = Server::new(dim, false).unwrap();

        // all zero data
        let mut data = vec![0; dim];

        if let Tweak::WrongInput = tweak {
            data[0] = 2;
        }

        let vdaf = Prio2::new(dim).unwrap();
        let (_, shares) = vdaf.shard(&data, &[0; 16]).unwrap();
        let share1_original = shares[0].get_encoded();
        let share2 = shares[1].get_encoded();

        let mut share1_field = FieldPrio2::byte_slice_into_vec(&share1_original).unwrap();
        let unpacked_share1 = unpack_proof_mut(&mut share1_field, dim).unwrap();

        let one = FieldPrio2::from(1);

        match tweak {
            Tweak::DataPartOfShare => unpacked_share1.data[0] += one,
            Tweak::ZeroTermF => *unpacked_share1.f0 += one,
            Tweak::ZeroTermG => *unpacked_share1.g0 += one,
            Tweak::ZeroTermH => *unpacked_share1.h0 += one,
            Tweak::PointsH => unpacked_share1.points_h_packed[0] += one,
            _ => (),
        };

        // reserialize altered share1
        let share1_modified = FieldPrio2::slice_into_byte_vec(&share1_field);

        let mut prng = Prng::from_prio2_seed(&random());
        let eval_at = vdaf.choose_eval_at(&mut prng);

        let mut v1 = server1
            .generate_verification_message(eval_at, &share1_modified)
            .unwrap();
        let v2 = server2
            .generate_verification_message(eval_at, &share2)
            .unwrap();

        match tweak {
            Tweak::VerificationF => v1.f_r += one,
            Tweak::VerificationG => v1.g_r += one,
            Tweak::VerificationH => v1.h_r += one,
            _ => (),
        }

        let should_be_valid = matches!(tweak, Tweak::None);
        assert_eq!(
            server1.aggregate(&share1_modified, &v1, &v2).unwrap(),
            should_be_valid
        );
        assert_eq!(
            server2.aggregate(&share2, &v1, &v2).unwrap(),
            should_be_valid
        );
    }

    #[test]
    fn tweak_none() {
        tweaks(Tweak::None);
    }

    #[test]
    fn tweak_input() {
        tweaks(Tweak::WrongInput);
    }

    #[test]
    fn tweak_data() {
        tweaks(Tweak::DataPartOfShare);
    }

    #[test]
    fn tweak_f_zero() {
        tweaks(Tweak::ZeroTermF);
    }

    #[test]
    fn tweak_g_zero() {
        tweaks(Tweak::ZeroTermG);
    }

    #[test]
    fn tweak_h_zero() {
        tweaks(Tweak::ZeroTermH);
    }

    #[test]
    fn tweak_h_points() {
        tweaks(Tweak::PointsH);
    }

    #[test]
    fn tweak_f_verif() {
        tweaks(Tweak::VerificationF);
    }

    #[test]
    fn tweak_g_verif() {
        tweaks(Tweak::VerificationG);
    }

    #[test]
    fn tweak_h_verif() {
        tweaks(Tweak::VerificationH);
    }
}
