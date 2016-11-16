// Copyright 2013-2015 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/*!

rust-url is an implementation of the [URL Standard](http://url.spec.whatwg.org/)
for the [Rust](http://rust-lang.org/) programming language.

It builds with [Cargo](http://crates.io/).
To use it in your project, add this to your `Cargo.toml` file:

```Cargo
[dependencies.url]
git = "https://github.com/servo/rust-url"
```

Supporting encodings other than UTF-8 in query strings is an optional feature
that requires [rust-encoding](https://github.com/lifthrasiir/rust-encoding)
and is off by default.
You can enable it with
[Cargo’s *features* mechanism](http://doc.crates.io/manifest.html#the-[features]-section):

```Cargo
[dependencies.url]
git = "https://github.com/servo/rust-url"
features = ["query_encoding"]
```

… or by passing `--cfg 'feature="query_encoding"'` to rustc.


# URL parsing and data structures

First, URL parsing may fail for various reasons and therefore returns a `Result`.

```
use url::{Url, ParseError};

assert!(Url::parse("http://[:::1]") == Err(ParseError::InvalidIpv6Address))
```

Let’s parse a valid URL and look at its components.

```
use url::{Url, Host};

let issue_list_url = Url::parse(
    "https://github.com/rust-lang/rust/issues?labels=E-easy&state=open"
).unwrap();


assert!(issue_list_url.scheme() == "https");
assert!(issue_list_url.username() == "");
assert!(issue_list_url.password() == None);
assert!(issue_list_url.host_str() == Some("github.com"));
assert!(issue_list_url.host() == Some(Host::Domain("github.com")));
assert!(issue_list_url.port() == None);
assert!(issue_list_url.path() == "/rust-lang/rust/issues");
assert!(issue_list_url.path_segments().map(|c| c.collect::<Vec<_>>()) ==
        Some(vec!["rust-lang", "rust", "issues"]));
assert!(issue_list_url.query() == Some("labels=E-easy&state=open"));
assert!(issue_list_url.fragment() == None);
assert!(!issue_list_url.cannot_be_a_base());
```

Some URLs are said to be *cannot-be-a-base*:
they don’t have a username, password, host, or port,
and their "path" is an arbitrary string rather than slash-separated segments:

```
use url::Url;

let data_url = Url::parse("data:text/plain,Hello?World#").unwrap();

assert!(data_url.cannot_be_a_base());
assert!(data_url.scheme() == "data");
assert!(data_url.path() == "text/plain,Hello");
assert!(data_url.path_segments().is_none());
assert!(data_url.query() == Some("World"));
assert!(data_url.fragment() == Some(""));
```


# Base URL

Many contexts allow URL *references* that can be relative to a *base URL*:

```html
<link rel="stylesheet" href="../main.css">
```

Since parsed URL are absolute, giving a base is required for parsing relative URLs:

```
use url::{Url, ParseError};

assert!(Url::parse("../main.css") == Err(ParseError::RelativeUrlWithoutBase))
```

Use the `join` method on an `Url` to use it as a base URL:

```
use url::Url;

let this_document = Url::parse("http://servo.github.io/rust-url/url/index.html").unwrap();
let css_url = this_document.join("../main.css").unwrap();
assert_eq!(css_url.as_str(), "http://servo.github.io/rust-url/main.css")
*/

#[cfg(feature="rustc-serialize")] extern crate rustc_serialize;
#[macro_use] extern crate matches;
#[cfg(feature="serde")] extern crate serde;
#[cfg(feature="heapsize")] #[macro_use] extern crate heapsize;

pub extern crate idna;

use encoding::EncodingOverride;
#[cfg(feature = "heapsize")] use heapsize::HeapSizeOf;
use host::HostInternal;
use parser::{Parser, Context, SchemeType, to_u32};
use percent_encoding::{PATH_SEGMENT_ENCODE_SET, USERINFO_ENCODE_SET,
                       percent_encode, percent_decode, utf8_percent_encode};
use std::cmp;
use std::fmt::{self, Write};
use std::hash;
use std::io;
use std::mem;
use std::net::{ToSocketAddrs, IpAddr};
use std::ops::{Range, RangeFrom, RangeTo};
use std::path::{Path, PathBuf};
use std::str;

pub use origin::{Origin, OpaqueOrigin};
pub use host::{Host, HostAndPort, SocketAddrs};
pub use path_segments::PathSegmentsMut;
pub use parser::ParseError;
pub use slicing::Position;

mod encoding;
mod host;
mod origin;
mod path_segments;
mod parser;
mod slicing;

pub mod form_urlencoded;
pub mod percent_encoding;
pub mod quirks;

/// A parsed URL record.
#[derive(Clone)]
pub struct Url {
    /// Syntax in pseudo-BNF:
    ///
    ///   url = scheme ":" [ hierarchical | non-hierarchical ] [ "?" query ]? [ "#" fragment ]?
    ///   non-hierarchical = non-hierarchical-path
    ///   non-hierarchical-path = /* Does not start with "/" */
    ///   hierarchical = authority? hierarchical-path
    ///   authority = "//" userinfo? host [ ":" port ]?
    ///   userinfo = username [ ":" password ]? "@"
    ///   hierarchical-path = [ "/" path-segment ]+
    serialization: String,

    // Components
    scheme_end: u32,  // Before ':'
    username_end: u32,  // Before ':' (if a password is given) or '@' (if not)
    host_start: u32,
    host_end: u32,
    host: HostInternal,
    port: Option<u16>,
    path_start: u32,  // Before initial '/', if any
    query_start: Option<u32>,  // Before '?', unlike Position::QueryStart
    fragment_start: Option<u32>,  // Before '#', unlike Position::FragmentStart
}

#[cfg(feature = "heapsize")]
impl HeapSizeOf for Url {
    fn heap_size_of_children(&self) -> usize {
        self.serialization.heap_size_of_children()
    }
}

/// Full configuration for the URL parser.
#[derive(Copy, Clone)]
pub struct ParseOptions<'a> {
    base_url: Option<&'a Url>,
    encoding_override: encoding::EncodingOverride,
    log_syntax_violation: Option<&'a Fn(&'static str)>,
}

impl<'a> ParseOptions<'a> {
    /// Change the base URL
    pub fn base_url(mut self, new: Option<&'a Url>) -> Self {
        self.base_url = new;
        self
    }

    /// Override the character encoding of query strings.
    /// This is a legacy concept only relevant for HTML.
    #[cfg(feature = "query_encoding")]
    pub fn encoding_override(mut self, new: Option<encoding::EncodingRef>) -> Self {
        self.encoding_override = EncodingOverride::from_opt_encoding(new).to_output_encoding();
        self
    }

    /// Call the provided function or closure on non-fatal parse errors.
    pub fn log_syntax_violation(mut self, new: Option<&'a Fn(&'static str)>) -> Self {
        self.log_syntax_violation = new;
        self
    }

    /// Parse an URL string with the configuration so far.
    pub fn parse(self, input: &str) -> Result<Url, ::ParseError> {
        Parser {
            serialization: String::with_capacity(input.len()),
            base_url: self.base_url,
            query_encoding_override: self.encoding_override,
            log_syntax_violation: self.log_syntax_violation,
            context: Context::UrlParser,
        }.parse_url(input)
    }
}

impl Url {
    /// Parse an absolute URL from a string.
    #[inline]
    pub fn parse(input: &str) -> Result<Url, ::ParseError> {
        Url::options().parse(input)
    }

    /// Parse a string as an URL, with this URL as the base URL.
    #[inline]
    pub fn join(&self, input: &str) -> Result<Url, ::ParseError> {
        Url::options().base_url(Some(self)).parse(input)
    }

    /// Return a default `ParseOptions` that can fully configure the URL parser.
    pub fn options<'a>() -> ParseOptions<'a> {
        ParseOptions {
            base_url: None,
            encoding_override: EncodingOverride::utf8(),
            log_syntax_violation: None,
        }
    }

    /// Return the serialization of this URL.
    ///
    /// This is fast since that serialization is already stored in the `Url` struct.
    #[inline]
    pub fn as_str(&self) -> &str {
        &self.serialization
    }

    /// Return the serialization of this URL.
    ///
    /// This consumes the `Url` and takes ownership of the `String` stored in it.
    #[inline]
    pub fn into_string(self) -> String {
        self.serialization
    }

    /// For internal testing, not part of the public API.
    ///
    /// Methods of the `Url` struct assume a number of invariants.
    /// This checks each of these invariants and panic if one is not met.
    /// This is for testing rust-url itself.
    #[doc(hidden)]
    pub fn assert_invariants(&self) {
        macro_rules! assert {
            ($x: expr) => {
                if !$x {
                    panic!("!( {} ) for URL {:?}", stringify!($x), self.serialization)
                }
            }
        }

        macro_rules! assert_eq {
            ($a: expr, $b: expr) => {
                {
                    let a = $a;
                    let b = $b;
                    if a != b {
                        panic!("{:?} != {:?} ({} != {}) for URL {:?}",
                               a, b, stringify!($a), stringify!($b), self.serialization)
                    }
                }
            }
        }

        assert!(self.scheme_end >= 1);
        assert!(matches!(self.byte_at(0), b'a'...b'z' | b'A'...b'Z'));
        assert!(self.slice(1..self.scheme_end).chars()
                .all(|c| matches!(c, 'a'...'z' | 'A'...'Z' | '0'...'9' | '+' | '-' | '.')));
        assert_eq!(self.byte_at(self.scheme_end), b':');

        if self.slice(self.scheme_end + 1 ..).starts_with("//") {
            // URL with authority
            match self.byte_at(self.username_end) {
                b':' => {
                    assert!(self.host_start >= self.username_end + 2);
                    assert_eq!(self.byte_at(self.host_start - 1), b'@');
                }
                b'@' => assert!(self.host_start == self.username_end + 1),
                _ => assert_eq!(self.username_end, self.scheme_end + 3),
            }
            assert!(self.host_start >= self.username_end);
            assert!(self.host_end >= self.host_start);
            let host_str = self.slice(self.host_start..self.host_end);
            match self.host {
                HostInternal::None => assert_eq!(host_str, ""),
                HostInternal::Ipv4(address) => assert_eq!(host_str, address.to_string()),
                HostInternal::Ipv6(address) => {
                    let h: Host<String> = Host::Ipv6(address);
                    assert_eq!(host_str, h.to_string())
                }
                HostInternal::Domain => {
                    if SchemeType::from(self.scheme()).is_special() {
                        assert!(!host_str.is_empty())
                    }
                }
            }
            if self.path_start == self.host_end {
                assert_eq!(self.port, None);
            } else {
                assert_eq!(self.byte_at(self.host_end), b':');
                let port_str = self.slice(self.host_end + 1..self.path_start);
                assert_eq!(self.port, Some(port_str.parse::<u16>().expect("Couldn't parse port?")));
            }
            assert_eq!(self.byte_at(self.path_start), b'/');
        } else {
            // Anarchist URL (no authority)
            assert_eq!(self.username_end, self.scheme_end + 1);
            assert_eq!(self.host_start, self.scheme_end + 1);
            assert_eq!(self.host_end, self.scheme_end + 1);
            assert_eq!(self.host, HostInternal::None);
            assert_eq!(self.port, None);
            assert_eq!(self.path_start, self.scheme_end + 1);
        }
        if let Some(start) = self.query_start {
            assert!(start > self.path_start);
            assert_eq!(self.byte_at(start), b'?');
        }
        if let Some(start) = self.fragment_start {
            assert!(start > self.path_start);
            assert_eq!(self.byte_at(start), b'#');
        }
        if let (Some(query_start), Some(fragment_start)) = (self.query_start, self.fragment_start) {
            assert!(fragment_start > query_start);
        }

        let other = Url::parse(self.as_str()).expect("Failed to parse myself?");
        assert_eq!(&self.serialization, &other.serialization);
        assert_eq!(self.scheme_end, other.scheme_end);
        assert_eq!(self.username_end, other.username_end);
        assert_eq!(self.host_start, other.host_start);
        assert_eq!(self.host_end, other.host_end);
        assert!(self.host == other.host ||
                // XXX No host round-trips to empty host.
                // See https://github.com/whatwg/url/issues/79
                (self.host_str(), other.host_str()) == (None, Some("")));
        assert_eq!(self.port, other.port);
        assert_eq!(self.path_start, other.path_start);
        assert_eq!(self.query_start, other.query_start);
        assert_eq!(self.fragment_start, other.fragment_start);
    }

    /// Return the origin of this URL (https://url.spec.whatwg.org/#origin)
    ///
    /// Note: this returns an opaque origin for `file:` URLs, which causes
    /// `url.origin() != url.origin()`.
    ///
    /// # Examples
    ///
    /// URL with `ftp` scheme:
    ///
    /// ```rust
    /// use url::{Host, Origin, Url};
    ///
    /// let url = Url::parse("ftp://example.com/foo").unwrap();
    /// assert_eq!(url.origin(),
    ///            Origin::Tuple("ftp".into(),
    ///                          Host::Domain("example.com".into()),
    ///                          21));
    /// ```
    ///
    /// URL with `blob` scheme:
    ///
    /// ```rust
    /// use url::{Host, Origin, Url};
    ///
    /// let url = Url::parse("blob:https://example.com/foo").unwrap();
    /// assert_eq!(url.origin(),
    ///            Origin::Tuple("https".into(),
    ///                          Host::Domain("example.com".into()),
    ///                          443));
    /// ```
    ///
    /// URL with `file` scheme:
    ///
    /// ```rust
    /// use url::{Host, Origin, Url};
    ///
    /// let url = Url::parse("file:///tmp/foo").unwrap();
    /// assert!(!url.origin().is_tuple());
    ///
    /// let other_url = Url::parse("file:///tmp/foo").unwrap();
    /// assert!(url.origin() != other_url.origin());
    /// ```
    ///
    /// URL with other scheme:
    ///
    /// ```rust
    /// use url::{Host, Origin, Url};
    ///
    /// let url = Url::parse("foo:bar").unwrap();
    /// assert!(!url.origin().is_tuple());
    /// ```
    #[inline]
    pub fn origin(&self) -> Origin {
        origin::url_origin(self)
    }

    /// Return the scheme of this URL, lower-cased, as an ASCII string without the ':' delimiter.
    ///
    /// # Examples
    ///
    /// ```
    /// use url::Url;
    ///
    /// let url = Url::parse("file:///tmp/foo").unwrap();
    /// assert_eq!(url.scheme(), "file");
    /// ```
    #[inline]
    pub fn scheme(&self) -> &str {
        self.slice(..self.scheme_end)
    }

    /// Return whether the URL has an 'authority',
    /// which can contain a username, password, host, and port number.
    ///
    /// URLs that do *not* are either path-only like `unix:/run/foo.socket`
    /// or cannot-be-a-base like `data:text/plain,Stuff`.
    #[inline]
    pub fn has_authority(&self) -> bool {
        debug_assert!(self.byte_at(self.scheme_end) == b':');
        self.slice(self.scheme_end..).starts_with("://")
    }

    /// Return whether this URL is a cannot-be-a-base URL,
    /// meaning that parsing a relative URL string with this URL as the base will return an error.
    ///
    /// This is the case if the scheme and `:` delimiter are not followed by a `/` slash,
    /// as is typically the case of `data:` and `mailto:` URLs.
    #[inline]
    pub fn cannot_be_a_base(&self) -> bool {
        self.byte_at(self.path_start) != b'/'
    }

    /// Return the username for this URL (typically the empty string)
    /// as a percent-encoded ASCII string.
    ///
    /// # Examples
    ///
    /// ```
    /// use url::Url;
    ///
    /// let url = Url::parse("ftp://rms@example.com").unwrap();
    /// assert_eq!(url.username(), "rms");
    ///
    /// let url = Url::parse("ftp://:secret123@example.com").unwrap();
    /// assert_eq!(url.username(), "");
    ///
    /// let url = Url::parse("https://example.com").unwrap();
    /// assert_eq!(url.username(), "");
    /// ```
    pub fn username(&self) -> &str {
        if self.has_authority() {
            self.slice(self.scheme_end + ("://".len() as u32)..self.username_end)
        } else {
            ""
        }
    }

    /// Return the password for this URL, if any, as a percent-encoded ASCII string.
    ///
    /// # Examples
    ///
    /// ```
    /// use url::Url;
    ///
    /// let url = Url::parse("ftp://rms:secret123@example.com").unwrap();
    /// assert_eq!(url.password(), Some("secret123"));
    ///
    /// let url = Url::parse("ftp://:secret123@example.com").unwrap();
    /// assert_eq!(url.password(), Some("secret123"));
    ///
    /// let url = Url::parse("ftp://rms@example.com").unwrap();
    /// assert_eq!(url.password(), None);
    ///
    /// let url = Url::parse("https://example.com").unwrap();
    /// assert_eq!(url.password(), None);
    /// ```
    pub fn password(&self) -> Option<&str> {
        // This ':' is not the one marking a port number since a host can not be empty.
        // (Except for file: URLs, which do not have port numbers.)
        if self.has_authority() && self.byte_at(self.username_end) == b':' {
            debug_assert!(self.byte_at(self.host_start - 1) == b'@');
            Some(self.slice(self.username_end + 1..self.host_start - 1))
        } else {
            None
        }
    }

    /// Equivalent to `url.host().is_some()`.
    pub fn has_host(&self) -> bool {
        !matches!(self.host, HostInternal::None)
    }

    /// Return the string representation of the host (domain or IP address) for this URL, if any.
    ///
    /// Non-ASCII domains are punycode-encoded per IDNA.
    /// IPv6 addresses are given between `[` and `]` brackets.
    ///
    /// Cannot-be-a-base URLs (typical of `data:` and `mailto:`) and some `file:` URLs
    /// don’t have a host.
    ///
    /// See also the `host` method.
    pub fn host_str(&self) -> Option<&str> {
        if self.has_host() {
            Some(self.slice(self.host_start..self.host_end))
        } else {
            None
        }
    }

    /// Return the parsed representation of the host for this URL.
    /// Non-ASCII domain labels are punycode-encoded per IDNA.
    ///
    /// Cannot-be-a-base URLs (typical of `data:` and `mailto:`) and some `file:` URLs
    /// don’t have a host.
    ///
    /// See also the `host_str` method.
    pub fn host(&self) -> Option<Host<&str>> {
        match self.host {
            HostInternal::None => None,
            HostInternal::Domain => Some(Host::Domain(self.slice(self.host_start..self.host_end))),
            HostInternal::Ipv4(address) => Some(Host::Ipv4(address)),
            HostInternal::Ipv6(address) => Some(Host::Ipv6(address)),
        }
    }

    /// If this URL has a host and it is a domain name (not an IP address), return it.
    pub fn domain(&self) -> Option<&str> {
        match self.host {
            HostInternal::Domain => Some(self.slice(self.host_start..self.host_end)),
            _ => None,
        }
    }

    /// Return the port number for this URL, if any.
    #[inline]
    pub fn port(&self) -> Option<u16> {
        self.port
    }

    /// Return the port number for this URL, or the default port number if it is known.
    ///
    /// This method only knows the default port number
    /// of the `http`, `https`, `ws`, `wss`, `ftp`, and `gopher` schemes.
    ///
    /// For URLs in these schemes, this method always returns `Some(_)`.
    /// For other schemes, it is the same as `Url::port()`.
    #[inline]
    pub fn port_or_known_default(&self) -> Option<u16> {
        self.port.or_else(|| parser::default_port(self.scheme()))
    }

    /// If the URL has a host, return something that implements `ToSocketAddrs`.
    ///
    /// If the URL has no port number and the scheme’s default port number is not known
    /// (see `Url::port_or_known_default`),
    /// the closure is called to obtain a port number.
    /// Typically, this closure can match on the result `Url::scheme`
    /// to have per-scheme default port numbers,
    /// and panic for schemes it’s not prepared to handle.
    /// For example:
    ///
    /// ```rust
    /// # use url::Url;
    /// # use std::net::TcpStream;
    /// # use std::io;
    ///
    /// fn connect(url: &Url) -> io::Result<TcpStream> {
    ///     TcpStream::connect(try!(url.with_default_port(default_port)))
    /// }
    ///
    /// fn default_port(url: &Url) -> Result<u16, ()> {
    ///     match url.scheme() {
    ///         "git" => Ok(9418),
    ///         "git+ssh" => Ok(22),
    ///         "git+https" => Ok(443),
    ///         "git+http" => Ok(80),
    ///         _ => Err(()),
    ///     }
    /// }
    /// ```
    pub fn with_default_port<F>(&self, f: F) -> io::Result<HostAndPort<&str>>
    where F: FnOnce(&Url) -> Result<u16, ()> {
        Ok(HostAndPort {
            host: try!(self.host()
                           .ok_or(())
                           .or_else(|()| io_error("URL has no host"))),
            port: try!(self.port_or_known_default()
                           .ok_or(())
                           .or_else(|()| f(self))
                           .or_else(|()| io_error("URL has no port number")))
        })
    }

    /// Return the path for this URL, as a percent-encoded ASCII string.
    /// For cannot-be-a-base URLs, this is an arbitrary string that doesn’t start with '/'.
    /// For other URLs, this starts with a '/' slash
    /// and continues with slash-separated path segments.
    pub fn path(&self) -> &str {
        match (self.query_start, self.fragment_start) {
            (None, None) => self.slice(self.path_start..),
            (Some(next_component_start), _) |
            (None, Some(next_component_start)) => {
                self.slice(self.path_start..next_component_start)
            }
        }
    }

    /// Unless this URL is cannot-be-a-base,
    /// return an iterator of '/' slash-separated path segments,
    /// each as a percent-encoded ASCII string.
    ///
    /// Return `None` for cannot-be-a-base URLs.
    ///
    /// When `Some` is returned, the iterator always contains at least one string
    /// (which may be empty).
    pub fn path_segments(&self) -> Option<str::Split<char>> {
        let path = self.path();
        if path.starts_with('/') {
            Some(path[1..].split('/'))
        } else {
            None
        }
    }

    /// Return this URL’s query string, if any, as a percent-encoded ASCII string.
    pub fn query(&self) -> Option<&str> {
        match (self.query_start, self.fragment_start) {
            (None, _) => None,
            (Some(query_start), None) => {
                debug_assert!(self.byte_at(query_start) == b'?');
                Some(self.slice(query_start + 1..))
            }
            (Some(query_start), Some(fragment_start)) => {
                debug_assert!(self.byte_at(query_start) == b'?');
                Some(self.slice(query_start + 1..fragment_start))
            }
        }
    }

    /// Parse the URL’s query string, if any, as `application/x-www-form-urlencoded`
    /// and return an iterator of (key, value) pairs.
    #[inline]
    pub fn query_pairs(&self) -> form_urlencoded::Parse {
        form_urlencoded::parse(self.query().unwrap_or("").as_bytes())
    }

    /// Return this URL’s fragment identifier, if any.
    ///
    /// **Note:** the parser did *not* percent-encode this component,
    /// but the input may have been percent-encoded already.
    pub fn fragment(&self) -> Option<&str> {
        self.fragment_start.map(|start| {
            debug_assert!(self.byte_at(start) == b'#');
            self.slice(start + 1..)
        })
    }

    fn mutate<F: FnOnce(&mut Parser) -> R, R>(&mut self, f: F) -> R {
        let mut parser = Parser::for_setter(mem::replace(&mut self.serialization, String::new()));
        let result = f(&mut parser);
        self.serialization = parser.serialization;
        result
    }

    /// Change this URL’s fragment identifier.
    pub fn set_fragment(&mut self, fragment: Option<&str>) {
        // Remove any previous fragment
        if let Some(start) = self.fragment_start {
            debug_assert!(self.byte_at(start) == b'#');
            self.serialization.truncate(start as usize);
        }
        // Write the new one
        if let Some(input) = fragment {
            self.fragment_start = Some(to_u32(self.serialization.len()).unwrap());
            self.serialization.push('#');
            self.mutate(|parser| parser.parse_fragment(parser::Input::new(input)))
        } else {
            self.fragment_start = None
        }
    }

    fn take_fragment(&mut self) -> Option<String> {
        self.fragment_start.take().map(|start| {
            debug_assert!(self.byte_at(start) == b'#');
            let fragment = self.slice(start + 1..).to_owned();
            self.serialization.truncate(start as usize);
            fragment
        })
    }

    fn restore_already_parsed_fragment(&mut self, fragment: Option<String>) {
        if let Some(ref fragment) = fragment {
            assert!(self.fragment_start.is_none());
            self.fragment_start = Some(to_u32(self.serialization.len()).unwrap());
            self.serialization.push('#');
            self.serialization.push_str(fragment);
        }
    }

    /// Change this URL’s query string.
    pub fn set_query(&mut self, query: Option<&str>) {
        let fragment = self.take_fragment();

        // Remove any previous query
        if let Some(start) = self.query_start.take() {
            debug_assert!(self.byte_at(start) == b'?');
            self.serialization.truncate(start as usize);
        }
        // Write the new query, if any
        if let Some(input) = query {
            self.query_start = Some(to_u32(self.serialization.len()).unwrap());
            self.serialization.push('?');
            let scheme_end = self.scheme_end;
            self.mutate(|parser| parser.parse_query(scheme_end, parser::Input::new(input)));
        }

        self.restore_already_parsed_fragment(fragment);
    }

    /// Manipulate this URL’s query string, viewed as a sequence of name/value pairs
    /// in `application/x-www-form-urlencoded` syntax.
    ///
    /// The return value has a method-chaining API:
    ///
    /// ```rust
    /// # use url::Url;
    /// let mut url = Url::parse("https://example.net?lang=fr#nav").unwrap();
    /// assert_eq!(url.query(), Some("lang=fr"));
    ///
    /// url.query_pairs_mut().append_pair("foo", "bar");
    /// assert_eq!(url.query(), Some("lang=fr&foo=bar"));
    /// assert_eq!(url.as_str(), "https://example.net/?lang=fr&foo=bar#nav");
    ///
    /// url.query_pairs_mut()
    ///     .clear()
    ///     .append_pair("foo", "bar & baz")
    ///     .append_pair("saisons", "\u{00C9}t\u{00E9}+hiver");
    /// assert_eq!(url.query(), Some("foo=bar+%26+baz&saisons=%C3%89t%C3%A9%2Bhiver"));
    /// assert_eq!(url.as_str(),
    ///            "https://example.net/?foo=bar+%26+baz&saisons=%C3%89t%C3%A9%2Bhiver#nav");
    /// ```
    ///
    /// Note: `url.query_pairs_mut().clear();` is equivalent to `url.set_query(Some(""))`,
    /// not `url.set_query(None)`.
    ///
    /// The state of `Url` is unspecified if this return value is leaked without being dropped.
    pub fn query_pairs_mut(&mut self) -> form_urlencoded::Serializer<UrlQuery> {
        let fragment = self.take_fragment();

        let query_start;
        if let Some(start) = self.query_start {
            debug_assert!(self.byte_at(start) == b'?');
            query_start = start as usize;
        } else {
            query_start = self.serialization.len();
            self.query_start = Some(to_u32(query_start).unwrap());
            self.serialization.push('?');
        }

        let query = UrlQuery { url: self, fragment: fragment };
        form_urlencoded::Serializer::for_suffix(query, query_start + "?".len())
    }

    fn take_after_path(&mut self) -> String {
        match (self.query_start, self.fragment_start) {
            (Some(i), _) | (None, Some(i)) => {
                let after_path = self.slice(i..).to_owned();
                self.serialization.truncate(i as usize);
                after_path
            },
            (None, None) => String::new(),
        }
    }

    /// Change this URL’s path.
    pub fn set_path(&mut self, mut path: &str) {
        let after_path = self.take_after_path();
        let old_after_path_pos = to_u32(self.serialization.len()).unwrap();
        let cannot_be_a_base = self.cannot_be_a_base();
        let scheme_type = SchemeType::from(self.scheme());
        self.serialization.truncate(self.path_start as usize);
        self.mutate(|parser| {
            if cannot_be_a_base {
                if path.starts_with('/') {
                    parser.serialization.push_str("%2F");
                    path = &path[1..];
                }
                parser.parse_cannot_be_a_base_path(parser::Input::new(path));
            } else {
                let mut has_host = true;  // FIXME
                parser.parse_path_start(scheme_type, &mut has_host, parser::Input::new(path));
            }
        });
        self.restore_after_path(old_after_path_pos, &after_path);
    }

    /// Return an object with methods to manipulate this URL’s path segments.
    ///
    /// Return `Err(())` if this URL is cannot-be-a-base.
    pub fn path_segments_mut(&mut self) -> Result<PathSegmentsMut, ()> {
        if self.cannot_be_a_base() {
            Err(())
        } else {
            Ok(path_segments::new(self))
        }
    }

    fn restore_after_path(&mut self, old_after_path_position: u32, after_path: &str) {
        let new_after_path_position = to_u32(self.serialization.len()).unwrap();
        let adjust = |index: &mut u32| {
            *index -= old_after_path_position;
            *index += new_after_path_position;
        };
        if let Some(ref mut index) = self.query_start { adjust(index) }
        if let Some(ref mut index) = self.fragment_start { adjust(index) }
        self.serialization.push_str(after_path)
    }

    /// Change this URL’s port number.
    ///
    /// If this URL is cannot-be-a-base, does not have a host, or has the `file` scheme;
    /// do nothing and return `Err`.
    pub fn set_port(&mut self, mut port: Option<u16>) -> Result<(), ()> {
        if !self.has_host() || self.scheme() == "file" {
            return Err(())
        }
        if port.is_some() && port == parser::default_port(self.scheme()) {
            port = None
        }
        self.set_port_internal(port);
        Ok(())
    }

    fn set_port_internal(&mut self, port: Option<u16>) {
        match (self.port, port) {
            (None, None) => {}
            (Some(_), None) => {
                self.serialization.drain(self.host_end as usize .. self.path_start as usize);
                let offset = self.path_start - self.host_end;
                self.path_start = self.host_end;
                if let Some(ref mut index) = self.query_start { *index -= offset }
                if let Some(ref mut index) = self.fragment_start { *index -= offset }
            }
            (Some(old), Some(new)) if old == new => {}
            (_, Some(new)) => {
                let path_and_after = self.slice(self.path_start..).to_owned();
                self.serialization.truncate(self.host_end as usize);
                write!(&mut self.serialization, ":{}", new).unwrap();
                let old_path_start = self.path_start;
                let new_path_start = to_u32(self.serialization.len()).unwrap();
                self.path_start = new_path_start;
                let adjust = |index: &mut u32| {
                    *index -= old_path_start;
                    *index += new_path_start;
                };
                if let Some(ref mut index) = self.query_start { adjust(index) }
                if let Some(ref mut index) = self.fragment_start { adjust(index) }
                self.serialization.push_str(&path_and_after);
            }
        }
        self.port = port;
    }

    /// Change this URL’s host.
    ///
    /// If this URL is cannot-be-a-base or there is an error parsing the given `host`,
    /// do nothing and return `Err`.
    ///
    /// Removing the host (calling this with `None`)
    /// will also remove any username, password, and port number.
    pub fn set_host(&mut self, host: Option<&str>) -> Result<(), ParseError> {
        if self.cannot_be_a_base() {
            return Err(ParseError::SetHostOnCannotBeABaseUrl)
        }

        if let Some(host) = host {
            self.set_host_internal(try!(Host::parse(host)), None)
        } else if self.has_host() {
            debug_assert!(self.byte_at(self.scheme_end) == b':');
            debug_assert!(self.byte_at(self.path_start) == b'/');
            let new_path_start = self.scheme_end + 1;
            self.serialization.drain(self.path_start as usize..new_path_start as usize);
            let offset = self.path_start - new_path_start;
            self.path_start = new_path_start;
            self.username_end = new_path_start;
            self.host_start = new_path_start;
            self.host_end = new_path_start;
            self.port = None;
            if let Some(ref mut index) = self.query_start { *index -= offset }
            if let Some(ref mut index) = self.fragment_start { *index -= offset }
        }
        Ok(())
    }

    /// opt_new_port: None means leave unchanged, Some(None) means remove any port number.
    fn set_host_internal(&mut self, host: Host<String>, opt_new_port: Option<Option<u16>>) {
        let old_suffix_pos = if opt_new_port.is_some() { self.path_start } else { self.host_end };
        let suffix = self.slice(old_suffix_pos..).to_owned();
        self.serialization.truncate(self.host_start as usize);
        if !self.has_authority() {
            debug_assert!(self.slice(self.scheme_end..self.host_start) == ":");
            debug_assert!(self.username_end == self.host_start);
            self.serialization.push('/');
            self.serialization.push('/');
            self.username_end += 2;
            self.host_start += 2;
        }
        write!(&mut self.serialization, "{}", host).unwrap();
        self.host_end = to_u32(self.serialization.len()).unwrap();
        self.host = host.into();

        if let Some(new_port) = opt_new_port {
            self.port = new_port;
            if let Some(port) = new_port {
                write!(&mut self.serialization, ":{}", port).unwrap();
            }
        }
        let new_suffix_pos = to_u32(self.serialization.len()).unwrap();
        self.serialization.push_str(&suffix);

        let adjust = |index: &mut u32| {
            *index -= old_suffix_pos;
            *index += new_suffix_pos;
        };
        adjust(&mut self.path_start);
        if let Some(ref mut index) = self.query_start { adjust(index) }
        if let Some(ref mut index) = self.fragment_start { adjust(index) }
    }

    /// Change this URL’s host to the given IP address.
    ///
    /// If this URL is cannot-be-a-base, do nothing and return `Err`.
    ///
    /// Compared to `Url::set_host`, this skips the host parser.
    pub fn set_ip_host(&mut self, address: IpAddr) -> Result<(), ()> {
        if self.cannot_be_a_base() {
            return Err(())
        }

        let address = match address {
            IpAddr::V4(address) => Host::Ipv4(address),
            IpAddr::V6(address) => Host::Ipv6(address),
        };
        self.set_host_internal(address, None);
        Ok(())
    }

    /// Change this URL’s password.
    ///
    /// If this URL is cannot-be-a-base or does not have a host, do nothing and return `Err`.
    pub fn set_password(&mut self, password: Option<&str>) -> Result<(), ()> {
        if !self.has_host() {
            return Err(())
        }
        if let Some(password) = password {
            let host_and_after = self.slice(self.host_start..).to_owned();
            self.serialization.truncate(self.username_end as usize);
            self.serialization.push(':');
            self.serialization.extend(utf8_percent_encode(password, USERINFO_ENCODE_SET));
            self.serialization.push('@');

            let old_host_start = self.host_start;
            let new_host_start = to_u32(self.serialization.len()).unwrap();
            let adjust = |index: &mut u32| {
                *index -= old_host_start;
                *index += new_host_start;
            };
            self.host_start = new_host_start;
            adjust(&mut self.host_end);
            adjust(&mut self.path_start);
            if let Some(ref mut index) = self.query_start { adjust(index) }
            if let Some(ref mut index) = self.fragment_start { adjust(index) }

            self.serialization.push_str(&host_and_after);
        } else if self.byte_at(self.username_end) == b':' {  // If there is a password to remove
            let has_username_or_password = self.byte_at(self.host_start - 1) == b'@';
            debug_assert!(has_username_or_password);
            let username_start = self.scheme_end + 3;
            let empty_username = username_start == self.username_end;
            let start = self.username_end;  // Remove the ':'
            let end = if empty_username {
                self.host_start // Remove the '@' as well
            } else {
                self.host_start - 1  // Keep the '@' to separate the username from the host
            };
            self.serialization.drain(start as usize .. end as usize);
            let offset = end - start;
            self.host_start -= offset;
            self.host_end -= offset;
            self.path_start -= offset;
            if let Some(ref mut index) = self.query_start { *index -= offset }
            if let Some(ref mut index) = self.fragment_start { *index -= offset }
        }
        Ok(())
    }

    /// Change this URL’s username.
    ///
    /// If this URL is cannot-be-a-base or does not have a host, do nothing and return `Err`.
    pub fn set_username(&mut self, username: &str) -> Result<(), ()> {
        if !self.has_host() {
            return Err(())
        }
        let username_start = self.scheme_end + 3;
        debug_assert!(self.slice(self.scheme_end..username_start) == "://");
        if self.slice(username_start..self.username_end) == username {
            return Ok(())
        }
        let after_username = self.slice(self.username_end..).to_owned();
        self.serialization.truncate(username_start as usize);
        self.serialization.extend(utf8_percent_encode(username, USERINFO_ENCODE_SET));

        let mut removed_bytes = self.username_end;
        self.username_end = to_u32(self.serialization.len()).unwrap();
        let mut added_bytes = self.username_end;

        let new_username_is_empty = self.username_end == username_start;
        match (new_username_is_empty, after_username.chars().next()) {
            (true, Some('@')) => {
                removed_bytes += 1;
                self.serialization.push_str(&after_username[1..]);
            }
            (false, Some('@')) | (_, Some(':')) | (true, _) => {
                self.serialization.push_str(&after_username);
            }
            (false, _) => {
                added_bytes += 1;
                self.serialization.push('@');
                self.serialization.push_str(&after_username);
            }
        }

        let adjust = |index: &mut u32| {
            *index -= removed_bytes;
            *index += added_bytes;
        };
        adjust(&mut self.host_start);
        adjust(&mut self.host_end);
        adjust(&mut self.path_start);
        if let Some(ref mut index) = self.query_start { adjust(index) }
        if let Some(ref mut index) = self.fragment_start { adjust(index) }
        Ok(())
    }

    /// Change this URL’s scheme.
    ///
    /// Do nothing and return `Err` if:
    /// * The new scheme is not in `[a-zA-Z][a-zA-Z0-9+.-]+`
    /// * This URL is cannot-be-a-base and the new scheme is one of
    ///   `http`, `https`, `ws`, `wss`, `ftp`, or `gopher`
    pub fn set_scheme(&mut self, scheme: &str) -> Result<(), ()> {
        let mut parser = Parser::for_setter(String::new());
        let remaining = try!(parser.parse_scheme(parser::Input::new(scheme)));
        if !remaining.is_empty() ||
                (!self.has_host() && SchemeType::from(&parser.serialization).is_special()) {
            return Err(())
        }
        let old_scheme_end = self.scheme_end;
        let new_scheme_end = to_u32(parser.serialization.len()).unwrap();
        let adjust = |index: &mut u32| {
            *index -= old_scheme_end;
            *index += new_scheme_end;
        };

        self.scheme_end = new_scheme_end;
        adjust(&mut self.username_end);
        adjust(&mut self.host_start);
        adjust(&mut self.host_end);
        adjust(&mut self.path_start);
        if let Some(ref mut index) = self.query_start { adjust(index) }
        if let Some(ref mut index) = self.fragment_start { adjust(index) }

        parser.serialization.push_str(self.slice(old_scheme_end..));
        self.serialization = parser.serialization;
        Ok(())
    }

    /// Convert a file name as `std::path::Path` into an URL in the `file` scheme.
    ///
    /// This returns `Err` if the given path is not absolute or,
    /// on Windows, if the prefix is not a disk prefix (e.g. `C:`).
    pub fn from_file_path<P: AsRef<Path>>(path: P) -> Result<Url, ()> {
        let mut serialization = "file://".to_owned();
        let path_start = serialization.len() as u32;
        try!(path_to_file_url_segments(path.as_ref(), &mut serialization));
        Ok(Url {
            serialization: serialization,
            scheme_end: "file".len() as u32,
            username_end: path_start,
            host_start: path_start,
            host_end: path_start,
            host: HostInternal::None,
            port: None,
            path_start: path_start,
            query_start: None,
            fragment_start: None,
        })
    }

    /// Convert a directory name as `std::path::Path` into an URL in the `file` scheme.
    ///
    /// This returns `Err` if the given path is not absolute or,
    /// on Windows, if the prefix is not a disk prefix (e.g. `C:`).
    ///
    /// Compared to `from_file_path`, this ensure that URL’s the path has a trailing slash
    /// so that the entire path is considered when using this URL as a base URL.
    ///
    /// For example:
    ///
    /// * `"index.html"` parsed with `Url::from_directory_path(Path::new("/var/www"))`
    ///   as the base URL is `file:///var/www/index.html`
    /// * `"index.html"` parsed with `Url::from_file_path(Path::new("/var/www"))`
    ///   as the base URL is `file:///var/index.html`, which might not be what was intended.
    ///
    /// Note that `std::path` does not consider trailing slashes significant
    /// and usually does not include them (e.g. in `Path::parent()`).
    pub fn from_directory_path<P: AsRef<Path>>(path: P) -> Result<Url, ()> {
        let mut url = try!(Url::from_file_path(path));
        if !url.serialization.ends_with('/') {
            url.serialization.push('/')
        }
        Ok(url)
    }

    /// Assuming the URL is in the `file` scheme or similar,
    /// convert its path to an absolute `std::path::Path`.
    ///
    /// **Note:** This does not actually check the URL’s `scheme`,
    /// and may give nonsensical results for other schemes.
    /// It is the user’s responsibility to check the URL’s scheme before calling this.
    ///
    /// ```
    /// # use url::Url;
    /// # let url = Url::parse("file:///etc/passwd").unwrap();
    /// let path = url.to_file_path();
    /// ```
    ///
    /// Returns `Err` if the host is neither empty nor `"localhost"`,
    /// or if `Path::new_opt()` returns `None`.
    /// (That is, if the percent-decoded path contains a NUL byte or,
    /// for a Windows path, is not UTF-8.)
    #[inline]
    pub fn to_file_path(&self) -> Result<PathBuf, ()> {
        // FIXME: Figure out what to do w.r.t host.
        if matches!(self.host(), None | Some(Host::Domain("localhost"))) {
            if let Some(segments) = self.path_segments() {
                return file_url_segments_to_pathbuf(segments)
            }
        }
        Err(())
    }

    // Private helper methods:

    #[inline]
    fn slice<R>(&self, range: R) -> &str where R: RangeArg {
        range.slice_of(&self.serialization)
    }

    #[inline]
    fn byte_at(&self, i: u32) -> u8 {
        self.serialization.as_bytes()[i as usize]
    }
}

/// Return an error if `Url::host` or `Url::port_or_known_default` return `None`.
impl ToSocketAddrs for Url {
    type Iter = SocketAddrs;

    fn to_socket_addrs(&self) -> io::Result<Self::Iter> {
        try!(self.with_default_port(|_| Err(()))).to_socket_addrs()
    }
}

/// Parse a string as an URL, without a base URL or encoding override.
impl str::FromStr for Url {
    type Err = ParseError;

    #[inline]
    fn from_str(input: &str) -> Result<Url, ::ParseError> {
        Url::parse(input)
    }
}

/// Display the serialization of this URL.
impl fmt::Display for Url {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.serialization, formatter)
    }
}

/// Debug the serialization of this URL.
impl fmt::Debug for Url {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.serialization, formatter)
    }
}

