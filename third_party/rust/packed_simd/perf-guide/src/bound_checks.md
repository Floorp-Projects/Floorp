# Bounds checking

Reading and writing packed vectors to/from slices is checked by default.
Independently of the configuration options used, the safe functions:

* `Simd<[T; N]>::from_slice_aligned(& s[..])`
* `Simd<[T; N]>::write_to_slice_aligned(&mut s[..])`

always check that:

* the slice is big enough to hold the vector
* the slice is suitably aligned to perform an aligned load/store for a `Simd<[T;
  N]>` (this alignment is often much larger than that of `T`).

There are `_unaligned` versions that use unaligned load and stores, as well as
`unsafe` `_unchecked` that do not perform any checks iff `debug-assertions =
false` / `debug = false`. That is, the `_unchecked` methods do still assert size
and alignment in debug builds and could also do so in release builds depending
on the configuration options.

These assertions do often significantly impact performance and you should be
aware of them.
