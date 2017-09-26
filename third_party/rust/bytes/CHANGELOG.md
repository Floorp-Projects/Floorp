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
