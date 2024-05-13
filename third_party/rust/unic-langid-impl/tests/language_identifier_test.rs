use unic_langid_impl::parser::parse_language_identifier;
use unic_langid_impl::subtags;
use unic_langid_impl::CharacterDirection;
use unic_langid_impl::LanguageIdentifier;

fn assert_language_identifier(
    loc: &LanguageIdentifier,
    language: Option<&str>,
    script: Option<&str>,
    region: Option<&str>,
    variants: Option<&[&str]>,
) {
    assert_eq!(
        loc.language,
        language.map_or(subtags::Language::default(), |l| {
            subtags::Language::from_bytes(l.as_bytes()).unwrap()
        })
    );
    assert_eq!(loc.script, script.map(|s| s.parse().unwrap()));
    assert_eq!(loc.region, region.map(|r| r.parse().unwrap()));
    let v = variants
        .unwrap_or(&[])
        .iter()
        .map(|v| -> subtags::Variant { v.parse().unwrap() })
        .collect::<Vec<_>>();
    assert_eq!(
        loc.variants().collect::<Vec<_>>(),
        v.iter().collect::<Vec<_>>(),
    );
}

fn assert_parsed_language_identifier(
    input: &str,
    language: Option<&str>,
    script: Option<&str>,
    region: Option<&str>,
    variants: Option<&[&str]>,
) {
    let langid = parse_language_identifier(input.as_bytes()).unwrap();
    assert_language_identifier(&langid, language, script, region, variants);
}

#[test]
fn test_language_identifier_parser() {
    assert_parsed_language_identifier("pl", Some("pl"), None, None, None);
    assert_parsed_language_identifier("und", None, None, None, None);
    assert_parsed_language_identifier("en-US", Some("en"), None, Some("US"), None);
    assert_parsed_language_identifier("en-Latn-US", Some("en"), Some("Latn"), Some("US"), None);
    assert_parsed_language_identifier("sl-nedis", Some("sl"), None, None, Some(&["nedis"]));
}

#[test]
fn test_language_casing() {
    assert_parsed_language_identifier("Pl", Some("pl"), None, None, None);
    assert_parsed_language_identifier("En-uS", Some("en"), None, Some("US"), None);
    assert_parsed_language_identifier("eN-lAtN-uS", Some("en"), Some("Latn"), Some("US"), None);
    assert_parsed_language_identifier("ZH_cyrl_hN", Some("zh"), Some("Cyrl"), Some("HN"), None);
}

#[test]
fn test_serialize_langid() {
    let langid: LanguageIdentifier = "en-Latn-US".parse().unwrap();
    assert_eq!(&langid.to_string(), "en-Latn-US");
}

#[test]
fn test_sorted_variants() {
    let langid: LanguageIdentifier = "en-nedis-macos".parse().unwrap();
    assert_eq!(&langid.to_string(), "en-macos-nedis");

    let langid = LanguageIdentifier::from_parts(
        "en".parse().unwrap(),
        None,
        None,
        &["nedis".parse().unwrap(), "macos".parse().unwrap()],
    );
    assert_eq!(&langid.to_string(), "en-macos-nedis");
}

#[test]
fn test_from_parts_unchecked() {
    let langid: LanguageIdentifier = "en-nedis-macos".parse().unwrap();
    let (lang, script, region, variants) = langid.into_parts();
    let langid = LanguageIdentifier::from_raw_parts_unchecked(
        lang,
        script,
        region,
        Some(variants.into_boxed_slice()),
    );
    assert_eq!(&langid.to_string(), "en-macos-nedis");
}

#[test]
fn test_matches() {
    let langid_en: LanguageIdentifier = "en".parse().unwrap();
    let langid_en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let langid_en_us2: LanguageIdentifier = "en-US".parse().unwrap();
    let langid_pl: LanguageIdentifier = "pl".parse().unwrap();
    assert_eq!(langid_en.matches(&langid_en_us, false, false), false);
    assert_eq!(langid_en_us.matches(&langid_en_us2, false, false), true);
    assert_eq!(langid_en.matches(&langid_pl, false, false), false);
    assert_eq!(langid_en.matches(&langid_en_us, true, false), true);
}

