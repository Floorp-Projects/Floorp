// SPDX-License-Identifier: MPL-2.0

//! Implementation of a dummy VDAF which conforms to the specification in [draft-irtf-cfrg-vdaf-06]
//! but does nothing. Useful for testing.
//!
//! [draft-irtf-cfrg-vdaf-06]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/06/

use crate::{
    codec::{CodecError, Decode, Encode},
    vdaf::{self, Aggregatable, PrepareTransition, VdafError},
};
use rand::random;
use std::{fmt::Debug, io::Cursor, sync::Arc};

type ArcPrepInitFn =
    Arc<dyn Fn(&AggregationParam) -> Result<(), VdafError> + 'static + Send + Sync>;
type ArcPrepStepFn = Arc<
    dyn Fn(&PrepareState) -> Result<PrepareTransition<Vdaf, 0, 16>, VdafError>
        + 'static
        + Send
        + Sync,
>;

/// Dummy VDAF that does nothing.
#[derive(Clone)]
pub struct Vdaf {
    prep_init_fn: ArcPrepInitFn,
    prep_step_fn: ArcPrepStepFn,
}

impl Debug for Vdaf {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Vdaf")
            .field("prep_init_fn", &"[redacted]")
            .field("prep_step_fn", &"[redacted]")
            .finish()
    }
}

impl Vdaf {
    /// The length of the verify key parameter for fake VDAF instantiations.
    pub const VERIFY_KEY_LEN: usize = 0;

    /// Construct a new instance of the dummy VDAF.
    pub fn new(rounds: u32) -> Self {
        Self {
            prep_init_fn: Arc::new(|_| -> Result<(), VdafError> { Ok(()) }),
            prep_step_fn: Arc::new(
                move |state| -> Result<PrepareTransition<Self, 0, 16>, VdafError> {
                    let new_round = state.current_round + 1;
                    if new_round == rounds {
                        Ok(PrepareTransition::Finish(OutputShare(state.input_share)))
                    } else {
                        Ok(PrepareTransition::Continue(
                            PrepareState {
                                current_round: new_round,
                                ..*state
                            },
                            (),
                        ))
                    }
                },
            ),
        }
    }

    /// Provide an alternate implementation of [`vdaf::Aggregator::prepare_init`].
    pub fn with_prep_init_fn<F: Fn(&AggregationParam) -> Result<(), VdafError>>(
        mut self,
        f: F,
    ) -> Self
    where
        F: 'static + Send + Sync,
    {
        self.prep_init_fn = Arc::new(f);
        self
    }

    /// Provide an alternate implementation of [`vdaf::Aggregator::prepare_step`].
    pub fn with_prep_step_fn<
        F: Fn(&PrepareState) -> Result<PrepareTransition<Self, 0, 16>, VdafError>,
    >(
        mut self,
        f: F,
    ) -> Self
    where
        F: 'static + Send + Sync,
    {
        self.prep_step_fn = Arc::new(f);
        self
    }
}

impl Default for Vdaf {
    fn default() -> Self {
        Self::new(1)
    }
}

impl vdaf::Vdaf for Vdaf {
    const ID: u32 = 0xFFFF0000;

    type Measurement = u8;
    type AggregateResult = u8;
    type AggregationParam = AggregationParam;
    type PublicShare = ();
    type InputShare = InputShare;
    type OutputShare = OutputShare;
    type AggregateShare = AggregateShare;

    fn num_aggregators(&self) -> usize {
        2
    }
}

impl vdaf::Aggregator<0, 16> for Vdaf {
    type PrepareState = PrepareState;
    type PrepareShare = ();
    type PrepareMessage = ();

    fn prepare_init(
        &self,
        _verify_key: &[u8; 0],
        _: usize,
        aggregation_param: &Self::AggregationParam,
        _nonce: &[u8; 16],
        _: &Self::PublicShare,
        input_share: &Self::InputShare,
    ) -> Result<(Self::PrepareState, Self::PrepareShare), VdafError> {
        (self.prep_init_fn)(aggregation_param)?;
        Ok((
            PrepareState {
                input_share: input_share.0,
                current_round: 0,
            },
            (),
        ))
    }

