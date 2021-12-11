# Performance profiling

While the rest of the book provides practical advice on how to improve the performance
of SIMD code, this chapter is dedicated to [**performance profiling**][profiling].
Profiling consists of recording a program's execution in order to identify program
hotspots.

**Important**: most profilers require debug information in order to accurately
link the program hotspots back to the corresponding source code lines. Rust will
disable debug info generation by default for optimized builds, but you can change
that [in your `Cargo.toml`][cargo-ref].

[profiling]: https://en.wikipedia.org/wiki/Profiling_(computer_programming)
[cargo-ref]: https://doc.rust-lang.org/cargo/reference/manifest.html#the-profile-sections
