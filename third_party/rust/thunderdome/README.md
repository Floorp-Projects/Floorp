# Thunderdome

[![GitHub CI Status](https://github.com/LPGhatguy/thunderdome/workflows/CI/badge.svg)](https://github.com/LPGhatguy/thunderdome/actions)
[![thunderdome on crates.io](https://img.shields.io/crates/v/thunderdome.svg)](https://crates.io/crates/thunderdome)
[![thunderdome docs](https://img.shields.io/badge/docs-docs.rs-orange.svg)](https://docs.rs/thunderdome)

Thunderdome is a ~gladitorial~ generational arena inspired by
[generational-arena](https://crates.io/crates/generational-arena),
[slotmap](https://crates.io/crates/slotmap), and
[slab](https://crates.io/crates/slab). It provides constant time insertion,
lookup, and removal via small (8 byte) keys returned from `Arena`.

Thunderdome's key type, `Index`, is still 8 bytes when put inside of an
`Option<T>` thanks to Rust's `NonZero*` types.

### Basic Examples

```rust
let mut arena = Arena::new();

let foo = arena.insert("Foo");
let bar = arena.insert("Bar");

assert_eq!(arena[foo], "Foo");
assert_eq!(arena[bar], "Bar");

arena[bar] = "Replaced";
assert_eq!(arena[bar], "Replaced");

let foo_value = arena.remove(foo);
assert_eq!(foo_value, Some("Foo"));

// The slot previously used by foo will be reused for baz
let baz = arena.insert("Baz");
assert_eq!(arena[baz], "Baz");

// foo is no longer a valid key
assert_eq!(arena.get(foo), None);
```

### Comparison With Similar Crates

| Feature                      | Thunderdome | generational-arena | slotmap | slab |
|------------------------------|-------------|--------------------|---------|------|
| Generational Indices¹        | Yes         | Yes                | Yes     | No   |
| `size_of::<Index>()`         | 8           | 16                 | 8       | 8    |
| `size_of::<Option<Index>>()` | 8           | 24                 | 8       | 16   |
| Max Elements                 | 2³²         | 2⁶⁴                | 2³²     | 2⁶⁴  |
| Non-`Copy` Values            | Yes         | Yes                | Yes     | Yes  |
| `no_std` Support             | No          | Yes                | Yes     | No   |
| Serde Support                | No          | Yes                | Yes     | No   |

* Sizes calculated on rustc `1.44.0-x86_64-pc-windows-msvc`
* See [the Thunderdome comparison
  Cargo.toml](https://github.com/LPGhatguy/thunderdome/blob/main/comparison/Cargo.toml)
  for versions of each library tested.

1. Generational indices help solve the [ABA
   Problem](https://en.wikipedia.org/wiki/ABA_problem), which can cause dangling
   keys to mistakenly access newly-inserted data.

### Minimum Supported Rust Version (MSRV)

Thunderdome supports Rust 1.34.1 and newer. Until Thunderdome reaches 1.0,
changes to the MSRV will require major version bumps. After 1.0, MSRV changes
will only require minor version bumps, but will need significant justification.

## License

Licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution
Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.
