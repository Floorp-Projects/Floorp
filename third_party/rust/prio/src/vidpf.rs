// SPDX-License-Identifier: MPL-2.0

//! Verifiable Incremental Distributed Point Function (VIDPF).
//!
//! The VIDPF construction is specified in [[draft-mouris-cfrg-mastic]] and builds
//! on techniques from [[MST23]] and [[CP22]] to lift an IDPF to a VIDPF.
//!
//! [CP22]: https://eprint.iacr.org/2021/580
//! [MST23]: https://eprint.iacr.org/2023/080
//! [draft-mouris-cfrg-mastic]: https://datatracker.ietf.org/doc/draft-mouris-cfrg-mastic/02/

use core::{
    iter::zip,
    ops::{Add, AddAssign, BitXor, BitXorAssign, Index, Sub},
};

use bitvec::field::BitField;
use rand_core::RngCore;
use std::io::Cursor;
use subtle::{Choice, ConditionallyNegatable, ConditionallySelectable};

use crate::{
    codec::{CodecError, Encode, ParameterizedDecode},
    field::FieldElement,
    idpf::{
        conditional_select_seed, conditional_swap_seed, conditional_xor_seeds, xor_seeds,
        IdpfInput, IdpfValue,
    },
    vdaf::xof::{Seed, Xof, XofFixedKeyAes128, XofTurboShake128},
};

/// VIDPF-related errors.
#[derive(Debug, thiserror::Error)]
#[non_exhaustive]
pub enum VidpfError {
    /// Error when key's identifier are equal.
    #[error("key's identifier should be different")]
    SameKeyId,

    /// Error when level does not fit in a 32-bit number.
    #[error("level is not representable as a 32-bit integer")]
    LevelTooBig,

    /// Error during VIDPF evaluation: tried to access a level index out of bounds.
    #[error("level index out of bounds")]
    IndexLevel,

    /// Error when weight's length mismatches the length in weight's parameter.
    #[error("invalid weight length")]
    InvalidWeightLength,

    /// Failure when calling getrandom().
    #[error("getrandom: {0}")]
    GetRandom(#[from] getrandom::Error),
}

/// Represents the domain of an incremental point function.
pub type VidpfInput = IdpfInput;

/// Represents the codomain of an incremental point function.
pub trait VidpfValue: IdpfValue + Clone {}

/// A VIDPF instance.
pub struct Vidpf<W: VidpfValue, const NONCE_SIZE: usize> {
    /// Any parameters required to instantiate a weight value.
    weight_parameter: W::ValueParameter,
}

impl<W: VidpfValue, const NONCE_SIZE: usize> Vidpf<W, NONCE_SIZE> {
    /// Creates a VIDPF instance.
    ///
    /// # Arguments
    ///
    /// * `weight_parameter`, any parameters required to instantiate a weight value.
    pub const fn new(weight_parameter: W::ValueParameter) -> Self {
        Self { weight_parameter }
    }

    /// The [`Vidpf::gen`] method splits an incremental point function `F` into two private keys
    /// used by the aggregation servers, and a common public share.
    ///
    /// The incremental point function is defined as `F`: [`VidpfInput`] --> [`VidpfValue`]
    /// such that:
    ///
    /// ```txt
    /// F(x) = weight, if x is a prefix of the input.
    /// F(x) = 0,      if x is not a prefix of the input.
    /// ```
    ///
    /// # Arguments
    ///
    /// * `input`, determines the input of the function.
    /// * `weight`, determines the input's weight of the function.
    /// * `nonce`, used to cryptographically bind some information.
    pub fn gen(
        &self,
        input: &VidpfInput,
        weight: &W,
        nonce: &[u8; NONCE_SIZE],
    ) -> Result<(VidpfPublicShare<W>, [VidpfKey; 2]), VidpfError> {
        let keys = [
            VidpfKey::gen(VidpfServerId::S0)?,
            VidpfKey::gen(VidpfServerId::S1)?,
        ];
        let public = self.gen_with_keys(&keys, input, weight, nonce)?;
        Ok((public, keys))
    }

