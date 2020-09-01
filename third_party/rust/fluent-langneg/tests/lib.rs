use std::error::Error;
use std::fs;
use std::fs::File;
use std::path::Path;

use fluent_langneg::convert_vec_str_to_langids_lossy;
use fluent_langneg::negotiate_languages;
use fluent_langneg::parse_accepted_languages;
use fluent_langneg::NegotiationStrategy;
use unic_langid::langid;
use unic_langid::LanguageIdentifier;
use unic_locale::locale;

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
#[serde(untagged)]
enum NegotiateTestInput {
    NoDefault(Vec<String>, Vec<String>),
    Default(Vec<String>, Vec<String>, String),
}

#[derive(Serialize, Deserialize)]
struct NegotiateTestSet {
    input: NegotiateTestInput,
    strategy: Option<String>,
    output: Vec<String>,
}

#[derive(Serialize, Deserialize)]
struct AcceptedLanguagesTestSet {
    input: String,
    output: Vec<String>,
}

fn read_negotiate_testsets<P: AsRef<Path>>(
    path: P,
) -> Result<Vec<NegotiateTestSet>, Box<dyn Error>> {
    let file = File::open(path)?;
    let sets = serde_json::from_reader(file)?;
    Ok(sets)
}

fn test_negotiate_fixtures(path: &str) {
    println!("Testing path: {}", path);
    let tests = read_negotiate_testsets(path).unwrap();

    for test in tests {
        let strategy = match test.strategy {
            Some(strategy) => match strategy.as_str() {
                "filtering" => NegotiationStrategy::Filtering,
                "matching" => NegotiationStrategy::Matching,
                "lookup" => NegotiationStrategy::Lookup,
                _ => NegotiationStrategy::Filtering,
            },
            _ => NegotiationStrategy::Filtering,
        };
        match test.input {
            NegotiateTestInput::NoDefault(requested, available) => {
                let requested = convert_vec_str_to_langids_lossy(requested);
                let available = convert_vec_str_to_langids_lossy(available);
                let output = convert_vec_str_to_langids_lossy(test.output);
                let output2: Vec<&LanguageIdentifier> = output.iter().collect();
                assert_eq!(
                    negotiate_languages(&requested, &available, None, strategy),
                    output2,
                    "Test in {} failed",
                    path
                );
            }
            NegotiateTestInput::Default(requested, available, default) => {
                let requested = convert_vec_str_to_langids_lossy(requested);
                let available = convert_vec_str_to_langids_lossy(available);
                let output = convert_vec_str_to_langids_lossy(test.output);
                let output2: Vec<&LanguageIdentifier> = output.iter().collect();
                assert_eq!(
                    negotiate_languages(
                        &requested,
                        &available,
                        default.parse().ok().as_ref(),
                        strategy
                    ),
                    output2,
                    "Test in {} failed",
                    path
                );
            }
        }
    }
}

#[test]
fn negotiate_filtering() {
    let paths = fs::read_dir("./tests/fixtures/negotiate/filtering").unwrap();

    for path in paths {
        let p = path.unwrap().path().to_str().unwrap().to_owned();
        test_negotiate_fixtures(p.as_str());
    }
}

#[test]
fn negotiate_matching() {
    let paths = fs::read_dir("./tests/fixtures/negotiate/matching").unwrap();

    for path in paths {
        let p = path.unwrap().path().to_str().unwrap().to_owned();
        test_negotiate_fixtures(p.as_str());
    }
}

#[test]
fn negotiate_lookup() {
    let paths = fs::read_dir("./tests/fixtures/negotiate/lookup").unwrap();

    for path in paths {
        let p = path.unwrap().path().to_str().unwrap().to_owned();
        test_negotiate_fixtures(p.as_str());
    }
}

#[test]
fn accepted_languages() {
    let file = File::open("./tests/fixtures/accepted_languages.json").unwrap();
    let tests: Vec<AcceptedLanguagesTestSet> = serde_json::from_reader(file).unwrap();

    for test in tests {
        let locales = parse_accepted_languages(test.input.as_str());
        let output = convert_vec_str_to_langids_lossy(test.output);
        assert_eq!(output, locales);
    }
}

#[test]
fn langid_matching() {
    let langid_en_us = langid!("en-US");
    let langid_de_at = langid!("de-AT");
    let langid_en = langid!("en");
    let langid_de = langid!("de");
    let langid_pl = langid!("pl");

    let requested = &[&langid_en_us, &langid_de_at];
    let available = &[&langid_pl, &langid_de, &langid_en];
    assert_eq!(
        negotiate_languages(requested, available, None, NegotiationStrategy::Matching),
        &[&&langid_en, &&langid_de],
    );

    let requested = &[langid_en_us, langid_de_at];
    let available = &[langid_pl, langid_de.clone(), langid_en.clone()];
    assert_eq!(
        negotiate_languages(requested, available, None, NegotiationStrategy::Matching),
        &[&langid_en, &langid_de],
    );
}

#[test]
fn cldr_feature() {
    // In this case, the full likelySubtags algorithm knows that `mn` -> `mn-Cyrl`, but
    // the mock doesn't.
    #[cfg(feature = "cldr")]
    assert_eq!(
        negotiate_languages(
            &[langid!("mn")],
            &[langid!("mn-Latn"), langid!("mn-Cyrl")],
            None,
            NegotiationStrategy::Filtering
        ),
        &[&langid!("mn-Cyrl")]
    );

    // In result, the mock will just return both in undefined
    // order.
    #[cfg(not(feature = "cldr"))]
    assert_eq!(
        negotiate_languages(
            &[langid!("mn")],
            &[langid!("mn-Latn"), langid!("mn-Cyrl")],
            None,
            NegotiationStrategy::Filtering
        )
        .len(),
        2
    );
}

#[test]
fn locale_matching() {
    let loc_en_us = locale!("en-US-u-hc-h12");
    let loc_de_at = locale!("de-AT-u-hc-h24");
    let loc_en = locale!("en-u-ca-buddhist");
    let loc_de = locale!("de");
    let loc_pl = locale!("pl-x-private");

    assert_eq!(
        negotiate_languages(
            &[&loc_en_us, &loc_de_at],
            &[&loc_pl, &loc_de, &loc_en],
            None,
            NegotiationStrategy::Matching
        ),
        &[&&loc_en, &&loc_de],
    );

    assert_eq!(
        negotiate_languages(
            &[loc_en_us, loc_de_at],
            &[loc_pl, loc_de.clone(), loc_en.clone()],
            None,
            NegotiationStrategy::Matching
        ),
        &[&loc_en, &loc_de],
    );
}
