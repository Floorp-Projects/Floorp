# Changelog

## Unreleased

  - …

## intl_pluralrules 6.0.0 (January 28, 2020)

  - Switch to use `TryFrom`/`TryInto` for `PluralOperands`.
  - Update `unic-langid` to 0.8.
  - Optimize `PluralOperands` struct memory layout.

## intl_pluralrules 5.0.2 (December 19, 2019)

  - Make `PluralRuleType` derive `Hash` and `Eq`.

## intl_pluralrules 5.0.1 (November 20, 2019)

  - The `rules.rs` grew too big, so use macro to shrink it.

## intl_pluralrules 5.0.0 (November 13, 2019)

  - Renamed `IntlPluralRules` to `PluralRules` in line with
    other internationalization naming conventions.
  - Use updated generated tables which operate on sorted
    vectors of `(LanguageIdentifier, PluralRule)` tuples.
  - Improved performance of construction of `PluralRules` by 66%.
  - Removed dependency on `phf`.

## intl_pluralrules 4.0.1 (October 15, 2019)

  - Update CLDR to 36.

## intl_pluralrules 4.0.0 (October 3, 2019)

  - Upgrade to unic-langid 0.6.

## intl_pluralrules 3.0.0 (September 10, 2019)

  - Upgrade to unic-langid 0.5.

## intl_pluralrules 2.0.0 (July 26, 2019)

  - Switch to unic_langid for Language Identifiers.

## intl_pluralrules 1.0.3 (March 29, 2019)

  - Update CLDR to 35

## intl_pluralrules 1.0.2 (January 7, 2019)

  - Update to Rust 2018.

## intl_pluralrules 1.0.1 (October 26, 2018)

  - Update CLDR to 34
  - Update rules to pass enums by reference

## intl_pluralrules 1.0.0 (August 15, 2018)

  - Add dedicated impls for PluralOperands::from
  - New trait extends impls to custom types
  - Other minor optimizations

## intl_pluralrules 0.9.1 (August 6, 2018)

  - Updates to docs for PluralRuleType.
  - Move PluralRuleType from rules.rs to lib.rs.
  - Fix lib.rs doc.

## intl_pluralrules 0.9.0 (August 3, 2018)

  - Updates to docs.

## intl_pluralrules 0.8.2 (July 31, 2018)

  - Optimization for operands.rs.

## intl_pluralrules 0.8.1 (July 30, 2018)

  - Locale field.
  - Locale field getter function.
  - Update rules.rs.

## intl_pluralrules 0.8.0 (July 30, 2018)

  - Initial release.
