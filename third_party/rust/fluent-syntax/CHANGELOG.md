# Changelog

## Unreleased

  - …

## fluent-syntax 0.9.3 (March 4, 2020)
  - Move JSON serialization from tests to source code behind the feature flag.
  - Fix a minor syntax issue that caused the parser to recognize a comment ending on EOF as Junk.
  - Add context benchmarks for Firefox contexts - browser and preferences.

## fluent-syntax 0.9.2 (February 13, 2020)
  - Import updated tests from the reference parser.
  - Minor parser improvements to align with new tests.

## fluent-syntax 0.9.1 (November 26, 2019)
  - Dependency updates.
  - Better test coverage.

## fluent-syntax 0.9.0 (March 26, 2019)
  - Update to Fluent Syntax 0.9
  - Unify benchmark testsuite with fluent.js

## fluent-syntax 0.8.0 (January 31, 2019)
  - Update to Fluent Syntax 0.8
  - Switch to zero-copy parser
  - Start using reference FTL fixtures in tests
  - Switch to criterion for benchmarks
  - Rust 2018 edition

## fluent-syntax 0.1.1 (August 29, 2018)

  - enable ParserError to be compared.

## fluent-syntax 0.1.0 (July 29, 2018)

  - Initial release of the standalone fluent-syntax.
    Based on fluent 0.2.0, and syntax 0.5