    /// [`Vidpf::gen_with_keys`] works as the [`Vidpf::gen`] method, except that two different
    /// keys must be provided.
    fn gen_with_keys(
        &self,
        keys: &[VidpfKey; 2],
        input: &VidpfInput,
        weight: &W,
        nonce: &[u8; NONCE_SIZE],
    ) -> Result<VidpfPublicShare<W>, VidpfError> {
        if keys[0].id == keys[1].id {
            return Err(VidpfError::SameKeyId);
        }

        let mut s_i = [keys[0].value, keys[1].value];
        let mut t_i = [Choice::from(keys[0].id), Choice::from(keys[1].id)];

        let n = input.len();
        let mut cw = Vec::with_capacity(n);
        let mut cs = Vec::with_capacity(n);

        for level in 0..n {
            let alpha_i = Choice::from(u8::from(input.get(level).ok_or(VidpfError::IndexLevel)?));

            // If alpha_i == 0 then
            //     (same_seed, diff_seed) = (right_seed, left_seed)
            // else
            //     (same_seed, diff_seed) = (left_seed, right_seed)
            let seq_0 = Self::prg(&s_i[0], nonce);
            let (same_seed_0, diff_seed_0) = &mut (seq_0.right_seed, seq_0.left_seed);
            conditional_swap_seed(same_seed_0, diff_seed_0, alpha_i);

            let seq_1 = Self::prg(&s_i[1], nonce);
            let (same_seed_1, diff_seed_1) = &mut (seq_1.right_seed, seq_1.left_seed);
            conditional_swap_seed(same_seed_1, diff_seed_1, alpha_i);

            // If alpha_i == 0 then
            //    diff_control_bit = left_control_bit
            // else
            //    diff_control_bit = right_control_bit
            let diff_control_bit_0 = Choice::conditional_select(
                &seq_0.left_control_bit,
                &seq_0.right_control_bit,
                alpha_i,
            );
            let diff_control_bit_1 = Choice::conditional_select(
                &seq_1.left_control_bit,
                &seq_1.right_control_bit,
                alpha_i,
            );

            let s_cw = xor_seeds(same_seed_0, same_seed_1);
            let t_cw_l =
                seq_0.left_control_bit ^ seq_1.left_control_bit ^ alpha_i ^ Choice::from(1);
            let t_cw_r = seq_0.right_control_bit ^ seq_1.right_control_bit ^ alpha_i;
            let t_cw_diff = Choice::conditional_select(&t_cw_l, &t_cw_r, alpha_i);

            let s_tilde_i_0 = conditional_xor_seeds(diff_seed_0, &s_cw, t_i[0]);
            let s_tilde_i_1 = conditional_xor_seeds(diff_seed_1, &s_cw, t_i[1]);

            t_i[0] = diff_control_bit_0 ^ (t_i[0] & t_cw_diff);
            t_i[1] = diff_control_bit_1 ^ (t_i[1] & t_cw_diff);

            let w_i_0;
            let w_i_1;
            (s_i[0], w_i_0) = self.convert(s_tilde_i_0, nonce);
            (s_i[1], w_i_1) = self.convert(s_tilde_i_1, nonce);

            let mut w_cw = w_i_1 - w_i_0 + weight.clone();
            w_cw.conditional_negate(t_i[1]);

            let cw_i = VidpfCorrectionWord {
                seed: s_cw,
                left_control_bit: t_cw_l,
                right_control_bit: t_cw_r,
                weight: w_cw,
            };
            cw.push(cw_i);

            let pi_tilde_0 = Self::node_proof(input, level, &s_i[0])?;
            let pi_tilde_1 = Self::node_proof(input, level, &s_i[1])?;
            let cs_i = xor_proof(pi_tilde_0, &pi_tilde_1);
            cs.push(cs_i);
        }

        Ok(VidpfPublicShare { cw, cs })
    }

    /// [`Vidpf::eval`] evaluates the entire `input` and produces a share of the
    /// input's weight.
    pub fn eval(
        &self,
        key: &VidpfKey,
        public: &VidpfPublicShare<W>,
        input: &VidpfInput,
        nonce: &[u8; NONCE_SIZE],
    ) -> Result<VidpfValueShare<W>, VidpfError> {
        let mut state = VidpfEvalState::init_from_key(key);
        let mut share = W::zero(&self.weight_parameter);

        let n = input.len();
        for level in 0..n {
            (state, share) = self.eval_next(key.id, public, input, level, &state, nonce)?;
        }

        Ok(VidpfValueShare {
            share,
            proof: state.proof,
        })
    }

