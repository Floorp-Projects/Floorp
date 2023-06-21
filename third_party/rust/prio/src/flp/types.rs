// SPDX-License-Identifier: MPL-2.0

//! A collection of [`Type`](crate::flp::Type) implementations.

use crate::field::{FftFriendlyFieldElement, FieldElementExt};
use crate::flp::gadgets::{BlindPolyEval, Mul, ParallelSumGadget, PolyEval};
use crate::flp::{FlpError, Gadget, Type};
use crate::polynomial::poly_range_check;
use std::convert::TryInto;
use std::marker::PhantomData;
/// The counter data type. Each measurement is `0` or `1` and the aggregate result is the sum of the measurements (i.e., the total number of `1s`).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Count<F> {
    range_checker: Vec<F>,
}

impl<F: FftFriendlyFieldElement> Count<F> {
    /// Return a new [`Count`] type instance.
    pub fn new() -> Self {
        Self {
            range_checker: poly_range_check(0, 2),
        }
    }
}

impl<F: FftFriendlyFieldElement> Default for Count<F> {
    fn default() -> Self {
        Self::new()
    }
}

impl<F: FftFriendlyFieldElement> Type for Count<F> {
    const ID: u32 = 0x00000000;
    type Measurement = F::Integer;
    type AggregateResult = F::Integer;
    type Field = F;

    fn encode_measurement(&self, value: &F::Integer) -> Result<Vec<F>, FlpError> {
        let max = F::valid_integer_try_from(1)?;
        if *value > max {
            return Err(FlpError::Encode("Count value must be 0 or 1".to_string()));
        }

        Ok(vec![F::from(*value)])
    }

    fn decode_result(&self, data: &[F], _num_measurements: usize) -> Result<F::Integer, FlpError> {
        decode_result(data)
    }

    fn gadget(&self) -> Vec<Box<dyn Gadget<F>>> {
        vec![Box::new(Mul::new(1))]
    }

    fn valid(
        &self,
        g: &mut Vec<Box<dyn Gadget<F>>>,
        input: &[F],
        joint_rand: &[F],
        _num_shares: usize,
    ) -> Result<F, FlpError> {
        self.valid_call_check(input, joint_rand)?;
        Ok(g[0].call(&[input[0], input[0]])? - input[0])
    }

    fn truncate(&self, input: Vec<F>) -> Result<Vec<F>, FlpError> {
        self.truncate_call_check(&input)?;
        Ok(input)
    }

    fn input_len(&self) -> usize {
        1
    }

    fn proof_len(&self) -> usize {
        5
    }

    fn verifier_len(&self) -> usize {
        4
    }

    fn output_len(&self) -> usize {
        self.input_len()
    }

    fn joint_rand_len(&self) -> usize {
        0
    }

    fn prove_rand_len(&self) -> usize {
        2
    }

    fn query_rand_len(&self) -> usize {
        1
    }
}

/// This sum type. Each measurement is a integer in `[0, 2^bits)` and the aggregate is the sum of
/// the measurements.
///
/// The validity circuit is based on the SIMD circuit construction of [[BBCG+19], Theorem 5.3].
///
/// [BBCG+19]: https://ia.cr/2019/188
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Sum<F: FftFriendlyFieldElement> {
    bits: usize,
    range_checker: Vec<F>,
}

impl<F: FftFriendlyFieldElement> Sum<F> {
    /// Return a new [`Sum`] type parameter. Each value of this type is an integer in range `[0,
    /// 2^bits)`.
    pub fn new(bits: usize) -> Result<Self, FlpError> {
        if !F::valid_integer_bitlength(bits) {
            return Err(FlpError::Encode(
                "invalid bits: number of bits exceeds maximum number of bits in this field"
                    .to_string(),
            ));
        }
        Ok(Self {
            bits,
            range_checker: poly_range_check(0, 2),
        })
    }
}

impl<F: FftFriendlyFieldElement> Type for Sum<F> {
    const ID: u32 = 0x00000001;
    type Measurement = F::Integer;
    type AggregateResult = F::Integer;
    type Field = F;

    fn encode_measurement(&self, summand: &F::Integer) -> Result<Vec<F>, FlpError> {
        let v = F::encode_into_bitvector_representation(summand, self.bits)?;
        Ok(v)
    }

    fn decode_result(&self, data: &[F], _num_measurements: usize) -> Result<F::Integer, FlpError> {
        decode_result(data)
    }

