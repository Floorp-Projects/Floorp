# Guide to upgrading from url 0.x to 1.x

* The fields of `Url` are now private because the `Url` constructor, parser,
  and setters maintain invariants that could be violated if you were to set the fields directly.
  Instead of accessing, for example, `url.scheme`, use the getter method, such as `url.scheme()`.
  Instead of assigning directly to a field, for example `url.scheme = "https".to_string()`,
  use the setter method, such as `url.set_scheme("https").unwrap()`.
  (Some setters validate the new value and return a `Result` that must be used).

* The methods of `Url` now return `&str` instead of `String`,
  thus reducing allocations and making serialization cheap.

* The `path()` method on `url::Url` instances used to return `Option<&[String]>`;
  now it returns `&str`.
  If you would like functionality more similar to the old behavior of `path()`,
  use `path_segments()` that returns `Option<str::Split<char>>`.

    Before upgrading:

    ```rust
    let issue_list_url = Url::parse(
         "https://github.com/rust-lang/rust/issues?labels=E-easy&state=open"
    ).unwrap();
    assert_eq!(issue_list_url.path(), Some(&["rust-lang".to_string(),
                                             "rust".to_string(),
                                             "issues".to_string()][..]));
    ```

    After upgrading:

    ```rust
    let issue_list_url = Url::parse(
         "https://github.com/rust-lang/rust/issues?labels=E-easy&state=open"
    ).unwrap();
    assert_eq!(issue_list_url.path(), "/rust-lang/rust/issues");
    assert_eq!(issue_list_url.path_segments().map(|c| c.collect::<Vec<_>>()),
               Some(vec!["rust-lang", "rust", "issues"]));
    ```

* The `path_mut()` method on `url::Url` instances that allowed modification of a URL's path
  has been replaced by `path_segments_mut()`.

    Before upgrading:

    ```rust
    let mut url = Url::parse("https://github.com/rust-lang/rust").unwrap();
    url.path_mut().unwrap().push("issues");
    ```

    After upgrading:

    ```rust
    let mut url = Url::parse("https://github.com/rust-lang/rust").unwrap();
    url.path_segments_mut().unwrap().push("issues");
    ```

* The `domain_mut()` method on `url::Url` instances that allowed modification of a URL's domain
  has been replaced by `set_host()` and `set_ip_host()`.

* The `host()` method on `url::Url` instances used to return `Option<&Host>`;
  now it returns `Option<Host<&str>>`.
  The `serialize_host()` method that returned `Option<String>`
  has been replaced by the `host_str()` method that returns `Option<&str>`.

* The `serialize()` method on `url::Url` instances that returned `String`
  has been replaced by an `as_str()` method that returns `&str`.

    Before upgrading:

    ```rust
    let this_document = Url::parse("http://servo.github.io/rust-url/url/index.html").unwrap();
    assert_eq!(this_document.serialize(), "http://servo.github.io/rust-url/url/index.html".to_string());
    ```

    After upgrading:

    ```rust
    let this_document = Url::parse("http://servo.github.io/rust-url/url/index.html").unwrap();
    assert_eq!(this_document.as_str(), "http://servo.github.io/rust-url/url/index.html");
    ```

* `url::UrlParser` has been replaced by `url::Url::parse()` and `url::Url::join()`.

    Before upgrading:

    ```rust
    let this_document = Url::parse("http://servo.github.io/rust-url/url/index.html").unwrap();
    let css_url = UrlParser::new().base_url(&this_document).parse("../main.css").unwrap();
    assert_eq!(css_url.serialize(), "http://servo.github.io/rust-url/main.css".to_string());
    ```

    After upgrading:

    ```rust
    let this_document = Url::parse("http://servo.github.io/rust-url/url/index.html").unwrap();
    let css_url = this_document.join("../main.css").unwrap();
    assert_eq!(css_url.as_str(), "http://servo.github.io/rust-url/main.css");
    ```

* `url::parse_path()` and `url::UrlParser::parse_path()` have been removed without replacement.
  As a workaround, you can give a base URL that you then ignore too `url::Url::parse()`.

    Before upgrading:

    ```rust
    let (path, query, fragment) = url::parse_path("/foo/bar/../baz?q=42").unwrap();
    assert_eq!(path, vec!["foo".to_string(), "baz".to_string()]);
    assert_eq!(query, Some("q=42".to_string()));
    assert_eq!(fragment, None);
    ```

    After upgrading:

    ```rust
    let base = Url::parse("http://example.com").unwrap();
    let with_path = base.join("/foo/bar/../baz?q=42").unwrap();
    assert_eq!(with_path.path(), "/foo/baz");
    assert_eq!(with_path.query(), Some("q=42"));
    assert_eq!(with_path.fragment(), None);
    ```

* The `url::form_urlencoded::serialize()` method
  has been replaced with the `url::form_urlencoded::Serializer` struct.
  Instead of calling `serialize()` with key/value pairs,
  create a new `Serializer` with a new string,
  call the `extend_pairs()` method on the `Serializer` instance with the key/value pairs as the argument,
  then call `finish()`.

    Before upgrading:

    ```rust
    let form = url::form_urlencoded::serialize(form.iter().map(|(k, v)| {
        (&k[..], &v[..])
    }));
    ```

    After upgrading:

    ```rust
    let form = url::form_urlencoded::Serializer::new(String::new()).extend_pairs(
        form.iter().map(|(k, v)| { (&k[..], &v[..]) })
    ).finish();
    ```

