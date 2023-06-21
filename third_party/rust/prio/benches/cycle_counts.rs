#![cfg_attr(windows, allow(dead_code))]

use cfg_if::cfg_if;
use iai::black_box;
use prio::{
    field::{random_vector, Field128, Field64},
    vdaf::{
        prio3::{Prio3, Prio3InputShare},
        Client,
    },
};
#[cfg(feature = "prio2")]
use prio::{
    field::{FieldElement, FieldPrio2},
    server::VerificationMessage,
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
    let prio3 = Prio3::new_aes128_count(2).unwrap();
    let measurement = 1;
    prio3.shard(&black_box(measurement)).unwrap().1
}

fn prio3_client_histogram_11() -> Vec<Prio3InputShare<Field128, 16>> {
    let buckets: Vec<u64> = (1..10).collect();
    let prio3 = Prio3::new_aes128_histogram(2, &buckets).unwrap();
    let measurement = 17;
    prio3.shard(&black_box(measurement)).unwrap().1
}

fn prio3_client_sum_32() -> Vec<Prio3InputShare<Field128, 16>> {
    let prio3 = Prio3::new_aes128_sum(2, 16).unwrap();
    let measurement = 1337;
    prio3.shard(&black_box(measurement)).unwrap().1
}

fn prio3_client_count_vec_1000() -> Vec<Prio3InputShare<Field128, 16>> {
    let len = 1000;
    let prio3 = Prio3::new_aes128_count_vec(2, len).unwrap();
    let measurement = vec![0; len];
    prio3.shard(&black_box(measurement)).unwrap().1
}

#[cfg(feature = "multithreaded")]
fn prio3_client_count_vec_multithreaded_1000() -> Vec<Prio3InputShare<Field128, 16>> {
    let len = 1000;
    let prio3 = Prio3::new_aes128_count_vec_multithreaded(2, len).unwrap();
    let measurement = vec![0; len];
    prio3.shard(&black_box(measurement)).unwrap().1
}

cfg_if! {
    if #[cfg(windows)] {
        fn main() {
            eprintln!("Cycle count benchmarks are not supported on Windows.");
        }
    }
    else if #[cfg(feature = "prio2")] {
        cfg_if! {
            if #[cfg(feature = "multithreaded")] {
                iai::main!(
                    prng_16,
                    prng_256,
                    prng_1024,
                    prng_4096,
                    prio2_prove_10,
                    prio2_prove_100,
                    prio2_prove_1000,
                    prio2_prove_and_verify_10,
                    prio2_prove_and_verify_100,
                    prio2_prove_and_verify_1000,
                    prio3_client_count,
                    prio3_client_histogram_11,
                    prio3_client_sum_32,
                    prio3_client_count_vec_1000,
                    prio3_client_count_vec_multithreaded_1000,
                );
            } else {
                iai::main!(
                    prng_16,
                    prng_256,
                    prng_1024,
                    prng_4096,
                    prio2_prove_10,
                    prio2_prove_100,
                    prio2_prove_1000,
                    prio2_prove_and_verify_10,
                    prio2_prove_and_verify_100,
                    prio2_prove_and_verify_1000,
                    prio3_client_count,
                    prio3_client_histogram_11,
                    prio3_client_sum_32,
                    prio3_client_count_vec_1000,
                );
            }
        }
    } else {
        cfg_if! {
            if #[cfg(feature = "multithreaded")] {
                iai::main!(
                    prng_16,
                    prng_256,
                    prng_1024,
                    prng_4096,
                    prio3_client_count,
                    prio3_client_histogram_11,
                    prio3_client_sum_32,
                    prio3_client_count_vec_1000,
                    prio3_client_count_vec_multithreaded_1000,
                );
            } else {
                iai::main!(
                    prng_16,
                    prng_256,
                    prng_1024,
                    prng_4096,
                    prio3_client_count,
                    prio3_client_histogram_11,
                    prio3_client_sum_32,
                    prio3_client_count_vec_1000,
                );
            }
        }
    }
}
