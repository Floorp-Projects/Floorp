# Release 0.1.39

- [The new `Roots` trait provides `sqrt`, `cbrt`, and `nth_root` methods][9],
  calculating an `Integer`'s principal roots rounded toward zero.

**Contributors**: @cuviper

[9]: https://github.com/rust-num/num-integer/pull/9

# Release 0.1.38

- [Support for 128-bit integers is now automatically detected and enabled.][8]
  Setting the `i128` crate feature now causes the build script to panic if such
  support is not detected.

**Contributors**: @cuviper

[8]: https://github.com/rust-num/num-integer/pull/8

# Release 0.1.37

- [`Integer` is now implemented for `i128` and `u128`][7] starting with Rust
  1.26, enabled by the new `i128` crate feature.

**Contributors**: @cuviper

[7]: https://github.com/rust-num/num-integer/pull/7

# Release 0.1.36

- [num-integer now has its own source repository][num-356] at [rust-num/num-integer][home].
- [Corrected the argument order documented in `Integer::is_multiple_of`][1]
- [There is now a `std` feature][5], enabled by default, along with the implication
  that building *without* this feature makes this a `#[no_std]` crate.
  - There is no difference in the API at this time.

**Contributors**: @cuviper, @jaystrictor

[home]: https://github.com/rust-num/num-integer
[num-356]: https://github.com/rust-num/num/pull/356
[1]: https://github.com/rust-num/num-integer/pull/1
[5]: https://github.com/rust-num/num-integer/pull/5


# Prior releases

No prior release notes were kept.  Thanks all the same to the many
contributors that have made this crate what it is!

