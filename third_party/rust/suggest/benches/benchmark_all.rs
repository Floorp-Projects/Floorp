use criterion::{criterion_group, criterion_main, BatchSize, Criterion};
use suggest::benchmarks::{ingest, BenchmarkWithInput};

pub fn ingest_single_provider(c: &mut Criterion) {
    let mut group = c.benchmark_group("ingest");
    viaduct_reqwest::use_reqwest_backend();
    // This needs to be 10 for now, or else the `ingest-amp-wikipedia` benchmark would take around
    // 100s to run which feels like too long.  `ingest-amp-mobile` also would take a around 50s.
    group.sample_size(10);
    for (name, benchmark) in ingest::all_benchmarks() {
        group.bench_function(format!("ingest-{name}"), |b| {
            b.iter_batched(
                || benchmark.generate_input(),
                |input| benchmark.benchmarked_code(input),
                // See https://docs.rs/criterion/latest/criterion/enum.BatchSize.html#variants for
                // a discussion of this.  PerIteration is chosen for these benchmarks because the
                // input holds a database file handle
                BatchSize::PerIteration,
            );
        });
    }
}

criterion_group!(benches, ingest_single_provider);
criterion_main!(benches);
