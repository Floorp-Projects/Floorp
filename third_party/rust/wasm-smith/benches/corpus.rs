use arbitrary::{Arbitrary, Unstructured};
use criterion::{black_box, criterion_group, criterion_main, Criterion};
use wasm_smith::Module;

pub fn benchmark_corpus(c: &mut Criterion) {
    let mut corpus = Vec::with_capacity(2000);
    let entries = std::fs::read_dir("./benches/corpus").expect("failed to read dir");
    for e in entries {
        let e = e.expect("failed to read dir entry");
        let seed = std::fs::read(e.path()).expect("failed to read seed file");
        corpus.push(seed);
    }

    // Benchmark how long it takes to generate a module for every seed in our
    // corpus (taken from the `validate` fuzz target).
    c.bench_function("corpus", |b| {
        b.iter(|| {
            for seed in &corpus {
                let seed = black_box(seed);
                let mut u = Unstructured::new(seed);
                let result = Module::arbitrary(&mut u);
                let _ = black_box(result);
            }
        })
    });
}

criterion_group!(benches, benchmark_corpus);
criterion_main!(benches);
