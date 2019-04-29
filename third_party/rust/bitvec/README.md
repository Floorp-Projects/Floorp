# `BitVec` – Managing memory bit by bit

[![Crate][crate_img]][crate]
[![Documentation][docs_img]][docs]
[![License][license_img]][license_file]
[![Continuous Integration][travis_img]][travis]
[![Code Coverage][codecov_img]][codecov]
[![Crate Downloads][downloads_img]][crate]

This crate provides packed bit-level analogues to `[T]` and `Vec<T>`. The slice
type `BitSlice` and the vector type `BitVec` allow bitwise access to a region of
memory in any endian ordering or underlying primitive type. This permits
construction of space-efficient sets or fine-grained control over the values in
a region of memory.

`BitVec` is a strict expansion of `BitSlice` to include allocation management.
Since `BitVec` is shorter to type, the rest of this document will use it by
default, and mark out sections that apply *only* to the vector type and not to
the slice type. Unless marked, assume that the text applies to both.

`BitVec` is generic over an ordering cursor, using the trait `Cursor`, and the
primitive type, using the trait `Bits`. This means that `BitVec` structures can
be built with a great deal of flexibility over how they manage their memory and
translate between the in-memory representation and their semantic contents.

`BitVec` acts as closely to a standard `Vec` as possible, and can be assumed by
default to be what a `Vec<u1>` would be if such a type were possible to express
in Rust. It has stack semantics, in that push and pop operations take place only
on one end of the `BitVec`’s buffer. It supports iteration, bitwise operations,
and rendering for `Display` and `Debug`.

## How Is This Different Than the `bit_vec` Crate

- It is more recently actively maintained (I may, in the future as of this
  writing, let it lapse)
- It doesn’t have a hyphen in the name, so you don’t have to deal with the
  hyphen/underscore dichotomy.
- My `BitVec` structure is exactly the size of a `Vec`; theirs is larger.
- I have a `BitSlice` borrowed view.
- My types implement all of the standard library’s slice and vector APIs

## Why Would You Use This

- You need to directly control a bitstream’s representation in memory.
- You need to do unpleasant things with communications protocols.
- You need a list of `bool`s that doesn’t waste 7 bits for every bit used.
- You need to do set arithmetic, or numeric arithmetic, on those lists.

## Usage

**Minimum Rust Version**: `1.31.0`

I wrote this crate because I was unhappy with the other bit-vector crates
available. I specifically need to manage raw memory in bit-level precision, and
this is not a behavior pattern the other bit-vector crates made easily available
to me. This served as the guiding star for my development process on this crate,
and remains the crate’s primary goal.

To this end, the default type parameters for the `BitVec` type use `u8` as the
storage primitive and use big-endian ordering of bits: the forwards direction is
from MSb to LSb, and the backwards direction is from LSb to MSb.

To use this crate, you need to depend on it in `Cargo.toml`:

```toml
[dependencies]
bitvec = "0.11"
```

and include it in your crate root `src/main.rs` or `src/lib.rs`:

```rust,no-run
extern crate bitvec;

use bitvec::prelude::*;
```

This imports the following symbols:

- `bitvec!` – a macro similar to `vec!`, which allows the creation of `BitVec`s
  of any desired endianness, storage type, and contents. The documentation page
  has a detailed explanation of its syntax.

- `BitSlice<C: Cursor, T: Bits>` – the actual bit-slice reference type. It is
  generic over a cursor type (`C`) and storage type (`T`). Note that `BitSlice`
  is unsized, and can never be held directly; it must always be behind a
  reference such as `&BitSlice` or `&mut BitSlice`.

  Furthermore, it is *impossible* to put `BitSlice` into any kind of intelligent
  pointer such as a `Box` or `Rc`! Any work that involves managing the memory
  behind a bitwise type *must* go through `BitBox` or `BitVec` instead. This may
  change in the future as I learn how to better manage this library, but for now
  this limitation stands.

- `BitVec<C: Cursor, T: Bits>` – the actual bit-vector structure type. It is
  generic over a cursor type (`C`) and storage type (`T`).

- `Cursor` – an open trait that defines an ordering schema for `BitVec` to use.
  Little and big endian orderings are provided by default. If you wish to
  implement other ordering types, the `Cursor` trait requires one function:

  - `fn at<T: Bits>(index: u8) -> u8` takes a semantic index and computes a bit
    offset into the primitive `T` for it.

- `BigEndian` – a zero-sized struct that implements `Cursor` by defining the
  forward direction as towards LSb and the backward direction as towards MSb.

- `LittleEndian` – a zero-sized struct that implements `Cursor` by defining the
  forward direction as towards MSb and the backward direction as towards LSb.

- `Bits` – a sealed trait that provides generic access to the four Rust
  primitives usable as storage types: `u8`, `u16`, `u32`, and `u64`. `usize`
  and the signed integers do *not* implement `Bits` and cannot be used as the
  storage type. `u128` also does not implement `Bits`, as I am not confident in
  its memory representation.

