// SPDX-License-Identifier: MPL-2.0

use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};
#[cfg(feature = "experimental")]
use criterion::{BatchSize, Throughput};
#[cfg(feature = "experimental")]
use fixed::types::{I1F15, I1F31};
#[cfg(feature = "experimental")]
use fixed_macro::fixed;
#[cfg(feature = "experimental")]
use num_bigint::BigUint;
#[cfg(feature = "experimental")]
use num_rational::Ratio;
#[cfg(feature = "experimental")]
use num_traits::ToPrimitive;
#[cfg(feature = "experimental")]
use prio::dp::distributions::DiscreteGaussian;
#[cfg(feature = "prio2")]
use prio::vdaf::prio2::Prio2;
use prio::{
    benchmarked::*,
    field::{random_vector, Field128 as F, FieldElement},
    flp::gadgets::Mul,
    vdaf::{prio3::Prio3, Aggregator, Client},
};
#[cfg(feature = "experimental")]
use prio::{
    field::{Field255, Field64},
    flp::types::fixedpoint_l2::FixedPointBoundedL2VecSum,
    idpf::{Idpf, IdpfInput, RingBufferCache},
    vdaf::poplar1::{Poplar1, Poplar1AggregationParam, Poplar1IdpfValue},
};
#[cfg(feature = "experimental")]
use rand::prelude::*;
#[cfg(feature = "experimental")]
use std::iter;
use std::time::Duration;
#[cfg(feature = "experimental")]
use zipf::ZipfDistribution;

/// Seed for generation of random benchmark inputs.
///
/// A fixed RNG seed is used to generate inputs in order to minimize run-to-run variability. The
/// seed value may be freely changed to get a different set of inputs.
#[cfg(feature = "experimental")]
const RNG_SEED: u64 = 0;

/// Speed test for generating a seed and deriving a pseudorandom sequence of field elements.
fn prng(c: &mut Criterion) {
    let mut group = c.benchmark_group("rand");
    let test_sizes = [16, 256, 1024, 4096];
    for size in test_sizes {
        group.bench_with_input(BenchmarkId::from_parameter(size), &size, |b, size| {
            b.iter(|| random_vector::<F>(*size))
        });
    }
    group.finish();
}

/// Speed test for generating samples from the discrete gaussian distribution using different
/// standard deviations.
#[cfg(feature = "experimental")]
pub fn dp_noise(c: &mut Criterion) {
    let mut group = c.benchmark_group("dp_noise");
    let mut rng = StdRng::seed_from_u64(RNG_SEED);

    let test_stds = [
        Ratio::<BigUint>::from_integer(BigUint::from(u128::MAX)).pow(2),
        Ratio::<BigUint>::from_integer(BigUint::from(u64::MAX)),
        Ratio::<BigUint>::from_integer(BigUint::from(u32::MAX)),
        Ratio::<BigUint>::from_integer(BigUint::from(5u8)),
        Ratio::<BigUint>::new(BigUint::from(10000u32), BigUint::from(23u32)),
    ];
    for std in test_stds {
        let sampler = DiscreteGaussian::new(std.clone()).unwrap();
        group.bench_function(
            BenchmarkId::new("discrete_gaussian", std.to_f64().unwrap_or(f64::INFINITY)),
            |b| b.iter(|| sampler.sample(&mut rng)),
        );
    }
    group.finish();
}

/// The asymptotic cost of polynomial multiplication is `O(n log n)` using FFT and `O(n^2)` using
/// the naive method. This benchmark demonstrates that the latter has better concrete performance
/// for small polynomials. The result is used to pick the `FFT_THRESHOLD` constant in
/// `src/flp/gadgets.rs`.
fn poly_mul(c: &mut Criterion) {
    let test_sizes = [1_usize, 30, 60, 90, 120, 150];

    let mut group = c.benchmark_group("poly_mul");
    for size in test_sizes {
        group.bench_with_input(BenchmarkId::new("fft", size), &size, |b, size| {
            let m = (size + 1).next_power_of_two();
            let mut g: Mul<F> = Mul::new(*size);
            let mut outp = vec![F::zero(); 2 * m];
            let inp = vec![random_vector(m).unwrap(); 2];

            b.iter(|| {
                benchmarked_gadget_mul_call_poly_fft(&mut g, &mut outp, &inp).unwrap();
            })
        });

        group.bench_with_input(BenchmarkId::new("direct", size), &size, |b, size| {
            let m = (size + 1).next_power_of_two();
            let mut g: Mul<F> = Mul::new(*size);
            let mut outp = vec![F::zero(); 2 * m];
            let inp = vec![random_vector(m).unwrap(); 2];

            b.iter(|| {
                benchmarked_gadget_mul_call_poly_direct(&mut g, &mut outp, &inp).unwrap();
            })
        });
    }
    group.finish();
}

