# `arraydeque`

[![build status](https://travis-ci.org/goandylok/arraydeque.svg?branch=master)](https://travis-ci.org/goandylok/arraydeque)
[![codecov](https://codecov.io/gh/goandylok/arraydeque/branch/master/graph/badge.svg)](https://codecov.io/gh/goandylok/arraydeque)
[![crates.io](https://img.shields.io/crates/v/arraydeque.svg)](https://crates.io/crates/arraydeque)
[![docs.rs](https://docs.rs/arraydeque/badge.svg)](https://docs.rs/arraydeque)

A circular buffer with fixed capacity.  Requires Rust 1.15+.

This crate is inspired by [**bluss/arrayvec**](https://github.com/bluss/arrayvec)

### [**Documentation**](https://docs.rs/arraydeque)

## Feature Flags

The **arraydeque** crate has the following cargo feature flags:

- `std`
  - Optional, enabled by default
  - Use libstd


- `use_union`
  - Optional
  - Requires Rust nightly channel
  - Use the unstable feature untagged unions for the internal implementation,
    which has reduced space overhead


- `use_generic_array`
  - Optional
  - Requires Rust stable channel
  - Depend on generic-array and allow using it just like a fixed
    size array for ArrayDeque storage.    


## Usage

First, add the following to your `Cargo.toml`:

```toml
[dependencies]
arraydeque = "0.2"
```

Next, add this to your crate root:

```rust
extern crate arraydeque;
```

Currently arraydeque by default links to the standard library, but if you would
instead like to use arraydeque in a `#![no_std]` situation or crate you can
request this via:

```toml
[dependencies]
arraydeque = { version = "0.2", default-features = false }
```
## Capacity

Note that the `capacity()` is always `backed_array.len() - 1`.
[Read more]

[Read more]: https://en.wikipedia.org/wiki/Circular_buffer

## Example

### Push & Pop

```rust
extern crate arraydeque;

use arraydeque::ArrayDeque;

fn main() {
    let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();
    assert_eq!(vector.capacity(), 7);
    assert_eq!(vector.len(), 0);

    vector.push_back(1);
    vector.push_back(2);
    assert_eq!(vector.len(), 2);

    assert_eq!(vector.pop_front(), Some(1));
    assert_eq!(vector.pop_front(), Some(2));
    assert_eq!(vector.pop_front(), None);
}
```

### Insert & Remove

```rust
use arraydeque::ArrayDeque;

let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();

vector.push_back(11);
vector.push_back(13);
vector.insert(1, 12);
vector.remove(0);

assert_eq!(vector[0], 12);
assert_eq!(vector[1], 13);
```

### Append & Extend

```rust
use arraydeque::ArrayDeque;

let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();
let mut vector2: ArrayDeque<[_; 8]> = ArrayDeque::new();

vector.extend(0..5);
vector2.extend(5..7);

assert_eq!(format!("{:?}", vector), "[0, 1, 2, 3, 4]");
assert_eq!(format!("{:?}", vector2), "[5, 6]");

vector.append(&mut vector2);

assert_eq!(format!("{:?}", vector), "[0, 1, 2, 3, 4, 5, 6]");
assert_eq!(format!("{:?}", vector2), "[]");
```

### Iterator

```rust
use arraydeque::ArrayDeque;

let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();

vector.extend(0..5);

let iters: Vec<_> = vector.into_iter().collect();
assert_eq!(iters, vec![0, 1, 2, 3, 4]);
```

### From Iterator

```rust
use arraydeque::ArrayDeque;

let vector: ArrayDeque<[_; 8]>;
let vector2: ArrayDeque<[_; 8]>;

vector = vec![0, 1, 2, 3, 4].into_iter().collect();

vector2 = (0..5).into_iter().collect();

assert_eq!(vector, vector2);
```

### Generic Array

```toml
[dependencies]
generic-array = "0.5.1"

[dependencies.arraydeque]
version = "0.2"
features = ["use_generic_array"]
```
```rust
#[macro_use]
extern crate generic_array;
extern crate arraydeque;

use generic_array::GenericArray;
use generic_array::typenum::U41;

use arraydeque::ArrayDeque;

fn main() {
    let mut vec: ArrayDeque<GenericArray<i32, U41>> = ArrayDeque::new();

    assert_eq!(vec.len(), 0);
    assert_eq!(vec.capacity(), 40);

    vec.extend(0..20);

    assert_eq!(vec.len(), 20);
    assert_eq!(vec.into_iter().take(5).collect::<Vec<_>>(), vec![0, 1, 2, 3, 4]);
}
```

## Contribution

All kinds of contribution are welcomed.

- **Issus.** Feel free to open an issue when you find typos, bugs, or have any question.
- **Pull requests**. New collection, better implementation, more tests, more documents and typo fixes are all welcomed.

## License

Licensed under MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)
