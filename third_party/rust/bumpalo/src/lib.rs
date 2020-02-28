/*!

**A fast bump allocation arena for Rust.**

[![](https://docs.rs/bumpalo/badge.svg)](https://docs.rs/bumpalo/)
[![](https://img.shields.io/crates/v/bumpalo.svg)](https://crates.io/crates/bumpalo)
[![](https://img.shields.io/crates/d/bumpalo.svg)](https://crates.io/crates/bumpalo)
[![Build Status](https://dev.azure.com/fitzgen/bumpalo/_apis/build/status/fitzgen.bumpalo?branchName=master)](https://dev.azure.com/fitzgen/bumpalo/_build/latest?definitionId=2&branchName=master)

![](https://github.com/fitzgen/bumpalo/raw/master/bumpalo.png)

## Bump Allocation

Bump allocation is a fast, but limited approach to allocation. We have a chunk
of memory, and we maintain a pointer within that memory. Whenever we allocate an
object, we do a quick test that we have enough capacity left in our chunk to
allocate the object and then increment the pointer by the object's size. *That's
it!*

The disadvantage of bump allocation is that there is no general way to
deallocate individual objects or reclaim the memory region for a
no-longer-in-use object.

These trade offs make bump allocation well-suited for *phase-oriented*
allocations. That is, a group of objects that will all be allocated during the
same program phase, used, and then can all be deallocated together as a group.

## Deallocation en Masse, but No `Drop`

To deallocate all the objects in the arena at once, we can simply reset the bump
pointer back to the start of the arena's memory chunk. This makes mass
deallocation *extremely* fast, but allocated objects' `Drop` implementations are
not invoked.

## What happens when the memory chunk is full?

This implementation will allocate a new memory chunk from the global allocator
and then start bump allocating into this new memory chunk.

## Example

```
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

## Collections

When the on-by-default `"collections"` feature is enabled, a fork of some of the
`std` library's collections are available in the `collections` module. These
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

## `#![no_std]` Support

Requires the `alloc` nightly feature. Disable the on-by-default `"std"` feature:

```toml
[dependencies.bumpalo]
version = "1"
default-features = false
```

 */

#![deny(missing_debug_implementations)]
#![deny(missing_docs)]
// In no-std mode, use the alloc crate to get `Vec`.
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(not(feature = "std"), feature(alloc))]

#[cfg(feature = "std")]
extern crate core;

#[cfg(feature = "collections")]
pub mod collections;

mod alloc;

#[cfg(feature = "std")]
mod imports {
    pub use std::alloc::{alloc, dealloc, Layout};
    pub use std::cell::{Cell, UnsafeCell};
    pub use std::cmp;
    pub use std::fmt;
    pub use std::mem;
    pub use std::ptr::{self, NonNull};
    pub use std::slice;
}

#[cfg(not(feature = "std"))]
mod imports {
    extern crate alloc;
    pub use self::alloc::alloc::{alloc, dealloc, Layout};
    pub use core::cell::{Cell, UnsafeCell};
    pub use core::cmp;
    pub use core::fmt;
    pub use core::mem;
    pub use core::ptr::{self, NonNull};
    pub use core::slice;
}

use crate::imports::*;

/// An arena to bump allocate into.
///
/// ## No `Drop`s
///
/// Objects that are bump-allocated will never have their `Drop` implementation
/// called &mdash; unless you do it manually yourself. This makes it relatively
/// easy to leak memory or other resources.
///
/// If you have a type which internally manages
///
/// * an allocation from the global heap (e.g. `Vec<T>`),
/// * open file descriptors (e.g. `std::fs::File`), or
/// * any other resource that must be cleaned up (e.g. an `mmap`)
///
/// and relies on its `Drop` implementation to clean up the internal resource,
/// then if you allocate that type with a `Bump`, you need to find a new way to
/// clean up after it yourself.
///
/// Potential solutions are
///
/// * calling [`drop_in_place`][drop_in_place] or using
///   [`std::mem::ManuallyDrop`][manuallydrop] to manually drop these types,
/// * using `bumpalo::collections::Vec` instead of `std::vec::Vec`, or
/// * simply avoiding allocating these problematic types within a `Bump`.
///
/// Note that not calling `Drop` is memory safe! Destructors are never
/// guaranteed to run in Rust, you can't rely on them for enforcing memory
/// safety.
///
/// [drop_in_place]: https://doc.rust-lang.org/stable/std/ptr/fn.drop_in_place.html
/// [manuallydrop]: https://doc.rust-lang.org/stable/std/mem/struct.ManuallyDrop.html
///
/// ## Example
///
/// ```
/// use bumpalo::Bump;
///
/// // Create a new bump arena.
/// let bump = Bump::new();
///
/// // Allocate values into the arena.
/// let forty_two = bump.alloc(42);
/// assert_eq!(*forty_two, 42);
///
/// // Mutable references are returned from allocation.
/// let mut s = bump.alloc("bumpalo");
/// *s = "the bump allocator; and also is a buffalo";
/// ```
#[derive(Debug)]
pub struct Bump {
    // The current chunk we are bump allocating within.
    current_chunk_footer: Cell<NonNull<ChunkFooter>>,

