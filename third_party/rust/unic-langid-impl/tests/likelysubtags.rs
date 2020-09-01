use tinystr::{TinyStr4, TinyStr8};
use unic_langid_impl::likelysubtags::{maximize, minimize, CLDR_VERSION};

static STRINGS: &[(&str, Option<&str>)] = &[
    ("en-US", Some("en-Latn-US")),
    ("en-GB", Some("en-Latn-GB")),
    ("es-AR", Some("es-Latn-AR")),
    ("it", Some("it-Latn-IT")),
    ("zh-Hans-CN", None),
    ("de-AT", Some("de-Latn-AT")),
    ("pl", Some("pl-Latn-PL")),
    ("fr-FR", Some("fr-Latn-FR")),
    ("de-AT", Some("de-Latn-AT")),
    ("sr-Cyrl-SR", None),
    ("nb-NO", Some("nb-Latn-NO")),
    ("fr-FR", Some("fr-Latn-FR")),
    ("mk", Some("mk-Cyrl-MK")),
    ("uk", Some("uk-Cyrl-UA")),
    ("und-PL", Some("pl-Latn-PL")),
    ("und-Latn-AM", Some("ku-Latn-AM")),
    ("ug-Cyrl", Some("ug-Cyrl-KZ")),
    ("sr-ME", Some("sr-Latn-ME")),
    ("mn-Mong", Some("mn-Mong-CN")),
    ("lif-Limb", Some("lif-Limb-IN")),
    ("gan", Some("gan-Hans-CN")),
    ("zh-Hant", Some("zh-Hant-TW")),
    ("yue-Hans", Some("yue-Hans-CN")),
    ("unr", Some("unr-Beng-IN")),
    ("unr-Deva", Some("unr-Deva-NP")),
    ("und-Thai-CN", Some("lcp-Thai-CN")),
    ("ug-Cyrl", Some("ug-Cyrl-KZ")),
    ("en-Latn-DE", None),
    ("pl-FR", Some("pl-Latn-FR")),
    ("de-CH", Some("de-Latn-CH")),
    ("tuq", Some("tuq-Latn")),
    ("sr-ME", Some("sr-Latn-ME")),
    ("ng", Some("ng-Latn-NA")),
    ("klx", Some("klx-Latn")),
    ("kk-Arab", Some("kk-Arab-CN")),
    ("en-Cyrl", Some("en-Cyrl-US")),
    ("und-Cyrl-UK", Some("ru-Cyrl-UK")),
    ("und-Arab", Some("ar-Arab-EG")),
    ("und-Arab-FO", Some("ar-Arab-FO")),
    ("zh-TW", Some("zh-Hant-TW")),
];

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

fn extract_output(
    s: Option<&str>,
) -> Option<(Option<TinyStr8>, Option<TinyStr4>, Option<TinyStr4>)> {
    s.map(|s| {
        let chunks: Vec<&str> = s.split("-").collect();
        (
            chunks.get(0).map(|s| s.parse().unwrap()),
            chunks.get(1).map(|s| s.parse().unwrap()),
            chunks.get(2).map(|s| s.parse().unwrap()),
        )
    })
}

#[test]
fn maximize_test() {
    for i in STRINGS {
        let chunks = extract_input(i.0);
        let result = maximize(chunks.0, chunks.1, chunks.2);
        assert_eq!(extract_output(i.1), result);
    }
}

#[test]
fn version_works() {
    assert_eq!(CLDR_VERSION, "36");
}

#[test]
fn minimize_test() {
    let lang: TinyStr8 = "zh".parse().unwrap();
    let script: TinyStr4 = "Hant".parse().unwrap();
    let result = minimize(Some(lang), Some(script), None);
    assert_eq!(result, Some(extract_input("zh-TW")));

    let lang: TinyStr8 = "en".parse().unwrap();
    let script: TinyStr4 = "Latn".parse().unwrap();
    let region: TinyStr4 = "US".parse().unwrap();
    let result = minimize(Some(lang), Some(script), Some(region));
    assert_eq!(result, Some(extract_input("en")));
}
