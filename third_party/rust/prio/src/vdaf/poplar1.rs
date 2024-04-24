// SPDX-License-Identifier: MPL-2.0

//! Implementation of Poplar1 as specified in [[draft-irtf-cfrg-vdaf-07]].
//!
//! [draft-irtf-cfrg-vdaf-07]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/07/

use crate::{
    codec::{CodecError, Decode, Encode, ParameterizedDecode},
    field::{decode_fieldvec, merge_vector, Field255, Field64, FieldElement},
    idpf::{Idpf, IdpfInput, IdpfOutputShare, IdpfPublicShare, IdpfValue, RingBufferCache},
    prng::Prng,
    vdaf::{
        xof::{Seed, Xof, XofShake128},
        Aggregatable, Aggregator, Client, Collector, PrepareTransition, Vdaf, VdafError,
    },
};
use bitvec::{prelude::Lsb0, vec::BitVec};
use rand_core::RngCore;
use std::{
    convert::TryFrom,
    fmt::Debug,
    io::{Cursor, Read},
    iter,
    marker::PhantomData,
    num::TryFromIntError,
    ops::{Add, AddAssign, Sub},
};
use subtle::{Choice, ConditionallyNegatable, ConditionallySelectable, ConstantTimeEq};

const DST_SHARD_RANDOMNESS: u16 = 1;
const DST_CORR_INNER: u16 = 2;
const DST_CORR_LEAF: u16 = 3;
const DST_VERIFY_RANDOMNESS: u16 = 4;

impl<P, const SEED_SIZE: usize> Poplar1<P, SEED_SIZE> {
    /// Create an instance of [`Poplar1`]. The caller provides the bit length of each
    /// measurement (`BITS` as defined in the [[draft-irtf-cfrg-vdaf-07]]).
    ///
    /// [draft-irtf-cfrg-vdaf-07]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/07/
    pub fn new(bits: usize) -> Self {
        Self {
            bits,
            phantom: PhantomData,
        }
    }
}

impl Poplar1<XofShake128, 16> {
    /// Create an instance of [`Poplar1`] using [`XofShake128`]. The caller provides the bit length of
    /// each measurement (`BITS` as defined in the [[draft-irtf-cfrg-vdaf-07]]).
    ///
    /// [draft-irtf-cfrg-vdaf-07]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/07/
    pub fn new_shake128(bits: usize) -> Self {
        Poplar1::new(bits)
    }
}

/// The Poplar1 VDAF.
#[derive(Debug)]
pub struct Poplar1<P, const SEED_SIZE: usize> {
    bits: usize,
    phantom: PhantomData<P>,
}

impl<P: Xof<SEED_SIZE>, const SEED_SIZE: usize> Poplar1<P, SEED_SIZE> {
    /// Construct a `Prng` with the given seed and info-string suffix.
    fn init_prng<I, B, F>(
        seed: &[u8; SEED_SIZE],
        usage: u16,
        binder_chunks: I,
    ) -> Prng<F, P::SeedStream>
    where
        I: IntoIterator<Item = B>,
        B: AsRef<[u8]>,
        P: Xof<SEED_SIZE>,
        F: FieldElement,
    {
        let mut xof = P::init(seed, &Self::domain_separation_tag(usage));
        for binder_chunk in binder_chunks.into_iter() {
            xof.update(binder_chunk.as_ref());
        }
        Prng::from_seed_stream(xof.into_seed_stream())
    }
}

impl<P, const SEED_SIZE: usize> Clone for Poplar1<P, SEED_SIZE> {
    fn clone(&self) -> Self {
        Self {
            bits: self.bits,
            phantom: PhantomData,
        }
    }
}

/// Poplar1 public share.
///
/// This is comprised of the correction words generated for the IDPF.
pub type Poplar1PublicShare =
    IdpfPublicShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>;

impl<P, const SEED_SIZE: usize> ParameterizedDecode<Poplar1<P, SEED_SIZE>> for Poplar1PublicShare {
    fn decode_with_param(
        poplar1: &Poplar1<P, SEED_SIZE>,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        Self::decode_with_param(&poplar1.bits, bytes)
    }
}

/// Poplar1 input share.
///
/// This is comprised of an IDPF key share and the correlated randomness used to compute the sketch
/// during preparation.
#[derive(Debug, Clone)]
pub struct Poplar1InputShare<const SEED_SIZE: usize> {
    /// IDPF key share.
    idpf_key: Seed<16>,

    /// Seed used to generate the Aggregator's share of the correlated randomness used in the first
    /// part of the sketch.
    corr_seed: Seed<SEED_SIZE>,

    /// Aggregator's share of the correlated randomness used in the second part of the sketch. Used
    /// for inner nodes of the IDPF tree.
    corr_inner: Vec<[Field64; 2]>,

    /// Aggregator's share of the correlated randomness used in the second part of the sketch. Used
    /// for leaf nodes of the IDPF tree.
    corr_leaf: [Field255; 2],
}

impl<const SEED_SIZE: usize> PartialEq for Poplar1InputShare<SEED_SIZE> {
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl<const SEED_SIZE: usize> Eq for Poplar1InputShare<SEED_SIZE> {}

impl<const SEED_SIZE: usize> ConstantTimeEq for Poplar1InputShare<SEED_SIZE> {
    fn ct_eq(&self, other: &Self) -> Choice {
        // We short-circuit on the length of corr_inner being different. Only the content is
        // protected.
        if self.corr_inner.len() != other.corr_inner.len() {
            return Choice::from(0);
        }

        let mut res = self.idpf_key.ct_eq(&other.idpf_key)
            & self.corr_seed.ct_eq(&other.corr_seed)
            & self.corr_leaf.ct_eq(&other.corr_leaf);
        for (x, y) in self.corr_inner.iter().zip(other.corr_inner.iter()) {
            res &= x.ct_eq(y);
        }
        res
    }
}

impl<const SEED_SIZE: usize> Encode for Poplar1InputShare<SEED_SIZE> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.idpf_key.encode(bytes);
        self.corr_seed.encode(bytes);
        for corr in self.corr_inner.iter() {
            corr[0].encode(bytes);
            corr[1].encode(bytes);
        }
        self.corr_leaf[0].encode(bytes);
        self.corr_leaf[1].encode(bytes);
    }

    fn encoded_len(&self) -> Option<usize> {
        let mut len = 0;
        len += SEED_SIZE; // idpf_key
        len += SEED_SIZE; // corr_seed
        len += self.corr_inner.len() * 2 * Field64::ENCODED_SIZE; // corr_inner
        len += 2 * Field255::ENCODED_SIZE; // corr_leaf
        Some(len)
    }
}

impl<'a, P, const SEED_SIZE: usize> ParameterizedDecode<(&'a Poplar1<P, SEED_SIZE>, usize)>
    for Poplar1InputShare<SEED_SIZE>
{
    fn decode_with_param(
        (poplar1, _agg_id): &(&'a Poplar1<P, SEED_SIZE>, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        let idpf_key = Seed::decode(bytes)?;
        let corr_seed = Seed::decode(bytes)?;
        let mut corr_inner = Vec::with_capacity(poplar1.bits - 1);
        for _ in 0..poplar1.bits - 1 {
            corr_inner.push([Field64::decode(bytes)?, Field64::decode(bytes)?]);
        }
        let corr_leaf = [Field255::decode(bytes)?, Field255::decode(bytes)?];
        Ok(Self {
            idpf_key,
            corr_seed,
            corr_inner,
            corr_leaf,
        })
    }
}

/// Poplar1 preparation state.
#[derive(Clone, Debug)]
pub struct Poplar1PrepareState(PrepareStateVariant);

impl PartialEq for Poplar1PrepareState {
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl Eq for Poplar1PrepareState {}

impl ConstantTimeEq for Poplar1PrepareState {
    fn ct_eq(&self, other: &Self) -> Choice {
        self.0.ct_eq(&other.0)
    }
}

impl Encode for Poplar1PrepareState {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.encode(bytes)
    }

    fn encoded_len(&self) -> Option<usize> {
        self.0.encoded_len()
    }
}

impl<'a, P, const SEED_SIZE: usize> ParameterizedDecode<(&'a Poplar1<P, SEED_SIZE>, usize)>
    for Poplar1PrepareState
{
    fn decode_with_param(
        decoding_parameter: &(&'a Poplar1<P, SEED_SIZE>, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        Ok(Self(PrepareStateVariant::decode_with_param(
            decoding_parameter,
            bytes,
        )?))
    }
}

#[derive(Clone, Debug)]
enum PrepareStateVariant {
    Inner(PrepareState<Field64>),
    Leaf(PrepareState<Field255>),
}

impl PartialEq for PrepareStateVariant {
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl Eq for PrepareStateVariant {}

impl ConstantTimeEq for PrepareStateVariant {
    fn ct_eq(&self, other: &Self) -> Choice {
        // We allow short-circuiting on the type (Inner vs Leaf).
        match (self, other) {
            (Self::Inner(self_val), Self::Inner(other_val)) => self_val.ct_eq(other_val),
            (Self::Leaf(self_val), Self::Leaf(other_val)) => self_val.ct_eq(other_val),
            _ => Choice::from(0),
        }
    }
}

impl Encode for PrepareStateVariant {
    fn encode(&self, bytes: &mut Vec<u8>) {
        match self {
            PrepareStateVariant::Inner(prep_state) => {
                0u8.encode(bytes);
                prep_state.encode(bytes);
            }
            PrepareStateVariant::Leaf(prep_state) => {
                1u8.encode(bytes);
                prep_state.encode(bytes);
            }
        }
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(
            1 + match self {
                PrepareStateVariant::Inner(prep_state) => prep_state.encoded_len()?,
                PrepareStateVariant::Leaf(prep_state) => prep_state.encoded_len()?,
            },
        )
    }
}

impl<'a, P, const SEED_SIZE: usize> ParameterizedDecode<(&'a Poplar1<P, SEED_SIZE>, usize)>
    for PrepareStateVariant
{
    fn decode_with_param(
        decoding_parameter: &(&'a Poplar1<P, SEED_SIZE>, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        match u8::decode(bytes)? {
            0 => {
                let prep_state = PrepareState::decode_with_param(decoding_parameter, bytes)?;
                Ok(Self::Inner(prep_state))
            }
            1 => {
                let prep_state = PrepareState::decode_with_param(decoding_parameter, bytes)?;
                Ok(Self::Leaf(prep_state))
            }
            _ => Err(CodecError::UnexpectedValue),
        }
    }
}

