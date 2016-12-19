// Copyright 2013-2014 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Unit tests

extern crate url;

use std::borrow::Cow;
use std::net::{Ipv4Addr, Ipv6Addr};
use std::path::{Path, PathBuf};
use url::{Host, Url, form_urlencoded};

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
fn from_str() {
    assert!("http://testing.com/this".parse::<Url>().is_ok());
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
    use std::hash::{Hash, Hasher, SipHasher};

    fn check_eq(a: &Url, b: &Url) {
        assert_eq!(a, b);

        let mut h1 = SipHasher::new();
        a.hash(&mut h1);
        let mut h2 = SipHasher::new();
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
    assert!(a != b);

    // Different host
    let a: Url = url("http://foo.com/");
    let b: Url = url("http://bar.com/");
    assert!(a != b);

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
        ("http://:@emptypass.com/", "http://:@emptypass.com/"),
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
/// https://github.com/servo/rust-url/issues/25
fn issue_25() {
    let filename = if cfg!(windows) { r"C:\run\pg.sock" } else { "/run/pg.sock" };
    let mut url = Url::from_file_path(filename).unwrap();
    url.assert_invariants();
    url.set_scheme("postgres").unwrap();
    url.assert_invariants();
    url.set_host(Some("")).unwrap();
    url.assert_invariants();
    url.set_username("me").unwrap();
    url.assert_invariants();
    let expected = format!("postgres://me@/{}run/pg.sock", if cfg!(windows) { "C:/" } else { "" });
    assert_eq!(url.as_str(), expected);
}

#[test]
/// https://github.com/servo/rust-url/issues/61
fn issue_61() {
    let mut url = Url::parse("http://mozilla.org").unwrap();
    url.set_scheme("https").unwrap();
    assert_eq!(url.port(), None);
    assert_eq!(url.port_or_known_default(), Some(443));
    url.assert_invariants();
}

#[test]
#[cfg(not(windows))]
/// https://github.com/servo/rust-url/issues/197
fn issue_197() {
    let mut url = Url::from_file_path("/").expect("Failed to parse path");
    url.assert_invariants();
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
    url.assert_invariants();
    url.path_segments_mut().unwrap().push("");
    url.assert_invariants();
    assert_eq!(url.to_string(), "http://localhost:6767/foo/bar/?a=b");
}

#[test]
/// https://github.com/servo/rust-url/issues/227
fn extend_query_pairs_then_mutate() {
    let mut url: Url = "http://localhost:6767/foo/bar".parse().unwrap();
    url.query_pairs_mut().extend_pairs(vec![ ("auth", "my-token") ].into_iter());
    url.assert_invariants();
    assert_eq!(url.to_string(), "http://localhost:6767/foo/bar?auth=my-token");
    url.path_segments_mut().unwrap().push("some_other_path");
    url.assert_invariants();
    assert_eq!(url.to_string(), "http://localhost:6767/foo/bar/some_other_path?auth=my-token");
}

#[test]
/// https://github.com/servo/rust-url/issues/222
fn append_empty_segment_then_mutate() {
    let mut url: Url = "http://localhost:6767/foo/bar?a=b".parse().unwrap();
    url.assert_invariants();
    url.path_segments_mut().unwrap().push("").pop();
    url.assert_invariants();
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
}
