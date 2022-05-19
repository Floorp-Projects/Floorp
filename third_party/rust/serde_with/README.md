# Custom de/serialization functions for Rust's [serde](https://serde.rs)

[![docs.rs badge](https://docs.rs/serde_with/badge.svg)](https://docs.rs/serde_with/)
[![crates.io badge](https://img.shields.io/crates/v/serde_with.svg)](https://crates.io/crates/serde_with/)
[![Build Status](https://github.com/jonasbb/serde_with/workflows/Rust%20CI/badge.svg)](https://github.com/jonasbb/serde_with)
[![codecov](https://codecov.io/gh/jonasbb/serde_with/branch/master/graph/badge.svg)](https://codecov.io/gh/jonasbb/serde_with)

---

This crate provides custom de/serialization helpers to use in combination with [serde's with-annotation][with-annotation] and with the improved [`serde_as`][user guide]-annotation.
Some common use cases are:

* De/Serializing a type using the `Display` and `FromStr` traits, e.g., for `u8`, `url::Url`, or `mime::Mime`.
     Check [`DisplayFromStr`][] or [`serde_with::rust::display_fromstr`][display_fromstr] for details.
* Skip serializing all empty `Option` types with [`#[skip_serializing_none]`][skip_serializing_none].
* Apply a prefix to each field name of a struct, without changing the de/serialize implementations of the struct using [`with_prefix!`][].
* Deserialize a comma separated list like `#hash,#tags,#are,#great` into a `Vec<String>`.
     Check the documentation for [`serde_with::rust::StringWithSeparator::<CommaSeparator>`][StringWithSeparator].

### Getting Help

**Check out the [user guide][user guide] to find out more tips and tricks about this crate.**

For further help using this crate you can [open a new discussion](https://github.com/jonasbb/serde_with/discussions/new) or ask on [users.rust-lang.org](https://users.rust-lang.org/).
For bugs please open a [new issue](https://github.com/jonasbb/serde_with/issues/new) on Github.

## Use `serde_with` in your Project

Add this to your `Cargo.toml`:

```toml
[dependencies.serde_with]
version = "1.6.4"
features = [ "..." ]
```

The crate contains different features for integration with other common crates.
Check the [feature flags][] section for information about all available features.

## Examples

Annotate your struct or enum to enable the custom de/serializer.

### `DisplayFromStr`

```rust
#[serde_as]
#[derive(Deserialize, Serialize)]
struct Foo {
    // Serialize with Display, deserialize with FromStr
    #[serde_as(as = "DisplayFromStr")]
    bar: u8,
}

// This will serialize
Foo {bar: 12}

// into this JSON
{"bar": "12"}
```

### `skip_serializing_none`

This situation often occurs with JSON, but other formats also support optional fields.
If many fields are optional, putting the annotations on the structs can become tedious.

```rust
#[skip_serializing_none]
#[derive(Deserialize, Serialize)]
struct Foo {
    a: Option<usize>,
    b: Option<usize>,
    c: Option<usize>,
    d: Option<usize>,
    e: Option<usize>,
    f: Option<usize>,
    g: Option<usize>,
}

// This will serialize
Foo {a: None, b: None, c: None, d: Some(4), e: None, f: None, g: Some(7)}

// into this JSON
{"d": 4, "g": 7}
```

### Advanced `serde_as` usage

This example is mainly supposed to highlight the flexibility of the `serde_as`-annotation compared to [serde's with-annotation][with-annotation].
More details about `serde_as` can be found in the [user guide][].

```rust
#[serde_as]
#[derive(Deserialize, Serialize)]
struct Foo {
     // Serialize them into a list of number as seconds
     #[serde_as(as = "Vec<DurationSeconds>")]
     durations: Vec<Duration>,
     // We can treat a Vec like a map with duplicates.
     // JSON only allows string keys, so convert i32 to strings
     // The bytes will be hex encoded
     #[serde_as(as = "BTreeMap<DisplayFromStr, Hex>")]
     bytes: Vec<(i32, Vec<u8>)>,
}

// This will serialize
Foo {
    durations: vec![Duration::new(5, 0), Duration::new(3600, 0), Duration::new(0, 0)],
    bytes: vec![
        (1, vec![0, 1, 2]),
        (-100, vec![100, 200, 255]),
        (1, vec![0, 111, 222]),
    ],
}

// into this JSON
{
    "durations": [5, 3600, 0],
    "bytes": {
        "1": "000102",
        "-100": "64c8ff",
        "1": "006fde"
    }
}
```

[`DisplayFromStr`]: https://docs.rs/serde_with/1.6.4/serde_with/struct.DisplayFromStr.html
[`with_prefix!`]: https://docs.rs/serde_with/1.6.4/serde_with/macro.with_prefix.html
[display_fromstr]: https://docs.rs/serde_with/1.6.4/serde_with/rust/display_fromstr/index.html
[feature flags]: https://docs.rs/serde_with/1.6.4/serde_with/guide/feature_flags/index.html
[skip_serializing_none]: https://docs.rs/serde_with/1.6.4/serde_with/attr.skip_serializing_none.html
[StringWithSeparator]: https://docs.rs/serde_with/1.6.4/serde_with/rust/struct.StringWithSeparator.html
[user guide]: https://docs.rs/serde_with/1.6.4/serde_with/guide/index.html
[with-annotation]: https://serde.rs/field-attrs.html#with

## License

Licensed under either of

* Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
* MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
