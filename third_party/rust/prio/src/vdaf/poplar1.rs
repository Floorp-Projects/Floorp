// SPDX-License-Identifier: MPL-2.0

//! **(NOTE: This module is experimental. Applications should not use it yet.)** This module
//! partially implements the core component of the Poplar protocol [[BBCG+21]]. Named for the
//! Poplar1 [[draft-irtf-cfrg-vdaf-01]], the specification of this VDAF is under active
//! development. Thus this code should be regarded as experimental and not compliant with any
//! existing speciication.
//!
//! TODO Make the input shares stateful so that applications can efficiently evaluate the IDPF over
//! multiple rounds. Question: Will this require API changes to [`crate::vdaf::Vdaf`]?
//!
//! TODO Update trait [`Idpf`] so that the IDPF can have a different field type at the leaves than
//! at the inner nodes.
//!
//! TODO Implement the efficient IDPF of [[BBCG+21]]. [`ToyIdpf`] is not space efficient and is
//! merely intended as a proof-of-concept.
//!
//! [BBCG+21]: https://eprint.iacr.org/2021/017
//! [draft-irtf-cfrg-vdaf-01]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/01/

use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};
use std::convert::{TryFrom, TryInto};
use std::fmt::Debug;
use std::io::Cursor;
use std::iter::FromIterator;
use std::marker::PhantomData;

use crate::codec::{
    decode_u16_items, decode_u24_items, encode_u16_items, encode_u24_items, CodecError, Decode,
    Encode, ParameterizedDecode,
};
use crate::field::{split_vector, FieldElement};
use crate::fp::log2;
use crate::prng::Prng;
use crate::vdaf::prg::{Prg, Seed};
use crate::vdaf::{
    Aggregatable, AggregateShare, Aggregator, Client, Collector, OutputShare, PrepareTransition,
    Share, ShareDecodingParameter, Vdaf, VdafError,
};

/// An input for an IDPF ([`Idpf`]).
///
/// TODO Make this an associated type of `Idpf`.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct IdpfInput {
    index: usize,
    level: usize,
}

impl IdpfInput {
    /// Constructs an IDPF input using the first `level` bits of `data`.
    pub fn new(data: &[u8], level: usize) -> Result<Self, VdafError> {
        if level > data.len() << 3 {
            return Err(VdafError::Uncategorized(format!(
                "desired bit length ({} bits) exceeds data length ({} bytes)",
                level,
                data.len()
            )));
        }

        let mut index = 0;
        let mut i = 0;
        for byte in data {
            for j in 0..8 {
                let bit = (byte >> j) & 1;
                if i < level {
                    index |= (bit as usize) << i;
                }
                i += 1;
            }
        }

        Ok(Self { index, level })
    }

    /// Construct a new input that is a prefix of `self`. Bounds checking is performed by the
    /// caller.
    fn prefix(&self, level: usize) -> Self {
        let index = self.index & ((1 << level) - 1);
        Self { index, level }
    }

    /// Return the position of `self` in the look-up table of `ToyIdpf`.
    fn data_index(&self) -> usize {
        self.index | (1 << self.level)
    }
}

impl Ord for IdpfInput {
    fn cmp(&self, other: &Self) -> Ordering {
        match self.level.cmp(&other.level) {
            Ordering::Equal => self.index.cmp(&other.index),
            ord => ord,
        }
    }
}

impl PartialOrd for IdpfInput {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Encode for IdpfInput {
    fn encode(&self, bytes: &mut Vec<u8>) {
        (self.index as u64).encode(bytes);
        (self.level as u64).encode(bytes);
    }
}

impl Decode for IdpfInput {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let index = u64::decode(bytes)? as usize;
        let level = u64::decode(bytes)? as usize;

        Ok(Self { index, level })
    }
}

