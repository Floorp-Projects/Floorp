use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;
use criterion::Fun;

use tinystr::{TinyStr4, TinyStr8};
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
            let entries: Vec<(Option<&str>, Option<&str>, Option<&str>, Vec<&str>)> =
                langids
                    .iter()
                    .map(|langid| {
                        let lang = Some(langid.language()).and_then(|s| {
                            if s == "und" {
                                None
                            } else {
                                Some(s)
                            }
                        });
                        (
                            lang,
                            langid.script(),
                            langid.region(),
                            langid.variants().collect::<Vec<_>>(),
                        )
                    })
                    .collect();
            b.iter(|| {
                for (language, script, region, variants) in &entries {
                    let _ = LanguageIdentifier::from_parts(*language, *script, *region, variants);
                }
            })
        }),
        Fun::new(
            "from_parts_unchecked",
            |b, langids: &Vec<LanguageIdentifier>| {
                let entries = langids
                    .iter()
                    .map(|langid| langid.clone().into_raw_parts())
                    .collect::<Vec<_>>();
                b.iter(|| {
                    for (language, script, region, variants) in &entries {
                        let _ = LanguageIdentifier::from_raw_parts_unchecked(
                            language.map(|l| unsafe { TinyStr8::new_unchecked(l) }),
                            script.map(|s| unsafe { TinyStr4::new_unchecked(s) }),
                            region.map(|r| unsafe { TinyStr4::new_unchecked(r) }),
                            variants.as_ref().map(|v| {
                                v.into_iter()
                                    .map(|v| unsafe { TinyStr8::new_unchecked(*v) })
                                    .collect()
                            }),
                        );
                    }
                })
            },
        ),
    ];

    c.bench_functions("language_identifier_construct", funcs, langids);
}

criterion_group!(benches, language_identifier_construct_bench,);
criterion_main!(benches);
