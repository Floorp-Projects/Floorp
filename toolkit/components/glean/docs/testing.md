# Testing

Given the multiple API languages, processes, and dependencies,
testing FOG is a matter of choosing the right tool for the situation.

## Logging

An often-overlooked first line of testing is "what do the logs say?".
To turn on logging for FOG, use any of the following:
* Run Firefox with `RUST_LOG="glean=info,fog=info,glean_core=info"`.
    * On some platforms this will use terminal colours to indicate log level.
* Run Firefox with `MOZ_LOG="timestamp,glean::*:5,fog::*:5,glean_core::*:5"`.
* Set the following prefs:
    * `logging.config.timestamp` to `true`
    * `logging.fog::*` to `5`
    * `logging.glean::*` to `5`
    * `logging.glean_core::*` to `5`

For more information on logging in Firefox Desktop, see the
[Gecko Logging docs](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Gecko_Logging).

## Rust

Not all of our Rust code can be tested in a single fashion, unfortunately.

### Using `rusttests`

If the crate you're testing has no Gecko symbols you can write standard
[Rust tests](https://doc.rust-lang.org/book/ch11-01-writing-tests.html).

This supports both unit tests
(inline in the file under test) and integration tests
(in the `tests/` folder in the crate root).

To run FOG's `rusttests` suite use `mach rusttests`

If the crate uses only a few Gecko symbols, they may use the
`with_gecko` feature to conditionally use them.
This allows the crate to test its non-Gecko-adjacent code using Rust tests.
(You will need to cover the Gecko-adjacent code via another means.)

### Using `gtest`

Because Gecko symbols aren't built for the
`rusttests` build,
any test that is written for code that uses Gecko symbols should be written as a
[`gtest`](https://github.com/google/googletest)
in `toolkit/components/glean/gtest/`.

By necessity these can only be integration tests against the compiled crate.

**Note:** When adding a new test file, don't forget to add it to
`toolkit/components/glean/gtest/moz.build` and use the
`FOG` prefix in your test names
(e.g. `TEST(FOG, YourTestName) { ... }`).

To run FOG's Rust `gtest` suite use `mach gtest FOG.*`
