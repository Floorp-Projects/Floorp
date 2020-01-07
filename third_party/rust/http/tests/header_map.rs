use http::header::*;
use http::*;

#[test]
fn smoke() {
    let mut headers = HeaderMap::new();

    assert!(headers.get("hello").is_none());

    let name: HeaderName = "hello".parse().unwrap();

    match headers.entry(&name) {
        Entry::Vacant(e) => {
            e.insert("world".parse().unwrap());
        }
        _ => panic!(),
    }

    assert!(headers.get("hello").is_some());

    match headers.entry(&name) {
        Entry::Occupied(mut e) => {
            assert_eq!(e.get(), &"world");

            // Push another value
            e.append("zomg".parse().unwrap());

            let mut i = e.iter();

            assert_eq!(*i.next().unwrap(), "world");
            assert_eq!(*i.next().unwrap(), "zomg");
            assert!(i.next().is_none());
        }
        _ => panic!(),
    }
}

#[test]
#[should_panic]
fn reserve_over_capacity() {
    // See https://github.com/hyperium/http/issues/352
    let mut headers = HeaderMap::<u32>::with_capacity(32);
    headers.reserve(50_000); // over MAX_SIZE
}

#[test]
#[should_panic]
fn reserve_overflow() {
    // See https://github.com/hyperium/http/issues/352
    let mut headers = HeaderMap::<u32>::with_capacity(0);
    headers.reserve(std::usize::MAX); // next_power_of_two overflows
}

#[test]
fn drain() {
    let mut headers = HeaderMap::new();

    // Insert a single value
    let name: HeaderName = "hello".parse().unwrap();
    headers.insert(name, "world".parse().unwrap());

    {
        let mut iter = headers.drain();
        let (name, value) = iter.next().unwrap();
        assert_eq!(name.unwrap().as_str(), "hello");

        assert_eq!(value, "world");

        assert!(iter.next().is_none());
    }

    assert!(headers.is_empty());

    // Insert two sequential values
    headers.insert(
        "hello".parse::<HeaderName>().unwrap(),
        "world".parse().unwrap(),
    );
    headers.insert(
        "zomg".parse::<HeaderName>().unwrap(),
        "bar".parse().unwrap(),
    );
    headers.append(
        "hello".parse::<HeaderName>().unwrap(),
        "world2".parse().unwrap(),
    );

    // Drain...
    {
        let mut iter = headers.drain();

        let (name, value) = iter.next().unwrap();
        assert_eq!(name.unwrap().as_str(), "hello");
        assert_eq!(value, "world");

        let (name, value) = iter.next().unwrap();
        assert_eq!(name, None);
        assert_eq!(value, "world2");

        let (name, value) = iter.next().unwrap();
        assert_eq!(name.unwrap().as_str(), "zomg");
        assert_eq!(value, "bar");

        assert!(iter.next().is_none());
    }
}

#[test]
fn drain_drop_immediately() {
    // test mem::forgetting does not double-free

    let mut headers = HeaderMap::new();
    headers.insert("hello", "world".parse().unwrap());
    headers.insert("zomg", "bar".parse().unwrap());
    headers.append("hello", "world2".parse().unwrap());

    let iter = headers.drain();
    assert_eq!(iter.size_hint(), (2, Some(3)));
    // not consuming `iter`
}

#[test]
fn drain_forget() {
    // test mem::forgetting does not double-free

    let mut headers = HeaderMap::<HeaderValue>::new();
    headers.insert("hello", "world".parse().unwrap());
    headers.insert("zomg", "bar".parse().unwrap());

    assert_eq!(headers.len(), 2);

    {
        let mut iter = headers.drain();
        assert_eq!(iter.size_hint(), (2, Some(2)));
        let _ = iter.next().unwrap();
        std::mem::forget(iter);
    }

    assert_eq!(headers.len(), 0);
}

