Windows Manifest Embedding for Rust
===================================

The `embed-manifest` crate provides a straightforward way to embed
a Windows manifest in an executable, whatever the build environment,
without dependencies on external tools from LLVM or MinGW.

If you need to embed more resources than just a manifest, you may
find the [winres](https://crates.io/crates/winres) or
[embed-resource](https://crates.io/crates/embed-resource)
crates more suitable. They have additional dependencies and setup
requirements that may make them a little more difficult to use, though.


Why use it?
-----------

The Rust compiler doesn’t add a manifest to a Windows executable, which
means that it runs with a few compatibility options and settings that
make it look like the program is running on an older version of Windows.
By adding an application manifest using this crate, even when cross-compiling:

- 32-bit programs with names that look like installers don’t try to run
  as an administrator.
- 32-bit programs aren’t shown a virtualised view of the filesystem and
  registry.
- Many non-Unicode APIs accept UTF-8 strings, the same as Rust uses.
- The program sees the real Windows version, not Windows Vista.
- Built-in message boxes and other UI elements can display without blurring
  on high-DPI monitors.
- Other features like long path names in APIs can be enabled.


Usage
-----

To embed a default manifest, include this code in a `build.rs` build
script:

```rust
use embed_manifest::{embed_manifest, new_manifest};

fn main() {
    if std::env::var_os("CARGO_CFG_WINDOWS").is_some() {
        embed_manifest(new_manifest("Contoso.Sample"))
            .expect("unable to embed manifest file");
    }
    println!("cargo:rerun-if-changed=build.rs");
}
```

See the [crate documentation](https://docs.rs/embed-manifest) for
information about how to customise the embedded manifest.


License
-------

For the avoidance of doubt, while this crate itself is licensed to
you under the MIT License, this does not affect the copyright status
and licensing of your own code when this is used from a Cargo build
script.
