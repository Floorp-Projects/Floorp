// Copyright 2014 The html5ever Project Developers. See the
// COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg(all(test, not(feature = "unstable")))]

extern crate unicode_bidi;

use unicode_bidi::{bidi_class, BidiInfo, format_chars, level, Level};

#[derive(Debug)]
struct Fail {
    pub line_num: usize,
    pub input_base_level: Option<Level>,
    pub input_classes: Vec<String>,
    pub input_string: String,
    pub exp_base_level: Option<Level>,
    pub exp_levels: Vec<String>,
    pub exp_ordering: Vec<String>,
    pub actual_base_level: Option<Level>,
    pub actual_levels: Vec<Level>,
    // TODO pub actual_ordering: Vec<String>,
}

#[test]
#[should_panic(expected = "12827 test cases failed! (243920 passed)")]
fn test_basic_conformance() {
    let test_data = include_str!("data/BidiTest.txt");

    // Test set state
    let mut passed_num: i32 = 0;
    let mut fails: Vec<Fail> = Vec::new();
    let mut exp_levels: Vec<String> = Vec::new();
    let mut exp_ordering: Vec<String> = Vec::new();

    for (line_num, line) in test_data.lines().enumerate() {
        let line = line.trim();

        // Empty and comment lines
        if line.is_empty() || line.starts_with('#') {
            // Ignore
            continue;
        }

        // State setting lines
        if line.starts_with('@') {
            let tokens: Vec<String> = line.split_whitespace().map(|x| x.to_owned()).collect();
            let (setting, values) = (tokens[0].as_ref(), tokens[1..].to_vec());
            match setting {
                "@Levels:" => {
                    exp_levels = values.to_owned();
                }
                "@Reorder:" => {
                    exp_ordering = values.to_owned();
                }
                _ => {
                    // Ignore, to allow some degree of forward compatibility
                }
            }
            continue;
        }

        // Data lines
        {
            // Levels and ordering need to be set before any data line
            assert!(!exp_levels.is_empty());
            assert!(exp_ordering.len() <= exp_levels.len());

            let fields: Vec<&str> = line.split(';').collect();
            let input_classes: Vec<&str> = fields[0].split_whitespace().collect();
            let bitset: u8 = fields[1].trim().parse().unwrap();
            assert!(!input_classes.is_empty());
            assert!(bitset > 0);

            let input_string = get_sample_string_from_bidi_classes(&input_classes);

            for input_base_level in gen_base_levels_for_base_tests(bitset) {
                let bidi_info = BidiInfo::new(&input_string, input_base_level);

                // Check levels
                let exp_levels: Vec<String> = exp_levels.iter().map(|x| x.to_owned()).collect();
                let levels = gen_levels_list_from_bidi_info(&input_string, &bidi_info);
                if levels != exp_levels {
                    fails.push(
                        Fail {
                            line_num: line_num,
                            input_base_level: input_base_level,
                            input_classes: input_classes.iter().map(|x| x.to_string()).collect(),
                            input_string: input_string.to_owned(),
                            exp_base_level: None,
                            exp_levels: exp_levels.to_owned(),
                            exp_ordering: exp_ordering.to_owned(),
                            actual_base_level: None,
                            actual_levels: levels.to_owned(),
                        }
                    );
                } else {
                    passed_num += 1;
                }

                // Check reorder map
                // TODO: Add reorder map to API output and test the map here
            }
        }
    }

    if !fails.is_empty() {
        // TODO: Show a list of failed cases when the number is less than 1K
        panic!(
            "{} test cases failed! ({} passed) {{\n\
            \n\
            0: {:?}\n\
            \n\
            ...\n\
            \n\
            {}: {:?}\n\
            \n\
            }}",
            fails.len(),
            passed_num,
            fails[0],
            fails.len() - 1,
            fails[fails.len() - 1],
        );
    }
}

