# Changelog

## v0.8.0
- Update dependency on `syn` to 0.15 [#44](https://github.com/darling/pull/44). Thanks to @hcpl

## v0.7.0 (July 24, 2018)
- Update dependencies on `syn` and `proc-macro2`
- Add `util::IdentString`, which acts as an Ident or its string equivalent

## v0.6.3 (May 22, 2018)
- Add support for `Uses*` traits in where predicates

## v0.6.2 (May 22, 2018)
- Add `usage` module for tracking type param and lifetime usage in generic declarations
  - Add `UsesTypeParams` and `CollectsTypeParams` traits [#37](https://github.com/darling/issues/37)
  - Add `UsesLifetimes` and `CollectLifetimes` traits [#41](https://github.com/darling/pull/41)
- Don't add `FromMeta` bounds to type parameters only used by skipped fields [#40](https://github.com/darling/pull/40)

## v0.6.1 (May 17, 2018)
- Fix an issue where the `syn` update broke shape validation [#36](https://github.com/TedDriggs/darling/issues/36)

## v0.6.0 (May 15, 2018)

### Breaking Changes
- Renamed `FromMetaItem` to `FromMeta`, and renamed `from_meta_item` method to `from_meta`
- Added dedicated `derive(FromMetaItem)` which panics and redirects users to `FromMeta`

## v0.5.0 (May 10, 2018)
- Add `ast::Generics` and `ast::GenericParam` to work with generics in a manner similar to `ast::Data`
- Add `ast::GenericParamExt` to support alternate representations of generic parameters
- Add `util::WithOriginal` to get a parsed representation and syn's own struct for a syntax block
- Add `FromGenerics` and `FromGenericParam` traits (without derive support)
- Change generated code for `generics` magic field to invoke `FromGenerics` trait during parsing
- Add `FromTypeParam` trait [#30](https://github.com/TedDriggs/darling/pull/30). Thanks to @upsuper

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
