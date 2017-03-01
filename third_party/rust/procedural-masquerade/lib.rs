//! # Custom `derive` pretending to be functional procedural macros on Rust 1.15
//!
//! This crate enables creating function-like macros (invoked as `foo!(...)`)
//! with a procedural component,
//! based on both custom `derive` (a.k.a. *Macros 1.1*) and `macro_rules!`.
//!
//! This convoluted mechanism enables such macros to run on stable Rust 1.15,
//! even though functional procedural macros (a.k.a. *Macros 2.0*) are not available yet.
//!
//! A library defining such a macro needs two crates: a “normal” one, and a `proc-macro` one.
//! In the example below we’ll call them `libfoo` and `libfoo-macros`, respectively.
//!
//! # Credits
//!
//! The trick that makes this crate work
//! is based on an idea from [David Tolnay](https://github.com/dtolnay).
//! Many thanks!
//!
//! # Example
//!
//! As a simple example, we’re going to re-implement the `stringify!` macro.
//! This is useless since `stringify!` already exists in the standard library,
//! and a bit absurd since this crate uses `stringify!` internally.
//!
//! Nevertheless, it serves as a simple example to demonstrate the use of this crate.
//!
//! ## The `proc-macro` crate
//!
//! The minimal `Cargo.toml` file is typical for Macros 1.1:
//!
//! ```toml
//! [package]
//! name = "libfoo-macros"
//! version = "1.0.0"
//!
//! [lib]
//! proc-macro = true
//! ```
//!
//! In the code, we define the procedural part of our macro in a function.
//! This function will not be used directly by end users,
//! but it still needs to be re-exported to them
//! (because of limitations in `macro_rules!`).
//!
//! To avoid name collisions, we and a long and explicit prefix in the function’s name.
//!
//! The function takes a string containing arbitrary Rust tokens,
//! and returns a string that is parsed as *items*.
//! The returned string can contain constants, statics, functions, `impl`s, etc.,
//! but not expressions directly.
//!
//! ```rust
//! #[macro_use] extern crate procedural_masquerade;
//! extern crate proc_macro;
//!
//! define_proc_macros! {
//!     #[allow(non_snake_case)]
//!     pub fn foo_internal__stringify_const(input: &str) -> String {
//!         format!("const STRINGIFIED: &'static str = {:?};", input)
//!     }
//! }
//! ```
//!
//! A less trivial macro would probably use
//! the [`syn`](https://github.com/dtolnay/syn/) crate to parse its input
//! and the [`quote`](https://github.com/dtolnay/quote) crate to generate its output.
//!
//! ## The library crate
//!
//! ```toml
//! [package]
//! name = "libfoo"
//! version = "1.0.0"
//!
//! [dependencies]
//! cssparser-macros = {path = "./macros", version = "1.0"}
//! ```
//!
//! ```rust
//! #[macro_use] extern crate libfoo_macros;  // (1)
//!
//! pub use libfoo_macros::*;  // (2)
//!
//! define_invoke_proc_macro!(libfoo__invoke_proc_macro);  // (3)
//!
//! #[macro_export]
//! macro_rules! foo_stringify {  // (4)
//!     ( $( $tts: tt ) ) => {
//!         {  // (5)
//!             libfoo__invoke_proc_macro! {  // (6)
//!                 foo_internal__stringify_const!( $( $tts ) )  // (7)
//!             }
//!             STRINGIFIED  // (8)
//!         }
//!     }
//! }
//! ```
//!
//! Let’s go trough the numbered lines one by one:
//!
//! 1. `libfoo` depends on the other `libfoo-macros`, and imports its macros.
//! 2. Everything exported by `libfoo-macros` (which is one custom `derive`)
//!    is re-exported to users of `libfoo`.
//!    They’re not expected to use it directly,
//!    but expansion of the `foo_stringify` macro needs it.
//! 3. This macro invocation defines yet another macro, called `libfoo__invoke_proc_macro`,
//!    which is also exported.
//!    This indirection is necessary
//!    because re-exporting `macro_rules!` macros doesn’t work currently,
//!    and once again it is used by the expansion of `foo_stringify`.
//!    Again, we use a long prefix to avoid name collisions.
//! 4. Finally, we define the macro that we really want.
//!    This one has a name that users will use.
//! 5. The expansion of this macro will define some items,
//!    whose names are not hygienic in `macro_rules`.
//!    So we wrap everything in an extra `{…}` block to prevent these names for leaking.
//! 6. Here we use the macro defined in (3),
//!    which allows us to write something that look like invoking a functional procedural macro,
//!    but really uses a custom `derive`.
//!    This will define a type called `ProceduralMasqueradeDummyType`,
//!    as a placeholder to use `derive`.
//!    If `libfoo__invoke_proc_macro!` is to be used more than once,
//!    each use needs to be nested in another block
//!    so that the names of multiple dummy types don’t collide.
//! 7. In addition to the dummy type,
//!    the items returned by our procedural component are inserted here.
//!    (In this case the `STRINGIFIED` constant.)
//! 8. Finally, we write the expression that we want the macro to evaluate to.
//!    This expression can use parts of `foo_stringify`’s input,
//!    it can contain control-flow statements like `return` or `continue`,
//!    and of course refer to procedurally-defined items.
//!
//! This macro can be used in an expression context.
//! It expands to a block-expression that contains some items (as an implementation detail)
//! and ends with another expression.
//!
//! ## For users
//!
//! Users of `libfoo` don’t need to worry about any of these implementation details.
//! They can use the `foo_stringify` macro as if it were a simle `macro_rules` macro:
//!
//! ```rust
//! #[macro_use] extern crate libfoo;
//!
//! fn main() {
//!     do_something(foo_stringify!(1 + 2));
//! }
//!
//! fn do_something(_: &str) { /* ... */ }
//! ```
//!
//! # More
//!
//! To see a more complex example, look at
//! [`cssparser`’s `src/macros.rs](https://github.com/servo/rust-cssparser/blob/master/src/macros.rs)
//! and
//! [`cssparser-macros`’s `macros/lib.rs](https://github.com/servo/rust-cssparser/blob/master/macros/lib.rs).

