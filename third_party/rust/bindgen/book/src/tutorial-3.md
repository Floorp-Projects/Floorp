# Create a `build.rs` File

First, we have to tell `cargo` that we have a `build.rs` script by adding
another line to the `Cargo.toml`:

```toml
[package]
build = "build.rs"
```

Second, we create the `build.rs` file in our crate's root. This file is compiled
and executed before the rest of the crate is built, and can be used to generate
code at compile time. And of course in our case, we will be generating Rust FFI
bindings to `bzip2` at compile time. The resulting bindings will be written to
`$OUT_DIR/bindings.rs` where `$OUT_DIR` is chosen by `cargo` and is something
like `./target/debug/build/libbindgen-tutorial-bzip2-sys-afc7747d7eafd720/out/`.

```rust,ignore
extern crate bindgen;

use std::env;
use std::path::PathBuf;

fn main() {
    // Tell cargo to tell rustc to link the system bzip2
    // shared library.
    println!("cargo:rustc-link-lib=bz2");

    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let bindings = bindgen::Builder::default()
        // Do not generate unstable Rust code that
        // requires a nightly rustc and enabling
        // unstable features.
        .no_unstable_rust()
        // The input header we would like to generate
        // bindings for.
        .header("wrapper.h")
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
```

Now, when we run `cargo build`, our bindings to `bzip2` are generated on the
fly!

[There's more info about `build.rs` files in the crates.io documentation.][build-rs]

[build-rs]: http://doc.crates.io/build-script.html