    // The first chunk we were ever given, which is the head of the intrusive
    // linked list of all chunks this arena has been bump allocating within.
    all_chunk_footers: Cell<NonNull<ChunkFooter>>,
}

#[repr(C)]
#[derive(Debug)]
struct ChunkFooter {
    // Pointer to the start of this chunk allocation. This footer is always at
    // the end of the chunk.
    data: NonNull<u8>,

    // The layout of this chunk's allocation.
    layout: Layout,

    // Link to the next chunk, if any.
    next: Cell<Option<NonNull<ChunkFooter>>>,

    // Bump allocation finger that is always in the range `self.data..=self`.
    ptr: Cell<NonNull<u8>>,
}

impl Default for Bump {
    fn default() -> Bump {
        Bump::new()
    }
}

impl Drop for Bump {
    fn drop(&mut self) {
        unsafe {
            let mut footer = Some(self.all_chunk_footers.get());
            while let Some(f) = footer {
                footer = f.as_ref().next.get();
                dealloc(f.as_ref().data.as_ptr(), f.as_ref().layout);
            }
        }
    }
}

// `Bump`s are safe to send between threads because nothing aliases its owned
// chunks until you start allocating from it. But by the time you allocate from
// it, the returned references to allocations borrow the `Bump` and therefore
// prevent sending the `Bump` across threads until the borrows end.
unsafe impl Send for Bump {}

#[inline]
pub(crate) fn round_up_to(n: usize, divisor: usize) -> usize {
    debug_assert!(divisor.is_power_of_two());
    (n + divisor - 1) & !(divisor - 1)
}

// Maximum typical overhead per allocation imposed by allocators.
const MALLOC_OVERHEAD: usize = 16;

// Choose a relatively small default initial chunk size, since we double chunk
// sizes as we grow bump arenas to amortize costs of hitting the global
// allocator.
const DEFAULT_CHUNK_SIZE_WITH_FOOTER: usize = (1 << 9) - MALLOC_OVERHEAD;
const DEFAULT_CHUNK_ALIGN: usize = mem::align_of::<ChunkFooter>();

/// Wrapper around `Layout::from_size_align` that adds debug assertions.
#[inline]
unsafe fn layout_from_size_align(size: usize, align: usize) -> Layout {
    if cfg!(debug_assertions) {
        Layout::from_size_align(size, align).unwrap()
    } else {
        Layout::from_size_align_unchecked(size, align)
    }
}

impl Bump {
    fn default_chunk_layout() -> Layout {
        unsafe { layout_from_size_align(DEFAULT_CHUNK_SIZE_WITH_FOOTER, DEFAULT_CHUNK_ALIGN) }
    }

    /// Construct a new arena to bump allocate into.
    ///
    /// ## Example
    ///
    /// ```
    /// let bump = bumpalo::Bump::new();
    /// # let _ = bump;
    /// ```
    pub fn new() -> Bump {
        let chunk_footer = Self::new_chunk(None);
        Bump {
            current_chunk_footer: Cell::new(chunk_footer),
            all_chunk_footers: Cell::new(chunk_footer),
        }
    }

