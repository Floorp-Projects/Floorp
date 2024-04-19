// SPDX-License-Identifier: MPL-2.0

//! A [`Type`] for summing vectors of fixed point numbers where the
//! [L2 norm](https://en.wikipedia.org/wiki/Norm_(mathematics)#Euclidean_norm)
//! of each vector is bounded by `1` and adding [discrete Gaussian
//! noise](https://arxiv.org/abs/2004.00010) in order to achieve server
//! differential privacy.
//!
//! In the following a high level overview over the inner workings of this type
//! is given and implementation details are discussed. It is not necessary for
//! using the type, but it should be helpful when trying to understand the
//! implementation.
//!
//! ### Overview
//!
//! Clients submit a vector of numbers whose values semantically lie in `[-1,1)`,
//! together with a norm in the range `[0,1)`. The validation circuit checks that
//! the norm of the vector is equal to the submitted norm, while the encoding
//! guarantees that the submitted norm lies in the correct range.
//!
//! The bound on the L2 norm allows calibration of discrete Gaussian noise added
//! after aggregation, making the procedure differentially private.
//!
//! ### Submission layout
//!
//! The client submissions contain a share of their vector and the norm
//! they claim it has.
//! The submission is a vector of field elements laid out as follows:
//! ```text
//! |---- bits_per_entry * entries ----|---- bits_for_norm ----|
//!  ^                                  ^
//!  \- the input vector entries        |
//!                                     \- the encoded norm
//! ```
//!
//! ### Different number encodings
//!
//! Let `n` denote the number of bits of the chosen fixed-point type.
//! Numbers occur in 5 different representations:
//! 1. Clients have a vector whose entries are fixed point numbers. Only those
//!    fixed point types are supported where the numbers lie in the range
//!    `[-1,1)`.
//! 2. Because norm computation happens in the validation circuit, it is done
//!    on entries encoded as field elements. That is, the same vector entries
//!    are now represented by integers in the range `[0,2^n)`, where `-1` is
//!    represented by `0` and `+1` by `2^n`.
//! 3. Because the field is not necessarily exactly of size `2^n`, but might be
//!    larger, it is not enough to encode a vector entry as in (2.) and submit
//!    it to the aggregator. Instead, in order to make sure that all submitted
//!    values are in the correct range, they are bit-encoded. (This is the same
//!    as what happens in the [`Sum`](crate::flp::types::Sum) type.)
//!    This means that instead of sending a field element in the range `[0,2^n)`,
//!    we send `n` field elements representing the bit encoding. The validation
//!    circuit can verify that all submitted "bits" are indeed either `0` or `1`.
//! 4. The computed and submitted norms are treated similar to the vector
//!    entries, but they have a different number of bits, namely `2n-2`.
//! 5. As the aggregation result is a pointwise sum of the client vectors,
//!    the numbers no longer (semantically) lie in the range `[-1,1)`, and cannot
//!    be represented by the same fixed point type as the input. Instead the
//!    decoding happens directly into a vector of floats.
//!
//! ### Fixed point encoding
//!
//! Submissions consist of encoded fixed-point numbers in `[-1,1)` represented as
//! field elements in `[0,2^n)`, where n is the number of bits the fixed-point
//! representation has. Encoding and decoding is handled by the associated functions
//! of the [`CompatibleFloat`] trait. Semantically, the following function describes
//! how a fixed-point value `x` in range `[-1,1)` is converted to a field integer:
//! ```text
//! enc : [-1,1) -> [0,2^n)
//! enc(x) = 2^(n-1) * x + 2^(n-1)
//! ```
//! The inverse is:
//! ```text
//! dec : [0,2^n) -> [-1,1)
//! dec(y) = (y - 2^(n-1)) * 2^(1-n)
//! ```
//! Note that these functions only make sense when interpreting all occuring
//! numbers as real numbers. Since our signed fixed-point numbers are encoded as
//! two's complement integers, the computation that happens in
//! [`CompatibleFloat::to_field_integer`] is actually simpler.
//!
//! ### Value `1`
//!
//! We actually do not allow the submitted norm or vector entries to be
//! exactly `1`, but rather require them to be strictly less. Supporting `1` would
//! entail a more fiddly encoding and is not necessary for our usecase.
//! The largest representable vector entry can be computed by `dec(2^n-1)`.
//! For example, it is `0.999969482421875` for `n = 16`.
//!
//! ### Norm computation
//!
//! The L2 norm of a vector xs of numbers in `[-1,1)` is given by:
//! ```text
//! norm(xs) = sqrt(sum_{x in xs} x^2)
//! ```
//! Instead of computing the norm, we make two simplifications:
//! 1. We ignore the square root, which means that we are actually computing
//!    the square of the norm.
//! 2. We want our norm computation result to be integral and in the range `[0, 2^(2n-2))`,
//!    so we can represent it in our field integers. We achieve this by multiplying with `2^(2n-2)`.
//! This means that what is actually computed in this type is the following:
//! ```text
//! our_norm(xs) = 2^(2n-2) * norm(xs)^2
//! ```
//!
//! Explained more visually, `our_norm()` is a composition of three functions:
//!
//! ```text
//!                    map of dec()                    norm()          "mult with 2^(2n-2)"
//!   vector of [0,2^n)    ->       vector of [-1,1)    ->       [0,1)         ->           [0,2^(2n-2))
//!                                         ^                      ^
//!                                         |                      |
//!                     fractions with denom of 2^(n-1)     fractions with denom of 2^(2n-2)
//! ```
//! (Note that the ranges on the LHS and RHS of `"mult with 2^(2n-2)"` are stated
//! here for vectors with a norm less than `1`.)
//!
//! Given a vector `ys` of numbers in the field integer encoding (in `[0,2^n)`),
//! this gives the following equation:
//! ```text
//! our_norm_on_encoded(ys) = our_norm([dec(y) for y in ys])
//!                         = 2^(2n-2) * sum_{y in ys} ((y - 2^(n-1)) * 2^(1-n))^2
//!                         = 2^(2n-2)
//!                           * sum_{y in ys} y^2 - 2*y*2^(n-1) + (2^(n-1))^2
//!                           * 2^(1-n)^2
//!                         = sum_{y in ys} y^2 - (2^n)*y + 2^(2n-2)
//! ```
//!
//! Let `d` denote the number of the vector entries. The maximal value the result
//! of `our_norm_on_encoded()` can take occurs in the case where all entries are
//! `2^n-1`, in which case `d * 2^(2n-2)` is an upper bound to the result. The
//! finite field used for encoding must be at least as large as this.
//! For validating that the norm of the submitted vector lies in the correct
//! range, consider the following:
//!  - The result of `norm(xs)` should be in `[0,1)`.
//!  - Thus, the result of `our_norm(xs)` should be in `[0,2^(2n-2))`.
//!  - The result of `our_norm_on_encoded(ys)` should be in `[0,2^(2n-2))`.
//! This means that the valid norms are exactly those representable with `2n-2`
//! bits.
//!
//! ### Noise and Differential Privacy
//!
//! Bounding the submission norm bounds the impact that changing a single
//! client's submission can have on the aggregate. That is, the so-called
//! L2-sensitivity of the procedure is equal to two times the norm bound, namely
//! `2^n`. Therefore, adding discrete Gaussian noise with standard deviation
//! `sigma = `(2^n)/epsilon` for some `epsilon` will make the procedure [`(epsilon^2)/2`
//! zero-concentrated differentially private](https://arxiv.org/abs/2004.00010).
//! `epsilon` is given as a parameter to the `add_noise_to_result` function, as part of the
//! `dp_strategy` argument of type [`ZCdpDiscreteGaussian`].
//!
//! ### Differences in the computation because of distribution
//!
//! In `decode_result()`, what is decoded are not the submitted entries of a
//! single client, but the sum of the the entries of all clients. We have to
//! take this into account, and cannot directly use the `dec()` function from
//! above. Instead we use:
//! ```text
//! dec'(y) = y * 2^(1-n) - c
//! ```
//! Here, `c` is the number of clients.
//!
//! ### Naming in the implementation
//!
//! The following names are used:
//!  - `self.bits_per_entry`           is `n`
//!  - `self.entries`                  is `d`
//!  - `self.bits_for_norm`            is `2n-2`
//!

