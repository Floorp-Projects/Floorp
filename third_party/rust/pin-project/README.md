# pin-project

[![crates-badge]][crates-url]
[![docs-badge]][docs-url]
[![license-badge]][license]
[![rustc-badge]][rustc-url]

[crates-badge]: https://img.shields.io/crates/v/pin-project.svg
[crates-url]: https://crates.io/crates/pin-project
[docs-badge]: https://docs.rs/pin-project/badge.svg
[docs-url]: https://docs.rs/pin-project
[license-badge]: https://img.shields.io/crates/l/pin-project.svg
[license]: #license
[rustc-badge]: https://img.shields.io/badge/rustc-1.33+-lightgray.svg
[rustc-url]: https://blog.rust-lang.org/2019/02/28/Rust-1.33.0.html

A crate for safe and ergonomic pin-projection.

[Documentation][docs-url]

[Examples](examples/README.md)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
pin-project = "0.4"
```

The current pin-project requires Rust 1.33 or later.

## Examples

[`pin_project`] attribute creates a projection struct covering all the fields.

```rust
use pin_project::pin_project;
use std::pin::Pin;

#[pin_project]
struct Struct<T, U> {
    #[pin]
    pinned: T,
    unpinned: U,
}

impl<T, U> Struct<T, U> {
    fn foo(self: Pin<&mut Self>) {
        let this = self.project();
        let _: Pin<&mut T> = this.pinned; // Pinned reference to the field
        let _: &mut U = this.unpinned; // Normal reference to the field
    }
}
```

[Code like this will be generated](examples/struct-default-expanded.rs)

See [API documentation][docs-url] for more details.

Also, there are examples and generated code of each feature in [examples](examples/README.md) directory.

[`pin_project`]: https://docs.rs/pin-project/0.4/pin_project/attr.pin_project.html

## License

Licensed under either of

* Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or <http://www.apache.org/licenses/LICENSE-2.0>)
* MIT license ([LICENSE-MIT](LICENSE-MIT) or <http://opensource.org/licenses/MIT>)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.
