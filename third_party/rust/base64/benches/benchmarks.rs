#![feature(test)]

extern crate base64;
extern crate test;
extern crate rand;

use base64::{decode, decode_config_buf, encode, encode_config_buf, Config, MIME, STANDARD};

use test::Bencher;
use rand::Rng;

#[bench]
fn encode_3b(b: &mut Bencher) {
    do_encode_bench(b, 3)
}

#[bench]
fn encode_3b_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 3, STANDARD)
}

#[bench]
fn encode_50b(b: &mut Bencher) {
    do_encode_bench(b, 50)
}

#[bench]
fn encode_50b_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 50, STANDARD)
}

#[bench]
fn encode_100b(b: &mut Bencher) {
    do_encode_bench(b, 100)
}

#[bench]
fn encode_100b_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 100, STANDARD)
}

#[bench]
fn encode_500b(b: &mut Bencher) {
    do_encode_bench(b, 500)
}

#[bench]
fn encode_500b_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 500, STANDARD)
}

#[bench]
fn encode_500b_reuse_buf_mime(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 500, MIME)
}

#[bench]
fn encode_3kib(b: &mut Bencher) {
    do_encode_bench(b, 3 * 1024)
}

#[bench]
fn encode_3kib_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 3 * 1024, STANDARD)
}

#[bench]
fn encode_3kib_reuse_buf_mime(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 3 * 1024, MIME)
}

#[bench]
fn encode_3mib(b: &mut Bencher) {
    do_encode_bench(b, 3 * 1024 * 1024)
}

#[bench]
fn encode_3mib_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 3 * 1024 * 1024, STANDARD)
}

#[bench]
fn encode_10mib(b: &mut Bencher) {
    do_encode_bench(b, 10 * 1024 * 1024)
}

#[bench]
fn encode_10mib_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 10 * 1024 * 1024, STANDARD)
}

#[bench]
fn encode_30mib(b: &mut Bencher) {
    do_encode_bench(b, 30 * 1024 * 1024)
}

#[bench]
fn encode_30mib_reuse_buf(b: &mut Bencher) {
    do_encode_bench_reuse_buf(b, 30 * 1024 * 1024, STANDARD)
}

#[bench]
fn decode_3b(b: &mut Bencher) {
    do_decode_bench(b, 3)
}

#[bench]
fn decode_3b_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 3)
}

#[bench]
fn decode_50b(b: &mut Bencher) {
    do_decode_bench(b, 50)
}

#[bench]
fn decode_50b_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 50)
}

#[bench]
fn decode_100b(b: &mut Bencher) {
    do_decode_bench(b, 100)
}

#[bench]
fn decode_100b_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 100)
}

#[bench]
fn decode_500b(b: &mut Bencher) {
    do_decode_bench(b, 500)
}

#[bench]
fn decode_500b_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 500)
}

#[bench]
fn decode_3kib(b: &mut Bencher) {
    do_decode_bench(b, 3 * 1024)
}

#[bench]
fn decode_3kib_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 3 * 1024)
}

#[bench]
fn decode_3mib(b: &mut Bencher) {
    do_decode_bench(b, 3 * 1024 * 1024)
}

#[bench]
fn decode_3mib_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 3 * 1024 * 1024)
}

#[bench]
fn decode_10mib(b: &mut Bencher) {
    do_decode_bench(b, 10 * 1024 * 1024)
}

#[bench]
fn decode_10mib_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 10 * 1024 * 1024)
}

#[bench]
fn decode_30mib(b: &mut Bencher) {
    do_decode_bench(b, 30 * 1024 * 1024)
}

#[bench]
fn decode_30mib_reuse_buf(b: &mut Bencher) {
    do_decode_bench_reuse_buf(b, 30 * 1024 * 1024)
}

fn do_decode_bench(b: &mut Bencher, size: usize) {
    let mut v: Vec<u8> = Vec::with_capacity(size * 3 / 4);
    fill(&mut v);
    let encoded = encode(&v);

    b.bytes = encoded.len() as u64;
    b.iter(|| {
        let orig = decode(&encoded);
        test::black_box(&orig);
    });
}

fn do_decode_bench_reuse_buf(b: &mut Bencher, size: usize) {
    let mut v: Vec<u8> = Vec::with_capacity(size * 3 / 4);
    fill(&mut v);
    let encoded = encode(&v);

    let mut buf = Vec::new();
    b.bytes = encoded.len() as u64;
    b.iter(|| {
        decode_config_buf(&encoded, STANDARD, &mut buf).unwrap();
        test::black_box(&buf);
        buf.clear();
    });
}

fn do_encode_bench(b: &mut Bencher, size: usize) {
    let mut v: Vec<u8> = Vec::with_capacity(size);
    fill(&mut v);

    b.bytes = v.len() as u64;
    b.iter(|| {
        let e = encode(&v);
        test::black_box(&e);
    });
}

fn do_encode_bench_reuse_buf(b: &mut Bencher, size: usize, config: Config) {
    let mut v: Vec<u8> = Vec::with_capacity(size);
    fill(&mut v);

    let mut buf = String::new();

    b.bytes = v.len() as u64;
    b.iter(|| {
        encode_config_buf(&v, config, &mut buf);
        buf.clear();
    });
}

fn fill(v: &mut Vec<u8>) {
    let cap = v.capacity();
    // weak randomness is plenty; we just want to not be completely friendly to the branch predictor
    let mut r = rand::weak_rng();
    while v.len() < cap {
        v.push(r.gen::<u8>());
    }
}
