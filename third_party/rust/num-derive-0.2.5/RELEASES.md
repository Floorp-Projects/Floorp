# Release 0.2.5 (2019-04-23)

- [Improved the masking of lints in derived code][23].

[23]: https://github.com/rust-num/num-derive/pull/23

# Release 0.2.4 (2019-01-25)

- [Adjusted dependencies to allow no-std targets][22].

[22]: https://github.com/rust-num/num-derive/pull/22

# Release 0.2.3 (2018-10-03)

- [Added newtype deriving][17] for `FromPrimitive`, `ToPrimitive`,
  `NumOps<Self, Self>`, `NumCast`, `Zero`, `One`, `Num`, and `Float`.
  Thanks @asayers!

[17]: https://github.com/rust-num/num-derive/pull/17

# Release 0.2.2 (2018-05-22)

- [Updated dependencies][14].

[14]: https://github.com/rust-num/num-derive/pull/14

# Release 0.2.1 (2018-05-09)

- [Updated dependencies][12] -- thanks @spearman!

[12]: https://github.com/rust-num/num-derive/pull/12

# Release 0.2.0 (2018-02-21)

- [Discriminant matching is now simplified][10], casting values directly by
  name, rather than trying to compute offsets from known values manually.
- **breaking change**: [Derivations now import the traits from `num-traits`][11]
  instead of the full `num` crate.  These are still compatible, but users need
  to have an explicit `num-traits = "0.2"` dependency in their `Cargo.toml`.

[10]: https://github.com/rust-num/num-derive/pull/10
[11]: https://github.com/rust-num/num-derive/pull/11


# Release 0.1.44 (2018-01-26)

- [The derived code now explicitly allows `unused_qualifications`][9], so users
  that globally deny that lint don't encounter an error.

[9]: https://github.com/rust-num/num-derive/pull/9


# Release 0.1.43 (2018-01-23)

- [The derived code now explicitly allows `trivial_numeric_casts`][7], so users
  that globally deny that lint don't encounter an error.

[7]: https://github.com/rust-num/num-derive/pull/7


# Release 0.1.42 (2018-01-22)

- [num-derive now has its own source repository][num-356] at [rust-num/num-derive][home].
- [The derivation macros have been updated][3] to using `syn` 0.12.  Support for complex
  expressions in enum values can be enabled with the `full-syntax` feature.

Thanks to @cuviper and @hcpl for their contributions!

[home]: https://github.com/rust-num/num-derive
[num-356]: https://github.com/rust-num/num/pull/356
[3]: https://github.com/rust-num/num-derive/pull/3


# Prior releases

No prior release notes were kept.  Thanks all the same to the many
contributors that have made this crate what it is!