/// URLs compare like their serialization.
impl Eq for Url {}

/// URLs compare like their serialization.
impl PartialEq for Url {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.serialization == other.serialization
    }
}

/// URLs compare like their serialization.
impl Ord for Url {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        self.serialization.cmp(&other.serialization)
    }
}

/// URLs compare like their serialization.
impl PartialOrd for Url {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        self.serialization.partial_cmp(&other.serialization)
    }
}

/// URLs hash like their serialization.
impl hash::Hash for Url {
    #[inline]
    fn hash<H>(&self, state: &mut H) where H: hash::Hasher {
        hash::Hash::hash(&self.serialization, state)
    }
}

/// Return the serialization of this URL.
impl AsRef<str> for Url {
    #[inline]
    fn as_ref(&self) -> &str {
        &self.serialization
    }
}

trait RangeArg {
    fn slice_of<'a>(&self, s: &'a str) -> &'a str;
}

impl RangeArg for Range<u32> {
    #[inline]
    fn slice_of<'a>(&self, s: &'a str) -> &'a str {
        &s[self.start as usize .. self.end as usize]
    }
}

impl RangeArg for RangeFrom<u32> {
    #[inline]
    fn slice_of<'a>(&self, s: &'a str) -> &'a str {
        &s[self.start as usize ..]
    }
}

