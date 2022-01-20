# proc-macro2

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/proc--macro2-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/proc-macro2)
[<img alt="crates.io" src="https://img.shields.io/crates/v/proc-macro2.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/proc-macro2)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-proc--macro2-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/proc-macro2)
[<img alt="build status" src="https://img.shields.io/github/workflow/status/dtolnay/proc-macro2/CI/master?style=for-the-badge" height="20">](https://github.com/dtolnay/proc-macro2/actions?query=branch%3Amaster)

A wrapper around the procedural macro API of the compiler's `proc_macro` crate.
This library serves two purposes:

- **Bring proc-macro-like functionality to other contexts like build.rs and
  main.rs.** Types from `proc_macro` are entirely specific to procedural macros
  and cannot ever exist in code outside of a procedural macro. Meanwhile
  `proc_macro2` types may exist anywhere including non-macro code. By developing
  foundational libraries like [syn] and [quote] against `proc_macro2` rather
  than `proc_macro`, the procedural macro ecosystem becomes easily applicable to
  many other use cases and we avoid reimplementing non-macro equivalents of
  those libraries.

- **Make procedural macros unit testable.** As a consequence of being specific
  to procedural macros, nothing that uses `proc_macro` can be executed from a
  unit test. In order for helper libraries or components of a macro to be
  testable in isolation, they must be implemented using `proc_macro2`.

[syn]: https://github.com/dtolnay/syn
[quote]: https://github.com/dtolnay/quote

## Usage

```toml
[dependencies]
proc-macro2 = "1.0"
```

The skeleton of a typical procedural macro typically looks like this:

```rust
extern crate proc_macro;

#[proc_macro_derive(MyDerive)]
pub fn my_derive(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = proc_macro2::TokenStream::from(input);

    let output: proc_macro2::TokenStream = {
        /* transform input */
    };

    proc_macro::TokenStream::from(output)
}
```

If parsing with [Syn], you'll use [`parse_macro_input!`] instead to propagate
parse errors correctly back to the compiler when parsing fails.

[`parse_macro_input!`]: https://docs.rs/syn/1.0/syn/macro.parse_macro_input.html

## Unstable features

The default feature set of proc-macro2 tracks the most recent stable compiler
API. Functionality in `proc_macro` that is not yet stable is not exposed by
proc-macro2 by default.

To opt into the additional APIs available in the most recent nightly compiler,
the `procmacro2_semver_exempt` config flag must be passed to rustc. We will
polyfill those nightly-only APIs back to Rust 1.31.0. As these are unstable APIs
that track the nightly compiler, minor versions of proc-macro2 may make breaking
changes to them at any time.

```
RUSTFLAGS='--cfg procmacro2_semver_exempt' cargo build
```

Note that this must not only be done for your crate, but for any crate that
depends on your crate. This infectious nature is intentional, as it serves as a
reminder that you are outside of the normal semver guarantees.

Semver exempt methods are marked as such in the proc-macro2 documentation.

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
