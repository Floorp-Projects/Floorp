use criterion::{criterion_group, criterion_main, Criterion};
use oxilangtag::LanguageTag;

fn bench_language_tag_parse(c: &mut Criterion) {
    let examples = [
        "fr",
        "fr-Latn",
        "fr-fra",
        "fr-Latn-FR",
        "fr-Latn-419",
        "fr-FR",
        "ax-TZ",
        "fr-shadok",
        "fr-y-myext-myext2",
        "fra-Latn",
        "fra",
        "fra-FX",
        "i-klingon",
        "I-kLINgon",
        "no-bok",
        "fr-Lat",
        "mn-Cyrl-MN",
        "mN-cYrL-Mn",
        "fr-Latn-CA",
        "en-US",
        "fr-Latn-CA",
        "i-enochian",
        "x-fr-CH",
        "sr-Latn-CS",
        "es-419",
        "sl-nedis",
        "de-CH-1996",
        "de-Latg-1996",
        "sl-IT-nedis",
        "en-a-bbb-x-a-ccc",
        "de-a-value",
        "en-Latn-GB-boont-r-extended-sequence-x-private",
        "en-x-US",
        "az-Arab-x-AZE-derbend",
        "es-Latn-CO-x-private",
        "en-US-boont",
        "ab-x-abc-x-abc",
        "ab-x-abc-a-a",
        "i-default",
        "i-klingon",
        "abcd-Latn",
        "AaBbCcDd-x-y-any-x",
        "en",
        "de-AT",
        "es-419",
        "de-CH-1901",
        "sr-Cyrl",
        "sr-Cyrl-CS",
        "sl-Latn-IT-rozaj",
        "en-US-x-twain",
        "zh-cmn",
        "zh-cmn-Hant",
        "zh-cmn-Hant-HK",
        "zh-gan",
        "zh-yue-Hant-HK",
        "xr-lxs-qut",
        "xr-lqt-qu",
        "xr-p-lze",
    ];

    c.bench_function("language tag parse tests", |b| {
        b.iter(|| {
            for tag in examples.iter() {
                LanguageTag::parse(*tag).unwrap();
            }
        })
    });
}

criterion_group!(language_tag, bench_language_tag_parse);

criterion_main!(language_tag);
