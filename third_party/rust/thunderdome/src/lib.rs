/*!
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

## Basic Examples

```rust
# use thunderdome::{Arena, Index};
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

## Comparison With Similar Crates

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

## Minimum Supported Rust Version (MSRV)

Thunderdome supports Rust 1.34.1 and newer. Until Thunderdome reaches 1.0,
changes to the MSRV will require major version bumps. After 1.0, MSRV changes
will only require minor version bumps, but will need significant justification.
*/

#![forbid(missing_docs)]
// This crate is sensitive to integer overflow and wrapping behavior. As such,
// we should usually use methods like `checked_add` and `checked_sub` instead
// of the `Add` or `Sub` operators.
#![deny(clippy::integer_arithmetic)]

mod arena;
mod drain;
mod free_pointer;
mod generation;
mod into_iter;
mod iter;
mod iter_mut;

pub use crate::arena::{Arena, Index};
pub use crate::drain::Drain;
pub use crate::into_iter::IntoIter;
pub use crate::iter::Iter;
pub use crate::iter_mut::IterMut;
