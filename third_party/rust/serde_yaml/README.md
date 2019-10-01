Serde YAML
==========

[![Build Status](https://api.travis-ci.org/dtolnay/serde-yaml.svg?branch=master)][travis]
[![Latest Version](https://img.shields.io/crates/v/serde_yaml.svg)][crates.io]
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)][docs.rs]

[travis]: https://travis-ci.org/dtolnay/serde-yaml
[crates.io]: https://crates.io/crates/serde_yaml
[docs.rs]: https://docs.rs/serde_yaml

This crate is a Rust library for using the [Serde] serialization framework with
data in [YAML] file format.

[Serde]: https://github.com/serde-rs/serde
[YAML]: http://yaml.org

This library does not reimplement a YAML parser; it uses [yaml-rust] which is a
pure Rust YAML 1.2 implementation.

[yaml-rust]: https://github.com/chyh1990/yaml-rust

## Dependency

```toml
[dependencies]
serde = "1.0"
serde_yaml = "0.8"
```

Release notes are available under [GitHub releases].

[GitHub releases]: https://github.com/dtolnay/serde-yaml/releases

## Using Serde YAML

[API documentation is available in rustdoc form][docs.rs] but the general idea
is:

```rust
use std::collections::BTreeMap;

fn main() -> Result<(), serde_yaml::Error> {
    // You have some type.
    let mut map = BTreeMap::new();
    map.insert("x".to_string(), 1.0);
    map.insert("y".to_string(), 2.0);

    // Serialize it to a YAML string.
    let s = serde_yaml::to_string(&map)?;
    assert_eq!(s, "---\nx: 1\ny: 2");

    // Deserialize it back to a Rust type.
    let deserialized_map: BTreeMap<String, f64> = serde_yaml::from_str(&s)?;
    assert_eq!(map, deserialized_map);
    Ok(())
}
```

It can also be used with Serde's derive macros to handle structs and enums
defined by your program.

```toml
[dependencies]
serde = { version = "1.0", features = ["derive"] }
serde_yaml = "0.8"
```

```rust
use serde::{Serialize, Deserialize};

#[derive(Debug, PartialEq, Serialize, Deserialize)]
struct Point {
    x: f64,
    y: f64,
}

fn main() -> Result<(), serde_yaml::Error> {
    let point = Point { x: 1.0, y: 2.0 };

    let s = serde_yaml::to_string(&point)?;
    assert_eq!(s, "---\nx: 1\ny: 2");

    let deserialized_point: Point = serde_yaml::from_str(&s)?;
    assert_eq!(point, deserialized_point);
    Ok(())
}
```

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Serde YAML by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
