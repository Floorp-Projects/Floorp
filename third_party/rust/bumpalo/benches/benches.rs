use criterion::*;

#[derive(Default)]
struct Small(u8);

#[derive(Default)]
struct Big([usize; 32]);

fn alloc<T: Default>(n: usize) {
    let arena = bumpalo::Bump::with_capacity(n * std::mem::size_of::<T>());
    for _ in 0..n {
        let arena = black_box(&arena);
        let val: &mut T = arena.alloc(black_box(Default::default()));
        black_box(val);
    }
}

fn alloc_with<T: Default>(n: usize) {
    let arena = bumpalo::Bump::with_capacity(n * std::mem::size_of::<T>());
    for _ in 0..n {
        let arena = black_box(&arena);
        let val: &mut T = arena.alloc_with(|| black_box(Default::default()));
        black_box(val);
    }
}

#[cfg(feature = "collections")]
fn format_realloc(bump: &bumpalo::Bump, n: usize) {
    let n = criterion::black_box(n);
    let s = bumpalo::format!(in bump, "Hello {:.*}", n, "World! ");
    criterion::black_box(s);
}

const ALLOCATIONS: usize = 10_000;

fn bench_alloc(c: &mut Criterion) {
    let mut group = c.benchmark_group("alloc");
    group.throughput(Throughput::Elements(ALLOCATIONS as u64));
    group.bench_function("small", |b| b.iter(|| alloc::<Small>(ALLOCATIONS)));
    group.bench_function("big", |b| b.iter(|| alloc::<Big>(ALLOCATIONS)));
}

fn bench_alloc_with(c: &mut Criterion) {
    let mut group = c.benchmark_group("alloc-with");
    group.throughput(Throughput::Elements(ALLOCATIONS as u64));
    group.bench_function("small", |b| b.iter(|| alloc_with::<Small>(ALLOCATIONS)));
    group.bench_function("big", |b| b.iter(|| alloc_with::<Big>(ALLOCATIONS)));
}

fn bench_format_realloc(c: &mut Criterion) {
    let mut group = c.benchmark_group("format-realloc");

    for n in (1..5).map(|n| n * n * n * 10) {
        group.throughput(Throughput::Elements(n as u64));
        group.bench_with_input(BenchmarkId::new("format-realloc", n), &n, |b, n| {
            let mut bump = bumpalo::Bump::new();
            b.iter(|| {
                bump.reset();
                format_realloc(&bump, *n);
            });
        });
    }
}

criterion_group!(benches, bench_alloc, bench_alloc_with, bench_format_realloc);
criterion_main!(benches);
