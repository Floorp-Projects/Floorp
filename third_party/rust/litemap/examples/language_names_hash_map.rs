// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// LiteMap is intended as a small and low-memory drop-in replacement for HashMap.
//
// This example does not use LiteMap. The reader may compare this HashMap example to the
// LiteMap example to showcase analogous operations between HashMap and LiteMap.

#![no_main] // https://github.com/unicode-org/icu4x/issues/395

icu_benchmark_macros::static_setup!();

use icu_locid::subtags::{language, Language};
use std::collections::HashMap;

const DATA: [(Language, &str); 11] = [
    (language!("ar"), "Arabic"),
    (language!("bn"), "Bangla"),
    (language!("ccp"), "Chakma"),
    (language!("en"), "English"),
    (language!("es"), "Spanish"),
    (language!("fr"), "French"),
    (language!("ja"), "Japanese"),
    (language!("ru"), "Russian"),
    (language!("sr"), "Serbian"),
    (language!("th"), "Thai"),
    (language!("tr"), "Turkish"),
];

#[no_mangle]
fn main(_argc: isize, _argv: *const *const u8) -> isize {
    icu_benchmark_macros::main_setup!();

    let mut map = HashMap::new();
    // https://github.com/rust-lang/rust/issues/62633 was declined :(
    for (lang, name) in DATA.iter() {
        map.insert(lang, name).ok_or(()).unwrap_err();
    }

    assert_eq!(11, map.len());
    assert_eq!(Some(&&"Thai"), map.get(&language!("th")));
    assert_eq!(None, map.get(&language!("de")));

    0
}