    fn gadget(&self) -> Vec<Box<dyn Gadget<F>>> {
        vec![Box::new(PolyEval::new(
            self.range_checker.clone(),
            self.bits,
        ))]
    }

    fn valid(
        &self,
        g: &mut Vec<Box<dyn Gadget<F>>>,
        input: &[F],
        joint_rand: &[F],
        _num_shares: usize,
    ) -> Result<F, FlpError> {
        self.valid_call_check(input, joint_rand)?;
        call_gadget_on_vec_entries(&mut g[0], input, joint_rand[0])
    }

    fn truncate(&self, input: Vec<F>) -> Result<Vec<F>, FlpError> {
        self.truncate_call_check(&input)?;
        let res = F::decode_from_bitvector_representation(&input)?;
        Ok(vec![res])
    }

    fn input_len(&self) -> usize {
        self.bits
    }

    fn proof_len(&self) -> usize {
        2 * ((1 + self.bits).next_power_of_two() - 1) + 2
    }

    fn verifier_len(&self) -> usize {
        3
    }

    fn output_len(&self) -> usize {
        1
    }

    fn joint_rand_len(&self) -> usize {
        1
    }

    fn prove_rand_len(&self) -> usize {
        1
    }

    fn query_rand_len(&self) -> usize {
        1
    }
}

/// The average type. Each measurement is an integer in `[0,2^bits)` for some `0 < bits < 64` and the
/// aggregate is the arithmetic average.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Average<F: FftFriendlyFieldElement> {
    bits: usize,
    range_checker: Vec<F>,
}

impl<F: FftFriendlyFieldElement> Average<F> {
    /// Return a new [`Average`] type parameter. Each value of this type is an integer in range `[0,
    /// 2^bits)`.
    pub fn new(bits: usize) -> Result<Self, FlpError> {
        if !F::valid_integer_bitlength(bits) {
            return Err(FlpError::Encode(
                "invalid bits: number of bits exceeds maximum number of bits in this field"
                    .to_string(),
            ));
        }
        Ok(Self {
            bits,
            range_checker: poly_range_check(0, 2),
        })
    }
}

impl<F: FftFriendlyFieldElement> Type for Average<F> {
    const ID: u32 = 0xFFFF0000;
    type Measurement = F::Integer;
    type AggregateResult = f64;
    type Field = F;

    fn encode_measurement(&self, summand: &F::Integer) -> Result<Vec<F>, FlpError> {
        let v = F::encode_into_bitvector_representation(summand, self.bits)?;
        Ok(v)
    }

    fn decode_result(&self, data: &[F], num_measurements: usize) -> Result<f64, FlpError> {
        // Compute the average from the aggregated sum.
        let data = decode_result(data)?;
        let data: u64 = data.try_into().map_err(|err| {
            FlpError::Decode(format!("failed to convert {data:?} to u64: {err}",))
        })?;
        let result = (data as f64) / (num_measurements as f64);
        Ok(result)
    }

    fn gadget(&self) -> Vec<Box<dyn Gadget<F>>> {
        vec![Box::new(PolyEval::new(
            self.range_checker.clone(),
            self.bits,
        ))]
    }

    fn valid(
        &self,
        g: &mut Vec<Box<dyn Gadget<F>>>,
        input: &[F],
        joint_rand: &[F],
        _num_shares: usize,
    ) -> Result<F, FlpError> {
        self.valid_call_check(input, joint_rand)?;
        call_gadget_on_vec_entries(&mut g[0], input, joint_rand[0])
    }

    fn truncate(&self, input: Vec<F>) -> Result<Vec<F>, FlpError> {
        self.truncate_call_check(&input)?;
        let res = F::decode_from_bitvector_representation(&input)?;
        Ok(vec![res])
    }

    fn input_len(&self) -> usize {
        self.bits
    }

    fn proof_len(&self) -> usize {
        2 * ((1 + self.bits).next_power_of_two() - 1) + 2
    }

    fn verifier_len(&self) -> usize {
        3
    }

    fn output_len(&self) -> usize {
        1
    }

    fn joint_rand_len(&self) -> usize {
        1
    }

    fn prove_rand_len(&self) -> usize {
        1
    }

    fn query_rand_len(&self) -> usize {
        1
    }
}

