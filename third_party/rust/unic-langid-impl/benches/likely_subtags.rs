use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

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
    c.bench_function("maximize", move |b| {
        b.iter(|| {
            let langids: Vec<LanguageIdentifier> = STRINGS
                .iter()
                .map(|s| -> LanguageIdentifier { s.parse().unwrap() })
                .collect();
            for mut s in langids {
                s.maximize();
                let _ = black_box(s.to_string());
            }
        })
    });
}

fn extract_input(s: &str) -> (Option<TinyStr8>, Option<TinyStr4>, Option<TinyStr4>) {
    let chunks: Vec<&str> = s.split("-").collect();
    let mut lang: Option<TinyStr8> = chunks.get(0).map(|s| s.parse().unwrap());
    let mut script: Option<TinyStr4> = chunks.get(1).map(|s| s.parse().unwrap());
    let mut region: Option<TinyStr4> = chunks.get(2).map(|s| s.parse().unwrap());
    if let Some(l) = lang {
        if l.as_str() == "und" {
            lang = None;
        }
    }
    if let Some(s) = script {
        if s.as_str().chars().count() == 2 {
            region = script;
            script = None;
        }
    }
    (lang, script, region)
}

fn raw_maximize_bench(c: &mut Criterion) {
    let entries: Vec<(Option<TinyStr8>, Option<TinyStr4>, Option<TinyStr4>)> =
        STRINGS.iter().map(|s| extract_input(s)).collect();

    c.bench_function("raw_maximize", move |b| {
        b.iter(|| {
            for (lang, script, region) in &entries {
                let _ = unic_langid_impl::likelysubtags::maximize(
                    lang.clone(),
                    script.clone(),
                    region.clone(),
                );
            }
        })
    });
}

criterion_group!(benches, maximize_bench, raw_maximize_bench,);
criterion_main!(benches);
