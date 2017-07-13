owning-ref-rs
==============

A library for creating references that carry their owner with them.

This can sometimes be useful because Rust borrowing rules normally prevent
moving a type that has been moved from. For example, this kind of code gets rejected:

```rust
fn return_owned_and_referenced<'a>() -> (Vec<u8>, &'a [u8]) {
    let v = vec![1, 2, 3, 4];
    let s = &v[1..3];
    (v, s)
}
```

This library enables this safe usage by keeping the owner and the reference
bundled together in a wrapper type that ensure that lifetime constraint:

```rust
fn return_owned_and_referenced() -> OwningRef<Vec<u8>, [u8]> {
    let v = vec![1, 2, 3, 4];
    let or = OwningRef::new(v);
    let or = or.map(|v| &v[1..3]);
    or
}
```

[![Travis-CI Status](https://travis-ci.org/Kimundi/owning-ref-rs.png?branch=master)](https://travis-ci.org/Kimundi/owning-ref-rs)

# Getting Started

[owning-ref-rs is available on crates.io](https://crates.io/crates/owning_ref).
It is recommended to look there for the newest released version, as well as links to the newest builds of the docs.

At the point of the last update of this README, the latest published version could be used like this:

Add the following dependency to your Cargo manifest...

```toml
[dependencies]
owning_ref = "0.3"
```

...and see the [docs](http://kimundi.github.io/owning-ref-rs/owning_ref/index.html) for how to use it.

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