#[derive(Clone)]
struct PrepareState<F> {
    sketch: SketchState<F>,
    output_share: Vec<F>,
}

impl<F: ConstantTimeEq> PartialEq for PrepareState<F> {
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl<F: ConstantTimeEq> Eq for PrepareState<F> {}

impl<F: ConstantTimeEq> ConstantTimeEq for PrepareState<F> {
    fn ct_eq(&self, other: &Self) -> Choice {
        self.sketch.ct_eq(&other.sketch) & self.output_share.ct_eq(&other.output_share)
    }
}

impl<F> Debug for PrepareState<F> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PrepareState")
            .field("sketch", &"[redacted]")
            .field("output_share", &"[redacted]")
            .finish()
    }
}

impl<F: FieldElement> Encode for PrepareState<F> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.sketch.encode(bytes);
        // `expect` safety: output_share's length is the same as the number of prefixes; the number
        // of prefixes is capped at 2^32-1.
        u32::try_from(self.output_share.len())
            .expect("Couldn't convert output_share length to u32")
            .encode(bytes);
        for elem in &self.output_share {
            elem.encode(bytes);
        }
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(self.sketch.encoded_len()? + 4 + self.output_share.len() * F::ENCODED_SIZE)
    }
}

impl<'a, P, F: FieldElement, const SEED_SIZE: usize>
    ParameterizedDecode<(&'a Poplar1<P, SEED_SIZE>, usize)> for PrepareState<F>
{
    fn decode_with_param(
        decoding_parameter: &(&'a Poplar1<P, SEED_SIZE>, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        let sketch = SketchState::<F>::decode_with_param(decoding_parameter, bytes)?;
        let output_share_len = u32::decode(bytes)?
            .try_into()
            .map_err(|err: TryFromIntError| CodecError::Other(err.into()))?;
        let output_share = iter::repeat_with(|| F::decode(bytes))
            .take(output_share_len)
            .collect::<Result<_, _>>()?;
        Ok(Self {
            sketch,
            output_share,
        })
    }
}

#[derive(Clone, Debug)]
enum SketchState<F> {
    #[allow(non_snake_case)]
    RoundOne {
        A_share: F,
        B_share: F,
        is_leader: bool,
    },
    RoundTwo,
}

impl<F: ConstantTimeEq> PartialEq for SketchState<F> {
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl<F: ConstantTimeEq> Eq for SketchState<F> {}

impl<F: ConstantTimeEq> ConstantTimeEq for SketchState<F> {
    fn ct_eq(&self, other: &Self) -> Choice {
        // We allow short-circuiting on the round (RoundOne vs RoundTwo), as well as is_leader for
        // RoundOne comparisons.
        match (self, other) {
            (
                SketchState::RoundOne {
                    A_share: self_a_share,
                    B_share: self_b_share,
                    is_leader: self_is_leader,
                },
                SketchState::RoundOne {
                    A_share: other_a_share,
                    B_share: other_b_share,
                    is_leader: other_is_leader,
                },
            ) => {
                if self_is_leader != other_is_leader {
                    return Choice::from(0);
                }

                self_a_share.ct_eq(other_a_share) & self_b_share.ct_eq(other_b_share)
            }

            (SketchState::RoundTwo, SketchState::RoundTwo) => Choice::from(1),
            _ => Choice::from(0),
        }
    }
}

impl<F: FieldElement> Encode for SketchState<F> {
    fn encode(&self, bytes: &mut Vec<u8>) {
        match self {
            SketchState::RoundOne {
                A_share, B_share, ..
            } => {
                0u8.encode(bytes);
                A_share.encode(bytes);
                B_share.encode(bytes);
            }
            SketchState::RoundTwo => 1u8.encode(bytes),
        }
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(
            1 + match self {
                SketchState::RoundOne { .. } => 2 * F::ENCODED_SIZE,
                SketchState::RoundTwo => 0,
            },
        )
    }
}

impl<'a, P, F: FieldElement, const SEED_SIZE: usize>
    ParameterizedDecode<(&'a Poplar1<P, SEED_SIZE>, usize)> for SketchState<F>
{
    #[allow(non_snake_case)]
    fn decode_with_param(
        (_, agg_id): &(&'a Poplar1<P, SEED_SIZE>, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        match u8::decode(bytes)? {
            0 => {
                let A_share = F::decode(bytes)?;
                let B_share = F::decode(bytes)?;
                let is_leader = agg_id == &0;
                Ok(Self::RoundOne {
                    A_share,
                    B_share,
                    is_leader,
                })
            }
            1 => Ok(Self::RoundTwo),
            _ => Err(CodecError::UnexpectedValue),
        }
    }
}

impl<F: FieldElement> SketchState<F> {
    fn decode_sketch_share(&self, bytes: &mut Cursor<&[u8]>) -> Result<Vec<F>, CodecError> {
        match self {
            // The sketch share is three field elements.
            Self::RoundOne { .. } => Ok(vec![
                F::decode(bytes)?,
                F::decode(bytes)?,
                F::decode(bytes)?,
            ]),
            // The sketch verifier share is one field element.
            Self::RoundTwo => Ok(vec![F::decode(bytes)?]),
        }
    }

    fn decode_sketch(&self, bytes: &mut Cursor<&[u8]>) -> Result<Option<[F; 3]>, CodecError> {
        match self {
            // The sketch is three field elements.
            Self::RoundOne { .. } => Ok(Some([
                F::decode(bytes)?,
                F::decode(bytes)?,
                F::decode(bytes)?,
            ])),
            // The sketch verifier should be zero if the sketch if valid. Instead of transmitting
            // this zero over the wire, we just expect an empty message.
            Self::RoundTwo => Ok(None),
        }
    }
}

/// Poplar1 preparation message.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Poplar1PrepareMessage(PrepareMessageVariant);

#[derive(Clone, Debug, PartialEq, Eq)]
enum PrepareMessageVariant {
    SketchInner([Field64; 3]),
    SketchLeaf([Field255; 3]),
    Done,
}

impl Encode for Poplar1PrepareMessage {
    fn encode(&self, bytes: &mut Vec<u8>) {
        match self.0 {
            PrepareMessageVariant::SketchInner(vec) => {
                vec[0].encode(bytes);
                vec[1].encode(bytes);
                vec[2].encode(bytes);
            }
            PrepareMessageVariant::SketchLeaf(vec) => {
                vec[0].encode(bytes);
                vec[1].encode(bytes);
                vec[2].encode(bytes);
            }
            PrepareMessageVariant::Done => (),
        }
    }

    fn encoded_len(&self) -> Option<usize> {
        match self.0 {
            PrepareMessageVariant::SketchInner(..) => Some(3 * Field64::ENCODED_SIZE),
            PrepareMessageVariant::SketchLeaf(..) => Some(3 * Field255::ENCODED_SIZE),
            PrepareMessageVariant::Done => Some(0),
        }
    }
}

impl ParameterizedDecode<Poplar1PrepareState> for Poplar1PrepareMessage {
    fn decode_with_param(
        state: &Poplar1PrepareState,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        match state.0 {
            PrepareStateVariant::Inner(ref state_variant) => Ok(Self(
                state_variant
                    .sketch
                    .decode_sketch(bytes)?
                    .map_or(PrepareMessageVariant::Done, |sketch| {
                        PrepareMessageVariant::SketchInner(sketch)
                    }),
            )),
            PrepareStateVariant::Leaf(ref state_variant) => Ok(Self(
                state_variant
                    .sketch
                    .decode_sketch(bytes)?
                    .map_or(PrepareMessageVariant::Done, |sketch| {
                        PrepareMessageVariant::SketchLeaf(sketch)
                    }),
            )),
        }
    }
}

/// A vector of field elements transmitted while evaluating Poplar1.
#[derive(Clone, Debug)]
pub enum Poplar1FieldVec {
    /// Field type for inner nodes of the IDPF tree.
    Inner(Vec<Field64>),

    /// Field type for leaf nodes of the IDPF tree.
    Leaf(Vec<Field255>),
}

impl Poplar1FieldVec {
    fn zero(is_leaf: bool, len: usize) -> Self {
        if is_leaf {
            Self::Leaf(vec![<Field255 as FieldElement>::zero(); len])
        } else {
            Self::Inner(vec![<Field64 as FieldElement>::zero(); len])
        }
    }
}

impl PartialEq for Poplar1FieldVec {
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl Eq for Poplar1FieldVec {}

impl ConstantTimeEq for Poplar1FieldVec {
    fn ct_eq(&self, other: &Self) -> Choice {
        // We allow short-circuiting on the type (Inner vs Leaf).
        match (self, other) {
            (Poplar1FieldVec::Inner(self_val), Poplar1FieldVec::Inner(other_val)) => {
                self_val.ct_eq(other_val)
            }
            (Poplar1FieldVec::Leaf(self_val), Poplar1FieldVec::Leaf(other_val)) => {
                self_val.ct_eq(other_val)
            }
            _ => Choice::from(0),
        }
    }
}

impl Encode for Poplar1FieldVec {
    fn encode(&self, bytes: &mut Vec<u8>) {
        match self {
            Self::Inner(ref data) => {
                for elem in data {
                    elem.encode(bytes);
                }
            }
            Self::Leaf(ref data) => {
                for elem in data {
                    elem.encode(bytes);
                }
            }
        }
    }

    fn encoded_len(&self) -> Option<usize> {
        match self {
            Self::Inner(ref data) => Some(Field64::ENCODED_SIZE * data.len()),
            Self::Leaf(ref data) => Some(Field255::ENCODED_SIZE * data.len()),
        }
    }
}

impl<'a, P: Xof<SEED_SIZE>, const SEED_SIZE: usize>
    ParameterizedDecode<(&'a Poplar1<P, SEED_SIZE>, &'a Poplar1AggregationParam)>
    for Poplar1FieldVec
{
    fn decode_with_param(
        (poplar1, agg_param): &(&'a Poplar1<P, SEED_SIZE>, &'a Poplar1AggregationParam),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        if agg_param.level() == poplar1.bits - 1 {
            decode_fieldvec(agg_param.prefixes().len(), bytes).map(Poplar1FieldVec::Leaf)
        } else {
            decode_fieldvec(agg_param.prefixes().len(), bytes).map(Poplar1FieldVec::Inner)
        }
    }
}

impl ParameterizedDecode<Poplar1PrepareState> for Poplar1FieldVec {
    fn decode_with_param(
        state: &Poplar1PrepareState,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        match state.0 {
            PrepareStateVariant::Inner(ref state_variant) => Ok(Poplar1FieldVec::Inner(
                state_variant.sketch.decode_sketch_share(bytes)?,
            )),
            PrepareStateVariant::Leaf(ref state_variant) => Ok(Poplar1FieldVec::Leaf(
                state_variant.sketch.decode_sketch_share(bytes)?,
            )),
        }
    }
}

impl Aggregatable for Poplar1FieldVec {
    type OutputShare = Self;

    fn merge(&mut self, agg_share: &Self) -> Result<(), VdafError> {
        match (self, agg_share) {
            (Self::Inner(ref mut left), Self::Inner(right)) => Ok(merge_vector(left, right)?),
            (Self::Leaf(ref mut left), Self::Leaf(right)) => Ok(merge_vector(left, right)?),
            _ => Err(VdafError::Uncategorized(
                "cannot merge leaf nodes wiith inner nodes".into(),
            )),
        }
    }

    fn accumulate(&mut self, output_share: &Self) -> Result<(), VdafError> {
        match (self, output_share) {
            (Self::Inner(ref mut left), Self::Inner(right)) => Ok(merge_vector(left, right)?),
            (Self::Leaf(ref mut left), Self::Leaf(right)) => Ok(merge_vector(left, right)?),
            _ => Err(VdafError::Uncategorized(
                "cannot accumulate leaf nodes with inner nodes".into(),
            )),
        }
    }
}

/// Poplar1 aggregation parameter.
///
/// This includes an indication of what level of the IDPF tree is being evaluated and the set of
/// prefixes to evaluate at that level.
#[derive(Clone, Debug, Hash, PartialEq, Eq)]
pub struct Poplar1AggregationParam {
    level: u16,
    prefixes: Vec<IdpfInput>,
}

impl Poplar1AggregationParam {
    /// Construct an aggregation parameter from a set of candidate prefixes.
    ///
    /// # Errors
    ///
    /// * The list of prefixes is empty.
    /// * The prefixes have different lengths (they must all be the same).
    /// * The prefixes have length 0, or length longer than 2^16 bits.
    /// * There are more than 2^32 - 1 prefixes.
    /// * The prefixes are not unique.
    /// * The prefixes are not in lexicographic order.
    pub fn try_from_prefixes(prefixes: Vec<IdpfInput>) -> Result<Self, VdafError> {
        if prefixes.is_empty() {
            return Err(VdafError::Uncategorized(
                "at least one prefix is required".into(),
            ));
        }
        if u32::try_from(prefixes.len()).is_err() {
            return Err(VdafError::Uncategorized("too many prefixes".into()));
        }

        let len = prefixes[0].len();
        let mut last_prefix = None;
        for prefix in prefixes.iter() {
            if prefix.len() != len {
                return Err(VdafError::Uncategorized(
                    "all prefixes must have the same length".into(),
                ));
            }
            if let Some(last_prefix) = last_prefix {
                if prefix <= last_prefix {
                    if prefix == last_prefix {
                        return Err(VdafError::Uncategorized(
                            "prefixes must be nonrepeating".into(),
                        ));
                    } else {
                        return Err(VdafError::Uncategorized(
                            "prefixes must be in lexicographic order".into(),
                        ));
                    }
                }
            }
            last_prefix = Some(prefix);
        }

        let level = len
            .checked_sub(1)
            .ok_or_else(|| VdafError::Uncategorized("prefixes are too short".into()))?;
        let level = u16::try_from(level)
            .map_err(|_| VdafError::Uncategorized("prefixes are too long".into()))?;

        Ok(Self { level, prefixes })
    }

    /// Return the level of the IDPF tree.
    pub fn level(&self) -> usize {
        usize::from(self.level)
    }

    /// Return the prefixes.
    pub fn prefixes(&self) -> &[IdpfInput] {
        self.prefixes.as_ref()
    }
}

impl Encode for Poplar1AggregationParam {
    fn encode(&self, bytes: &mut Vec<u8>) {
        // Okay to unwrap because `try_from_prefixes()` checks this conversion succeeds.
        let prefix_count = u32::try_from(self.prefixes.len()).unwrap();
        self.level.encode(bytes);
        prefix_count.encode(bytes);

        // The encoding of the prefixes is defined by treating the IDPF indices as integers,
        // shifting and ORing them together, and encoding the resulting arbitrary precision integer
        // in big endian byte order. Thus, the first prefix will appear in the last encoded byte,
        // aligned to its least significant bit. The last prefix will appear in the first encoded
        // byte, not necessarily aligned to a byte boundary. If the highest bits in the first byte
        // are unused, they will be set to zero.

        // When an IDPF index is treated as an integer, the first bit is the integer's most
        // significant bit, and bits are subsequently processed in order of decreasing significance.
        // Thus, setting aside the order of bytes, bits within each byte are ordered with the
        // [`Msb0`](bitvec::prelude::Msb0) convention, not [`Lsb0`](bitvec::prelude::Msb0). Yet,
        // the entire integer is aligned to the least significant bit of the last byte, so we
        // could not use `Msb0` directly without padding adjustments. Instead, we use `Lsb0`
        // throughout and reverse the bit order of each prefix.

        let mut packed = self
            .prefixes
            .iter()
            .flat_map(|input| input.iter().rev())
            .collect::<BitVec<u8, Lsb0>>();
        packed.set_uninitialized(false);
        let mut packed = packed.into_vec();
        packed.reverse();
        bytes.append(&mut packed);
    }

    fn encoded_len(&self) -> Option<usize> {
        let packed_bit_count = (usize::from(self.level) + 1) * self.prefixes.len();
        // 4 bytes for the number of prefixes, 2 bytes for the level, and a variable number of bytes
        // for the packed prefixes themselves.
        Some(6 + (packed_bit_count + 7) / 8)
    }
}

impl Decode for Poplar1AggregationParam {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let level = u16::decode(bytes)?;
        let prefix_count =
            usize::try_from(u32::decode(bytes)?).map_err(|e| CodecError::Other(e.into()))?;

        let packed_bit_count = (usize::from(level) + 1) * prefix_count;
        let mut packed = vec![0u8; (packed_bit_count + 7) / 8];
        bytes.read_exact(&mut packed)?;
        if packed_bit_count % 8 != 0 {
            let unused_bits = packed[0] >> (packed_bit_count % 8);
            if unused_bits != 0 {
                return Err(CodecError::UnexpectedValue);
            }
        }
        packed.reverse();
        let bits = BitVec::<u8, Lsb0>::from_vec(packed);

        let prefixes = bits
            .chunks_exact(usize::from(level) + 1)
            .take(prefix_count)
            .map(|chunk| IdpfInput::from(chunk.iter().rev().collect::<BitVec>()))
            .collect::<Vec<IdpfInput>>();

        Poplar1AggregationParam::try_from_prefixes(prefixes)
            .map_err(|e| CodecError::Other(e.into()))
    }
}

impl<P: Xof<SEED_SIZE>, const SEED_SIZE: usize> Vdaf for Poplar1<P, SEED_SIZE> {
    const ID: u32 = 0x00001000;
    type Measurement = IdpfInput;
    type AggregateResult = Vec<u64>;
    type AggregationParam = Poplar1AggregationParam;
    type PublicShare = Poplar1PublicShare;
    type InputShare = Poplar1InputShare<SEED_SIZE>;
    type OutputShare = Poplar1FieldVec;
    type AggregateShare = Poplar1FieldVec;

    fn num_aggregators(&self) -> usize {
        2
    }
}

impl<P: Xof<SEED_SIZE>, const SEED_SIZE: usize> Poplar1<P, SEED_SIZE> {
    fn shard_with_random(
        &self,
        input: &IdpfInput,
        nonce: &[u8; 16],
        idpf_random: &[[u8; 16]; 2],
        poplar_random: &[[u8; SEED_SIZE]; 3],
    ) -> Result<(Poplar1PublicShare, Vec<Poplar1InputShare<SEED_SIZE>>), VdafError> {
        if input.len() != self.bits {
            return Err(VdafError::Uncategorized(format!(
                "unexpected input length ({})",
                input.len()
            )));
        }

        // Generate the authenticator for each inner level of the IDPF tree.
        let mut prng =
            Self::init_prng::<_, _, Field64>(&poplar_random[2], DST_SHARD_RANDOMNESS, [&[]]);
        let auth_inner: Vec<Field64> = (0..self.bits - 1).map(|_| prng.get()).collect();

        // Generate the authenticator for the last level of the IDPF tree (i.e., the leaves).
        //
        // TODO(cjpatton) spec: Consider using a different XOF for the leaf and inner nodes.
        // "Switching" the XOF between field types is awkward.
        let mut prng = prng.into_new_field::<Field255>();
        let auth_leaf = prng.get();

        // Generate the IDPF shares.
        let idpf = Idpf::new((), ());
        let (public_share, [idpf_key_0, idpf_key_1]) = idpf.gen_with_random(
            input,
            auth_inner
                .iter()
                .map(|auth| Poplar1IdpfValue([Field64::one(), *auth])),
            Poplar1IdpfValue([Field255::one(), auth_leaf]),
            nonce,
            idpf_random,
        )?;

        // Generate the correlated randomness for the inner nodes. This includes additive shares of
        // the random offsets `a, b, c` and additive shares of `A := -2*a + auth` and `B := a^2 + b
        // - a*auth + c`, where `auth` is the authenticator for the level of the tree. These values
        // are used, respectively, to compute and verify the sketch during the preparation phase.
        // (See Section 4.2 of [BBCG+21].)
        let corr_seed_0 = &poplar_random[0];
        let corr_seed_1 = &poplar_random[1];
        let mut prng = prng.into_new_field::<Field64>();
        let mut corr_prng_0 = Self::init_prng::<_, _, Field64>(
            corr_seed_0,
            DST_CORR_INNER,
            [[0].as_slice(), nonce.as_slice()],
        );
        let mut corr_prng_1 = Self::init_prng::<_, _, Field64>(
            corr_seed_1,
            DST_CORR_INNER,
            [[1].as_slice(), nonce.as_slice()],
        );
        let mut corr_inner_0 = Vec::with_capacity(self.bits - 1);
        let mut corr_inner_1 = Vec::with_capacity(self.bits - 1);
        for auth in auth_inner.into_iter() {
            let (next_corr_inner_0, next_corr_inner_1) =
                compute_next_corr_shares(&mut prng, &mut corr_prng_0, &mut corr_prng_1, auth);
            corr_inner_0.push(next_corr_inner_0);
            corr_inner_1.push(next_corr_inner_1);
        }

        // Generate the correlated randomness for the leaf nodes.
        let mut prng = prng.into_new_field::<Field255>();
        let mut corr_prng_0 = Self::init_prng::<_, _, Field255>(
            corr_seed_0,
            DST_CORR_LEAF,
            [[0].as_slice(), nonce.as_slice()],
        );
        let mut corr_prng_1 = Self::init_prng::<_, _, Field255>(
            corr_seed_1,
            DST_CORR_LEAF,
            [[1].as_slice(), nonce.as_slice()],
        );
        let (corr_leaf_0, corr_leaf_1) =
            compute_next_corr_shares(&mut prng, &mut corr_prng_0, &mut corr_prng_1, auth_leaf);

        Ok((
            public_share,
            vec![
                Poplar1InputShare {
                    idpf_key: idpf_key_0,
                    corr_seed: Seed::from_bytes(*corr_seed_0),
                    corr_inner: corr_inner_0,
                    corr_leaf: corr_leaf_0,
                },
                Poplar1InputShare {
                    idpf_key: idpf_key_1,
                    corr_seed: Seed::from_bytes(*corr_seed_1),
                    corr_inner: corr_inner_1,
                    corr_leaf: corr_leaf_1,
                },
            ],
        ))
    }
}

impl<P: Xof<SEED_SIZE>, const SEED_SIZE: usize> Client<16> for Poplar1<P, SEED_SIZE> {
    fn shard(
        &self,
        input: &IdpfInput,
        nonce: &[u8; 16],
    ) -> Result<(Self::PublicShare, Vec<Poplar1InputShare<SEED_SIZE>>), VdafError> {
        let mut idpf_random = [[0u8; 16]; 2];
        let mut poplar_random = [[0u8; SEED_SIZE]; 3];
        for random_seed in idpf_random.iter_mut() {
            getrandom::getrandom(random_seed)?;
        }
        for random_seed in poplar_random.iter_mut() {
            getrandom::getrandom(random_seed)?;
        }
        self.shard_with_random(input, nonce, &idpf_random, &poplar_random)
    }
}

impl<P: Xof<SEED_SIZE>, const SEED_SIZE: usize> Aggregator<SEED_SIZE, 16>
    for Poplar1<P, SEED_SIZE>
{
    type PrepareState = Poplar1PrepareState;
    type PrepareShare = Poplar1FieldVec;
    type PrepareMessage = Poplar1PrepareMessage;

    #[allow(clippy::type_complexity)]
    fn prepare_init(
        &self,
        verify_key: &[u8; SEED_SIZE],
        agg_id: usize,
        agg_param: &Poplar1AggregationParam,
        nonce: &[u8; 16],
        public_share: &Poplar1PublicShare,
        input_share: &Poplar1InputShare<SEED_SIZE>,
    ) -> Result<(Poplar1PrepareState, Poplar1FieldVec), VdafError> {
        let is_leader = match agg_id {
            0 => true,
            1 => false,
            _ => {
                return Err(VdafError::Uncategorized(format!(
                    "invalid aggregator ID ({agg_id})"
                )))
            }
        };

        if usize::from(agg_param.level) < self.bits - 1 {
            let mut corr_prng = Self::init_prng::<_, _, Field64>(
                input_share.corr_seed.as_ref(),
                DST_CORR_INNER,
                [[agg_id as u8].as_slice(), nonce.as_slice()],
            );
            // Fast-forward the correlated randomness XOF to the level of the tree that we are
            // aggregating.
            for _ in 0..3 * agg_param.level {
                corr_prng.get();
            }

            let (output_share, sketch_share) = eval_and_sketch::<P, Field64, SEED_SIZE>(
                verify_key,
                agg_id,
                nonce,
                agg_param,
                public_share,
                &input_share.idpf_key,
                &mut corr_prng,
            )?;

            Ok((
                Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                    sketch: SketchState::RoundOne {
                        A_share: input_share.corr_inner[usize::from(agg_param.level)][0],
                        B_share: input_share.corr_inner[usize::from(agg_param.level)][1],
                        is_leader,
                    },
                    output_share,
                })),
                Poplar1FieldVec::Inner(sketch_share),
            ))
        } else {
            let corr_prng = Self::init_prng::<_, _, Field255>(
                input_share.corr_seed.as_ref(),
                DST_CORR_LEAF,
                [[agg_id as u8].as_slice(), nonce.as_slice()],
            );

            let (output_share, sketch_share) = eval_and_sketch::<P, Field255, SEED_SIZE>(
                verify_key,
                agg_id,
                nonce,
                agg_param,
                public_share,
                &input_share.idpf_key,
                &mut corr_prng.into_new_field(),
            )?;

            Ok((
                Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                    sketch: SketchState::RoundOne {
                        A_share: input_share.corr_leaf[0],
                        B_share: input_share.corr_leaf[1],
                        is_leader,
                    },
                    output_share,
                })),
                Poplar1FieldVec::Leaf(sketch_share),
            ))
        }
    }

    fn prepare_shares_to_prepare_message<M: IntoIterator<Item = Poplar1FieldVec>>(
        &self,
        _: &Poplar1AggregationParam,
        inputs: M,
    ) -> Result<Poplar1PrepareMessage, VdafError> {
        let mut inputs = inputs.into_iter();
        let prep_share_0 = inputs
            .next()
            .ok_or_else(|| VdafError::Uncategorized("insufficient number of prep shares".into()))?;
        let prep_share_1 = inputs
            .next()
            .ok_or_else(|| VdafError::Uncategorized("insufficient number of prep shares".into()))?;
        if inputs.next().is_some() {
            return Err(VdafError::Uncategorized(
                "more prep shares than expected".into(),
            ));
        }

        match (prep_share_0, prep_share_1) {
            (Poplar1FieldVec::Inner(share_0), Poplar1FieldVec::Inner(share_1)) => {
                Ok(Poplar1PrepareMessage(
                    next_message(share_0, share_1)?.map_or(PrepareMessageVariant::Done, |sketch| {
                        PrepareMessageVariant::SketchInner(sketch)
                    }),
                ))
            }
            (Poplar1FieldVec::Leaf(share_0), Poplar1FieldVec::Leaf(share_1)) => {
                Ok(Poplar1PrepareMessage(
                    next_message(share_0, share_1)?.map_or(PrepareMessageVariant::Done, |sketch| {
                        PrepareMessageVariant::SketchLeaf(sketch)
                    }),
                ))
            }
            _ => Err(VdafError::Uncategorized(
                "received prep shares with mismatched field types".into(),
            )),
        }
    }

    fn prepare_next(
        &self,
        state: Poplar1PrepareState,
        msg: Poplar1PrepareMessage,
    ) -> Result<PrepareTransition<Self, SEED_SIZE, 16>, VdafError> {
        match (state.0, msg.0) {
            // Round one
            (
                PrepareStateVariant::Inner(PrepareState {
                    sketch:
                        SketchState::RoundOne {
                            A_share,
                            B_share,
                            is_leader,
                        },
                    output_share,
                }),
                PrepareMessageVariant::SketchInner(sketch),
            ) => Ok(PrepareTransition::Continue(
                Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share,
                })),
                Poplar1FieldVec::Inner(finish_sketch(sketch, A_share, B_share, is_leader)),
            )),
            (
                PrepareStateVariant::Leaf(PrepareState {
                    sketch:
                        SketchState::RoundOne {
                            A_share,
                            B_share,
                            is_leader,
                        },
                    output_share,
                }),
                PrepareMessageVariant::SketchLeaf(sketch),
            ) => Ok(PrepareTransition::Continue(
                Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share,
                })),
                Poplar1FieldVec::Leaf(finish_sketch(sketch, A_share, B_share, is_leader)),
            )),

            // Round two
            (
                PrepareStateVariant::Inner(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share,
                }),
                PrepareMessageVariant::Done,
            ) => Ok(PrepareTransition::Finish(Poplar1FieldVec::Inner(
                output_share,
            ))),
            (
                PrepareStateVariant::Leaf(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share,
                }),
                PrepareMessageVariant::Done,
            ) => Ok(PrepareTransition::Finish(Poplar1FieldVec::Leaf(
                output_share,
            ))),

            _ => Err(VdafError::Uncategorized(
                "prep message field type does not match state".into(),
            )),
        }
    }

    fn aggregate<M: IntoIterator<Item = Poplar1FieldVec>>(
        &self,
        agg_param: &Poplar1AggregationParam,
        output_shares: M,
    ) -> Result<Poplar1FieldVec, VdafError> {
        aggregate(
            usize::from(agg_param.level) == self.bits - 1,
            agg_param.prefixes.len(),
            output_shares,
        )
    }
}

