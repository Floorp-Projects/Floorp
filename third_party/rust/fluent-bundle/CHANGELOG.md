# Changelog

## Unreleased

  - …

## fluent-bundle 0.11.0 (March 10, 2020)
  - Separate out `concurrent` version of `FluentBundle`.
  - Switch FluentBundle functions to use function pointers.

## fluent-bundle 0.10.2 (February 20, 2020)
  - Update to `intl_memoizer` 0.3.0 to allow for Send+Sync on FluentBundle.

## fluent-bundle 0.10.1 (February 15, 2020)
  - Switch RefCell in FluentBundle to Mutex.

## fluent-bundle 0.10.0 (February 13, 2020)
  - Update `fluent-langneg` to 0.12.
  - Update `intl_pluralrules` to 6.0.
  - Update `unic-langid` to 0.8.
  - Introduce `intl-memoizer`.
  - Improve the ergonomics of FluentArgs.
  - Add `add_resource_overriding`.
  - Remove dependency on `failure`.
  - Switch the strategy to mitigate bomb attack to limit the number of placeables.
  - Introduce `FluentType` for custom types.
  - Improve ergonomics of `FluentNumber` and bring its features closer to ECMA402 Intl.NumberFormat.

## fluent-bundle 0.9.0 (November 26, 2019)
  - Update `unic-langid` to 0.7.
  - Update `fluent-langneg` to 0.11.
  - Update `intl_pluralrules` to 5.0.

## fluent-bundle 0.8.0 (October 3, 2019)

  - Update `unic-langid` to 0.6.
  - Update `fluent-locale` to 0.10.

## fluent-bundle 0.7.2 (October 1, 2019)

  - Update `unic-langid` to 0.5.
  - Update `fluent-locale` to 0.9.
  - Stop using macros to cut on compilation time and dependencies.

## fluent-bundle 0.7.1 (August 1, 2019)

  - Fix FluentBundle::default to use isolating by default.

## fluent-bundle 0.7.0 (August 1, 2019)

  - Turn FluentBundle to be a generic over Borrow<FluentResource> (#114)
  - Update FluentBundle to the latest API (0.14) (#120)
  - Switch to unic_langid for Language Identifier Management
  - Refactor FluentArgs (#130)
  - Add transform to FluentBundle to enable pseudolocalization (#131)
  - Refactor resolver errors to provide better fallbacking and return errors out of formatting (#93)
  - Enable FSI/PDI direction isolation (#116)
  - Add more convenience From impls for FluentValue (#108)
  - Fix `bare_trait_objects` warnings (#110)

## fluent-bundle 0.6.0 (March 26, 2019)

  - Update to fluent-syntax 0.9
  - Unify benchmark testsuite with fluent.js

## fluent-bundle 0.5.0 (January 31, 2019)

  - Update to fluent-syntax 0.8
  - Add unicode escaping
  - Align with zero-copy parser

## fluent 0.4.3 (October 13, 2018)

  - Support Sync+Send in Entry (#70)

## fluent 0.4.2 (October 1, 2018)

  - Separate lifetimes of `FluentBundle::new` and return values. (#68)

## fluent 0.4.1 (August 31, 2018)

  - Update README to make the example match  new API

## fluent 0.4.0 (August 31, 2018)

  - Rename MessageContext to FluentBundle
  - Update the FluentBundle API to match fluent.js 0.8
  - Update intl-pluralrules to 1.0
  - Add FluentBundle::format_message
  - Add FluentResource for external resource caching
  - Update fluent-syntax to 0.1.1
  - Update the signature of FluentBundle::format and FluentBundle::format_message

## fluent 0.3.1 (August 6, 2018)

  - Update `fluent-locale` to 0.4.1.
  - Switch MessageContext::locales to be an owned Vec\<String>
  - Switch FluentValue::From\<i8> to FluentValue::From\<isize>

## fluent 0.3.0 (August 3, 2018)

  - Add support for custom functions in MessageContext. (#50)
  - Switch error handling to `annotate-snippets crate`.
  - Separate `fluent` and `fluent-syntax` crates.
  - Handle cyclic references. (#55)
  - Switch parser binary to use `clap`.
  - Switch plural rules handling to `intl_pluralrules`. (#56)
  - Add `FluentValue::as_number`
  - Move `IntlPluralRules` initialization into `MessageContext::new`
  - General cleanups in line with `cargo fmt` and `cargo clippy`

## fluent 0.2.0 (February 11, 2018)

  - Support Rust 1.23 stable
  - Support Fluent 0.5 syntax
  - Dual-license Apache 2.0 and MIT

## fluent 0.1.2 (October 14, 2017)

  - Add more complex PluralRules support

## fluent 0.1.0 (October 13, 2017)

  - Support parsing Fluent Syntax 0.3.
  - Support formatting Messages and Attributes alike.
  - Support string- and Number-typed external arguments
  - Select expressions:
    - without a selector.
    - with literal strings and numbers as selector,
    - with external arguments as selector,
    - with message reference as selector (using tags).
  - Support matching numbers in select expression to plural categories.
    - Only a single mock plural rule has been implemented for now.
  - Support Attribute expressions.
  - Support Variant expressions.
  - `MessageContext::new` now takes a slice as the `locales` argument.
  - Added integration with Travis CI and Coveralls.
  - Expanded module documentation.


## fluent 0.0.1 (January 17, 2017)

  - This is the first release to be listed in the CHANGELOG.
  - Basic parser support for the FTL syntax.
  - Message references.
