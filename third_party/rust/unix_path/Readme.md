# `unix_path`

Platform-independent handling of Unix paths, including `#![no_std]`
environments. This crate is mostly extracted from `std`, except that it uses
[`unix_str`] instead of `std`'s `OsStr` and some methods are renamed
appropriately.

[`unix_str`]: https://crates.io/crates/unix_str

## Features

- `shrink_to`: implements the unstable `shrink_to` method;
- `alloc`: implements `PathBuf` and transformations with `Box`, `Rc` and `Arc`;
- `std`: `alloc` + implements the `Error` trait for errors. Enabled by default;
- `serde`: Implements `Serialize` and `Deserialize` for `Path` and `PathBuf`.

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE)
  or http://www.apache.org/licenses/LICENSE-2.0)
- MIT license ([LICENSE-MIT](LICENSE-MIT)
  or http://opensource.org/licenses/MIT)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