#[test]
fn drain_entry() {
    let mut headers = HeaderMap::new();

    headers.insert(
        "hello".parse::<HeaderName>().unwrap(),
        "world".parse().unwrap(),
    );
    headers.insert(
        "zomg".parse::<HeaderName>().unwrap(),
        "foo".parse().unwrap(),
    );
    headers.append(
        "hello".parse::<HeaderName>().unwrap(),
        "world2".parse().unwrap(),
    );
    headers.insert(
        "more".parse::<HeaderName>().unwrap(),
        "words".parse().unwrap(),
    );
    headers.append(
        "more".parse::<HeaderName>().unwrap(),
        "insertions".parse().unwrap(),
    );

    // Using insert
    {
        let mut e = match headers.entry("hello") {
            Entry::Occupied(e) => e,
            _ => panic!(),
        };

        let vals: Vec<_> = e.insert_mult("wat".parse().unwrap()).collect();
        assert_eq!(2, vals.len());
        assert_eq!(vals[0], "world");
        assert_eq!(vals[1], "world2");
    }
}

#[test]
fn eq() {
    let mut a = HeaderMap::new();
    let mut b = HeaderMap::new();

    assert_eq!(a, b);

    a.insert(
        "hello".parse::<HeaderName>().unwrap(),
        "world".parse().unwrap(),
    );
    assert_ne!(a, b);

    b.insert(
        "hello".parse::<HeaderName>().unwrap(),
        "world".parse().unwrap(),
    );
    assert_eq!(a, b);

    a.insert("foo".parse::<HeaderName>().unwrap(), "bar".parse().unwrap());
    a.append("foo".parse::<HeaderName>().unwrap(), "baz".parse().unwrap());
    assert_ne!(a, b);

    b.insert("foo".parse::<HeaderName>().unwrap(), "bar".parse().unwrap());
    assert_ne!(a, b);

    b.append("foo".parse::<HeaderName>().unwrap(), "baz".parse().unwrap());
    assert_eq!(a, b);

    a.append("a".parse::<HeaderName>().unwrap(), "a".parse().unwrap());
    a.append("a".parse::<HeaderName>().unwrap(), "b".parse().unwrap());
    b.append("a".parse::<HeaderName>().unwrap(), "b".parse().unwrap());
    b.append("a".parse::<HeaderName>().unwrap(), "a".parse().unwrap());

    assert_ne!(a, b);
}

#[test]
fn into_header_name() {
    let mut m = HeaderMap::new();
    m.insert(HOST, "localhost".parse().unwrap());
    m.insert(&ACCEPT, "*/*".parse().unwrap());
    m.insert("connection", "keep-alive".parse().unwrap());

    m.append(LOCATION, "/".parse().unwrap());
    m.append(&VIA, "bob".parse().unwrap());
    m.append("transfer-encoding", "chunked".parse().unwrap());

    assert_eq!(m.len(), 6);
}

#[test]
fn as_header_name() {
    let mut m = HeaderMap::new();
    let v: HeaderValue = "localhost".parse().unwrap();
    m.insert(HOST, v.clone());

    let expected = Some(&v);

    assert_eq!(m.get("host"), expected);
    assert_eq!(m.get(&HOST), expected);

    let s = String::from("host");
    assert_eq!(m.get(&s), expected);
    assert_eq!(m.get(s.as_str()), expected);
}

#[test]
fn insert_all_std_headers() {
    let mut m = HeaderMap::new();

    for (i, hdr) in STD.iter().enumerate() {
        m.insert(hdr.clone(), hdr.as_str().parse().unwrap());

        for j in 0..(i + 1) {
            assert_eq!(m[&STD[j]], STD[j].as_str());
        }

        if i != 0 {
            for j in (i + 1)..STD.len() {
                assert!(
                    m.get(&STD[j]).is_none(),
                    "contained {}; j={}",
                    STD[j].as_str(),
                    j
                );
            }
        }
    }
}

#[test]
fn insert_79_custom_std_headers() {
    let mut h = HeaderMap::new();
    let hdrs = custom_std(79);

    for (i, hdr) in hdrs.iter().enumerate() {
        h.insert(hdr.clone(), hdr.as_str().parse().unwrap());

        for j in 0..(i + 1) {
            assert_eq!(h[&hdrs[j]], hdrs[j].as_str());
        }

        for j in (i + 1)..hdrs.len() {
            assert!(h.get(&hdrs[j]).is_none());
        }
    }
}

