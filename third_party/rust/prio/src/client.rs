// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! The Prio v2 client. Only 0 / 1 vectors are supported for now.

use crate::{
    encrypt::{encrypt_share, EncryptError, PublicKey},
    field::FftFriendlyFieldElement,
    polynomial::{poly_fft, PolyAuxMemory},
    prng::{Prng, PrngError},
    util::{proof_length, unpack_proof_mut},
    vdaf::{prg::SeedStreamAes128, VdafError},
};

use std::convert::TryFrom;

/// The main object that can be used to create Prio shares
///
/// Client is used to create Prio shares.
#[derive(Debug)]
pub struct Client<F: FftFriendlyFieldElement> {
    dimension: usize,
    mem: ClientMemory<F>,
    public_key1: PublicKey,
    public_key2: PublicKey,
}

/// Errors that might be emitted by the client.
#[derive(Debug, thiserror::Error)]
pub enum ClientError {
    /// Encryption/decryption error
    #[error("encryption/decryption error")]
    Encrypt(#[from] EncryptError),
    /// PRNG error
    #[error("prng error: {0}")]
    Prng(#[from] PrngError),
    /// VDAF error
    #[error("vdaf error: {0}")]
    Vdaf(#[from] VdafError),
}

impl<F: FftFriendlyFieldElement> Client<F> {
    /// Construct a new Prio client
    pub fn new(
        dimension: usize,
        public_key1: PublicKey,
        public_key2: PublicKey,
    ) -> Result<Self, ClientError> {
        Ok(Client {
            dimension,
            mem: ClientMemory::new(dimension)?,
            public_key1,
            public_key2,
        })
    }

    /// Construct a pair of encrypted shares based on the input data.
    pub fn encode_simple(&mut self, data: &[F]) -> Result<(Vec<u8>, Vec<u8>), ClientError> {
        let copy_data = |share_data: &mut [F]| {
            share_data[..].clone_from_slice(data);
        };
        Ok(self.encode_with(copy_data)?)
    }

    /// Construct a pair of encrypted shares using a initilization function.
    ///
    /// This might be slightly more efficient on large vectors, because one can
    /// avoid copying the input data.
    pub fn encode_with<G>(&mut self, init_function: G) -> Result<(Vec<u8>, Vec<u8>), EncryptError>
    where
        G: FnOnce(&mut [F]),
    {
        let mut proof = self.mem.prove_with(self.dimension, init_function);

        // use prng to share the proof: share2 is the PRNG seed, and proof is mutated
        // in-place
        let mut share2 = [0; 32];
        getrandom::getrandom(&mut share2)?;
        let share2_prng = Prng::from_prio2_seed(&share2);
        for (s1, d) in proof.iter_mut().zip(share2_prng.into_iter()) {
            *s1 -= d;
        }
        let share1 = F::slice_into_byte_vec(&proof);
        // encrypt shares with respective keys
        let encrypted_share1 = encrypt_share(&share1, &self.public_key1)?;
        let encrypted_share2 = encrypt_share(&share2, &self.public_key2)?;
        Ok((encrypted_share1, encrypted_share2))
    }

    /// Generate a proof of the input's validity. The output is the encoded input and proof.
    pub(crate) fn gen_proof(&mut self, input: &[F]) -> Vec<F> {
        let copy_data = |share_data: &mut [F]| {
            share_data[..].clone_from_slice(input);
        };
        self.mem.prove_with(self.dimension, copy_data)
    }
}

#[derive(Debug)]
pub(crate) struct ClientMemory<F> {
    prng: Prng<F, SeedStreamAes128>,
    points_f: Vec<F>,
    points_g: Vec<F>,
    evals_f: Vec<F>,
    evals_g: Vec<F>,
    poly_mem: PolyAuxMemory<F>,
}

impl<F: FftFriendlyFieldElement> ClientMemory<F> {
    pub(crate) fn new(dimension: usize) -> Result<Self, VdafError> {
        let n = (dimension + 1).next_power_of_two();
        if let Ok(size) = F::Integer::try_from(2 * n) {
            if size > F::generator_order() {
                return Err(VdafError::Uncategorized(
                    "input size exceeds field capacity".into(),
                ));
            }
        } else {
            return Err(VdafError::Uncategorized(
                "input size exceeds field capacity".into(),
            ));
        }

        Ok(Self {
            prng: Prng::new()?,
            points_f: vec![F::zero(); n],
            points_g: vec![F::zero(); n],
            evals_f: vec![F::zero(); 2 * n],
            evals_g: vec![F::zero(); 2 * n],
            poly_mem: PolyAuxMemory::new(n),
        })
    }
}

impl<F: FftFriendlyFieldElement> ClientMemory<F> {
    pub(crate) fn prove_with<G>(&mut self, dimension: usize, init_function: G) -> Vec<F>
    where
        G: FnOnce(&mut [F]),
    {
        let mut proof = vec![F::zero(); proof_length(dimension)];
        // unpack one long vector to different subparts
        let unpacked = unpack_proof_mut(&mut proof, dimension).unwrap();
        // initialize the data part
        init_function(unpacked.data);
        // fill in the rest
        construct_proof(
            unpacked.data,
            dimension,
            unpacked.f0,
            unpacked.g0,
            unpacked.h0,
            unpacked.points_h_packed,
            self,
        );

        proof
    }
}

/// Convenience function if one does not want to reuse
/// [`Client`](struct.Client.html).
pub fn encode_simple<F: FftFriendlyFieldElement>(
    data: &[F],
    public_key1: PublicKey,
    public_key2: PublicKey,
) -> Result<(Vec<u8>, Vec<u8>), ClientError> {
    let dimension = data.len();
    let mut client_memory = Client::new(dimension, public_key1, public_key2)?;
    client_memory.encode_simple(data)
}

fn interpolate_and_evaluate_at_2n<F: FftFriendlyFieldElement>(
    n: usize,
    points_in: &[F],
    evals_out: &mut [F],
    mem: &mut PolyAuxMemory<F>,
) {
    // interpolate through roots of unity
    poly_fft(
        &mut mem.coeffs,
        points_in,
        &mem.roots_n_inverted,
        n,
        true,
        &mut mem.fft_memory,
    );
    // evaluate at 2N roots of unity
    poly_fft(
        evals_out,
        &mem.coeffs,
        &mem.roots_2n,
        2 * n,
        false,
        &mut mem.fft_memory,
    );
}

/// Proof construction
///
/// Based on Theorem 2.3.3 from Henry Corrigan-Gibbs' dissertation
/// This constructs the output \pi by doing the necessesary calculations
fn construct_proof<F: FftFriendlyFieldElement>(
    data: &[F],
    dimension: usize,
    f0: &mut F,
    g0: &mut F,
    h0: &mut F,
    points_h_packed: &mut [F],
    mem: &mut ClientMemory<F>,
) {
    let n = (dimension + 1).next_power_of_two();

    // set zero terms to random
    *f0 = mem.prng.get();
    *g0 = mem.prng.get();
    mem.points_f[0] = *f0;
    mem.points_g[0] = *g0;

    // set zero term for the proof polynomial
    *h0 = *f0 * *g0;

    // set f_i = data_(i - 1)
    // set g_i = f_i - 1
    #[allow(clippy::needless_range_loop)]
    for i in 0..dimension {
        mem.points_f[i + 1] = data[i];
        mem.points_g[i + 1] = data[i] - F::one();
    }

    // interpolate and evaluate at roots of unity
    interpolate_and_evaluate_at_2n(n, &mem.points_f, &mut mem.evals_f, &mut mem.poly_mem);
    interpolate_and_evaluate_at_2n(n, &mem.points_g, &mut mem.evals_g, &mut mem.poly_mem);

    // calculate the proof polynomial as evals_f(r) * evals_g(r)
    // only add non-zero points
    let mut j: usize = 0;
    let mut i: usize = 1;
    while i < 2 * n {
        points_h_packed[j] = mem.evals_f[i] * mem.evals_g[i];
        j += 1;
        i += 2;
    }
}

#[test]
fn test_encode() {
    use crate::field::FieldPrio2;
    let pub_key1 = PublicKey::from_base64(
        "BIl6j+J6dYttxALdjISDv6ZI4/VWVEhUzaS05LgrsfswmbLOgNt9HUC2E0w+9RqZx3XMkdEHBHfNuCSMpOwofVQ=",
    )
    .unwrap();
    let pub_key2 = PublicKey::from_base64(
        "BNNOqoU54GPo+1gTPv+hCgA9U2ZCKd76yOMrWa1xTWgeb4LhFLMQIQoRwDVaW64g/WTdcxT4rDULoycUNFB60LE=",
    )
    .unwrap();

    let data_u32 = [0u32, 1, 0, 1, 1, 0, 0, 0, 1];
    let data = data_u32
        .iter()
        .map(|x| FieldPrio2::from(*x))
        .collect::<Vec<FieldPrio2>>();
    let encoded_shares = encode_simple(&data, pub_key1, pub_key2);
    assert!(encoded_shares.is_ok());
}
