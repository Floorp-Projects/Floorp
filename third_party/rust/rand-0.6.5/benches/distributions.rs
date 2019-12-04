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

use std::mem::size_of;
use test::Bencher;
use std::time::Duration;

use rand::{Rng, FromEntropy};
use rand::rngs::SmallRng;
use rand::distributions::*;

macro_rules! distr_int {
    ($fnn:ident, $ty:ty, $distr:expr) => {
        #[bench]
        fn $fnn(b: &mut Bencher) {
            let mut rng = SmallRng::from_entropy();
            let distr = $distr;

            b.iter(|| {
                let mut accum = 0 as $ty;
                for _ in 0..::RAND_BENCH_N {
                    let x: $ty = distr.sample(&mut rng);
                    accum = accum.wrapping_add(x);
                }
                accum
            });
            b.bytes = size_of::<$ty>() as u64 * ::RAND_BENCH_N;
        }
    }
}

macro_rules! distr_float {
    ($fnn:ident, $ty:ty, $distr:expr) => {
        #[bench]
        fn $fnn(b: &mut Bencher) {
            let mut rng = SmallRng::from_entropy();
            let distr = $distr;

            b.iter(|| {
                let mut accum = 0.0;
                for _ in 0..::RAND_BENCH_N {
                    let x: $ty = distr.sample(&mut rng);
                    accum += x;
                }
                accum
            });
            b.bytes = size_of::<$ty>() as u64 * ::RAND_BENCH_N;
        }
    }
}

macro_rules! distr_duration {
    ($fnn:ident, $distr:expr) => {
        #[bench]
        fn $fnn(b: &mut Bencher) {
            let mut rng = SmallRng::from_entropy();
            let distr = $distr;

            b.iter(|| {
                let mut accum = Duration::new(0, 0);
                for _ in 0..::RAND_BENCH_N {
                    let x: Duration = distr.sample(&mut rng);
                    accum = accum.checked_add(x).unwrap_or(Duration::new(u64::max_value(), 999_999_999));
                }
                accum
            });
            b.bytes = size_of::<Duration>() as u64 * ::RAND_BENCH_N;
        }
    }
}

macro_rules! distr {
    ($fnn:ident, $ty:ty, $distr:expr) => {
        #[bench]
        fn $fnn(b: &mut Bencher) {
            let mut rng = SmallRng::from_entropy();
            let distr = $distr;

            b.iter(|| {
                let mut accum = 0u32;
                for _ in 0..::RAND_BENCH_N {
                    let x: $ty = distr.sample(&mut rng);
                    accum = accum.wrapping_add(x as u32);
                }
                accum
            });
            b.bytes = size_of::<$ty>() as u64 * ::RAND_BENCH_N;
        }
    }
}

macro_rules! distr_arr {
    ($fnn:ident, $ty:ty, $distr:expr) => {
        #[bench]
        fn $fnn(b: &mut Bencher) {
            let mut rng = SmallRng::from_entropy();
            let distr = $distr;

            b.iter(|| {
                let mut accum = 0u32;
                for _ in 0..::RAND_BENCH_N {
                    let x: $ty = distr.sample(&mut rng);
                    accum = accum.wrapping_add(x[0] as u32);
                }
                accum
            });
            b.bytes = size_of::<$ty>() as u64 * ::RAND_BENCH_N;
        }
    }
}

// uniform
distr_int!(distr_uniform_i8, i8, Uniform::new(20i8, 100));
distr_int!(distr_uniform_i16, i16, Uniform::new(-500i16, 2000));
distr_int!(distr_uniform_i32, i32, Uniform::new(-200_000_000i32, 800_000_000));
distr_int!(distr_uniform_i64, i64, Uniform::new(3i64, 123_456_789_123));
distr_int!(distr_uniform_i128, i128, Uniform::new(-123_456_789_123i128, 123_456_789_123_456_789));

distr_float!(distr_uniform_f32, f32, Uniform::new(2.26f32, 2.319));
distr_float!(distr_uniform_f64, f64, Uniform::new(2.26f64, 2.319));

const LARGE_SEC: u64 = u64::max_value() / 1000;

distr_duration!(distr_uniform_duration_largest,
    Uniform::new_inclusive(Duration::new(0, 0), Duration::new(u64::max_value(), 999_999_999))
);
distr_duration!(distr_uniform_duration_large,
    Uniform::new(Duration::new(0, 0), Duration::new(LARGE_SEC, 1_000_000_000 / 2))
);
distr_duration!(distr_uniform_duration_one,
    Uniform::new(Duration::new(0, 0), Duration::new(1, 0))
);
distr_duration!(distr_uniform_duration_variety,
    Uniform::new(Duration::new(10000, 423423), Duration::new(200000, 6969954))
);
distr_duration!(distr_uniform_duration_edge,
    Uniform::new_inclusive(Duration::new(LARGE_SEC, 999_999_999), Duration::new(LARGE_SEC + 1, 1))
);


