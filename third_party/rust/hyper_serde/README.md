Serde support for Hyper types
=============================

This crate provides wrappers and convenience functions to support [Serde] for
some types defined in [cookie], [Hyper], [mime] and [time].

[cookie]: https://github.com/alexcrichton/cookie-rs
[Hyper]: https://github.com/hyperium/hyper
[mime]: https://github.com/hyperium/mime.rs
[Serde]: https://github.com/serde-rs/serde
[time]: https://github.com/rust-lang-deprecated/time

The supported types are:

* `cookie::Cookie`
* `hyper::header::ContentType`
* `hyper::header::Headers`
* `hyper::http::RawStatus`
* `hyper::method::Method`
* `mime::Mime`
* `time::Tm`

For more details, see the crate documentation.

## License

hyper_serde is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in hyper_serde by you, as defined in the Apache-2.0 license,
shall be dual licensed as above, without any additional terms or conditions.
