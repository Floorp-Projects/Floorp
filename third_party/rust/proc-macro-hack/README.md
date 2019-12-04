Procedural macros in expression position
========================================

[![Build Status](https://api.travis-ci.org/dtolnay/proc-macro-hack.svg?branch=master)](https://travis-ci.org/dtolnay/proc-macro-hack)
[![Latest Version](https://img.shields.io/crates/v/proc-macro-hack.svg)](https://crates.io/crates/proc-macro-hack)

As of Rust 1.30, the language supports user-defined function-like procedural
macros. However these can only be invoked in item position, not in
statements or expressions.

This crate implements an alternative type of procedural macro that can be
invoked in statement or expression position.

This approach works with any stable or nightly Rust version 1.31+.

## Defining procedural macros

Two crates are required to define a procedural macro.

### The implementation crate

This crate must contain nothing but procedural macros. Private helper
functions and private modules are fine but nothing can be public.

[> example of an implementation crate][demo-hack-impl]

Just like you would use a #\[proc_macro\] attribute to define a natively
supported procedural macro, use proc-macro-hack's #\[proc_macro_hack\]
attribute to define a procedural macro that works in expression position.
The function signature is the same as for ordinary function-like procedural
macros.

```rust
extern crate proc_macro;

use proc_macro::TokenStream;
use proc_macro_hack::proc_macro_hack;
use quote::quote;
use syn::{parse_macro_input, Expr};

#[proc_macro_hack]
pub fn add_one(input: TokenStream) -> TokenStream {
    let expr = parse_macro_input!(input as Expr);
    TokenStream::from(quote! {
        1 + (#expr)
    })
}
```

### The declaration crate

This crate is allowed to contain other public things if you need, for
example traits or functions or ordinary macros.

[> example of a declaration crate][demo-hack]

Within the declaration crate there needs to be a re-export of your
procedural macro from the implementation crate. The re-export also carries a
\#\[proc_macro_hack\] attribute.

```rust
use proc_macro_hack::proc_macro_hack;

/// Add one to an expression.
///
/// (Documentation goes here on the re-export, not in the other crate.)
#[proc_macro_hack]
pub use demo_hack_impl::add_one;
```

Both crates depend on `proc-macro-hack`:

```toml
[dependencies]
proc-macro-hack = "0.5"
```

Additionally, your implementation crate (but not your declaration crate) is
a proc macro crate:

```toml
[lib]
proc-macro = true
```

## Using procedural macros

Users of your crate depend on your declaration crate (not your
implementation crate), then use your procedural macros as usual.

[> example of a downstream crate][example]

```rust
use demo_hack::add_one;

fn main() {
    let two = 2;
    let nine = add_one!(two) + add_one!(2 + 3);
    println!("nine = {}", nine);
}
```

[demo-hack-impl]: https://github.com/dtolnay/proc-macro-hack/tree/master/demo-hack-impl
[demo-hack]: https://github.com/dtolnay/proc-macro-hack/tree/master/demo-hack
[example]: https://github.com/dtolnay/proc-macro-hack/tree/master/example

## Limitations

- Only proc macros in expression position are supported. Proc macros in type
  position ([#10]) or pattern position ([#20]) are not supported.

- By default, nested invocations are not supported i.e. the code emitted by a
  proc-macro-hack macro invocation cannot contain recursive calls to the same
  proc-macro-hack macro nor calls to any other proc-macro-hack macros. Use
  [`proc-macro-nested`] if you require support for nested invocations.

- By default, hygiene is structured such that the expanded code can't refer to
  local variables other than those passed by name somewhere in the macro input.
  If your macro must refer to *local* variables that don't get named in the
  macro input, use `#[proc_macro_hack(fake_call_site)]` on the re-export in your
  declaration crate. *Most macros won't need this.*

[#10]: https://github.com/dtolnay/proc-macro-hack/issues/10
[#20]: https://github.com/dtolnay/proc-macro-hack/issues/20
[`proc-macro-nested`]: https://docs.rs/proc-macro-nested

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this hack by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