impl<P: Xof<SEED_SIZE>, const SEED_SIZE: usize> Collector for Poplar1<P, SEED_SIZE> {
    fn unshard<M: IntoIterator<Item = Poplar1FieldVec>>(
        &self,
        agg_param: &Poplar1AggregationParam,
        agg_shares: M,
        _num_measurements: usize,
    ) -> Result<Vec<u64>, VdafError> {
        let result = aggregate(
            usize::from(agg_param.level) == self.bits - 1,
            agg_param.prefixes.len(),
            agg_shares,
        )?;

        match result {
            Poplar1FieldVec::Inner(vec) => Ok(vec.into_iter().map(u64::from).collect()),
            Poplar1FieldVec::Leaf(vec) => Ok(vec
                .into_iter()
                .map(u64::try_from)
                .collect::<Result<Vec<_>, _>>()?),
        }
    }
}

impl From<IdpfOutputShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>>
    for Poplar1IdpfValue<Field64>
{
    fn from(
        out_share: IdpfOutputShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>,
    ) -> Poplar1IdpfValue<Field64> {
        match out_share {
            IdpfOutputShare::Inner(array) => array,
            IdpfOutputShare::Leaf(..) => panic!("tried to convert leaf share into inner field"),
        }
    }
}

impl From<IdpfOutputShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>>
    for Poplar1IdpfValue<Field255>
{
    fn from(
        out_share: IdpfOutputShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>,
    ) -> Poplar1IdpfValue<Field255> {
        match out_share {
            IdpfOutputShare::Inner(..) => panic!("tried to convert inner share into leaf field"),
            IdpfOutputShare::Leaf(array) => array,
        }
    }
}

