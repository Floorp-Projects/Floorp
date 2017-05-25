rustc-version-rs
==============

[![Travis-CI Status](https://travis-ci.org/Kimundi/rustc-version-rs.png?branch=master)](https://travis-ci.org/Kimundi/rustc-version-rs)

A library for querying the version of a installed rustc compiler.

For more details, see the [docs](http://kimundi.github.io/rustc-version-rs/rustc_version/index.html).

# Getting Started

[rustc-version-rs is available on crates.io](https://crates.io/crates/rustc_version).
Add the following dependency to your Cargo manifest to get the latest version of the 0.1 branch:
```toml
[dependencies]

rustc_version = "0.1.*"
```

To always get the latest version, add this git repository to your
Cargo manifest:

```toml
[dependencies.rustc_version]
git = "https://github.com/Kimundi/rustc-version-rs"
```
# Example

```rust
// This could be a cargo build script

extern crate rustc_version;
use rustc_version::{version, version_matches, version_meta, Channel};

fn main() {
    // Assert we haven't travelled back in time
    assert!(version().major >= 1);

    // Set cfg flags depending on release channel
    match version_meta().channel {
        Channel::Stable => {
            println!("cargo:rustc-cfg=RUSTC_IS_STABLE");
        }
        Channel::Beta => {
            println!("cargo:rustc-cfg=RUSTC_IS_BETA");
        }
        Channel::Nightly => {
            println!("cargo:rustc-cfg=RUSTC_IS_NIGHTLY");
        }
        Channel::Dev => {
            println!("cargo:rustc-cfg=RUSTC_IS_DEV");
        }
    }

    // Directly check a semver version requirment
    if version_matches(">= 1.4.0") {
        println!("cargo:rustc-cfg=compiler_has_important_bugfix");
    }
}
```

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.
