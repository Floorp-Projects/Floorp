![Rust CI](https://github.com/Gankra/thin-vec/workflows/Rust/badge.svg?branch=master) [![crates.io](https://img.shields.io/crates/v/thin-vec.svg)](https://crates.io/crates/thin-vec) [![](https://docs.rs/thin-vec/badge.svg)](https://docs.rs/thin-vec)

# thin-vec

ThinVec is a Vec that stores its length and capacity inline, making it take up
less space.

Currently this crate mostly exists to facilitate Gecko (Firefox) FFI, but it
works perfectly fine as a native rust library as well.