/// Derive shares of the correlated randomness for the next level of the IDPF tree.
//
// TODO(cjpatton) spec: Consider deriving the shares of a, b, c for each level directly from the
// seed, rather than iteratively, as we do in Doplar. This would be more efficient for the
// Aggregators. As long as the Client isn't significantly slower, this should be a win.
#[allow(non_snake_case)]
fn compute_next_corr_shares<F: FieldElement + From<u64>, S: RngCore>(
    prng: &mut Prng<F, S>,
    corr_prng_0: &mut Prng<F, S>,
    corr_prng_1: &mut Prng<F, S>,
    auth: F,
) -> ([F; 2], [F; 2]) {
    let two = F::from(2);
    let a = corr_prng_0.get() + corr_prng_1.get();
    let b = corr_prng_0.get() + corr_prng_1.get();
    let c = corr_prng_0.get() + corr_prng_1.get();
    let A = -two * a + auth;
    let B = a * a + b - a * auth + c;
    let corr_1 = [prng.get(), prng.get()];
    let corr_0 = [A - corr_1[0], B - corr_1[1]];
    (corr_0, corr_1)
}

/// Evaluate the IDPF at the given prefixes and compute the Aggregator's share of the sketch.
fn eval_and_sketch<P, F, const SEED_SIZE: usize>(
    verify_key: &[u8; SEED_SIZE],
    agg_id: usize,
    nonce: &[u8; 16],
    agg_param: &Poplar1AggregationParam,
    public_share: &Poplar1PublicShare,
    idpf_key: &Seed<16>,
    corr_prng: &mut Prng<F, P::SeedStream>,
) -> Result<(Vec<F>, Vec<F>), VdafError>
where
    P: Xof<SEED_SIZE>,
    F: FieldElement,
    Poplar1IdpfValue<F>:
        From<IdpfOutputShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>>,
{
    // TODO(cjpatton) spec: Consider not encoding the prefixes here.
    let mut verify_prng = Poplar1::<P, SEED_SIZE>::init_prng(
        verify_key,
        DST_VERIFY_RANDOMNESS,
        [nonce.as_slice(), agg_param.level.to_be_bytes().as_slice()],
    );

    let mut out_share = Vec::with_capacity(agg_param.prefixes.len());
    let mut sketch_share = vec![
        corr_prng.get(), // a_share
        corr_prng.get(), // b_share
        corr_prng.get(), // c_share
    ];

    let mut idpf_eval_cache = RingBufferCache::new(agg_param.prefixes.len());
    let idpf = Idpf::<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>::new((), ());
    for prefix in agg_param.prefixes.iter() {
        let share = Poplar1IdpfValue::<F>::from(idpf.eval(
            agg_id,
            public_share,
            idpf_key,
            prefix,
            nonce,
            &mut idpf_eval_cache,
        )?);

        let r = verify_prng.get();
        let checked_data_share = share.0[0] * r;
        sketch_share[0] += checked_data_share;
        sketch_share[1] += checked_data_share * r;
        sketch_share[2] += share.0[1] * r;
        out_share.push(share.0[0]);
    }

    Ok((out_share, sketch_share))
}

