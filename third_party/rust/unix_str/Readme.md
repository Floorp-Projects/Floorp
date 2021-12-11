# `unix_str`

Unix-compatible strings regardless of platform, including `#![no_std]`
environents. This crate is extracted from `std`.

## Features

- `shrink_to`: implements the unstable `shrink_to` method;
- `unixstring_ascii`: ASCII transformations, `std`'s unstable feature;
- `toowned_clone_into`: implements `ToOwned::clone_into`, an unstable method;
- `alloc`: implements `UnixString` and transformations with `Box`, `Rc`
  and `Arc`;
- `std`: an alias for `alloc`.

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
