# Library Usage with `build.rs`

💡 This is the recommended way to use `bindgen`. 💡

Often times C and C++ headers will have platform- and architecture-specific
`#ifdef`s that affect the shape of the Rust FFI bindings we need to create to
interface Rust code with the outside world. By using `bindgen` as a library
inside your `build.rs`, you can generate bindings for the current target
on-the-fly. Otherwise, you would need to generate and maintain
`x86_64-unknown-linux-gnu-bindings.rs`, `x86_64-apple-darwin-bindings.rs`,
etc... separate bindings files for each of your supported targets, which can be
a huge pain. The downside is that everyone building your crate also needs
`libclang` available to run `bindgen`.

## Library API Documentation

[📚 There is complete API reference documentation on docs.rs 📚](https://docs.rs/bindgen)

## Tutorial

The next section contains a detailed, step-by-step tutorial for using `bindgen`
as a library inside `build.rs`.