    /// Allocate a new chunk and return its initialized footer.
    ///
    /// If given, `layouts` is a tuple of the current chunk size and the
    /// layout of the allocation request that triggered us to fall back to
    /// allocating a new chunk of memory.
    fn new_chunk(layouts: Option<(usize, Layout)>) -> NonNull<ChunkFooter> {
        unsafe {
            let layout: Layout =
                layouts.map_or_else(Bump::default_chunk_layout, |(old_size, requested)| {
                    let old_doubled = old_size.checked_mul(2).unwrap();
                    let footer_align = mem::align_of::<ChunkFooter>();
                    debug_assert_eq!(
                        old_doubled,
                        round_up_to(old_doubled, footer_align),
                        "The old size was already a multiple of our chunk footer alignment, so no \
                         need to round it up again."
                    );

                    // Have a reasonable "doubling behavior" but ensure that if
                    // a very large size is requested we round up to that.
                    let size_to_allocate = cmp::max(old_doubled, requested.size());

                    // Handle size/alignment of our allocated chunk, taking into
                    // account an overaligned allocation if one is required.
                    // Note that we also add to the size a `ChunkFooter` because
                    // we'll be placing one at the end, and we need to at least
                    // satisfy `requested.size()` bytes.
                    let size = cmp::max(
                        size_to_allocate,
                        requested.size() + mem::size_of::<ChunkFooter>(),
                    );
                    let size = round_up_to(size, footer_align);
                    let align = cmp::max(footer_align, requested.align());

                    layout_from_size_align(size, align)
                });

            let size = layout.size();
            debug_assert!(layout.align() % mem::align_of::<ChunkFooter>() == 0);

            let data = alloc(layout);
            let data = NonNull::new(data).unwrap_or_else(|| oom());

            let next = Cell::new(None);
            let ptr = Cell::new(data);
            let footer_ptr = data.as_ptr() as usize + size - mem::size_of::<ChunkFooter>();
            let footer_ptr = footer_ptr as *mut ChunkFooter;
            ptr::write(
                footer_ptr,
                ChunkFooter {
                    data,
                    layout,
                    next,
                    ptr,
                },
            );
            NonNull::new_unchecked(footer_ptr)
        }
    }

    /// Reset this bump allocator.
    ///
    /// Performs mass deallocation on everything allocated in this arena by
    /// resetting the pointer into the underlying chunk of memory to the start
    /// of the chunk. Does not run any `Drop` implementations on deallocated
    /// objects; see [the `Bump` type's top-level
    /// documentation](./struct.Bump.html) for details.
    ///
    /// If this arena has allocated multiple chunks to bump allocate into, then
    /// the excess chunks are returned to the global allocator.
    ///
    /// ## Example
    ///
    /// ```
    /// let mut bump = bumpalo::Bump::new();
    ///
    /// // Allocate a bunch of things.
    /// {
    ///     for i in 0..100 {
    ///         bump.alloc(i);
    ///     }
    /// }
    ///
    /// // Reset the arena.
    /// bump.reset();
    ///
    /// // Allocate some new things in the space previously occupied by the
    /// // original things.
    /// for j in 200..400 {
    ///     bump.alloc(j);
    /// }
    ///```
    pub fn reset(&mut self) {
        // Takes `&mut self` so `self` must be unique and there can't be any
        // borrows active that would get invalidated by resetting.
        unsafe {
            let mut footer = Some(self.all_chunk_footers.get());

            // Reset the pointer in each of our chunks.
            while let Some(f) = footer {
                footer = f.as_ref().next.get();

                if f == self.current_chunk_footer.get() {
                    // If this is the current chunk, then reset the bump finger
                    // to the start of the chunk.
                    f.as_ref()
                        .ptr
                        .set(NonNull::new_unchecked(f.as_ref().data.as_ptr() as *mut u8));
                    f.as_ref().next.set(None);
                    self.all_chunk_footers.set(f);
                } else {
                    // If this is not the current chunk, return it to the global
                    // allocator.
                    dealloc(f.as_ref().data.as_ptr(), f.as_ref().layout.clone());
                }
            }

            debug_assert_eq!(
                self.all_chunk_footers.get(),
                self.current_chunk_footer.get(),
                "The current chunk should be the list head of all of our chunks"
            );
            debug_assert!(
                self.current_chunk_footer
                    .get()
                    .as_ref()
                    .next
                    .get()
                    .is_none(),
                "We should only have a single chunk"
            );
            debug_assert_eq!(
                self.current_chunk_footer.get().as_ref().ptr.get(),
                self.current_chunk_footer.get().as_ref().data,
                "Our chunk's bump finger should be reset to the start of its allocation"
            );
        }
    }

