# Unreleased

# 0.7.2

- Add `quick_main!` (#88).
- `allow(unused)` for the `Result` wrapper.
- Minimum rust version supported is not 1.10 on some conditions (#103).

# 0.7.1

- [Add the `bail!` macro](https://github.com/brson/error-chain/pull/76)

# 0.7.0

- [Rollback several design changes to fix regressions](https://github.com/brson/error-chain/pull/75)
- New `Variant(Error) #[attrs]` for `links` and `foreign_links`.
- Hide implementation details from the doc.
- Always generate `Error::backtrace`.

# 0.6.2

- Allow dead code.

# 0.6.1

- Fix wrong trait constraint in ResultExt implementation (#66).

# 0.6.0

- Conditional compilation for error variants.
- Backtrace generation is now a feature.
- More standard trait implementations for extra convenience.
- Remove ChainErr.
- Remove need to specify `ErrorKind` in `links {}`.
- Add ResultExt trait.
- Error.1 is a struct instead of a tuple.
- Error is now a struct.
- The declarations order is more flexible.
- Way better error reporting when there is a syntax error in the macro call.
- `Result` generation can be disabled.
- At most one declaration of each type can be present.

# 0.5.0

- [Only generate backtraces with RUST_BACKTRACE set](https://github.com/brson/error-chain/pull/27)
- [Fixup matching, disallow repeating "types" section](https://github.com/brson/error-chain/pull/26)
- [Fix tests on stable/beta](https://github.com/brson/error-chain/pull/28)
- [Only deploy docs when tagged](https://github.com/brson/error-chain/pull/30)

Contributors: benaryorg, Brian Anderson, Georg Brandl

# 0.4.2

- [Fix the resolution of the ErrorKind description method](https://github.com/brson/error-chain/pull/24)

Contributors: Brian Anderson

# 0.4.1 (yanked)

- [Fix a problem with resolving methods of the standard Error type](https://github.com/brson/error-chain/pull/22)

Contributors: Brian Anderson

# 0.4.0 (yanked)

- [Remove the foreign link description and forward to the foreign error](https://github.com/brson/error-chain/pull/19)
- [Allow missing sections](https://github.com/brson/error-chain/pull/17)

Contributors: Brian Anderson, Taylor Cramer

# 0.3.0

- [Forward Display implementation for foreign errors](https://github.com/brson/error-chain/pull/13)

Contributors: Brian Anderson, Taylor Cramer

# 0.2.2

- [Don't require `types` section in macro invocation](https://github.com/brson/error-chain/pull/8)
- [Add "quick start" to README](https://github.com/brson/error-chain/pull/9)

Contributors: Brian Anderson, Jake Shadle, Nate Mara
