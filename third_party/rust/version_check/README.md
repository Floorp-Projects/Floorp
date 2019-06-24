# version\_check

This tiny crate checks that the running or installed `rustc` meets some version
requirements. The version is queried by calling the Rust compiler with
`--version`. The path to the compiler is determined first via the `RUSTC`
environment variable. If it is not set, then `rustc` is used. If that fails, no
determination is made, and calls return `None`.

## Usage

Add to your `Cargo.toml` file, typically as a build dependency:

```toml
[build-dependencies]
version_check = "0.1"
```

## Examples

Check that the running compiler is a nightly release:

```rust
extern crate version_check;

match version_check::is_nightly() {
    Some(true) => "running a nightly",
    Some(false) => "not nightly",
    None => "couldn't figure it out"
};
```

Check that the running compiler is at least version `1.13.0`:

```rust
extern crate version_check;

match version_check::is_min_version("1.13.0") {
    Some((true, version)) => format!("Yes! It's: {}", version),
    Some((false, version)) => format!("No! {} is too old!", version),
    None => "couldn't figure it out".into()
};
```

Check that the running compiler was released on or after `2016-12-18`:

```rust
extern crate version_check;

match version_check::is_min_date("2016-12-18") {
    Some((true, date)) => format!("Yes! It's: {}", date),
    Some((false, date)) => format!("No! {} is too long ago!", date),
    None => "couldn't figure it out".into()
};
```

## Alternatives

This crate is dead simple with no dependencies. If you need something more and
don't care about panicking if the version cannot be obtained or adding
dependencies, see [rustc_version](https://crates.io/crates/rustc_version).

## License

`version_check` is licensed under either of the following, at your option:

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT License ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)
