#[test]
fn empty_string_fails() {
    assert!(wat::parse_str("").is_err());
}