impl RangeArg for RangeTo<u32> {
    #[inline]
    fn slice_of<'a>(&self, s: &'a str) -> &'a str {
        &s[.. self.end as usize]
    }
}

#[cfg(feature="rustc-serialize")]
impl rustc_serialize::Encodable for Url {
    fn encode<S: rustc_serialize::Encoder>(&self, encoder: &mut S) -> Result<(), S::Error> {
        encoder.emit_str(self.as_str())
    }
}


#[cfg(feature="rustc-serialize")]
impl rustc_serialize::Decodable for Url {
    fn decode<D: rustc_serialize::Decoder>(decoder: &mut D) -> Result<Url, D::Error> {
        Url::parse(&*try!(decoder.read_str())).map_err(|error| {
            decoder.error(&format!("URL parsing error: {}", error))
        })
    }
}

/// Serializes this URL into a `serde` stream.
///
/// This implementation is only available if the `serde` Cargo feature is enabled.
#[cfg(feature="serde")]
impl serde::Serialize for Url {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error> where S: serde::Serializer {
        format!("{}", self).serialize(serializer)
    }
}

/// Deserializes this URL from a `serde` stream.
///
/// This implementation is only available if the `serde` Cargo feature is enabled.
#[cfg(feature="serde")]
impl serde::Deserialize for Url {
    fn deserialize<D>(deserializer: &mut D) -> Result<Url, D::Error> where D: serde::Deserializer {
        let string_representation: String = try!(serde::Deserialize::deserialize(deserializer));
        Ok(Url::parse(&string_representation).unwrap())
    }
}