#[test]
fn test_set_fields() {
    let mut langid = LanguageIdentifier::default();
    assert_eq!(&langid.to_string(), "und");

    langid.language = "pl".parse().expect("Setting language failed");
    assert_eq!(&langid.to_string(), "pl");

    langid.language = "de".parse().expect("Setting language failed");
    assert_eq!(&langid.to_string(), "de");
    langid.region = Some("AT".parse().expect("Setting region failed"));
    assert_eq!(&langid.to_string(), "de-AT");
    langid.script = Some("Latn".parse().expect("Setting script failed"));
    assert_eq!(&langid.to_string(), "de-Latn-AT");
    langid.set_variants(&["macos".parse().expect("Setting variants failed")]);
    assert_eq!(&langid.to_string(), "de-Latn-AT-macos");

    assert_eq!(langid.has_variant("macos".parse().unwrap()), true);
    assert_eq!(langid.has_variant("windows".parse().unwrap()), false);

    langid.language.clear();
    assert_eq!(&langid.to_string(), "und-Latn-AT-macos");
    langid.region = None;
    assert_eq!(&langid.to_string(), "und-Latn-macos");
    langid.script = None;
    assert_eq!(&langid.to_string(), "und-macos");
    langid.clear_variants();
    assert_eq!(&langid.to_string(), "und");

    assert_eq!(langid.has_variant("macos".parse().unwrap()), false);
}

#[test]
fn test_matches_as_range() {
    let langid: LanguageIdentifier = "en-US".parse().unwrap();
    let langid2: LanguageIdentifier = "en-US-windows".parse().unwrap();
    assert_eq!(langid.matches(&langid2, false, false), false);
    assert_eq!(langid.matches(&langid2, true, false), true);
    assert_eq!(langid.matches(&langid2, false, true), false);
    assert_eq!(langid.matches(&langid2, true, true), true);
}

#[test]
fn test_character_direction() {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    assert_eq!(en_us.character_direction(), CharacterDirection::LTR);

    let ar_af: LanguageIdentifier = "ar-AF".parse().unwrap();
    assert_eq!(ar_af.character_direction(), CharacterDirection::RTL);

    let ks: LanguageIdentifier = "ks".parse().unwrap();
    assert_eq!(ks.character_direction(), CharacterDirection::RTL);

    let ks_deva: LanguageIdentifier = "ks-Deva".parse().unwrap();
    assert_eq!(ks_deva.character_direction(), CharacterDirection::LTR);

    let mn_mong: LanguageIdentifier = "mn-Mong".parse().unwrap();
    assert_eq!(mn_mong.character_direction(), CharacterDirection::TTB);

    let lid: LanguageIdentifier = "pa-Guru".parse().unwrap();
    assert_eq!(lid.character_direction(), CharacterDirection::LTR);

    let lid_pa_pk: LanguageIdentifier = "pa-PK".parse().unwrap();
    assert_eq!(lid_pa_pk.character_direction(), CharacterDirection::RTL);

    let lid_ar_us: LanguageIdentifier = "ar-US".parse().unwrap();
    assert_eq!(lid_ar_us.character_direction(), CharacterDirection::RTL);
}

#[cfg(not(feature = "likelysubtags"))]
#[test]
fn test_character_direction_without_likelysubtags() {
    let lid: LanguageIdentifier = "pa".parse().unwrap();
    assert_eq!(lid.character_direction(), CharacterDirection::RTL);
}

#[cfg(feature = "likelysubtags")]
#[test]
fn test_character_direction_with_likelysubtags() {
    let lid_pa: LanguageIdentifier = "pa".parse().unwrap();
    assert_eq!(lid_pa.character_direction(), CharacterDirection::LTR);
}

#[test]
fn test_langid_ord() {
    let input = &[
        "en-US-macos-zarab",
        "en-US-macos-nedis",
        "en-US-macos",
        "en-GB",
        "en",
        "en-US",
        "ar",
        "fr",
        "de",
    ];

    let mut langids = input
        .iter()
        .map(|l| -> LanguageIdentifier { l.parse().unwrap() })
        .collect::<Vec<_>>();

    langids.sort();

    let result = langids.iter().map(|l| l.to_string()).collect::<Vec<_>>();

    assert_eq!(
        &result,
        &[
            "ar",
            "de",
            "en",
            "en-GB",
            "en-US",
            "en-US-macos",
            "en-US-macos-nedis",
            "en-US-macos-zarab",
            "fr"
        ]
    );
}