    /// [`Vidpf::eval_next`] evaluates the `input` at the given level using the provided initial
    /// state, and returns a new state and a share of the input's weight at that level.
    fn eval_next(
        &self,
        id: VidpfServerId,
        public: &VidpfPublicShare<W>,
        input: &VidpfInput,
        level: usize,
        state: &VidpfEvalState,
        nonce: &[u8; NONCE_SIZE],
    ) -> Result<(VidpfEvalState, W), VidpfError> {
        let cw = public.cw.get(level).ok_or(VidpfError::IndexLevel)?;

        let seq_tilde = Self::prg(&state.seed, nonce);

        let t_i = state.control_bit;
        let sl = conditional_xor_seeds(&seq_tilde.left_seed, &cw.seed, t_i);
        let sr = conditional_xor_seeds(&seq_tilde.right_seed, &cw.seed, t_i);
        let tl = seq_tilde.left_control_bit ^ (t_i & cw.left_control_bit);
        let tr = seq_tilde.right_control_bit ^ (t_i & cw.right_control_bit);

        let x_i = Choice::from(u8::from(input.get(level).ok_or(VidpfError::IndexLevel)?));
        let s_tilde_i = conditional_select_seed(x_i, &[sl, sr]);

        let next_control_bit = Choice::conditional_select(&tl, &tr, x_i);
        let (next_seed, w_i) = self.convert(s_tilde_i, nonce);

        let zero = <W as IdpfValue>::zero(&self.weight_parameter);
        let mut y = <W as IdpfValue>::conditional_select(&zero, &cw.weight, next_control_bit);
        y += w_i;
        y.conditional_negate(Choice::from(id));

        let pi_i = &state.proof;
        let cs_i = public.cs.get(level).ok_or(VidpfError::IndexLevel)?;
        let pi_tilde = Self::node_proof(input, level, &next_seed)?;
        let h2_input = xor_proof(
            conditional_xor_proof(pi_tilde, cs_i, next_control_bit),
            pi_i,
        );
        let next_proof = xor_proof(Self::node_proof_adjustment(h2_input), pi_i);

        let next_state = VidpfEvalState {
            seed: next_seed,
            control_bit: next_control_bit,
            proof: next_proof,
        };

        Ok((next_state, y))
    }

    fn prg(seed: &VidpfSeed, nonce: &[u8]) -> VidpfPrgOutput {
        let mut rng = XofFixedKeyAes128::seed_stream(&Seed(*seed), VidpfDomainSepTag::PRG, nonce);

        let mut left_seed = VidpfSeed::default();
        let mut right_seed = VidpfSeed::default();
        rng.fill_bytes(&mut left_seed);
        rng.fill_bytes(&mut right_seed);
        // Use the LSB of seeds as control bits, and clears the bit,
        // i.e., seeds produced by `prg` always have their LSB = 0.
        // This ensures `prg` costs two AES calls only.
        let left_control_bit = Choice::from(left_seed[0] & 0x01);
        let right_control_bit = Choice::from(right_seed[0] & 0x01);
        left_seed[0] &= 0xFE;
        right_seed[0] &= 0xFE;

        VidpfPrgOutput {
            left_seed,
            left_control_bit,
            right_seed,
            right_control_bit,
        }
    }

    fn convert(&self, seed: VidpfSeed, nonce: &[u8; NONCE_SIZE]) -> (VidpfSeed, W) {
        let mut rng =
            XofFixedKeyAes128::seed_stream(&Seed(seed), VidpfDomainSepTag::CONVERT, nonce);

        let mut out_seed = VidpfSeed::default();
        rng.fill_bytes(&mut out_seed);
        let value = <W as IdpfValue>::generate(&mut rng, &self.weight_parameter);

        (out_seed, value)
    }

