// Copyright 2012-2014 The Rust Project Developers.
// Copyright 2017 Matt Brubeck.
// See the COPYRIGHT file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![feature(test)]

extern crate bit_vec;
extern crate rand;
extern crate smallbitvec;
extern crate test;

use bit_vec::BitVec;
use rand::{weak_rng, Rng, XorShiftRng};
use smallbitvec::SmallBitVec;
use test::{black_box, Bencher};

const BENCH_BITS: usize = 1 << 14;
const USIZE_BITS: usize = ::std::mem::size_of::<usize>() * 8;

fn rng() -> XorShiftRng {
    weak_rng()
}

#[bench]
fn bench_bit_set_big_fixed_bv(b: &mut Bencher) {
    let mut r = rng();
    let mut bit_vec = BitVec::from_elem(BENCH_BITS, false);
    b.iter(|| {
        for _ in 0..100 {
            bit_vec.set((r.next_u64() as usize) % BENCH_BITS, true);
        }
        black_box(&bit_vec);
    });
}

#[bench]
fn bench_bit_set_big_fixed_sbv(b: &mut Bencher) {
    let mut r = rng();
    let mut bit_vec = SmallBitVec::from_elem(BENCH_BITS, false);
    b.iter(|| {
        for _ in 0..100 {
            bit_vec.set(r.next_u64() as usize % BENCH_BITS, true);
        }
        black_box(&bit_vec);
    });
}

#[bench]
fn bench_big_set_big_variable_bv(b: &mut Bencher) {
    let mut r = rng();
    let mut bit_vec = BitVec::from_elem(BENCH_BITS, false);
    b.iter(|| {
        for _ in 0..100 {
            bit_vec.set((r.next_u64() as usize) % BENCH_BITS, r.gen());
        }
        black_box(&bit_vec);
    });
}

#[bench]
fn bench_bit_set_big_variable_sbv(b: &mut Bencher) {
    let mut r = rng();
    let mut bit_vec = SmallBitVec::from_elem(BENCH_BITS, false);
    b.iter(|| {
        for _ in 0..100 {
            bit_vec.set(r.next_u64() as usize % BENCH_BITS, r.gen());
        }
        black_box(&bit_vec);
    });
}

#[bench]
fn bench_bit_set_small_bv(b: &mut Bencher) {
    let mut r = rng();
    let mut bit_vec = BitVec::from_elem(USIZE_BITS, false);
    b.iter(|| {
        for _ in 0..100 {
            bit_vec.set((r.next_u64() as usize) % USIZE_BITS, true);
        }
        black_box(&bit_vec);
    });
}

#[bench]
fn bench_bit_set_small_sbv(b: &mut Bencher) {
    let mut r = rng();
    let mut bit_vec = SmallBitVec::from_elem(USIZE_BITS, false);
    b.iter(|| {
        for _ in 0..100 {
            bit_vec.set(r.next_u64() as usize % USIZE_BITS, true);
        }
        black_box(&bit_vec);
    });
}

#[bench]
fn bench_bit_vec_small_eq_bv(b: &mut Bencher) {
    let x = BitVec::from_elem(USIZE_BITS, false);
    let y = BitVec::from_elem(USIZE_BITS, false);
    b.iter(|| x == y);
}

#[bench]
fn bench_bit_vec_small_eq_sbv(b: &mut Bencher) {
    let x = SmallBitVec::from_elem(USIZE_BITS, false);
    let y = SmallBitVec::from_elem(USIZE_BITS, false);
    b.iter(|| x == y);
}

#[bench]
fn bench_bit_vec_big_eq_bv(b: &mut Bencher) {
    let x = BitVec::from_elem(BENCH_BITS, false);
    let y = BitVec::from_elem(BENCH_BITS, false);
    b.iter(|| x == y);
}

#[bench]
fn bench_bit_vec_big_eq_sbv(b: &mut Bencher) {
    let x = SmallBitVec::from_elem(BENCH_BITS, false);
    let y = SmallBitVec::from_elem(BENCH_BITS, false);
    b.iter(|| x == y);
}

#[bench]
fn bench_bit_vec_small_iter_bv(b: &mut Bencher) {
    let bit_vec = BitVec::from_elem(USIZE_BITS, false);
    b.iter(|| {
        let mut sum = 0;
        for _ in 0..10 {
            for pres in &bit_vec {
                sum += pres as usize;
            }
        }
        sum
    })
}

#[bench]
fn bench_bit_vec_small_iter_sbv(b: &mut Bencher) {
    let bit_vec = SmallBitVec::from_elem(USIZE_BITS, false);
    b.iter(|| {
        let mut sum = 0;
        for _ in 0..10 {
            for pres in &bit_vec {
                sum += pres as usize;
            }
        }
        sum
    })
}

#[bench]
fn bench_bit_vec_big_iter_bv(b: &mut Bencher) {
    let bit_vec = BitVec::from_elem(BENCH_BITS, false);
    b.iter(|| {
        let mut sum = 0;
        for pres in &bit_vec {
            sum += pres as usize;
        }
        sum
    })
}

#[bench]
fn bench_bit_vec_big_iter_sbv(b: &mut Bencher) {
    let bit_vec = SmallBitVec::from_elem(BENCH_BITS, false);
    b.iter(|| {
        let mut sum = 0;
        for pres in &bit_vec {
            sum += pres as usize;
        }
        sum
    })
}

#[bench]
fn bench_from_elem_bv(b: &mut Bencher) {
    let cap = black_box(BENCH_BITS);
    let bit = black_box(true);
    b.iter(|| BitVec::from_elem(cap, bit));
    b.bytes = cap as u64 / 8;
}

#[bench]
fn bench_from_elem_sbv(b: &mut Bencher) {
    let cap = black_box(BENCH_BITS);
    let bit = black_box(true);
    b.iter(|| SmallBitVec::from_elem(cap, bit));
    b.bytes = cap as u64 / 8;
}

#[bench]
fn bench_remove_small(b: &mut Bencher) {
    b.iter(|| {
        let mut v = SmallBitVec::from_elem(USIZE_BITS, false);
        for _ in 0..USIZE_BITS {
            v.remove(0);
        }
    });
}

#[bench]
fn bench_remove_big(b: &mut Bencher) {
    b.iter(|| {
        let mut v = SmallBitVec::from_elem(BENCH_BITS, false);
        for _ in 0..200 {
            v.remove(0);
        }
    });
}
