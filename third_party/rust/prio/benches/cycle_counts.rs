#![cfg_attr(windows, allow(dead_code))]

use cfg_if::cfg_if;
use iai::black_box;
#[cfg(feature = "experimental")]
use prio::{
    codec::{Decode, Encode, ParameterizedDecode},
    field::{Field255, FieldElement},
    idpf::{Idpf, IdpfInput, IdpfPublicShare, RingBufferCache},
    vdaf::{poplar1::Poplar1IdpfValue, xof::Seed},
};
#[cfg(feature = "prio2")]
use prio::{
    field::FieldPrio2,
    vdaf::{
        prio2::{Prio2, Prio2PrepareShare},
        Aggregator, Share,
    },
};
use prio::{
    field::{random_vector, Field128, Field64},
    vdaf::{
        prio3::{Prio3, Prio3InputShare},
        Client,
    },
};

fn prng(size: usize) -> Vec<Field128> {
    random_vector(size).unwrap()
}

fn prng_16() -> Vec<Field128> {
    prng(16)
}

fn prng_256() -> Vec<Field128> {
    prng(256)
}

fn prng_1024() -> Vec<Field128> {
    prng(1024)
}

fn prng_4096() -> Vec<Field128> {
    prng(4096)
}

#[cfg(feature = "prio2")]
fn prio2_client(size: usize) -> Vec<Share<FieldPrio2, 32>> {
    let prio2 = Prio2::new(size).unwrap();
    let input = vec![0u32; size];
    let nonce = [0; 16];
    prio2.shard(&black_box(input), &black_box(nonce)).unwrap().1
}

#[cfg(feature = "prio2")]
fn prio2_client_10() -> Vec<Share<FieldPrio2, 32>> {
    prio2_client(10)
}

#[cfg(feature = "prio2")]
fn prio2_client_100() -> Vec<Share<FieldPrio2, 32>> {
    prio2_client(100)
}

#[cfg(feature = "prio2")]
fn prio2_client_1000() -> Vec<Share<FieldPrio2, 32>> {
    prio2_client(1000)
}

#[cfg(feature = "prio2")]
fn prio2_shard_and_prepare(size: usize) -> Prio2PrepareShare {
    let prio2 = Prio2::new(size).unwrap();
    let input = vec![0u32; size];
    let nonce = [0; 16];
    let (public_share, input_shares) = prio2.shard(&black_box(input), &black_box(nonce)).unwrap();
    prio2
        .prepare_init(&[0; 32], 0, &(), &nonce, &public_share, &input_shares[0])
        .unwrap()
        .1
}

#[cfg(feature = "prio2")]
fn prio2_shard_and_prepare_10() -> Prio2PrepareShare {
    prio2_shard_and_prepare(10)
}

#[cfg(feature = "prio2")]
fn prio2_shard_and_prepare_100() -> Prio2PrepareShare {
    prio2_shard_and_prepare(100)
}

#[cfg(feature = "prio2")]
fn prio2_shard_and_prepare_1000() -> Prio2PrepareShare {
    prio2_shard_and_prepare(1000)
}

fn prio3_client_count() -> Vec<Prio3InputShare<Field64, 16>> {
    let prio3 = Prio3::new_count(2).unwrap();
    let measurement = 1;
    let nonce = [0; 16];
    prio3
        .shard(&black_box(measurement), &black_box(nonce))
        .unwrap()
        .1
}

fn prio3_client_histogram_10() -> Vec<Prio3InputShare<Field128, 16>> {
    let prio3 = Prio3::new_histogram(2, 10, 3).unwrap();
    let measurement = 9;
    let nonce = [0; 16];
    prio3
        .shard(&black_box(measurement), &black_box(nonce))
        .unwrap()
        .1
}

fn prio3_client_sum_32() -> Vec<Prio3InputShare<Field128, 16>> {
    let prio3 = Prio3::new_sum(2, 16).unwrap();
    let measurement = 1337;
    let nonce = [0; 16];
    prio3
        .shard(&black_box(measurement), &black_box(nonce))
        .unwrap()
        .1
}

fn prio3_client_count_vec_1000() -> Vec<Prio3InputShare<Field128, 16>> {
    let len = 1000;
    let prio3 = Prio3::new_sum_vec(2, 1, len, 31).unwrap();
    let measurement = vec![0; len];
    let nonce = [0; 16];
    prio3
        .shard(&black_box(measurement), &black_box(nonce))
        .unwrap()
        .1
}

#[cfg(feature = "multithreaded")]
fn prio3_client_count_vec_multithreaded_1000() -> Vec<Prio3InputShare<Field128, 16>> {
    let len = 1000;
    let prio3 = Prio3::new_sum_vec_multithreaded(2, 1, len, 31).unwrap();
    let measurement = vec![0; len];
    let nonce = [0; 16];
    prio3
        .shard(&black_box(measurement), &black_box(nonce))
        .unwrap()
        .1
}

#[cfg(feature = "experimental")]
fn idpf_poplar_gen(
    input: &IdpfInput,
    inner_values: Vec<Poplar1IdpfValue<Field64>>,
    leaf_value: Poplar1IdpfValue<Field255>,
) {
    let idpf = Idpf::new((), ());
    idpf.gen(input, inner_values, leaf_value, &[0; 16]).unwrap();
}

#[cfg(feature = "experimental")]
fn idpf_poplar_gen_8() {
    let input = IdpfInput::from_bytes(b"A");
    let one = Field64::one();
    idpf_poplar_gen(
        &input,
        vec![Poplar1IdpfValue::new([one, one]); 7],
        Poplar1IdpfValue::new([Field255::one(), Field255::one()]),
    );
}

