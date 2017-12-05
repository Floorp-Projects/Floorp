This directory contains snippets of code that should yield a
warning/note/help/error at compilation. Syntax of annotations is described in
[rust documentation](https://github.com/rust-lang/rust/blob/master/src/test/COMPILER_TESTS.md).
For more information check out [`compiletest` crate](https://github.com/laumann/compiletest-rs).

To run compile tests issue `cargo +nightly --test --features compiletest`.

## Notes on working with `compiletest` crate

* Currently code that is inside macro should not be annotated, as `compiletest`
    crate cannot deal with the fact that macro invocations effectively changes
    line numbering. To prevent this add a `// error-pattern:<your error message here>`
    on the top of the file and make sure that you set `deny` lint level
    if you want to test compiler message different than error.
* `compiletest` crate by default sets `allow(dead_code)` lint level so make sure
    that you change it to something suiting your needs even if the warning is
    issued prior to any macro invocation.
* If you get a message `error: 0 unexpected errors found, 1 expected errors not found`
  despite the fact that some error was bound to occur don't worry - it's a known
  issue in the `compiletest` crate and your error was probably not registered -
  make sure that your annotations are correct and that you are setting correct
  lint levels.
