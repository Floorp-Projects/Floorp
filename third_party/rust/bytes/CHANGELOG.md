# 0.5.6 (July 13, 2020)

- Improve `BytesMut` to reuse buffer when fully `advance`d.
- Mark `BytesMut::{as_mut, set_len}` with `#[inline]`.
- Relax synchronization when cloning in shared vtable of `Bytes`.
- Move `loom` to `dev-dependencies`.

# 0.5.5 (June 18, 2020)

### Added
- Allow using the `serde` feature in `no_std` environments (#385).

### Fix
- Fix `BufMut::advance_mut` to panic if advanced passed the capacity (#354)..
- Fix `BytesMut::freeze` ignoring amount previously `advance`d (#352).

# 0.5.4 (January 23, 2020)

### Added
- Make `Bytes::new` a `const fn`.
- Add `From<BytesMut>` for `Bytes`.

### Fix
- Fix reversed arguments in `PartialOrd` for `Bytes`.
- Fix `Bytes::truncate` losing original capacity when repr is an unshared `Vec`.
- Fix `Bytes::from(Vec)` when allocator gave `Vec` a pointer with LSB set.
- Fix panic in `Bytes::slice_ref` if argument is an empty slice.

# 0.5.3 (December 12, 2019)

### Added
- `must_use` attributes to `split`, `split_off`, and `split_to` methods (#337).

### Fix
- Potential freeing of a null pointer in `Bytes` when constructed with an empty `Vec<u8>` (#341, #342).
- Calling `Bytes::truncate` with a size large than the length will no longer clear the `Bytes` (#333).

# 0.5.2 (November 27, 2019)

### Added
- `Limit` methods `into_inner`, `get_ref`, `get_mut`, `limit`, and `set_limit` (#325).

# 0.5.1 (November 25, 2019)

### Fix
- Growth documentation for `BytesMut` (#321)

# 0.5.0 (November 25, 2019)

### Fix
- Potential overflow in `copy_to_slice`

### Changed
- Increased minimum supported Rust version to 1.39.
- `Bytes` is now a "trait object", allowing for custom allocation strategies (#298)
- `BytesMut` implicitly grows internal storage. `remaining_mut()` returns
  `usize::MAX` (#316).
- `BufMut::bytes_mut` returns `&mut [MaybeUninit<u8>]` to reflect the unknown
  initialization state (#305).
- `Buf` / `BufMut` implementations for `&[u8]` and `&mut [u8]`
  respectively (#261).
- Move `Buf` / `BufMut` "extra" functions to an extension trait (#306).
- `BufMutExt::limit` (#309).
- `Bytes::slice` takes a `RangeBounds` argument (#265).
- `Bytes::from_static` is now a `const fn` (#311).
- A multitude of smaller performance optimizations.

### Added
- `no_std` support (#281).
- `get_*`, `put_*`, `get_*_le`, and `put_*le` accessors for handling byte order.
- `BorrowMut` implementation for `BytesMut` (#185).

### Removed
- `IntoBuf` (#288).
- `Buf` implementation for `&str` (#301).
- `byteorder` dependency (#280).
- `iovec` dependency, use `std::IoSlice` instead (#263).
- optional `either` dependency (#315).
- optional `i128` feature -- now available on stable. (#276).

# 0.4.12 (March 6, 2019)

### Added
- Implement `FromIterator<&'a u8>` for `BytesMut`/`Bytes` (#244).
- Implement `Buf` for `VecDeque` (#249).

# 0.4.11 (November 17, 2018)

* Use raw pointers for potentially racy loads (#233).
* Implement `BufRead` for `buf::Reader` (#232).
* Documentation tweaks (#234).

# 0.4.10 (September 4, 2018)

* impl `Buf` and `BufMut` for `Either` (#225).
* Add `Bytes::slice_ref` (#208).

# 0.4.9 (July 12, 2018)

* Add 128 bit number support behind a feature flag (#209).
* Implement `IntoBuf` for `&mut [u8]`

# 0.4.8 (May 25, 2018)

* Fix panic in `BytesMut` `FromIterator` implementation.
* Bytes: Recycle space when reserving space in vec mode (#197).
* Bytes: Add resize fn (#203).

# 0.4.7 (April 27, 2018)

* Make `Buf` and `BufMut` usable as trait objects (#186).
* impl BorrowMut for BytesMut (#185).
* Improve accessor performance (#195).

# 0.4.6 (Janary 8, 2018)

* Implement FromIterator for Bytes/BytesMut (#148).
* Add `advance` fn to Bytes/BytesMut (#166).
* Add `unsplit` fn to `BytesMut` (#162, #173).
* Improvements to Bytes split fns (#92).

# 0.4.5 (August 12, 2017)

* Fix range bug in `Take::bytes`
* Misc performance improvements
* Add extra `PartialEq` implementations.
* Add `Bytes::with_capacity`
* Implement `AsMut[u8]` for `BytesMut`

# 0.4.4 (May 26, 2017)

* Add serde support behind feature flag
* Add `extend_from_slice` on `Bytes` and `BytesMut`
* Add `truncate` and `clear` on `Bytes`
* Misc additional std trait implementations
* Misc performance improvements

# 0.4.3 (April 30, 2017)

* Fix Vec::advance_mut bug
* Bump minimum Rust version to 1.15
* Misc performance tweaks

# 0.4.2 (April 5, 2017)

* Misc performance tweaks
* Improved `Debug` implementation for `Bytes`
* Avoid some incorrect assert panics

# 0.4.1 (March 15, 2017)

* Expose `buf` module and have most types available from there vs. root.
* Implement `IntoBuf` for `T: Buf`.
* Add `FromBuf` and `Buf::collect`.
* Add iterator adapter for `Buf`.
* Add scatter/gather support to `Buf` and `BufMut`.
* Add `Buf::chain`.
* Reduce allocations on repeated calls to `BytesMut::reserve`.
* Implement `Debug` for more types.
* Remove `Source` in favor of `IntoBuf`.
* Implement `Extend` for `BytesMut`.


# 0.4.0 (February 24, 2017)

* Initial release
