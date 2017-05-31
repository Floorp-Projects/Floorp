// Copyright 2013-2014 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::char;
use idna::uts46;
use test::TestFn;

pub fn collect_tests<F: FnMut(String, TestFn)>(add_test: &mut F) {
    // http://www.unicode.org/Public/idna/latest/IdnaTest.txt
    for (i, line) in include_str!("IdnaTest.txt").lines().enumerate() {
        if line == "" || line.starts_with("#") {
            continue
        }
        // Remove comments
        let mut line = match line.find("#") {
            Some(index) => &line[0..index],
            None => line
        };

        let mut expected_failure = false;
        if line.starts_with("XFAIL") {
            expected_failure = true;
            line = &line[5..line.len()];
        };

        let mut pieces = line.split(';').map(|x| x.trim()).collect::<Vec<&str>>();

        let test_type = pieces.remove(0);
        let original = pieces.remove(0);
        let source = unescape(original);
        let to_unicode = pieces.remove(0);
        let to_ascii = pieces.remove(0);
        let nv8 = if pieces.len() > 0 { pieces.remove(0) } else { "" };

        if expected_failure {
            continue;
        }

        let test_name = format!("UTS #46 line {}", i + 1);
        add_test(test_name, TestFn::dyn_test_fn(move || {
            let result = uts46::to_ascii(&source, uts46::Flags {
                use_std3_ascii_rules: true,
                transitional_processing: test_type == "T",
                verify_dns_length: true,
            });

            if to_ascii.starts_with("[") {
                if to_ascii.starts_with("[C") {
                    // http://unicode.org/reports/tr46/#Deviations
                    // applications that perform IDNA2008 lookup are not required to check
                    // for these contexts
                    return;
                }
                if to_ascii == "[V2]" {
                    // Everybody ignores V2
                    // https://github.com/servo/rust-url/pull/240
                    // https://github.com/whatwg/url/issues/53#issuecomment-181528158
                    // http://www.unicode.org/review/pri317/
                    return;
                }
                let res = result.ok();
                assert!(res == None, "Expected error. result: {} | original: {} | source: {}",
                        res.unwrap(), original, source);
                return;
            }

            let to_ascii = if to_ascii.len() > 0 {
                to_ascii.to_string()
            } else {
                if to_unicode.len() > 0 {
                    to_unicode.to_string()
                } else {
                    source.clone()
                }
            };

            if nv8 == "NV8" {
                // This result isn't valid under IDNA2008. Skip it
                return;
            }

            assert!(result.is_ok(), "Couldn't parse {} | original: {} | error: {:?}",
                    source, original, result.err());
            let output = result.ok().unwrap();
            assert!(output == to_ascii, "result: {} | expected: {} | original: {} | source: {}",
                    output, to_ascii, original, source);
        }))
    }
}

fn unescape(input: &str) -> String {
    let mut output = String::new();
    let mut chars = input.chars();
    loop {
        match chars.next() {
            None => return output,
            Some(c) =>
                if c == '\\' {
                    match chars.next().unwrap() {
                        '\\' => output.push('\\'),
                        'u' => {
                            let c1 = chars.next().unwrap().to_digit(16).unwrap();
                            let c2 = chars.next().unwrap().to_digit(16).unwrap();
                            let c3 = chars.next().unwrap().to_digit(16).unwrap();
                            let c4 = chars.next().unwrap().to_digit(16).unwrap();
                            match char::from_u32((((c1 * 16 + c2) * 16 + c3) * 16 + c4))
                            {
                                Some(c) => output.push(c),
                                None => { output.push_str(&format!("\\u{:X}{:X}{:X}{:X}",c1,c2,c3,c4)); }
                            };
                        }
                        _ => panic!("Invalid test data input"),
                    }
                } else {
                    output.push(c);
                }
        }
    }
}