pub mod compatible_float;

use crate::dp::{distributions::ZCdpDiscreteGaussian, DifferentialPrivacyStrategy, DpError};
use crate::field::{Field128, FieldElement, FieldElementWithInteger, FieldElementWithIntegerExt};
use crate::flp::gadgets::{Mul, ParallelSumGadget, PolyEval};
use crate::flp::types::fixedpoint_l2::compatible_float::CompatibleFloat;
use crate::flp::types::parallel_sum_range_checks;
use crate::flp::{FlpError, Gadget, Type, TypeWithNoise};
use crate::vdaf::xof::SeedStreamSha3;
use fixed::traits::Fixed;
use num_bigint::{BigInt, BigUint, TryFromBigIntError};
use num_integer::Integer;
use num_rational::Ratio;
use rand::{distributions::Distribution, Rng};
use rand_core::SeedableRng;
use std::{convert::TryFrom, convert::TryInto, fmt::Debug, marker::PhantomData};

/// The fixed point vector sum data type. Each measurement is a vector of fixed point numbers of
/// type `T`, and the aggregate result is the float vector of the sum of the measurements.
///
/// The validity circuit verifies that the L2 norm of each measurement is bounded by 1.
///
/// The [*fixed* crate](https://crates.io/crates/fixed) is used for fixed point numbers, in
/// particular, exactly the following types are supported:
/// `FixedI16<U15>`, `FixedI32<U31>` and `FixedI64<U63>`.
///
/// The type implements the [`TypeWithNoise`] trait. The `add_noise_to_result` function adds
/// discrete Gaussian noise to an aggregate share, calibrated to the passed privacy budget.
/// This will result in the aggregate satisfying zero-concentrated differential privacy.
///
/// Depending on the size of the vector that needs to be transmitted, a corresponding field type has
/// to be chosen for `F`. For a `n`-bit fixed point type and a `d`-dimensional vector, the field
/// modulus needs to be larger than `d * 2^(2n-2)` so there are no overflows during norm validity
/// computation.
#[derive(Clone, PartialEq, Eq)]
pub struct FixedPointBoundedL2VecSum<
    T: Fixed,
    SPoly: ParallelSumGadget<Field128, PolyEval<Field128>> + Clone,
    SMul: ParallelSumGadget<Field128, Mul<Field128>> + Clone,
