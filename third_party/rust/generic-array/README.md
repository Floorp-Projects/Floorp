[![Crates.io](https://img.shields.io/crates/v/generic-array.svg)](https://crates.io/crates/generic-array)
[![Build Status](https://travis-ci.org/fizyk20/generic-array.svg?branch=master)](https://travis-ci.org/fizyk20/generic-array)
# generic-array

This crate implements generic array types for Rust.

[Documentation](http://fizyk20.github.io/generic-array/generic_array/)

## Usage

The Rust arrays `[T; N]` are problematic in that they can't be used generically with respect to `N`, so for example this won't work:

```rust
struct Foo<N> {
	data: [i32; N]
}
```

**generic-array** defines a new trait `ArrayLength<T>` and a struct `GenericArray<T, N: ArrayLength<T>>`, which let the above be implemented as:

```rust
struct Foo<N: ArrayLength<i32>> {
	data: GenericArray<i32, N>
}
```

To actually define a type implementing `ArrayLength`, you can use unsigned integer types defined in [typenum](https://github.com/paholg/typenum) crate - for example, `GenericArray<T, U5>` would work almost like `[T; 5]` :)

In version 0.1.1 an `arr!` macro was introduced, allowing for creation of arrays as shown below:

```rust
let array = arr![u32; 1, 2, 3];
assert_eq!(array[2], 3);
```