    /// Allocate an object in this `Bump` and return an exclusive reference to
    /// it.
    ///
    /// ## Panics
    ///
    /// Panics if reserving space for `T` would cause an overflow.
    ///
    /// ## Example
    ///
    /// ```
    /// let bump = bumpalo::Bump::new();
    /// let x = bump.alloc("hello");
    /// assert_eq!(*x, "hello");
    /// ```
    #[inline(always)]
    pub fn alloc<T>(&self, val: T) -> &mut T {
        self.alloc_with(|| val)
    }

    /// Pre-allocate space for an object in this `Bump`, initializes it using
    /// the closure, then returns an exclusive reference to it.
    ///
    /// Calling `bump.alloc(x)` is essentially equivalent to calling
    /// `bump.alloc_with(|| x)`. However if you use `alloc_with`, then the
    /// closure will not be invoked until after allocating space for storing
    /// `x` on the heap.
    ///
    /// This can be useful in certain edge-cases related to compiler
    /// optimizations. When evaluating `bump.alloc(x)`, semantically `x` is
    /// first put on the stack and then moved onto the heap. In some cases,
    /// the compiler is able to optimize this into constructing `x` directly
    /// on the heap, however in many cases it does not.
    ///
    /// The function `alloc_with` tries to help the compiler be smarter. In
    /// most cases doing `bump.alloc_with(|| x)` on release mode will be
    /// enough to help the compiler to realize this optimization is valid
    /// and construct `x` directly onto the heap.
    ///
    /// ## Warning
    ///
    /// This function critically depends on compiler optimizations to achieve
    /// its desired effect. This means that it is not an effective tool when
    /// compiling without optimizations on.
    ///
    /// Even when optimizations are on, this function does not **guarantee**
    /// that the value is constructed on the heap. To the best of our
    /// knowledge no such guarantee can be made in stable Rust as of 1.33.
    ///
    /// ## Panics
    ///
    /// Panics if reserving space for `T` would cause an overflow.
    ///
    /// ## Example
    ///
    /// ```
    /// let bump = bumpalo::Bump::new();
    /// let x = bump.alloc_with(|| "hello");
    /// assert_eq!(*x, "hello");
    /// ```
    #[inline(always)]
    pub fn alloc_with<F, T>(&self, f: F) -> &mut T
    where
        F: FnOnce() -> T,
    {
        #[inline(always)]
        unsafe fn inner_writer<T, F>(ptr: *mut T, f: F)
        where
            F: FnOnce() -> T,
        {
            // This function is translated as:
            // - allocate space for a T on the stack
            // - call f() with the return value being put onto this stack space
            // - memcpy from the stack to the heap
            //
            // Ideally we want LLVM to always realize that doing a stack
            // allocation is unnecessary and optimize the code so it writes
            // directly into the heap instead. It seems we get it to realize
            // this most consistently if we put this critical line into it's
            // own function instead of inlining it into the surrounding code.
            ptr::write(ptr, f())
        }

        let layout = Layout::new::<T>();

        unsafe {
            let p = self.alloc_layout(layout);
            let p = p.as_ptr() as *mut T;
            inner_writer(p, f);
            &mut *p
        }
    }

    /// `Copy` a slice into this `Bump` and return an exclusive reference to
    /// the copy.
    ///
    /// ## Panics
    ///
    /// Panics if reserving space for the slice would cause an overflow.
    ///
    /// ## Example
    ///
    /// ```
    /// let bump = bumpalo::Bump::new();
    /// let x = bump.alloc_slice_copy(&[1, 2, 3]);
    /// assert_eq!(x, &[1, 2, 3]);
    /// ```
    #[inline(always)]
    pub fn alloc_slice_copy<T>(&self, src: &[T]) -> &mut [T]
    where
        T: Copy,
    {
        let layout = Layout::for_value(src);
        let dst = self.alloc_layout(layout).cast::<T>();

        unsafe {
            ptr::copy_nonoverlapping(src.as_ptr(), dst.as_ptr(), src.len());
            slice::from_raw_parts_mut(dst.as_ptr(), src.len())
        }
    }

