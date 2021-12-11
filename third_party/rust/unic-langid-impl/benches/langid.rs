use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;
use criterion::Fun;

use unic_langid_impl::subtags;
use unic_langid_impl::LanguageIdentifier;

static STRINGS: &[&str] = &[
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

fn language_identifier_construct_bench(c: &mut Criterion) {
    let langids: Vec<LanguageIdentifier> = STRINGS
        .iter()
        .map(|s| -> LanguageIdentifier { s.parse().unwrap() })
        .collect();

    let funcs = vec![
        Fun::new("from_str", |b, _| {
            b.iter(|| {
                for s in STRINGS {
                    let _: Result<LanguageIdentifier, _> = black_box(s).parse();
                }
            })
        }),
        Fun::new("from_bytes", |b, _| {
            let slices: Vec<&[u8]> = STRINGS.iter().map(|s| s.as_bytes()).collect();
            b.iter(|| {
                for s in &slices {
                    let _ = LanguageIdentifier::from_bytes(black_box(s));
                }
            })
        }),
        Fun::new("from_parts", |b, langids: &Vec<LanguageIdentifier>| {
            let entries: Vec<(
                subtags::Language,
                Option<subtags::Script>,
                Option<subtags::Region>,
                Vec<subtags::Variant>,
            )> = langids
                .iter()
                .cloned()
                .map(|langid| langid.into_parts())
                .collect();
            b.iter(|| {
                for (language, script, region, variants) in &entries {
                    let _ = LanguageIdentifier::from_parts(
                        language.clone(),
                        script.clone(),
                        region.clone(),
                        variants,
                    );
                }
            })
        }),
    ];

    c.bench_functions("language_identifier_construct", funcs, langids);
}

criterion_group!(benches, language_identifier_construct_bench,);
criterion_main!(benches);
