use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use unic_langid_impl::parser::parse_language_identifier;

fn language_identifier_parser_bench(c: &mut Criterion) {
    let strings = &[
        "en-US",
        "en-GB",
        "es-AR",
        "it",
        "zh-Hans-CN",
        "de-AT",
        "pl",
        "fr-FR",
        "de-AT",
        "sr-Cyrl-SR",
        "nb-NO",
        "fr-FR",
        "mk",
        "uk",
        "en-US",
        "en-GB",
        "es-AR",
        "th",
        "de",
        "zh-Cyrl-HN",
        "en-Latn-US",
    ];

    c.bench_function("language_identifier_parser", |b| {
        let slices: Vec<&[u8]> = strings.iter().map(|s| s.as_bytes()).collect();
        b.iter(|| {
            for s in &slices {
                let _ = parse_language_identifier(black_box(s));
            }
        })
    });
}

fn language_identifier_parser_casing_bench(c: &mut Criterion) {
    let strings = &[
        "En_uS",
        "EN-GB",
        "ES-aR",
        "iT",
        "zH_HaNs_cN",
        "dE-aT",
        "Pl",
        "FR-FR",
        "de_AT",
        "sR-CyrL_sr",
        "NB-NO",
        "fr_fr",
        "Mk",
        "uK",
        "en-us",
        "en_gb",
        "ES-AR",
        "tH",
        "DE",
        "ZH_cyrl_hN",
        "eN-lAtN-uS",
    ];
    c.bench_function("language_identifier_parser_casing", |b| {
        let slices: Vec<&[u8]> = strings.iter().map(|s| s.as_bytes()).collect();
        b.iter(|| {
            for s in &slices {
                let _ = parse_language_identifier(black_box(s));
            }
        })
    });
}

criterion_group!(
    benches,
    language_identifier_parser_bench,
    language_identifier_parser_casing_bench,
);
criterion_main!(benches);
