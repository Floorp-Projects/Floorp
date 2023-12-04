// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;
mod helpers;

use icu_locid::Locale;
use icu_locid_transform::{LocaleCanonicalizer, LocaleExpander, TransformResult};
use writeable::assert_writeable_eq;

#[test]
fn test_maximize() {
    let lc = LocaleExpander::new_extended();

    let path = "./tests/fixtures/maximize.json";
    let testcases: Vec<fixtures::CanonicalizationTest> =
        helpers::read_fixture(path).expect("Failed to read a fixture");

    for case in testcases {
        if let Some(true) = case.disabled {
            continue;
        }
        let mut locale: Locale = case.input.parse().unwrap();
        let unmodified = locale.clone();
        let result = lc.maximize(&mut locale);
        assert_writeable_eq!(locale, case.output);
        if result == TransformResult::Modified {
            assert_ne!(locale, unmodified);
        } else {
            assert_eq!(locale, unmodified);
        }
    }
}

#[test]
fn test_minimize() {
    let lc = LocaleExpander::new_extended();

    let path = "./tests/fixtures/minimize.json";
    let testcases: Vec<fixtures::CanonicalizationTest> =
        helpers::read_fixture(path).expect("Failed to read a fixture");

    for case in testcases {
        if let Some(true) = case.disabled {
            continue;
        }
        let mut locale: Locale = case.input.parse().unwrap();
        let unmodified = locale.clone();
        let result = lc.minimize(&mut locale);
        assert_writeable_eq!(locale, case.output);
        if result == TransformResult::Modified {
            assert_ne!(locale, unmodified);
        } else {
            assert_eq!(locale, unmodified);
        }
    }
}

#[test]
fn test_canonicalize() {
    let lc = LocaleCanonicalizer::new();

    let path = "./tests/fixtures/canonicalize.json";
    let testcases: Vec<fixtures::CanonicalizationTest> =
        helpers::read_fixture(path).expect("Failed to read a fixture");

    for case in testcases {
        if let Some(true) = case.disabled {
            continue;
        }
        let mut locale: Locale = case.input.parse().expect("Unable to parse input");
        let unmodified = locale.clone();
        let result = lc.canonicalize(&mut locale);
        assert_writeable_eq!(locale, case.output);
        if result == TransformResult::Modified {
            assert_ne!(locale, unmodified);
        } else {
            assert_eq!(locale, unmodified);
        }
    }
}