/// This macro wraps `&str -> String` functions
/// in custom `derive` implementations with `#[proc_macro_derive]`.
///
/// See crate documentation for details.
#[macro_export]
macro_rules! define_proc_macros {
    (
        $(
            $( #[$attr:meta] )*
            pub fn $proc_macro_name: ident ($input: ident : &str) -> String
            $body: block
        )+
    ) => {
        $(
            $( #[$attr] )*
            #[proc_macro_derive($proc_macro_name)]
            pub fn $proc_macro_name(derive_input: ::proc_macro::TokenStream)
                                    -> ::proc_macro::TokenStream {
                let $input = derive_input.to_string();
                let $input = $crate::_extract_input(&$input);
                $body.parse().unwrap()
            }
        )+
    }
}

/// Implementation detail of `define_proc_macros!`.
///
/// **This function is not part of the public API. It can change or be removed between any versions.**
#[doc(hidden)]
pub fn _extract_input(derive_input: &str) -> &str {
    let mut input = derive_input;

    for expected in &["#[allow(unused)]", "enum", "ProceduralMasqueradeDummyType", "{",
                     "Input", "=", "(0,", "stringify!", "("] {
        input = input.trim_left();
        assert!(input.starts_with(expected),
                "expected prefix {:?} not found in {:?}", expected, derive_input);
        input = &input[expected.len()..];
    }

    for expected in [")", ").0,", "}"].iter().rev() {
        input = input.trim_right();
        assert!(input.ends_with(expected),
                "expected suffix {:?} not found in {:?}", expected, derive_input);
        let end = input.len() - expected.len();
        input = &input[..end];
    }

    input
}

/// This macro expands to the definition of another macro (whose name is given as a parameter).
///
/// See crate documentation for details.
#[macro_export]
macro_rules! define_invoke_proc_macro {
    ($macro_name: ident) => {
        /// Implementation detail of other macros in this crate.
        #[doc(hidden)]
        #[macro_export]
        macro_rules! $macro_name {
            ($proc_macro_name: ident ! $paren: tt) => {
                #[derive($proc_macro_name)]
                #[allow(unused)]
                enum ProceduralMasqueradeDummyType {
                    // The magic happens here.
                    //
                    // We use an `enum` with an explicit discriminant
                    // because that is the only case where a type definition
                    // can contain a (const) expression.
                    //
                    // `(0, "foo").0` evalutes to 0, with the `"foo"` part ignored.
                    //
                    // By the time the `#[proc_macro_derive]` function
                    // implementing `#[derive($proc_macro_name)]` is called,
                    // `$paren` has already been replaced with the input of this inner macro,
                    // but `stringify!` has not been expanded yet.
                    //
                    // This how arbitrary tokens can be inserted
                    // in the input to the `#[proc_macro_derive]` function.
                    //
                    // Later, `stringify!(...)` is expanded into a string literal
                    // which is then ignored.
                    // Using `stringify!` enables passing arbitrary tokens
                    // rather than only what can be parsed as a const expression.
                    Input = (0, stringify! $paren ).0
                }
            }
        }
    }
}