    fn node_proof(
        input: &VidpfInput,
        level: usize,
        seed: &VidpfSeed,
    ) -> Result<VidpfProof, VidpfError> {
        let mut shake = XofTurboShake128::init(seed, VidpfDomainSepTag::NODE_PROOF);
        for chunk128 in input
            .index(..=level)
            .chunks(128)
            .map(BitField::load_le::<u128>)
            .map(u128::to_le_bytes)
        {
            shake.update(&chunk128);
        }
        shake.update(
            &u16::try_from(level)
                .map_err(|_e| VidpfError::LevelTooBig)?
                .to_le_bytes(),
        );
        let mut rng = shake.into_seed_stream();

        let mut proof = VidpfProof::default();
        rng.fill_bytes(&mut proof);

        Ok(proof)
    }

    fn node_proof_adjustment(mut proof: VidpfProof) -> VidpfProof {
        let mut rng = XofTurboShake128::seed_stream(
            &Seed(Default::default()),
            VidpfDomainSepTag::NODE_PROOF_ADJUST,
            &proof,
        );
        rng.fill_bytes(&mut proof);

        proof
    }
}

/// Contains the domain separation tags for invoking different oracles.
struct VidpfDomainSepTag;
impl VidpfDomainSepTag {
    const PRG: &'static [u8] = b"Prg";
    const CONVERT: &'static [u8] = b"Convert";
    const NODE_PROOF: &'static [u8] = b"NodeProof";
    const NODE_PROOF_ADJUST: &'static [u8] = b"NodeProofAdjust";
}

/// Private key of an aggregation server.
pub struct VidpfKey {
    id: VidpfServerId,
    value: [u8; 16],
}

impl VidpfKey {
    /// Generates a key at random.
    ///
    /// # Errors
    /// Triggers an error if the random generator fails.
    pub(crate) fn gen(id: VidpfServerId) -> Result<Self, VidpfError> {
        let mut value = [0; 16];
        getrandom::getrandom(&mut value)?;
        Ok(Self { id, value })
    }
}

/// Identifies the two aggregation servers.
#[derive(Clone, Copy, PartialEq, Eq)]
pub(crate) enum VidpfServerId {
    /// S0 is the first server.
    S0,
    /// S1 is the second server.
    S1,
}

impl From<VidpfServerId> for Choice {
    fn from(value: VidpfServerId) -> Self {
        match value {
            VidpfServerId::S0 => Self::from(0),
            VidpfServerId::S1 => Self::from(1),
        }
    }
}

/// Adjusts values of shares during the VIDPF evaluation.
#[derive(Debug)]
struct VidpfCorrectionWord<W: VidpfValue> {
    seed: VidpfSeed,
    left_control_bit: Choice,
    right_control_bit: Choice,
    weight: W,
}

/// Common public information used by aggregation servers.
#[derive(Debug)]
pub struct VidpfPublicShare<W: VidpfValue> {
    cw: Vec<VidpfCorrectionWord<W>>,
    cs: Vec<VidpfProof>,
}

/// Contains the values produced during input evaluation at a given level.
pub struct VidpfEvalState {
    seed: VidpfSeed,
    control_bit: Choice,
    proof: VidpfProof,
}

impl VidpfEvalState {
    fn init_from_key(key: &VidpfKey) -> Self {
        Self {
            seed: key.value,
            control_bit: Choice::from(key.id),
            proof: VidpfProof::default(),
        }
    }
}

/// Contains a share of the input's weight together with a proof for verification.
pub struct VidpfValueShare<W: VidpfValue> {
    /// Secret share of the input's weight.
    pub share: W,
    /// Proof used to verify the share.
    pub proof: VidpfProof,
}

/// Proof size in bytes.
const VIDPF_PROOF_SIZE: usize = 32;

/// Allows to validate user input and shares after evaluation.
type VidpfProof = [u8; VIDPF_PROOF_SIZE];

fn xor_proof(mut lhs: VidpfProof, rhs: &VidpfProof) -> VidpfProof {
    zip(&mut lhs, rhs).for_each(|(a, b)| a.bitxor_assign(b));
    lhs
}

fn conditional_xor_proof(mut lhs: VidpfProof, rhs: &VidpfProof, choice: Choice) -> VidpfProof {
    zip(&mut lhs, rhs).for_each(|(a, b)| a.conditional_assign(&a.bitxor(b), choice));
    lhs
}

/// Feeds a pseudorandom generator during evaluation.
type VidpfSeed = [u8; 16];

/// Contains the seeds and control bits produced by [`Vidpf::prg`].
struct VidpfPrgOutput {
    left_seed: VidpfSeed,
    left_control_bit: Choice,
    right_seed: VidpfSeed,
    right_control_bit: Choice,
}

/// Represents an array of field elements that implements the [`VidpfValue`] trait.
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct VidpfWeight<F: FieldElement>(Vec<F>);

impl<F: FieldElement> From<Vec<F>> for VidpfWeight<F> {
    fn from(value: Vec<F>) -> Self {
        Self(value)
    }
}

impl<F: FieldElement> VidpfValue for VidpfWeight<F> {}

impl<F: FieldElement> IdpfValue for VidpfWeight<F> {
    /// The parameter determines the number of field elements in the vector.
    type ValueParameter = usize;