    fn prepare_shares_to_prepare_message<M: IntoIterator<Item = Self::PrepareShare>>(
        &self,
        _: &Self::AggregationParam,
        _: M,
    ) -> Result<Self::PrepareMessage, VdafError> {
        Ok(())
    }

    fn prepare_next(
        &self,
        state: Self::PrepareState,
        _: Self::PrepareMessage,
    ) -> Result<PrepareTransition<Self, 0, 16>, VdafError> {
        (self.prep_step_fn)(&state)
    }

    fn aggregate<M: IntoIterator<Item = Self::OutputShare>>(
        &self,
        _: &Self::AggregationParam,
        output_shares: M,
    ) -> Result<Self::AggregateShare, VdafError> {
        let mut aggregate_share = AggregateShare(0);
        for output_share in output_shares {
            aggregate_share.accumulate(&output_share)?;
        }
        Ok(aggregate_share)
    }
}

impl vdaf::Client<16> for Vdaf {
    fn shard(
        &self,
        measurement: &Self::Measurement,
        _nonce: &[u8; 16],
    ) -> Result<(Self::PublicShare, Vec<Self::InputShare>), VdafError> {
        let first_input_share = random();
        let (second_input_share, _) = measurement.overflowing_sub(first_input_share);
        Ok((
            (),
            Vec::from([
                InputShare(first_input_share),
                InputShare(second_input_share),
            ]),
        ))
    }
}

/// A dummy input share.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct InputShare(pub u8);

impl Encode for InputShare {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.encode(bytes)
    }

    fn encoded_len(&self) -> Option<usize> {
        self.0.encoded_len()
    }
}

impl Decode for InputShare {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(Self(u8::decode(bytes)?))
    }
}

/// Dummy aggregation parameter.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct AggregationParam(pub u8);

impl Encode for AggregationParam {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.encode(bytes)
    }

    fn encoded_len(&self) -> Option<usize> {
        self.0.encoded_len()
    }
}

impl Decode for AggregationParam {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(Self(u8::decode(bytes)?))
    }
}

/// Dummy output share.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct OutputShare(pub u8);

impl Decode for OutputShare {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(Self(u8::decode(bytes)?))
    }
}

impl Encode for OutputShare {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.encode(bytes);
    }

    fn encoded_len(&self) -> Option<usize> {
        self.0.encoded_len()
    }
}

/// Dummy prepare state.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PrepareState {
    input_share: u8,
    current_round: u32,
}

impl Encode for PrepareState {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.input_share.encode(bytes);
        self.current_round.encode(bytes);
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(self.input_share.encoded_len()? + self.current_round.encoded_len()?)
    }
}

impl Decode for PrepareState {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let input_share = u8::decode(bytes)?;
        let current_round = u32::decode(bytes)?;

        Ok(Self {
            input_share,
            current_round,
        })
    }
}

/// Dummy aggregate share.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct AggregateShare(pub u64);

impl Aggregatable for AggregateShare {
    type OutputShare = OutputShare;

    fn merge(&mut self, other: &Self) -> Result<(), VdafError> {
        self.0 += other.0;
        Ok(())
    }

    fn accumulate(&mut self, out_share: &Self::OutputShare) -> Result<(), VdafError> {
        self.0 += u64::from(out_share.0);
        Ok(())
    }
}

impl From<OutputShare> for AggregateShare {
    fn from(out_share: OutputShare) -> Self {
        Self(u64::from(out_share.0))
    }
}

impl Decode for AggregateShare {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let val = u64::decode(bytes)?;
        Ok(Self(val))
    }
}

impl Encode for AggregateShare {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.encode(bytes)
    }

    fn encoded_len(&self) -> Option<usize> {
        self.0.encoded_len()
    }
}
