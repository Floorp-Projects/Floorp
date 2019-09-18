# Release 0.1.44

- [Division with single-digit divisors is now much faster.][42]
- The README now compares [`ramp`, `rug`, `rust-gmp`][20], and [`apint`][21].

**Contributors**: @cuviper, @Robbepop

[20]: https://github.com/rust-num/num-bigint/pull/20
[21]: https://github.com/rust-num/num-bigint/pull/21
[42]: https://github.com/rust-num/num-bigint/pull/42

# Release 0.1.43

- [The new `BigInt::modpow`][18] performs signed modular exponentiation, using
  the existing `BigUint::modpow` and rounding negatives similar to `mod_floor`.

**Contributors**: @cuviper

[18]: https://github.com/rust-num/num-bigint/pull/18

# Release 0.1.42

- [num-bigint now has its own source repository][num-356] at [rust-num/num-bigint][home].
- [`lcm` now avoids creating a large intermediate product][num-350].
- [`gcd` now uses Stein's algorithm][15] with faster shifts instead of division.
- [`rand` support is now extended to 0.4][11] (while still allowing 0.3).

**Contributors**: @cuviper, @Emerentius, @ignatenkobrain, @mhogrefe

[home]: https://github.com/rust-num/num-bigint
[num-350]: https://github.com/rust-num/num/pull/350
[num-356]: https://github.com/rust-num/num/pull/356
[11]: https://github.com/rust-num/num-bigint/pull/11
[15]: https://github.com/rust-num/num-bigint/pull/15


# Prior releases

No prior release notes were kept.  Thanks all the same to the many
contributors that have made this crate what it is!