/// The histogram type. Each measurement is a non-negative integer and the aggregate is a histogram
/// approximating the distribution of the measurements.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Histogram<F: FftFriendlyFieldElement> {
    buckets: Vec<F::Integer>,
    range_checker: Vec<F>,
}

impl<F: FftFriendlyFieldElement> Histogram<F> {
    /// Return a new [`Histogram`] type with the given buckets.
    pub fn new(buckets: Vec<F::Integer>) -> Result<Self, FlpError> {
        if buckets.len() >= u32::MAX as usize {
            return Err(FlpError::Encode(
                "invalid buckets: number of buckets exceeds maximum permitted".to_string(),
            ));
        }

        if !buckets.is_empty() {
            for i in 0..buckets.len() - 1 {
                if buckets[i + 1] <= buckets[i] {
                    return Err(FlpError::Encode(
                        "invalid buckets: out-of-order boundary".to_string(),
                    ));
                }
            }
        }

        Ok(Self {
            buckets,
            range_checker: poly_range_check(0, 2),
        })
    }
}

impl<F: FftFriendlyFieldElement> Type for Histogram<F> {
    const ID: u32 = 0x00000002;
    type Measurement = F::Integer;
    type AggregateResult = Vec<F::Integer>;
    type Field = F;

    fn encode_measurement(&self, measurement: &F::Integer) -> Result<Vec<F>, FlpError> {
        let mut data = vec![F::zero(); self.buckets.len() + 1];

        let bucket = match self.buckets.binary_search(measurement) {
            Ok(i) => i,  // on a bucket boundary
            Err(i) => i, // smaller than the i-th bucket boundary
        };

        data[bucket] = F::one();
        Ok(data)
    }

    fn decode_result(
        &self,
        data: &[F],
        _num_measurements: usize,
    ) -> Result<Vec<F::Integer>, FlpError> {
        decode_result_vec(data, self.buckets.len() + 1)
    }

    fn gadget(&self) -> Vec<Box<dyn Gadget<F>>> {
        vec![Box::new(PolyEval::new(
            self.range_checker.to_vec(),
            self.input_len(),
        ))]
    }

    fn valid(
        &self,
        g: &mut Vec<Box<dyn Gadget<F>>>,
        input: &[F],
        joint_rand: &[F],
        num_shares: usize,
    ) -> Result<F, FlpError> {
        self.valid_call_check(input, joint_rand)?;

        // Check that each element of `input` is a 0 or 1.
        let range_check = call_gadget_on_vec_entries(&mut g[0], input, joint_rand[0])?;

        // Check that the elements of `input` sum to 1.
        let mut sum_check = -(F::one() / F::from(F::valid_integer_try_from(num_shares)?));
        for val in input.iter() {
            sum_check += *val;
        }

        // Take a random linear combination of both checks.
        let out = joint_rand[1] * range_check + (joint_rand[1] * joint_rand[1]) * sum_check;
        Ok(out)
    }

    fn truncate(&self, input: Vec<F>) -> Result<Vec<F>, FlpError> {
        self.truncate_call_check(&input)?;
        Ok(input)
    }

    fn input_len(&self) -> usize {
        self.buckets.len() + 1
    }

    fn proof_len(&self) -> usize {
        2 * ((1 + self.input_len()).next_power_of_two() - 1) + 2
    }

    fn verifier_len(&self) -> usize {
        3
    }

    fn output_len(&self) -> usize {
        self.input_len()
    }

    fn joint_rand_len(&self) -> usize {
        2
    }

    fn prove_rand_len(&self) -> usize {
        1
    }

    fn query_rand_len(&self) -> usize {
        1
    }
}

/// A sequence of integers in range `[0, 2^bits)`. This type uses a neat trick from [[BBCG+19],
/// Corollary 4.9] to reduce the proof size to roughly the square root of the input size.
///
/// [BBCG+19]: https://eprint.iacr.org/2019/188
#[derive(Debug, PartialEq, Eq)]
pub struct SumVec<F: FftFriendlyFieldElement, S> {
    range_checker: Vec<F>,
    len: usize,
    bits: usize,
    flattened_len: usize,
    max: F::Integer,
    chunk_len: usize,
    gadget_calls: usize,
    phantom: PhantomData<S>,
}

