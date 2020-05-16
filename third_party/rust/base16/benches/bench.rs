#![allow(unknown_lints)]

use criterion::{criterion_group, criterion_main, Criterion, BatchSize, Throughput, ParameterizedBenchmark};
use rand::prelude::*;


const SIZES: &[usize] = &[3, 16, 64, 256, 1024];

fn rand_enc_input(sz: usize) -> (Vec<u8>, base16::EncConfig) {
    let mut rng = thread_rng();
    let mut vec = vec![0u8; sz];
    let cfg = if rng.gen::<bool>() {
        base16::EncodeUpper
    } else {
        base16::EncodeLower
    };
    rng.fill_bytes(&mut vec);
    (vec, cfg)
}

fn batch_size_for_input(i: usize) -> BatchSize {
    if i < 1024 {
        BatchSize::SmallInput
    } else {
        BatchSize::LargeInput
    }
}

fn bench_encode(c: &mut Criterion) {
    c.bench(
        "encode to fresh string",
        ParameterizedBenchmark::new(
            "encode_config",
            |b, items| {
                b.iter_batched(
                    || rand_enc_input(*items),
                    |(input, enc)| base16::encode_config(&input, enc),
                    batch_size_for_input(*items),
                )
            },
            SIZES.iter().cloned(),
        ).throughput(|bytes| Throughput::Bytes(*bytes as u32)),
    );

    c.bench(
        "encode to preallocated string",
        ParameterizedBenchmark::new(
            "encode_config_buf",
            |b, items| {
                b.iter_batched(
                    || (rand_enc_input(*items), String::with_capacity(2 * *items)),
                    |((input, enc), mut buf)| {
                        buf.truncate(0);
                        base16::encode_config_buf(&input, enc, &mut buf)
                    },
                    batch_size_for_input(*items),
                )
            },
            SIZES.iter().cloned(),
        ).throughput(|bytes| Throughput::Bytes(*bytes as u32)),
    );

    c.bench(
        "encode to slice",
        ParameterizedBenchmark::new(
            "encode_config_slice",
            |b, items| {
                b.iter_batched(
                    || (rand_enc_input(*items), vec![0u8; 2 * *items]),
                    |((input, enc), mut dst)| {
                        base16::encode_config_slice(&input, enc, &mut dst)
                    },
                    batch_size_for_input(*items),
                )
            },
            SIZES.iter().cloned(),
        ).throughput(|bytes| Throughput::Bytes(*bytes as u32)),
    );
}

fn rand_hex_string(size: usize) -> String {
    let mut rng = thread_rng();
    let mut s = String::with_capacity(size);
    let chars: &[u8] = b"0123456789abcdefABCDEF";
    while s.len() < size {
        s.push(*chars.choose(&mut rng).unwrap() as char);
    }
    s
}

fn bench_decode(c: &mut Criterion) {
    c.bench(
        "decode to fresh vec",
        ParameterizedBenchmark::new(
            "decode",
            |b, items| {
                b.iter_batched(
                    || rand_hex_string(*items),
                    |input| base16::decode(&input),
                    batch_size_for_input(*items),
                )
            },
            SIZES.iter().cloned(),
        ).throughput(|bytes| Throughput::Bytes(*bytes as u32 * 2)),
    );

    c.bench(
        "decode to preallocated vec",
        ParameterizedBenchmark::new(
            "decode_buf",
            |b, items| {
                b.iter_batched(
                    || (rand_hex_string(*items), Vec::with_capacity(*items)),
                    |(input, mut buf)| {
                        buf.truncate(0);
                        base16::decode_buf(&input, &mut buf)
                    },
                    batch_size_for_input(*items),
                )
            },
            SIZES.iter().cloned(),
        ).throughput(|bytes| Throughput::Bytes(*bytes as u32 * 2)),
    );

    c.bench(
        "decode to slice",
        ParameterizedBenchmark::new(
            "decode_slice",
            |b, items| {
                b.iter_batched(
                    || (rand_hex_string(*items), vec![0u8; *items]),
                    |(input, mut buf)| {
                        base16::decode_slice(&input, &mut buf)
                    },
                    batch_size_for_input(*items),
                )
            },
            SIZES.iter().cloned(),
        ).throughput(|bytes| Throughput::Bytes(*bytes as u32 * 2)),
    );
}


criterion_group!(benches, bench_encode, bench_decode);
criterion_main!(benches);