/// Benchmark prio2.
#[cfg(feature = "prio2")]
fn prio2(c: &mut Criterion) {
    let mut group = c.benchmark_group("prio2_shard");
    for input_length in [10, 100, 1_000] {
        group.bench_with_input(
            BenchmarkId::from_parameter(input_length),
            &input_length,
            |b, input_length| {
                let vdaf = Prio2::new(*input_length).unwrap();
                let measurement = (0..u32::try_from(*input_length).unwrap())
                    .map(|i| i & 1)
                    .collect::<Vec<_>>();
                let nonce = black_box([0u8; 16]);
                b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
            },
        );
    }
    group.finish();

    let mut group = c.benchmark_group("prio2_prepare_init");
    for input_length in [10, 100, 1_000] {
        group.bench_with_input(
            BenchmarkId::from_parameter(input_length),
            &input_length,
            |b, input_length| {
                let vdaf = Prio2::new(*input_length).unwrap();
                let measurement = (0..u32::try_from(*input_length).unwrap())
                    .map(|i| i & 1)
                    .collect::<Vec<_>>();
                let nonce = black_box([0u8; 16]);
                let verify_key = black_box([0u8; 32]);
                let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
                b.iter(|| {
                    vdaf.prepare_init(&verify_key, 0, &(), &nonce, &public_share, &input_shares[0])
                        .unwrap();
                });
            },
        );
    }
    group.finish();
}

