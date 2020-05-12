use std::mem;

use bytes::Buf;
use criterion::{Benchmark, Criterion, Throughput};
use prost::encoding::{decode_varint, encode_varint, encoded_len_varint};
use rand::{rngs::StdRng, seq::SliceRandom, SeedableRng};

fn benchmark_varint(criterion: &mut Criterion, name: &str, mut values: Vec<u64>) {
    // Shuffle the values in a stable order.
    values.shuffle(&mut StdRng::seed_from_u64(0));

    let encoded_len = values
        .iter()
        .cloned()
        .map(encoded_len_varint)
        .sum::<usize>() as u64;
    let decoded_len = (values.len() * mem::size_of::<u64>()) as u64;

    let encode_values = values.clone();
    let encode = Benchmark::new("encode", move |b| {
        let mut buf = Vec::<u8>::with_capacity(encode_values.len() * 10);
        b.iter(|| {
            buf.clear();
            for &value in &encode_values {
                encode_varint(value, &mut buf);
            }
            criterion::black_box(&buf);
        })
    })
    .throughput(Throughput::Bytes(encoded_len));

    let mut decode_values = values.clone();
    let decode = Benchmark::new("decode", move |b| {
        let mut buf = Vec::with_capacity(decode_values.len() * 10);
        for &value in &decode_values {
            encode_varint(value, &mut buf);
        }

        b.iter(|| {
            decode_values.clear();
            let mut buf = &mut buf.as_slice();
            while buf.has_remaining() {
                let value = decode_varint(&mut buf).unwrap();
                decode_values.push(value);
            }
            criterion::black_box(&decode_values);
        })
    })
    .throughput(Throughput::Bytes(decoded_len));

    let encoded_len_values = values.clone();
    let encoded_len = Benchmark::new("encoded_len", move |b| {
        b.iter(|| {
            let mut sum = 0;
            for &value in &encoded_len_values {
                sum += encoded_len_varint(value);
            }
            criterion::black_box(sum);
        })
    })
    .throughput(Throughput::Bytes(decoded_len));

    let name = format!("varint/{}", name);
    criterion
        .bench(&name, encode)
        .bench(&name, decode)
        .bench(&name, encoded_len);
}

fn main() {
    let mut criterion = Criterion::default().configure_from_args();

    // Benchmark encoding and decoding 100 small (1 byte) varints.
    benchmark_varint(&mut criterion, "small", (0..100).collect());

    // Benchmark encoding and decoding 100 medium (5 byte) varints.
    benchmark_varint(&mut criterion, "medium", (1 << 28..).take(100).collect());

    // Benchmark encoding and decoding 100 large (10 byte) varints.
    benchmark_varint(&mut criterion, "large", (1 << 63..).take(100).collect());

    // Benchmark encoding and decoding 100 varints of mixed width (average 5.5 bytes).
    benchmark_varint(
        &mut criterion,
        "mixed",
        (0..10)
            .flat_map(move |width| {
                let exponent = width * 7;
                (0..10).map(move |offset| offset + (1 << exponent))
            })
            .collect(),
    );

    criterion.final_summary();
}
