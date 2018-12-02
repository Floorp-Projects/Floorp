// Copyright 2013-2014 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Unit tests

#[macro_use]
extern crate url;

use std::ascii::AsciiExt;
use std::borrow::Cow;
use std::cell::{Cell, RefCell};
use std::net::{Ipv4Addr, Ipv6Addr};
use std::path::{Path, PathBuf};
use url::{Host, HostAndPort, Url, form_urlencoded};

#[test]
fn size() {
    use std::mem::size_of;
    assert_eq!(size_of::<Url>(), size_of::<Option<Url>>());
}

macro_rules! assert_from_file_path {
    ($path: expr) => { assert_from_file_path!($path, $path) };
    ($path: expr, $url_path: expr) => {{
        let url = Url::from_file_path(Path::new($path)).unwrap();
        assert_eq!(url.host(), None);
        assert_eq!(url.path(), $url_path);
        assert_eq!(url.to_file_path(), Ok(PathBuf::from($path)));
    }};
}



#[test]
fn new_file_paths() {
    if cfg!(unix) {
        assert_eq!(Url::from_file_path(Path::new("relative")), Err(()));
        assert_eq!(Url::from_file_path(Path::new("../relative")), Err(()));
    }
    if cfg!(windows) {
        assert_eq!(Url::from_file_path(Path::new("relative")), Err(()));
        assert_eq!(Url::from_file_path(Path::new(r"..\relative")), Err(()));
        assert_eq!(Url::from_file_path(Path::new(r"\drive-relative")), Err(()));
        assert_eq!(Url::from_file_path(Path::new(r"\\ucn\")), Err(()));
    }

    if cfg!(unix) {
        assert_from_file_path!("/foo/bar");
        assert_from_file_path!("/foo/ba\0r", "/foo/ba%00r");
        assert_from_file_path!("/foo/ba%00r", "/foo/ba%2500r");
    }
}

#[test]
#[cfg(unix)]
fn new_path_bad_utf8() {
    use std::ffi::OsStr;
    use std::os::unix::prelude::*;

    let url = Url::from_file_path(Path::new(OsStr::from_bytes(b"/foo/ba\x80r"))).unwrap();
    let os_str = OsStr::from_bytes(b"/foo/ba\x80r");
    assert_eq!(url.to_file_path(), Ok(PathBuf::from(os_str)));
}

#[test]
fn new_path_windows_fun() {
    if cfg!(windows) {
        assert_from_file_path!(r"C:\foo\bar", "/C:/foo/bar");
        assert_from_file_path!("C:\\foo\\ba\0r", "/C:/foo/ba%00r");

        // Invalid UTF-8
        assert!(Url::parse("file:///C:/foo/ba%80r").unwrap().to_file_path().is_err());

        // test windows canonicalized path
        let path = PathBuf::from(r"\\?\C:\foo\bar");
        assert!(Url::from_file_path(path).is_ok());

        // Percent-encoded drive letter
        let url = Url::parse("file:///C%3A/foo/bar").unwrap();
        assert_eq!(url.to_file_path(), Ok(PathBuf::from(r"C:\foo\bar")));
    }
}


#[test]
fn new_directory_paths() {
    if cfg!(unix) {
        assert_eq!(Url::from_directory_path(Path::new("relative")), Err(()));
        assert_eq!(Url::from_directory_path(Path::new("../relative")), Err(()));

        let url = Url::from_directory_path(Path::new("/foo/bar")).unwrap();
        assert_eq!(url.host(), None);
        assert_eq!(url.path(), "/foo/bar/");
    }
    if cfg!(windows) {
        assert_eq!(Url::from_directory_path(Path::new("relative")), Err(()));
        assert_eq!(Url::from_directory_path(Path::new(r"..\relative")), Err(()));
        assert_eq!(Url::from_directory_path(Path::new(r"\drive-relative")), Err(()));
        assert_eq!(Url::from_directory_path(Path::new(r"\\ucn\")), Err(()));

        let url = Url::from_directory_path(Path::new(r"C:\foo\bar")).unwrap();
        assert_eq!(url.host(), None);
        assert_eq!(url.path(), "/C:/foo/bar/");
    }
}

#[test]
fn path_backslash_fun() {
    let mut special_url = "http://foobar.com".parse::<Url>().unwrap();
    special_url.path_segments_mut().unwrap().push("foo\\bar");
    assert_eq!(special_url.as_str(), "http://foobar.com/foo%5Cbar");

    let mut nonspecial_url = "thing://foobar.com".parse::<Url>().unwrap();
    nonspecial_url.path_segments_mut().unwrap().push("foo\\bar");
    assert_eq!(nonspecial_url.as_str(), "thing://foobar.com/foo\\bar");
}

#[test]
fn from_str() {
    assert!("http://testing.com/this".parse::<Url>().is_ok());
}

#[test]
fn parse_with_params() {
    let url = Url::parse_with_params("http://testing.com/this?dont=clobberme",
                                     &[("lang", "rust")]).unwrap();

    assert_eq!(url.as_str(), "http://testing.com/this?dont=clobberme&lang=rust");
}

#[test]
fn issue_124() {
    let url: Url = "file:a".parse().unwrap();
    assert_eq!(url.path(), "/a");
    let url: Url = "file:...".parse().unwrap();
    assert_eq!(url.path(), "/...");
    let url: Url = "file:..".parse().unwrap();
    assert_eq!(url.path(), "/");
}

#[test]
fn test_equality() {
    use std::hash::{Hash, Hasher};
    use std::collections::hash_map::DefaultHasher;

    fn check_eq(a: &Url, b: &Url) {
        assert_eq!(a, b);

        let mut h1 = DefaultHasher::new();
        a.hash(&mut h1);
        let mut h2 = DefaultHasher::new();
        b.hash(&mut h2);
        assert_eq!(h1.finish(), h2.finish());
    }

    fn url(s: &str) -> Url {
        let rv = s.parse().unwrap();
        check_eq(&rv, &rv);
        rv
    }

    // Doesn't care if default port is given.
    let a: Url = url("https://example.com/");
    let b: Url = url("https://example.com:443/");
    check_eq(&a, &b);

    // Different ports
    let a: Url = url("http://example.com/");
    let b: Url = url("http://example.com:8080/");
    assert!(a != b, "{:?} != {:?}", a, b);

    // Different scheme
    let a: Url = url("http://example.com/");
    let b: Url = url("https://example.com/");
    assert_ne!(a, b);

    // Different host
    let a: Url = url("http://foo.com/");
    let b: Url = url("http://bar.com/");
    assert_ne!(a, b);

    // Missing path, automatically substituted. Semantically the same.
    let a: Url = url("http://foo.com");
    let b: Url = url("http://foo.com/");
    check_eq(&a, &b);
}

#[test]
fn host() {
    fn assert_host(input: &str, host: Host<&str>) {
        assert_eq!(Url::parse(input).unwrap().host(), Some(host));
    }
    assert_host("http://www.mozilla.org", Host::Domain("www.mozilla.org"));
    assert_host("http://1.35.33.49", Host::Ipv4(Ipv4Addr::new(1, 35, 33, 49)));
    assert_host("http://[2001:0db8:85a3:08d3:1319:8a2e:0370:7344]", Host::Ipv6(Ipv6Addr::new(
        0x2001, 0x0db8, 0x85a3, 0x08d3, 0x1319, 0x8a2e, 0x0370, 0x7344)));
    assert_host("http://1.35.+33.49", Host::Domain("1.35.+33.49"));
    assert_host("http://[::]", Host::Ipv6(Ipv6Addr::new(0, 0, 0, 0, 0, 0, 0, 0)));
    assert_host("http://[::1]", Host::Ipv6(Ipv6Addr::new(0, 0, 0, 0, 0, 0, 0, 1)));
    assert_host("http://0x1.0X23.0x21.061", Host::Ipv4(Ipv4Addr::new(1, 35, 33, 49)));
    assert_host("http://0x1232131", Host::Ipv4(Ipv4Addr::new(1, 35, 33, 49)));
    assert_host("http://111", Host::Ipv4(Ipv4Addr::new(0, 0, 0, 111)));
    assert_host("http://2..2.3", Host::Domain("2..2.3"));
    assert!(Url::parse("http://42.0x1232131").is_err());
    assert!(Url::parse("http://192.168.0.257").is_err());
}

#[test]
fn host_serialization() {
    // libstd’s `Display for Ipv6Addr` serializes 0:0:0:0:0:0:_:_ and 0:0:0:0:0:ffff:_:_
    // using IPv4-like syntax, as suggested in https://tools.ietf.org/html/rfc5952#section-4
    // but https://url.spec.whatwg.org/#concept-ipv6-serializer specifies not to.

    // Not [::0.0.0.2] / [::ffff:0.0.0.2]
    assert_eq!(Url::parse("http://[0::2]").unwrap().host_str(), Some("[::2]"));
    assert_eq!(Url::parse("http://[0::ffff:0:2]").unwrap().host_str(), Some("[::ffff:0:2]"));
}

#[test]
fn test_idna() {
    assert!("http://goșu.ro".parse::<Url>().is_ok());
    assert_eq!(Url::parse("http://☃.net/").unwrap().host(), Some(Host::Domain("xn--n3h.net")));
    assert!("https://r2---sn-huoa-cvhl.googlevideo.com/crossdomain.xml".parse::<Url>().is_ok());
}

#[test]
fn test_serialization() {
    let data = [
        ("http://example.com/", "http://example.com/"),
        ("http://addslash.com", "http://addslash.com/"),
        ("http://@emptyuser.com/", "http://emptyuser.com/"),
        ("http://:@emptypass.com/", "http://emptypass.com/"),
        ("http://user@user.com/", "http://user@user.com/"),
        ("http://user:pass@userpass.com/", "http://user:pass@userpass.com/"),
        ("http://slashquery.com/path/?q=something", "http://slashquery.com/path/?q=something"),
        ("http://noslashquery.com/path?q=something", "http://noslashquery.com/path?q=something")
    ];
    for &(input, result) in &data {
        let url = Url::parse(input).unwrap();
        assert_eq!(url.as_str(), result);
    }
}

#[test]
fn test_form_urlencoded() {
    let pairs: &[(Cow<str>, Cow<str>)] = &[
        ("foo".into(), "é&".into()),
        ("bar".into(), "".into()),
        ("foo".into(), "#".into())
    ];
    let encoded = form_urlencoded::Serializer::new(String::new()).extend_pairs(pairs).finish();
    assert_eq!(encoded, "foo=%C3%A9%26&bar=&foo=%23");
    assert_eq!(form_urlencoded::parse(encoded.as_bytes()).collect::<Vec<_>>(), pairs.to_vec());
}

#[test]
fn test_form_serialize() {
    let encoded = form_urlencoded::Serializer::new(String::new())
        .append_pair("foo", "é&")
        .append_pair("bar", "")
        .append_pair("foo", "#")
        .finish();
    assert_eq!(encoded, "foo=%C3%A9%26&bar=&foo=%23");
}

#[test]
fn form_urlencoded_custom_encoding_override() {
    let encoded = form_urlencoded::Serializer::new(String::new())
        .custom_encoding_override(|s| s.as_bytes().to_ascii_uppercase().into())
        .append_pair("foo", "bar")
        .finish();
    assert_eq!(encoded, "FOO=BAR");
}

#[test]
fn host_and_port_display() {
    assert_eq!(
        format!(
            "{}",
            HostAndPort{ host: Host::Domain("www.mozilla.org"), port: 80}
        ),
        "www.mozilla.org:80"
    );
    assert_eq!(
        format!(
            "{}",
            HostAndPort::<String>{ host: Host::Ipv4(Ipv4Addr::new(1, 35, 33, 49)), port: 65535 }
        ),
        "1.35.33.49:65535"
    );
    assert_eq!(
        format!(
            "{}",
            HostAndPort::<String>{
                host: Host::Ipv6(Ipv6Addr::new(
                    0x2001, 0x0db8, 0x85a3, 0x08d3, 0x1319, 0x8a2e, 0x0370, 0x7344
                )),
                port: 1337
            })
        ,
        "[2001:db8:85a3:8d3:1319:8a2e:370:7344]:1337"
    )
}

#[test]
/// https://github.com/servo/rust-url/issues/61
fn issue_61() {
    let mut url = Url::parse("http://mozilla.org").unwrap();
    url.set_scheme("https").unwrap();
    assert_eq!(url.port(), None);
    assert_eq!(url.port_or_known_default(), Some(443));
    url.check_invariants().unwrap();
}

#[test]
#[cfg(not(windows))]
/// https://github.com/servo/rust-url/issues/197
fn issue_197() {
    let mut url = Url::from_file_path("/").expect("Failed to parse path");
    url.check_invariants().unwrap();
    assert_eq!(url, Url::parse("file:///").expect("Failed to parse path + protocol"));
    url.path_segments_mut().expect("path_segments_mut").pop_if_empty();
}

#[test]
fn issue_241() {
    Url::parse("mailto:").unwrap().cannot_be_a_base();
}

#[test]
/// https://github.com/servo/rust-url/issues/222
fn append_trailing_slash() {
    let mut url: Url = "http://localhost:6767/foo/bar?a=b".parse().unwrap();
    url.check_invariants().unwrap();
    url.path_segments_mut().unwrap().push("");
    url.check_invariants().unwrap();
    assert_eq!(url.to_string(), "http://localhost:6767/foo/bar/?a=b");
}

#[test]
/// https://github.com/servo/rust-url/issues/227
fn extend_query_pairs_then_mutate() {
    let mut url: Url = "http://localhost:6767/foo/bar".parse().unwrap();
    url.query_pairs_mut().extend_pairs(vec![ ("auth", "my-token") ].into_iter());
    url.check_invariants().unwrap();
    assert_eq!(url.to_string(), "http://localhost:6767/foo/bar?auth=my-token");
    url.path_segments_mut().unwrap().push("some_other_path");
    url.check_invariants().unwrap();
    assert_eq!(url.to_string(), "http://localhost:6767/foo/bar/some_other_path?auth=my-token");
}

#[test]
/// https://github.com/servo/rust-url/issues/222
fn append_empty_segment_then_mutate() {
    let mut url: Url = "http://localhost:6767/foo/bar?a=b".parse().unwrap();
    url.check_invariants().unwrap();
    url.path_segments_mut().unwrap().push("").pop();
    url.check_invariants().unwrap();
    assert_eq!(url.to_string(), "http://localhost:6767/foo/bar?a=b");
}

#[test]
/// https://github.com/servo/rust-url/issues/243
fn test_set_host() {
    let mut url = Url::parse("https://example.net/hello").unwrap();
    url.set_host(Some("foo.com")).unwrap();
    assert_eq!(url.as_str(), "https://foo.com/hello");
    assert!(url.set_host(None).is_err());
    assert_eq!(url.as_str(), "https://foo.com/hello");
    assert!(url.set_host(Some("")).is_err());
    assert_eq!(url.as_str(), "https://foo.com/hello");

    let mut url = Url::parse("foobar://example.net/hello").unwrap();
    url.set_host(None).unwrap();
    assert_eq!(url.as_str(), "foobar:/hello");

    let mut url = Url::parse("foo://ș").unwrap();
    assert_eq!(url.as_str(), "foo://%C8%99/");
    url.set_host(Some("goșu.ro")).unwrap();
    assert_eq!(url.as_str(), "foo://go%C8%99u.ro/");
}

#[test]
// https://github.com/servo/rust-url/issues/166
fn test_leading_dots() {
    assert_eq!(Host::parse(".org").unwrap(), Host::Domain(".org".to_owned()));
    assert_eq!(Url::parse("file://./foo").unwrap().domain(), Some("."));
}

// This is testing that the macro produces buildable code when invoked
// inside both a module and a function
#[test]
fn define_encode_set_scopes() {
    use url::percent_encoding::{utf8_percent_encode, SIMPLE_ENCODE_SET};

    define_encode_set! {
        /// This encode set is used in the URL parser for query strings.
        pub QUERY_ENCODE_SET = [SIMPLE_ENCODE_SET] | {' ', '"', '#', '<', '>'}
    }

    assert_eq!(utf8_percent_encode("foo bar", QUERY_ENCODE_SET).collect::<String>(), "foo%20bar");

    mod m {
        use url::percent_encoding::{utf8_percent_encode, SIMPLE_ENCODE_SET};

        define_encode_set! {
            /// This encode set is used in the URL parser for query strings.
            pub QUERY_ENCODE_SET = [SIMPLE_ENCODE_SET] | {' ', '"', '#', '<', '>'}
        }

        pub fn test() {
            assert_eq!(utf8_percent_encode("foo bar", QUERY_ENCODE_SET).collect::<String>(), "foo%20bar");
        }
    }

    m::test();
}

#[test]
/// https://github.com/servo/rust-url/issues/302
fn test_origin_hash() {
    use std::hash::{Hash,Hasher};
    use std::collections::hash_map::DefaultHasher;

    fn hash<T: Hash>(value: &T) -> u64 {
        let mut hasher = DefaultHasher::new();
        value.hash(&mut hasher);
        hasher.finish()
    }

    let origin = &Url::parse("http://example.net/").unwrap().origin();

    let origins_to_compare = [
        Url::parse("http://example.net:80/").unwrap().origin(),
        Url::parse("http://example.net:81/").unwrap().origin(),
        Url::parse("http://example.net").unwrap().origin(),
        Url::parse("http://example.net/hello").unwrap().origin(),
        Url::parse("https://example.net").unwrap().origin(),
        Url::parse("ftp://example.net").unwrap().origin(),
        Url::parse("file://example.net").unwrap().origin(),
        Url::parse("http://user@example.net/").unwrap().origin(),
        Url::parse("http://user:pass@example.net/").unwrap().origin(),
    ];

    for origin_to_compare in &origins_to_compare {
        if origin == origin_to_compare {
            assert_eq!(hash(origin), hash(origin_to_compare));
        } else {
            assert_ne!(hash(origin), hash(origin_to_compare));
        }
    }

    let opaque_origin = Url::parse("file://example.net").unwrap().origin();
    let same_opaque_origin = Url::parse("file://example.net").unwrap().origin();
    let other_opaque_origin = Url::parse("file://other").unwrap().origin();

    assert_ne!(hash(&opaque_origin), hash(&same_opaque_origin));
    assert_ne!(hash(&opaque_origin), hash(&other_opaque_origin));
}

#[test]
fn test_windows_unc_path() {
    if !cfg!(windows) {
        return
    }

    let url = Url::from_file_path(Path::new(r"\\host\share\path\file.txt")).unwrap();
    assert_eq!(url.as_str(), "file://host/share/path/file.txt");

    let url = Url::from_file_path(Path::new(r"\\höst\share\path\file.txt")).unwrap();
    assert_eq!(url.as_str(), "file://xn--hst-sna/share/path/file.txt");

    let url = Url::from_file_path(Path::new(r"\\192.168.0.1\share\path\file.txt")).unwrap();
    assert_eq!(url.host(), Some(Host::Ipv4(Ipv4Addr::new(192, 168, 0, 1))));

    let path = url.to_file_path().unwrap();
    assert_eq!(path.to_str(), Some(r"\\192.168.0.1\share\path\file.txt"));

    // Another way to write these:
    let url = Url::from_file_path(Path::new(r"\\?\UNC\host\share\path\file.txt")).unwrap();
    assert_eq!(url.as_str(), "file://host/share/path/file.txt");

    // Paths starting with "\\.\" (Local Device Paths) are intentionally not supported.
    let url = Url::from_file_path(Path::new(r"\\.\some\path\file.txt"));
    assert!(url.is_err());
}

// Test the now deprecated log_syntax_violation method for backward
// compatibility
#[test]
#[allow(deprecated)]
fn test_old_log_violation_option() {
    let violation = Cell::new(None);
    let url = Url::options()
        .log_syntax_violation(Some(&|s| violation.set(Some(s.to_owned()))))
        .parse("http:////mozilla.org:42").unwrap();
    assert_eq!(url.port(), Some(42));

    let violation = violation.take();
    assert_eq!(violation, Some("expected //".to_string()));
}

#[test]
fn test_syntax_violation_callback() {
    use url::SyntaxViolation::*;
    let violation = Cell::new(None);
    let url = Url::options()
        .syntax_violation_callback(Some(&|v| violation.set(Some(v))))
        .parse("http:////mozilla.org:42").unwrap();
    assert_eq!(url.port(), Some(42));

    let v = violation.take().unwrap();
    assert_eq!(v, ExpectedDoubleSlash);
    assert_eq!(v.description(), "expected //");
}

#[test]
fn test_syntax_violation_callback_lifetimes() {
    use url::SyntaxViolation::*;
    let violation = Cell::new(None);
    let vfn = |s| violation.set(Some(s));

    let url = Url::options()
        .syntax_violation_callback(Some(&vfn))
        .parse("http:////mozilla.org:42").unwrap();
    assert_eq!(url.port(), Some(42));
    assert_eq!(violation.take(), Some(ExpectedDoubleSlash));

    let url = Url::options()
        .syntax_violation_callback(Some(&vfn))
        .parse("http://mozilla.org\\path").unwrap();
    assert_eq!(url.path(), "/path");
    assert_eq!(violation.take(), Some(Backslash));
}

#[test]
fn test_options_reuse() {
    use url::SyntaxViolation::*;
    let violations = RefCell::new(Vec::new());
    let vfn = |v| violations.borrow_mut().push(v);

    let options = Url::options()
        .syntax_violation_callback(Some(&vfn));
    let url = options.parse("http:////mozilla.org").unwrap();

    let options = options.base_url(Some(&url));
    let url = options.parse("/sub\\path").unwrap();
    assert_eq!(url.as_str(), "http://mozilla.org/sub/path");
    assert_eq!(*violations.borrow(),
               vec!(ExpectedDoubleSlash, Backslash));
}
