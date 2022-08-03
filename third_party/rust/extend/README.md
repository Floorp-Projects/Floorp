# extend

[![Crates.io](https://img.shields.io/crates/v/extend.svg)](https://crates.io/crates/extend)
[![Docs](https://docs.rs/extend/badge.svg)](https://docs.rs/extend)
[![dependency status](https://deps.rs/repo/github/davidpdrsn/extend/status.svg)](https://deps.rs/repo/github/davidpdrsn/extend)
[![Build status](https://github.com/davidpdrsn/extend/workflows/CI/badge.svg)](https://github.com/davidpdrsn/extend/actions)
![maintenance-status](https://img.shields.io/badge/maintenance-passively--maintained-yellowgreen.svg)

Create extensions for types you don't own with [extension traits] but without the boilerplate.

Example:

```rust
use extend::ext;

#[ext]
impl<T: Ord> Vec<T> {
    fn sorted(mut self) -> Self {
        self.sort();
        self
    }
}

fn main() {
    assert_eq!(
        vec![1, 2, 3],
        vec![2, 3, 1].sorted(),
    );
}
```

[extension traits]: https://dev.to/matsimitsu/extending-existing-functionality-in-rust-with-traits-in-rust-3622
