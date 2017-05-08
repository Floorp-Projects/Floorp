# Publish Your Crate!

That's it! Now we can publish our crate on crates.io and we can write a nice,
Rust-y API wrapping the raw FFI bindings in a safe interface. However, there is
already a [`bzip2-sys`][bz-sys] crate providing raw FFI bindings, and there is
already a [`bzip2`][bz] crate providing a nice, safe, Rust-y API on top of the
bindings, so we have nothing left to do here!

Check out the [full code on Github!][example]

[bz-sys]: https://crates.io/crates/bzip2-sys
[bz]: https://crates.io/crates/bzip2
[example]: https://github.com/fitzgen/libbindgen-tutorial-bzip2-sys