`BitVec` has the same API as `Vec`, and should be easy to use.

The `bitvec!` macro requires type information as its first two arguments.
Because macros do not have access to the type checker, this currently only
accepts the literal tokens `BigEndian` or `LittleEndian` as the first argument,
one of the four unsigned integer primitives as the second argument, and then as
many values as you wish to insert into the `BitVec`. It accepts any integer
value, and maps them to bits by comparing against 0. `0` becomes `0` and any
other integer, whether it is odd or not, becomes `1`. While the syntax is loose,
you should only use `0` and `1` to fill the macro, for readability and lack of
surprise.

### `no_std`

This crate can be used in `#![no_std]` libraries, by disabling the default
feature set. In your `Cargo.toml`, write:

```toml
[dependencies]
bitvec = { version = "0.11", default-features = false }
```

or

```toml
[dependencies.bitvec]
version = "0.11"
default-features = false
```

This turns off the standard library imports *and* all usage of dynamic memory
allocation. Without an allocator, the `bitvec!` macro and the `BitVec` type are
both disable and removed from the library, leaving only the `BitSlice` type.

To use `bitvec` in a `#![no_std]` environment that *does* have an allocator,
re-enable the `alloc` feature, like so:

```toml
[dependencies.bitvec]
version = "0.11"
default-features = false
features = ["alloc"]
```

The `alloc` feature restores `bitvec!` and `BitVec`, as well as the `BitSlice`
interoperability with `BitVec`. The only difference between `alloc` and `std` is
the presence of the standard library façade and runtime support.

The `std` feature turns on `alloc`, so using this crate without any feature
flags *or* by explicitly enabling the `std` feature will enable full
functionality.

## Example

```rust
extern crate bitvec;

use bitvec::prelude::*;

use std::iter::repeat;

fn main() {
    let mut bv = bitvec![BigEndian, u8; 0, 1, 0, 1];
    bv.reserve(8);
    bv.extend(repeat(false).take(4).chain(repeat(true).take(4)));

    //  Memory access
    assert_eq!(bv.as_slice(), &[0b0101_0000, 0b1111_0000]);
    //                 index 0 -^               ^- index 11
    assert_eq!(bv.len(), 12);
    assert!(bv.capacity() >= 16);

    //  Set operations
    bv &= repeat(true);
    bv = bv | repeat(false);
    bv ^= repeat(true);
    bv = !bv;

    //  Arithmetic operations
    let one = bitvec![1];
    bv += one.clone();
    assert_eq!(bv.as_slice(), &[0b0101_0001, 0b0000_0000]);
    bv -= one.clone();
    assert_eq!(bv.as_slice(), &[0b0101_0000, 0b1111_0000]);

    //  Borrowing iteration
    let mut iter = bv.iter();
    //  index 0
    assert_eq!(iter.next().unwrap(), false);
    //  index 11
    assert_eq!(iter.next_back().unwrap(), true);
    assert_eq!(iter.len(), 10);
}
```

Immutable and mutable access to the underlying memory is provided by the `AsRef`
and `AsMut` implementations, so the `BitVec` can be readily passed to transport
functions.

`BitVec` implements `Borrow` down to `BitSlice`, and `BitSlice` implements
`ToOwned` up to `BitVec`, so they can be used in a `Cow` or wherever this API
is desired. Any case where a `Vec`/`[T]` pair cannot be replaced with a
`BitVec`/`BitSlice` pair is a bug in this library, and a bug report is
appropriate.

`BitVec` can relinquish its owned memory as a `Box<[T]>` via the
`.into_boxed_slice()` method, and `BitSlice` can relinquish access to its memory
simply by going out of scope.

## Planned Features

Contributions of items in this list are *absolutely* welcome! Contributions of
other features are also welcome, but I’ll have to be sold on them.

- Creation of specialized pointers `Rc<BitSlice>` and `Arc<BitSlice>`.

[codecov]: https://codecov.io/gh/myrrlyn/bitvec "Code Coverage"
[codecov_img]: https://img.shields.io/codecov/c/github/myrrlyn/bitvec.svg?logo=codecov "Code Coverage Display"
[crate]: https://crates.io/crates/bitvec "Crate Link"
[crate_img]: https://img.shields.io/crates/v/bitvec.svg?logo=rust "Crate Page"
[docs]: https://docs.rs/bitvec "Documentation"
[docs_img]: https://docs.rs/bitvec/badge.svg "Documentation Display"
[downloads_img]: https://img.shields.io/crates/dv/bitvec.svg?logo=rust "Crate Downloads"
[license_file]: https://github.com/myrrlyn/bitvec/blob/master/LICENSE.txt "License File"
[license_img]: https://img.shields.io/crates/l/bitvec.svg "License Display"
[travis]: https://travis-ci.org/myrrlyn/bitvec "Travis CI"
[travis_img]: https://img.shields.io/travis/myrrlyn/bitvec.svg?logo=travis "Travis CI Display"