/// An Incremental Distributed Point Function (IDPF), as defined by [[BBCG+21]].
///
/// [BBCG+21]: https://eprint.iacr.org/2021/017
//
// NOTE(cjpatton) The real IDPF API probably needs to be stateful.
pub trait Idpf<const KEY_LEN: usize, const OUT_LEN: usize>:
    Sized + Clone + Debug + Encode + Decode
{
    /// The finite field over which the IDPF is defined.
    //
    // NOTE(cjpatton) The IDPF of [BBCG+21] might use different fields for different levels of the
    // prefix tree.
    type Field: FieldElement;

    /// Generate and return a sequence of IDPF shares for `input`. Parameter `output` is an
    /// iterator that is invoked to get the output value for each successive level of the prefix
    /// tree.
    fn gen<M: IntoIterator<Item = [Self::Field; OUT_LEN]>>(
        input: &IdpfInput,
        values: M,
    ) -> Result<[Self; KEY_LEN], VdafError>;

    /// Evaluate an IDPF share on `prefix`.
    fn eval(&self, prefix: &IdpfInput) -> Result<[Self::Field; OUT_LEN], VdafError>;
}

/// A "toy" IDPF used for demonstration purposes. The space consumed by each share is `O(2^n)`,
/// where `n` is the length of the input. The size of each share is restricted to 1MB, so this IDPF
/// is only suitable for very short inputs.
//
// NOTE(cjpatton) It would be straight-forward to generalize this construction to any `KEY_LEN` and
// `OUT_LEN`.
#[derive(Debug, Clone)]
pub struct ToyIdpf<F> {
    data0: Vec<F>,
    data1: Vec<F>,
    level: usize,
}

impl<F: FieldElement> Idpf<2, 2> for ToyIdpf<F> {
    type Field = F;

    fn gen<M: IntoIterator<Item = [Self::Field; 2]>>(
        input: &IdpfInput,
        values: M,
    ) -> Result<[Self; 2], VdafError> {
        const MAX_DATA_BYTES: usize = 1024 * 1024; // 1MB

        let max_input_len =
            usize::try_from(log2((MAX_DATA_BYTES / F::ENCODED_SIZE) as u128)).unwrap();
        if input.level > max_input_len {
            return Err(VdafError::Uncategorized(format!(
                "input length ({}) exceeds maximum of ({})",
                input.level, max_input_len
            )));
        }

        let data_len = 1 << (input.level + 1);
        let mut data0 = vec![F::zero(); data_len];
        let mut data1 = vec![F::zero(); data_len];
        let mut values = values.into_iter();
        for level in 0..input.level + 1 {
            let value = values.next().unwrap();
            let index = input.prefix(level).data_index();
            data0[index] = value[0];
            data1[index] = value[1];
        }

        let mut data0 = split_vector(&data0, 2)?.into_iter();
        let mut data1 = split_vector(&data1, 2)?.into_iter();
        Ok([
            ToyIdpf {
                data0: data0.next().unwrap(),
                data1: data1.next().unwrap(),
                level: input.level,
            },
            ToyIdpf {
                data0: data0.next().unwrap(),
                data1: data1.next().unwrap(),
                level: input.level,
            },
        ])
    }

    fn eval(&self, prefix: &IdpfInput) -> Result<[F; 2], VdafError> {
        if prefix.level > self.level {
            return Err(VdafError::Uncategorized(format!(
                "prefix length ({}) exceeds input length ({})",
                prefix.level, self.level
            )));
        }

        let index = prefix.data_index();
        Ok([self.data0[index], self.data1[index]])
    }
}

impl<F: FieldElement> Encode for ToyIdpf<F> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        encode_u24_items(bytes, &(), &self.data0);
        encode_u24_items(bytes, &(), &self.data1);
        (self.level as u64).encode(bytes);
    }
}

impl<F: FieldElement> Decode for ToyIdpf<F> {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let data0 = decode_u24_items(&(), bytes)?;
        let data1 = decode_u24_items(&(), bytes)?;
        let level = u64::decode(bytes)? as usize;

        Ok(Self {
            data0,
            data1,
            level,
        })
    }
}