/// Benchmark prio3.
fn prio3(c: &mut Criterion) {
    let num_shares = 2;

    c.bench_function("prio3count_shard", |b| {
        let vdaf = Prio3::new_count(num_shares).unwrap();
        let measurement = black_box(1);
        let nonce = black_box([0u8; 16]);
        b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
    });

    c.bench_function("prio3count_prepare_init", |b| {
        let vdaf = Prio3::new_count(num_shares).unwrap();
        let measurement = black_box(1);
        let nonce = black_box([0u8; 16]);
        let verify_key = black_box([0u8; 16]);
        let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
        b.iter(|| {
            vdaf.prepare_init(&verify_key, 0, &(), &nonce, &public_share, &input_shares[0])
                .unwrap()
        });
    });

    let mut group = c.benchmark_group("prio3sum_shard");
    for bits in [8, 32] {
        group.bench_with_input(BenchmarkId::from_parameter(bits), &bits, |b, bits| {
            let vdaf = Prio3::new_sum(num_shares, *bits).unwrap();
            let measurement = (1 << bits) - 1;
            let nonce = black_box([0u8; 16]);
            b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
        });
    }
    group.finish();

    let mut group = c.benchmark_group("prio3sum_prepare_init");
    for bits in [8, 32] {
        group.bench_with_input(BenchmarkId::from_parameter(bits), &bits, |b, bits| {
            let vdaf = Prio3::new_sum(num_shares, *bits).unwrap();
            let measurement = (1 << bits) - 1;
            let nonce = black_box([0u8; 16]);
            let verify_key = black_box([0u8; 16]);
            let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
            b.iter(|| {
                vdaf.prepare_init(&verify_key, 0, &(), &nonce, &public_share, &input_shares[0])
                    .unwrap()
            });
        });
    }
    group.finish();

    let mut group = c.benchmark_group("prio3sumvec_shard");
    for (input_length, chunk_length) in [(10, 3), (100, 10), (1_000, 31)] {
        group.bench_with_input(
            BenchmarkId::new("serial", input_length),
            &(input_length, chunk_length),
            |b, (input_length, chunk_length)| {
                let vdaf = Prio3::new_sum_vec(num_shares, 1, *input_length, *chunk_length).unwrap();
                let measurement = (0..u128::try_from(*input_length).unwrap())
                    .map(|i| i & 1)
                    .collect::<Vec<_>>();
                let nonce = black_box([0u8; 16]);
                b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
            },
        );
    }

    #[cfg(feature = "multithreaded")]
    {
        for (input_length, chunk_length) in [(10, 3), (100, 10), (1_000, 31)] {
            group.bench_with_input(
                BenchmarkId::new("parallel", input_length),
                &(input_length, chunk_length),
                |b, (input_length, chunk_length)| {
                    let vdaf = Prio3::new_sum_vec_multithreaded(
                        num_shares,
                        1,
                        *input_length,
                        *chunk_length,
                    )
                    .unwrap();
                    let measurement = (0..u128::try_from(*input_length).unwrap())
                        .map(|i| i & 1)
                        .collect::<Vec<_>>();
                    let nonce = black_box([0u8; 16]);
                    b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
                },
            );
        }
    }
    group.finish();

    let mut group = c.benchmark_group("prio3sumvec_prepare_init");
    for (input_length, chunk_length) in [(10, 3), (100, 10), (1_000, 31)] {
        group.bench_with_input(
            BenchmarkId::new("serial", input_length),
            &(input_length, chunk_length),
            |b, (input_length, chunk_length)| {
                let vdaf = Prio3::new_sum_vec(num_shares, 1, *input_length, *chunk_length).unwrap();
                let measurement = (0..u128::try_from(*input_length).unwrap())
                    .map(|i| i & 1)
                    .collect::<Vec<_>>();
                let nonce = black_box([0u8; 16]);
                let verify_key = black_box([0u8; 16]);
                let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
                b.iter(|| {
                    vdaf.prepare_init(&verify_key, 0, &(), &nonce, &public_share, &input_shares[0])
                        .unwrap()
                });
            },
        );
    }

    #[cfg(feature = "multithreaded")]
    {
        for (input_length, chunk_length) in [(10, 3), (100, 10), (1_000, 31)] {
            group.bench_with_input(
                BenchmarkId::new("parallel", input_length),
                &(input_length, chunk_length),
                |b, (input_length, chunk_length)| {
                    let vdaf = Prio3::new_sum_vec_multithreaded(
                        num_shares,
                        1,
                        *input_length,
                        *chunk_length,
                    )
                    .unwrap();
                    let measurement = (0..u128::try_from(*input_length).unwrap())
                        .map(|i| i & 1)
                        .collect::<Vec<_>>();
                    let nonce = black_box([0u8; 16]);
                    let verify_key = black_box([0u8; 16]);
                    let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
                    b.iter(|| {
                        vdaf.prepare_init(
                            &verify_key,
                            0,
                            &(),
                            &nonce,
                            &public_share,
                            &input_shares[0],
                        )
                        .unwrap()
                    });
                },
            );
        }
    }
    group.finish();

    let mut group = c.benchmark_group("prio3histogram_shard");
    for (input_length, chunk_length) in [
        (10, 3),
        (100, 10),
        (1_000, 31),
        (10_000, 100),
        (100_000, 316),
    ] {
        if input_length >= 100_000 {
            group.measurement_time(Duration::from_secs(15));
        }
        group.bench_with_input(
            BenchmarkId::new("serial", input_length),
            &(input_length, chunk_length),
            |b, (input_length, chunk_length)| {
                let vdaf = Prio3::new_histogram(num_shares, *input_length, *chunk_length).unwrap();
                let measurement = black_box(0);
                let nonce = black_box([0u8; 16]);
                b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
            },
        );
    }

    #[cfg(feature = "multithreaded")]
    {
        for (input_length, chunk_length) in [
            (10, 3),
            (100, 10),
            (1_000, 31),
            (10_000, 100),
            (100_000, 316),
        ] {
            if input_length >= 100_000 {
                group.measurement_time(Duration::from_secs(15));
            }
            group.bench_with_input(
                BenchmarkId::new("parallel", input_length),
                &(input_length, chunk_length),
                |b, (input_length, chunk_length)| {
                    let vdaf = Prio3::new_histogram_multithreaded(
                        num_shares,
                        *input_length,
                        *chunk_length,
                    )
                    .unwrap();
                    let measurement = black_box(0);
                    let nonce = black_box([0u8; 16]);
                    b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
                },
            );
        }
    }
    group.finish();

    let mut group = c.benchmark_group("prio3histogram_prepare_init");
    for (input_length, chunk_length) in [
        (10, 3),
        (100, 10),
        (1_000, 31),
        (10_000, 100),
        (100_000, 316),
    ] {
        if input_length >= 100_000 {
            group.measurement_time(Duration::from_secs(15));
        }
        group.bench_with_input(
            BenchmarkId::new("serial", input_length),
            &(input_length, chunk_length),
            |b, (input_length, chunk_length)| {
                let vdaf = Prio3::new_histogram(num_shares, *input_length, *chunk_length).unwrap();
                let measurement = black_box(0);
                let nonce = black_box([0u8; 16]);
                let verify_key = black_box([0u8; 16]);
                let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
                b.iter(|| {
                    vdaf.prepare_init(&verify_key, 0, &(), &nonce, &public_share, &input_shares[0])
                        .unwrap()
                });
            },
        );
    }

    #[cfg(feature = "multithreaded")]
    {
        for (input_length, chunk_length) in [
            (10, 3),
            (100, 10),
            (1_000, 31),
            (10_000, 100),
            (100_000, 316),
        ] {
            if input_length >= 100_000 {
                group.measurement_time(Duration::from_secs(15));
            }
            group.bench_with_input(
                BenchmarkId::new("parallel", input_length),
                &(input_length, chunk_length),
                |b, (input_length, chunk_length)| {
                    let vdaf = Prio3::new_histogram_multithreaded(
                        num_shares,
                        *input_length,
                        *chunk_length,
                    )
                    .unwrap();
                    let measurement = black_box(0);
                    let nonce = black_box([0u8; 16]);
                    let verify_key = black_box([0u8; 16]);
                    let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
                    b.iter(|| {
                        vdaf.prepare_init(
                            &verify_key,
                            0,
                            &(),
                            &nonce,
                            &public_share,
                            &input_shares[0],
                        )
                        .unwrap()
                    });
                },
            );
        }
    }
    group.finish();

    #[cfg(feature = "experimental")]
    {
        let mut group = c.benchmark_group("prio3fixedpointboundedl2vecsum_i1f15_shard");
        for dimension in [10, 100, 1_000] {
            group.bench_with_input(
                BenchmarkId::new("serial", dimension),
                &dimension,
                |b, dimension| {
                    let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F15, _, _>, _, 16> =
                        Prio3::new_fixedpoint_boundedl2_vec_sum(num_shares, *dimension).unwrap();
                    let mut measurement = vec![fixed!(0: I1F15); *dimension];
                    measurement[0] = fixed!(0.5: I1F15);
                    let nonce = black_box([0u8; 16]);
                    b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
                },
            );
        }

        #[cfg(feature = "multithreaded")]
        {
            for dimension in [10, 100, 1_000] {
                group.bench_with_input(
                    BenchmarkId::new("parallel", dimension),
                    &dimension,
                    |b, dimension| {
                        let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F15, _, _>, _, 16> =
                            Prio3::new_fixedpoint_boundedl2_vec_sum_multithreaded(
                                num_shares, *dimension,
                            )
                            .unwrap();
                        let mut measurement = vec![fixed!(0: I1F15); *dimension];
                        measurement[0] = fixed!(0.5: I1F15);
                        let nonce = black_box([0u8; 16]);
                        b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
                    },
                );
            }
        }
        group.finish();

        let mut group = c.benchmark_group("prio3fixedpointboundedl2vecsum_i1f15_prepare_init");
        for dimension in [10, 100, 1_000] {
            group.bench_with_input(
                BenchmarkId::new("series", dimension),
                &dimension,
                |b, dimension| {
                    let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F15, _, _>, _, 16> =
                        Prio3::new_fixedpoint_boundedl2_vec_sum(num_shares, *dimension).unwrap();
                    let mut measurement = vec![fixed!(0: I1F15); *dimension];
                    measurement[0] = fixed!(0.5: I1F15);
                    let nonce = black_box([0u8; 16]);
                    let verify_key = black_box([0u8; 16]);
                    let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
                    b.iter(|| {
                        vdaf.prepare_init(
                            &verify_key,
                            0,
                            &(),
                            &nonce,
                            &public_share,
                            &input_shares[0],
                        )
                        .unwrap()
                    });
                },
            );
        }

        #[cfg(feature = "multithreaded")]
        {
            for dimension in [10, 100, 1_000] {
                group.bench_with_input(
                    BenchmarkId::new("parallel", dimension),
                    &dimension,
                    |b, dimension| {
                        let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F15, _, _>, _, 16> =
                            Prio3::new_fixedpoint_boundedl2_vec_sum_multithreaded(
                                num_shares, *dimension,
                            )
                            .unwrap();
                        let mut measurement = vec![fixed!(0: I1F15); *dimension];
                        measurement[0] = fixed!(0.5: I1F15);
                        let nonce = black_box([0u8; 16]);
                        let verify_key = black_box([0u8; 16]);
                        let (public_share, input_shares) =
                            vdaf.shard(&measurement, &nonce).unwrap();
                        b.iter(|| {
                            vdaf.prepare_init(
                                &verify_key,
                                0,
                                &(),
                                &nonce,
                                &public_share,
                                &input_shares[0],
                            )
                            .unwrap()
                        });
                    },
                );
            }
        }
        group.finish();

        let mut group = c.benchmark_group("prio3fixedpointboundedl2vecsum_i1f31_shard");
        for dimension in [10, 100, 1_000] {
            group.bench_with_input(
                BenchmarkId::new("serial", dimension),
                &dimension,
                |b, dimension| {
                    let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F31, _, _>, _, 16> =
                        Prio3::new_fixedpoint_boundedl2_vec_sum(num_shares, *dimension).unwrap();
                    let mut measurement = vec![fixed!(0: I1F31); *dimension];
                    measurement[0] = fixed!(0.5: I1F31);
                    let nonce = black_box([0u8; 16]);
                    b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
                },
            );
        }

        #[cfg(feature = "multithreaded")]
        {
            for dimension in [10, 100, 1_000] {
                group.bench_with_input(
                    BenchmarkId::new("parallel", dimension),
                    &dimension,
                    |b, dimension| {
                        let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F31, _, _>, _, 16> =
                            Prio3::new_fixedpoint_boundedl2_vec_sum_multithreaded(
                                num_shares, *dimension,
                            )
                            .unwrap();
                        let mut measurement = vec![fixed!(0: I1F31); *dimension];
                        measurement[0] = fixed!(0.5: I1F31);
                        let nonce = black_box([0u8; 16]);
                        b.iter(|| vdaf.shard(&measurement, &nonce).unwrap());
                    },
                );
            }
        }
        group.finish();

        let mut group = c.benchmark_group("prio3fixedpointboundedl2vecsum_i1f31_prepare_init");
        for dimension in [10, 100, 1_000] {
            group.bench_with_input(
                BenchmarkId::new("series", dimension),
                &dimension,
                |b, dimension| {
                    let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F31, _, _>, _, 16> =
                        Prio3::new_fixedpoint_boundedl2_vec_sum(num_shares, *dimension).unwrap();
                    let mut measurement = vec![fixed!(0: I1F31); *dimension];
                    measurement[0] = fixed!(0.5: I1F31);
                    let nonce = black_box([0u8; 16]);
                    let verify_key = black_box([0u8; 16]);
                    let (public_share, input_shares) = vdaf.shard(&measurement, &nonce).unwrap();
                    b.iter(|| {
                        vdaf.prepare_init(
                            &verify_key,
                            0,
                            &(),
                            &nonce,
                            &public_share,
                            &input_shares[0],
                        )
                        .unwrap()
                    });
                },
            );
        }

        #[cfg(feature = "multithreaded")]
        {
            for dimension in [10, 100, 1_000] {
                group.bench_with_input(
                    BenchmarkId::new("parallel", dimension),
                    &dimension,
                    |b, dimension| {
                        let vdaf: Prio3<FixedPointBoundedL2VecSum<I1F31, _, _>, _, 16> =
                            Prio3::new_fixedpoint_boundedl2_vec_sum_multithreaded(
                                num_shares, *dimension,
                            )
                            .unwrap();
                        let mut measurement = vec![fixed!(0: I1F31); *dimension];
                        measurement[0] = fixed!(0.5: I1F31);
                        let nonce = black_box([0u8; 16]);
                        let verify_key = black_box([0u8; 16]);
                        let (public_share, input_shares) =
                            vdaf.shard(&measurement, &nonce).unwrap();
                        b.iter(|| {
                            vdaf.prepare_init(
                                &verify_key,
                                0,
                                &(),
                                &nonce,
                                &public_share,
                                &input_shares[0],
                            )
                            .unwrap()
                        });
                    },
                );
            }
        }
        group.finish();
    }
}

