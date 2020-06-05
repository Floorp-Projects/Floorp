# `bumpalo`


**A fast bump allocation arena for Rust.**

[![](https://docs.rs/bumpalo/badge.svg)](https://docs.rs/bumpalo/)
[![](https://img.shields.io/crates/v/bumpalo.svg)](https://crates.io/crates/bumpalo)
[![](https://img.shields.io/crates/d/bumpalo.svg)](https://crates.io/crates/bumpalo)
[![Build Status](https://dev.azure.com/fitzgen/bumpalo/_apis/build/status/fitzgen.bumpalo?branchName=master)](https://dev.azure.com/fitzgen/bumpalo/_build/latest?definitionId=2&branchName=master)

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

assert!(scooter.scritches_required);
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

