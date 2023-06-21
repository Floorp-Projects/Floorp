// SPDX-License-Identifier: MPL-2.0

//! Port of the ENPA Prio system to a VDAF. It is backwards compatible with
//! [`Client`](crate::client::Client) and [`Server`](crate::server::Server).

use super::{AggregateShare, OutputShare};
use crate::{
    client as v2_client,
    codec::{CodecError, Decode, Encode, ParameterizedDecode},
    field::{decode_fieldvec, FftFriendlyFieldElement, FieldElement, FieldPrio2},
    prng::Prng,
    server as v2_server,
    util::proof_length,
    vdaf::{
        prg::Seed, Aggregatable, Aggregator, Client, Collector, PrepareTransition, Share,
        ShareDecodingParameter, Vdaf, VdafError,
    },
};
use ring::hmac;
use std::{
    convert::{TryFrom, TryInto},
    io::Cursor,
};

/// The Prio2 VDAF. It supports the same measurement type as
/// [`Prio3SumVec`](crate::vdaf::prio3::Prio3SumVec) with `bits == 1` but uses the proof system and
/// finite field deployed in ENPA.
#[derive(Clone, Debug)]
pub struct Prio2 {
    input_len: usize,
}

impl Prio2 {
    /// Returns an instance of the VDAF for the given input length.
    pub fn new(input_len: usize) -> Result<Self, VdafError> {
        let n = (input_len + 1).next_power_of_two();
        if let Ok(size) = u32::try_from(2 * n) {
            if size > FieldPrio2::generator_order() {
                return Err(VdafError::Uncategorized(
                    "input size exceeds field capacity".into(),
                ));
            }
        } else {
            return Err(VdafError::Uncategorized(
                "input size exceeds memory capacity".into(),
            ));
        }

        Ok(Prio2 { input_len })
    }

    /// Prepare an input share for aggregation using the given field element `query_rand` to
    /// compute the verifier share.
    ///
    /// In the [`Aggregator`](crate::vdaf::Aggregator) trait implementation for [`Prio2`], the
    /// query randomness is computed jointly by the Aggregators. This method is designed to be used
    /// in applications, like ENPA, in which the query randomness is instead chosen by a
    /// third-party.
    pub fn prepare_init_with_query_rand(
        &self,
        query_rand: FieldPrio2,
        input_share: &Share<FieldPrio2, 32>,
        is_leader: bool,
    ) -> Result<(Prio2PrepareState, Prio2PrepareShare), VdafError> {
        let expanded_data: Option<Vec<FieldPrio2>> = match input_share {
            Share::Leader(_) => None,
            Share::Helper(ref seed) => {
                let prng = Prng::from_prio2_seed(seed.as_ref());
                Some(prng.take(proof_length(self.input_len)).collect())
            }
        };
        let data = match input_share {
            Share::Leader(ref data) => data,
            Share::Helper(_) => expanded_data.as_ref().unwrap(),
        };

        let mut mem = v2_server::ValidationMemory::new(self.input_len);
        let verifier_share = v2_server::generate_verification_message(
            self.input_len,
            query_rand,
            data, // Combined input and proof shares
            is_leader,
            &mut mem,
        )
        .map_err(|e| VdafError::Uncategorized(e.to_string()))?;

        Ok((
            Prio2PrepareState(input_share.truncated(self.input_len)),
            Prio2PrepareShare(verifier_share),
        ))
    }
}

impl Vdaf for Prio2 {
    const ID: u32 = 0xFFFF0000;
    type Measurement = Vec<u32>;
    type AggregateResult = Vec<u32>;
    type AggregationParam = ();
    type PublicShare = ();
    type InputShare = Share<FieldPrio2, 32>;
    type OutputShare = OutputShare<FieldPrio2>;
    type AggregateShare = AggregateShare<FieldPrio2>;

    fn num_aggregators(&self) -> usize {
        // Prio2 can easily be extended to support more than two Aggregators.
        2
    }
}

impl Client<16> for Prio2 {
    fn shard(
        &self,
        measurement: &Vec<u32>,
        _nonce: &[u8; 16],
    ) -> Result<(Self::PublicShare, Vec<Share<FieldPrio2, 32>>), VdafError> {
        if measurement.len() != self.input_len {
            return Err(VdafError::Uncategorized("incorrect input length".into()));
        }
        let mut input: Vec<FieldPrio2> = Vec::with_capacity(measurement.len());
        for int in measurement {
            input.push((*int).into());
        }

        let mut mem = v2_client::ClientMemory::new(self.input_len)?;
        let copy_data = |share_data: &mut [FieldPrio2]| {
            share_data[..].clone_from_slice(&input);
        };
        let mut leader_data = mem.prove_with(self.input_len, copy_data);

        let helper_seed = Seed::generate()?;
        let helper_prng = Prng::from_prio2_seed(helper_seed.as_ref());
        for (s1, d) in leader_data.iter_mut().zip(helper_prng.into_iter()) {
            *s1 -= d;
        }

        Ok((
            (),
            vec![Share::Leader(leader_data), Share::Helper(helper_seed)],
        ))
    }
}