#[test]
#[should_panic(expected = "14558 test cases failed! (77141 passed)")]
fn test_character_conformance() {
    let test_data = include_str!("data/BidiCharacterTest.txt");

    // Test set state
    let mut passed_num: i32 = 0;
    let mut fails: Vec<Fail> = Vec::new();

    for (line_num, line) in test_data.lines().enumerate() {
        let line = line.trim();

        // Empty and comment lines
        if line.is_empty() || line.starts_with('#') {
            // Ignore
            continue;
        }

        // Data lines
        {
            let fields: Vec<&str> = line.split(';').collect();
            let input_chars: Vec<char> = fields[0]
                .split_whitespace()
                .map(|cp_hex| u32::from_str_radix(cp_hex, 16).unwrap())
                .map(|cp_u32| std::char::from_u32(cp_u32).unwrap())
                .collect();
            let input_string: String = input_chars.into_iter().collect();
            let input_base_level: Option<Level> =
                gen_base_level_for_characters_tests(fields[1].trim().parse().unwrap());
            let exp_base_level: Level = Level::new(fields[2].trim().parse().unwrap()).unwrap();
            let exp_levels: Vec<String> =
                fields[3].split_whitespace().map(|x| x.to_owned()).collect();
            let exp_ordering: Vec<String> =
                fields[4].split_whitespace().map(|x| x.to_owned()).collect();

            let bidi_info = BidiInfo::new(&input_string, input_base_level);

            // Check levels
            let levels = gen_levels_list_from_bidi_info(&input_string, &bidi_info);
            if levels != exp_levels {
                fails.push(
                    Fail {
                        line_num: line_num,
                        input_base_level: input_base_level,
                        input_classes: vec![],
                        input_string: input_string.to_owned(),
                        exp_base_level: Some(exp_base_level),
                        exp_levels: exp_levels.to_owned(),
                        exp_ordering: exp_ordering.to_owned(),
                        actual_base_level: None,
                        actual_levels: levels.to_owned(),
                    }
                );
            } else {
                passed_num += 1;
            }

            // Check reorder map
            // TODO: Add reorder map to API output and test the map here
        }
    }

    if !fails.is_empty() {
        // TODO: Show a list of failed cases when the number is less than 1K
        panic!(
            "{} test cases failed! ({} passed) {{\n\
            \n\
            0: {:?}\n\
            \n\
            ...\n\
            \n\
            {}: {:?}\n\
            \n\
            }}",
            fails.len(),
            passed_num,
            fails[0],
            fails.len() - 1,
            fails[fails.len() - 1],
        );
    }
}

// TODO: Support auto-RTL
fn gen_base_levels_for_base_tests(bitset: u8) -> Vec<Option<Level>> {
    /// Values: auto-LTR, LTR, RTL
    const VALUES: &'static [Option<Level>] = &[None, Some(level::LTR_LEVEL), Some(level::RTL_LEVEL)];
    assert!(bitset < (1 << VALUES.len()));
    (0..VALUES.len())
        .filter(|bit| bitset & (1u8 << bit) == 1)
        .map(|idx| VALUES[idx])
        .collect()
}

// TODO: Support auto-RTL
fn gen_base_level_for_characters_tests(idx: usize) -> Option<Level> {
    /// Values: LTR, RTL, auto-LTR
    const VALUES: &'static [Option<Level>] = &[Some(level::LTR_LEVEL), Some(level::RTL_LEVEL), None];
    assert!(idx < VALUES.len());
    VALUES[idx]
}

/// We need to collaps levels to one-per-character from one-per-byte format.
fn gen_levels_list_from_bidi_info(input_str: &str, bidi_info: &BidiInfo) -> Vec<Level> {
    let para = &bidi_info.paragraphs[0];
    let levels = bidi_info.reordered_levels(para, para.range.clone());
    // TODO: Move to impl BidiInfo as pub api
    input_str.char_indices().map(|(i, _)| levels[i]).collect()
}

fn get_sample_string_from_bidi_classes(class_names: &[&str]) -> String {
    class_names
        .iter()
        .map(|class_name| gen_char_from_bidi_class(class_name))
        .collect()
}

/// TODO: Auto-gen in tables.rs ?
fn gen_char_from_bidi_class(class_name: &str) -> char {
    match class_name {
        "AL" => '\u{060b}',
        "AN" => '\u{0605}',
        "B" => '\u{000a}',
        "BN" => '\u{0000}',
        "CS" => '\u{002c}',
        "EN" => '\u{0039}',
        "ES" => '\u{002b}',
        "ET" => '\u{0023}',
        "FSI" => format_chars::FSI,
        "L" => '\u{0041}',
        "LRE" => format_chars::LRE,
        "LRI" => format_chars::LRI,
        "LRO" => format_chars::LRO,
        "NSM" => '\u{0300}',
        "ON" => '\u{0021}',
        "PDF" => format_chars::PDF,
        "PDI" => format_chars::PDI,
        "R" => '\u{0590}',
        "RLE" => format_chars::RLE,
        "RLI" => format_chars::RLI,
        "RLO" => format_chars::RLO,
        "S" => '\u{0009}',
        "WS" => '\u{000c}',
        &_ => panic!("Invalid Bidi_Class name: {}", class_name),
    }
}

#[test]
fn test_gen_char_from_bidi_class() {
    use unicode_bidi::BidiClass::*;
    for &class in &[
        AL,
        AN,
        B,
        BN,
        CS,
        EN,
        ES,
        ET,
        FSI,
        L,
        LRE,
        LRI,
        LRO,
        NSM,
        ON,
        PDF,
        PDI,
        R,
        RLE,
        RLI,
        RLO,
        S,
        WS,
    ] {
        let class_name = format!("{:?}", class);
        let sample_char = gen_char_from_bidi_class(&class_name);
        assert_eq!(bidi_class(sample_char), class);
    }
}
