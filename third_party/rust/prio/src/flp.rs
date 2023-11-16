// SPDX-License-Identifier: MPL-2.0

//! Implementation of the generic Fully Linear Proof (FLP) system specified in
//! [[draft-irtf-cfrg-vdaf-07]]. This is the main building block of [`Prio3`](crate::vdaf::prio3).
//!
//! The FLP is derived for any implementation of the [`Type`] trait. Such an implementation
//! specifies a validity circuit that defines the set of valid measurements, as well as the finite
//! field in which the validity circuit is evaluated. It also determines how raw measurements are
//! encoded as inputs to the validity circuit, and how aggregates are decoded from sums of
//! measurements.
//!
//! # Overview
//!
//! The proof system is comprised of three algorithms. The first, `prove`, is run by the prover in
//! order to generate a proof of a statement's validity. The second and third, `query` and
//! `decide`, are run by the verifier in order to check the proof. The proof asserts that the input
//! is an element of a language recognized by the arithmetic circuit. If an input is _not_ valid,
//! then the verification step will fail with high probability:
//!
//! ```
//! use prio::flp::types::Count;
//! use prio::flp::Type;
//! use prio::field::{random_vector, FieldElement, Field64};
//!
//! // The prover chooses a measurement.
//! let count = Count::new();
//! let input: Vec<Field64> = count.encode_measurement(&0).unwrap();
//!
//! // The prover and verifier agree on "joint randomness" used to generate and
//! // check the proof. The application needs to ensure that the prover
//! // "commits" to the input before this point. In Prio3, the joint
//! // randomness is derived from additive shares of the input.
//! let joint_rand = random_vector(count.joint_rand_len()).unwrap();
//!
//! // The prover generates the proof.
//! let prove_rand = random_vector(count.prove_rand_len()).unwrap();
//! let proof = count.prove(&input, &prove_rand, &joint_rand).unwrap();
//!
//! // The verifier checks the proof. In the first step, the verifier "queries"
//! // the input and proof, getting the "verifier message" in response. It then
//! // inspects the verifier to decide if the input is valid.
//! let query_rand = random_vector(count.query_rand_len()).unwrap();
//! let verifier = count.query(&input, &proof, &query_rand, &joint_rand, 1).unwrap();
//! assert!(count.decide(&verifier).unwrap());
//! ```
//!
//! [draft-irtf-cfrg-vdaf-07]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/07/

#[cfg(feature = "experimental")]
use crate::dp::DifferentialPrivacyStrategy;
use crate::fft::{discrete_fourier_transform, discrete_fourier_transform_inv_finish, FftError};
use crate::field::{FftFriendlyFieldElement, FieldElement, FieldElementWithInteger, FieldError};
use crate::fp::log2;
use crate::polynomial::poly_eval;
use std::any::Any;
use std::convert::TryFrom;
use std::fmt::Debug;

pub mod gadgets;
pub mod types;

/// Errors propagated by methods in this module.
#[derive(Debug, thiserror::Error)]
pub enum FlpError {
    /// Calling [`Type::prove`] returned an error.
    #[error("prove error: {0}")]
    Prove(String),

    /// Calling [`Type::query`] returned an error.
    #[error("query error: {0}")]
    Query(String),

    /// Calling [`Type::decide`] returned an error.
    #[error("decide error: {0}")]
    Decide(String),

    /// Calling a gadget returned an error.
    #[error("gadget error: {0}")]
    Gadget(String),

    /// Calling the validity circuit returned an error.
    #[error("validity circuit error: {0}")]
    Valid(String),

    /// Calling [`Type::encode_measurement`] returned an error.
    #[error("value error: {0}")]
    Encode(String),

    /// Calling [`Type::decode_result`] returned an error.
    #[error("value error: {0}")]
    Decode(String),

    /// Calling [`Type::truncate`] returned an error.
    #[error("truncate error: {0}")]
    Truncate(String),

    /// Generic invalid parameter. This may be returned when an FLP type cannot be constructed.
    #[error("invalid paramter: {0}")]
    InvalidParameter(String),