#[cfg(unix)]
fn path_to_file_url_segments(path: &Path, serialization: &mut String) -> Result<(), ()> {
    use std::os::unix::prelude::OsStrExt;
    if !path.is_absolute() {
        return Err(())
    }
    let mut empty = true;
    // skip the root component
    for component in path.components().skip(1) {
        empty = false;
        serialization.push('/');
        serialization.extend(percent_encode(
            component.as_os_str().as_bytes(), PATH_SEGMENT_ENCODE_SET));
    }
    if empty {
        // An URL’s path must not be empty.
        serialization.push('/');
    }
    Ok(())
}

#[cfg(windows)]
fn path_to_file_url_segments(path: &Path, serialization: &mut String) -> Result<(), ()> {
    path_to_file_url_segments_windows(path, serialization)
}

// Build this unconditionally to alleviate https://github.com/servo/rust-url/issues/102
#[cfg_attr(not(windows), allow(dead_code))]
fn path_to_file_url_segments_windows(path: &Path, serialization: &mut String) -> Result<(), ()> {
    use std::path::{Prefix, Component};
    if !path.is_absolute() {
        return Err(())
    }
    let mut components = path.components();
    let disk = match components.next() {
        Some(Component::Prefix(ref p)) => match p.kind() {
            Prefix::Disk(byte) => byte,
            Prefix::VerbatimDisk(byte) => byte,
            _ => return Err(()),
        },

        // FIXME: do something with UNC and other prefixes?
        _ => return Err(())
    };

    // Start with the prefix, e.g. "C:"
    serialization.push('/');
    serialization.push(disk as char);
    serialization.push(':');

    for component in components {
        if component == Component::RootDir { continue }
        // FIXME: somehow work with non-unicode?
        let component = try!(component.as_os_str().to_str().ok_or(()));
        serialization.push('/');
        serialization.extend(percent_encode(component.as_bytes(), PATH_SEGMENT_ENCODE_SET));
    }
    Ok(())
}

