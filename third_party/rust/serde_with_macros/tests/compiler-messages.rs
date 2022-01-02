extern crate rustversion;
extern crate trybuild;

// This test fails for older compiler versions since the error messages are different.
#[rustversion::attr(before(1.35), ignore)]
#[test]
fn compile_test() {
    // This test does not work under tarpaulin, so skip it if detected
    if std::env::var("TARPAULIN") == Ok("1".to_string()) {
        return;
    }

    let t = trybuild::TestCases::new();
    t.compile_fail("tests/compile-fail/*.rs");
}
