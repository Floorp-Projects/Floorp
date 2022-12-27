# Ordered Floats

Provides several wrapper types for Ord and Eq implementations on f64 and friends.

See the [API documentation](https://docs.rs/ordered-float) for further details.

## no_std

To use `ordered_float` without requiring the Rust standard library, disable
the default `std` feature:

```toml
[dependencies]
ordered-float = { version = "3.0", default-features = false }
```

## Optional features

The following optional features can be enabled in `Cargo.toml`:

* `bytemuck`: Adds implementations for traits provided by the `bytemuck` crate.
* `rand`: Adds implementations for various distribution types provided by the `rand` crate.
* `serde`: Implements the `serde::Serialize` and `serde::Deserialize` traits.
* `schemars`: Implements the `schemars::JsonSchema` trait.
* `proptest`: Implements the `proptest::Arbitrary` trait.
* `rkyv`: Implements `rkyv`'s `Archive`, `Serialize` and `Deserialize` traits.
* `speedy`: Implements `speedy`'s `Readable` and `Writable` traits.

## License

MIT