#[cfg(unix)]
fn file_url_segments_to_pathbuf(segments: str::Split<char>) -> Result<PathBuf, ()> {
    use std::ffi::OsStr;
    use std::os::unix::prelude::OsStrExt;
    use std::path::PathBuf;

    let mut bytes = Vec::new();
    for segment in segments {
        bytes.push(b'/');
        bytes.extend(percent_decode(segment.as_bytes()));
    }
    let os_str = OsStr::from_bytes(&bytes);
    let path = PathBuf::from(os_str);
    debug_assert!(path.is_absolute(),
                  "to_file_path() failed to produce an absolute Path");
    Ok(path)
}

#[cfg(windows)]
fn file_url_segments_to_pathbuf(segments: str::Split<char>) -> Result<PathBuf, ()> {
    file_url_segments_to_pathbuf_windows(segments)
}

// Build this unconditionally to alleviate https://github.com/servo/rust-url/issues/102
#[cfg_attr(not(windows), allow(dead_code))]
fn file_url_segments_to_pathbuf_windows(mut segments: str::Split<char>) -> Result<PathBuf, ()> {
    let first = try!(segments.next().ok_or(()));

    let mut string = match first.len() {
        2 => {
            if !first.starts_with(parser::ascii_alpha) || first.as_bytes()[1] != b':' {
                return Err(())
            }

            first.to_owned()
        },

        4 => {
            if !first.starts_with(parser::ascii_alpha) {
                return Err(())
            }
            let bytes = first.as_bytes();
            if bytes[1] != b'%' || bytes[2] != b'3' || (bytes[3] != b'a' && bytes[3] != b'A') {
                return Err(())
            }

            first[0..1].to_owned() + ":"
        },

        _ => return Err(()),
    };

    for segment in segments {
        string.push('\\');

        // Currently non-unicode windows paths cannot be represented
        match String::from_utf8(percent_decode(segment.as_bytes()).collect()) {
            Ok(s) => string.push_str(&s),
            Err(..) => return Err(()),
        }
    }
    let path = PathBuf::from(string);
    debug_assert!(path.is_absolute(),
                  "to_file_path() failed to produce an absolute Path");
    Ok(path)
}

fn io_error<T>(reason: &str) -> io::Result<T> {
    Err(io::Error::new(io::ErrorKind::InvalidData, reason))
}

/// Implementation detail of `Url::query_pairs_mut`. Typically not used directly.
pub struct UrlQuery<'a> {
    url: &'a mut Url,
    fragment: Option<String>,
}

impl<'a> Drop for UrlQuery<'a> {
    fn drop(&mut self) {
        self.url.restore_already_parsed_fragment(self.fragment.take())
    }
}
