# Release 0.2.1 (2018-06-22)

- Maintenance release to fix `html_root_url`.

# Release 0.2.0 (2018-06-19)

### Enhancements

- [`Ratio` now implements `One::is_one` and the `Inv` trait][19].
- [`Ratio` now implements `Sum` and `Product`][25].
- [`Ratio` now supports `i128` and `u128` components][29] with Rust 1.26+.
- [`Ratio` now implements the `Pow` trait][21].

### Breaking Changes

- [`num-rational` now requires rustc 1.15 or greater][18].
- [There is now a `std` feature][23], enabled by default, along with the
  implication that building *without* this feature makes this a `#![no_std]`
  crate.  A few methods now require `FloatCore` instead of `Float`.
- [The `serde` dependency has been updated to 1.0][24], and `rustc-serialize`
  is no longer supported by `num-complex`.
- The optional `num-bigint` dependency has been updated to 0.2, and should be
  enabled using the `bigint-std` feature.  In the future, it may be possible
  to use the `bigint` feature with `no_std`.

**Contributors**: @clarcharr, @cuviper, @Emerentius, @robomancer-or, @vks

[18]: https://github.com/rust-num/num-rational/pull/18
[19]: https://github.com/rust-num/num-rational/pull/19
[21]: https://github.com/rust-num/num-rational/pull/21
[23]: https://github.com/rust-num/num-rational/pull/23
[24]: https://github.com/rust-num/num-rational/pull/24
[25]: https://github.com/rust-num/num-rational/pull/25
[29]: https://github.com/rust-num/num-rational/pull/29


# Release 0.1.42 (2018-02-08)

- Maintenance release to update dependencies.


# Release 0.1.41 (2018-01-26)

- [num-rational now has its own source repository][num-356] at [rust-num/num-rational][home].
- [`Ratio` now implements `CheckedAdd`, `CheckedSub`, `CheckedMul`, and `CheckedDiv`][11].
- [`Ratio` now implements `AddAssign`, `SubAssign`, `MulAssign`, `DivAssign`, and `RemAssign`][12]
  with either `Ratio` or an integer on the right side.  The non-assignment operators now also
  accept integers as an operand.
- [`Ratio` operators now make fewer `clone()` calls][14].

Thanks to @c410-f3r, @cuviper, and @psimonyi for their contributions!

[home]: https://github.com/rust-num/num-rational
[num-356]: https://github.com/rust-num/num/pull/356
[11]: https://github.com/rust-num/num-rational/pull/11
[12]: https://github.com/rust-num/num-rational/pull/12
[14]: https://github.com/rust-num/num-rational/pull/14


# Prior releases

No prior release notes were kept.  Thanks all the same to the many
contributors that have made this crate what it is!