impl<F: FftFriendlyFieldElement, S: ParallelSumGadget<F, BlindPolyEval<F>>> SumVec<F, S> {
    /// Returns a new [`SumVec`] with the desired bit width and vector length.
    ///
    /// # Errors
    ///
    /// * The length of the encoded measurement, i.e., `bits * len`, overflows addressable memory.
    /// * The bit width cannot be encoded, i.e., `bits` is larger than or equal to the number of
    ///   bits required to encode field elements.
    pub fn new(bits: usize, len: usize) -> Result<Self, FlpError> {
        let flattened_len = bits.checked_mul(len).ok_or_else(|| {
            FlpError::InvalidParameter("`bits*len` overflows addressable memory".into())
        })?;

        // Check if the bit width is too large. This limit is defined to be one bit less than the
        // number of bits required to encode `F::Integer`. (One less so that we can compute `1 <<
        // bits` without overflowing.)
        let limit = std::mem::size_of::<F::Integer>() * 8 - 1;
        if bits > limit {
            return Err(FlpError::InvalidParameter(format!(
                "bit wdith exceeds limit of {limit}"
            )));
        }

        // Compute the largest encodable measurement.
        let one = F::Integer::from(F::one());
        let max = (one << bits) - one;

        // The optimal chunk length is the square root of the input length. If the input length is
        // not a perfect square, then round down. If the result is 0, then let the chunk length be
        // 1 so that the underlying gadget can still be called.
        let chunk_len = std::cmp::max(1, (flattened_len as f64).sqrt() as usize);

        let mut gadget_calls = flattened_len / chunk_len;
        if flattened_len % chunk_len != 0 {
            gadget_calls += 1;
        }

        Ok(Self {
            range_checker: poly_range_check(0, 2),
            len,
            bits,
            flattened_len,
            max,
            chunk_len,
            gadget_calls,
            phantom: PhantomData,
        })
    }
}

impl<F: FftFriendlyFieldElement, S> Clone for SumVec<F, S> {
    fn clone(&self) -> Self {
        Self {
            range_checker: self.range_checker.clone(),
            len: self.len,
            bits: self.bits,
            flattened_len: self.flattened_len,
            max: self.max,
            chunk_len: self.chunk_len,
            gadget_calls: self.gadget_calls,
            phantom: PhantomData,
        }
    }
}

impl<F, S> Type for SumVec<F, S>
where
    F: FftFriendlyFieldElement,
    S: ParallelSumGadget<F, BlindPolyEval<F>> + Eq + 'static,
{
    const ID: u32 = 0xFFFF0000;
    type Measurement = Vec<F::Integer>;
    type AggregateResult = Vec<F::Integer>;
    type Field = F;

    fn encode_measurement(&self, measurement: &Vec<F::Integer>) -> Result<Vec<F>, FlpError> {
        if measurement.len() != self.len {
            return Err(FlpError::Encode(format!(
                "unexpected measurement length: got {}; want {}",
                measurement.len(),
                self.len
            )));
        }

        let mut flattened = Vec::with_capacity(self.flattened_len);
        for summand in measurement {
            if summand > &self.max {
                return Err(FlpError::Encode(format!(
                    "summand exceeds maximum of 2^{}-1",
                    self.bits
                )));
            }
            flattened.append(&mut F::encode_into_bitvector_representation(
                summand, self.bits,
            )?);
        }

        Ok(flattened)
    }

    fn decode_result(
        &self,
        data: &[F],
        _num_measurements: usize,
    ) -> Result<Vec<F::Integer>, FlpError> {
        decode_result_vec(data, self.len)
    }

    fn gadget(&self) -> Vec<Box<dyn Gadget<F>>> {
        vec![Box::new(S::new(
            BlindPolyEval::new(self.range_checker.clone(), self.gadget_calls),
            self.chunk_len,
        ))]
    }

    fn valid(
        &self,
        g: &mut Vec<Box<dyn Gadget<F>>>,
        input: &[F],
        joint_rand: &[F],
        num_shares: usize,
    ) -> Result<F, FlpError> {
        self.valid_call_check(input, joint_rand)?;

        let s = F::from(F::valid_integer_try_from(num_shares)?).inv();
        let mut r = joint_rand[0];
        let mut outp = F::zero();
        let mut padded_chunk = vec![F::zero(); 2 * self.chunk_len];
        for chunk in input.chunks(self.chunk_len) {
            let d = chunk.len();
            for i in 0..self.chunk_len {
                if i < d {
                    padded_chunk[2 * i] = chunk[i];
                } else {
                    // If the chunk is smaller than the chunk length, then copy the last element of
                    // the chunk into the remaining slots.
                    padded_chunk[2 * i] = chunk[d - 1];
                }
                padded_chunk[2 * i + 1] = r * s;
                r *= joint_rand[0];
            }

            outp += g[0].call(&padded_chunk)?;
        }

        Ok(outp)
    }

    fn truncate(&self, input: Vec<F>) -> Result<Vec<F>, FlpError> {
        self.truncate_call_check(&input)?;
        let mut unflattened = Vec::with_capacity(self.len);
        for chunk in input.chunks(self.bits) {
            unflattened.push(F::decode_from_bitvector_representation(chunk)?);
        }
        Ok(unflattened)
    }

    fn input_len(&self) -> usize {
        self.flattened_len
    }

    fn proof_len(&self) -> usize {
        (self.chunk_len * 2) + 3 * ((1 + self.gadget_calls).next_power_of_two() - 1) + 1
    }

    fn verifier_len(&self) -> usize {
        2 + self.chunk_len * 2
    }

    fn output_len(&self) -> usize {
        self.len
    }

    fn joint_rand_len(&self) -> usize {
        1
    }

    fn prove_rand_len(&self) -> usize {
        self.chunk_len * 2
    }

    fn query_rand_len(&self) -> usize {
        1
    }
}

