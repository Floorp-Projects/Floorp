extern crate criterion;

use criterion::{criterion_group, criterion_main, Criterion, ParameterizedBenchmark, Throughput};

#[derive(Default)]
struct Small(u8);

#[derive(Default)]
struct Big([usize; 32]);

struct Huge([usize; 1024]);

impl Default for Huge {
    fn default() -> Huge {
        Huge([0; 1024])
    }
}

fn alloc<T: Default>(n: usize) {
    let arena = bumpalo::Bump::new();
    for _ in 0..n {
        let val: &mut T = arena.alloc(Default::default());
        criterion::black_box(val);
    }
}

fn alloc_with<T: Default>(n: usize) {
    let arena = bumpalo::Bump::new();
    for _ in 0..n {
        let val: &mut T = arena.alloc_with(|| Default::default());
        criterion::black_box(val);
    }
}

fn format_realloc(bump: &bumpalo::Bump, n: usize) {
    let n = criterion::black_box(n);
    let s = bumpalo::format!(in bump, "Hello {:.*}", n, "World! ");
    criterion::black_box(s);
}

fn criterion_benchmark(c: &mut Criterion) {
    c.bench(
        "alloc",
        ParameterizedBenchmark::new(
            "small",
            |b, n| b.iter(|| alloc::<Small>(*n)),
            (1..3).map(|n| n * 1000).collect::<Vec<usize>>(),
        )
        .throughput(|n| Throughput::Elements(*n as u32)),
    );

    c.bench(
        "alloc",
        ParameterizedBenchmark::new(
            "big",
            |b, n| b.iter(|| alloc::<Big>(*n)),
            (1..3).map(|n| n * 1000).collect::<Vec<usize>>(),
        )
        .throughput(|n| Throughput::Elements(*n as u32)),
    );

    c.bench(
        "alloc",
        ParameterizedBenchmark::new(
            "huge",
            |b, n| b.iter(|| alloc::<Huge>(*n)),
            (1..3).map(|n| n * 1000).collect::<Vec<usize>>(),
        )
        .throughput(|n| Throughput::Elements(*n as u32)),
    );

    c.bench(
        "alloc_with",
        ParameterizedBenchmark::new(
            "small",
            |b, n| b.iter(|| alloc_with::<Small>(*n)),
            (1..3).map(|n| n * 1000).collect::<Vec<usize>>(),
        )
        .throughput(|n| Throughput::Elements(*n as u32)),
    );

    c.bench(
        "alloc_with",
        ParameterizedBenchmark::new(
            "big",
            |b, n| b.iter(|| alloc_with::<Big>(*n)),
            (1..3).map(|n| n * 1000).collect::<Vec<usize>>(),
        )
        .throughput(|n| Throughput::Elements(*n as u32)),
    );

    c.bench(
        "alloc_with",
        ParameterizedBenchmark::new(
            "huge",
            |b, n| b.iter(|| alloc_with::<Huge>(*n)),
            (1..3).map(|n| n * 1000).collect::<Vec<usize>>(),
        )
        .throughput(|n| Throughput::Elements(*n as u32)),
    );

    c.bench(
        "format-realloc",
        ParameterizedBenchmark::new(
            "hello {someone}",
            |b, n| {
                let mut bump = bumpalo::Bump::new();
                b.iter(|| {
                    bump.reset();
                    format_realloc(&bump, *n);
                });
            },
            (1..5).map(|n| n * n * n * 10).collect::<Vec<usize>>(),
        )
        .throughput(|n| Throughput::Elements(*n as u32)),
    );
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
