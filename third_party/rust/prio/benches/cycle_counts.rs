#![cfg_attr(windows, allow(dead_code))]

use cfg_if::cfg_if;
use iai::black_box;
#[cfg(any(feature = "prio2", feature = "experimental"))]
use prio::field::FieldElement;
#[cfg(feature = "experimental")]
use prio::{
    codec::{Decode, Encode, ParameterizedDecode},
    field::Field255,
    idpf::{self, IdpfInput, IdpfPublicShare, RingBufferCache},
    vdaf::{poplar1::Poplar1IdpfValue, prg::Seed},
};
#[cfg(feature = "prio2")]
use prio::{field::FieldPrio2, server::VerificationMessage};
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
const PRIO2_PUBKEY1: &str =
    "BIl6j+J6dYttxALdjISDv6ZI4/VWVEhUzaS05LgrsfswmbLOgNt9HUC2E0w+9RqZx3XMkdEHBHfNuCSMpOwofVQ=";
#[cfg(feature = "prio2")]
const PRIO2_PUBKEY2: &str =
    "BNNOqoU54GPo+1gTPv+hCgA9U2ZCKd76yOMrWa1xTWgeb4LhFLMQIQoRwDVaW64g/WTdcxT4rDULoycUNFB60LE=";

#[cfg(feature = "prio2")]
fn prio2_prove(size: usize) -> Vec<FieldPrio2> {
    use prio::{benchmarked::benchmarked_v2_prove, client::Client, encrypt::PublicKey};

    let input = vec![FieldPrio2::zero(); size];
    let pk1 = PublicKey::from_base64(PRIO2_PUBKEY1).unwrap();
    let pk2 = PublicKey::from_base64(PRIO2_PUBKEY2).unwrap();
    let mut client: Client<FieldPrio2> = Client::new(input.len(), pk1, pk2).unwrap();
    benchmarked_v2_prove(&black_box(input), &mut client)
}

#[cfg(feature = "prio2")]
fn prio2_prove_10() -> Vec<FieldPrio2> {
    prio2_prove(10)
}

#[cfg(feature = "prio2")]
fn prio2_prove_100() -> Vec<FieldPrio2> {
    prio2_prove(100)
}

#[cfg(feature = "prio2")]
fn prio2_prove_1000() -> Vec<FieldPrio2> {
    prio2_prove(1_000)
}

#[cfg(feature = "prio2")]
fn prio2_prove_and_verify(size: usize) -> VerificationMessage<FieldPrio2> {
    use prio::{
        benchmarked::benchmarked_v2_prove,
        client::Client,
        encrypt::PublicKey,
        server::{generate_verification_message, ValidationMemory},
    };

    let input = vec![FieldPrio2::zero(); size];
    let pk1 = PublicKey::from_base64(PRIO2_PUBKEY1).unwrap();
    let pk2 = PublicKey::from_base64(PRIO2_PUBKEY2).unwrap();
    let mut client: Client<FieldPrio2> = Client::new(input.len(), pk1, pk2).unwrap();
    let input_and_proof = benchmarked_v2_prove(&input, &mut client);
    let mut validator = ValidationMemory::new(input.len());
    let eval_at = random_vector(1).unwrap()[0];
    generate_verification_message(
        input.len(),
        eval_at,
        &black_box(input_and_proof),
        true,
        &mut validator,
    )
    .unwrap()
}

#[cfg(feature = "prio2")]
fn prio2_prove_and_verify_10() -> VerificationMessage<FieldPrio2> {
    prio2_prove_and_verify(10)
}

#[cfg(feature = "prio2")]
fn prio2_prove_and_verify_100() -> VerificationMessage<FieldPrio2> {
    prio2_prove_and_verify(100)
}

#[cfg(feature = "prio2")]
fn prio2_prove_and_verify_1000() -> VerificationMessage<FieldPrio2> {
    prio2_prove_and_verify(1_000)
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

fn prio3_client_histogram_11() -> Vec<Prio3InputShare<Field128, 16>> {
    let buckets: Vec<u64> = (1..10).collect();
    let prio3 = Prio3::new_histogram(2, &buckets).unwrap();
    let measurement = 17;
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
    let prio3 = Prio3::new_sum_vec(2, 1, len).unwrap();
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
    let prio3 = Prio3::new_sum_vec_multithreaded(2, 1, len).unwrap();
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
    idpf::gen(input, inner_values, leaf_value, &[0; 16]).unwrap();
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
    idpf::eval(0, public_share, key, input, &[0; 16], &mut cache).unwrap();
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
            prio3_client_histogram_11,
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
            prio2_prove_10,
            prio2_prove_100,
            prio2_prove_1000,
            prio2_prove_and_verify_10,
            prio2_prove_and_verify_100,
            prio2_prove_and_verify_1000,
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
