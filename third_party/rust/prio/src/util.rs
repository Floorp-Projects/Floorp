// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! Utility functions for handling Prio stuff.

use crate::field::{FftFriendlyFieldElement, FieldError};

/// Serialization errors
#[derive(Debug, thiserror::Error)]
pub enum SerializeError {
    /// Emitted by `unpack_proof[_mut]` if the serialized share+proof has the wrong length
    #[error("serialized input has wrong length")]
    UnpackInputSizeMismatch,
    /// Finite field operation error.
    #[error("finite field operation error")]
    Field(#[from] FieldError),
}

/// Returns the number of field elements in the proof for given dimension of
/// data elements
///
/// Proof is a vector, where the first `dimension` elements are the data
/// elements, the next 3 elements are the zero terms for polynomials f, g and h
/// and the remaining elements are non-zero points of h(x).
pub fn proof_length(dimension: usize) -> usize {
    // number of data items + number of zero terms + N
    dimension + 3 + (dimension + 1).next_power_of_two()
}

/// Unpacked proof with subcomponents
#[derive(Debug)]
pub struct UnpackedProof<'a, F: FftFriendlyFieldElement> {
    /// Data
    pub data: &'a [F],
    /// Zeroth coefficient of polynomial f
    pub f0: &'a F,
    /// Zeroth coefficient of polynomial g
    pub g0: &'a F,
    /// Zeroth coefficient of polynomial h
    pub h0: &'a F,
    /// Non-zero points of polynomial h
    pub points_h_packed: &'a [F],
}

/// Unpacked proof with mutable subcomponents
#[derive(Debug)]
pub struct UnpackedProofMut<'a, F: FftFriendlyFieldElement> {
    /// Data
    pub data: &'a mut [F],
    /// Zeroth coefficient of polynomial f
    pub f0: &'a mut F,
    /// Zeroth coefficient of polynomial g
    pub g0: &'a mut F,
    /// Zeroth coefficient of polynomial h
    pub h0: &'a mut F,
    /// Non-zero points of polynomial h
    pub points_h_packed: &'a mut [F],
}

/// Unpacks the proof vector into subcomponents
pub(crate) fn unpack_proof<F: FftFriendlyFieldElement>(
    proof: &[F],
    dimension: usize,
) -> Result<UnpackedProof<F>, SerializeError> {
    // check the proof length
    if proof.len() != proof_length(dimension) {
        return Err(SerializeError::UnpackInputSizeMismatch);
    }
    // split share into components
    let (data, rest) = proof.split_at(dimension);
    if let ([f0, g0, h0], points_h_packed) = rest.split_at(3) {
        Ok(UnpackedProof {
            data,
            f0,
            g0,
            h0,
            points_h_packed,
        })
    } else {
        Err(SerializeError::UnpackInputSizeMismatch)
    }
}

/// Unpacks a mutable proof vector into mutable subcomponents
// TODO(timg): This is public because it is used by tests/tweaks.rs. We should
// refactor that test so it doesn't require the crate to expose this function or
// UnpackedProofMut.
pub fn unpack_proof_mut<F: FftFriendlyFieldElement>(
    proof: &mut [F],
    dimension: usize,
) -> Result<UnpackedProofMut<F>, SerializeError> {
    // check the share length
    if proof.len() != proof_length(dimension) {
        return Err(SerializeError::UnpackInputSizeMismatch);
    }
    // split share into components
    let (data, rest) = proof.split_at_mut(dimension);
    if let ([f0, g0, h0], points_h_packed) = rest.split_at_mut(3) {
        Ok(UnpackedProofMut {
            data,
            f0,
            g0,
            h0,
            points_h_packed,
        })
    } else {
        Err(SerializeError::UnpackInputSizeMismatch)
    }
}

/// Add two field element arrays together elementwise.
///
/// Returns None, when array lengths are not equal.
pub fn reconstruct_shares<F: FftFriendlyFieldElement>(
    share1: &[F],
    share2: &[F],
) -> Option<Vec<F>> {
    if share1.len() != share2.len() {
        return None;
    }

    let mut reconstructed: Vec<F> = vec![F::zero(); share1.len()];

    for (r, (s1, s2)) in reconstructed
        .iter_mut()
        .zip(share1.iter().zip(share2.iter()))
    {
        *r = *s1 + *s2;
    }

    Some(reconstructed)
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use crate::field::{Field64, FieldElement, FieldPrio2};
    use assert_matches::assert_matches;

    pub fn secret_share(share: &mut [FieldPrio2]) -> Vec<FieldPrio2> {
        use rand::Rng;
        let mut rng = rand::thread_rng();
        let mut random = vec![0u32; share.len()];
        let mut share2 = vec![FieldPrio2::zero(); share.len()];

        rng.fill(&mut random[..]);

        for (r, f) in random.iter().zip(share2.iter_mut()) {
            *f = FieldPrio2::from(*r);
        }

        for (f1, f2) in share.iter_mut().zip(share2.iter()) {
            *f1 -= *f2;
        }

        share2
    }

    #[test]
    fn test_unpack_share_mut() {
        let dim = 15;
        let len = proof_length(dim);

        let mut share = vec![FieldPrio2::from(0); len];
        let unpacked = unpack_proof_mut(&mut share, dim).unwrap();
        *unpacked.f0 = FieldPrio2::from(12);
        assert_eq!(share[dim], 12);

        let mut short_share = vec![FieldPrio2::from(0); len - 1];
        assert_matches!(
            unpack_proof_mut(&mut short_share, dim),
            Err(SerializeError::UnpackInputSizeMismatch)
        );
    }

    #[test]
    fn test_unpack_share() {
        let dim = 15;
        let len = proof_length(dim);

        let share = vec![Field64::from(0); len];
        unpack_proof(&share, dim).unwrap();

        let short_share = vec![Field64::from(0); len - 1];
        assert_matches!(
            unpack_proof(&short_share, dim),
            Err(SerializeError::UnpackInputSizeMismatch)
        );
    }

    #[test]
    fn secret_sharing() {
        let mut share1 = vec![FieldPrio2::zero(); 10];
        share1[3] = 21.into();
        share1[8] = 123.into();

        let original_data = share1.clone();

        let share2 = secret_share(&mut share1);

        let reconstructed = reconstruct_shares(&share1, &share2).unwrap();
        assert_eq!(reconstructed, original_data);
    }
}
