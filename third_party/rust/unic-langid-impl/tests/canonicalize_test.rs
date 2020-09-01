use unic_langid_impl::canonicalize;

fn assert_canonicalize(input: &str, output: &str) {
    assert_eq!(&canonicalize(input).unwrap(), output);
}

#[test]
fn test_canonicalize() {
    assert_canonicalize("Pl", "pl");
    assert_canonicalize("eN-uS", "en-US");
    assert_canonicalize("ZH_hans_hK", "zh-Hans-HK");
    assert_canonicalize("en-scouse-fonipa", "en-fonipa-scouse");
}
