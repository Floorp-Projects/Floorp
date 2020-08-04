# Version History

## 1.7.0

* Enables `bincode` support via the feature `serde-bincode`. This provides a long term fix for a regression 
  that was introduced in version `0.6.5` (tests now cover this case!). [Issue 43](https://github.com/paupino/rust-decimal/issues/43).
* Fixes issue where `rescale` on zero would not have an affect. This was due to an early exit condition which failed to 
  set the new scale. [Issue 253](https://github.com/paupino/rust-decimal/issues/253).
* Add `min` and `max` functions, similar to what `f32` and `f64` provide. Thank you [@michalsieron](https://github.com/michalsieron). 
* Updates documentation for `is_sign_positive` and `is_sign_negative` to specify that the sign bit is being checked.

Please note: feature naming conventions have been modified, however backwards compatible aliases have been created where 
necessary. It's highly recommended that you move over to the new naming conventions as these aliases may be removed at a 
later date. 

## 1.6.0

* Fixes issue with PostgreSQL conversions whereby certain inputs would cause unexpected
  outputs. [Issue 241](https://github.com/paupino/rust-decimal/issues/241).
* Fixes issue with `from_str_radix` whereby rounding logic would kick in too early, 
  especially with radix less than 10. [Issue 242](https://github.com/paupino/rust-decimal/issues/242).
* Fixes issue whereby `from_str` (implicity `from_str_radix`) would panic when there was overflow
  and overflow significant digit was < 5. [Issue 246](https://github.com/paupino/rust-decimal/issues/246).
* Make `bytes` and `byteorder` optional since they're only used in the `postgres` feature and tests.
* Fix edge case in `from_i128_with_scale` when `i128::MIN` was provided.

Thank you to [@serejkaaa512](https://github.com/serejkaaa512), [@AbsurdlySuspicious](https://github.com/AbsurdlySuspicious) and [@0e4ef622]((https://github.com/0e4ef622)) for your contributions!

## 1.5.0

* Added additional `RoundStrategy` abilities: `RoundUp` to always round up and `RoundDown` to always round down.
* Updated prelude to include expected structs and traits by default. 

Special thank you to [@jean-airoldie](https://github.com/jean-airoldie) for adding the additional rounding strategies and to [@pfrenssen](https://github.com/pfrenssen) for fixing an
issue in the README.

## 1.4.1

* Performance improvements for `to_f64` when using a scale > 0.

Special thank you to [@hengchu](https://github.com/hengchu) who discovered and resolved the issue!

## 1.4.0

* Allow uppercase "E" in scientific notation.
* Allow scientific notation in `dec!` macro.
* Deprecate `set_sign` and replace with `set_sign_positive` and `set_sign_negative`. This is intended
 to improve the readability of the API.
* Fixes precision issue when parsing `f64` values. The base 2 mantissa of the float was assuming guaranteed accuracy
 of 53 bit precision, however 52 bit precision is more accurate (`f64` only).
* Removes deprecated usage of `Error::description`.

## 1.3.0

* Replace `num` dependency with `num_trait` - implemented `Signed` and `Num` traits.

## 1.2.1

* Fixes issue whereby overflow would occur reading from PostgreSQL with high precision. The library now 
  handles this by rounding high precision numbers as they're read as opposed to crashing (similar to other
  underflow situations e.g. 1/3).

## 1.2.0

* Retain trailing zeros from PostgreSQL. This ensures that the scale is maintained when serialized into the Decimal type.
* Fixes issue where -0 != 0 (these are now equivalent - thank you @hengchu for discovering).
* Improve hashing function so that the following property is true: `k1 == k2 -> hash(k1) == hash(k2)`
* Update normalize function so that -0 normalizes to 0.

Special thanks to @hathawsh for their help in this release!

## 1.1.0

* Update to Postgres 0.17 and add postgres async/await support via `tokio-pg` 
* Added option for serializing decimals as float via `serde-float` 

Special thanks to @pimeys and @kaibyao!

## 1.0.3

Updates dependencies to prevent build issues.

## 1.0.2

Bug fix release:

* Fixes issue where scaling logic produced incorrect results when one arm was a high precision zero. Thank you @KonishchevDmitry!

## 1.0.1

Bug fix release:

* Fixes issue where `ToSql` was incorrectly calculating weight when whole portion = numeric portion.
* Fixes issue where `Decimal::new` incorrectly handled `i64::max_value()` and `i64::min_value()`.
* Fixes issue where `rem` operation incorrectly returned results when `scale` was required. 

## 1.0.0

This release represents the start of semantic versioning and allows the library to start making fundamental improvements under
the guise of V2.0. Leading up to that I expect to release 1.x versions which will include adding
various mathematical functions such as `pow`, `ln`, `log10` etc.

Version `1.0.0` does come with some new features:

* Checked Operations! This implements `checked_add`, `checked_sub`, `checked_mul`, `checked_div` and `checked_rem`.
* Fixes overflow from `max_value()` and `min_value()` for `i32` and `i64`.
* Minor documentation improvements and test coverage. 

Special thanks to @0e4ef622 for their help with this release! 

## 0.11.3

* Add prelude to help num trait inclusion (`use rust_decimal::prelude::*`)
* Add `Default` trait to the library. This is equivalent to using `Decimal::zero()`
* Added assignment operators for references.

Special thanks to @jean-airoldie for his help with this release!

## 0.11.2

* Fall back to `from_scientific` when `from_str` fails during deserialization. Thanks @mattjbray!
* Added basic `Sum` trait implementation

## 0.11.1

* Fixes a bug in `floor` and `ceil` where negative numbers were incorrectly handled.

## 0.11.0

* Macros are now supported on stable. This does use a [hack](https://github.com/dtolnay/proc-macro-hack) for the meantime 
so due diligence is required before usage. 
* Fixes issue when parsing strings where an underscore preceded a decimal point. 
* `const_fn` support via a feature flag. In the future this will be the default option however in order to support older
compiler versions is behind a feature flag.   

## 0.10.2

* Macros (nightly) now output structural data as opposed to serialized data. This is fully backwards compatible and results in some minor performance improvements. Also, removed feature gate so that it can be compiled in stable.
* Fixes a string parsing bug when given highly significant numbers that require rounding.

## 0.10.1

* Bumped dependencies to remove some legacy serialization requirements.

## 0.10.0

Special thanks to @xilec, @snd and @AndrewSpeed for their help with this release.

* New rounding strategies introduced via `round_dp_with_strategy`. Previously default rounding support used bankers rounding by default whereas now you can choose to round the half way point either up or down.
* PostgreSQL write performance improved so that it is at least 3 times faster than the previous implementation.
* `Debug` trait now outputs the actual decimal number by default to make it more useful within consuming libraries (e.g. `criterion.rs`). To get something similar to the previous functionality you can use the `unpack` argument - this is likely for core `rust-decimal` library maintainers.
* Various other performance improvements for common operations such as `rescale`, `sub` and `div`.

## 0.9.1

* Performance optimization for `add`.

## 0.9.0

* Introduces the `Neg` trait to support the ability to use `-decimal_variable`.
* Fixes bug with underflow on addition.

## 0.8.1

This release updates the published documentation only and is a no-op for functionality.

## 0.8.0

* Introduces `from_scientific` allowing parsing of scientific notation into the Decimal type.
* Fixes a bug when formatting a number with a leading zero's.

## 0.7.2

* Fixes bug in `rescale` whereby scaling which invoked rounding incorrectly set the new scale for the left/right sides.

## 0.7.1

* Fixes bug in `cmp` whereby two negatives would return an incorrect result.
* Further documentation examples
* Small improvements in division logic
* New `abs`, `floor` and `ceil` functions.

## 0.7.0

This is a minor version bump as we slowly build our way towards 1.0. Thank you for everyone's support and help as we get there! This has a few notable changes - also introducing a few new interfaces which is the reason for the version bump:

* `from_parts` function to allow effective creation of `Decimal`'s without requiring binary serialization. An example of this benefit is with the lazy static group initializers for Postgres.
* `normalize` function to allow stripping trailing zero's easily.
* `trunc` function allows truncation of a number without any rounding. This effectively "truncates" the fractional part of the number.
* `fract` function returns the fractional part of the number without the integral.
* Minor improvements in some iterator logic, utilizing the compiler for further optimizations.
* Fixes issue in string parsing logic whereby `_` would cause numbers to be incorrectly identified.
* Many improvements to `mul`. Numbers utilizing the `lo` portion of the decimal only will now be shortcut and bigger numbers will now correctly overflow. True overflows will still panic, however large underflows will now be rounded as necessary as opposed to panicing.
* `Hash` was implemented by convention in `0.6.5` however is reimplemented explicitly in `0.7.0` for effectiveness.
* PostgreSQL read performance improved by pre-caching groups and leveraging `normalize` (i.e. avoiding strings). Further optimizations can be made in write however require some `div` optimizations first.
* Added short circuit write improvement for zero in PostgreSQL writes.
* Benchmarks are now recorded per build so we can start tracking where slow downs have occurred. This does mean there is a performance hit on Travis builds however hopefully the pay off will make it worthwhile.

## 0.6.5

Fixes issue with rescale sometimes causing a silent overflow which led to incorrect results during addition, subtraction and compare. Consequently Decimal now rounds the most significant number so that these operations work successfully.

In addition, Decimal now derive's the `Hash` trait so that it can be used for indexing.

## 0.6.4

Fixes silent overflow errors when parsing highly significant strings. `from_str` will now round in these scenario's, similar to oleaut32 behavior.

## 0.6.3

Fixes a regression in ordering where by different scales would be rescaled towards losing precision instead of increasing precision. Have added numerous test suites to help cover more issues like this in the future.
Also fixes an issue in parsing invalid strings whereby the precision exceeded our maximum precision. Previously, this would work with unintended results however this now returns an Error returned from `FromStr`.

## 0.6.2

Fixes an issue with division of rational numbers allowing results greater than `MAX_PRECISION`. This would ultimately cause issues for future operations on this number.
In addition, in some cases transitive operations would not be equal due to overflow being lost.

## 0.6.1

This minor release is purely to expose `rust_decimal_macros` for use on the nightly channel. Documentation has been updated accordingly.

## 0.6.0

This release has a few major changes to the internal workings of the `Decimal` implementation and consequently comes with a number of performance improvements.

* Floats can now be parsed into a `Decimal` type using `from_f32` and `from_f64`.
* `add`, `sub`, `mul` run roughly 1500% faster than before.
* `div` run's roughly 1000% faster than before with room for future improvement.
* Also get significant speed improvements with `cmp`, `rescale`, `round_dp` and some string manipulations.
* Implemented `*Assign` traits for simpler usage.
* Removed `BigInt` and `BigUint` as being intermediary data types.

## 0.5.2

Minor bug fix to prevent a `panic` from overflow during comparison of high significant digit decimals. 

## 0.5.1

Minor bux fix to prevent `panic` upon parsing an empty string.

## 0.5.0

* Removes postgres from default feature set.
* `bincode` support for serde
* Better support for format strings
* Benchmarks added to tests

## 0.4.2

Fixes bug in `cmp` whereby negative's were not being compared correctly.

## 0.4.1

Minor bug fix to support creating negative numbers using the default constructor.

## 0.4.0

This release is a stylistic cleanup however does include some minor changes that may break existing builds.

### Changed
* Serde is now optional. You can enable Serde support within `features` using the keyword `serde`.
* Serde now returns errors on invalid input as opposed to `0`.
* `f64` conversion support has been added.
* Update Postgres dependency to use v0.15.

## 0.3.1

This is a documentation release that should help with discoverability and usage.

## 0.3.0

### Changed
* Removed trait `ToDecimal` and replaced with builtin [`From`](https://doc.rust-lang.org/std/convert/trait.From.html) trait ([`#12`](https://github.com/paupino/rust-decimal/pull/12))
