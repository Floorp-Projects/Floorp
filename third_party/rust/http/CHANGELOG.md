# 0.1.10 (August, 8 2018)

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
