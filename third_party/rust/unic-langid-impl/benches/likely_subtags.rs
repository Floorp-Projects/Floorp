use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

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
    "und-PL",
    "und-Latn-AM",
    "ug-Cyrl",
    "sr-ME",
    "mn-Mong",
    "lif-Limb",
    "gan",
    "zh-Hant",
    "yue-Hans",
    "unr",
    "unr-Deva",
    "und-Thai-CN",
    "ug-Cyrl",
    "en-Latn-DE",
    "pl-FR",
    "de-CH",
    "tuq",
    "sr-ME",
    "ng",
    "klx",
    "kk-Arab",
    "en-Cyrl",
    "und-Cyrl-UK",
    "und-Arab",
    "und-Arab-FO",
];

fn maximize_bench(c: &mut Criterion) {
    let langids: Vec<LanguageIdentifier> = STRINGS
        .iter()
        .map(|s| -> LanguageIdentifier { s.parse().unwrap() })
        .collect();
    c.bench_function("maximize", move |b| {
        b.iter(|| {
            for mut s in langids.clone().into_iter() {
                s.maximize();
            }
        })
    });
}

fn extract_input(
    s: &str,
) -> (
    subtags::Language,
    Option<subtags::Script>,
    Option<subtags::Region>,
) {
    let langid: LanguageIdentifier = s.parse().unwrap();
    (langid.language, langid.script, langid.region)
}

fn raw_maximize_bench(c: &mut Criterion) {
    let entries: Vec<_> = STRINGS.iter().map(|s| extract_input(s)).collect();

    c.bench_function("raw_maximize", move |b| {
        b.iter(|| {
            for (lang, script, region) in entries.clone().into_iter() {
                let _ = unic_langid_impl::likelysubtags::maximize(lang, script, region);
            }
        })
    });
}

criterion_group!(benches, maximize_bench, raw_maximize_bench,);
criterion_main!(benches);