/// State of each [`Aggregator`](crate::vdaf::Aggregator) during the Preparation phase.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Prio2PrepareState(Share<FieldPrio2, 32>);

impl Encode for Prio2PrepareState {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.encode(bytes);
    }

    fn encoded_len(&self) -> Option<usize> {
        self.0.encoded_len()
    }
}

impl<'a> ParameterizedDecode<(&'a Prio2, usize)> for Prio2PrepareState {
    fn decode_with_param(
        (prio2, agg_id): &(&'a Prio2, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        let share_decoder = if *agg_id == 0 {
            ShareDecodingParameter::Leader(prio2.input_len)
        } else {
            ShareDecodingParameter::Helper
        };
        let out_share = Share::decode_with_param(&share_decoder, bytes)?;
        Ok(Self(out_share))
    }
}

/// Message emitted by each [`Aggregator`](crate::vdaf::Aggregator) during the Preparation phase.
#[derive(Clone, Debug)]
pub struct Prio2PrepareShare(v2_server::VerificationMessage<FieldPrio2>);

impl Encode for Prio2PrepareShare {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.f_r.encode(bytes);
        self.0.g_r.encode(bytes);
        self.0.h_r.encode(bytes);
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(FieldPrio2::ENCODED_SIZE * 3)
    }
}

impl ParameterizedDecode<Prio2PrepareState> for Prio2PrepareShare {
    fn decode_with_param(
        _state: &Prio2PrepareState,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        Ok(Self(v2_server::VerificationMessage {
            f_r: FieldPrio2::decode(bytes)?,
            g_r: FieldPrio2::decode(bytes)?,
            h_r: FieldPrio2::decode(bytes)?,
        }))
    }
}

impl Aggregator<32, 16> for Prio2 {
    type PrepareState = Prio2PrepareState;
    type PrepareShare = Prio2PrepareShare;
    type PrepareMessage = ();

    fn prepare_init(
        &self,
        agg_key: &[u8; 32],
        agg_id: usize,
        _agg_param: &Self::AggregationParam,
        nonce: &[u8; 16],
        _public_share: &Self::PublicShare,
        input_share: &Share<FieldPrio2, 32>,
    ) -> Result<(Prio2PrepareState, Prio2PrepareShare), VdafError> {
        let is_leader = role_try_from(agg_id)?;

        // In the ENPA Prio system, the query randomness is generated by a third party and
        // distributed to the Aggregators after they receive their input shares. In a VDAF, shared
        // randomness is derived from a nonce selected by the client. For Prio2 we compute the
        // query using HMAC-SHA256 evaluated over the nonce.
        let hmac_key = hmac::Key::new(hmac::HMAC_SHA256, agg_key);
        let hmac_tag = hmac::sign(&hmac_key, nonce);
        let query_rand = Prng::from_prio2_seed(hmac_tag.as_ref().try_into().unwrap())
            .next()
            .unwrap();

        self.prepare_init_with_query_rand(query_rand, input_share, is_leader)
    }

    fn prepare_preprocess<M: IntoIterator<Item = Prio2PrepareShare>>(
        &self,
        inputs: M,
    ) -> Result<(), VdafError> {
        let verifier_shares: Vec<v2_server::VerificationMessage<FieldPrio2>> =
            inputs.into_iter().map(|msg| msg.0).collect();
        if verifier_shares.len() != 2 {
            return Err(VdafError::Uncategorized(
                "wrong number of verifier shares".into(),
            ));
        }

        if !v2_server::is_valid_share(&verifier_shares[0], &verifier_shares[1]) {
            return Err(VdafError::Uncategorized(
                "proof verifier check failed".into(),
            ));
        }

        Ok(())
    }

    fn prepare_step(
        &self,
        state: Prio2PrepareState,
        _input: (),
    ) -> Result<PrepareTransition<Self, 32, 16>, VdafError> {
        let data = match state.0 {
            Share::Leader(data) => data,
            Share::Helper(seed) => {
                let prng = Prng::from_prio2_seed(seed.as_ref());
                prng.take(self.input_len).collect()
            }
        };
        Ok(PrepareTransition::Finish(OutputShare::from(data)))
    }

