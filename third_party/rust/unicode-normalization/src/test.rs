// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use UnicodeNormalization;

#[test]
fn test_nfd() {
    macro_rules! t {
        ($input: expr, $expected: expr) => {
            assert_eq!($input.nfd().collect::<String>(), $expected);
            // A dummy iterator that is not std::str::Chars directly:
            assert_eq!($input.chars().map(|c| c).nfd().collect::<String>(), $expected);
        }
    }
    t!("abc", "abc");
    t!("\u{1e0b}\u{1c4}", "d\u{307}\u{1c4}");
    t!("\u{2026}", "\u{2026}");
    t!("\u{2126}", "\u{3a9}");
    t!("\u{1e0b}\u{323}", "d\u{323}\u{307}");
    t!("\u{1e0d}\u{307}", "d\u{323}\u{307}");
    t!("a\u{301}", "a\u{301}");
    t!("\u{301}a", "\u{301}a");
    t!("\u{d4db}", "\u{1111}\u{1171}\u{11b6}");
    t!("\u{ac1c}", "\u{1100}\u{1162}");
}

#[test]
fn test_nfkd() {
    macro_rules! t {
        ($input: expr, $expected: expr) => {
            assert_eq!($input.nfkd().collect::<String>(), $expected);
        }
    }
    t!("abc", "abc");
    t!("\u{1e0b}\u{1c4}", "d\u{307}DZ\u{30c}");
    t!("\u{2026}", "...");
    t!("\u{2126}", "\u{3a9}");
    t!("\u{1e0b}\u{323}", "d\u{323}\u{307}");
    t!("\u{1e0d}\u{307}", "d\u{323}\u{307}");
    t!("a\u{301}", "a\u{301}");
    t!("\u{301}a", "\u{301}a");
    t!("\u{d4db}", "\u{1111}\u{1171}\u{11b6}");
    t!("\u{ac1c}", "\u{1100}\u{1162}");
}

#[test]
fn test_nfc() {
    macro_rules! t {
        ($input: expr, $expected: expr) => {
            assert_eq!($input.nfc().collect::<String>(), $expected);
        }
    }
    t!("abc", "abc");
    t!("\u{1e0b}\u{1c4}", "\u{1e0b}\u{1c4}");
    t!("\u{2026}", "\u{2026}");
    t!("\u{2126}", "\u{3a9}");
    t!("\u{1e0b}\u{323}", "\u{1e0d}\u{307}");
    t!("\u{1e0d}\u{307}", "\u{1e0d}\u{307}");
    t!("a\u{301}", "\u{e1}");
    t!("\u{301}a", "\u{301}a");
    t!("\u{d4db}", "\u{d4db}");
    t!("\u{ac1c}", "\u{ac1c}");
    t!("a\u{300}\u{305}\u{315}\u{5ae}b", "\u{e0}\u{5ae}\u{305}\u{315}b");
}

#[test]
fn test_nfkc() {
    macro_rules! t {
        ($input: expr, $expected: expr) => {
            assert_eq!($input.nfkc().collect::<String>(), $expected);
        }
    }
    t!("abc", "abc");
    t!("\u{1e0b}\u{1c4}", "\u{1e0b}D\u{17d}");
    t!("\u{2026}", "...");
    t!("\u{2126}", "\u{3a9}");
    t!("\u{1e0b}\u{323}", "\u{1e0d}\u{307}");
    t!("\u{1e0d}\u{307}", "\u{1e0d}\u{307}");
    t!("a\u{301}", "\u{e1}");
    t!("\u{301}a", "\u{301}a");
    t!("\u{d4db}", "\u{d4db}");
    t!("\u{ac1c}", "\u{ac1c}");
    t!("a\u{300}\u{305}\u{315}\u{5ae}b", "\u{e0}\u{5ae}\u{305}\u{315}b");
}

#[test]
fn test_official() {
    use testdata::TEST_NORM;
    macro_rules! normString {
        ($method: ident, $input: expr) => { $input.$method().collect::<String>() }
    }

    for &(s1, s2, s3, s4, s5) in TEST_NORM {
        // these invariants come from the CONFORMANCE section of
        // http://www.unicode.org/Public/UNIDATA/NormalizationTest.txt
        {
            let r1 = normString!(nfc, s1);
            let r2 = normString!(nfc, s2);
            let r3 = normString!(nfc, s3);
            let r4 = normString!(nfc, s4);
            let r5 = normString!(nfc, s5);
            assert_eq!(s2, &r1[..]);
            assert_eq!(s2, &r2[..]);
            assert_eq!(s2, &r3[..]);
            assert_eq!(s4, &r4[..]);
            assert_eq!(s4, &r5[..]);
        }

        {
            let r1 = normString!(nfd, s1);
            let r2 = normString!(nfd, s2);
            let r3 = normString!(nfd, s3);
            let r4 = normString!(nfd, s4);
            let r5 = normString!(nfd, s5);
            assert_eq!(s3, &r1[..]);
            assert_eq!(s3, &r2[..]);
            assert_eq!(s3, &r3[..]);
            assert_eq!(s5, &r4[..]);
            assert_eq!(s5, &r5[..]);
        }

        {
            let r1 = normString!(nfkc, s1);
            let r2 = normString!(nfkc, s2);
            let r3 = normString!(nfkc, s3);
            let r4 = normString!(nfkc, s4);
            let r5 = normString!(nfkc, s5);
            assert_eq!(s4, &r1[..]);
            assert_eq!(s4, &r2[..]);
            assert_eq!(s4, &r3[..]);
            assert_eq!(s4, &r4[..]);
            assert_eq!(s4, &r5[..]);
        }

        {
            let r1 = normString!(nfkd, s1);
            let r2 = normString!(nfkd, s2);
            let r3 = normString!(nfkd, s3);
            let r4 = normString!(nfkd, s4);
            let r5 = normString!(nfkd, s5);
            assert_eq!(s5, &r1[..]);
            assert_eq!(s5, &r2[..]);
            assert_eq!(s5, &r3[..]);
            assert_eq!(s5, &r4[..]);
            assert_eq!(s5, &r5[..]);
        }
    }
}