/// Benchmark IdpfPoplar performance.
#[cfg(feature = "experimental")]
fn idpf(c: &mut Criterion) {
    let test_sizes = [8usize, 8 * 16, 8 * 256];

    let mut group = c.benchmark_group("idpf_gen");
    for size in test_sizes.iter() {
        group.throughput(Throughput::Bytes(*size as u64 / 8));
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let bits = iter::repeat_with(random).take(size).collect::<Vec<bool>>();
            let input = IdpfInput::from_bools(&bits);

            let inner_values = random_vector::<Field64>(size - 1)
                .unwrap()
                .into_iter()
                .map(|random_element| Poplar1IdpfValue::new([Field64::one(), random_element]))
                .collect::<Vec<_>>();
            let leaf_value = Poplar1IdpfValue::new([Field255::one(), random_vector(1).unwrap()[0]]);

            let idpf = Idpf::new((), ());
            b.iter(|| {
                idpf.gen(&input, inner_values.clone(), leaf_value, &[0; 16])
                    .unwrap();
            });
        });
    }
    group.finish();

    let mut group = c.benchmark_group("idpf_eval");
    for size in test_sizes.iter() {
        group.throughput(Throughput::Bytes(*size as u64 / 8));
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let bits = iter::repeat_with(random).take(size).collect::<Vec<bool>>();
            let input = IdpfInput::from_bools(&bits);

            let inner_values = random_vector::<Field64>(size - 1)
                .unwrap()
                .into_iter()
                .map(|random_element| Poplar1IdpfValue::new([Field64::one(), random_element]))
                .collect::<Vec<_>>();
            let leaf_value = Poplar1IdpfValue::new([Field255::one(), random_vector(1).unwrap()[0]]);

            let idpf = Idpf::new((), ());
            let (public_share, keys) = idpf
                .gen(&input, inner_values, leaf_value, &[0; 16])
                .unwrap();

            b.iter(|| {
                // This is an aggressively small cache, to minimize its impact on the benchmark.
                // In this synthetic benchmark, we are only checking one candidate prefix per level
                // (typically there are many candidate prefixes per level) so the cache hit rate
                // will be unaffected.
                let mut cache = RingBufferCache::new(1);

                for prefix_length in 1..=size {
                    let prefix = input[..prefix_length].to_owned().into();
                    idpf.eval(0, &public_share, &keys[0], &prefix, &[0; 16], &mut cache)
                        .unwrap();
                }
            });
        });
    }
    group.finish();
}

