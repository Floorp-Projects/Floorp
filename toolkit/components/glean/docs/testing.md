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

## `about:glean`

`about:glean` is a page in a running Firefox that allows you to
[debug the Glean SDK](https://mozilla.github.io/glean/book/user/debugging/index.html)
in Firefox Desktop.
It does this through query parameters:
* `logPings=true` To turn on debug-level logging of ping assembly and sending in
  `glean_core`.
  (Note you need to set `glean_core`'s log level to at least Debug
  (3) for these logs to make it out through `RUST_LOG` or `MOZ_LOG`)
* `tagPings=my-debug-tag` Set the
  [debug ping tag](https://mozilla.github.io/glean/book/dev/core/internal/debug-pings.html)
  to "my-debug-tag" for all pings assembled from this point on.
  Tagged pings are rerouted to the
  [Debug Ping Viewer](https://glean-debug-view-dev-237806.firebaseapp.com/)
  for ease of manual testing.
* `sendPing=ping-name` Send the ping named
  "ping-name" immediately with whatever data we have on hand for it.

If multiple query parameters are specified,
`logPings` will take effect before
`tagPings` which will take effect before
`sendPing`. That means
`about:glean?sendPing=baseline&tagPings=my-debug-tag&logPings=true`
will log and assemble the
"baseline" ping immediately with a debug tag of "my-debug-tag".

**Note:** At present no pings are registered in FOG so `sendPing` will not work.
However, builtin pings like "deletion-request" will respect the
`logPings` and `tagPings` values you set.
This will be fixed in
[bug 1653605](https://bugzilla.mozilla.org/show_bug.cgi?id=1653605).

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

## Python

The [Glean Parser](https://github.com/mozilla/glean_parser/)
has been augmented to generate FOG-specific APIs for Glean metrics.
This augmentation is tested by running:

`mach test toolkit/components/glean/pytest`

These tests require Python 3+.
If your default Python is Python 2, you may need to instead run:

`mach python-test --python 3 toolkit/components/glean/pytest`