// standard
distr_int!(distr_standard_i8, i8, Standard);
distr_int!(distr_standard_i16, i16, Standard);
distr_int!(distr_standard_i32, i32, Standard);
distr_int!(distr_standard_i64, i64, Standard);
distr_int!(distr_standard_i128, i128, Standard);

distr!(distr_standard_bool, bool, Standard);
distr!(distr_standard_alphanumeric, char, Alphanumeric);
distr!(distr_standard_codepoint, char, Standard);

distr_float!(distr_standard_f32, f32, Standard);
distr_float!(distr_standard_f64, f64, Standard);
distr_float!(distr_open01_f32, f32, Open01);
distr_float!(distr_open01_f64, f64, Open01);
distr_float!(distr_openclosed01_f32, f32, OpenClosed01);
distr_float!(distr_openclosed01_f64, f64, OpenClosed01);

// distributions
distr_float!(distr_exp, f64, Exp::new(1.23 * 4.56));
distr_float!(distr_normal, f64, Normal::new(-1.23, 4.56));
distr_float!(distr_log_normal, f64, LogNormal::new(-1.23, 4.56));
distr_float!(distr_gamma_large_shape, f64, Gamma::new(10., 1.0));
distr_float!(distr_gamma_small_shape, f64, Gamma::new(0.1, 1.0));
distr_float!(distr_cauchy, f64, Cauchy::new(4.2, 6.9));
distr_int!(distr_binomial, u64, Binomial::new(20, 0.7));
distr_int!(distr_poisson, u64, Poisson::new(4.0));
distr!(distr_bernoulli, bool, Bernoulli::new(0.18));
distr_arr!(distr_circle, [f64; 2], UnitCircle::new());
distr_arr!(distr_sphere_surface, [f64; 3], UnitSphereSurface::new());

// Weighted
distr_int!(distr_weighted_i8, usize, WeightedIndex::new(&[1i8, 2, 3, 4, 12, 0, 2, 1]).unwrap());
distr_int!(distr_weighted_u32, usize, WeightedIndex::new(&[1u32, 2, 3, 4, 12, 0, 2, 1]).unwrap());
distr_int!(distr_weighted_f64, usize, WeightedIndex::new(&[1.0f64, 0.001, 1.0/3.0, 4.01, 0.0, 3.3, 22.0, 0.001]).unwrap());
distr_int!(distr_weighted_large_set, usize, WeightedIndex::new((0..10000).rev().chain(1..10001)).unwrap());

// construct and sample from a range
macro_rules! gen_range_int {
    ($fnn:ident, $ty:ident, $low:expr, $high:expr) => {
        #[bench]
        fn $fnn(b: &mut Bencher) {
            let mut rng = SmallRng::from_entropy();

            b.iter(|| {
                let mut high = $high;
                let mut accum: $ty = 0;
                for _ in 0..::RAND_BENCH_N {
                    accum = accum.wrapping_add(rng.gen_range($low, high));
                    // force recalculation of range each time
                    high = high.wrapping_add(1) & std::$ty::MAX;
                }
                accum
            });
            b.bytes = size_of::<$ty>() as u64 * ::RAND_BENCH_N;
        }
    }
}

gen_range_int!(gen_range_i8, i8, -20i8, 100);
gen_range_int!(gen_range_i16, i16, -500i16, 2000);
gen_range_int!(gen_range_i32, i32, -200_000_000i32, 800_000_000);
gen_range_int!(gen_range_i64, i64, 3i64, 123_456_789_123);
gen_range_int!(gen_range_i128, i128, -12345678901234i128, 123_456_789_123_456_789);

// construct and sample from a floating-point range
macro_rules! gen_range_float {
    ($fnn:ident, $ty:ident, $low:expr, $high:expr) => {
        #[bench]
        fn $fnn(b: &mut Bencher) {
            let mut rng = SmallRng::from_entropy();

            b.iter(|| {
                let mut high = $high;
                let mut low = $low;
                let mut accum: $ty = 0.0;
                for _ in 0..::RAND_BENCH_N {
                    accum += rng.gen_range(low, high);
                    // force recalculation of range each time
                    low += 0.9;
                    high += 1.1;
                }
                accum
            });
            b.bytes = size_of::<$ty>() as u64 * ::RAND_BENCH_N;
        }
    }
}

gen_range_float!(gen_range_f32, f32, -20000.0f32, 100000.0);
gen_range_float!(gen_range_f64, f64, 123.456f64, 7890.12);

#[bench]
fn dist_iter(b: &mut Bencher) {
    let mut rng = SmallRng::from_entropy();
    let distr = Normal::new(-2.71828, 3.14159);
    let mut iter = distr.sample_iter(&mut rng);

    b.iter(|| {
        let mut accum = 0.0;
        for _ in 0..::RAND_BENCH_N {
            accum += iter.next().unwrap();
        }
        accum
    });
    b.bytes = size_of::<f64>() as u64 * ::RAND_BENCH_N;
}