/// Benchmark Poplar1.
#[cfg(feature = "experimental")]
fn poplar1(c: &mut Criterion) {
    let test_sizes = [16_usize, 128, 256];

    let mut group = c.benchmark_group("poplar1_shard");
    for size in test_sizes.iter() {
        group.throughput(Throughput::Bytes(*size as u64 / 8));
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let vdaf = Poplar1::new_shake128(size);
            let mut rng = StdRng::seed_from_u64(RNG_SEED);
            let nonce = rng.gen::<[u8; 16]>();

            b.iter_batched(
                || {
                    let bits = iter::repeat_with(|| rng.gen())
                        .take(size)
                        .collect::<Vec<bool>>();
                    IdpfInput::from_bools(&bits)
                },
                |measurement| {
                    vdaf.shard(&measurement, &nonce).unwrap();
                },
                BatchSize::SmallInput,
            );
        });
    }
    group.finish();

    let mut group = c.benchmark_group("poplar1_prepare_init");
    for size in test_sizes.iter() {
        group.measurement_time(Duration::from_secs(30)); // slower benchmark
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, &size| {
            let vdaf = Poplar1::new_shake128(size);
            let mut rng = StdRng::seed_from_u64(RNG_SEED);

            b.iter_batched(
                || {
                    let verify_key: [u8; 16] = rng.gen();
                    let nonce: [u8; 16] = rng.gen();

                    // Parameters are chosen to match Chris Wood's experimental setup:
                    // https://github.com/chris-wood/heavy-hitter-comparison
                    let (measurements, prefix_tree) = poplar1_generate_zipf_distributed_batch(
                        &mut rng, // rng
                        size,     // bits
                        10,       // threshold
                        1000,     // number of measurements
                        128,      // Zipf support
                        1.03,     // Zipf exponent
                    );

                    // We are benchmarking preparation of a single report. For this test, it doesn't matter
                    // which measurement we generate a report for, so pick the first measurement
                    // arbitrarily.
                    let (public_share, input_shares) =
                        vdaf.shard(&measurements[0], &nonce).unwrap();

                    // For the aggregation paramter, we use the candidate prefixes from the prefix tree
                    // for the sampled measurements. Run preparation for the last step, which ought to
                    // represent the worst-case performance.
                    let agg_param =
                        Poplar1AggregationParam::try_from_prefixes(prefix_tree[size - 1].clone())
                            .unwrap();

                    (
                        verify_key,
                        nonce,
                        agg_param,
                        public_share,
                        input_shares.into_iter().next().unwrap(),
                    )
                },
                |(verify_key, nonce, agg_param, public_share, input_share)| {
                    vdaf.prepare_init(
                        &verify_key,
                        0,
                        &agg_param,
                        &nonce,
                        &public_share,
                        &input_share,
                    )
                    .unwrap();
                },
                BatchSize::SmallInput,
            );
        });
    }
    group.finish();
}