impl Encode for BTreeSet<IdpfInput> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        // Encodes the aggregation parameter as a variable length vector of
        // [`IdpfInput`], because the size of the aggregation parameter is not
        // determined by the VDAF.
        let items: Vec<IdpfInput> = self.iter().map(IdpfInput::clone).collect();
        encode_u24_items(bytes, &(), &items);
    }
}

impl Decode for BTreeSet<IdpfInput> {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let inputs = decode_u24_items(&(), bytes)?;
        Ok(Self::from_iter(inputs.into_iter()))
    }
}

/// An input share for the `poplar1` VDAF.
#[derive(Debug, Clone)]
pub struct Poplar1InputShare<I: Idpf<2, 2>, const L: usize> {
    /// IDPF share of input
    idpf: I,

    /// PRNG seed used to generate the aggregator's share of the randomness used in the first part
    /// of the sketching protocol.
    sketch_start_seed: Seed<L>,

    /// Aggregator's share of the randomness used in the second part of the sketching protocol.
    sketch_next: Share<I::Field, L>,
}

impl<I: Idpf<2, 2>, const L: usize> Encode for Poplar1InputShare<I, L> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.idpf.encode(bytes);
        self.sketch_start_seed.encode(bytes);
        self.sketch_next.encode(bytes);
    }
}

impl<'a, I, P, const L: usize> ParameterizedDecode<(&'a Poplar1<I, P, L>, usize)>
    for Poplar1InputShare<I, L>