/// Compute the Aggregator's share of the sketch verifier. The shares should sum to zero.
#[allow(non_snake_case)]
fn finish_sketch<F: FieldElement>(
    sketch: [F; 3],
    A_share: F,
    B_share: F,
    is_leader: bool,
) -> Vec<F> {
    let mut next_sketch_share = A_share * sketch[0] + B_share;
    if !is_leader {
        next_sketch_share += sketch[0] * sketch[0] - sketch[1] - sketch[2];
    }
    vec![next_sketch_share]
}

fn next_message<F: FieldElement>(
    mut share_0: Vec<F>,
    share_1: Vec<F>,
) -> Result<Option<[F; 3]>, VdafError> {
    merge_vector(&mut share_0, &share_1)?;

    if share_0.len() == 1 {
        if share_0[0] != F::zero() {
            Err(VdafError::Uncategorized(
                "sketch verification failed".into(),
            )) // Invalid sketch
        } else {
            Ok(None) // Sketch verification succeeded
        }
    } else if share_0.len() == 3 {
        Ok(Some([share_0[0], share_0[1], share_0[2]])) // Sketch verification continues
    } else {
        Err(VdafError::Uncategorized(format!(
            "unexpected sketch length ({})",
            share_0.len()
        )))
    }
}

fn aggregate<M: IntoIterator<Item = Poplar1FieldVec>>(
    is_leaf: bool,
    len: usize,
    shares: M,
) -> Result<Poplar1FieldVec, VdafError> {
    let mut result = Poplar1FieldVec::zero(is_leaf, len);
    for share in shares.into_iter() {
        result.accumulate(&share)?;
    }
    Ok(result)
}

/// A vector of two field elements.
///
/// This represents the values that Poplar1 programs into IDPFs while sharding.
#[derive(Debug, Clone, Copy)]
pub struct Poplar1IdpfValue<F>([F; 2]);

impl<F> Poplar1IdpfValue<F> {
    /// Create a new value from a pair of field elements.
    pub fn new(array: [F; 2]) -> Self {
        Self(array)
    }
}

impl<F> IdpfValue for Poplar1IdpfValue<F>
where
    F: FieldElement,
{
    type ValueParameter = ();

    fn zero(_: &()) -> Self {
        Self([F::zero(); 2])
    }

    fn generate<S: RngCore>(seed_stream: &mut S, _: &()) -> Self {
        Self([F::generate(seed_stream, &()), F::generate(seed_stream, &())])
    }

    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
        ConditionallySelectable::conditional_select(a, b, choice)
    }
}

impl<F> Add for Poplar1IdpfValue<F>
where
    F: Copy + Add<Output = F>,
{
    type Output = Self;

    fn add(self, rhs: Self) -> Self {
        Self([self.0[0] + rhs.0[0], self.0[1] + rhs.0[1]])
    }
}

impl<F> AddAssign for Poplar1IdpfValue<F>
where
    F: Copy + AddAssign,
{
    fn add_assign(&mut self, rhs: Self) {
        self.0[0] += rhs.0[0];
        self.0[1] += rhs.0[1];
    }
}

impl<F> Sub for Poplar1IdpfValue<F>
where
    F: Copy + Sub<Output = F>,
{
    type Output = Self;

    fn sub(self, rhs: Self) -> Self {
        Self([self.0[0] - rhs.0[0], self.0[1] - rhs.0[1]])
    }
}

impl<F> PartialEq for Poplar1IdpfValue<F>
where
    F: PartialEq,
{
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl<F> ConstantTimeEq for Poplar1IdpfValue<F>
where
    F: ConstantTimeEq,
{
    fn ct_eq(&self, other: &Self) -> Choice {
        self.0.ct_eq(&other.0)
    }
}

impl<F> Encode for Poplar1IdpfValue<F>
where
    F: FieldElement,
{
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0[0].encode(bytes);
        self.0[1].encode(bytes);
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(F::ENCODED_SIZE * 2)
    }
}

impl<F> Decode for Poplar1IdpfValue<F>
where
    F: Decode,
{
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(Self([F::decode(bytes)?, F::decode(bytes)?]))
    }
}

impl<F> ConditionallySelectable for Poplar1IdpfValue<F>
where
    F: ConditionallySelectable,
{
    fn conditional_select(a: &Self, b: &Self, choice: subtle::Choice) -> Self {
        Self([
            F::conditional_select(&a.0[0], &b.0[0], choice),
            F::conditional_select(&a.0[1], &b.0[1], choice),
        ])
    }
}

