// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg(feature = "id")]
#![cfg(feature = "pattern")]

#[macro_use]
extern crate unic_char_range;

#[macro_use]
extern crate matches;
use regex;

use std::char;
use std::collections::BTreeSet;

use unic_ucd_category::GeneralCategory;
use unic_ucd_category::GeneralCategory::*;
use unic_ucd_ident::{is_id_continue, is_id_start};
use unic_ucd_ident::{is_pattern_syntax, is_pattern_whitespace};

use regex::Regex;

#[test]
/// ref: <https://www.unicode.org/reports/tr31/#Table_Lexical_Classes_for_Identifiers>
fn test_id_derivation() {
    let other_start: BTreeSet<char> = {
        Regex::new(
            r"(?xm)^
              ([[:xdigit:]]{4,6})        # low
              (?:..([[:xdigit:]]{4,6}))? # high
              \s+;\s+
              Other_ID_Start             # property
             ",
        )
        .unwrap()
        .captures_iter(include_str!(
            "../../../../external/unicode/ucd/data/PropList.txt"
        ))
        .flat_map(|cap: regex::Captures<'_>| {
            let low = char::from_u32(u32::from_str_radix(&cap[1], 16).unwrap()).unwrap();
            let high = cap
                .get(2)
                .map(|s| u32::from_str_radix(s.as_str(), 16).unwrap())
                .map(|u| char::from_u32(u).unwrap())
                .unwrap_or(low);
            chars!(low..=high)
        })
        .collect()
    };

    let other_continue: BTreeSet<char> = {
        Regex::new(
            r"(?xm)^
              ([[:xdigit:]]{4,6})        # low
              (?:..([[:xdigit:]]{4,6}))? # high
              \s+;\s+
              Other_ID_Continue          # property
             ",
        )
        .unwrap()
        .captures_iter(include_str!(
            "../../../../external/unicode/ucd/data/PropList.txt"
        ))
        .flat_map(|cap: regex::Captures<'_>| {
            let low = char::from_u32(u32::from_str_radix(&cap[1], 16).unwrap()).unwrap();
            let high = cap
                .get(2)
                .map(|s| u32::from_str_radix(s.as_str(), 16).unwrap())
                .map(|u| char::from_u32(u).unwrap())
                .unwrap_or(low);
            chars!(low..=high)
        })
        .collect()
    };

    let is_id_start_derived = |ch| {
        let class = GeneralCategory::of(ch);
        (class.is_letter() || class == LetterNumber || other_start.contains(&ch))
            && !is_pattern_syntax(ch)
            && !is_pattern_whitespace(ch)
    };

    let is_id_continue_derived = |ch| {
        let class = GeneralCategory::of(ch);
        (matches!(
            class,
            NonspacingMark | SpacingMark | DecimalNumber | ConnectorPunctuation
        ) || is_id_start_derived(ch)
            || other_continue.contains(&ch))
            && !is_pattern_syntax(ch)
            && !is_pattern_whitespace(ch)
    };

    for ch in chars!(..) {
        assert_eq!(is_id_start(ch), is_id_start_derived(ch));
        assert_eq!(is_id_continue(ch), is_id_continue_derived(ch));
    }
}
