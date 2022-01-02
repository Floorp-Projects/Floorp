#![warn(rust_2018_idioms, single_use_lifetimes)]

#[rustversion::attr(not(nightly), ignore)]
#[test]
fn ui() {
    let t = trybuild::TestCases::new();
    t.compile_fail("tests/ui/cfg/*.rs");
    t.compile_fail("tests/ui/not_unpin/*.rs");
    t.compile_fail("tests/ui/pin_project/*.rs");
    t.compile_fail("tests/ui/pinned_drop/*.rs");
    t.compile_fail("tests/ui/project/*.rs");
    t.compile_fail("tests/ui/unsafe_unpin/*.rs");
    t.compile_fail("tests/ui/unstable-features/*.rs");
    t.pass("tests/ui/unstable-features/run-pass/*.rs");
}
