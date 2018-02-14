Nom parser for Rust source code
===============================

[![Build Status](https://api.travis-ci.org/dtolnay/syn.svg?branch=master)](https://travis-ci.org/dtolnay/syn)
[![Latest Version](https://img.shields.io/crates/v/syn.svg)](https://crates.io/crates/syn)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://dtolnay.github.io/syn/syn/)

Parse Rust source code without a Syntex dependency, intended for use with
[Macros 1.1](https://github.com/rust-lang/rfcs/blob/master/text/1681-macros-1.1.md).

Designed for fast compile time.

- Compile time for `syn` (from scratch including all dependencies): **6 seconds**
- Compile time for the `syntex`/`quasi`/`aster` stack: **60+ seconds**

If you get stuck with Macros 1.1 I am happy to provide help even if the issue is
not related to syn. Please file a ticket in this repo.

## Usage with Macros 1.1

```toml
[dependencies]
syn = "0.11"
quote = "0.3"

[lib]
proc-macro = true
```

```rust
extern crate proc_macro;
use proc_macro::TokenStream;

extern crate syn;

#[macro_use]
extern crate quote;

#[proc_macro_derive(MyMacro)]
pub fn my_macro(input: TokenStream) -> TokenStream {
    let source = input.to_string();

    // Parse the string representation into a syntax tree
    let ast = syn::parse_derive_input(&source).unwrap();

    // Build the output, possibly using quasi-quotation
    let expanded = quote! {
        // ...
    };

    // Parse back to a token stream and return it
    expanded.parse().unwrap()
}
```

## Complete example

Suppose we have the following simple trait which returns the number of fields in
a struct:

```rust
trait NumFields {
    fn num_fields() -> usize;
}
```

A complete Macros 1.1 implementation of `#[derive(NumFields)]` based on `syn`
and [`quote`](https://github.com/dtolnay/quote) looks like this:

```rust
extern crate proc_macro;
use proc_macro::TokenStream;

extern crate syn;

#[macro_use]
extern crate quote;

#[proc_macro_derive(NumFields)]
pub fn num_fields(input: TokenStream) -> TokenStream {
    let source = input.to_string();

    // Parse the string representation into a syntax tree
    let ast = syn::parse_derive_input(&source).unwrap();

    // Build the output
    let expanded = expand_num_fields(&ast);

    // Return the generated impl as a TokenStream
    expanded.parse().unwrap()
}

fn expand_num_fields(ast: &syn::DeriveInput) -> quote::Tokens {
    let n = match ast.body {
        syn::Body::Struct(ref data) => data.fields().len(),
        syn::Body::Enum(_) => panic!("#[derive(NumFields)] can only be used with structs"),
    };

    // Used in the quasi-quotation below as `#name`
    let name = &ast.ident;

    // Helper is provided for handling complex generic types correctly and effortlessly
    let (impl_generics, ty_generics, where_clause) = ast.generics.split_for_impl();

    quote! {
        // The generated impl
        impl #impl_generics ::mycrate::NumFields for #name #ty_generics #where_clause {
            fn num_fields() -> usize {
                #n
            }
        }
    }
}
```

For a more elaborate example that involves trait bounds, enums, and different
kinds of structs, check out [`DeepClone`] and [`deep-clone-derive`].

[`DeepClone`]: https://github.com/asajeffrey/deep-clone
[`deep-clone-derive`]: https://github.com/asajeffrey/deep-clone/blob/master/deep-clone-derive/lib.rs

## Testing

Macros 1.1 has a restriction that your proc-macro crate must export nothing but
`proc_macro_derive` functions, and also `proc_macro_derive` procedural macros
cannot be used from the same crate in which they are defined. These restrictions
may be lifted in the future but for now they make writing tests a bit trickier
than for other types of code.

In particular, you will not be able to write test functions like `#[test] fn
it_works() { ... }` in line with your code. Instead, either put tests in a
[`tests` directory](https://doc.rust-lang.org/book/testing.html#the-tests-directory)
or in a separate crate entirely.

Additionally, if your procedural macro implements a particular trait, that trait
must be defined in a separate crate from the procedural macro.

As a concrete example, suppose your procedural macro crate is called `my_derive`
and it implements a trait called `my_crate::MyTrait`. Your unit tests for the
procedural macro can go in `my_derive/tests/test.rs` or into a separate crate
`my_tests/tests/test.rs`. Either way the test would look something like this:

```rust
#[macro_use]
extern crate my_derive;

extern crate my_crate;
use my_crate::MyTrait;

#[test]
fn it_works() {
    #[derive(MyTrait)]
    struct S { /* ... */ }

    /* test the thing */
}
```

## Debugging

When developing a procedural macro it can be helpful to look at what the
generated code looks like. Use `cargo rustc -- -Zunstable-options
--pretty=expanded` or the
[`cargo expand`](https://github.com/dtolnay/cargo-expand) subcommand.

To show the expanded code for some crate that uses your procedural macro, run
`cargo expand` from that crate. To show the expanded code for one of your own
test cases, run `cargo expand --test the_test_case` where the last argument is
the name of the test file without the `.rs` extension.

This write-up by Brandon W Maister discusses debugging in more detail:
[Debugging Rust's new Custom Derive
system](https://quodlibetor.github.io/posts/debugging-rusts-new-custom-derive-system/).

## Optional features

Syn puts a lot of functionality behind optional features in order to optimize
compile time for the most common use cases. These are the available features and
their effect on compile time. Dependencies are included in the compile times.

Features | Compile time | Functionality
--- | --- | ---
*(none)* | 3 sec | The data structures representing the AST of Rust structs, enums, and types.
parsing | 6 sec | Parsing Rust source code containing structs and enums into an AST.
printing | 4 sec | Printing an AST of structs and enums as Rust source code.
**parsing, printing** | **6 sec** | **This is the default.** Parsing and printing of Rust structs and enums. This is typically what you want for implementing Macros 1.1 custom derives.
full | 4 sec | The data structures representing the full AST of all possible Rust code.
full, parsing | 9 sec | Parsing any valid Rust source code to an AST.
full, printing | 6 sec | Turning an AST into Rust source code.
full, parsing, printing | 11 sec | Parsing and printing any Rust syntax.

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