* The `set_query_from_pairs()` method on `url::Url` instances that took key/value pairs
  has been replaced with `query_pairs_mut()`, which allows you to modify the `url::Url`'s query pairs.

    Before upgrading:

    ```rust
    let mut url = Url::parse("https://duckduckgo.com/").unwrap();
    let pairs = vec![
        ("q", "test"),
        ("ia", "images"),
    ];
    url.set_query_from_pairs(pairs.iter().map(|&(k, v)| {
        (&k[..], &v[..])
    }));
    ```

    After upgrading:

    ```rust
    let mut url = Url::parse("https://duckduckgo.com/").unwrap();
    let pairs = vec![
        ("q", "test"),
        ("ia", "images"),
    ];
    url.query_pairs_mut().clear().extend_pairs(
      pairs.iter().map(|&(k, v)| { (&k[..], &v[..]) })
    );
    ```

* `url::SchemeData`, its variants `Relative` and `NonRelative`,
  and the struct `url::RelativeSchemeData` have been removed.
  Instead of matching on these variants
  to determine if you have a URL in a relative scheme such as HTTP
  versus a URL in a non-relative scheme as data,
  use the `cannot_be_a_base()` method to determine which kind you have.

    Before upgrading:

    ```rust
    match url.scheme_data {
        url::SchemeData::Relative(..) => {}
        url::SchemeData::NonRelative(..) => {
            return Err(human(format!("`{}` must have relative scheme \
                                      data: {}", field, url)))
        }
    }
    ```

    After upgrading:

    ```rust
    if url.cannot_be_a_base() {
        return Err(human(format!("`{}` must have relative scheme \
                                  data: {}", field, url)))
    }
    ```

* The functions `url::whatwg_scheme_type_mapper()`, the `SchemeType` enum,
  and the `scheme_type_mapper()` method on `url::UrlParser` instances have been removed.
  `SchemeType` had a method for getting the `default_port()`;
  to replicate this functionality, use the method `port_or_known_default()` on `url::Url` instances.
  The `port_or_default()` method on `url::Url` instances has been removed;
  use `port_or_known_default()` instead.

    Before upgrading:

    ```rust
    let port = match whatwg_scheme_type_mapper(&url.scheme) {
        SchemeType::Relative(port) => port,
        _ => return Err(format!("Invalid special scheme: `{}`",
                                raw_url.scheme)),
    };
    ```

    After upgrading:

    ```rust
    let port = match url.port_or_known_default() {
        Some(port) => port,
        _ => return Err(format!("Invalid special scheme: `{}`",
                                url.scheme())),
    };
    ```

* The following formatting utilities have been removed without replacement;
  look at their linked previous implementations
  if you would like to replicate the functionality in your code:
  * [`url::format::PathFormatter`](https://github.com/servo/rust-url/pull/176/commits/9e759f18726c8e1343162922b87163d4dd08fe3c#diff-0bb16ac13b75e9b568fa4aff61b0e71dL24)
  * [`url::format::UserInfoFormatter`](https://github.com/servo/rust-url/pull/176/commits/9e759f18726c8e1343162922b87163d4dd08fe3c#diff-0bb16ac13b75e9b568fa4aff61b0e71dL50)
  * [`url::format::UrlNoFragmentFormatter`](https://github.com/servo/rust-url/pull/176/commits/9e759f18726c8e1343162922b87163d4dd08fe3c#diff-0bb16ac13b75e9b568fa4aff61b0e71dL70)

* `url::percent_encoding::percent_decode()` used to have a return type of `Vec<u8>`;
  now it returns an iterator of decoded `u8` bytes that also implements `Into<Cow<u8>>`.
  Use `.into().to_owned()` to obtain a `Vec<u8>`.
  (`.collect()` also works but might not be as efficient.)

* The `url::percent_encoding::EncodeSet` struct and constant instances
  used with `url::percent_encoding::percent_encode()`
  have been changed to structs that implement the trait `url::percent_encoding::EncodeSet`.
  * `SIMPLE_ENCODE_SET`, `QUERY_ENCODE_SET`, `DEFAULT_ENCODE_SET`,
    and `USERINFO_ENCODE_SET` have the same behavior.
  * `USERNAME_ENCODE_SET` and `PASSWORD_ENCODE_SET` have been removed;
    use `USERINFO_ENCODE_SET` instead.
  * `HTTP_VALUE_ENCODE_SET` has been removed;
    an implementation of it in the new types can be found [in hyper's source](
    https://github.com/hyperium/hyper/blob/67436c5bf615cf5a55a71e32b788afef5985570e/src/header/parsing.rs#L131-L138)
    if you need to replicate this functionality in your code.
  * `FORM_URLENCODED_ENCODE_SET` has been removed;
    instead, use the functionality in `url::form_urlencoded`.
  * `PATH_SEGMENT_ENCODE_SET` has been added for use on '/'-separated path segments.

* `url::percent_encoding::percent_decode_to()` has been removed.
  Use `url::percent_encoding::percent_decode()` which returns an iterator.
  You can then use the iterator’s `collect()` method
  or give it to some data structure’s `extend()` method.
* A number of `ParseError` variants have changed.
  [See the documentation for the current set](http://servo.github.io/rust-url/url/enum.ParseError.html).
* `url::OpaqueOrigin::new()` and `url::Origin::UID(OpaqueOrigin)`
  have been replaced by `url::Origin::new_opaque()` and `url::Origin::Opaque(OpaqueOrigin)`, respectively.
