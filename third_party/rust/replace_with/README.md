# replace_with

[![Crates.io](https://img.shields.io/crates/v/replace_with.svg?maxAge=86400)](https://crates.io/crates/replace_with)
[![MIT / Apache 2.0 licensed](https://img.shields.io/crates/l/replace_with.svg?maxAge=2592000)](#License)
[![Build Status](https://dev.azure.com/alecmocatta/replace_with/_apis/build/status/tests?branchName=master)](https://dev.azure.com/alecmocatta/replace_with/_build?definitionId=11)

[📖 Docs](https://docs.rs/replace_with) | [💬 Chat](https://constellation.zulipchat.com/#narrow/stream/213236-subprojects)

Temporarily take ownership of a value at a mutable location, and replace it with a new value based on the old one.

This crate provides the function [`replace_with()`](https://docs.rs/replace_with/0.1/replace_with/fn.replace_with.html), which is like [`std::mem::replace()`](https://doc.rust-lang.org/std/mem/fn.replace.html) except it allows the replacement value to be mapped from the original value.

See [RFC 1736](https://github.com/rust-lang/rfcs/pull/1736) for a lot of discussion as to its merits. It was never merged, and the desired ability to temporarily move out of `&mut T` doesn't exist yet, so this crate is my interim solution.

It's very akin to [`take_mut`](https://github.com/Sgeo/take_mut), though uses `Drop` instead of [`std::panic::catch_unwind()`](https://doc.rust-lang.org/std/panic/fn.catch_unwind.html) to react to unwinding, which avoids the optimisation barrier of calling the `extern "C" __rust_maybe_catch_panic()`. As such it's up to ∞x faster. The API also attempts to make slightly more explicit the behavior on panic – [`replace_with()`](https://docs.rs/replace_with/0.1/replace_with/fn.replace_with.html) accepts two closures such that aborting in the "standard case" where the mapping closure (`FnOnce(T) -> T`) panics (as [`take_mut::take()`](https://docs.rs/take_mut/0.2.2/take_mut/fn.take.html) does) is avoided. If the second closure (`FnOnce() -> T`) panics, however, then it does indeed abort. The "abort on first panic" behaviour is available with [`replace_with_or_abort()`](https://docs.rs/replace_with/0.1/replace_with/fn.replace_with_or_abort.html).

## Example

Consider this motivating example:

```rust
enum States {
    A(String),
    B(String),
}

impl States {
    fn poll(&mut self) {
        // error[E0507]: cannot move out of borrowed content
        *self = match *self {
            //        ^^^^^ cannot move out of borrowed content
            States::A(a) => States::B(a),
            States::B(a) => States::A(a),
        };
    }
}
```

Depending on context this can be quite tricky to work around. With this crate, however:

```rust
enum States {
    A(String),
    B(String),
}

impl States {
    fn poll(&mut self) {
        replace_with_or_abort(self, |self_| match self_ {
            States::A(a) => States::B(a),
            States::B(a) => States::A(a),
        });
    }
}
```

Huzzah!

## `no_std`

To use `replace_with` with `no_std` you have to disable the `std` feature, which is active by default, by specifying your dependency to it like this:

```toml
# Cargo.toml

[dependencies.replace_with]
version = ...
default-features = false
features = []
...
```

The [`replace_with()`](https://docs.rs/replace_with/0.1/replace_with/fn.replace_with.html) & [`replace_with_or_default()`](https://docs.rs/replace_with/0.1/replace_with/fn.replace_with_or_default.html) functions are available on stable Rust both, with and without `std`.

The [`replace_with_or_abort()`](https://docs.rs/replace_with/0.1/replace_with/fn.replace_with_or_abort.html) function however by default makes use of [`std::process::abort()`](https://doc.rust-lang.org/std/process/fn.abort.html) which is not available with `no_std`.

As such `replace_with` will by default call [`core::intrinsics::abort()`](https://doc.rust-lang.org/core/intrinsics/fn.abort.html) instead, which in turn requires nightly Rust.

Not everything is lost for stable `no_std` though, `replace_with` has one more trick up its sleeve:

### panic = "abort"

If you define [`panic = abort`](https://doc.rust-lang.org/edition-guide/rust-2018/error-handling-and-panics/aborting-on-panic.html) in the `[profile]` section of your crate's `Cargo.toml` …

```toml
# Cargo.toml

[profile.debug]
panic = "abort"

[profile.release]
panic = "abort"
```

… and add the `"panic_abort"` feature to `replace_with` in the `dependencies` section of your crate's `Cargo.toml` …

```toml
# Cargo.toml

[dependencies.replace_with]
features = ["panic_abort"]
...
```

… the `"panic_abort"` feature enables the [`replace_with_or_abort_unchecked()`](https://docs.rs/replace_with/0.1/replace_with/fn.replace_with_or_abort_unchecked.html) function becomes on stable Rust as an `unsafe` function, a simple wrapper around `ptr::write(dest, f(ptr::read(dest)));`.

**Word of caution:** It is crucial to only ever use this function having defined `panic = "abort"`, or else bad things may happen. It's *up to you* to uphold this invariant!

## License
Licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE.txt](LICENSE-APACHE.txt) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT.txt](LICENSE-MIT.txt) or http://opensource.org/licenses/MIT)

at your option.

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.
