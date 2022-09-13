#![feature(test)]
extern crate test;

use scroll::{Cread, Pread, LE};
use test::black_box;

#[bench]
fn bench_parallel_cread_with(b: &mut test::Bencher) {
    use rayon::prelude::*;
    let vec = vec![0u8; 1_000_000];
    let nums = vec![0usize; 500_000];
    b.iter(|| {
        let data = black_box(&vec[..]);
        nums.par_iter().for_each(|offset| {
            let _: u16 = black_box(data.cread_with(*offset, LE));
        });
    });
    b.bytes = vec.len() as u64;
}

#[bench]
fn bench_cread_vec(b: &mut test::Bencher) {
    let vec = vec![0u8; 1_000_000];
    b.iter(|| {
        let data = black_box(&vec[..]);
        for val in data.chunks(2) {
            let _: u16 = black_box(val.cread_with(0, LE));
        }
    });
    b.bytes = vec.len() as u64;
}

#[bench]
fn bench_cread(b: &mut test::Bencher) {
    const NITER: i32 = 100_000;
    b.iter(|| {
        for _ in 1..NITER {
            let data = black_box([1, 2]);
            let _: u16 = black_box(data.cread(0));
        }
    });
    b.bytes = 2 * NITER as u64;
}

#[bench]
fn bench_pread_ctx_vec(b: &mut test::Bencher) {
    let vec = vec![0u8; 1_000_000];
    b.iter(|| {
        let data = black_box(&vec[..]);
        for val in data.chunks(2) {
            let _: Result<u16, _> = black_box(val.pread(0));
        }
    });
    b.bytes = vec.len() as u64;
}

#[bench]
fn bench_pread_with_unwrap(b: &mut test::Bencher) {
    const NITER: i32 = 100_000;
    b.iter(|| {
        for _ in 1..NITER {
            let data: &[u8] = &black_box([1, 2]);
            let _: u16 = black_box(data.pread_with(0, LE).unwrap());
        }
    });
    b.bytes = 2 * NITER as u64;
}

#[bench]
fn bench_pread_vec(b: &mut test::Bencher) {
    let vec = vec![0u8; 1_000_000];
    b.iter(|| {
        let data = black_box(&vec[..]);
        for val in data.chunks(2) {
            let _: Result<u16, _> = black_box(val.pread_with(0, LE));
        }
    });
    b.bytes = vec.len() as u64;
}

#[bench]
fn bench_pread_unwrap(b: &mut test::Bencher) {
    const NITER: i32 = 100_000;
    b.iter(|| {
        for _ in 1..NITER {
            let data = black_box([1, 2]);
            let _: u16 = black_box(data.pread(0)).unwrap();
        }
    });
    b.bytes = 2 * NITER as u64;
}

#[bench]
fn bench_gread_vec(b: &mut test::Bencher) {
    let vec = vec![0u8; 1_000_000];
    b.iter(|| {
        let data = black_box(&vec[..]);
        for val in data.chunks(2) {
            let mut offset = 0;
            let _: Result<u16, _> = black_box(val.gread(&mut offset));
        }
    });
    b.bytes = vec.len() as u64;
}

#[bench]
fn bench_gread_unwrap(b: &mut test::Bencher) {
    const NITER: i32 = 100_000;
    b.iter(|| {
        for _ in 1..NITER {
            let data = black_box([1, 2]);
            let mut offset = 0;
            let _: u16 = black_box(data.gread_with(&mut offset, LE).unwrap());
        }
    });
    b.bytes = 2 * NITER as u64;
}

#[bench]
fn bench_parallel_pread_with(b: &mut test::Bencher) {
    use rayon::prelude::*;
    let vec = vec![0u8; 1_000_000];
    let nums = vec![0usize; 500_000];
    b.iter(|| {
        let data = black_box(&vec[..]);
        nums.par_iter().for_each(|offset| {
            let _: Result<u16, _> = black_box(data.pread_with(*offset, LE));
        });
    });
    b.bytes = vec.len() as u64;
}

#[bench]
fn bench_byteorder_vec(b: &mut test::Bencher) {
    use byteorder::ReadBytesExt;
    let vec = vec![0u8; 1_000_000];
    b.iter(|| {
        let data = black_box(&vec[..]);
        for mut val in data.chunks(2) {
            let _: Result<u16, _> = black_box(val.read_u16::<byteorder::LittleEndian>());
        }
    });
    b.bytes = vec.len() as u64;
}

#[bench]
fn bench_byteorder(b: &mut test::Bencher) {
    use byteorder::ByteOrder;
    const NITER: i32 = 100_000;
    b.iter(|| {
        for _ in 1..NITER {
            let data = black_box([1, 2]);
            let _: u16 = black_box(byteorder::LittleEndian::read_u16(&data));
        }
    });
    b.bytes = 2 * NITER as u64;
}
