# 0.1.17 (April 5, 2019)

* Add `Error::inner_ref()` to view the kind of error (#303)
* Add `headers_ref()` and `headers_mut()` methods to `request::Builder` and `response::Builder` (#293)

# 0.1.16 (February 19, 2019)

* Fix `Uri` to permit more characters in the `path` (#296)

# 0.1.15 (January 22, 2019)

* Fix `Uri::host()` to include brackets of IPv6 literals (#292)
* Add `scheme_str` and `port_u16` methods to `Uri` (#287)
* Add `method_ref`, `uri_ref`, and `headers_ref` to `request::Builder` (#284)

# 0.1.14 (November 21, 2018)

* Add `Port` struct (#252, #255, #265)
* Introduce `Uri` builder (#219)
* Empty `Method` no longer considered valid (#262)
* Fix `Uri` equality when terminating question mark is present (#270)
* Allow % character in userinfo (#269)
* Support additional tokens for header names (#271)
* Export `http::headers::{IterMut, ValuesMut}` (#278)

# 0.1.13 (September 14, 2018)

* impl `fmt::Display` for `HeaderName` (#249)
* Fix `uri::Authority` parsing when there is no host after an `@` (#248)
* Fix `Uri` parsing to allow more characters in query strings (#247)

# 0.1.12 (September 7, 2018)

* Fix `HeaderValue` parsing to allow HTABs (#244)

# 0.1.11 (September 5, 2018)

* Add `From<&Self>` for `HeaderValue`, `Method`, and `StatusCode` (#238)
* Add `Uri::from_static` (#240)

# 0.1.10 (August 8, 2018)

* impl HttpTryFrom<String> for HeaderValue (#236)

# 0.1.9 (August 7, 2018)

* Fix double percent encoding (#233)
* Add additional HttpTryFrom impls (#234)

# 0.1.8 (July 23, 2018)

* Add fuller set of `PartialEq` for `Method` (#221)
* Reduce size of `HeaderMap` by using `Box<[Entry]>` instea of `Vec` (#224)
* Reduce size of `Extensions` by storing as `Option<Box<AnyMap>>` (#227)
* Implement `Iterator::size_hint` for most iterators in `header` (#226)

# 0.1.7 (June 22, 2018)

* Add `From<uN> for HeaderValue` for most integer types (#218).
* Add `Uri::into_parts()` inherent method (same as `Parts::from(uri)`) (#214).
* Fix converting `Uri`s in authority-form to `Parts` and then back into `Uri` (#216).
* Fix `Authority` parsing to reject multiple port sections (#215).
* Fix parsing 1 character authority-form `Uri`s into illegal forms (#220).

# 0.1.6 (June 13, 2018)

* Add `HeaderName::from_static()` constructor (#195).
* Add `Authority::from_static()` constructor (#186).
* Implement `From<HeaderName>` for `HeaderValue` (#184).
* Fix duplicate keys when iterating over `header::Keys` (#201).

# 0.1.5 (February 28, 2018)

* Add websocket handshake related header constants (#162).
* Parsing `Authority` with an empty string now returns an error (#164).
* Implement `PartialEq<u16>` for `StatusCode` (#153).
* Implement `HttpTryFrom<&Uri>` for `Uri` (#165).
* Implement `FromStr` for `Method` (#167).
* Implement `HttpTryFrom<String>` for `Uri` (#171).
* Add `into_body` fns to `Request` and `Response` (#172).
* Fix `Request::options` (#177).

# 0.1.4 (January 4, 2018)

* Add PathAndQuery::from_static (#148).
* Impl PartialOrd / PartialEq for Authority and PathAndQuery (#150).
* Add `map` fn to `Request` and `Response` (#151).

# 0.1.3 (December 11, 2017)

* Add `Scheme` associated consts for common protos.

# 0.1.2 (November 29, 2017)

* Add Uri accessor for scheme part.
* Fix Uri parsing bug (#134)

# 0.1.1 (October 9, 2017)

* Provide Uri accessors for parts (#129)
* Add Request builder helpers. (#123)
* Misc performance improvements (#126)

# 0.1.0 (September 8, 2017)

* Initial release.
