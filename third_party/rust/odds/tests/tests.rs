
extern crate odds;
extern crate itertools;

use odds::string::StrExt;

#[test]
fn rep() {
    assert_eq!("".rep(0), "");
    assert_eq!("xy".rep(0), "");
    assert_eq!("xy".rep(3), "xyxyxy");
}

#[test]
fn prefixes() {
    itertools::assert_equal(
        "".prefixes(),
        Vec::<&str>::new());
    itertools::assert_equal(
        "x".prefixes(),
        vec!["x"]);
    itertools::assert_equal(
        "abc".prefixes(),
        vec!["a", "ab", "abc"]);
}

#[test]
fn substrings() {
    itertools::assert_equal(
        "αβγ".substrings(),
        vec!["α", "αβ", "β", "αβγ", "βγ", "γ"]);
}
