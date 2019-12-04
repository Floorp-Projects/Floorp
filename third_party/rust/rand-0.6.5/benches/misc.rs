// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![feature(test)]

extern crate test;
extern crate rand;

const RAND_BENCH_N: u64 = 1000;

use test::Bencher;

use rand::prelude::*;

#[bench]
fn misc_gen_bool_const(b: &mut Bencher) {
    let mut rng = StdRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let mut accum = true;
        for _ in 0..::RAND_BENCH_N {
            accum ^= rng.gen_bool(0.18);
        }
        accum
    })
}

#[bench]
fn misc_gen_bool_var(b: &mut Bencher) {
    let mut rng = StdRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let mut accum = true;
        let mut p = 0.18;
        for _ in 0..::RAND_BENCH_N {
            accum ^= rng.gen_bool(p);
            p += 0.0001;
        }
        accum
    })
}

#[bench]
fn misc_gen_ratio_const(b: &mut Bencher) {
    let mut rng = StdRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let mut accum = true;
        for _ in 0..::RAND_BENCH_N {
            accum ^= rng.gen_ratio(2, 3);
        }
        accum
    })
}

#[bench]
fn misc_gen_ratio_var(b: &mut Bencher) {
    let mut rng = StdRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let mut accum = true;
        for i in 2..(::RAND_BENCH_N as u32 + 2) {
            accum ^= rng.gen_ratio(i, i + 1);
        }
        accum
    })
}

#[bench]
fn misc_bernoulli_const(b: &mut Bencher) {
    let mut rng = StdRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let d = rand::distributions::Bernoulli::new(0.18);
        let mut accum = true;
        for _ in 0..::RAND_BENCH_N {
            accum ^= rng.sample(d);
        }
        accum
    })
}

#[bench]
fn misc_bernoulli_var(b: &mut Bencher) {
    let mut rng = StdRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let mut accum = true;
        let mut p = 0.18;
        for _ in 0..::RAND_BENCH_N {
            let d = rand::distributions::Bernoulli::new(p);
            accum ^= rng.sample(d);
            p += 0.0001;
        }
        accum
    })
}

macro_rules! sample_binomial {
    ($name:ident, $n:expr, $p:expr) => {
        #[bench]
        fn $name(b: &mut Bencher) {
            let mut rng = SmallRng::from_rng(&mut thread_rng()).unwrap();
            let (n, p) = ($n, $p);
            b.iter(|| {
                let d = rand::distributions::Binomial::new(n, p);
                rng.sample(d)
            })
        }
    }
}

sample_binomial!(misc_binomial_1, 1, 0.9);
sample_binomial!(misc_binomial_10, 10, 0.9);
sample_binomial!(misc_binomial_100, 100, 0.99);
sample_binomial!(misc_binomial_1000, 1000, 0.01);
sample_binomial!(misc_binomial_1e12, 1000_000_000_000, 0.2);

#[bench]
fn gen_1k_iter_repeat(b: &mut Bencher) {
    use std::iter;
    let mut rng = SmallRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let v: Vec<u64> = iter::repeat(()).map(|()| rng.gen()).take(128).collect();
        v
    });
    b.bytes = 1024;
}

#[bench]
fn gen_1k_sample_iter(b: &mut Bencher) {
    use rand::distributions::{Distribution, Standard};
    let mut rng = SmallRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        let v: Vec<u64> = Standard.sample_iter(&mut rng).take(128).collect();
        v
    });
    b.bytes = 1024;
}

#[bench]
fn gen_1k_gen_array(b: &mut Bencher) {
    let mut rng = SmallRng::from_rng(&mut thread_rng()).unwrap();
    b.iter(|| {
        // max supported array length is 32!
        let v: [[u64; 32]; 4] = rng.gen();
        v
    });
    b.bytes = 1024;
}

#[bench]
fn gen_1k_fill(b: &mut Bencher) {
    let mut rng = SmallRng::from_rng(&mut thread_rng()).unwrap();
    let mut buf = [0u64; 128];
    b.iter(|| {
        rng.fill(&mut buf[..]);
        buf
    });
    b.bytes = 1024;
}