where
    I: Idpf<2, 2>,
{
    fn decode_with_param(
        (poplar1, agg_id): &(&'a Poplar1<I, P, L>, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        let idpf = I::decode(bytes)?;
        let sketch_start_seed = Seed::decode(bytes)?;
        let is_leader = role_try_from(*agg_id).map_err(|e| CodecError::Other(Box::new(e)))?;

        let share_decoding_parameter = if is_leader {
            // The sketch is two field elements for every bit of input, plus two more, corresponding
            // to construction of shares in `Poplar1::shard`.
            ShareDecodingParameter::Leader((poplar1.input_length + 1) * 2)
        } else {
            ShareDecodingParameter::Helper
        };

        let sketch_next =
            <Share<I::Field, L>>::decode_with_param(&share_decoding_parameter, bytes)?;

        Ok(Self {
            idpf,
            sketch_start_seed,
            sketch_next,
        })
    }
}

/// The poplar1 VDAF.
#[derive(Debug)]
pub struct Poplar1<I, P, const L: usize> {
    input_length: usize,
    phantom: PhantomData<(I, P)>,
}

impl<I, P, const L: usize> Poplar1<I, P, L> {
    /// Create an instance of the poplar1 VDAF. The caller provides a cipher suite `suite` used for
    /// deriving pseudorandom sequences of field elements, and a input length in bits, corresponding
    /// to `BITS` as defined in the [VDAF specification][1].
    ///
    /// [1]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/01/
    pub fn new(bits: usize) -> Self {
        Self {
            input_length: bits,
            phantom: PhantomData,
        }
    }
}

impl<I, P, const L: usize> Clone for Poplar1<I, P, L> {
    fn clone(&self) -> Self {
        Self::new(self.input_length)
    }
}
impl<I, P, const L: usize> Vdaf for Poplar1<I, P, L>
where
    I: Idpf<2, 2>,
    P: Prg<L>,
{
    type Measurement = IdpfInput;
    type AggregateResult = BTreeMap<IdpfInput, u64>;
    type AggregationParam = BTreeSet<IdpfInput>;
    type InputShare = Poplar1InputShare<I, L>;
    type OutputShare = OutputShare<I::Field>;
    type AggregateShare = AggregateShare<I::Field>;

    fn num_aggregators(&self) -> usize {
        2
    }
}

impl<I, P, const L: usize> Client for Poplar1<I, P, L>
where
    I: Idpf<2, 2>,
    P: Prg<L>,
{
    #[allow(clippy::many_single_char_names)]
    fn shard(&self, input: &IdpfInput) -> Result<Vec<Poplar1InputShare<I, L>>, VdafError> {
        let idpf_values: Vec<[I::Field; 2]> = Prng::new()?
            .take(input.level + 1)
            .map(|k| [I::Field::one(), k])
            .collect();

        // For each level of the prefix tree, generate correlated randomness that the aggregators use
        // to validate the output. See [BBCG+21, Appendix C.4].
        let leader_sketch_start_seed = Seed::generate()?;
        let helper_sketch_start_seed = Seed::generate()?;
        let helper_sketch_next_seed = Seed::generate()?;
        let mut leader_sketch_start_prng: Prng<I::Field, _> =
            Prng::from_seed_stream(P::seed_stream(&leader_sketch_start_seed, b""));
        let mut helper_sketch_start_prng: Prng<I::Field, _> =
            Prng::from_seed_stream(P::seed_stream(&helper_sketch_start_seed, b""));
        let mut helper_sketch_next_prng: Prng<I::Field, _> =
            Prng::from_seed_stream(P::seed_stream(&helper_sketch_next_seed, b""));
        let mut leader_sketch_next: Vec<I::Field> = Vec::with_capacity(2 * idpf_values.len());
        for value in idpf_values.iter() {
            let k = value[1];

            // [BBCG+21, Appendix C.4]
            //
            // $(a, b, c)$
            let a = leader_sketch_start_prng.get() + helper_sketch_start_prng.get();
            let b = leader_sketch_start_prng.get() + helper_sketch_start_prng.get();
            let c = leader_sketch_start_prng.get() + helper_sketch_start_prng.get();

            // $A = -2a + k$
            // $B = a^2 + b + -ak + c$
            let d = k - (a + a);
            let e = (a * a) + b - (a * k) + c;
            leader_sketch_next.push(d - helper_sketch_next_prng.get());
            leader_sketch_next.push(e - helper_sketch_next_prng.get());
        }

        // Generate IDPF shares of the data and authentication vectors.
        let idpf_shares = I::gen(input, idpf_values)?;

        Ok(vec![
            Poplar1InputShare {
                idpf: idpf_shares[0].clone(),
                sketch_start_seed: leader_sketch_start_seed,
                sketch_next: Share::Leader(leader_sketch_next),
            },
            Poplar1InputShare {
                idpf: idpf_shares[1].clone(),
                sketch_start_seed: helper_sketch_start_seed,
                sketch_next: Share::Helper(helper_sketch_next_seed),
            },
        ])
    }
}

fn get_level(agg_param: &BTreeSet<IdpfInput>) -> Result<usize, VdafError> {
    let mut level = None;
    for prefix in agg_param {
        if let Some(l) = level {
            if prefix.level != l {
                return Err(VdafError::Uncategorized(
                    "prefixes must all have the same length".to_string(),
                ));
            }
        } else {
            level = Some(prefix.level);
        }
    }

    match level {
        Some(level) => Ok(level),
        None => Err(VdafError::Uncategorized("prefix set is empty".to_string())),
    }
}

impl<I, P, const L: usize> Aggregator<L> for Poplar1<I, P, L>
where
    I: Idpf<2, 2>,
    P: Prg<L>,
{
    type PrepareState = Poplar1PrepareState<I::Field>;
    type PrepareShare = Poplar1PrepareMessage<I::Field>;
    type PrepareMessage = Poplar1PrepareMessage<I::Field>;

    #[allow(clippy::type_complexity)]
    fn prepare_init(
        &self,
        verify_key: &[u8; L],
        agg_id: usize,
        agg_param: &BTreeSet<IdpfInput>,
        nonce: &[u8],
        input_share: &Self::InputShare,
    ) -> Result<
        (
            Poplar1PrepareState<I::Field>,
            Poplar1PrepareMessage<I::Field>,
        ),
        VdafError,
    > {
        let level = get_level(agg_param)?;
        let is_leader = role_try_from(agg_id)?;

        // Derive the verification randomness.
        let mut p = P::init(verify_key);
        p.update(nonce);
        let mut verify_rand_prng: Prng<I::Field, _> = Prng::from_seed_stream(p.into_seed_stream());

        // Evaluate the IDPF shares and compute the polynomial coefficients.
        let mut z = [I::Field::zero(); 3];
        let mut output_share = Vec::with_capacity(agg_param.len());
        for prefix in agg_param.iter() {
            let value = input_share.idpf.eval(prefix)?;
            let (v, k) = (value[0], value[1]);
            let r = verify_rand_prng.get();

            // [BBCG+21, Appendix C.4]
            //
            // $(z_\sigma, z^*_\sigma, z^{**}_\sigma)$
            let tmp = r * v;
            z[0] += tmp;
            z[1] += r * tmp;
            z[2] += r * k;
            output_share.push(v);
        }

        // [BBCG+21, Appendix C.4]
        //
        // Add blind shares $(a_\sigma b_\sigma, c_\sigma)$
        //
        // NOTE(cjpatton) We can make this faster by a factor of 3 by using three seed shares instead
        // of one. On the other hand, if the input shares are made stateful, then we could store
        // the PRNG state theire and avoid fast-forwarding.
        let mut prng = Prng::<I::Field, _>::from_seed_stream(P::seed_stream(
            &input_share.sketch_start_seed,
            b"",
        ))
        .skip(3 * level);
        z[0] += prng.next().unwrap();
        z[1] += prng.next().unwrap();
        z[2] += prng.next().unwrap();

        let (d, e) = match &input_share.sketch_next {
            Share::Leader(data) => (data[2 * level], data[2 * level + 1]),
            Share::Helper(seed) => {
                let mut prng = Prng::<I::Field, _>::from_seed_stream(P::seed_stream(seed, b""))
                    .skip(2 * level);
                (prng.next().unwrap(), prng.next().unwrap())
            }
        };

        let x = if is_leader {
            I::Field::one()
        } else {
            I::Field::zero()
        };

        Ok((
            Poplar1PrepareState {
                sketch: SketchState::RoundOne,
                output_share: OutputShare(output_share),
                d,
                e,
                x,
            },
            Poplar1PrepareMessage(z.to_vec()),
        ))
    }

    fn prepare_preprocess<M: IntoIterator<Item = Poplar1PrepareMessage<I::Field>>>(
        &self,
        inputs: M,
    ) -> Result<Poplar1PrepareMessage<I::Field>, VdafError> {
        let mut output: Option<Vec<I::Field>> = None;
        let mut count = 0;
        for data_share in inputs.into_iter() {
            count += 1;
            if let Some(ref mut data) = output {
                if data_share.0.len() != data.len() {
                    return Err(VdafError::Uncategorized(format!(
                        "unexpected message length: got {}; want {}",
                        data_share.0.len(),
                        data.len(),
                    )));
                }

                for (x, y) in data.iter_mut().zip(data_share.0.iter()) {
                    *x += *y;
                }
            } else {
                output = Some(data_share.0);
            }
        }

        if count != 2 {
            return Err(VdafError::Uncategorized(format!(
                "unexpected message count: got {}; want 2",
                count,
            )));
        }

        Ok(Poplar1PrepareMessage(output.unwrap()))
    }

    fn prepare_step(
        &self,
        mut state: Poplar1PrepareState<I::Field>,
        msg: Poplar1PrepareMessage<I::Field>,
    ) -> Result<PrepareTransition<Self, L>, VdafError> {
        match &state.sketch {
            SketchState::RoundOne => {
                if msg.0.len() != 3 {
                    return Err(VdafError::Uncategorized(format!(
                        "unexpected message length ({:?}): got {}; want 3",
                        state.sketch,
                        msg.0.len(),
                    )));
                }

                // Compute polynomial coefficients.
                let z: [I::Field; 3] = msg.0.try_into().unwrap();
                let y_share =
                    vec![(state.d * z[0]) + state.e + state.x * ((z[0] * z[0]) - z[1] - z[2])];

                state.sketch = SketchState::RoundTwo;
                Ok(PrepareTransition::Continue(
                    state,
                    Poplar1PrepareMessage(y_share),
                ))
            }

            SketchState::RoundTwo => {
                if msg.0.len() != 1 {
                    return Err(VdafError::Uncategorized(format!(
                        "unexpected message length ({:?}): got {}; want 1",
                        state.sketch,
                        msg.0.len(),
                    )));
                }

                let y = msg.0[0];
                if y != I::Field::zero() {
                    return Err(VdafError::Uncategorized(format!(
                        "output is invalid: polynomial evaluated to {}; want {}",
                        y,
                        I::Field::zero(),
                    )));
                }

                Ok(PrepareTransition::Finish(state.output_share))
            }
        }
    }

    fn aggregate<M: IntoIterator<Item = OutputShare<I::Field>>>(
        &self,
        agg_param: &BTreeSet<IdpfInput>,
        output_shares: M,
    ) -> Result<AggregateShare<I::Field>, VdafError> {
        let mut agg_share = AggregateShare(vec![I::Field::zero(); agg_param.len()]);
        for output_share in output_shares.into_iter() {
            agg_share.accumulate(&output_share)?;
        }

        Ok(agg_share)
    }
}

/// A prepare message sent exchanged between Poplar1 aggregators
#[derive(Clone, Debug)]
pub struct Poplar1PrepareMessage<F>(Vec<F>);

impl<F> AsRef<[F]> for Poplar1PrepareMessage<F> {
    fn as_ref(&self) -> &[F] {
        &self.0
    }
}

impl<F: FieldElement> Encode for Poplar1PrepareMessage<F> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        // TODO: This is encoded as a variable length vector of F, but we may
        // be able to make this a fixed-length vector for specific Poplar1
        // instantations
        encode_u16_items(bytes, &(), &self.0);
    }
}

