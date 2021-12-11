use criterion::*;

use rand::distributions::Standard;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};

use phf_generator::generate_hash;

fn gen_vec(len: usize) -> Vec<u64> {
    SmallRng::seed_from_u64(0xAAAAAAAAAAAAAAAA).sample_iter(Standard).take(len).collect()
}

fn bench_hash(b: &mut Bencher, len: &usize) {
    let vec = gen_vec(*len);
    b.iter(|| generate_hash(&vec))
}

fn gen_hash_small(c: &mut Criterion) {
    let sizes = vec![0, 1, 2, 5, 10, 25, 50, 75];
    c.bench_function_over_inputs("gen_hash_small", bench_hash, sizes);
}

fn gen_hash_med(c: &mut Criterion) {
    let sizes = vec![100, 250, 500, 1000, 2500, 5000, 7500];
    c.bench_function_over_inputs("gen_hash_medium", bench_hash, sizes);
}

fn gen_hash_large(c: &mut Criterion) {
    let sizes = vec![10_000, 25_000, 50_000, 75_000];
    c.bench_function_over_inputs("gen_hash_large", bench_hash, sizes);
}

fn gen_hash_xlarge(c: &mut Criterion) {
    let sizes = vec![100_000, 250_000, 500_000, 750_000, 1_000_000];
    c.bench_function_over_inputs("gen_hash_xlarge", bench_hash, sizes);
}

criterion_group!(benches, gen_hash_small, gen_hash_med, gen_hash_large, gen_hash_xlarge);

#[cfg(not(feature = "rayon"))]
criterion_main!(benches);

#[cfg(feature = "rayon")]
criterion_main!(benches, rayon::benches);

#[cfg(feature = "rayon")]
mod rayon {
    use criterion::*;

    use phf_generator::generate_hash_rayon;

    use super::gen_vec;

    fn bench_hash(b: &mut Bencher, len: &usize) {
        let vec = gen_vec(*len);
        b.iter(|| generate_hash_rayon(&vec))
    }

    fn gen_hash_small(c: &mut Criterion) {
        let sizes = vec![0, 1, 2, 5, 10, 25, 50, 75];
        c.bench_function_over_inputs("gen_hash_small_rayon", bench_hash, sizes);
    }

    fn gen_hash_med(c: &mut Criterion) {
        let sizes = vec![100, 250, 500, 1000, 2500, 5000, 7500];
        c.bench_function_over_inputs("gen_hash_medium_rayon", bench_hash, sizes);
    }

    fn gen_hash_large(c: &mut Criterion) {
        let sizes = vec![10_000, 25_000, 50_000, 75_000];
        c.bench_function_over_inputs("gen_hash_large_rayon", bench_hash, sizes);
    }

    fn gen_hash_xlarge(c: &mut Criterion) {
        let sizes = vec![100_000, 250_000, 500_000, 750_000, 1_000_000];
        c.bench_function_over_inputs("gen_hash_xlarge_rayon", bench_hash, sizes);
    }

    criterion_group!(benches, gen_hash_small, gen_hash_med, gen_hash_large, gen_hash_xlarge);
}