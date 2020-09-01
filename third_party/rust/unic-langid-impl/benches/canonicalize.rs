use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use unic_langid_impl::canonicalize;

fn langid_canonicalize_bench(c: &mut Criterion) {
    let strings = &[
        "en-us",
        "en_gb",
        "ES-AR",
        "tH",
        "DE",
        "ZH_cyrl_hN",
        "eN-lAtN-uS",
    ];
    c.bench_function("langid_canonicalize", |b| {
        b.iter(|| {
            for s in strings {
                let _ = canonicalize(black_box(s));
            }
        })
    });
    c.bench_function("langid_canonicalize_from_bytes", |b| {
        let slices: Vec<&[u8]> = strings.iter().map(|s| s.as_bytes()).collect();
        b.iter(|| {
            for s in &slices {
                let _ = canonicalize(black_box(s));
            }
        })
    });
}

criterion_group!(benches, langid_canonicalize_bench,);
criterion_main!(benches);
