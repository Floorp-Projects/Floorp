[![Travis CI][tcii]][tci] [![Appveyor CI][acii]][aci]

[tcii]: https://travis-ci.org/nagisa/target_build_utils.rs.svg?branch=master
[tci]: https://travis-ci.org/nagisa/target_build_utils.rs
[acii]: https://ci.appveyor.com/api/projects/status/jasfj7r1o085xpxo?svg=true
[aci]: https://ci.appveyor.com/project/nagisa/target-build-utils-rs

Utility crate to handle the `TARGET` environment variable passed into build.rs scripts.

Unlike rust’s `#[cfg(target…)]` attributes, `build.rs`-scripts do not expose a convenient way
to detect the system the code will be built for in a way which would properly support
cross-compilation.

This crate exposes `target_arch`, `target_vendor`, `target_os` and `target_abi` very much in
the same manner as the corresponding `cfg` attributes in Rust do, thus allowing `build.rs`
script to adjust the output depending on the target the crate is being built for..

Custom target json files are also supported.

# Using target_build_utils

This crate is only useful if you’re using a build script (`build.rs`). Add dependency to this
crate to your `Cargo.toml` via:

```toml
[package]
# ...
build = "build.rs"

[build-dependencies]
target_build_utils = "0.1"
```

Then write your `build.rs` like this:

```rust,no_run
extern crate target_build_utils;
use target_build_utils::TargetInfo;

fn main() {
    let target = TargetInfo::new().expect("could not get target info");
    if target.target_os() == "windows" {
        // conditional stuff for windows
    }
}
```

Now, when running `cargo build`, your `build.rs` should be aware of the properties of the
target system when your crate is being cross-compiled.

# License

llvm_build_utils is distributed under ISC (MIT-like) or Apache (version 2.0) license at your
choice.