impl<F: FieldElement> ParameterizedDecode<Poplar1PrepareState<F>> for Poplar1PrepareMessage<F> {
    fn decode_with_param(
        _decoding_parameter: &Poplar1PrepareState<F>,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        // TODO: This is decoded as a variable length vector of F, but we may be
        // able to make this a fixed-length vector for specific Poplar1
        // instantiations.
        let items = decode_u16_items(&(), bytes)?;

        Ok(Self(items))
    }
}

/// The state of each Aggregator during the Prepare process.
#[derive(Clone, Debug)]
pub struct Poplar1PrepareState<F> {
    /// State of the secure sketching protocol.
    sketch: SketchState,

    /// The output share.
    output_share: OutputShare<F>,

    /// Aggregator's share of $A = -2a + k$.
    d: F,

    /// Aggregator's share of $B = a^2 + b -ak + c$.
    e: F,

    /// Equal to 1 if this Aggregator is the "leader" and 0 otherwise.
    x: F,
}

#[derive(Clone, Debug)]
enum SketchState {
    RoundOne,
    RoundTwo,
}

impl<I, P, const L: usize> Collector for Poplar1<I, P, L>
where
    I: Idpf<2, 2>,
    P: Prg<L>,
{
    fn unshard<M: IntoIterator<Item = AggregateShare<I::Field>>>(
        &self,
        agg_param: &BTreeSet<IdpfInput>,
        agg_shares: M,
        _num_measurements: usize,
    ) -> Result<BTreeMap<IdpfInput, u64>, VdafError> {
        let mut agg_data = AggregateShare(vec![I::Field::zero(); agg_param.len()]);
        for agg_share in agg_shares.into_iter() {
            agg_data.merge(&agg_share)?;
        }

        let mut agg = BTreeMap::new();
        for (prefix, count) in agg_param.iter().zip(agg_data.as_ref()) {
            let count = <I::Field as FieldElement>::Integer::from(*count);
            let count: u64 = count
                .try_into()
                .map_err(|_| VdafError::Uncategorized("aggregate overflow".to_string()))?;
            agg.insert(*prefix, count);
        }
        Ok(agg)
    }
}

