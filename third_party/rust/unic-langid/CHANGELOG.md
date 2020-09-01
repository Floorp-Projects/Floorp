# Changelog

## Unreleased

  - …

## unic-langid 0.7.1 (November 10, 2019)

  - Add `PartialOrd` and Ord` for `LanguageIdentifier`.

## unic-langid 0.7.0 (October 29, 2019)

  - Separate out `clear_*` methods.
  - Switch the API to operate on `AsRef<[u8]>`.
  - Switch set-based methods to use iterators.
  - Update TinyStr to 0.3.2.

## unic-langid 0.6.0 (October 3, 2019)

  - Add CLDR 35 backed `get_character_direction`.
  - Fix a bug in likely subtags.
  - Add basic documentation.

## unic-langid 0.5.2 (October 1, 2019)

  - Add CLDR 35 backed `add_likely_subtags` and `remove_likely_subtags`.

## unic-langid 0.5.0 (September 4, 2019)

  - Migrate unic-langid to TinyStr.
  - Enable `langid!` macro to work in const (unless it has variants).

## unic-langid 0.4.2 (August 2, 2019)

  - Update the macros to 0.3.0.

## unic-langid 0.4.1 (July 29, 2019)

  - Update the macros to 0.2.0 to make the macro work without explicit import of the impl.

## unic-langid 0.4.0 (July 26, 2019)

  - Switch to FromStr instead of TryFrom
  - Add langid! macro
  - Skip parsing and allocating when using macros

## unic-langid 0.3.0 (July 24, 2019)

  - Switch variants to handle Option for ergonomics
  - Extend fixtures coverage
  - Introduce From/Into/AsRef helpers

## unic-langid 0.2.0 (July 09, 2019)

  - Allow for hashing of LanguageIdentifiers
  - Switch to TryFrom

## unic-langid 0.1.0 (June 15, 2019)

  - Initial release