    fn aggregate<M: IntoIterator<Item = OutputShare<FieldPrio2>>>(
        &self,
        _agg_param: &Self::AggregationParam,
        out_shares: M,
    ) -> Result<AggregateShare<FieldPrio2>, VdafError> {
        let mut agg_share = AggregateShare(vec![FieldPrio2::zero(); self.input_len]);
        for out_share in out_shares.into_iter() {
            agg_share.accumulate(&out_share)?;
        }

        Ok(agg_share)
    }
}

impl Collector for Prio2 {
    fn unshard<M: IntoIterator<Item = AggregateShare<FieldPrio2>>>(
        &self,
        _agg_param: &Self::AggregationParam,
        agg_shares: M,
        _num_measurements: usize,
    ) -> Result<Vec<u32>, VdafError> {
        let mut agg = AggregateShare(vec![FieldPrio2::zero(); self.input_len]);
        for agg_share in agg_shares.into_iter() {
            agg.merge(&agg_share)?;
        }

        Ok(agg.0.into_iter().map(u32::from).collect())
    }
}

impl<'a> ParameterizedDecode<(&'a Prio2, usize)> for Share<FieldPrio2, 32> {
    fn decode_with_param(
        (prio2, agg_id): &(&'a Prio2, usize),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        let is_leader = role_try_from(*agg_id).map_err(|e| CodecError::Other(Box::new(e)))?;
        let decoder = if is_leader {
            ShareDecodingParameter::Leader(proof_length(prio2.input_len))
        } else {
            ShareDecodingParameter::Helper
        };

        Share::decode_with_param(&decoder, bytes)
    }
}

impl<'a, F> ParameterizedDecode<(&'a Prio2, &'a ())> for OutputShare<F>
where
    F: FieldElement,
{
    fn decode_with_param(
        (prio2, _): &(&'a Prio2, &'a ()),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        decode_fieldvec(prio2.input_len, bytes).map(Self)
    }
}

impl<'a, F> ParameterizedDecode<(&'a Prio2, &'a ())> for AggregateShare<F>
where
    F: FieldElement,
{
    fn decode_with_param(
        (prio2, _): &(&'a Prio2, &'a ()),
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        decode_fieldvec(prio2.input_len, bytes).map(Self)
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
    use crate::{
        client::encode_simple,
        encrypt::{decrypt_share, encrypt_share, PrivateKey, PublicKey},
        field::random_vector,
        server::Server,
        vdaf::{fieldvec_roundtrip_test, run_vdaf, run_vdaf_prepare},
    };
    use rand::prelude::*;

    const PRIV_KEY1: &str = "BIl6j+J6dYttxALdjISDv6ZI4/VWVEhUzaS05LgrsfswmbLOgNt9HUC2E0w+9RqZx3XMkdEHBHfNuCSMpOwofVSq3TfyKwn0NrftKisKKVSaTOt5seJ67P5QL4hxgPWvxw==";
    const PRIV_KEY2: &str = "BNNOqoU54GPo+1gTPv+hCgA9U2ZCKd76yOMrWa1xTWgeb4LhFLMQIQoRwDVaW64g/WTdcxT4rDULoycUNFB60LER6hPEHg/ObBnRPV1rwS3nj9Bj0tbjVPPyL9p8QW8B+w==";

    #[test]
    fn run_prio2() {
        let prio2 = Prio2::new(6).unwrap();

        assert_eq!(
            run_vdaf(
                &prio2,
                &(),
                [
                    vec![0, 0, 0, 0, 1, 0],
                    vec![0, 1, 0, 0, 0, 0],
                    vec![0, 1, 1, 0, 0, 0],
                    vec![1, 1, 1, 0, 0, 0],
                    vec![0, 0, 0, 0, 1, 1],
                ]
            )
            .unwrap(),
            vec![1, 3, 2, 0, 2, 1],
        );
    }

    #[test]
    fn enpa_client_interop() {
        let mut rng = thread_rng();
        let priv_key1 = PrivateKey::from_base64(PRIV_KEY1).unwrap();
        let priv_key2 = PrivateKey::from_base64(PRIV_KEY2).unwrap();
        let pub_key1 = PublicKey::from(&priv_key1);
        let pub_key2 = PublicKey::from(&priv_key2);

        let data: Vec<FieldPrio2> = [0, 0, 1, 1, 0]
            .iter()
            .map(|x| FieldPrio2::from(*x))
            .collect();
        let (encrypted_input_share1, encrypted_input_share2) =
            encode_simple(&data, pub_key1, pub_key2).unwrap();

        let input_share1 = decrypt_share(&encrypted_input_share1, &priv_key1).unwrap();
        let input_share2 = decrypt_share(&encrypted_input_share2, &priv_key2).unwrap();

        let prio2 = Prio2::new(data.len()).unwrap();
        let input_shares = vec![
            Share::get_decoded_with_param(&(&prio2, 0), &input_share1).unwrap(),
            Share::get_decoded_with_param(&(&prio2, 1), &input_share2).unwrap(),
        ];

        let verify_key = rng.gen::<[u8; 32]>();
        let nonce = rng.gen::<[u8; 16]>();
        run_vdaf_prepare(&prio2, &verify_key, &(), &nonce, (), input_shares).unwrap();
    }

    #[test]
    fn enpa_server_interop() {
        let priv_key1 = PrivateKey::from_base64(PRIV_KEY1).unwrap();
        let priv_key2 = PrivateKey::from_base64(PRIV_KEY2).unwrap();
        let pub_key1 = PublicKey::from(&priv_key1);
        let pub_key2 = PublicKey::from(&priv_key2);

        let data = vec![0, 0, 1, 1, 0];
        let prio2 = Prio2::new(data.len()).unwrap();
        let ignored_nonce = [0; 16];
        let (_public_share, input_shares) = prio2.shard(&data, &ignored_nonce).unwrap();

        let encrypted_input_share1 =
            encrypt_share(&input_shares[0].get_encoded(), &pub_key1).unwrap();
        let encrypted_input_share2 =
            encrypt_share(&input_shares[1].get_encoded(), &pub_key2).unwrap();

        let mut server1 = Server::new(data.len(), true, priv_key1).unwrap();
        let mut server2 = Server::new(data.len(), false, priv_key2).unwrap();

        let eval_at: FieldPrio2 = random_vector(1).unwrap()[0];
        let verifier1 = server1
            .generate_verification_message(eval_at, &encrypted_input_share1)
            .unwrap();
        let verifier2 = server2
            .generate_verification_message(eval_at, &encrypted_input_share2)
            .unwrap();

        server1
            .aggregate(&encrypted_input_share1, &verifier1, &verifier2)
            .unwrap();
        server2
            .aggregate(&encrypted_input_share2, &verifier1, &verifier2)
            .unwrap();
    }

    #[test]
    fn prepare_state_serialization() {
        let mut rng = thread_rng();
        let verify_key = rng.gen::<[u8; 32]>();
        let nonce = rng.gen::<[u8; 16]>();
        let data = vec![0, 0, 1, 1, 0];
        let prio2 = Prio2::new(data.len()).unwrap();
        let (public_share, input_shares) = prio2.shard(&data, &nonce).unwrap();
        for (agg_id, input_share) in input_shares.iter().enumerate() {
            let (prepare_state, prepare_share) = prio2
                .prepare_init(
                    &verify_key,
                    agg_id,
                    &(),
                    &[0; 16],
                    &public_share,
                    input_share,
                )
                .unwrap();

            let encoded_prepare_state = prepare_state.get_encoded();
            let decoded_prepare_state = Prio2PrepareState::get_decoded_with_param(
                &(&prio2, agg_id),
                &encoded_prepare_state,
            )
            .expect("failed to decode prepare state");
            assert_eq!(decoded_prepare_state, prepare_state);
            assert_eq!(
                prepare_state.encoded_len().unwrap(),
                encoded_prepare_state.len()
            );

            let encoded_prepare_share = prepare_share.get_encoded();
            let decoded_prepare_share =
                Prio2PrepareShare::get_decoded_with_param(&prepare_state, &encoded_prepare_share)
                    .expect("failed to decode prepare share");
            assert_eq!(decoded_prepare_share.0.f_r, prepare_share.0.f_r);
            assert_eq!(decoded_prepare_share.0.g_r, prepare_share.0.g_r);
            assert_eq!(decoded_prepare_share.0.h_r, prepare_share.0.h_r);
            assert_eq!(
                prepare_share.encoded_len().unwrap(),
                encoded_prepare_share.len()
            );
        }
    }

    #[test]
    fn roundtrip_output_share() {
        let vdaf = Prio2::new(31).unwrap();
        fieldvec_roundtrip_test::<FieldPrio2, Prio2, OutputShare<FieldPrio2>>(&vdaf, &(), 31);
    }

    #[test]
    fn roundtrip_aggregate_share() {
        let vdaf = Prio2::new(31).unwrap();
        fieldvec_roundtrip_test::<FieldPrio2, Prio2, AggregateShare<FieldPrio2>>(&vdaf, &(), 31);
    }
}
