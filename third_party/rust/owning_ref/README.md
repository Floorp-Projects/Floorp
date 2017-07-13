owning-ref-rs
==============

[![Travis-CI Status](https://travis-ci.org/Kimundi/owning-ref-rs.png?branch=master)](https://travis-ci.org/Kimundi/owning-ref-rs)

A library for creating references that carry their owner with them.

For more details, see the [docs](http://kimundi.github.io/owning-ref-rs/owning_ref/index.html).

# Getting Started

[owning-ref-rs is available on crates.io](https://crates.io/crates/owning_ref).
Add the following dependency to your Cargo manifest to get the latest version of the 0.2 branch:

```toml
[dependencies]
owning_ref = "0.2"
```

To always get the latest version, add this git repository to your
Cargo manifest:

```toml
[dependencies.owning_ref]
git = "https://github.com/Kimundi/owning-ref-rs"
```
# Example

```rust
extern crate owning_ref;
use owning_ref::BoxRef;

fn main() {
    // Create an array owned by a Box.
    let arr = Box::new([1, 2, 3, 4]) as Box<[i32]>;

    // Transfer into a BoxRef.
    let arr: BoxRef<[i32]> = BoxRef::new(arr);
    assert_eq!(&*arr, &[1, 2, 3, 4]);

    // We can slice the array without losing ownership or changing type.
    let arr: BoxRef<[i32]> = arr.map(|arr| &arr[1..3]);
    assert_eq!(&*arr, &[2, 3]);

    // Also works for Arc, Rc, String and Vec!
}
```
