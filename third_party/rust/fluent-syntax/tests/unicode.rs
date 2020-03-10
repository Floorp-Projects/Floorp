use fluent_syntax::unicode::unescape_unicode;
use std::borrow::Cow;

fn is_cow_borrowed<'a>(input: Cow<'a, str>) -> bool {
    if let Cow::Borrowed(_) = input {
        true
    } else {
        false
    }
}

#[test]
fn unescape_unicode_test() {
    assert!(is_cow_borrowed(unescape_unicode("foo")));

    assert_eq!(unescape_unicode("foo"), "foo");
    assert_eq!(unescape_unicode("foo \\\\"), "foo \\");
    assert_eq!(unescape_unicode("foo \\\""), "foo \"");
    assert_eq!(unescape_unicode("foo \\\\ faa"), "foo \\ faa");
    assert_eq!(
        unescape_unicode("foo \\\\ faa \\\\ fii"),
        "foo \\ faa \\ fii"
    );
    assert_eq!(
        unescape_unicode("foo \\\\\\\" faa \\\"\\\\ fii"),
        "foo \\\" faa \"\\ fii"
    );
    assert_eq!(unescape_unicode("\\u0041\\u004F"), "AO");
    assert_eq!(unescape_unicode("\\uA"), "�");
    assert_eq!(unescape_unicode("\\uA0Pl"), "�");
    assert_eq!(unescape_unicode("\\d Foo"), "� Foo");
}