    /// Returned if an FFT operation propagates an error.
    #[error("FFT error: {0}")]
    Fft(#[from] FftError),

    /// Returned if a field operation encountered an error.
    #[error("Field error: {0}")]
    Field(#[from] FieldError),

    #[cfg(feature = "experimental")]
    /// An error happened during noising.
    #[error("differential privacy error: {0}")]
    DifferentialPrivacy(#[from] crate::dp::DpError),

    /// Unit test error.
    #[cfg(test)]
    #[error("test failed: {0}")]
    Test(String),
}

/// A type. Implementations of this trait specify how a particular kind of measurement is encoded
/// as a vector of field elements and how validity of the encoded measurement is determined.
/// Validity is determined via an arithmetic circuit evaluated over the encoded measurement.
pub trait Type: Sized + Eq + Clone + Debug {
    /// The Prio3 VDAF identifier corresponding to this type.
    const ID: u32;

    /// The type of raw measurement to be encoded.
    type Measurement: Clone + Debug;

    /// The type of aggregate result for this type.
    type AggregateResult: Clone + Debug;

    /// The finite field used for this type.
    type Field: FftFriendlyFieldElement;

    /// Encodes a measurement as a vector of [`Self::input_len`] field elements.
    fn encode_measurement(
        &self,
        measurement: &Self::Measurement,
    ) -> Result<Vec<Self::Field>, FlpError>;

    /// Decode an aggregate result.
    fn decode_result(
        &self,
        data: &[Self::Field],
        num_measurements: usize,
    ) -> Result<Self::AggregateResult, FlpError>;

    /// Returns the sequence of gadgets associated with the validity circuit.
    ///
    /// # Notes
    ///
    /// The construction of [[BBCG+19], Theorem 4.3] uses a single gadget rather than many.  The
    /// idea to generalize the proof system to allow multiple gadgets is discussed briefly in
    /// [[BBCG+19], Remark 4.5], but no construction is given. The construction implemented here
    /// requires security analysis.
    ///
    /// [BBCG+19]: https://ia.cr/2019/188
    fn gadget(&self) -> Vec<Box<dyn Gadget<Self::Field>>>;

    /// Evaluates the validity circuit on an input and returns the output.
    ///
    /// # Parameters
    ///
    /// * `gadgets` is the sequence of gadgets, presumably output by [`Self::gadget`].
    /// * `input` is the input to be validated.
    /// * `joint_rand` is the joint randomness shared by the prover and verifier.
    /// * `num_shares` is the number of input shares.
    ///
    /// # Example usage
    ///
    /// Applications typically do not call this method directly. It is used internally by
    /// [`Self::prove`] and [`Self::query`] to generate and verify the proof respectively.
    ///
    /// ```
    /// use prio::flp::types::Count;
    /// use prio::flp::Type;
    /// use prio::field::{random_vector, FieldElement, Field64};
    ///
    /// let count = Count::new();
    /// let input: Vec<Field64> = count.encode_measurement(&1).unwrap();
    /// let joint_rand = random_vector(count.joint_rand_len()).unwrap();
    /// let v = count.valid(&mut count.gadget(), &input, &joint_rand, 1).unwrap();
    /// assert_eq!(v, Field64::zero());
    /// ```
    fn valid(
        &self,
        gadgets: &mut Vec<Box<dyn Gadget<Self::Field>>>,
        input: &[Self::Field],
        joint_rand: &[Self::Field],
        num_shares: usize,
    ) -> Result<Self::Field, FlpError>;

    /// Constructs an aggregatable output from an encoded input. Calling this method is only safe
    /// once `input` has been validated.
    fn truncate(&self, input: Vec<Self::Field>) -> Result<Vec<Self::Field>, FlpError>;

    /// The length in field elements of the encoded input returned by [`Self::encode_measurement`].
    fn input_len(&self) -> usize;

    /// The length in field elements of the proof generated for this type.
    fn proof_len(&self) -> usize;

    /// The length in field elements of the verifier message constructed by [`Self::query`].
    fn verifier_len(&self) -> usize;

    /// The length of the truncated output (i.e., the output of [`Type::truncate`]).
    fn output_len(&self) -> usize;

    /// The length of the joint random input.
    fn joint_rand_len(&self) -> usize;

    /// The length in field elements of the random input consumed by the prover to generate a
    /// proof. This is the same as the sum of the arity of each gadget in the validity circuit.
    fn prove_rand_len(&self) -> usize;

    /// The length in field elements of the random input consumed by the verifier to make queries
    /// against inputs and proofs. This is the same as the number of gadgets in the validity
    /// circuit.
    fn query_rand_len(&self) -> usize;

    /// Generate a proof of an input's validity. The return value is a sequence of
    /// [`Self::proof_len`] field elements.
    ///
    /// # Parameters
    ///
    /// * `input` is the input.
    /// * `prove_rand` is the prover' randomness.
    /// * `joint_rand` is the randomness shared by the prover and verifier.
    fn prove(
        &self,
        input: &[Self::Field],
        prove_rand: &[Self::Field],
        joint_rand: &[Self::Field],
    ) -> Result<Vec<Self::Field>, FlpError> {
        if input.len() != self.input_len() {
            return Err(FlpError::Prove(format!(
                "unexpected input length: got {}; want {}",
                input.len(),
                self.input_len()
            )));
        }

        if prove_rand.len() != self.prove_rand_len() {
            return Err(FlpError::Prove(format!(
                "unexpected prove randomness length: got {}; want {}",
                prove_rand.len(),
                self.prove_rand_len()
            )));
        }

        if joint_rand.len() != self.joint_rand_len() {
            return Err(FlpError::Prove(format!(
                "unexpected joint randomness length: got {}; want {}",
                joint_rand.len(),
                self.joint_rand_len()
            )));
        }

        let mut prove_rand_len = 0;
        let mut shims = self
            .gadget()
            .into_iter()
            .map(|inner| {
                let inner_arity = inner.arity();
                if prove_rand_len + inner_arity > prove_rand.len() {
                    return Err(FlpError::Prove(format!(
                        "short prove randomness: got {}; want at least {}",
                        prove_rand.len(),
                        prove_rand_len + inner_arity
                    )));
                }

                let gadget = Box::new(ProveShimGadget::new(
                    inner,
                    &prove_rand[prove_rand_len..prove_rand_len + inner_arity],
                )?) as Box<dyn Gadget<Self::Field>>;
                prove_rand_len += inner_arity;

                Ok(gadget)
            })
            .collect::<Result<Vec<_>, FlpError>>()?;
        assert_eq!(prove_rand_len, self.prove_rand_len());

        // Create a buffer for storing the proof. The buffer is longer than the proof itself; the extra
        // length is to accommodate the computation of each gadget polynomial.
        let data_len = shims
            .iter()
            .map(|shim| {
                let gadget_poly_len = gadget_poly_len(shim.degree(), wire_poly_len(shim.calls()));

                // Computing the gadget polynomial using FFT requires an amount of memory that is a
                // power of 2. Thus we choose the smallest power of 2 that is at least as large as
                // the gadget polynomial. The wire seeds are encoded in the proof, too, so we
                // include the arity of the gadget to ensure there is always enough room at the end
                // of the buffer to compute the next gadget polynomial. It's likely that the
                // memory footprint here can be reduced, with a bit of care.
                shim.arity() + gadget_poly_len.next_power_of_two()
            })
            .sum();
        let mut proof = vec![Self::Field::zero(); data_len];

        // Run the validity circuit with a sequence of "shim" gadgets that record the value of each
        // input wire of each gadget evaluation. These values are used to construct the wire
        // polynomials for each gadget in the next step.
        let _ = self.valid(&mut shims, input, joint_rand, 1)?;

        // Construct the proof.
        let mut proof_len = 0;
        for shim in shims.iter_mut() {
            let gadget = shim
                .as_any()
                .downcast_mut::<ProveShimGadget<Self::Field>>()
                .unwrap();

            // Interpolate the wire polynomials `f[0], ..., f[g_arity-1]` from the input wires of each
            // evaluation of the gadget.
            let m = wire_poly_len(gadget.calls());
            let m_inv = Self::Field::from(
                <Self::Field as FieldElementWithInteger>::Integer::try_from(m).unwrap(),
            )
            .inv();
            let mut f = vec![vec![Self::Field::zero(); m]; gadget.arity()];
            for ((coefficients, values), proof_val) in f[..gadget.arity()]
                .iter_mut()
                .zip(gadget.f_vals[..gadget.arity()].iter())
                .zip(proof[proof_len..proof_len + gadget.arity()].iter_mut())
            {
                discrete_fourier_transform(coefficients, values, m)?;
                discrete_fourier_transform_inv_finish(coefficients, m, m_inv);

                // The first point on each wire polynomial is a random value chosen by the prover. This
                // point is stored in the proof so that the verifier can reconstruct the wire
                // polynomials.
                *proof_val = values[0];
            }

            // Construct the gadget polynomial `G(f[0], ..., f[g_arity-1])` and append it to `proof`.
            let gadget_poly_len = gadget_poly_len(gadget.degree(), m);
            let start = proof_len + gadget.arity();
            let end = start + gadget_poly_len.next_power_of_two();
            gadget.call_poly(&mut proof[start..end], &f)?;
            proof_len += gadget.arity() + gadget_poly_len;
        }

        // Truncate the buffer to the size of the proof.
        assert_eq!(proof_len, self.proof_len());
        proof.truncate(proof_len);
        Ok(proof)
    }

    /// Query an input and proof and return the verifier message. The return value has length
    /// [`Self::verifier_len`].
    ///
    /// # Parameters
    ///
    /// * `input` is the input or input share.
    /// * `proof` is the proof or proof share.
    /// * `query_rand` is the verifier's randomness.
    /// * `joint_rand` is the randomness shared by the prover and verifier.
    /// * `num_shares` is the total number of input shares.
    fn query(
        &self,
        input: &[Self::Field],
        proof: &[Self::Field],
        query_rand: &[Self::Field],
        joint_rand: &[Self::Field],
        num_shares: usize,
    ) -> Result<Vec<Self::Field>, FlpError> {
        if input.len() != self.input_len() {
            return Err(FlpError::Query(format!(
                "unexpected input length: got {}; want {}",
                input.len(),
                self.input_len()
            )));
        }

        if proof.len() != self.proof_len() {
            return Err(FlpError::Query(format!(
                "unexpected proof length: got {}; want {}",
                proof.len(),
                self.proof_len()
            )));
        }

        if query_rand.len() != self.query_rand_len() {
            return Err(FlpError::Query(format!(
                "unexpected query randomness length: got {}; want {}",
                query_rand.len(),
                self.query_rand_len()
            )));
        }

        if joint_rand.len() != self.joint_rand_len() {
            return Err(FlpError::Query(format!(
                "unexpected joint randomness length: got {}; want {}",
                joint_rand.len(),
                self.joint_rand_len()
            )));
        }

        let mut proof_len = 0;
        let mut shims = self
            .gadget()
            .into_iter()
            .enumerate()
            .map(|(idx, gadget)| {
                let gadget_degree = gadget.degree();
                let gadget_arity = gadget.arity();
                let m = (1 + gadget.calls()).next_power_of_two();
                let r = query_rand[idx];

                // Make sure the query randomness isn't a root of unity. Evaluating the gadget
                // polynomial at any of these points would be a privacy violation, since these points
                // were used by the prover to construct the wire polynomials.
                if r.pow(<Self::Field as FieldElementWithInteger>::Integer::try_from(m).unwrap())
                    == Self::Field::one()
                {
                    return Err(FlpError::Query(format!(
                        "invalid query randomness: encountered 2^{m}-th root of unity"
                    )));
                }

                // Compute the length of the sub-proof corresponding to the `idx`-th gadget.
                let next_len = gadget_arity + gadget_degree * (m - 1) + 1;
                let proof_data = &proof[proof_len..proof_len + next_len];
                proof_len += next_len;

                Ok(Box::new(QueryShimGadget::new(gadget, r, proof_data)?)
                    as Box<dyn Gadget<Self::Field>>)
            })
            .collect::<Result<Vec<_>, _>>()?;

        // Create a buffer for the verifier data. This includes the output of the validity circuit and,
        // for each gadget `shim[idx].inner`, the wire polynomials evaluated at the query randomness
        // `query_rand[idx]` and the gadget polynomial evaluated at `query_rand[idx]`.
        let data_len = 1 + shims.iter().map(|shim| shim.arity() + 1).sum::<usize>();
        let mut verifier = Vec::with_capacity(data_len);

        // Run the validity circuit with a sequence of "shim" gadgets that record the inputs to each
        // wire for each gadget call. Record the output of the circuit and append it to the verifier
        // message.
        //
        // NOTE The proof of [BBC+19, Theorem 4.3] assumes that the output of the validity circuit is
        // equal to the output of the last gadget evaluation. Here we relax this assumption. This
        // should be OK, since it's possible to transform any circuit into one for which this is true.
        // (Needs security analysis.)
        let validity = self.valid(&mut shims, input, joint_rand, num_shares)?;
        verifier.push(validity);

        // Fill the buffer with the verifier message.
        for (query_rand_val, shim) in query_rand[..shims.len()].iter().zip(shims.iter_mut()) {
            let gadget = shim
                .as_any()
                .downcast_ref::<QueryShimGadget<Self::Field>>()
                .unwrap();

            // Reconstruct the wire polynomials `f[0], ..., f[g_arity-1]` and evaluate each wire
            // polynomial at query randomness value.
            let m = (1 + gadget.calls()).next_power_of_two();
            let m_inv = Self::Field::from(
                <Self::Field as FieldElementWithInteger>::Integer::try_from(m).unwrap(),
            )
            .inv();
            let mut f = vec![Self::Field::zero(); m];
            for wire in 0..gadget.arity() {
                discrete_fourier_transform(&mut f, &gadget.f_vals[wire], m)?;
                discrete_fourier_transform_inv_finish(&mut f, m, m_inv);
                verifier.push(poly_eval(&f, *query_rand_val));
            }

            // Add the value of the gadget polynomial evaluated at the query randomness value.
            verifier.push(gadget.p_at_r);
        }

        assert_eq!(verifier.len(), self.verifier_len());
        Ok(verifier)
    }

    /// Returns true if the verifier message indicates that the input from which it was generated is valid.
    fn decide(&self, verifier: &[Self::Field]) -> Result<bool, FlpError> {
        if verifier.len() != self.verifier_len() {
            return Err(FlpError::Decide(format!(
                "unexpected verifier length: got {}; want {}",
                verifier.len(),
                self.verifier_len()
            )));
        }

        // Check if the output of the circuit is 0.
        if verifier[0] != Self::Field::zero() {
            return Ok(false);
        }

        // Check that each of the proof polynomials are well-formed.
        let mut gadgets = self.gadget();
        let mut verifier_len = 1;
        for gadget in gadgets.iter_mut() {
            let next_len = 1 + gadget.arity();

            let e = gadget.call(&verifier[verifier_len..verifier_len + next_len - 1])?;
            if e != verifier[verifier_len + next_len - 1] {
                return Ok(false);
            }

            verifier_len += next_len;
        }

        Ok(true)
    }

    /// Check whether `input` and `joint_rand` have the length expected by `self`,
    /// return [`FlpError::Valid`] otherwise.
    fn valid_call_check(
        &self,
        input: &[Self::Field],
        joint_rand: &[Self::Field],
    ) -> Result<(), FlpError> {
        if input.len() != self.input_len() {
            return Err(FlpError::Valid(format!(
                "unexpected input length: got {}; want {}",
                input.len(),
                self.input_len(),
            )));
        }

        if joint_rand.len() != self.joint_rand_len() {
            return Err(FlpError::Valid(format!(
                "unexpected joint randomness length: got {}; want {}",
                joint_rand.len(),
                self.joint_rand_len()
            )));
        }

        Ok(())
    }

    /// Check if the length of `input` matches `self`'s `input_len()`,
    /// return [`FlpError::Truncate`] otherwise.
    fn truncate_call_check(&self, input: &[Self::Field]) -> Result<(), FlpError> {
        if input.len() != self.input_len() {
            return Err(FlpError::Truncate(format!(
                "Unexpected input length: got {}; want {}",
                input.len(),
                self.input_len()
            )));
        }

        Ok(())
    }
}

/// A type which supports adding noise to aggregate shares for Server Differential Privacy.
#[cfg(feature = "experimental")]
pub trait TypeWithNoise<S>: Type
where
    S: DifferentialPrivacyStrategy,
{
    /// Add noise to the aggregate share to obtain differential privacy.
    fn add_noise_to_result(
        &self,
        dp_strategy: &S,
        agg_result: &mut [Self::Field],
        num_measurements: usize,
    ) -> Result<(), FlpError>;
}

/// A gadget, a non-affine arithmetic circuit that is called when evaluating a validity circuit.
pub trait Gadget<F: FftFriendlyFieldElement>: Debug {
    /// Evaluates the gadget on input `inp` and returns the output.
    fn call(&mut self, inp: &[F]) -> Result<F, FlpError>;

    /// Evaluate the gadget on input of a sequence of polynomials. The output is written to `outp`.
    fn call_poly(&mut self, outp: &mut [F], inp: &[Vec<F>]) -> Result<(), FlpError>;

    /// Returns the arity of the gadget. This is the length of `inp` passed to `call` or
    /// `call_poly`.
    fn arity(&self) -> usize;

    /// Returns the circuit's arithmetic degree. This determines the minimum length the `outp`
    /// buffer passed to `call_poly`.
    fn degree(&self) -> usize;

    /// Returns the number of times the gadget is expected to be called.
    fn calls(&self) -> usize;

    /// This call is used to downcast a `Box<dyn Gadget<F>>` to a concrete type.
    fn as_any(&mut self) -> &mut dyn Any;
}

// A "shim" gadget used during proof generation to record the input wires each time a gadget is
// evaluated.
#[derive(Debug)]
struct ProveShimGadget<F: FftFriendlyFieldElement> {
    inner: Box<dyn Gadget<F>>,

    /// Points at which the wire polynomials are interpolated.
    f_vals: Vec<Vec<F>>,

    /// The number of times the gadget has been called so far.
    ct: usize,
}

impl<F: FftFriendlyFieldElement> ProveShimGadget<F> {
    fn new(inner: Box<dyn Gadget<F>>, prove_rand: &[F]) -> Result<Self, FlpError> {
        let mut f_vals = vec![vec![F::zero(); 1 + inner.calls()]; inner.arity()];

        for (prove_rand_val, wire_poly_vals) in
            prove_rand[..f_vals.len()].iter().zip(f_vals.iter_mut())
        {
            // Choose a random field element as the first point on the wire polynomial.
            wire_poly_vals[0] = *prove_rand_val;
        }

        Ok(Self {
            inner,
            f_vals,
            ct: 1,
        })
    }
}

impl<F: FftFriendlyFieldElement> Gadget<F> for ProveShimGadget<F> {
    fn call(&mut self, inp: &[F]) -> Result<F, FlpError> {
        for (wire_poly_vals, inp_val) in self.f_vals[..inp.len()].iter_mut().zip(inp.iter()) {
            wire_poly_vals[self.ct] = *inp_val;
        }
        self.ct += 1;
        self.inner.call(inp)
    }

    fn call_poly(&mut self, outp: &mut [F], inp: &[Vec<F>]) -> Result<(), FlpError> {
        self.inner.call_poly(outp, inp)
    }

    fn arity(&self) -> usize {
        self.inner.arity()
    }

    fn degree(&self) -> usize {
        self.inner.degree()
    }

    fn calls(&self) -> usize {
        self.inner.calls()
    }

    fn as_any(&mut self) -> &mut dyn Any {
        self
    }
}

// A "shim" gadget used during proof verification to record the points at which the intermediate
// proof polynomials are evaluated.
#[derive(Debug)]
struct QueryShimGadget<F: FftFriendlyFieldElement> {
    inner: Box<dyn Gadget<F>>,

    /// Points at which intermediate proof polynomials are interpolated.
    f_vals: Vec<Vec<F>>,

    /// Points at which the gadget polynomial is interpolated.
    p_vals: Vec<F>,

    /// The gadget polynomial evaluated on a random input `r`.
    p_at_r: F,

    /// Used to compute an index into `p_val`.
    step: usize,

    /// The number of times the gadget has been called so far.
    ct: usize,
}

impl<F: FftFriendlyFieldElement> QueryShimGadget<F> {
    fn new(inner: Box<dyn Gadget<F>>, r: F, proof_data: &[F]) -> Result<Self, FlpError> {
        let gadget_degree = inner.degree();
        let gadget_arity = inner.arity();
        let m = (1 + inner.calls()).next_power_of_two();
        let p = m * gadget_degree;

        // Each call to this gadget records the values at which intermediate proof polynomials were
        // interpolated. The first point was a random value chosen by the prover and transmitted in
        // the proof.
        let mut f_vals = vec![vec![F::zero(); 1 + inner.calls()]; gadget_arity];
        for wire in 0..gadget_arity {
            f_vals[wire][0] = proof_data[wire];
        }

        // Evaluate the gadget polynomial at roots of unity.
        let size = p.next_power_of_two();
        let mut p_vals = vec![F::zero(); size];
        discrete_fourier_transform(&mut p_vals, &proof_data[gadget_arity..], size)?;

        // The step is used to compute the element of `p_val` that will be returned by a call to
        // the gadget.
        let step = (1 << (log2(p as u128) - log2(m as u128))) as usize;

        // Evaluate the gadget polynomial `p` at query randomness `r`.
        let p_at_r = poly_eval(&proof_data[gadget_arity..], r);

        Ok(Self {
            inner,
            f_vals,
            p_vals,
            p_at_r,
            step,
            ct: 1,
        })
    }
}

impl<F: FftFriendlyFieldElement> Gadget<F> for QueryShimGadget<F> {
    fn call(&mut self, inp: &[F]) -> Result<F, FlpError> {
        for (wire_poly_vals, inp_val) in self.f_vals[..inp.len()].iter_mut().zip(inp.iter()) {
            wire_poly_vals[self.ct] = *inp_val;
        }
        let outp = self.p_vals[self.ct * self.step];
        self.ct += 1;
        Ok(outp)
    }

    fn call_poly(&mut self, _outp: &mut [F], _inp: &[Vec<F>]) -> Result<(), FlpError> {
        panic!("no-op");
    }

    fn arity(&self) -> usize {
        self.inner.arity()
    }

    fn degree(&self) -> usize {
        self.inner.degree()
    }

    fn calls(&self) -> usize {
        self.inner.calls()
    }

    fn as_any(&mut self) -> &mut dyn Any {
        self
    }
}

/// Compute the length of the wire polynomial constructed from the given number of gadget calls.
#[inline]
pub(crate) fn wire_poly_len(num_calls: usize) -> usize {
    (1 + num_calls).next_power_of_two()
}

/// Compute the length of the gadget polynomial for a gadget with the given degree and from wire
/// polynomials of the given length.
#[inline]
pub(crate) fn gadget_poly_len(gadget_degree: usize, wire_poly_len: usize) -> usize {
    gadget_degree * (wire_poly_len - 1) + 1
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::field::{random_vector, split_vector, Field128};
    use crate::flp::gadgets::{Mul, PolyEval};
    use crate::polynomial::poly_range_check;

    use std::marker::PhantomData;

    // Simple integration test for the core FLP logic. You'll find more extensive unit tests for
    // each implemented data type in src/types.rs.
    #[test]
    fn test_flp() {
        const NUM_SHARES: usize = 2;

        let typ: TestType<Field128> = TestType::new();
        let input = typ.encode_measurement(&3).unwrap();
        assert_eq!(input.len(), typ.input_len());

        let input_shares: Vec<Vec<Field128>> = split_vector(input.as_slice(), NUM_SHARES)
            .unwrap()
            .into_iter()
            .collect();

        let joint_rand = random_vector(typ.joint_rand_len()).unwrap();
        let prove_rand = random_vector(typ.prove_rand_len()).unwrap();
        let query_rand = random_vector(typ.query_rand_len()).unwrap();

        let proof = typ.prove(&input, &prove_rand, &joint_rand).unwrap();
        assert_eq!(proof.len(), typ.proof_len());

        let proof_shares: Vec<Vec<Field128>> = split_vector(&proof, NUM_SHARES)
            .unwrap()
            .into_iter()
            .collect();

        let verifier: Vec<Field128> = (0..NUM_SHARES)
            .map(|i| {
                typ.query(
                    &input_shares[i],
                    &proof_shares[i],
                    &query_rand,
                    &joint_rand,
                    NUM_SHARES,
                )
                .unwrap()
            })
            .reduce(|mut left, right| {
                for (x, y) in left.iter_mut().zip(right.iter()) {
                    *x += *y;
                }
                left
            })
            .unwrap();
        assert_eq!(verifier.len(), typ.verifier_len());

        assert!(typ.decide(&verifier).unwrap());
    }

    /// A toy type used for testing multiple gadgets. Valid inputs of this type consist of a pair
    /// of field elements `(x, y)` where `2 <= x < 5` and `x^3 == y`.
    #[derive(Clone, Debug, PartialEq, Eq)]
    struct TestType<F>(PhantomData<F>);

    impl<F> TestType<F> {
        fn new() -> Self {
            Self(PhantomData)
        }
    }

    impl<F: FftFriendlyFieldElement> Type for TestType<F> {
        const ID: u32 = 0xFFFF0000;
        type Measurement = F::Integer;
        type AggregateResult = F::Integer;
        type Field = F;

        fn valid(
            &self,
            g: &mut Vec<Box<dyn Gadget<F>>>,
            input: &[F],
            joint_rand: &[F],
            _num_shares: usize,
        ) -> Result<F, FlpError> {
            let r = joint_rand[0];
            let mut res = F::zero();

            // Check that `data[0]^3 == data[1]`.
            let mut inp = [input[0], input[0]];
            inp[0] = g[0].call(&inp)?;
            inp[0] = g[0].call(&inp)?;
            let x3_diff = inp[0] - input[1];
            res += r * x3_diff;

            // Check that `data[0]` is in the correct range.
            let x_checked = g[1].call(&[input[0]])?;
            res += (r * r) * x_checked;

            Ok(res)
        }

        fn input_len(&self) -> usize {
            2
        }

        fn proof_len(&self) -> usize {
            // First chunk
            let mul = 2 /* gadget arity */ + 2 /* gadget degree */ * (
                (1 + 2_usize /* gadget calls */).next_power_of_two() - 1) + 1;

            // Second chunk
            let poly = 1 /* gadget arity */ + 3 /* gadget degree */ * (
                (1 + 1_usize /* gadget calls */).next_power_of_two() - 1) + 1;

            mul + poly
        }

        fn verifier_len(&self) -> usize {
            // First chunk
            let mul = 1 + 2 /* gadget arity */;

            // Second chunk
            let poly = 1 + 1 /* gadget arity */;

            1 + mul + poly
        }

        fn output_len(&self) -> usize {
            self.input_len()
        }

        fn joint_rand_len(&self) -> usize {
            1
        }

        fn prove_rand_len(&self) -> usize {
            3
        }

        fn query_rand_len(&self) -> usize {
            2
        }

        fn gadget(&self) -> Vec<Box<dyn Gadget<F>>> {
            vec![
                Box::new(Mul::new(2)),
                Box::new(PolyEval::new(poly_range_check(2, 5), 1)),
            ]
        }

        fn encode_measurement(&self, measurement: &F::Integer) -> Result<Vec<F>, FlpError> {
            Ok(vec![
                F::from(*measurement),
                F::from(*measurement).pow(F::Integer::try_from(3).unwrap()),
            ])
        }

        fn truncate(&self, input: Vec<F>) -> Result<Vec<F>, FlpError> {
            Ok(input)
        }

        fn decode_result(
            &self,
            _data: &[F],
            _num_measurements: usize,
        ) -> Result<F::Integer, FlpError> {
            panic!("not implemented");
        }
    }

    // In https://github.com/divviup/libprio-rs/issues/254 an out-of-bounds bug was reported that
    // gets triggered when the size of the buffer passed to `gadget.call_poly()` is larger than
    // needed for computing the gadget polynomial.
    #[test]
    fn issue254() {
        let typ: Issue254Type<Field128> = Issue254Type::new();
        let input = typ.encode_measurement(&0).unwrap();
        assert_eq!(input.len(), typ.input_len());
        let joint_rand = random_vector(typ.joint_rand_len()).unwrap();
        let prove_rand = random_vector(typ.prove_rand_len()).unwrap();
        let query_rand = random_vector(typ.query_rand_len()).unwrap();
        let proof = typ.prove(&input, &prove_rand, &joint_rand).unwrap();
        let verifier = typ
            .query(&input, &proof, &query_rand, &joint_rand, 1)
            .unwrap();
        assert_eq!(verifier.len(), typ.verifier_len());
        assert!(typ.decide(&verifier).unwrap());
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct Issue254Type<F> {
        num_gadget_calls: [usize; 2],
        phantom: PhantomData<F>,
    }

    impl<F> Issue254Type<F> {
        fn new() -> Self {
            Self {
                // The bug is triggered when there are two gadgets, but it doesn't matter how many
                // times the second gadget is called.
                num_gadget_calls: [100, 0],
                phantom: PhantomData,
            }
        }
    }

    impl<F: FftFriendlyFieldElement> Type for Issue254Type<F> {
        const ID: u32 = 0xFFFF0000;
        type Measurement = F::Integer;
        type AggregateResult = F::Integer;
        type Field = F;

        fn valid(
            &self,
            g: &mut Vec<Box<dyn Gadget<F>>>,
            input: &[F],
            _joint_rand: &[F],
            _num_shares: usize,
        ) -> Result<F, FlpError> {
            // This is a useless circuit, as it only accepts "0". Its purpose is to exercise the
            // use of multiple gadgets, each of which is called an arbitrary number of times.
            let mut res = F::zero();
            for _ in 0..self.num_gadget_calls[0] {
                res += g[0].call(&[input[0]])?;
            }
            for _ in 0..self.num_gadget_calls[1] {
                res += g[1].call(&[input[0]])?;
            }
            Ok(res)
        }

        fn input_len(&self) -> usize {
            1
        }

        fn proof_len(&self) -> usize {
            // First chunk
            let first = 1 /* gadget arity */ + 2 /* gadget degree */ * (
                (1 + self.num_gadget_calls[0]).next_power_of_two() - 1) + 1;

            // Second chunk
            let second = 1 /* gadget arity */ + 2 /* gadget degree */ * (
                (1 + self.num_gadget_calls[1]).next_power_of_two() - 1) + 1;

            first + second
        }

        fn verifier_len(&self) -> usize {
            // First chunk
            let first = 1 + 1 /* gadget arity */;

            // Second chunk
            let second = 1 + 1 /* gadget arity */;

            1 + first + second
        }

        fn output_len(&self) -> usize {
            self.input_len()
        }

        fn joint_rand_len(&self) -> usize {
            0
        }

        fn prove_rand_len(&self) -> usize {
            // First chunk
            let first = 1; // gadget arity

            // Second chunk
            let second = 1; // gadget arity

            first + second
        }

        fn query_rand_len(&self) -> usize {
            2 // number of gadgets
        }

        fn gadget(&self) -> Vec<Box<dyn Gadget<F>>> {
            let poly = poly_range_check(0, 2); // A polynomial with degree 2
            vec![
                Box::new(PolyEval::new(poly.clone(), self.num_gadget_calls[0])),
                Box::new(PolyEval::new(poly, self.num_gadget_calls[1])),
            ]
        }

        fn encode_measurement(&self, measurement: &F::Integer) -> Result<Vec<F>, FlpError> {
            Ok(vec![F::from(*measurement)])
        }

        fn truncate(&self, input: Vec<F>) -> Result<Vec<F>, FlpError> {
            Ok(input)
        }

        fn decode_result(
            &self,
            _data: &[F],
            _num_measurements: usize,
        ) -> Result<F::Integer, FlpError> {
            panic!("not implemented");
        }
    }
}