> {
    bits_per_entry: usize,
    entries: usize,
    bits_for_norm: usize,
    norm_summand_poly: Vec<Field128>,
    phantom: PhantomData<(T, SPoly, SMul)>,

    // range/position constants
    range_norm_begin: usize,
    range_norm_end: usize,

    // configuration of parallel sum gadgets
    gadget0_calls: usize,
    gadget0_chunk_length: usize,
    gadget1_calls: usize,
    gadget1_chunk_length: usize,
}

impl<T, SPoly, SMul> Debug for FixedPointBoundedL2VecSum<T, SPoly, SMul>
where
    T: Fixed,
    SPoly: ParallelSumGadget<Field128, PolyEval<Field128>> + Clone,
    SMul: ParallelSumGadget<Field128, Mul<Field128>> + Clone,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("FixedPointBoundedL2VecSum")
            .field("bits_per_entry", &self.bits_per_entry)
            .field("entries", &self.entries)
            .finish()
    }
}

impl<T, SPoly, SMul> FixedPointBoundedL2VecSum<T, SPoly, SMul>
where
    T: Fixed,
    SPoly: ParallelSumGadget<Field128, PolyEval<Field128>> + Clone,
    SMul: ParallelSumGadget<Field128, Mul<Field128>> + Clone,
{
    /// Return a new [`FixedPointBoundedL2VecSum`] type parameter. Each value of this type is a
    /// fixed point vector with `entries` entries.
    pub fn new(entries: usize) -> Result<Self, FlpError> {
        // (0) initialize constants
        let fi_one = u128::from(Field128::one());

        // (I) Check that the fixed type is compatible.
        //
        // We only support fixed types that encode values in [-1,1].
        // These have a single integer bit.
        if <T as Fixed>::INT_NBITS != 1 {
            return Err(FlpError::Encode(format!(
                "Expected fixed point type with one integer bit, but got {}.",
                <T as Fixed>::INT_NBITS,
            )));
        }

        // Compute number of bits of an entry, and check that an entry fits
        // into the field.
        let bits_per_entry: usize = (<T as Fixed>::INT_NBITS + <T as Fixed>::FRAC_NBITS)
            .try_into()
            .map_err(|_| FlpError::Encode("Could not convert u32 into usize.".to_string()))?;
        if !Field128::valid_integer_bitlength(bits_per_entry) {
            return Err(FlpError::Encode(format!(
                "fixed point type bit length ({bits_per_entry}) too large for field modulus",
            )));
        }

        // (II) Check that the field is large enough for the norm.
        //
        // Valid norms encoded as field integers lie in [0,2^(2*bits - 2)).
        let bits_for_norm = 2 * bits_per_entry - 2;
        if !Field128::valid_integer_bitlength(bits_for_norm) {
            return Err(FlpError::Encode(format!(
                "maximal norm bit length ({bits_for_norm}) too large for field modulus",
            )));
        }

        // In order to compare the actual norm of the vector with the claimed
        // norm, the field needs to be able to represent all numbers that can
        // occur during the computation of the norm of any submitted vector,
        // even if its norm is not bounded by 1. Because of our encoding, an
        // upper bound to that value is `entries * 2^(2*bits - 2)` (see docs of
        // compute_norm_of_entries for details). It has to fit into the field.
        let err = Err(FlpError::Encode(format!(
            "number of entries ({entries}) not compatible with field size",
        )));

        if let Some(val) = (entries as u128).checked_mul(1 << bits_for_norm) {
            if val >= Field128::modulus() {
                return err;
            }
        } else {
            return err;
        }

        // Construct the polynomial that computes a part of the norm for a
        // single vector entry.
        //
        // the linear part is 2^n,
        // the constant part is 2^(2n-2),
        // the polynomial is:
        //   p(y) = 2^(2n-2) + -(2^n) * y + 1 * y^2
        let linear_part = fi_one << bits_per_entry;
        let constant_part = fi_one << (bits_per_entry + bits_per_entry - 2);
        let norm_summand_poly = vec![
            Field128::from(constant_part),
            -Field128::from(linear_part),
            Field128::one(),
        ];

        // Compute chunk length and number of calls for parallel sum gadgets.
        let len0 = bits_per_entry * entries + bits_for_norm;
        let gadget0_chunk_length = std::cmp::max(1, (len0 as f64).sqrt() as usize);
        let gadget0_calls = (len0 + gadget0_chunk_length - 1) / gadget0_chunk_length;

        let len1 = entries;
        let gadget1_chunk_length = std::cmp::max(1, (len1 as f64).sqrt() as usize);
        let gadget1_calls = (len1 + gadget1_chunk_length - 1) / gadget1_chunk_length;

        Ok(Self {
            bits_per_entry,
            entries,
            bits_for_norm,
            norm_summand_poly,
            phantom: PhantomData,

            // range constants
            range_norm_begin: entries * bits_per_entry,
            range_norm_end: entries * bits_per_entry + bits_for_norm,

            // configuration of parallel sum gadgets
            gadget0_calls,
            gadget0_chunk_length,
            gadget1_calls,
            gadget1_chunk_length,
        })
    }

    /// This noising function can be called on the aggregate share to make
    /// the entire aggregation process differentially private. The noise is
    /// calibrated to result in a guarantee of `1/2 * epsilon^2` zero-concentrated
    /// differential privacy, where `epsilon` is given by `dp_strategy.budget`.
    fn add_noise<R: Rng>(
        &self,
        dp_strategy: &ZCdpDiscreteGaussian,
        agg_result: &mut [Field128],
        rng: &mut R,
    ) -> Result<(), FlpError> {
        // generate and add discrete gaussian noise for each entry

        // 0. Compute sensitivity of aggregation, namely 2^n.
        let sensitivity = BigUint::from(2u128).pow(self.bits_per_entry as u32);
        // Also create a BigInt containing the field modulus.
        let modulus = BigInt::from(Field128::modulus());

        // 1. initialize sampler
        let sampler = dp_strategy.create_distribution(Ratio::from_integer(sensitivity))?;

        // 2. Generate noise for each slice entry and apply it.
        for entry in agg_result.iter_mut() {
            // (a) Generate noise.
            let noise: BigInt = sampler.sample(rng);

            // (b) Put noise into field.
            //
            // The noise is generated as BigInt, but has to fit into the Field128,
            // which has modulus `Field128::modulus()`. Thus we use `BigInt::mod_floor()`
            // to calculate `noise mod modulus`. This value fits into `u128`, and
            // can be then put into the field.
            //
            // Note: we cannot use the operator `%` here, since it is not the mathematical
            // modulus operation: for negative inputs and positive modulus it gives a
            // negative result!
            let noise: BigInt = noise.mod_floor(&modulus);
            let noise: u128 = noise.try_into().map_err(|e: TryFromBigIntError<BigInt>| {
                FlpError::DifferentialPrivacy(DpError::BigIntConversion(e))
            })?;
            let f_noise = Field128::from(Field128::valid_integer_try_from::<u128>(noise)?);

            // (c) Apply noise to each entry of the aggregate share.
            *entry += f_noise;
        }

        Ok(())
    }
}

