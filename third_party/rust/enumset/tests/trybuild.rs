#[rustversion::nightly]
#[test]
fn ui() {
    let t = trybuild::TestCases::new();
    t.compile_fail("tests/compile-fail/*.rs");
    t.pass("tests/compile-pass/*.rs");
}
