# Changelog

## Unreleased

  - …

## fluent-fallback 0.7.0 (Nov 9, 2022)
  - The `ResourceId`s are now stored as a `HashSet` rather than as a Vec. Adding a
    duplicate `ResourceId` is now a noop.

## fluent-fallback 0.6.0 (Dec 17, 2021)
  - Add `ResourceId` struct which allows fluent resources to be optional.

## fluent-fallback 0.5.0 (Jul 8, 2021)
  - Separate out `Bundles` for state management.

## fluent-fallback 0.4.4 (May 3, 2021)
  - Fix waiting from multiple tasks. (#224)
  - Bind locale iterator generics of `LocalesProvider` and `BundleGenerator`.

## fluent-fallback 0.4.3 (April 26, 2021)
  - Align errors even closer to fluent.js

## fluent-fallback 0.4.2 (April 9, 2021)
  - Align errors closer to fluent.js

## fluent-fallback 0.4.0 (February 9, 2021)
  - Use `fluent-bundle` 0.15.

## fluent-fallback 0.3.0 (February 3, 2021)
  - Handle locale management in `Localization`.

## fluent-fallback 0.2.2 (January 16, 2021)
  - Invalidate bundles on resource list change.

## fluent-fallback 0.2.1 (January 15, 2021)
  - Add `Localization::is_sync`

## fluent-fallback 0.2.0 (January 12, 2021)
  - Separate `Sync` and `Async` bundle generators.
  - Reorganize fallback logic.
  - Separate out prefetching trait.
  - Vendor in pin-cell.

## fluent-fallback 0.1.0 (January 3, 2021)
  - Update `fluent-bundle` to 0.14.
  - Switch from `elsa` to `chunky-vec`.
  - Add `Localization::with_generator`.
  - Add support for Streamed bundles.
  - Add `LocalizationError`.
  - Make `L10nKey`, `L10nMessage` and `L10nAttribute` types.

## fluent-fallback 0.0.4 (May 6, 2020)
  - Update `fluent-bundle` to 0.12.
  - Update `unic-langid` to 0.9.

## fluent-fallback 0.0.3 (February 13, 2020)
  - Update `fluent-bundle` to 0.10.
  - Update `unic-langid` to 0.8.

## fluent-fallback 0.0.2 (November 26, 2019)
  - Update `fluent-bundle` to 0.9.
  - Update `unic-langid` to 0.7.

## fluent-fallback 0.0.1 (August 1, 2019)

  - This is the first release to be listed in the CHANGELOG.
  - Basic support for language fallbacking and runtime locale changes.
