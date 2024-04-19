# Enabling target features

Not all processors of a certain architecture will have SIMD processing units,
and using a SIMD instruction which is not supported will trigger undefined behavior.

To allow building safe, portable programs, the Rust compiler will **not**, by default,
generate any sort of vector instructions, unless it can statically determine
they are supported. For example, on AMD64, SSE2 support is architecturally guaranteed.
The `x86_64-apple-darwin` target enables up to SSSE3. The get a defintive list of
which features are enabled by default on various platforms, refer to the target
specifications [in the compiler's source code][targets].

[targets]: https://github.com/rust-lang/rust/tree/master/src/librustc_target/spec
