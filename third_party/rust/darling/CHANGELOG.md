# Changelog

## Unreleased Features
_None_

## v0.4.0 (April 5, 2018)
- Update dependencies on `proc-macro`, `quote`, and `syn` [#26](https://github.com/TedDriggs/darling/pull/26). Thanks to @hcpl

## v0.3.3 (April 2, 2018)
**YANKED**

## v0.3.2 (March 13, 2018)
- Derive `Default` on `darling::Ignored` (fixes [#25](https://github.com/TedDriggs/darling/issues/25)).

## v0.3.1 (March 7, 2018)
- Support proc-macro2/nightly [#24](https://github.com/TedDriggs/darling/pull/24). Thanks to @kdy1

## v0.3.0 (January 26, 2018)

### Breaking Changes
- Update `syn` to 0.12 [#20](https://github.com/TedDriggs/darling/pull/20). Thanks to @Eijebong
- Update `quote` to 0.4 [#20](https://github.com/TedDriggs/darling/pull/20). Thanks to @Eijebong
- Rename magic field `body` in derived `FromDeriveInput` structs to `data` to stay in sync with `syn`
- Rename magic field `data` in derived `FromVariant` structs to `fields` to stay in sync with `syn`

## v0.2.2 (December 5, 2017)

- Update `lazy_static` to 1.0 [#15](https://github.com/TedDriggs/darling/pull/16). Thanks to @Eijebong

## v0.2.1 (November 28, 2017)

- Add `impl FromMetaItem` for integer types [#15](https://github.com/TedDriggs/darling/pull/15)

## v0.2.0 (June 18, 2017)

- Added support for returning multiple errors from parsing [#5](https://github.com/TedDriggs/darling/pull/5)
- Derived impls no longer return on first error [#5](https://github.com/TedDriggs/darling/pull/5)
- Removed default types for `V` and `F` from `ast::Body`
- Enum variants are automatically converted to snake_case [#12](https://github.com/TedDriggs/darling/pull/12)