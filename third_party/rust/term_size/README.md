term_size
====

[![Crates.io](https://img.shields.io/crates/v/term_size.svg)](https://crates.io/crates/term_size) [![Crates.io](https://img.shields.io/crates/d/term_size.svg)](https://crates.io/crates/term_size) [![license](http://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/kbknapp/term_size-rs/blob/master/LICENSE-MIT) [![license](http://img.shields.io/badge/license-Apache2.0-blue.svg)](https://github.com/kbknapp/term_size-rs/blob/master/LICENSE-APACHE) [![Coverage Status](https://coveralls.io/repos/kbknapp/term_size-rs/badge.svg?branch=master&service=github)](https://coveralls.io/github/kbknapp/term_size-rs?branch=master) [![Join the chat at https://gitter.im/kbknapp/term_size-rs](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/kbknapp/term_size-rs?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Linux: [![Build Status](https://travis-ci.org/clap-rs/term_size-rs.svg?branch=master)](https://travis-ci.org/clap-rs/term_size-rs)
Windows: [![Build status](https://ci.appveyor.com/api/projects/status/6q0x4h6i0e3ypbm5?svg=true
)](https://ci.appveyor.com/project/kbknapp/term_size-rs/branch/master)

A Rust library to enable getting terminal sizes and dimensions

[Documentation](https://docs.rs/term_size)

## Usage

First, add the following to your `Cargo.toml`:

```toml
[dependencies]
term_size = "1"
```

Next, add this to your crate root:

```rust
extern crate term_size;
```

To get the dimensions of your terminal window, simply use the following:

```rust
fn main() {
  if let Some((w, h)) = term_size::dimensions() {
    println!("Width: {}\nHeight: {}", w, h);
  } else {
    println!("Unable to get term size :(")
  }
}
```

## License

Copyright Benjamin Sago, Kevin Knapp, and `term_size` contributors.

Licensed under either of

* Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
* MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option. Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the
Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.

## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :D

## Minimum Version of Rust

`term_size` will officially support current stable Rust, minus two releases, but may work with prior releases as well. For example, current stable Rust at the time of this writing is 1.22.1, meaning `term_size` is guaranteed to compile with 1.20.0 and newer.

At the 1.23.0 stable release, `term_size` will be guaranteed to compile with 1.21.0 and newer, etc.

Upon bumping the minimum version of Rust (assuming it's within the stable-2 range), it must be clearly annotated in the [`CHANGELOG.md`](./CHANGELOG.md)

## Breaking Changes

`term_size` takes a similar policy to Rust and will bump the major version number upon breaking changes with only the following exceptions:

* The breaking change is to fix a security concern
* The breaking change is to be fixing a bug (i.e. relying on a bug as a feature)
* The breaking change is a feature isn't used in the wild, or all users of said feature have given approval prior to the change