    fn generate<S: RngCore>(seed_stream: &mut S, length: &Self::ValueParameter) -> Self {
        Self(
            (0..*length)
                .map(|_| <F as IdpfValue>::generate(seed_stream, &()))
                .collect(),
        )
    }

    fn zero(length: &Self::ValueParameter) -> Self {
        Self((0..*length).map(|_| <F as IdpfValue>::zero(&())).collect())
    }

    /// Panics if weight lengths are different.
    fn conditional_select(lhs: &Self, rhs: &Self, choice: Choice) -> Self {
        assert_eq!(
            lhs.0.len(),
            rhs.0.len(),
            "{}",
            VidpfError::InvalidWeightLength
        );

        Self(
            zip(&lhs.0, &rhs.0)
                .map(|(a, b)| <F as IdpfValue>::conditional_select(a, b, choice))
                .collect(),
        )
    }
}

impl<F: FieldElement> ConditionallyNegatable for VidpfWeight<F> {
    fn conditional_negate(&mut self, choice: Choice) {
        self.0.iter_mut().for_each(|a| a.conditional_negate(choice));
    }
}

impl<F: FieldElement> Add for VidpfWeight<F> {
    type Output = Self;

    /// Panics if weight lengths are different.
    fn add(self, rhs: Self) -> Self::Output {
        assert_eq!(
            self.0.len(),
            rhs.0.len(),
            "{}",
            VidpfError::InvalidWeightLength
        );

        Self(zip(self.0, rhs.0).map(|(a, b)| a.add(b)).collect())
    }
}

impl<F: FieldElement> AddAssign for VidpfWeight<F> {
    /// Panics if weight lengths are different.
    fn add_assign(&mut self, rhs: Self) {
        assert_eq!(
            self.0.len(),
            rhs.0.len(),
            "{}",
            VidpfError::InvalidWeightLength
        );

        zip(&mut self.0, rhs.0).for_each(|(a, b)| a.add_assign(b));
    }
}

impl<F: FieldElement> Sub for VidpfWeight<F> {
    type Output = Self;

    /// Panics if weight lengths are different.
    fn sub(self, rhs: Self) -> Self::Output {
        assert_eq!(
            self.0.len(),
            rhs.0.len(),
            "{}",
            VidpfError::InvalidWeightLength
        );

        Self(zip(self.0, rhs.0).map(|(a, b)| a.sub(b)).collect())
    }
}

impl<F: FieldElement> Encode for VidpfWeight<F> {
    fn encode(&self, bytes: &mut Vec<u8>) -> Result<(), CodecError> {
        for e in &self.0 {
            F::encode(e, bytes)?;
        }
        Ok(())
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(self.0.len() * F::ENCODED_SIZE)
    }
}

impl<F: FieldElement> ParameterizedDecode<<Self as IdpfValue>::ValueParameter> for VidpfWeight<F> {
    fn decode_with_param(
        decoding_parameter: &<Self as IdpfValue>::ValueParameter,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        let mut v = Vec::with_capacity(*decoding_parameter);
        for _ in 0..*decoding_parameter {
            v.push(F::decode_with_param(&(), bytes)?);
        }

        Ok(Self(v))
    }
}

#[cfg(test)]
mod tests {
    use crate::field::Field128;

    use super::VidpfWeight;

