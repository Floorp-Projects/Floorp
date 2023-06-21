// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! The Prio v2 server. Only 0 / 1 vectors are supported for now.
use crate::{
    encrypt::{decrypt_share, EncryptError, PrivateKey},
    field::{merge_vector, FftFriendlyFieldElement, FieldError},
    polynomial::{poly_interpret_eval, PolyAuxMemory},
    prng::{Prng, PrngError},
    util::{proof_length, unpack_proof, SerializeError},
    vdaf::prg::SeedStreamAes128,
};
use serde::{Deserialize, Serialize};
use std::convert::TryInto;

/// Possible errors from server operations
#[derive(Debug, thiserror::Error)]
pub enum ServerError {
    /// Unexpected Share Length
    #[error("unexpected share length")]
    ShareLength,
    /// Encryption/decryption error
    #[error("encryption/decryption error")]
    Encrypt(#[from] EncryptError),
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

/// Auxiliary memory for constructing a
/// [`VerificationMessage`](struct.VerificationMessage.html)
#[derive(Debug)]
pub struct ValidationMemory<F> {
    points_f: Vec<F>,
    points_g: Vec<F>,
    points_h: Vec<F>,
    poly_mem: PolyAuxMemory<F>,
}

impl<F: FftFriendlyFieldElement> ValidationMemory<F> {
    /// Construct a new ValidationMemory object for validating proof shares of
    /// length `dimension`.
    pub fn new(dimension: usize) -> Self {
        let n: usize = (dimension + 1).next_power_of_two();
        ValidationMemory {
            points_f: vec![F::zero(); n],
            points_g: vec![F::zero(); n],
            points_h: vec![F::zero(); 2 * n],
            poly_mem: PolyAuxMemory::new(n),
        }
    }
}

/// Main workhorse of the server.
#[derive(Debug)]
pub struct Server<F> {
    prng: Prng<F, SeedStreamAes128>,
    dimension: usize,
    is_first_server: bool,
    accumulator: Vec<F>,
    validation_mem: ValidationMemory<F>,
    private_key: PrivateKey,
}

impl<F: FftFriendlyFieldElement> Server<F> {
    /// Construct a new server instance
    ///
    /// Params:
    ///  * `dimension`: the number of elements in the aggregation vector.
    ///  * `is_first_server`: only one of the servers should have this true.
    ///  * `private_key`: the private key for decrypting the share of the proof.
    pub fn new(
        dimension: usize,
        is_first_server: bool,
        private_key: PrivateKey,
    ) -> Result<Server<F>, ServerError> {
        Ok(Server {
            prng: Prng::new()?,
            dimension,
            is_first_server,
            accumulator: vec![F::zero(); dimension],
            validation_mem: ValidationMemory::new(dimension),
            private_key,
        })
    }

    /// Decrypt and deserialize
    fn deserialize_share(&self, encrypted_share: &[u8]) -> Result<Vec<F>, ServerError> {
        let len = proof_length(self.dimension);
        let share = decrypt_share(encrypted_share, &self.private_key)?;
        Ok(if self.is_first_server {
            F::byte_slice_into_vec(&share)?
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
            &mut self.validation_mem,
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

    /// Return the current accumulated shares.
    ///
    /// These can be merged together using
    /// [`reconstruct_shares`](../util/fn.reconstruct_shares.html).
    pub fn total_shares(&self) -> &[F] {
        &self.accumulator
    }

    /// Merge shares from another server.
    ///
    /// This modifies the current accumulator.
    ///
    /// # Errors
    ///
    /// Returns an error if `other_total_shares.len()` is not equal to this
    //// server's `dimension`.
    pub fn merge_total_shares(&mut self, other_total_shares: &[F]) -> Result<(), ServerError> {
        Ok(merge_vector(&mut self.accumulator, other_total_shares)?)
    }

    /// Choose a random point for polynomial evaluation
    ///
    /// The point returned is not one of the roots used for polynomial
    /// evaluation.
    pub fn choose_eval_at(&mut self) -> F {
        loop {
            let eval_at = self.prng.get();
            if !self.validation_mem.poly_mem.roots_2n.contains(&eval_at) {
                break eval_at;
            }
        }
    }
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
pub fn generate_verification_message<F: FftFriendlyFieldElement>(
    dimension: usize,
    eval_at: F,
    proof: &[F],
    is_first_server: bool,
    mem: &mut ValidationMemory<F>,
) -> Result<VerificationMessage<F>, ServerError> {
    let unpacked = unpack_proof(proof, dimension)?;
    let proof_length = 2 * (dimension + 1).next_power_of_two();

    // set zero terms
    mem.points_f[0] = *unpacked.f0;
    mem.points_g[0] = *unpacked.g0;
    mem.points_h[0] = *unpacked.h0;

    // set points_f and points_g
    for (i, x) in unpacked.data.iter().enumerate() {
        mem.points_f[i + 1] = *x;

        if is_first_server {
            // only one server needs to subtract one for point_g
            mem.points_g[i + 1] = *x - F::one();
        } else {
            mem.points_g[i + 1] = *x;
        }
    }

    // set points_h, skipping over elements that should be zero
    let mut i = 1;
    let mut j = 0;
    while i < proof_length {
        mem.points_h[i] = unpacked.points_h_packed[j];
        j += 1;
        i += 2;
    }

    // evaluate polynomials at random point
    let f_r = poly_interpret_eval(
        &mem.points_f,
        &mem.poly_mem.roots_n_inverted,
        eval_at,
        &mut mem.poly_mem.coeffs,
        &mut mem.poly_mem.fft_memory,
    );
    let g_r = poly_interpret_eval(
        &mem.points_g,
        &mem.poly_mem.roots_n_inverted,
        eval_at,
        &mut mem.poly_mem.coeffs,
        &mut mem.poly_mem.fft_memory,
    );
    let h_r = poly_interpret_eval(
        &mem.points_h,
        &mem.poly_mem.roots_2n_inverted,
        eval_at,
        &mut mem.poly_mem.coeffs,
        &mut mem.poly_mem.fft_memory,
    );

    Ok(VerificationMessage { f_r, g_r, h_r })
}

/// Decides if the distributed proof is valid
pub fn is_valid_share<F: FftFriendlyFieldElement>(
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
mod tests {
    use super::*;
    use crate::{
        encrypt::{encrypt_share, PublicKey},
        field::{FieldElement, FieldPrio2},
        test_vector::Priov2TestVector,
        util::{self, unpack_proof_mut},
    };
    use serde_json;

    #[test]
    fn test_validation() {
        let dim = 8;
        let proof_u32: Vec<u32> = vec![
            1, 0, 0, 0, 0, 0, 0, 0, 2052337230, 3217065186, 1886032198, 2533724497, 397524722,
            3820138372, 1535223968, 4291254640, 3565670552, 2447741959, 163741941, 335831680,
            2567182742, 3542857140, 124017604, 4201373647, 431621210, 1618555683, 267689149,
        ];

        let mut proof: Vec<FieldPrio2> = proof_u32.iter().map(|x| FieldPrio2::from(*x)).collect();
        let share2 = util::tests::secret_share(&mut proof);
        let eval_at = FieldPrio2::from(12313);

        let mut validation_mem = ValidationMemory::new(dim);

        let v1 =
            generate_verification_message(dim, eval_at, &proof, true, &mut validation_mem).unwrap();
        let v2 = generate_verification_message(dim, eval_at, &share2, false, &mut validation_mem)
            .unwrap();
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
        let share2 = util::tests::secret_share(&mut proof);
        let eval_at = FieldPrio2::from(12313);

        let mut validation_mem = ValidationMemory::new(dim);

        let v1 =
            generate_verification_message(dim, eval_at, &proof, true, &mut validation_mem).unwrap();
        let v2 = generate_verification_message(dim, eval_at, &share2, false, &mut validation_mem)
            .unwrap();

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

        // We generate a test vector just to get a `Client` and `Server`s with
        // encryption keys but construct and tweak inputs below.
        let test_vector = Priov2TestVector::new(dim, 0).unwrap();
        let mut server1 = test_vector.server_1().unwrap();
        let mut server2 = test_vector.server_2().unwrap();
        let mut client = test_vector.client().unwrap();

        // all zero data
        let mut data = vec![FieldPrio2::zero(); dim];

        if let Tweak::WrongInput = tweak {
            data[0] = FieldPrio2::from(2);
        }

        let (share1_original, share2) = client.encode_simple(&data).unwrap();

        let decrypted_share1 = decrypt_share(&share1_original, &server1.private_key).unwrap();
        let mut share1_field = FieldPrio2::byte_slice_into_vec(&decrypted_share1).unwrap();
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
        let share1_modified = encrypt_share(
            &FieldPrio2::slice_into_byte_vec(&share1_field),
            &PublicKey::from(&server1.private_key),
        )
        .unwrap();

        let eval_at = server1.choose_eval_at();

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
