# Version 0.1.1

- Add a `Causes` iterator, which iterates over the causes of a failure. Can be
  accessed through the `Fail::causes` or `Error::causes` methods.
- Add the `bail!` macro, which "throws" from the function.
- Add the `ensure!` macro, which is like an "assert" which throws instead of
  panicking.
- The derive now supports a no_std mode.
- The derive is re-exported from `failure` by default, so that users do not
  have to directly depend on `failure_derive`.
- Add a impl of `From<D> for Context<D>`, allowing users to `?` the `D` type to
  produce a `Context<D>` (for cases where there is no further underlying
  error).

# Version 0.1.0

- Initial version.
