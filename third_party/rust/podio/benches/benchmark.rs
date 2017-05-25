// bench.rs
#![feature(test)]

extern crate podio;
extern crate test;

use test::Bencher;
use podio::{Endianness, BigEndian, LittleEndian, WritePodExt, ReadPodExt};

// bench: find the `BENCH_SIZE` first terms of the fibonacci sequence
const BENCH_SIZE: usize = 2000;

#[bench]
fn write_u64_be(b: &mut Bencher) {
    write_u64::<BigEndian>(b);
}

#[bench]
fn write_u64_le(b: &mut Bencher) {
    write_u64::<LittleEndian>(b);
}

fn write_u64<T: Endianness>(b: &mut Bencher) {
    b.iter(|| {
        let mut writer : &mut[u8] = &mut [0; BENCH_SIZE * 8];
        for _ in 0..BENCH_SIZE {
            writer.write_u64::<T>(0x012345678u64).unwrap();
        }
    })
}

#[bench]
fn read_u64_be(b: &mut Bencher) {
    read_u64::<BigEndian>(b);
}

#[bench]
fn read_u64_le(b: &mut Bencher) {
    read_u64::<LittleEndian>(b);
}

fn read_u64<T: Endianness>(b: &mut Bencher) {
    b.iter(|| {
        let mut reader : &[u8] = &[0; BENCH_SIZE * 8];
        for _ in 0..BENCH_SIZE {
            reader.read_u64::<T>().unwrap();
        }
    })
}