    /// `Clone` a slice into this `Bump` and return an exclusive reference to
    /// the clone. Prefer `alloc_slice_copy` if `T` is `Copy`.
    ///
    /// ## Panics
    ///
    /// Panics if reserving space for the slice would cause an overflow.
    ///
    /// ## Example
    ///
    /// ```
    /// #[derive(Clone, Debug, Eq, PartialEq)]
    /// struct Sheep {
    ///     name: String,
    /// }
    ///
    /// let originals = vec![
    ///     Sheep { name: "Alice".into() },
    ///     Sheep { name: "Bob".into() },
    ///     Sheep { name: "Cathy".into() },
    /// ];
    ///
    /// let bump = bumpalo::Bump::new();
    /// let clones = bump.alloc_slice_clone(&originals);
    /// assert_eq!(originals, clones);
    /// ```
    #[inline(always)]
    pub fn alloc_slice_clone<T>(&self, src: &[T]) -> &mut [T]
    where
        T: Clone,
    {
        let layout = Layout::for_value(src);
        let dst = self.alloc_layout(layout).cast::<T>();

        unsafe {
            for (i, val) in src.iter().cloned().enumerate() {
                ptr::write(dst.as_ptr().offset(i as isize), val);
            }

            slice::from_raw_parts_mut(dst.as_ptr(), src.len())
        }
    }

    /// Allocate space for an object with the given `Layout`.
    ///
    /// The returned pointer points at uninitialized memory, and should be
    /// initialized with
    /// [`std::ptr::write`](https://doc.rust-lang.org/stable/std/ptr/fn.write.html).
    #[inline(always)]
    pub fn alloc_layout(&self, layout: Layout) -> NonNull<u8> {
        if let Some(p) = self.try_alloc_layout_fast(layout) {
            p
        } else {
            self.alloc_layout_slow(layout)
        }
    }

    #[inline(always)]
    fn try_alloc_layout_fast(&self, layout: Layout) -> Option<NonNull<u8>> {
        unsafe {
            let footer = self.current_chunk_footer.get();
            let footer = footer.as_ref();
            let ptr = footer.ptr.get().as_ptr() as usize;
            let ptr = round_up_to(ptr, layout.align());
            let end = footer as *const _ as usize;
            debug_assert!(ptr <= end);

            // If the pointer overflows, the allocation definitely doesn't fit into the current
            // chunk, so we try to get a new one.
            let new_ptr = ptr.checked_add(layout.size())?;

            if new_ptr <= end {
                let p = ptr as *mut u8;
                debug_assert!(new_ptr <= footer as *const _ as usize);
                footer.ptr.set(NonNull::new_unchecked(new_ptr as *mut u8));
                Some(NonNull::new_unchecked(p))
            } else {
                None
            }
        }
    }

    // Slow path allocation for when we need to allocate a new chunk from the
    // parent bump set because there isn't enough room in our current chunk.
    #[inline(never)]
    fn alloc_layout_slow(&self, layout: Layout) -> NonNull<u8> {
        unsafe {
            let size = layout.size();

            // Get a new chunk from the global allocator.
            let current_layout = self.current_chunk_footer.get().as_ref().layout.clone();
            let footer = Bump::new_chunk(Some((current_layout.size(), layout)));

            // Set our current chunk's next link to this new chunk.
            self.current_chunk_footer
                .get()
                .as_ref()
                .next
                .set(Some(footer));

            // Set the new chunk as our new current chunk.
            self.current_chunk_footer.set(footer);

            // Move the bump ptr finger ahead to allocate room for `val`.
            let footer = footer.as_ref();
            let ptr = footer.ptr.get().as_ptr() as usize + size;
            debug_assert!(
                ptr <= footer as *const _ as usize,
                "{} <= {}",
                ptr,
                footer as *const _ as usize
            );
            footer.ptr.set(NonNull::new_unchecked(ptr as *mut u8));

            // Return a pointer to the start of this chunk.
            footer.data.cast::<u8>()
        }
    }