    type TestWeight = VidpfWeight<Field128>;
    const TEST_WEIGHT_LEN: usize = 3;
    const TEST_NONCE_SIZE: usize = 16;
    const TEST_NONCE: &[u8; TEST_NONCE_SIZE] = b"Test Nonce VIDPF";

    mod vidpf {
        use crate::{
            idpf::IdpfValue,
            vidpf::{
                Vidpf, VidpfError, VidpfEvalState, VidpfInput, VidpfKey, VidpfPublicShare,
                VidpfServerId,
            },
        };

        use super::{TestWeight, TEST_NONCE, TEST_NONCE_SIZE, TEST_WEIGHT_LEN};

        fn vidpf_gen_setup(
            input: &VidpfInput,
            weight: &TestWeight,
        ) -> (
            Vidpf<TestWeight, TEST_NONCE_SIZE>,
            VidpfPublicShare<TestWeight>,
            [VidpfKey; 2],
            [u8; TEST_NONCE_SIZE],
        ) {
            let vidpf = Vidpf::new(TEST_WEIGHT_LEN);
            let (public, keys) = vidpf.gen(input, weight, TEST_NONCE).unwrap();
            (vidpf, public, keys, *TEST_NONCE)
        }

        #[test]
        fn gen_with_keys() {
            let input = VidpfInput::from_bytes(&[0xFF]);
            let weight = TestWeight::from(vec![21.into(), 22.into(), 23.into()]);
            let vidpf = Vidpf::new(TEST_WEIGHT_LEN);
            let keys_with_same_id = [
                VidpfKey::gen(VidpfServerId::S0).unwrap(),
                VidpfKey::gen(VidpfServerId::S0).unwrap(),
            ];

            let err = vidpf
                .gen_with_keys(&keys_with_same_id, &input, &weight, TEST_NONCE)
                .unwrap_err();

            assert_eq!(err.to_string(), VidpfError::SameKeyId.to_string());
        }

        #[test]
        fn correctness_at_last_level() {
            let input = VidpfInput::from_bytes(&[0xFF]);
            let weight = TestWeight::from(vec![21.into(), 22.into(), 23.into()]);
            let (vidpf, public, [key_0, key_1], nonce) = vidpf_gen_setup(&input, &weight);

            let value_share_0 = vidpf.eval(&key_0, &public, &input, &nonce).unwrap();
            let value_share_1 = vidpf.eval(&key_1, &public, &input, &nonce).unwrap();

            assert_eq!(
                value_share_0.share + value_share_1.share,
                weight,
                "shares must add up to the expected weight",
            );

            assert_eq!(
                value_share_0.proof, value_share_1.proof,
                "proofs must be equal"
            );

            let bad_input = VidpfInput::from_bytes(&[0x00]);
            let zero = TestWeight::zero(&TEST_WEIGHT_LEN);
            let value_share_0 = vidpf.eval(&key_0, &public, &bad_input, &nonce).unwrap();
            let value_share_1 = vidpf.eval(&key_1, &public, &bad_input, &nonce).unwrap();

            assert_eq!(
                value_share_0.share + value_share_1.share,
                zero,
                "shares must add up to zero",
            );

            assert_eq!(
                value_share_0.proof, value_share_1.proof,
                "proofs must be equal"
            );
        }

        #[test]
        fn correctness_at_each_level() {
            let input = VidpfInput::from_bytes(&[0xFF]);
            let weight = TestWeight::from(vec![21.into(), 22.into(), 23.into()]);
            let (vidpf, public, keys, nonce) = vidpf_gen_setup(&input, &weight);

            assert_eval_at_each_level(&vidpf, &keys, &public, &input, &weight, &nonce);

            let bad_input = VidpfInput::from_bytes(&[0x00]);
            let zero = TestWeight::zero(&TEST_WEIGHT_LEN);

            assert_eval_at_each_level(&vidpf, &keys, &public, &bad_input, &zero, &nonce);
        }

