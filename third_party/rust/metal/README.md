# metal-rs
[![Actions Status](https://github.com/gfx-rs/metal-rs/workflows/ci/badge.svg)](https://github.com/gfx-rs/metal-rs/actions)
[![Crates.io](https://img.shields.io/crates/v/metal.svg?label=metal)](https://crates.io/crates/metal)

<p align="center">
  <img width="150" height="150" src="./assets/metal.svg">
</p>

<p align="center">Unsafe Rust bindings for the Metal 3D Graphics API.</p>

## Documentation

Note that [docs.rs](docs.rs) will fail to build the (albeit limited) documentation for this crate!
They build in a Linux container, but of course this will only compile on MacOS.

Please build the documentation yourself with `cargo docs`.

## Examples

The [examples](/examples) directory highlights different ways of using the Metal graphics API for rendering
and computation.

Examples can be run using commands such as:

```
# Replace `window` with the name of the example that you would like to run
cargo run --example window
```

## License

Licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
