// SPDX-License-Identifier: MPL-2.0

use crate::{
    codec::{Encode, ParameterizedDecode},
    flp::Type,
    vdaf::{
        prio3::{Prio3, Prio3InputShare, Prio3PrepareShare, Prio3PublicShare},
        xof::Xof,
        Aggregator, Collector, OutputShare, PrepareTransition, Vdaf,
    },
};
use serde::{Deserialize, Serialize};
use std::{collections::HashMap, convert::TryInto, fmt::Debug};

#[derive(Debug, Deserialize, Serialize)]
struct TEncoded(#[serde(with = "hex")] Vec<u8>);

impl AsRef<[u8]> for TEncoded {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

#[derive(Deserialize, Serialize)]
struct TPrio3Prep<M> {
    measurement: M,
    #[serde(with = "hex")]
    nonce: Vec<u8>,
    #[serde(with = "hex")]
    rand: Vec<u8>,
    public_share: TEncoded,
    input_shares: Vec<TEncoded>,
    prep_shares: Vec<Vec<TEncoded>>,
    prep_messages: Vec<TEncoded>,
    out_shares: Vec<Vec<TEncoded>>,
}

#[derive(Deserialize, Serialize)]
struct TPrio3<M> {
    verify_key: TEncoded,
    shares: u8,
    prep: Vec<TPrio3Prep<M>>,
    agg_shares: Vec<TEncoded>,
    agg_result: serde_json::Value,
    #[serde(flatten)]
    other_params: HashMap<String, serde_json::Value>,
}

macro_rules! err {
    (
        $test_num:ident,
        $error:expr,
        $msg:expr
    ) => {
        panic!("test #{} failed: {} err: {}", $test_num, $msg, $error)
    };
}

// TODO Generalize this method to work with any VDAF. To do so we would need to add
// `shard_with_random()` to traits. (There may be a less invasive alternative.)
fn check_prep_test_vec<M, T, P, const SEED_SIZE: usize>(
    prio3: &Prio3<T, P, SEED_SIZE>,
    verify_key: &[u8; SEED_SIZE],
    test_num: usize,
    t: &TPrio3Prep<M>,
) -> Vec<OutputShare<T::Field>>
where
    T: Type<Measurement = M>,
    P: Xof<SEED_SIZE>,
{
    let nonce = <[u8; 16]>::try_from(t.nonce.clone()).unwrap();
    let (public_share, input_shares) = prio3
        .shard_with_random(&t.measurement, &nonce, &t.rand)
        .expect("failed to generate input shares");

    assert_eq!(
        public_share,
        Prio3PublicShare::get_decoded_with_param(prio3, t.public_share.as_ref())
            .unwrap_or_else(|e| err!(test_num, e, "decode test vector (public share)")),
    );
    for (agg_id, want) in t.input_shares.iter().enumerate() {
        assert_eq!(
            input_shares[agg_id],
            Prio3InputShare::get_decoded_with_param(&(prio3, agg_id), want.as_ref())
                .unwrap_or_else(|e| err!(test_num, e, "decode test vector (input share)")),
            "#{test_num}"
        );
        assert_eq!(
            input_shares[agg_id].get_encoded(),
            want.as_ref(),
            "#{test_num}"
        )
    }

    let mut states = Vec::new();
    let mut prep_shares = Vec::new();
    for (agg_id, input_share) in input_shares.iter().enumerate() {
        let (state, prep_share) = prio3
            .prepare_init(verify_key, agg_id, &(), &nonce, &public_share, input_share)
            .unwrap_or_else(|e| err!(test_num, e, "prep state init"));
        states.push(state);
        prep_shares.push(prep_share);
    }

    assert_eq!(1, t.prep_shares.len(), "#{test_num}");
    for (i, want) in t.prep_shares[0].iter().enumerate() {
        assert_eq!(
            prep_shares[i],
            Prio3PrepareShare::get_decoded_with_param(&states[i], want.as_ref())
                .unwrap_or_else(|e| err!(test_num, e, "decode test vector (prep share)")),
            "#{test_num}"
        );
        assert_eq!(prep_shares[i].get_encoded(), want.as_ref(), "#{test_num}");
    }

    let inbound = prio3
        .prepare_shares_to_prepare_message(&(), prep_shares)
        .unwrap_or_else(|e| err!(test_num, e, "prep preprocess"));
    assert_eq!(t.prep_messages.len(), 1);
    assert_eq!(inbound.get_encoded(), t.prep_messages[0].as_ref());

    let mut out_shares = Vec::new();
    for state in states.iter_mut() {
        match prio3.prepare_next(state.clone(), inbound.clone()).unwrap() {
            PrepareTransition::Finish(out_share) => {
                out_shares.push(out_share);
            }
            _ => panic!("unexpected transition"),
        }
    }

    for (got, want) in out_shares.iter().zip(t.out_shares.iter()) {
        let got: Vec<Vec<u8>> = got.as_ref().iter().map(|x| x.get_encoded()).collect();
        assert_eq!(got.len(), want.len());
        for (got_elem, want_elem) in got.iter().zip(want.iter()) {
            assert_eq!(got_elem.as_slice(), want_elem.as_ref());
        }
    }

    out_shares
}

#[must_use]
fn check_aggregate_test_vec<M, T, P, const SEED_SIZE: usize>(
    prio3: &Prio3<T, P, SEED_SIZE>,
    t: &TPrio3<M>,
) -> T::AggregateResult
where
    T: Type<Measurement = M>,
    P: Xof<SEED_SIZE>,
{
    let verify_key = t.verify_key.as_ref().try_into().unwrap();

    let mut all_output_shares = vec![Vec::new(); prio3.num_aggregators()];
    for (test_num, p) in t.prep.iter().enumerate() {
        let output_shares = check_prep_test_vec(prio3, verify_key, test_num, p);
        for (aggregator_output_shares, output_share) in
            all_output_shares.iter_mut().zip(output_shares.into_iter())
        {
            aggregator_output_shares.push(output_share);
        }
    }

    let aggregate_shares = all_output_shares
        .into_iter()
        .map(|aggregator_output_shares| prio3.aggregate(&(), aggregator_output_shares).unwrap())
        .collect::<Vec<_>>();

    for (got, want) in aggregate_shares.iter().zip(t.agg_shares.iter()) {
        let got = got.get_encoded();
        assert_eq!(got.as_slice(), want.as_ref());
    }

    prio3.unshard(&(), aggregate_shares, 1).unwrap()
}

#[test]
fn test_vec_prio3_count() {
    for test_vector_str in [
        include_str!("test_vec/07/Prio3Count_0.json"),
        include_str!("test_vec/07/Prio3Count_1.json"),
    ] {
        let t: TPrio3<u64> = serde_json::from_str(test_vector_str).unwrap();
        let prio3 = Prio3::new_count(t.shares).unwrap();

        let aggregate_result = check_aggregate_test_vec(&prio3, &t);
        assert_eq!(aggregate_result, t.agg_result.as_u64().unwrap());
    }
}

#[test]
fn test_vec_prio3_sum() {
    for test_vector_str in [
        include_str!("test_vec/07/Prio3Sum_0.json"),
        include_str!("test_vec/07/Prio3Sum_1.json"),
    ] {
        let t: TPrio3<u128> = serde_json::from_str(test_vector_str).unwrap();
        let bits = t.other_params["bits"].as_u64().unwrap() as usize;
        let prio3 = Prio3::new_sum(t.shares, bits).unwrap();

        let aggregate_result = check_aggregate_test_vec(&prio3, &t);
        assert_eq!(aggregate_result, t.agg_result.as_u64().unwrap() as u128);
    }
}

#[test]
fn test_vec_prio3_sum_vec() {
    for test_vector_str in [
        include_str!("test_vec/07/Prio3SumVec_0.json"),
        include_str!("test_vec/07/Prio3SumVec_1.json"),
    ] {
        let t: TPrio3<Vec<u128>> = serde_json::from_str(test_vector_str).unwrap();
        let bits = t.other_params["bits"].as_u64().unwrap() as usize;
        let length = t.other_params["length"].as_u64().unwrap() as usize;
        let chunk_length = t.other_params["chunk_length"].as_u64().unwrap() as usize;
        let prio3 = Prio3::new_sum_vec(t.shares, bits, length, chunk_length).unwrap();

        let aggregate_result = check_aggregate_test_vec(&prio3, &t);
        let expected_aggregate_result = t
            .agg_result
            .as_array()
            .unwrap()
            .iter()
            .map(|val| val.as_u64().unwrap() as u128)
            .collect::<Vec<u128>>();
        assert_eq!(aggregate_result, expected_aggregate_result);
    }
}

#[test]
fn test_vec_prio3_histogram() {
    for test_vector_str in [
        include_str!("test_vec/07/Prio3Histogram_0.json"),
        include_str!("test_vec/07/Prio3Histogram_1.json"),
    ] {
        let t: TPrio3<usize> = serde_json::from_str(test_vector_str).unwrap();
        let length = t.other_params["length"].as_u64().unwrap() as usize;
        let chunk_length = t.other_params["chunk_length"].as_u64().unwrap() as usize;
        let prio3 = Prio3::new_histogram(t.shares, length, chunk_length).unwrap();

        let aggregate_result = check_aggregate_test_vec(&prio3, &t);
        let expected_aggregate_result = t
            .agg_result
            .as_array()
            .unwrap()
            .iter()
            .map(|val| val.as_u64().unwrap() as u128)
            .collect::<Vec<u128>>();
        assert_eq!(aggregate_result, expected_aggregate_result);
    }
}