/// Compute a random linear combination of the result of calls of `g` on each element of `input`.
///
/// # Arguments
///
/// * `g` - The gadget to be applied elementwise
/// * `input` - The vector on whose elements to apply `g`
/// * `rnd` - The randomness used for the linear combination
pub(crate) fn call_gadget_on_vec_entries<F: FftFriendlyFieldElement>(
    g: &mut Box<dyn Gadget<F>>,
    input: &[F],
    rnd: F,
) -> Result<F, FlpError> {
    let mut range_check = F::zero();
    let mut r = rnd;
    for chunk in input.chunks(1) {
        range_check += r * g.call(chunk)?;
        r *= rnd;
    }
    Ok(range_check)
}

/// Given a vector `data` of field elements which should contain exactly one entry, return the
/// integer representation of that entry.
pub(crate) fn decode_result<F: FftFriendlyFieldElement>(
    data: &[F],
) -> Result<F::Integer, FlpError> {
    if data.len() != 1 {
        return Err(FlpError::Decode("unexpected input length".into()));
    }
    Ok(F::Integer::from(data[0]))
}

/// Given a vector `data` of field elements, return a vector containing the corresponding integer
/// representations, if the number of entries matches `expected_len`.
pub(crate) fn decode_result_vec<F: FftFriendlyFieldElement>(
    data: &[F],
    expected_len: usize,
) -> Result<Vec<F::Integer>, FlpError> {
    if data.len() != expected_len {
        return Err(FlpError::Decode("unexpected input length".into()));
    }
    Ok(data.iter().map(|elem| F::Integer::from(*elem)).collect())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::field::{random_vector, Field64 as TestField, FieldElement};
    use crate::flp::gadgets::ParallelSum;
    #[cfg(feature = "multithreaded")]
    use crate::flp::gadgets::ParallelSumMultithreaded;
    use crate::flp::types::test_utils::{flp_validity_test, ValidityTestCase};

    #[test]
    fn test_count() {
        let count: Count<TestField> = Count::new();
        let zero = TestField::zero();
        let one = TestField::one();

        // Round trip
        assert_eq!(
            count
                .decode_result(
                    &count
                        .truncate(count.encode_measurement(&1).unwrap())
                        .unwrap(),
                    1
                )
                .unwrap(),
            1,
        );

        // Test FLP on valid input.
        flp_validity_test(
            &count,
            &count.encode_measurement(&1).unwrap(),
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![one]),
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &count,
            &count.encode_measurement(&0).unwrap(),
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![zero]),
                num_shares: 3,
            },
        )
        .unwrap();

        // Test FLP on invalid input.
        flp_validity_test(
            &count,
            &[TestField::from(1337)],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();

        // Try running the validity circuit on an input that's too short.
        count.valid(&mut count.gadget(), &[], &[], 1).unwrap_err();
        count
            .valid(&mut count.gadget(), &[1.into(), 2.into()], &[], 1)
            .unwrap_err();
    }

    #[test]
    fn test_sum() {
        let sum = Sum::new(11).unwrap();
        let zero = TestField::zero();
        let one = TestField::one();
        let nine = TestField::from(9);

        // Round trip
        assert_eq!(
            sum.decode_result(
                &sum.truncate(sum.encode_measurement(&27).unwrap()).unwrap(),
                1
            )
            .unwrap(),
            27,
        );

        // Test FLP on valid input.
        flp_validity_test(
            &sum,
            &sum.encode_measurement(&1337).unwrap(),
            &ValidityTestCase {
                expect_valid: true,
                expected_output: Some(vec![TestField::from(1337)]),
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &Sum::new(0).unwrap(),
            &[],
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![zero]),
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &Sum::new(2).unwrap(),
            &[one, zero],
            &ValidityTestCase {
                expect_valid: true,
                expected_output: Some(vec![one]),
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &Sum::new(9).unwrap(),
            &[one, zero, one, one, zero, one, one, one, zero],
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![TestField::from(237)]),
                num_shares: 3,
            },
        )
        .unwrap();

        // Test FLP on invalid input.
        flp_validity_test(
            &Sum::new(3).unwrap(),
            &[one, nine, zero],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &Sum::new(5).unwrap(),
            &[zero, zero, zero, zero, nine],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();
    }

    #[test]
    fn test_average() {
        let average = Average::new(11).unwrap();
        let zero = TestField::zero();
        let one = TestField::one();
        let ten = TestField::from(10);

        // Testing that average correctly quotients the sum of the measurements
        // by the number of measurements.
        assert_eq!(average.decode_result(&[zero], 1).unwrap(), 0.0);
        assert_eq!(average.decode_result(&[one], 1).unwrap(), 1.0);
        assert_eq!(average.decode_result(&[one], 2).unwrap(), 0.5);
        assert_eq!(average.decode_result(&[one], 4).unwrap(), 0.25);
        assert_eq!(average.decode_result(&[ten], 8).unwrap(), 1.25);

        // round trip of 12 with `num_measurements`=1
        assert_eq!(
            average
                .decode_result(
                    &average
                        .truncate(average.encode_measurement(&12).unwrap())
                        .unwrap(),
                    1
                )
                .unwrap(),
            12.0
        );

        // round trip of 12 with `num_measurements`=24
        assert_eq!(
            average
                .decode_result(
                    &average
                        .truncate(average.encode_measurement(&12).unwrap())
                        .unwrap(),
                    24
                )
                .unwrap(),
            0.5
        );
    }

    #[test]
    fn test_histogram() {
        let hist = Histogram::new(vec![10, 20]).unwrap();
        let zero = TestField::zero();
        let one = TestField::one();
        let nine = TestField::from(9);

        assert_eq!(&hist.encode_measurement(&7).unwrap(), &[one, zero, zero]);
        assert_eq!(&hist.encode_measurement(&10).unwrap(), &[one, zero, zero]);
        assert_eq!(&hist.encode_measurement(&17).unwrap(), &[zero, one, zero]);
        assert_eq!(&hist.encode_measurement(&20).unwrap(), &[zero, one, zero]);
        assert_eq!(&hist.encode_measurement(&27).unwrap(), &[zero, zero, one]);

        // Round trip
        assert_eq!(
            hist.decode_result(
                &hist
                    .truncate(hist.encode_measurement(&27).unwrap())
                    .unwrap(),
                1
            )
            .unwrap(),
            [0, 0, 1]
        );

        // Invalid bucket boundaries.
        Histogram::<TestField>::new(vec![10, 0]).unwrap_err();
        Histogram::<TestField>::new(vec![10, 10]).unwrap_err();

        // Test valid inputs.
        flp_validity_test(
            &hist,
            &hist.encode_measurement(&0).unwrap(),
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![one, zero, zero]),
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &hist,
            &hist.encode_measurement(&17).unwrap(),
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![zero, one, zero]),
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &hist,
            &hist.encode_measurement(&1337).unwrap(),
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![zero, zero, one]),
                num_shares: 3,
            },
        )
        .unwrap();

        // Test invalid inputs.
        flp_validity_test(
            &hist,
            &[zero, zero, nine],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &hist,
            &[zero, one, one],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &hist,
            &[one, one, one],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();

        flp_validity_test(
            &hist,
            &[zero, zero, zero],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();
    }

    fn test_sum_vec<F, S>(f: F)
    where
        F: Fn(usize, usize) -> Result<SumVec<TestField, S>, FlpError>,
        S: 'static + ParallelSumGadget<TestField, BlindPolyEval<TestField>> + Eq,
    {
        let one = TestField::one();
        let nine = TestField::from(9);

        // Test on valid inputs.
        for len in 0..10 {
            let sum_vec = f(1, len).unwrap();
            flp_validity_test(
                &sum_vec,
                &sum_vec.encode_measurement(&vec![1; len]).unwrap(),
                &ValidityTestCase::<TestField> {
                    expect_valid: true,
                    expected_output: Some(vec![one; len]),
                    num_shares: 3,
                },
            )
            .unwrap();
        }

        let len = 100;
        let sum_vec = f(1, len).unwrap();
        flp_validity_test(
            &sum_vec,
            &sum_vec.encode_measurement(&vec![1; len]).unwrap(),
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![one; len]),
                num_shares: 3,
            },
        )
        .unwrap();

        let len = 23;
        let sum_vec = f(4, len).unwrap();
        flp_validity_test(
            &sum_vec,
            &sum_vec.encode_measurement(&vec![9; len]).unwrap(),
            &ValidityTestCase::<TestField> {
                expect_valid: true,
                expected_output: Some(vec![nine; len]),
                num_shares: 3,
            },
        )
        .unwrap();

        // Test on invalid inputs.
        for len in 1..10 {
            let sum_vec = f(1, len).unwrap();
            flp_validity_test(
                &sum_vec,
                &vec![nine; len],
                &ValidityTestCase::<TestField> {
                    expect_valid: false,
                    expected_output: None,
                    num_shares: 3,
                },
            )
            .unwrap();
        }

        let len = 23;
        let sum_vec = f(2, len).unwrap();
        flp_validity_test(
            &sum_vec,
            &vec![nine; 2 * len],
            &ValidityTestCase::<TestField> {
                expect_valid: false,
                expected_output: None,
                num_shares: 3,
            },
        )
        .unwrap();

        // Round trip
        let want = vec![1; len];
        assert_eq!(
            sum_vec
                .decode_result(
                    &sum_vec
                        .truncate(sum_vec.encode_measurement(&want).unwrap())
                        .unwrap(),
                    1
                )
                .unwrap(),
            want
        );
    }

    #[test]
    fn test_sum_vec_serial() {
        test_sum_vec(SumVec::<TestField, ParallelSum<TestField, BlindPolyEval<TestField>>>::new)
    }

    #[test]
    #[cfg(feature = "multithreaded")]
    fn test_sum_vec_parallel() {
        test_sum_vec(
            SumVec::<TestField, ParallelSumMultithreaded<TestField, BlindPolyEval<TestField>>>::new,
        )
    }

    #[test]
    fn sum_vec_serial_long() {
        let typ: SumVec<TestField, ParallelSum<TestField, BlindPolyEval<TestField>>> =
            SumVec::new(1, 1000).unwrap();
        let input = typ.encode_measurement(&vec![0; 1000]).unwrap();
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

    #[test]
    #[cfg(feature = "multithreaded")]
    fn sum_vec_parallel_long() {
        let typ: SumVec<TestField, ParallelSumMultithreaded<TestField, BlindPolyEval<TestField>>> =
            SumVec::new(1, 1000).unwrap();
        let input = typ.encode_measurement(&vec![0; 1000]).unwrap();
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
}

#[cfg(test)]
mod test_utils {
    use super::*;
    use crate::field::{random_vector, split_vector, FieldElement};

    pub(crate) struct ValidityTestCase<F> {
        pub(crate) expect_valid: bool,
        pub(crate) expected_output: Option<Vec<F>>,
        // Number of shares to split input and proofs into in `flp_test`.
        pub(crate) num_shares: usize,
    }

    pub(crate) fn flp_validity_test<T: Type>(
        typ: &T,
        input: &[T::Field],
        t: &ValidityTestCase<T::Field>,
    ) -> Result<(), FlpError> {
        let mut gadgets = typ.gadget();

        if input.len() != typ.input_len() {
            return Err(FlpError::Test(format!(
                "unexpected input length: got {}; want {}",
                input.len(),
                typ.input_len()
            )));
        }

        if typ.query_rand_len() != gadgets.len() {
            return Err(FlpError::Test(format!(
                "query rand length: got {}; want {}",
                typ.query_rand_len(),
                gadgets.len()
            )));
        }

        let joint_rand = random_vector(typ.joint_rand_len()).unwrap();
        let prove_rand = random_vector(typ.prove_rand_len()).unwrap();
        let query_rand = random_vector(typ.query_rand_len()).unwrap();

        // Run the validity circuit.
        let v = typ.valid(&mut gadgets, input, &joint_rand, 1)?;
        if v != T::Field::zero() && t.expect_valid {
            return Err(FlpError::Test(format!(
                "expected valid input: valid() returned {v}"
            )));
        }
        if v == T::Field::zero() && !t.expect_valid {
            return Err(FlpError::Test(format!(
                "expected invalid input: valid() returned {v}"
            )));
        }

        // Generate the proof.
        let proof = typ.prove(input, &prove_rand, &joint_rand)?;
        if proof.len() != typ.proof_len() {
            return Err(FlpError::Test(format!(
                "unexpected proof length: got {}; want {}",
                proof.len(),
                typ.proof_len()
            )));
        }

        // Query the proof.
        let verifier = typ.query(input, &proof, &query_rand, &joint_rand, 1)?;
        if verifier.len() != typ.verifier_len() {
            return Err(FlpError::Test(format!(
                "unexpected verifier length: got {}; want {}",
                verifier.len(),
                typ.verifier_len()
            )));
        }

        // Decide if the input is valid.
        let res = typ.decide(&verifier)?;
        if res != t.expect_valid {
            return Err(FlpError::Test(format!(
                "decision is {}; want {}",
                res, t.expect_valid,
            )));
        }

        // Run distributed FLP.
        let input_shares: Vec<Vec<T::Field>> = split_vector(input, t.num_shares)
            .unwrap()
            .into_iter()
            .collect();

        let proof_shares: Vec<Vec<T::Field>> = split_vector(&proof, t.num_shares)
            .unwrap()
            .into_iter()
            .collect();

        let verifier: Vec<T::Field> = (0..t.num_shares)
            .map(|i| {
                typ.query(
                    &input_shares[i],
                    &proof_shares[i],
                    &query_rand,
                    &joint_rand,
                    t.num_shares,
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

        let res = typ.decide(&verifier)?;
        if res != t.expect_valid {
            return Err(FlpError::Test(format!(
                "distributed decision is {}; want {}",
                res, t.expect_valid,
            )));
        }

        // Try verifying various proof mutants.
        for i in 0..proof.len() {
            let mut mutated_proof = proof.clone();
            mutated_proof[i] += T::Field::one();
            let verifier = typ.query(input, &mutated_proof, &query_rand, &joint_rand, 1)?;
            if typ.decide(&verifier)? {
                return Err(FlpError::Test(format!(
                    "decision for proof mutant {} is {}; want {}",
                    i, true, false,
                )));
            }
        }

        // Try verifying a proof that is too short.
        let mut mutated_proof = proof.clone();
        mutated_proof.truncate(gadgets[0].arity() - 1);
        if typ
            .query(input, &mutated_proof, &query_rand, &joint_rand, 1)
            .is_ok()
        {
            return Err(FlpError::Test(
                "query on short proof succeeded; want failure".to_string(),
            ));
        }

        // Try verifying a proof that is too long.
        let mut mutated_proof = proof;
        mutated_proof.extend_from_slice(&[T::Field::one(); 17]);
        if typ
            .query(input, &mutated_proof, &query_rand, &joint_rand, 1)
            .is_ok()
        {
            return Err(FlpError::Test(
                "query on long proof succeeded; want failure".to_string(),
            ));
        }

        if let Some(ref want) = t.expected_output {
            let got = typ.truncate(input.to_vec())?;

            if got.len() != typ.output_len() {
                return Err(FlpError::Test(format!(
                    "unexpected output length: got {}; want {}",
                    got.len(),
                    typ.output_len()
                )));
            }

            if &got != want {
                return Err(FlpError::Test(format!(
                    "unexpected output: got {got:?}; want {want:?}"
                )));
            }
        }

        Ok(())
    }
}

#[cfg(feature = "experimental")]
#[cfg_attr(docsrs, doc(cfg(feature = "experimental")))]
pub mod fixedpoint_l2;