fn role_try_from(agg_id: usize) -> Result<bool, VdafError> {
    match agg_id {
        0 => Ok(true),
        1 => Ok(false),
        _ => Err(VdafError::Uncategorized("unexpected aggregator id".into())),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::field::Field128;
    use crate::vdaf::prg::PrgAes128;
    use crate::vdaf::{run_vdaf, run_vdaf_prepare};
    use rand::prelude::*;

    #[test]
    fn test_idpf() {
        // IDPF input equality tests.
        assert_eq!(
            IdpfInput::new(b"hello", 40).unwrap(),
            IdpfInput::new(b"hello", 40).unwrap()
        );
        assert_eq!(
            IdpfInput::new(b"hi", 9).unwrap(),
            IdpfInput::new(b"ha", 9).unwrap(),
        );
        assert_eq!(
            IdpfInput::new(b"hello", 25).unwrap(),
            IdpfInput::new(b"help", 25).unwrap()
        );
        assert_ne!(
            IdpfInput::new(b"hello", 40).unwrap(),
            IdpfInput::new(b"hello", 39).unwrap()
        );
        assert_ne!(
            IdpfInput::new(b"hello", 40).unwrap(),
            IdpfInput::new(b"hell-", 40).unwrap()
        );

        // IDPF uniqueness tests
        let mut unique = BTreeSet::new();
        assert!(unique.insert(IdpfInput::new(b"hello", 40).unwrap()));
        assert!(!unique.insert(IdpfInput::new(b"hello", 40).unwrap()));
        assert!(unique.insert(IdpfInput::new(b"hello", 39).unwrap()));
        assert!(unique.insert(IdpfInput::new(b"bye", 20).unwrap()));

        // Generate IDPF keys.
        let input = IdpfInput::new(b"hi", 16).unwrap();
        let keys = ToyIdpf::<Field128>::gen(
            &input,
            std::iter::repeat([Field128::one(), Field128::one()]),
        )
        .unwrap();

        // Try evaluating the IDPF keys on all prefixes.
        for prefix_len in 0..input.level + 1 {
            let res = eval_idpf(
                &keys,
                &input.prefix(prefix_len),
                &[Field128::one(), Field128::one()],
            );
            assert!(res.is_ok(), "prefix_len={} error: {:?}", prefix_len, res);
        }

        // Try evaluating the IDPF keys on incorrect prefixes.
        eval_idpf(
            &keys,
            &IdpfInput::new(&[2], 2).unwrap(),
            &[Field128::zero(), Field128::zero()],
        )
        .unwrap();

        eval_idpf(
            &keys,
            &IdpfInput::new(&[23, 1], 12).unwrap(),
            &[Field128::zero(), Field128::zero()],
        )
        .unwrap();
    }

    fn eval_idpf<I, const KEY_LEN: usize, const OUT_LEN: usize>(
        keys: &[I; KEY_LEN],
        input: &IdpfInput,
        expected_output: &[I::Field; OUT_LEN],
    ) -> Result<(), VdafError>
    where
        I: Idpf<KEY_LEN, OUT_LEN>,
    {
        let mut output = [I::Field::zero(); OUT_LEN];
        for key in keys {
            let output_share = key.eval(input)?;
            for (x, y) in output.iter_mut().zip(output_share) {
                *x += y;
            }
        }

        if expected_output != &output {
            return Err(VdafError::Uncategorized(format!(
                "eval_idpf(): unexpected output: got {:?}; want {:?}",
                output, expected_output
            )));
        }

        Ok(())
    }

    #[test]
    fn test_poplar1() {
        const INPUT_LEN: usize = 8;

        let vdaf: Poplar1<ToyIdpf<Field128>, PrgAes128, 16> = Poplar1::new(INPUT_LEN);
        assert_eq!(vdaf.num_aggregators(), 2);

        // Run the VDAF input-distribution algorithm.
        let input = vec![IdpfInput::new(&[0b0110_1000], INPUT_LEN).unwrap()];

        let mut agg_param = BTreeSet::new();
        agg_param.insert(input[0]);
        check_btree(&run_vdaf(&vdaf, &agg_param, input.clone()).unwrap(), &[1]);

        // Try evaluating the VDAF on each prefix of the input.
        for prefix_len in 0..input[0].level + 1 {
            let mut agg_param = BTreeSet::new();
            agg_param.insert(input[0].prefix(prefix_len));
            check_btree(&run_vdaf(&vdaf, &agg_param, input.clone()).unwrap(), &[1]);
        }

        // Try various prefixes.
        let prefix_len = 4;
        let mut agg_param = BTreeSet::new();
        // At length 4, the next two prefixes are equal. Neither one matches the input.
        agg_param.insert(IdpfInput::new(&[0b0000_0000], prefix_len).unwrap());
        agg_param.insert(IdpfInput::new(&[0b0001_0000], prefix_len).unwrap());
        agg_param.insert(IdpfInput::new(&[0b0000_0001], prefix_len).unwrap());
        agg_param.insert(IdpfInput::new(&[0b0000_1101], prefix_len).unwrap());
        // At length 4, the next two prefixes are equal. Both match the input.
        agg_param.insert(IdpfInput::new(&[0b0111_1101], prefix_len).unwrap());
        agg_param.insert(IdpfInput::new(&[0b0000_1101], prefix_len).unwrap());
        let aggregate = run_vdaf(&vdaf, &agg_param, input.clone()).unwrap();
        assert_eq!(aggregate.len(), agg_param.len());
        check_btree(
            &aggregate,
            // We put six prefixes in the aggregation parameter, but the vector we get back is only
            // 4 elements because at the given prefix length, some of the prefixes are equal.
            &[0, 0, 0, 1],
        );

        let mut verify_key = [0; 16];
        thread_rng().fill(&mut verify_key[..]);
        let nonce = b"this is a nonce";

        // Try evaluating the VDAF with an invalid aggregation parameter. (It's an error to have a
        // mixture of prefix lengths.)
        let mut agg_param = BTreeSet::new();
        agg_param.insert(IdpfInput::new(&[0b0000_0111], 6).unwrap());
        agg_param.insert(IdpfInput::new(&[0b0000_1000], 7).unwrap());
        let input_shares = vdaf.shard(&input[0]).unwrap();
        run_vdaf_prepare(&vdaf, &verify_key, &agg_param, nonce, input_shares).unwrap_err();

        // Try evaluating the VDAF with malformed inputs.
        //
        // This IDPF key pair evaluates to 1 everywhere, which is illegal.
        let mut input_shares = vdaf.shard(&input[0]).unwrap();
        for (i, x) in input_shares[0].idpf.data0.iter_mut().enumerate() {
            if i != input[0].index {
                *x += Field128::one();
            }
        }
        let mut agg_param = BTreeSet::new();
        agg_param.insert(IdpfInput::new(&[0b0000_0111], 8).unwrap());
        run_vdaf_prepare(&vdaf, &verify_key, &agg_param, nonce, input_shares).unwrap_err();

        // This IDPF key pair has a garbled authentication vector.
        let mut input_shares = vdaf.shard(&input[0]).unwrap();
        for x in input_shares[0].idpf.data1.iter_mut() {
            *x = Field128::zero();
        }
        let mut agg_param = BTreeSet::new();
        agg_param.insert(IdpfInput::new(&[0b0000_0111], 8).unwrap());
        run_vdaf_prepare(&vdaf, &verify_key, &agg_param, nonce, input_shares).unwrap_err();
    }

    fn check_btree(btree: &BTreeMap<IdpfInput, u64>, counts: &[u64]) {
        for (got, want) in btree.values().zip(counts.iter()) {
            assert_eq!(got, want, "got {:?} want {:?}", btree.values(), counts);
        }
    }
}
