#![cfg(all(target_family = "wasm", feature = "js", not(unix)))]

use wasm_bindgen_test::wasm_bindgen_test as test;
wasm_bindgen_test::wasm_bindgen_test_configure!(run_in_worker);

use sys_locale::{get_locale, get_locales};

#[test]
fn can_obtain_locale() {
    assert!(get_locale().is_some(), "no locales were returned");
    let locales = get_locales();
    for (i, locale) in locales.enumerate() {
        assert!(!locale.is_empty(), "locale string {} was empty", i);
    }
}
