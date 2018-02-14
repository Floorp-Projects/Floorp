Rust Quasi-Quoting
==================

[![Build Status](https://api.travis-ci.org/dtolnay/quote.svg?branch=master)](https://travis-ci.org/dtolnay/quote)
[![Latest Version](https://img.shields.io/crates/v/quote.svg)](https://crates.io/crates/quote)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://docs.rs/quote/)

Quasi-quoting without a Syntex dependency, intended for use with [Macros
1.1](https://github.com/rust-lang/rfcs/blob/master/text/1681-macros-1.1.md).

```toml
[dependencies]
quote = "0.3"
```

```rust
#[macro_use]
extern crate quote;
```

## What is quasi-quoting?

Quasi-quoting is a way of writing code and treating it as data, similar to
writing code inside of a double-quoted string literal except more friendly to
your text editor or IDE. It does not get in the way of syntax highlighting,
brace matching, indentation, or autocompletion, all of which you would lose by
writing code inside of double quotes.

Check out
[my meetup talk](https://air.mozilla.org/rust-meetup-december-2016-12-15/)
on the topic to learn more about the use case. Start the video at 3:00.

This crate is motivated by the Macros 1.1 use case, but is a general-purpose
Rust quasi-quoting library and is not specific to procedural macros.

## Syntax

The quote crate provides a `quote!` macro within which you can write Rust code
that gets packaged into a `quote::Tokens` and can be treated as data. You should
think of `quote::Tokens` as representing a fragment of Rust source code. Call
`to_string()` or `as_str()` on a Tokens to get back the fragment of source code
as a string.

Within the `quote!` macro, interpolation is done with `#var`. Any type
implementing the `quote::ToTokens` trait can be interpolated. This includes most
Rust primitive types as well as most of the syntax tree types from
[`syn`](https://github.com/dtolnay/syn).

```rust
let tokens = quote! {
    struct SerializeWith #generics #where_clause {
        value: &'a #field_ty,
        phantom: ::std::marker::PhantomData<#item_ty>,
    }

    impl #generics serde::Serialize for SerializeWith #generics #where_clause {
        fn serialize<S>(&self, s: &mut S) -> Result<(), S::Error>
            where S: serde::Serializer
        {
            #path(self.value, s)
        }
    }

    SerializeWith {
        value: #value,
        phantom: ::std::marker::PhantomData::<#item_ty>,
    }
};
```

Repetition is done using `#(...)*` or `#(...),*` very similar to `macro_rules!`:

- `#(#var)*` - no separators
- `#(#var),*` - the character before the asterisk is used as a separator
- `#( struct #var; )*` - the repetition can contain other things
- `#( #k => println!("{}", #v), )*` - even multiple interpolations

Tokens can be interpolated into other quotes:

```rust
let t = quote! { /* ... */ };
return quote! { /* ... */ #t /* ... */ };
```

The `quote!` macro relies on deep recursion so some large invocations may fail
with "recursion limit reached" when you compile. If it fails, bump up the
recursion limit by adding `#![recursion_limit = "128"]` to your crate. An even
higher limit may be necessary for especially large invocations. You don't need
this unless the compiler tells you that you need it.

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
