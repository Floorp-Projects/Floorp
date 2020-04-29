# Testing

Given the multiple API languages, processes, and dependencies,
testing FOG is a matter of choosing the right tool for the situation.

## Rust

Not all of our Rust code can be tested in a single fashion, unfortunately.

### Using `rusttests`

If the crate you're testing has no Gecko symbols you can write standard
[Rust tests](https://doc.rust-lang.org/book/ch11-01-writing-tests.html).

This supports both unit tests
(inline in the file under test) and integration tests
(in the `tests/` folder in the crate root).

To run FOG's `rusttests` suite use `mach rusttests`

### Using `gtest`

Because Gecko symbols aren't built for the
`rusttests` build,
any test that is written for a crate that uses Gecko symbols should be written as a
[`gtest`](https://github.com/google/googletest)
in `toolkit/components/glean/gtest/`.

By necessity these can only be integration tests against the compiled crate.

**Note:** When adding a new test file, don't forget to add it to
`toolkit/components/glean/gtest/moz.build` and use the
`FOG` prefix in your test names.

To run FOG's Rust `gtest` suite use `mach gtest FOG.*`