impl<F> ConditionallyNegatable for Poplar1IdpfValue<F>
where
    F: ConditionallyNegatable,
{
    fn conditional_negate(&mut self, choice: subtle::Choice) {
        F::conditional_negate(&mut self.0[0], choice);
        F::conditional_negate(&mut self.0[1], choice);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vdaf::{equality_comparison_test, run_vdaf_prepare};
    use assert_matches::assert_matches;
    use rand::prelude::*;
    use serde::Deserialize;
    use std::collections::HashSet;

    fn test_prepare<P: Xof<SEED_SIZE>, const SEED_SIZE: usize>(
        vdaf: &Poplar1<P, SEED_SIZE>,
        verify_key: &[u8; SEED_SIZE],
        nonce: &[u8; 16],
        public_share: &Poplar1PublicShare,
        input_shares: &[Poplar1InputShare<SEED_SIZE>],
        agg_param: &Poplar1AggregationParam,
        expected_result: Vec<u64>,
    ) {
        let out_shares = run_vdaf_prepare(
            vdaf,
            verify_key,
            agg_param,
            nonce,
            public_share.clone(),
            input_shares.to_vec(),
        )
        .unwrap();

        // Convert aggregate shares and unshard.
        let agg_share_0 = vdaf.aggregate(agg_param, [out_shares[0].clone()]).unwrap();
        let agg_share_1 = vdaf.aggregate(agg_param, [out_shares[1].clone()]).unwrap();
        let result = vdaf
            .unshard(agg_param, [agg_share_0, agg_share_1], 1)
            .unwrap();
        assert_eq!(
            result, expected_result,
            "unexpected result (level={})",
            agg_param.level
        );
    }

    fn run_heavy_hitters<B: AsRef<[u8]>, P: Xof<SEED_SIZE>, const SEED_SIZE: usize>(
        vdaf: &Poplar1<P, SEED_SIZE>,
        verify_key: &[u8; SEED_SIZE],
        threshold: usize,
        measurements: impl IntoIterator<Item = B>,
        expected_result: impl IntoIterator<Item = B>,
    ) {
        let mut rng = thread_rng();

        // Sharding step
        let reports: Vec<(
            [u8; 16],
            Poplar1PublicShare,
            Vec<Poplar1InputShare<SEED_SIZE>>,
        )> = measurements
            .into_iter()
            .map(|measurement| {
                let nonce = rng.gen();
                let (public_share, input_shares) = vdaf
                    .shard(&IdpfInput::from_bytes(measurement.as_ref()), &nonce)
                    .unwrap();
                (nonce, public_share, input_shares)
            })
            .collect();

        let mut agg_param = Poplar1AggregationParam {
            level: 0,
            prefixes: vec![
                IdpfInput::from_bools(&[false]),
                IdpfInput::from_bools(&[true]),
            ],
        };

        let mut agg_result = Vec::new();
        for level in 0..vdaf.bits {
            let mut out_shares_0 = Vec::with_capacity(reports.len());
            let mut out_shares_1 = Vec::with_capacity(reports.len());

            // Preparation step
            for (nonce, public_share, input_shares) in reports.iter() {
                let out_shares = run_vdaf_prepare(
                    vdaf,
                    verify_key,
                    &agg_param,
                    nonce,
                    public_share.clone(),
                    input_shares.to_vec(),
                )
                .unwrap();

                out_shares_0.push(out_shares[0].clone());
                out_shares_1.push(out_shares[1].clone());
            }

            // Aggregation step
            let agg_share_0 = vdaf.aggregate(&agg_param, out_shares_0).unwrap();
            let agg_share_1 = vdaf.aggregate(&agg_param, out_shares_1).unwrap();

            // Unsharding step
            agg_result = vdaf
                .unshard(&agg_param, [agg_share_0, agg_share_1], reports.len())
                .unwrap();

            agg_param.level += 1;

            // Unless this is the last level of the tree, construct the next set of candidate
            // prefixes.
            if level < vdaf.bits - 1 {
                let mut next_prefixes = Vec::new();
                for (prefix, count) in agg_param.prefixes.into_iter().zip(agg_result.iter()) {
                    if *count >= threshold as u64 {
                        next_prefixes.push(prefix.clone_with_suffix(&[false]));
                        next_prefixes.push(prefix.clone_with_suffix(&[true]));
                    }
                }

                agg_param.prefixes = next_prefixes;
            }
        }

        let got: HashSet<IdpfInput> = agg_param
            .prefixes
            .into_iter()
            .zip(agg_result.iter())
            .filter(|(_prefix, count)| **count >= threshold as u64)
            .map(|(prefix, _count)| prefix)
            .collect();

        let want: HashSet<IdpfInput> = expected_result
            .into_iter()
            .map(|bytes| IdpfInput::from_bytes(bytes.as_ref()))
            .collect();

        assert_eq!(got, want);
    }

    #[test]
    fn shard_prepare() {
        let mut rng = thread_rng();
        let vdaf = Poplar1::new_shake128(64);
        let verify_key = rng.gen();
        let input = IdpfInput::from_bytes(b"12341324");
        let nonce = rng.gen();
        let (public_share, input_shares) = vdaf.shard(&input, &nonce).unwrap();

        test_prepare(
            &vdaf,
            &verify_key,
            &nonce,
            &public_share,
            &input_shares,
            &Poplar1AggregationParam {
                level: 7,
                prefixes: vec![
                    IdpfInput::from_bytes(b"0"),
                    IdpfInput::from_bytes(b"1"),
                    IdpfInput::from_bytes(b"2"),
                    IdpfInput::from_bytes(b"f"),
                ],
            },
            vec![0, 1, 0, 0],
        );

        for level in 0..vdaf.bits {
            test_prepare(
                &vdaf,
                &verify_key,
                &nonce,
                &public_share,
                &input_shares,
                &Poplar1AggregationParam {
                    level: level as u16,
                    prefixes: vec![input.prefix(level)],
                },
                vec![1],
            );
        }
    }

    #[test]
    fn heavy_hitters() {
        let mut rng = thread_rng();
        let verify_key = rng.gen();
        let vdaf = Poplar1::new_shake128(8);

        run_heavy_hitters(
            &vdaf,
            &verify_key,
            2, // threshold
            [
                "a", "b", "c", "d", "e", "f", "g", "g", "h", "i", "i", "i", "j", "j", "k", "l",
            ], // measurements
            ["g", "i", "j"], // heavy hitters
        );
    }

    #[test]
    fn encoded_len() {
        // Input share
        let input_share = Poplar1InputShare {
            idpf_key: Seed::<16>::generate().unwrap(),
            corr_seed: Seed::<16>::generate().unwrap(),
            corr_inner: vec![
                [Field64::one(), <Field64 as FieldElement>::zero()],
                [Field64::one(), <Field64 as FieldElement>::zero()],
                [Field64::one(), <Field64 as FieldElement>::zero()],
            ],
            corr_leaf: [Field255::one(), <Field255 as FieldElement>::zero()],
        };
        assert_eq!(
            input_share.get_encoded().len(),
            input_share.encoded_len().unwrap()
        );

        // Prepaare message variants
        let prep_msg = Poplar1PrepareMessage(PrepareMessageVariant::SketchInner([
            Field64::one(),
            Field64::one(),
            Field64::one(),
        ]));
        assert_eq!(
            prep_msg.get_encoded().len(),
            prep_msg.encoded_len().unwrap()
        );
        let prep_msg = Poplar1PrepareMessage(PrepareMessageVariant::SketchLeaf([
            Field255::one(),
            Field255::one(),
            Field255::one(),
        ]));
        assert_eq!(
            prep_msg.get_encoded().len(),
            prep_msg.encoded_len().unwrap()
        );
        let prep_msg = Poplar1PrepareMessage(PrepareMessageVariant::Done);
        assert_eq!(
            prep_msg.get_encoded().len(),
            prep_msg.encoded_len().unwrap()
        );

        // Field vector variants.
        let field_vec = Poplar1FieldVec::Inner(vec![Field64::one(); 23]);
        assert_eq!(
            field_vec.get_encoded().len(),
            field_vec.encoded_len().unwrap()
        );
        let field_vec = Poplar1FieldVec::Leaf(vec![Field255::one(); 23]);
        assert_eq!(
            field_vec.get_encoded().len(),
            field_vec.encoded_len().unwrap()
        );

        // Aggregation parameter.
        let agg_param = Poplar1AggregationParam::try_from_prefixes(Vec::from([
            IdpfInput::from_bytes(b"ab"),
            IdpfInput::from_bytes(b"cd"),
        ]))
        .unwrap();
        assert_eq!(
            agg_param.get_encoded().len(),
            agg_param.encoded_len().unwrap()
        );
        let agg_param = Poplar1AggregationParam::try_from_prefixes(Vec::from([
            IdpfInput::from_bools(&[false]),
            IdpfInput::from_bools(&[true]),
        ]))
        .unwrap();
        assert_eq!(
            agg_param.get_encoded().len(),
            agg_param.encoded_len().unwrap()
        );
    }

    #[test]
    fn round_trip_prepare_state() {
        let vdaf = Poplar1::new_shake128(1);
        for (agg_id, prep_state) in [
            (
                0,
                Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                    sketch: SketchState::RoundOne {
                        A_share: Field64::from(0),
                        B_share: Field64::from(1),
                        is_leader: true,
                    },
                    output_share: Vec::from([Field64::from(2), Field64::from(3), Field64::from(4)]),
                })),
            ),
            (
                1,
                Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                    sketch: SketchState::RoundOne {
                        A_share: Field64::from(5),
                        B_share: Field64::from(6),
                        is_leader: false,
                    },
                    output_share: Vec::from([Field64::from(7), Field64::from(8), Field64::from(9)]),
                })),
            ),
            (
                0,
                Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share: Vec::from([
                        Field64::from(10),
                        Field64::from(11),
                        Field64::from(12),
                    ]),
                })),
            ),
            (
                1,
                Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share: Vec::from([
                        Field64::from(13),
                        Field64::from(14),
                        Field64::from(15),
                    ]),
                })),
            ),
            (
                0,
                Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                    sketch: SketchState::RoundOne {
                        A_share: Field255::from(16),
                        B_share: Field255::from(17),
                        is_leader: true,
                    },
                    output_share: Vec::from([
                        Field255::from(18),
                        Field255::from(19),
                        Field255::from(20),
                    ]),
                })),
            ),
            (
                1,
                Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                    sketch: SketchState::RoundOne {
                        A_share: Field255::from(21),
                        B_share: Field255::from(22),
                        is_leader: false,
                    },
                    output_share: Vec::from([
                        Field255::from(23),
                        Field255::from(24),
                        Field255::from(25),
                    ]),
                })),
            ),
            (
                0,
                Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share: Vec::from([
                        Field255::from(26),
                        Field255::from(27),
                        Field255::from(28),
                    ]),
                })),
            ),
            (
                1,
                Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                    sketch: SketchState::RoundTwo,
                    output_share: Vec::from([
                        Field255::from(29),
                        Field255::from(30),
                        Field255::from(31),
                    ]),
                })),
            ),
        ] {
            let encoded_prep_state = prep_state.get_encoded();
            assert_eq!(prep_state.encoded_len(), Some(encoded_prep_state.len()));
            let decoded_prep_state =
                Poplar1PrepareState::get_decoded_with_param(&(&vdaf, agg_id), &encoded_prep_state)
                    .unwrap();
            assert_eq!(prep_state, decoded_prep_state);
        }
    }

    #[test]
    fn round_trip_agg_param() {
        // These test cases were generated using the reference Sage implementation.
        // (https://github.com/cfrg/draft-irtf-cfrg-vdaf/tree/main/poc) Sage statements used to
        // generate each test case are given in comments.
        for (prefixes, reference_encoding) in [
            // poplar.encode_agg_param(0, [0])
            (
                Vec::from([IdpfInput::from_bools(&[false])]),
                [0, 0, 0, 0, 0, 1, 0].as_slice(),
            ),
            // poplar.encode_agg_param(0, [1])
            (
                Vec::from([IdpfInput::from_bools(&[true])]),
                [0, 0, 0, 0, 0, 1, 1].as_slice(),
            ),
            // poplar.encode_agg_param(0, [0, 1])
            (
                Vec::from([
                    IdpfInput::from_bools(&[false]),
                    IdpfInput::from_bools(&[true]),
                ]),
                [0, 0, 0, 0, 0, 2, 2].as_slice(),
            ),
            // poplar.encode_agg_param(1, [0b00, 0b01, 0b10, 0b11])
            (
                Vec::from([
                    IdpfInput::from_bools(&[false, false]),
                    IdpfInput::from_bools(&[false, true]),
                    IdpfInput::from_bools(&[true, false]),
                    IdpfInput::from_bools(&[true, true]),
                ]),
                [0, 1, 0, 0, 0, 4, 0xe4].as_slice(),
            ),
            // poplar.encode_agg_param(1, [0b00, 0b10, 0b11])
            (
                Vec::from([
                    IdpfInput::from_bools(&[false, false]),
                    IdpfInput::from_bools(&[true, false]),
                    IdpfInput::from_bools(&[true, true]),
                ]),
                [0, 1, 0, 0, 0, 3, 0x38].as_slice(),
            ),
            // poplar.encode_agg_param(2, [0b000, 0b001, 0b010, 0b011, 0b100, 0b101, 0b110, 0b111])
            (
                Vec::from([
                    IdpfInput::from_bools(&[false, false, false]),
                    IdpfInput::from_bools(&[false, false, true]),
                    IdpfInput::from_bools(&[false, true, false]),
                    IdpfInput::from_bools(&[false, true, true]),
                    IdpfInput::from_bools(&[true, false, false]),
                    IdpfInput::from_bools(&[true, false, true]),
                    IdpfInput::from_bools(&[true, true, false]),
                    IdpfInput::from_bools(&[true, true, true]),
                ]),
                [0, 2, 0, 0, 0, 8, 0xfa, 0xc6, 0x88].as_slice(),
            ),
            // poplar.encode_agg_param(9, [0b01_1011_0010, 0b10_1101_1010])
            (
                Vec::from([
                    IdpfInput::from_bools(&[
                        false, true, true, false, true, true, false, false, true, false,
                    ]),
                    IdpfInput::from_bools(&[
                        true, false, true, true, false, true, true, false, true, false,
                    ]),
                ]),
                [0, 9, 0, 0, 0, 2, 0x0b, 0x69, 0xb2].as_slice(),
            ),
            // poplar.encode_agg_param(15, [0xcafe])
            (
                Vec::from([IdpfInput::from_bytes(b"\xca\xfe")]),
                [0, 15, 0, 0, 0, 1, 0xca, 0xfe].as_slice(),
            ),
        ] {
            let agg_param = Poplar1AggregationParam::try_from_prefixes(prefixes).unwrap();
            let encoded = agg_param.get_encoded();
            assert_eq!(encoded, reference_encoding);
            let decoded = Poplar1AggregationParam::get_decoded(reference_encoding).unwrap();
            assert_eq!(decoded, agg_param);
        }
    }

    #[test]
    fn agg_param_wrong_unused_bit() {
        let err = Poplar1AggregationParam::get_decoded(&[0, 0, 0, 0, 0, 1, 2]).unwrap_err();
        assert_matches!(err, CodecError::UnexpectedValue);
    }

    #[test]
    fn agg_param_ordering() {
        let err = Poplar1AggregationParam::get_decoded(&[0, 0, 0, 0, 0, 2, 1]).unwrap_err();
        assert_matches!(err, CodecError::Other(_));
        let err = Poplar1AggregationParam::get_decoded(&[0, 0, 0, 0, 0, 2, 0]).unwrap_err();
        assert_matches!(err, CodecError::Other(_));
        let err = Poplar1AggregationParam::get_decoded(&[0, 0, 0, 0, 0, 2, 3]).unwrap_err();
        assert_matches!(err, CodecError::Other(_));
    }

    #[derive(Debug, Deserialize)]
    struct HexEncoded(#[serde(with = "hex")] Vec<u8>);

    impl AsRef<[u8]> for HexEncoded {
        fn as_ref(&self) -> &[u8] {
            &self.0
        }
    }

    #[derive(Debug, Deserialize)]
    struct PoplarTestVector {
        agg_param: (usize, Vec<u64>),
        agg_result: Vec<u64>,
        agg_shares: Vec<HexEncoded>,
        bits: usize,
        prep: Vec<PreparationTestVector>,
        verify_key: HexEncoded,
    }

    #[derive(Debug, Deserialize)]
    struct PreparationTestVector {
        input_shares: Vec<HexEncoded>,
        measurement: u64,
        nonce: HexEncoded,
        out_shares: Vec<Vec<HexEncoded>>,
        prep_messages: Vec<HexEncoded>,
        prep_shares: Vec<Vec<HexEncoded>>,
        public_share: HexEncoded,
        rand: HexEncoded,
    }

    fn check_test_vec(input: &str) {
        let test_vector: PoplarTestVector = serde_json::from_str(input).unwrap();
        assert_eq!(test_vector.prep.len(), 1);
        let prep = &test_vector.prep[0];
        let measurement_bits = (0..test_vector.bits)
            .rev()
            .map(|i| (prep.measurement >> i) & 1 != 0)
            .collect::<BitVec>();
        let measurement = IdpfInput::from(measurement_bits);
        let (agg_param_level, agg_param_prefixes_int) = test_vector.agg_param;
        let agg_param_prefixes = agg_param_prefixes_int
            .iter()
            .map(|int| {
                let bits = (0..=agg_param_level)
                    .rev()
                    .map(|i| (*int >> i) & 1 != 0)
                    .collect::<BitVec>();
                bits.into()
            })
            .collect::<Vec<IdpfInput>>();
        let agg_param = Poplar1AggregationParam::try_from_prefixes(agg_param_prefixes).unwrap();
        let verify_key = test_vector.verify_key.as_ref().try_into().unwrap();
        let nonce = prep.nonce.as_ref().try_into().unwrap();

        let mut idpf_random = [[0u8; 16]; 2];
        let mut poplar_random = [[0u8; 16]; 3];
        for (input, output) in prep
            .rand
            .as_ref()
            .chunks_exact(16)
            .zip(idpf_random.iter_mut().chain(poplar_random.iter_mut()))
        {
            output.copy_from_slice(input);
        }

        // Shard measurement.
        let poplar = Poplar1::new_shake128(test_vector.bits);
        let (public_share, input_shares) = poplar
            .shard_with_random(&measurement, &nonce, &idpf_random, &poplar_random)
            .unwrap();

        // Run aggregation.
        let (init_prep_state_0, init_prep_share_0) = poplar
            .prepare_init(
                &verify_key,
                0,
                &agg_param,
                &nonce,
                &public_share,
                &input_shares[0],
            )
            .unwrap();
        let (init_prep_state_1, init_prep_share_1) = poplar
            .prepare_init(
                &verify_key,
                1,
                &agg_param,
                &nonce,
                &public_share,
                &input_shares[1],
            )
            .unwrap();

        let r1_prep_msg = poplar
            .prepare_shares_to_prepare_message(
                &agg_param,
                [init_prep_share_0.clone(), init_prep_share_1.clone()],
            )
            .unwrap();

        let (r1_prep_state_0, r1_prep_share_0) = assert_matches!(
            poplar
                .prepare_next(init_prep_state_0.clone(), r1_prep_msg.clone())
                .unwrap(),
            PrepareTransition::Continue(state, share) => (state, share)
        );
        let (r1_prep_state_1, r1_prep_share_1) = assert_matches!(
            poplar
                .prepare_next(init_prep_state_1.clone(), r1_prep_msg.clone())
                .unwrap(),
            PrepareTransition::Continue(state, share) => (state, share)
        );

        let r2_prep_msg = poplar
            .prepare_shares_to_prepare_message(
                &agg_param,
                [r1_prep_share_0.clone(), r1_prep_share_1.clone()],
            )
            .unwrap();

        let out_share_0 = assert_matches!(
            poplar
                .prepare_next(r1_prep_state_0.clone(), r2_prep_msg.clone())
                .unwrap(),
            PrepareTransition::Finish(out) => out
        );
        let out_share_1 = assert_matches!(
            poplar
                .prepare_next(r1_prep_state_1, r2_prep_msg.clone())
                .unwrap(),
            PrepareTransition::Finish(out) => out
        );

        let agg_share_0 = poplar.aggregate(&agg_param, [out_share_0.clone()]).unwrap();
        let agg_share_1 = poplar.aggregate(&agg_param, [out_share_1.clone()]).unwrap();

        // Collect result.
        let agg_result = poplar
            .unshard(&agg_param, [agg_share_0.clone(), agg_share_1.clone()], 1)
            .unwrap();

        // Check all intermediate results against the test vector, and exercise both encoding and decoding.
        assert_eq!(
            public_share,
            Poplar1PublicShare::get_decoded_with_param(&poplar, prep.public_share.as_ref())
                .unwrap()
        );
        assert_eq!(&public_share.get_encoded(), prep.public_share.as_ref());
        assert_eq!(
            input_shares[0],
            Poplar1InputShare::get_decoded_with_param(&(&poplar, 0), prep.input_shares[0].as_ref())
                .unwrap()
        );
        assert_eq!(
            &input_shares[0].get_encoded(),
            prep.input_shares[0].as_ref()
        );
        assert_eq!(
            input_shares[1],
            Poplar1InputShare::get_decoded_with_param(&(&poplar, 1), prep.input_shares[1].as_ref())
                .unwrap()
        );
        assert_eq!(
            &input_shares[1].get_encoded(),
            prep.input_shares[1].as_ref()
        );
        assert_eq!(
            init_prep_share_0,
            Poplar1FieldVec::get_decoded_with_param(
                &init_prep_state_0,
                prep.prep_shares[0][0].as_ref()
            )
            .unwrap()
        );
        assert_eq!(
            &init_prep_share_0.get_encoded(),
            prep.prep_shares[0][0].as_ref()
        );
        assert_eq!(
            init_prep_share_1,
            Poplar1FieldVec::get_decoded_with_param(
                &init_prep_state_1,
                prep.prep_shares[0][1].as_ref()
            )
            .unwrap()
        );
        assert_eq!(
            &init_prep_share_1.get_encoded(),
            prep.prep_shares[0][1].as_ref()
        );
        assert_eq!(
            r1_prep_msg,
            Poplar1PrepareMessage::get_decoded_with_param(
                &init_prep_state_0,
                prep.prep_messages[0].as_ref()
            )
            .unwrap()
        );
        assert_eq!(&r1_prep_msg.get_encoded(), prep.prep_messages[0].as_ref());

        assert_eq!(
            r1_prep_share_0,
            Poplar1FieldVec::get_decoded_with_param(
                &r1_prep_state_0,
                prep.prep_shares[1][0].as_ref()
            )
            .unwrap()
        );
        assert_eq!(
            &r1_prep_share_0.get_encoded(),
            prep.prep_shares[1][0].as_ref()
        );
        assert_eq!(
            r1_prep_share_1,
            Poplar1FieldVec::get_decoded_with_param(
                &r1_prep_state_0,
                prep.prep_shares[1][1].as_ref()
            )
            .unwrap()
        );
        assert_eq!(
            &r1_prep_share_1.get_encoded(),
            prep.prep_shares[1][1].as_ref()
        );
        assert_eq!(
            r2_prep_msg,
            Poplar1PrepareMessage::get_decoded_with_param(
                &r1_prep_state_0,
                prep.prep_messages[1].as_ref()
            )
            .unwrap()
        );
        assert_eq!(&r2_prep_msg.get_encoded(), prep.prep_messages[1].as_ref());
        for (out_share, expected_out_share) in [
            (out_share_0, &prep.out_shares[0]),
            (out_share_1, &prep.out_shares[1]),
        ] {
            match out_share {
                Poplar1FieldVec::Inner(vec) => {
                    assert_eq!(vec.len(), expected_out_share.len());
                    for (element, expected) in vec.iter().zip(expected_out_share.iter()) {
                        assert_eq!(&element.get_encoded(), expected.as_ref());
                    }
                }
                Poplar1FieldVec::Leaf(vec) => {
                    assert_eq!(vec.len(), expected_out_share.len());
                    for (element, expected) in vec.iter().zip(expected_out_share.iter()) {
                        assert_eq!(&element.get_encoded(), expected.as_ref());
                    }
                }
            };
        }
        assert_eq!(
            agg_share_0,
            Poplar1FieldVec::get_decoded_with_param(
                &(&poplar, &agg_param),
                test_vector.agg_shares[0].as_ref()
            )
            .unwrap()
        );

        assert_eq!(
            &agg_share_0.get_encoded(),
            test_vector.agg_shares[0].as_ref()
        );
        assert_eq!(
            agg_share_1,
            Poplar1FieldVec::get_decoded_with_param(
                &(&poplar, &agg_param),
                test_vector.agg_shares[1].as_ref()
            )
            .unwrap()
        );
        assert_eq!(
            &agg_share_1.get_encoded(),
            test_vector.agg_shares[1].as_ref()
        );
        assert_eq!(agg_result, test_vector.agg_result);
    }

    #[test]
    fn test_vec_poplar1_0() {
        check_test_vec(include_str!("test_vec/07/Poplar1_0.json"));
    }

    #[test]
    fn test_vec_poplar1_1() {
        check_test_vec(include_str!("test_vec/07/Poplar1_1.json"));
    }

    #[test]
    fn test_vec_poplar1_2() {
        check_test_vec(include_str!("test_vec/07/Poplar1_2.json"));
    }

    #[test]
    fn test_vec_poplar1_3() {
        check_test_vec(include_str!("test_vec/07/Poplar1_3.json"));
    }

    #[test]
    fn input_share_equality_test() {
        equality_comparison_test(&[
            // Default.
            Poplar1InputShare {
                idpf_key: Seed([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]),
                corr_seed: Seed([16, 17, 18]),
                corr_inner: Vec::from([
                    [Field64::from(19), Field64::from(20)],
                    [Field64::from(21), Field64::from(22)],
                    [Field64::from(23), Field64::from(24)],
                ]),
                corr_leaf: [Field255::from(25), Field255::from(26)],
            },
            // Modified idpf_key.
            Poplar1InputShare {
                idpf_key: Seed([15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]),
                corr_seed: Seed([16, 17, 18]),
                corr_inner: Vec::from([
                    [Field64::from(19), Field64::from(20)],
                    [Field64::from(21), Field64::from(22)],
                    [Field64::from(23), Field64::from(24)],
                ]),
                corr_leaf: [Field255::from(25), Field255::from(26)],
            },
            // Modified corr_seed.
            Poplar1InputShare {
                idpf_key: Seed([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]),
                corr_seed: Seed([18, 17, 16]),
                corr_inner: Vec::from([
                    [Field64::from(19), Field64::from(20)],
                    [Field64::from(21), Field64::from(22)],
                    [Field64::from(23), Field64::from(24)],
                ]),
                corr_leaf: [Field255::from(25), Field255::from(26)],
            },
            // Modified corr_inner.
            Poplar1InputShare {
                idpf_key: Seed([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]),
                corr_seed: Seed([16, 17, 18]),
                corr_inner: Vec::from([
                    [Field64::from(24), Field64::from(23)],
                    [Field64::from(22), Field64::from(21)],
                    [Field64::from(20), Field64::from(19)],
                ]),
                corr_leaf: [Field255::from(25), Field255::from(26)],
            },
            // Modified corr_leaf.
            Poplar1InputShare {
                idpf_key: Seed([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]),
                corr_seed: Seed([16, 17, 18]),
                corr_inner: Vec::from([
                    [Field64::from(19), Field64::from(20)],
                    [Field64::from(21), Field64::from(22)],
                    [Field64::from(23), Field64::from(24)],
                ]),
                corr_leaf: [Field255::from(26), Field255::from(25)],
            },
        ])
    }

    #[test]
    fn prepare_state_equality_test() {
        // This test effectively covers PrepareStateVariant, PrepareState, SketchState as well.
        equality_comparison_test(&[
            // Inner, round one. (default)
            Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field64::from(0),
                    B_share: Field64::from(1),
                    is_leader: false,
                },
                output_share: Vec::from([Field64::from(2), Field64::from(3)]),
            })),
            // Inner, round one, modified A_share.
            Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field64::from(100),
                    B_share: Field64::from(1),
                    is_leader: false,
                },
                output_share: Vec::from([Field64::from(2), Field64::from(3)]),
            })),
            // Inner, round one, modified B_share.
            Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field64::from(0),
                    B_share: Field64::from(101),
                    is_leader: false,
                },
                output_share: Vec::from([Field64::from(2), Field64::from(3)]),
            })),
            // Inner, round one, modified is_leader.
            Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field64::from(0),
                    B_share: Field64::from(1),
                    is_leader: true,
                },
                output_share: Vec::from([Field64::from(2), Field64::from(3)]),
            })),
            // Inner, round one, modified output_share.
            Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field64::from(0),
                    B_share: Field64::from(1),
                    is_leader: false,
                },
                output_share: Vec::from([Field64::from(3), Field64::from(2)]),
            })),
            // Inner, round two. (default)
            Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                sketch: SketchState::RoundTwo,
                output_share: Vec::from([Field64::from(2), Field64::from(3)]),
            })),
            // Inner, round two, modified output_share.
            Poplar1PrepareState(PrepareStateVariant::Inner(PrepareState {
                sketch: SketchState::RoundTwo,
                output_share: Vec::from([Field64::from(3), Field64::from(2)]),
            })),
            // Leaf, round one. (default)
            Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field255::from(0),
                    B_share: Field255::from(1),
                    is_leader: false,
                },
                output_share: Vec::from([Field255::from(2), Field255::from(3)]),
            })),
            // Leaf, round one, modified A_share.
            Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field255::from(100),
                    B_share: Field255::from(1),
                    is_leader: false,
                },
                output_share: Vec::from([Field255::from(2), Field255::from(3)]),
            })),
            // Leaf, round one, modified B_share.
            Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field255::from(0),
                    B_share: Field255::from(101),
                    is_leader: false,
                },
                output_share: Vec::from([Field255::from(2), Field255::from(3)]),
            })),
            // Leaf, round one, modified is_leader.
            Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field255::from(0),
                    B_share: Field255::from(1),
                    is_leader: true,
                },
                output_share: Vec::from([Field255::from(2), Field255::from(3)]),
            })),
            // Leaf, round one, modified output_share.
            Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                sketch: SketchState::RoundOne {
                    A_share: Field255::from(0),
                    B_share: Field255::from(1),
                    is_leader: false,
                },
                output_share: Vec::from([Field255::from(3), Field255::from(2)]),
            })),
            // Leaf, round two. (default)
            Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                sketch: SketchState::RoundTwo,
                output_share: Vec::from([Field255::from(2), Field255::from(3)]),
            })),
            // Leaf, round two, modified output_share.
            Poplar1PrepareState(PrepareStateVariant::Leaf(PrepareState {
                sketch: SketchState::RoundTwo,
                output_share: Vec::from([Field255::from(3), Field255::from(2)]),
            })),
        ])
    }

    #[test]
    fn field_vec_equality_test() {
        equality_comparison_test(&[
            // Inner. (default)
            Poplar1FieldVec::Inner(Vec::from([Field64::from(0), Field64::from(1)])),
            // Inner, modified value.
            Poplar1FieldVec::Inner(Vec::from([Field64::from(1), Field64::from(0)])),
            // Leaf. (deafult)
            Poplar1FieldVec::Leaf(Vec::from([Field255::from(0), Field255::from(1)])),
            // Leaf, modified value.
            Poplar1FieldVec::Leaf(Vec::from([Field255::from(1), Field255::from(0)])),
        ])
    }
}