#[cfg(feature = "experimental")]
fn idpf_poplar_gen_128() {
    let input = IdpfInput::from_bytes(b"AAAAAAAAAAAAAAAA");
    let one = Field64::one();
    idpf_poplar_gen(
        &input,
        vec![Poplar1IdpfValue::new([one, one]); 127],
        Poplar1IdpfValue::new([Field255::one(), Field255::one()]),
    );
}

#[cfg(feature = "experimental")]
fn idpf_poplar_gen_2048() {
    let input = IdpfInput::from_bytes(&[0x41; 256]);
    let one = Field64::one();
    idpf_poplar_gen(
        &input,
        vec![Poplar1IdpfValue::new([one, one]); 2047],
        Poplar1IdpfValue::new([Field255::one(), Field255::one()]),
    );
}

#[cfg(feature = "experimental")]
fn idpf_poplar_eval(
    input: &IdpfInput,
    public_share: &IdpfPublicShare<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>,
    key: &Seed<16>,
) {
    let mut cache = RingBufferCache::new(1);
    let idpf = Idpf::new((), ());
    idpf.eval(0, public_share, key, input, &[0; 16], &mut cache)
        .unwrap();
}

#[cfg(feature = "experimental")]
fn idpf_poplar_eval_8() {
    let input = IdpfInput::from_bytes(b"A");
    let public_share = IdpfPublicShare::get_decoded_with_param(&8, &[0x7f; 306]).unwrap();
    let key = Seed::get_decoded(&[0xff; 16]).unwrap();
    idpf_poplar_eval(&input, &public_share, &key);
}

#[cfg(feature = "experimental")]
fn idpf_poplar_eval_128() {
    let input = IdpfInput::from_bytes(b"AAAAAAAAAAAAAAAA");
    let public_share = IdpfPublicShare::get_decoded_with_param(&128, &[0x7f; 4176]).unwrap();
    let key = Seed::get_decoded(&[0xff; 16]).unwrap();
    idpf_poplar_eval(&input, &public_share, &key);
}

#[cfg(feature = "experimental")]
fn idpf_poplar_eval_2048() {
    let input = IdpfInput::from_bytes(&[0x41; 256]);
    let public_share = IdpfPublicShare::get_decoded_with_param(&2048, &[0x7f; 66096]).unwrap();
    let key = Seed::get_decoded(&[0xff; 16]).unwrap();
    idpf_poplar_eval(&input, &public_share, &key);
}

#[cfg(feature = "experimental")]
fn idpf_codec() {
    let data = hex::decode(concat!(
        "9a",
        "0000000000000000000000000000000000000000000000",
        "01eb3a1bd6b5fa4a4500000000000000000000000000000000",
        "ffffffff0000000022522c3fd5a33cac00000000000000000000000000000000",
        "ffffffff0000000069f41eee46542b6900000000000000000000000000000000",
        "00000000000000000000000000000000000000000000000000000000000000",
        "017d1fd6df94280145a0dcc933ceb706e9219d50e7c4f92fd8ca9a0ffb7d819646",
    ))
    .unwrap();
    let bits = 4;
    let public_share = IdpfPublicShare::<Poplar1IdpfValue<Field64>, Poplar1IdpfValue<Field255>>::get_decoded_with_param(&bits, &data).unwrap();
    let encoded = public_share.get_encoded();
    let _ = black_box(encoded.len());
}

macro_rules! main_base {
    ( $( $func_name:ident ),* $(,)* ) => {
        iai::main!(
            prng_16,
            prng_256,
            prng_1024,
            prng_4096,
            prio3_client_count,
            prio3_client_histogram_10,
            prio3_client_sum_32,
            prio3_client_count_vec_1000,
            $( $func_name, )*
        );
    };
}

#[cfg(feature = "prio2")]
macro_rules! main_add_prio2 {
    ( $( $func_name:ident ),* $(,)* ) => {
        main_base!(
            prio2_client_10,
            prio2_client_100,
            prio2_client_1000,
            prio2_shard_and_prepare_10,
            prio2_shard_and_prepare_100,
            prio2_shard_and_prepare_1000,
            $( $func_name, )*
        );
    };
}

#[cfg(not(feature = "prio2"))]
macro_rules! main_add_prio2 {
    ( $( $func_name:ident ),* $(,)* ) => {
        main_base!(
            $( $func_name, )*
        );
    };
}

#[cfg(feature = "multithreaded")]
macro_rules! main_add_multithreaded {
    ( $( $func_name:ident ),* $(,)* ) => {
        main_add_prio2!(
            prio3_client_count_vec_multithreaded_1000,
            $( $func_name, )*
        );
    };
}

#[cfg(not(feature = "multithreaded"))]
macro_rules! main_add_multithreaded {
    ( $( $func_name:ident ),* $(,)* ) => {
        main_add_prio2!(
            $( $func_name, )*
        );
    };
}

#[cfg(feature = "experimental")]
macro_rules! main_add_experimental {
    ( $( $func_name:ident ),* $(,)* ) => {
        main_add_multithreaded!(
            idpf_codec,
            idpf_poplar_gen_8,
            idpf_poplar_gen_128,
            idpf_poplar_gen_2048,
            idpf_poplar_eval_8,
            idpf_poplar_eval_128,
            idpf_poplar_eval_2048,
            $( $func_name, )*
        );
    };
}

#[cfg(not(feature = "experimental"))]
macro_rules! main_add_experimental {
    ( $( $func_name:ident ),* $(,)* ) => {
        main_add_multithreaded!(
            $( $func_name, )*
        );
    };
}

cfg_if! {
    if #[cfg(windows)] {
        fn main() {
            eprintln!("Cycle count benchmarks are not supported on Windows.");
        }
    }
    else {
        main_add_experimental!();
    }
}