impl<T, SPoly, SMul> Type for FixedPointBoundedL2VecSum<T, SPoly, SMul>
where
    T: Fixed + CompatibleFloat,
    SPoly: ParallelSumGadget<Field128, PolyEval<Field128>> + Eq + Clone + 'static,
    SMul: ParallelSumGadget<Field128, Mul<Field128>> + Eq + Clone + 'static,
{
    const ID: u32 = 0xFFFF0000;
    type Measurement = Vec<T>;
    type AggregateResult = Vec<f64>;
    type Field = Field128;

    fn encode_measurement(&self, fp_entries: &Vec<T>) -> Result<Vec<Field128>, FlpError> {
        if fp_entries.len() != self.entries {
            return Err(FlpError::Encode("unexpected input length".into()));
        }

        // Convert the fixed-point encoded input values to field integers. We do
        // this once here because we need them for encoding but also for
        // computing the norm.
        let integer_entries = fp_entries.iter().map(|x| x.to_field_integer());

        // (I) Vector entries.
        // Encode the integer entries bitwise, and write them into the `encoded`
        // vector.
        let mut encoded: Vec<Field128> =
            vec![Field128::zero(); self.bits_per_entry * self.entries + self.bits_for_norm];
        for (l, entry) in integer_entries.clone().enumerate() {
            Field128::fill_with_bitvector_representation(
                &entry,
                &mut encoded[l * self.bits_per_entry..(l + 1) * self.bits_per_entry],
            )?;
        }

        // (II) Vector norm.
        // Compute the norm of the input vector.
        let field_entries = integer_entries.map(Field128::from);
        let norm = compute_norm_of_entries(field_entries, self.bits_per_entry)?;
        let norm_int = u128::from(norm);

        // Write the norm into the `entries` vector.
        Field128::fill_with_bitvector_representation(
            &norm_int,
            &mut encoded[self.range_norm_begin..self.range_norm_end],
        )?;

        Ok(encoded)
    }

    fn decode_result(
        &self,
        data: &[Field128],
        num_measurements: usize,
    ) -> Result<Vec<f64>, FlpError> {
        if data.len() != self.entries {
            return Err(FlpError::Decode("unexpected input length".into()));
        }
        let num_measurements = match u128::try_from(num_measurements) {
            Ok(m) => m,
            Err(_) => {
                return Err(FlpError::Decode(
                    "number of clients is too large to fit into u128".into(),
                ))
            }
        };
        let mut res = Vec::with_capacity(data.len());
        for d in data {
            let decoded = <T as CompatibleFloat>::to_float(*d, num_measurements);
            res.push(decoded);
        }
        Ok(res)
    }

    fn gadget(&self) -> Vec<Box<dyn Gadget<Field128>>> {
        // This gadget checks that a field element is zero or one.
        // It is called for all the "bits" of the encoded entries
        // and of the encoded norm.
        let gadget0 = SMul::new(Mul::new(self.gadget0_calls), self.gadget0_chunk_length);

        // This gadget computes the square of a fixed point number, operating on
        // its encoding as a field element. It is called on each entry during
        // norm computation.
        let gadget1 = SPoly::new(
            PolyEval::new(self.norm_summand_poly.clone(), self.gadget1_calls),
            self.gadget1_chunk_length,
        );

        vec![Box::new(gadget0), Box::new(gadget1)]
    }

    fn valid(
        &self,
        g: &mut Vec<Box<dyn Gadget<Field128>>>,
        input: &[Field128],
        joint_rand: &[Field128],
        num_shares: usize,
    ) -> Result<Field128, FlpError> {
        self.valid_call_check(input, joint_rand)?;

        let f_num_shares = Field128::from(Field128::valid_integer_try_from::<usize>(num_shares)?);
        let num_shares_inverse = Field128::one() / f_num_shares;

        // Ensure that all submitted field elements are either 0 or 1.
        // This is done for:
        //  (I) all vector entries (each of them encoded in `self.bits_per_entry`
        //     field elements)
        //  (II) the submitted norm (encoded in `self.bits_for_norm` field
        //    elements)
        //
        // Since all input vector entry (field-)bits, as well as the norm bits,
        // are contiguous, we do the check directly for all bits from 0 to
        // entries*bits_per_entry + bits_for_norm.
        //
        // In order to keep the proof size down, this is done using the
        // `ParallelSum` gadget. For a similar application see the `SumVec`
        // type.
        let range_check = parallel_sum_range_checks(
            &mut g[0],
            &input[..self.range_norm_end],
            joint_rand[0],
            self.gadget0_chunk_length,
            num_shares,
        )?;

        // Compute the norm of the entries and ensure that it is the same as the
        // submitted norm. There are exactly enough bits such that a submitted
        // norm is always a valid norm (semantically in the range [0,1]). By
        // comparing submitted with actual, we make sure the actual norm is
        // valid.
        //
        // The function to compute here (see explanatory comment at the top) is
        //   norm(ys) = sum_{y in ys} y^2 - (2^n)*y + 2^(2n-2)
        //
        // This is done by the `ParallelSum` gadget `g[1]`, which evaluates the
        // inner polynomial on each (decoded) vector entry, and then sums the
        // results. Note that the gadget is not called on the whole vector at
        // once, but sequentially on chunks of size `self.gadget1_chunk_length` of
        // it. The results of these calls are accumulated in the `outp` variable.
        //
        // decode the bit-encoded entries into elements in the range [0,2^n):
        let decoded_entries: Result<Vec<_>, _> = input[0..self.entries * self.bits_per_entry]
            .chunks(self.bits_per_entry)
            .map(Field128::decode_from_bitvector_representation)
            .collect();

        // run parallel sum gadget on the decoded entries
        let computed_norm = {
            let mut outp = Field128::zero();

            // Chunks which are too short need to be extended with a share of the
            // encoded zero value, that is: 1/num_shares * (2^(n-1))
            let fi_one = u128::from(Field128::one());
            let zero_enc = Field128::from(fi_one << (self.bits_per_entry - 1));
            let zero_enc_share = zero_enc * num_shares_inverse;

            for chunk in decoded_entries?.chunks(self.gadget1_chunk_length) {
                let d = chunk.len();
                if d == self.gadget1_chunk_length {
                    outp += g[1].call(chunk)?;
                } else {
                    // If the chunk is smaller than the chunk length, extend
                    // chunk with zeros.
                    let mut padded_chunk: Vec<_> = chunk.to_owned();
                    padded_chunk.resize(self.gadget1_chunk_length, zero_enc_share);
                    outp += g[1].call(&padded_chunk)?;
                }
            }

            outp
        };

        // The submitted norm is also decoded from its bit-encoding, and
        // compared with the computed norm.
        let submitted_norm_enc = &input[self.range_norm_begin..self.range_norm_end];
        let submitted_norm = Field128::decode_from_bitvector_representation(submitted_norm_enc)?;

        let norm_check = computed_norm - submitted_norm;

        // Finally, we require both checks to be successful by computing a
        // random linear combination of them.
        let out = joint_rand[1] * range_check + (joint_rand[1] * joint_rand[1]) * norm_check;
        Ok(out)
    }

    fn truncate(&self, input: Vec<Field128>) -> Result<Vec<Self::Field>, FlpError> {
        self.truncate_call_check(&input)?;

        let mut decoded_vector = vec![];

        for i_entry in 0..self.entries {
            let start = i_entry * self.bits_per_entry;
            let end = (i_entry + 1) * self.bits_per_entry;

            let decoded = Field128::decode_from_bitvector_representation(&input[start..end])?;
            decoded_vector.push(decoded);
        }
        Ok(decoded_vector)
    }

    fn input_len(&self) -> usize {
        self.bits_per_entry * self.entries + self.bits_for_norm
    }

    fn proof_len(&self) -> usize {
        // computed via
        // `gadget.arity() + gadget.degree()
        //   * ((1 + gadget.calls()).next_power_of_two() - 1) + 1;`
        let proof_gadget_0 = (self.gadget0_chunk_length * 2)
            + 2 * ((1 + self.gadget0_calls).next_power_of_two() - 1)
            + 1;
        let proof_gadget_1 = (self.gadget1_chunk_length)
            + 2 * ((1 + self.gadget1_calls).next_power_of_two() - 1)
            + 1;

        proof_gadget_0 + proof_gadget_1
    }

    fn verifier_len(&self) -> usize {
        self.gadget0_chunk_length * 2 + self.gadget1_chunk_length + 3
    }

    fn output_len(&self) -> usize {
        self.entries
    }

    fn joint_rand_len(&self) -> usize {
        2
    }

    fn prove_rand_len(&self) -> usize {
        self.gadget0_chunk_length * 2 + self.gadget1_chunk_length
    }

    fn query_rand_len(&self) -> usize {
        2
    }
}