/// Generate a set of Poplar1 measurements with the given bit length `bits`. They are sampled
/// according to the Zipf distribution with parameters `zipf_support` and `zipf_exponent`. Return
/// the measurements, along with the prefix tree for the desired threshold.
///
/// The prefix tree consists of a sequence of candidate prefixes for each level. For a given level,
/// the candidate prefixes are computed from the hit counts of the prefixes at the previous level:
/// For any prefix `p` whose hit count is at least the desired threshold, add `p || 0` and `p || 1`
/// to the list.
#[cfg(feature = "experimental")]
fn poplar1_generate_zipf_distributed_batch(
    rng: &mut impl Rng,
    bits: usize,
    threshold: usize,
    measurement_count: usize,
    zipf_support: usize,
    zipf_exponent: f64,
) -> (Vec<IdpfInput>, Vec<Vec<IdpfInput>>) {
    // Generate random inputs.
    let mut inputs = Vec::with_capacity(zipf_support);
    for _ in 0..zipf_support {
        let bools: Vec<bool> = (0..bits).map(|_| rng.gen()).collect();
        inputs.push(IdpfInput::from_bools(&bools));
    }

    // Sample a number of inputs according to the Zipf distribution.
    let mut samples = Vec::with_capacity(measurement_count);
    let zipf = ZipfDistribution::new(zipf_support, zipf_exponent).unwrap();
    for _ in 0..measurement_count {
        samples.push(inputs[zipf.sample(rng) - 1].clone());
    }

    // Compute the prefix tree for the desired threshold.
    let mut prefix_tree = Vec::with_capacity(bits);
    prefix_tree.push(vec![
        IdpfInput::from_bools(&[false]),
        IdpfInput::from_bools(&[true]),
    ]);

    for level in 0..bits - 1 {
        // Compute the hit count of each prefix from the previous level.
        let mut hit_counts = vec![0; prefix_tree[level].len()];
        for (hit_count, prefix) in hit_counts.iter_mut().zip(prefix_tree[level].iter()) {
            for sample in samples.iter() {
                let mut is_prefix = true;
                for j in 0..prefix.len() {
                    if prefix[j] != sample[j] {
                        is_prefix = false;
                        break;
                    }
                }
                if is_prefix {
                    *hit_count += 1;
                }
            }
        }

        // Compute the next set of candidate prefixes.
        let mut next_prefixes = Vec::new();
        for (hit_count, prefix) in hit_counts.iter().zip(prefix_tree[level].iter()) {
            if *hit_count >= threshold {
                next_prefixes.push(prefix.clone_with_suffix(&[false]));
                next_prefixes.push(prefix.clone_with_suffix(&[true]));
            }
        }
        prefix_tree.push(next_prefixes);
    }

    (samples, prefix_tree)
}

#[cfg(all(feature = "prio2", feature = "experimental"))]
criterion_group!(benches, poplar1, prio3, prio2, poly_mul, prng, idpf, dp_noise);
#[cfg(all(not(feature = "prio2"), feature = "experimental"))]
criterion_group!(benches, poplar1, prio3, poly_mul, prng, idpf, dp_noise);
#[cfg(all(feature = "prio2", not(feature = "experimental")))]
criterion_group!(benches, prio3, prio2, prng, poly_mul);
#[cfg(all(not(feature = "prio2"), not(feature = "experimental")))]
criterion_group!(benches, prio3, prng, poly_mul);

criterion_main!(benches);
