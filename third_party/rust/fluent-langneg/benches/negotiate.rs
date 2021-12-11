use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use fluent_langneg::convert_vec_str_to_langids_lossy;
use fluent_langneg::negotiate_languages;

use unic_langid::LanguageIdentifier;

#[no_mangle]
#[inline(never)]
fn do_negotiate<'a>(
    requested: &[LanguageIdentifier],
    available: &'a [LanguageIdentifier],
) -> Vec<&'a LanguageIdentifier> {
    negotiate_languages(
        requested,
        available,
        None,
        fluent_langneg::NegotiationStrategy::Filtering,
    )
}

fn negotiate_bench(c: &mut Criterion) {
    let requested = &["de", "it", "ru"];
    let available = &[
        "en-US", "fr", "de", "en-GB", "it", "pl", "ru", "sr-Cyrl", "sr-Latn", "zh-Hant", "zh-Hans",
        "ja-JP", "he-IL", "de-DE", "de-IT",
    ];

    let requested = convert_vec_str_to_langids_lossy(requested);
    let available = convert_vec_str_to_langids_lossy(available);

    c.bench_function("negotiate", |b| {
        b.iter(|| do_negotiate(&requested, &available))
    });
}

criterion_group!(benches, negotiate_bench);
criterion_main!(benches);
