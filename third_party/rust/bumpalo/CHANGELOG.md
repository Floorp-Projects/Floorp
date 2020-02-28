# 2.6.0

Released 2019-08-19.

* Implement `Send` for `Bump`.

# 2.5.0

Released 2019-07-01.

* Add `alloc_slice_copy` and `alloc_slice_clone` methods that allocate space for
  slices and either copy (with bound `T: Copy`) or clone (with bound `T: Clone`)
  the provided slice's data into the newly allocated space.

# 2.4.3

Released 2019-05-20.

* Fixed a bug where chunks were always deallocated with the default chunk
  layout, not the layout that the chunk was actually allocated with (i.e. if we
  started growing largers chunks with larger layouts, we would deallocate those
  chunks with an incorrect layout).

# 2.4.2

Released 2019-05-17.

* Added an implementation `Default` for `Bump`.
* Made it so that if bump allocation within a chunk overflows, we still try to
  allocate a new chunk to bump out of for the requested allocation. This can
  avoid some OOMs in scenarios where the chunk we are currently allocating out
  of is very near the high end of the address space, and there is still
  available address space lower down for new chunks.

# 2.4.1

Released 2019-04-19.

* Added readme metadata to Cargo.toml so it shows up on crates.io

# 2.4.0

Released 2019-04-19.

* Added support for `realloc`ing in-place when the pointer being `realloc`ed is
  the last allocation made from the bump arena. This should speed up various
  `String`, `Vec`, and `format!` operations in many cases.

# 2.3.0

Released 2019-03-26.

* Add the `alloc_with` method, that (usually) avoids stack-allocating the
  allocated value and then moving it into the bump arena. This avoids potential
  stack overflows in release mode when allocating very large objects, and also
  some `memcpy` calls. This is similar to the `copyless` crate. Read [the
  `alloc_with` doc comments][alloc-with-doc-comments] and [the original issue
  proposing this API][issue-proposing-alloc-with] for more.

[alloc-with-doc-comments]: https://github.com/fitzgen/bumpalo/blob/9f47aee8a6839ba65c073b9ad5372aacbbd02352/src/lib.rs#L436-L475
[issue-proposing-alloc-with]: https://github.com/fitzgen/bumpalo/issues/10

# 2.2.2

Released 2019-03-18.

* Fix a regression from 2.2.1 where chunks were not always aligned to the chunk
  footer's alignment.

# 2.2.1

Released 2019-03-18.

* Fix a regression in 2.2.0 where newly allocated bump chunks could fail to have
  capacity for a large requested bump allocation in some corner cases.

# 2.2.0

Released 2019-03-15.

* Chunks in an arena now start out small, and double in size as more chunks are
  requested.

# 2.1.0

Released 2019-02-12.

* Added the `into_bump_slice` method on `bumpalo::collections::Vec<T>`.

# 2.0.0

Released 2019-02-11.

* Removed the `BumpAllocSafe` trait.
* Correctly detect overflows from large allocations and panic.

# 1.2.0

Released 2019-01-15.

* Fixed an overly-aggressive `debug_assert!` that had false positives.
* Ported to Rust 2018 edition.

# 1.1.0

Released 2018-11-28.

* Added the `collections` module, which contains ports of `std`'s collection
  types that are compatible with backing their storage in `Bump` arenas.
* Lifted the limits on size and alignment of allocations.

# 1.0.2

# 1.0.1

# 1.0.0