impl<T, SPoly, SMul> TypeWithNoise<ZCdpDiscreteGaussian>
    for FixedPointBoundedL2VecSum<T, SPoly, SMul>
where
    T: Fixed + CompatibleFloat,
    SPoly: ParallelSumGadget<Field128, PolyEval<Field128>> + Eq + Clone + 'static,
    SMul: ParallelSumGadget<Field128, Mul<Field128>> + Eq + Clone + 'static,
{
    fn add_noise_to_result(
        &self,
        dp_strategy: &ZCdpDiscreteGaussian,
        agg_result: &mut [Self::Field],
        _num_measurements: usize,
    ) -> Result<(), FlpError> {
        self.add_noise(dp_strategy, agg_result, &mut SeedStreamSha3::from_entropy())
    }
}

/// Compute the square of the L2 norm of a vector of fixed-point numbers encoded as field elements.
///
/// * `entries` - Iterator over the vector entries.
/// * `bits_per_entry` - Number of bits one entry has.
fn compute_norm_of_entries<Fs>(entries: Fs, bits_per_entry: usize) -> Result<Field128, FlpError>
where
    Fs: IntoIterator<Item = Field128>,
{
    let fi_one = u128::from(Field128::one());

    // The value that is computed here is:
    //    sum_{y in entries} 2^(2n-2) + -(2^n) * y + 1 * y^2
    //
    // Check out the norm computation bit in the explanatory comment block for
    // more information.
    //
    // Initialize `norm_accumulator`.
    let mut norm_accumulator = Field128::zero();

    // constants
    let linear_part = fi_one << bits_per_entry; // = 2^(2n-2)
    let constant_part = fi_one << (bits_per_entry + bits_per_entry - 2); // = 2^n

    // Add term for a given `entry` to `norm_accumulator`.
    for entry in entries.into_iter() {
        let summand =
            entry * entry + Field128::from(constant_part) - Field128::from(linear_part) * (entry);
        norm_accumulator += summand;
    }
    Ok(norm_accumulator)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::dp::{Rational, ZCdpBudget};
    use crate::field::{random_vector, Field128, FieldElement};
    use crate::flp::gadgets::ParallelSum;
    use crate::flp::types::test_utils::{flp_validity_test, ValidityTestCase};
    use crate::vdaf::xof::SeedStreamSha3;
    use fixed::types::extra::{U127, U14, U63};
    use fixed::{FixedI128, FixedI16, FixedI64};
    use fixed_macro::fixed;
    use rand::SeedableRng;

    #[test]
    fn test_bounded_fpvec_sum_parallel_fp16() {
        let fp16_4_inv = fixed!(0.25: I1F15);
        let fp16_8_inv = fixed!(0.125: I1F15);
        let fp16_16_inv = fixed!(0.0625: I1F15);

        let fp16_vec = vec![fp16_4_inv, fp16_8_inv, fp16_16_inv];

        // the encoded vector has the following entries:
        // enc(0.25) =  2^(n-1) * 0.25 + 2^(n-1)     = 40960
        // enc(0.125) =  2^(n-1) * 0.125 + 2^(n-1)   = 36864
        // enc(0.0625) =  2^(n-1) * 0.0625 + 2^(n-1) = 34816
        test_fixed(fp16_vec, vec![40960, 36864, 34816]);
    }

    #[test]
    fn test_bounded_fpvec_sum_parallel_fp32() {
        let fp32_4_inv = fixed!(0.25: I1F31);
        let fp32_8_inv = fixed!(0.125: I1F31);
        let fp32_16_inv = fixed!(0.0625: I1F31);

        let fp32_vec = vec![fp32_4_inv, fp32_8_inv, fp32_16_inv];
        // computed as above but with n=32
        test_fixed(fp32_vec, vec![2684354560, 2415919104, 2281701376]);
    }

    #[test]
    fn test_bounded_fpvec_sum_parallel_fp64() {
        let fp64_4_inv = fixed!(0.25: I1F63);
        let fp64_8_inv = fixed!(0.125: I1F63);
        let fp64_16_inv = fixed!(0.0625: I1F63);

        let fp64_vec = vec![fp64_4_inv, fp64_8_inv, fp64_16_inv];
        // computed as above but with n=64
        test_fixed(
            fp64_vec,
            vec![
                11529215046068469760,
                10376293541461622784,
                9799832789158199296,
            ],
        );
    }

    fn test_fixed<F: Fixed>(fp_vec: Vec<F>, enc_vec: Vec<u128>)
    where
        F: CompatibleFloat,
    {
        let n: usize = (F::INT_NBITS + F::FRAC_NBITS).try_into().unwrap();

        type Ps = ParallelSum<Field128, PolyEval<Field128>>;
        type Psm = ParallelSum<Field128, Mul<Field128>>;

        let vsum: FixedPointBoundedL2VecSum<F, Ps, Psm> =
            FixedPointBoundedL2VecSum::new(3).unwrap();
        let one = Field128::one();
        // Round trip
        assert_eq!(
            vsum.decode_result(
                &vsum
                    .truncate(vsum.encode_measurement(&fp_vec).unwrap())
                    .unwrap(),
                1
            )
            .unwrap(),
            vec!(0.25, 0.125, 0.0625)
        );

        // Noise
        let mut v = vsum
            .truncate(vsum.encode_measurement(&fp_vec).unwrap())
            .unwrap();
        let strategy = ZCdpDiscreteGaussian::from_budget(ZCdpBudget::new(
            Rational::from_unsigned(100u8, 3u8).unwrap(),
        ));
        vsum.add_noise(&strategy, &mut v, &mut SeedStreamSha3::from_seed([0u8; 16]))
            .unwrap();
        assert_eq!(
            vsum.decode_result(&v, 1).unwrap(),
            match n {
                // sensitivity depends on encoding so the noise differs
                16 => vec![0.150604248046875, 0.139373779296875, -0.03759765625],
                32 => vec![0.3051439793780446, 0.1226568529382348, 0.08595499861985445],
                64 => vec![0.2896077990915178, 0.16115188007715098, 0.0788390114728425],
                _ => panic!("unsupported bitsize"),
            }
        );

        // encoded norm does not match computed norm
        let mut input: Vec<Field128> = vsum.encode_measurement(&fp_vec).unwrap();
        assert_eq!(input[0], Field128::zero());
        input[0] = one; // it was zero
        flp_validity_test(
            &vsum,
            &input,
            &ValidityTestCase::<Field128> {
                expect_valid: false,
                expected_output: Some(vec![
                    Field128::from(enc_vec[0] + 1), // = enc(0.25) + 2^0
                    Field128::from(enc_vec[1]),
                    Field128::from(enc_vec[2]),
                ]),
                num_shares: 3,
            },
        )
        .unwrap();

        // encoding contains entries that are not zero or one
        let mut input2: Vec<Field128> = vsum.encode_measurement(&fp_vec).unwrap();
        input2[0] = one + one;
        flp_validity_test(
            &vsum,
            &input2,
            &ValidityTestCase::<Field128> {
                expect_valid: false,
                expected_output: Some(vec![
                    Field128::from(enc_vec[0] + 2), // = enc(0.25) + 2*2^0
                    Field128::from(enc_vec[1]),
                    Field128::from(enc_vec[2]),
                ]),
                num_shares: 3,
            },
        )
        .unwrap();

        // norm is too big
        // 2^n - 1, the field element encoded by the all-1 vector
        let one_enc = Field128::from(((2_u128) << (n - 1)) - 1);
        flp_validity_test(
            &vsum,
            &vec![one; 3 * n + 2 * n - 2], // all vector entries and the norm are all-1-vectors
            &ValidityTestCase::<Field128> {
                expect_valid: false,
                expected_output: Some(vec![one_enc; 3]),
                num_shares: 3,
            },
        )
        .unwrap();

        // invalid submission length, should be 3n + (2*n - 2) for a
        // 3-element n-bit vector. 3*n bits for 3 entries, (2*n-2) for norm.
        let joint_rand = random_vector(vsum.joint_rand_len()).unwrap();
        vsum.valid(
            &mut vsum.gadget(),
            &vec![one; 3 * n + 2 * n - 1],
            &joint_rand,
            1,
        )
        .unwrap_err();

        // test that the zero vector has correct norm, where zero is encoded as:
        // enc(0) = 2^(n-1) * 0 + 2^(n-1)
        let zero_enc = Field128::from((2_u128) << (n - 2));
        {
            let entries = vec![zero_enc; 3];
            let norm = compute_norm_of_entries(entries, vsum.bits_per_entry).unwrap();
            let expected_norm = Field128::from(0);
            assert_eq!(norm, expected_norm);
        }

        // ensure that no overflow occurs with largest possible norm
        {
            // the largest possible entries (2^n-1)
            let entries = vec![one_enc; 3];
            let norm = compute_norm_of_entries(entries, vsum.bits_per_entry).unwrap();
            let expected_norm = Field128::from(3 * (1 + (1 << (2 * n - 2)) - (1 << n)));
            // = 3 * ((2^n-1)^2 - (2^n-1)*2^16 + 2^(2*n-2))
            assert_eq!(norm, expected_norm);

            // the smallest possible entries (0)
            let entries = vec![Field128::from(0), Field128::from(0), Field128::from(0)];
            let norm = compute_norm_of_entries(entries, vsum.bits_per_entry).unwrap();
            let expected_norm = Field128::from(3 * (1 << (2 * n - 2)));
            // = 3 * (0^2 - 0*2^n + 2^(2*n-2))
            assert_eq!(norm, expected_norm);
        }
    }

    #[test]
    fn test_bounded_fpvec_sum_parallel_invalid_args() {
        // invalid initialization
        // fixed point too large
        <FixedPointBoundedL2VecSum<
            FixedI128<U127>,
            ParallelSum<Field128, PolyEval<Field128>>,
            ParallelSum<Field128, Mul<Field128>>,
        >>::new(3)
        .unwrap_err();
        // vector too large
        <FixedPointBoundedL2VecSum<
            FixedI64<U63>,
            ParallelSum<Field128, PolyEval<Field128>>,
            ParallelSum<Field128, Mul<Field128>>,
        >>::new(3000000000)
        .unwrap_err();
        // fixed point type has more than one int bit
        <FixedPointBoundedL2VecSum<
            FixedI16<U14>,
            ParallelSum<Field128, PolyEval<Field128>>,
            ParallelSum<Field128, Mul<Field128>>,
        >>::new(3)
        .unwrap_err();
    }
}
