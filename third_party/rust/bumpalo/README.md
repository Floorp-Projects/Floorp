# `bumpalo`


**A fast bump allocation arena for Rust.**

[![](https://docs.rs/bumpalo/badge.svg)](https://docs.rs/bumpalo/)
[![](https://img.shields.io/crates/v/bumpalo.svg)](https://crates.io/crates/bumpalo)
[![](https://img.shields.io/crates/d/bumpalo.svg)](https://crates.io/crates/bumpalo)
[![Build Status](https://github.com/fitzgen/bumpalo/workflows/Rust/badge.svg)](https://github.com/fitzgen/bumpalo/actions?query=workflow%3ARust)

![](https://github.com/fitzgen/bumpalo/raw/master/bumpalo.png)

### Bump Allocation

Bump allocation is a fast, but limited approach to allocation. We have a chunk
of memory, and we maintain a pointer within that memory. Whenever we allocate an
object, we do a quick test that we have enough capacity left in our chunk to
allocate the object and then update the pointer by the object's size. *That's
it!*

The disadvantage of bump allocation is that there is no general way to
deallocate individual objects or reclaim the memory region for a
no-longer-in-use object.

These trade offs make bump allocation well-suited for *phase-oriented*
allocations. That is, a group of objects that will all be allocated during the
same program phase, used, and then can all be deallocated together as a group.

### Deallocation en Masse, but No `Drop`

To deallocate all the objects in the arena at once, we can simply reset the bump
pointer back to the start of the arena's memory chunk. This makes mass
deallocation *extremely* fast, but allocated objects' `Drop` implementations are
not invoked.

> **However:** [`bumpalo::boxed::Box<T>`][crate::boxed::Box] can be used to wrap
> `T` values allocated in the `Bump` arena, and calls `T`'s `Drop`
> implementation when the `Box<T>` wrapper goes out of scope. This is similar to
> how [`std::boxed::Box`] works, except without deallocating its backing memory.

[`std::boxed::Box`]: https://doc.rust-lang.org/std/boxed/struct.Box.html

### What happens when the memory chunk is full?

This implementation will allocate a new memory chunk from the global allocator
and then start bump allocating into this new memory chunk.

### Example

```rust
use bumpalo::Bump;
use std::u64;

struct Doggo {
    cuteness: u64,
    age: u8,
    scritches_required: bool,
}

// Create a new arena to bump allocate into.
let bump = Bump::new();

// Allocate values into the arena.
let scooter = bump.alloc(Doggo {
    cuteness: u64::max_value(),
    age: 8,
    scritches_required: true,
});

// Exclusive, mutable references to the just-allocated value are returned.
assert!(scooter.scritches_required);
scooter.age += 1;
```

### Collections

When the `"collections"` cargo feature is enabled, a fork of some of the `std`
library's collections are available in the `collections` module. These
collection types are modified to allocate their space inside `bumpalo::Bump`
arenas.

```rust
use bumpalo::{Bump, collections::Vec};

// Create a new bump arena.
let bump = Bump::new();

// Create a vector of integers whose storage is backed by the bump arena. The
// vector cannot outlive its backing arena, and this property is enforced with
// Rust's lifetime rules.
let mut v = Vec::new_in(&bump);

// Push a bunch of integers onto `v`!
for i in 0..100 {
    v.push(i);
}
```

Eventually [all `std` collection types will be parameterized by an
allocator](https://github.com/rust-lang/rust/issues/42774) and we can remove
this `collections` module and use the `std` versions.

For unstable, nightly-only support for custom allocators in `std`, see the
`allocator_api` section below.

### `bumpalo::boxed::Box`

When the `"boxed"` cargo feature is enabled, a fork of `std::boxed::Box` library
is available in the `boxed` module. This `Box` type is modified to allocate its
space inside `bumpalo::Bump` arenas.

**A `Box<T>` runs `T`'s drop implementation when the `Box<T>` is dropped.** You
can use this to work around the fact that `Bump` does not drop values allocated
in its space itself.

```rust
use bumpalo::{Bump, boxed::Box};
use std::sync::atomic::{AtomicUsize, Ordering};

static NUM_DROPPED: AtomicUsize = AtomicUsize::new(0);

struct CountDrops;

impl Drop for CountDrops {
    fn drop(&mut self) {
        NUM_DROPPED.fetch_add(1, Ordering::SeqCst);
    }
}

// Create a new bump arena.
let bump = Bump::new();

// Create a `CountDrops` inside the bump arena.
let mut c = Box::new_in(CountDrops, &bump);

// No `CountDrops` have been dropped yet.
assert_eq!(NUM_DROPPED.load(Ordering::SeqCst), 0);

// Drop our `Box<CountDrops>`.
drop(c);

// Its `Drop` implementation was run, and so `NUM_DROPS` has been incremented.
assert_eq!(NUM_DROPPED.load(Ordering::SeqCst), 1);
```

### `#![no_std]` Support

Bumpalo is a `no_std` crate. It depends only on the `alloc` and `core` crates.

### Thread support

The `Bump` is `!Send`, which makes it hard to use in certain situations around threads ‒ for
example in `rayon`.

The [`bumpalo-herd`](https://crates.io/crates/bumpalo-herd) crate provides a pool of `Bump`
allocators for use in such situations.

### Nightly Rust `feature(allocator_api)` Support

The unstable, nightly-only Rust `allocator_api` feature defines an `Allocator`
trait and exposes custom allocators for `std` types. Bumpalo has a matching
`allocator_api` cargo feature to enable implementing `Allocator` and using
`Bump` with `std` collections. Note that, as `feature(allocator_api)` is
unstable and only in nightly Rust, Bumpalo's matching `allocator_api` cargo
feature should be considered unstable, and will not follow the semver
conventions that the rest of the crate does.

First, enable the `allocator_api` feature in your `Cargo.toml`:

```toml
[dependencies]
bumpalo = { version = "3.4.0", features = ["allocator_api"] }
```

Next, enable the `allocator_api` nightly Rust feature in your `src/lib.rs` or `src/main.rs`:

```rust
#![feature(allocator_api)]
```

Finally, use `std` collections with `Bump`, so that their internal heap
allocations are made within the given bump arena:

```rust
#![feature(allocator_api)]
use bumpalo::Bump;

// Create a new bump arena.
let bump = Bump::new();

// Create a `Vec` whose elements are allocated within the bump arena.
let mut v = Vec::new_in(&bump);
v.push(0);
v.push(1);
v.push(2);
```

#### Minimum Supported Rust Version (MSRV)

This crate is guaranteed to compile on stable Rust 1.44 and up. It might compile
with older versions but that may change in any new patch release.

We reserve the right to increment the MSRV on minor releases, however we will strive
to only do it deliberately and for good reasons.

