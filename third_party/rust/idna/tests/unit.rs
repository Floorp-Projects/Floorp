extern crate idna;
extern crate unicode_normalization;

use unicode_normalization::char::is_combining_mark;

fn _to_ascii(domain: &str) -> Result<String, idna::Errors> {
    idna::Config::default()
        .verify_dns_length(true)
        .use_std3_ascii_rules(true)
        .to_ascii(domain)
}

#[test]
fn test_v5() {
    // IdnaTest:784 蔏｡𑰺
    assert!(is_combining_mark('\u{11C3A}'));
    assert!(_to_ascii("\u{11C3A}").is_err());
    assert!(_to_ascii("\u{850f}.\u{11C3A}").is_err());
    assert!(_to_ascii("\u{850f}\u{ff61}\u{11C3A}").is_err());
}

#[test]
fn test_v8_bidi_rules() {
    assert_eq!(_to_ascii("abc").unwrap(), "abc");
    assert_eq!(_to_ascii("123").unwrap(), "123");
    assert_eq!(_to_ascii("אבּג").unwrap(), "xn--kdb3bdf");
    assert_eq!(_to_ascii("ابج").unwrap(), "xn--mgbcm");
    assert_eq!(_to_ascii("abc.ابج").unwrap(), "abc.xn--mgbcm");
    assert_eq!(
        _to_ascii("אבּג.ابج").unwrap(),
        "xn--kdb3bdf.xn--mgbcm"
    );

    // Bidi domain names cannot start with digits
    assert!(_to_ascii("0a.\u{05D0}").is_err());
    assert!(_to_ascii("0à.\u{05D0}").is_err());

    // Bidi chars may be punycode-encoded
    assert!(_to_ascii("xn--0ca24w").is_err());
}