        fn assert_eval_at_each_level(
            vidpf: &Vidpf<TestWeight, TEST_NONCE_SIZE>,
            [key_0, key_1]: &[VidpfKey; 2],
            public: &VidpfPublicShare<TestWeight>,
            input: &VidpfInput,
            weight: &TestWeight,
            nonce: &[u8; TEST_NONCE_SIZE],
        ) {
            let mut state_0 = VidpfEvalState::init_from_key(key_0);
            let mut state_1 = VidpfEvalState::init_from_key(key_1);

            let n = input.len();
            for level in 0..n {
                let share_0;
                let share_1;
                (state_0, share_0) = vidpf
                    .eval_next(key_0.id, public, input, level, &state_0, nonce)
                    .unwrap();
                (state_1, share_1) = vidpf
                    .eval_next(key_1.id, public, input, level, &state_1, nonce)
                    .unwrap();

                assert_eq!(
                    share_0 + share_1,
                    *weight,
                    "shares must add up to the expected weight at the current level: {:?}",
                    level
                );

                assert_eq!(
                    state_0.proof, state_1.proof,
                    "proofs must be equal at the current level: {:?}",
                    level
                );
            }
        }
    }

    mod weight {
        use std::io::Cursor;
        use subtle::{Choice, ConditionallyNegatable};

        use crate::{
            codec::{Encode, ParameterizedDecode},
            idpf::IdpfValue,
            vdaf::xof::{Seed, Xof, XofTurboShake128},
        };

        use super::{TestWeight, TEST_WEIGHT_LEN};

        #[test]
        fn roundtrip_codec() {
            let weight = TestWeight::from(vec![21.into(), 22.into(), 23.into()]);

            let mut bytes = vec![];
            weight.encode(&mut bytes).unwrap();

            let expected_bytes = [
                [vec![21], vec![0u8; 15]].concat(),
                [vec![22], vec![0u8; 15]].concat(),
                [vec![23], vec![0u8; 15]].concat(),
            ]
            .concat();

            assert_eq!(weight.encoded_len().unwrap(), expected_bytes.len());
            // Check endianness of encoding
            assert_eq!(bytes, expected_bytes);

            let decoded =
                TestWeight::decode_with_param(&TEST_WEIGHT_LEN, &mut Cursor::new(&bytes)).unwrap();
            assert_eq!(weight, decoded);
        }

        #[test]
        fn add_sub() {
            let [a, b] = compatible_weights();
            let mut c = a.clone();
            c += a.clone();

            assert_eq!(
                (a.clone() + b.clone()) + (a.clone() - b.clone()),
                c,
                "a: {:?} b:{:?}",
                a,
                b
            );
        }

        #[test]
        fn conditional_negate() {
            let [a, _] = compatible_weights();
            let mut c = a.clone();
            c.conditional_negate(Choice::from(0));
            let mut d = a.clone();
            d.conditional_negate(Choice::from(1));
            let zero = TestWeight::zero(&TEST_WEIGHT_LEN);

            assert_eq!(c + d, zero, "a: {:?}", a);
        }

        #[test]
        #[should_panic = "invalid weight length"]
        fn add_panics() {
            let [w0, w1] = incompatible_weights();
            let _ = w0 + w1;
        }

        #[test]
        #[should_panic = "invalid weight length"]
        fn add_assign_panics() {
            let [mut w0, w1] = incompatible_weights();
            w0 += w1;
        }

        #[test]
        #[should_panic = "invalid weight length"]
        fn sub_panics() {
            let [w0, w1] = incompatible_weights();
            let _ = w0 - w1;
        }

        #[test]
        #[should_panic = "invalid weight length"]
        fn conditional_select_panics() {
            let [w0, w1] = incompatible_weights();
            TestWeight::conditional_select(&w0, &w1, Choice::from(0));
        }

        fn compatible_weights() -> [TestWeight; 2] {
            let mut xof = XofTurboShake128::seed_stream(&Seed(Default::default()), &[], &[]);
            [
                TestWeight::generate(&mut xof, &TEST_WEIGHT_LEN),
                TestWeight::generate(&mut xof, &TEST_WEIGHT_LEN),
            ]
        }

        fn incompatible_weights() -> [TestWeight; 2] {
            let mut xof = XofTurboShake128::seed_stream(&Seed(Default::default()), &[], &[]);
            [
                TestWeight::generate(&mut xof, &TEST_WEIGHT_LEN),
                TestWeight::generate(&mut xof, &(2 * TEST_WEIGHT_LEN)),
            ]
        }
    }
}