#[test]
fn append_multiple_values() {
    let mut map = HeaderMap::new();

    map.append(header::CONTENT_TYPE, "json".parse().unwrap());
    map.append(header::CONTENT_TYPE, "html".parse().unwrap());
    map.append(header::CONTENT_TYPE, "xml".parse().unwrap());

    let vals = map
        .get_all(&header::CONTENT_TYPE)
        .iter()
        .collect::<Vec<_>>();

    assert_eq!(&vals, &[&"json", &"html", &"xml"]);
}

fn custom_std(n: usize) -> Vec<HeaderName> {
    (0..n)
        .map(|i| {
            let s = format!("{}-{}", STD[i % STD.len()].as_str(), i);
            s.parse().unwrap()
        })
        .collect()
}

const STD: &'static [HeaderName] = &[
    ACCEPT,
    ACCEPT_CHARSET,
    ACCEPT_ENCODING,
    ACCEPT_LANGUAGE,
    ACCEPT_RANGES,
    ACCESS_CONTROL_ALLOW_CREDENTIALS,
    ACCESS_CONTROL_ALLOW_HEADERS,
    ACCESS_CONTROL_ALLOW_METHODS,
    ACCESS_CONTROL_ALLOW_ORIGIN,
    ACCESS_CONTROL_EXPOSE_HEADERS,
    ACCESS_CONTROL_MAX_AGE,
    ACCESS_CONTROL_REQUEST_HEADERS,
    ACCESS_CONTROL_REQUEST_METHOD,
    AGE,
    ALLOW,
    ALT_SVC,
    AUTHORIZATION,
    CACHE_CONTROL,
    CONNECTION,
    CONTENT_DISPOSITION,
    CONTENT_ENCODING,
    CONTENT_LANGUAGE,
    CONTENT_LENGTH,
    CONTENT_LOCATION,
    CONTENT_RANGE,
    CONTENT_SECURITY_POLICY,
    CONTENT_SECURITY_POLICY_REPORT_ONLY,
    CONTENT_TYPE,
    COOKIE,
    DNT,
    DATE,
    ETAG,
    EXPECT,
    EXPIRES,
    FORWARDED,
    FROM,
    HOST,
    IF_MATCH,
    IF_MODIFIED_SINCE,
    IF_NONE_MATCH,
    IF_RANGE,
    IF_UNMODIFIED_SINCE,
    LAST_MODIFIED,
    LINK,
    LOCATION,
    MAX_FORWARDS,
    ORIGIN,
    PRAGMA,
    PROXY_AUTHENTICATE,
    PROXY_AUTHORIZATION,
    PUBLIC_KEY_PINS,
    PUBLIC_KEY_PINS_REPORT_ONLY,
    RANGE,
    REFERER,
    REFERRER_POLICY,
    RETRY_AFTER,
    SERVER,
    SET_COOKIE,
    STRICT_TRANSPORT_SECURITY,
    TE,
    TRAILER,
    TRANSFER_ENCODING,
    USER_AGENT,
    UPGRADE,
    UPGRADE_INSECURE_REQUESTS,
    VARY,
    VIA,
    WARNING,
    WWW_AUTHENTICATE,
    X_CONTENT_TYPE_OPTIONS,
    X_DNS_PREFETCH_CONTROL,
    X_FRAME_OPTIONS,
    X_XSS_PROTECTION,
];

#[test]
fn get_invalid() {
    let mut headers = HeaderMap::new();
    headers.insert("foo", "bar".parse().unwrap());
    assert!(headers.get("Evil\r\nKey").is_none());
}

#[test]
#[should_panic]
fn insert_invalid() {
    let mut headers = HeaderMap::new();
    headers.insert("evil\r\nfoo", "bar".parse().unwrap());
}

#[test]
fn value_htab() {
    // RFC 7230 Section 3.2:
    // > field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
    HeaderValue::from_static("hello\tworld");
    HeaderValue::from_str("hello\tworld").unwrap();
}