    /// Call `f` on each chunk of allocated memory that this arena has bump
    /// allocated into.
    ///
    /// `f` is invoked in order of allocation: oldest chunks first, newest
    /// chunks last.
    ///
    /// ## Safety
    ///
    /// Because this method takes `&mut self`, we know that the bump arena
    /// reference is unique and therefore there aren't any active references to
    /// any of the objects we've allocated in it either. This potential aliasing
    /// of exclusive references is one common footgun for unsafe code that we
    /// don't need to worry about here.
    ///
    /// However, there could be regions of uninitialized memory used as padding
    /// between allocations. Reading uninitialized memory is big time undefined
    /// behavior!
    ///
    /// The only way to guarantee that there is no padding between allocations
    /// or within allocated objects is if all of these properties hold:
    ///
    /// 1. Every object allocated in this arena has the same alignment.
    /// 2. Every object's size is a multiple of its alignment.
    /// 3. None of the objects allocated in this arena contain any internal
    ///    padding.
    ///
    /// If you want to use this `each_allocated_chunk` method, it is *your*
    /// responsibility to ensure that these properties hold!
    ///
    /// ## Example
    ///
    /// ```
    /// let mut bump = bumpalo::Bump::new();
    ///
    /// // Allocate a bunch of things in this bump arena, potentially causing
    /// // additional memory chunks to be reserved.
    /// for i in 0..10000 {
    ///     bump.alloc(i);
    /// }
    ///
    /// // Iterate over each chunk we've bump allocated into. This is safe
    /// // because we have only allocated `i32` objects in this arena.
    /// unsafe {
    ///     bump.each_allocated_chunk(|ch| {
    ///         println!("Used a chunk that is {} bytes long", ch.len());
    ///     });
    /// }
    /// ```
    pub unsafe fn each_allocated_chunk<F>(&mut self, mut f: F)
    where
        F: for<'a> FnMut(&'a [u8]),
    {
        let mut footer = Some(self.all_chunk_footers.get());
        while let Some(foot) = footer {
            let foot = foot.as_ref();

            let start = foot.data.as_ptr() as usize;
            let end_of_allocated_region = foot.ptr.get().as_ptr() as usize;
            debug_assert!(end_of_allocated_region <= foot as *const _ as usize);
            debug_assert!(
                end_of_allocated_region >= start,
                "end_of_allocated_region (0x{:x}) >= start (0x{:x})",
                end_of_allocated_region,
                start
            );

            let len = end_of_allocated_region - start;
            let slice = slice::from_raw_parts(start as *const u8, len);
            f(slice);

            footer = foot.next.get();
        }
    }
}

#[inline(never)]
#[cold]
fn oom() -> ! {
    panic!("out of memory")
}

unsafe impl<'a> alloc::Alloc for &'a Bump {
    #[inline(always)]
    unsafe fn alloc(&mut self, layout: Layout) -> Result<NonNull<u8>, alloc::AllocErr> {
        Ok(self.alloc_layout(layout))
    }

    #[inline(always)]
    unsafe fn dealloc(&mut self, _ptr: NonNull<u8>, _layout: Layout) {}

    #[inline]
    unsafe fn realloc(
        &mut self,
        ptr: NonNull<u8>,
        layout: Layout,
        new_size: usize,
    ) -> Result<NonNull<u8>, alloc::AllocErr> {
        let old_size = layout.size();

        // Shrinking allocations. Do nothing.
        if new_size < old_size {
            return Ok(ptr);
        }

        // See if the `ptr` is the last allocation we made. If so, we can likely
        // realloc in place by just bumping a bit further!
        //
        // Note that we align-up the bump pointer on new allocation requests
        // (not eagerly) so if `ptr` was the result of our last allocation, then
        // the bump pointer is still pointing just after it.
        let footer = self.current_chunk_footer.get();
        let footer = footer.as_ref();
        let footer_ptr = footer.ptr.get().as_ptr() as usize;
        if footer_ptr.checked_sub(old_size) == Some(ptr.as_ptr() as usize) {
            let delta = layout_from_size_align(new_size - old_size, 1);
            if let Some(_) = self.try_alloc_layout_fast(delta) {
                return Ok(ptr);
            }
        }

        // Otherwise, fall back on alloc + copy + dealloc.
        let new_layout = layout_from_size_align(new_size, layout.align());
        let result = self.alloc(new_layout);
        if let Ok(new_ptr) = result {
            ptr::copy_nonoverlapping(ptr.as_ptr(), new_ptr.as_ptr(), cmp::min(old_size, new_size));
            self.dealloc(ptr, layout);
        }
        result
    }
}

#[test]
fn chunk_footer_is_five_words() {
    assert_eq!(mem::size_of::<ChunkFooter>(), mem::size_of::<usize>() * 5);
}
