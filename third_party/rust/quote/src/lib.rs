//! [![github]](https://github.com/dtolnay/quote)&ensp;[![crates-io]](https://crates.io/crates/quote)&ensp;[![docs-rs]](https://docs.rs/quote)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! <br>
//!
//! This crate provides the [`quote!`] macro for turning Rust syntax tree data
//! structures into tokens of source code.
//!
//! [`quote!`]: macro.quote.html
//!
//! Procedural macros in Rust receive a stream of tokens as input, execute
//! arbitrary Rust code to determine how to manipulate those tokens, and produce
//! a stream of tokens to hand back to the compiler to compile into the caller's
//! crate. Quasi-quoting is a solution to one piece of that &mdash; producing
//! tokens to return to the compiler.
//!
//! The idea of quasi-quoting is that we write *code* that we treat as *data*.
//! Within the `quote!` macro, we can write what looks like code to our text
//! editor or IDE. We get all the benefits of the editor's brace matching,
//! syntax highlighting, indentation, and maybe autocompletion. But rather than
//! compiling that as code into the current crate, we can treat it as data, pass
//! it around, mutate it, and eventually hand it back to the compiler as tokens
//! to compile into the macro caller's crate.
//!
//! This crate is motivated by the procedural macro use case, but is a
//! general-purpose Rust quasi-quoting library and is not specific to procedural
//! macros.
//!
//! ```toml
//! [dependencies]
//! quote = "1.0"
//! ```
//!
//! <br>
//!
//! # Example
//!
//! The following quasi-quoted block of code is something you might find in [a]
//! procedural macro having to do with data structure serialization. The `#var`
//! syntax performs interpolation of runtime variables into the quoted tokens.
//! Check out the documentation of the [`quote!`] macro for more detail about
//! the syntax. See also the [`quote_spanned!`] macro which is important for
//! implementing hygienic procedural macros.
//!
//! [a]: https://serde.rs/
//! [`quote_spanned!`]: macro.quote_spanned.html
//!
//! ```
//! # use quote::quote;
//! #
//! # let generics = "";
//! # let where_clause = "";
//! # let field_ty = "";
//! # let item_ty = "";
//! # let path = "";
//! # let value = "";
//! #
//! let tokens = quote! {
//!     struct SerializeWith #generics #where_clause {
//!         value: &'a #field_ty,
//!         phantom: core::marker::PhantomData<#item_ty>,
//!     }
//!
//!     impl #generics serde::Serialize for SerializeWith #generics #where_clause {
//!         fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
//!         where
//!             S: serde::Serializer,
//!         {
//!             #path(self.value, serializer)
//!         }
//!     }
//!
//!     SerializeWith {
//!         value: #value,
//!         phantom: core::marker::PhantomData::<#item_ty>,
//!     }
//! };
//! ```

// Quote types in rustdoc of other crates get linked to here.
#![doc(html_root_url = "https://docs.rs/quote/1.0.9")]
#![allow(
    clippy::doc_markdown,
    clippy::missing_errors_doc,
    clippy::missing_panics_doc,
    clippy::module_name_repetitions
)]

#[cfg(all(
    not(all(target_arch = "wasm32", target_os = "unknown")),
    feature = "proc-macro"
))]
extern crate proc_macro;

mod ext;
mod format;
mod ident_fragment;
mod to_tokens;

// Not public API.
#[doc(hidden)]
#[path = "runtime.rs"]
pub mod __private;

pub use crate::ext::TokenStreamExt;
pub use crate::ident_fragment::IdentFragment;
pub use crate::to_tokens::ToTokens;

// Not public API.
#[doc(hidden)]
pub mod spanned;

/// The whole point.
///
/// Performs variable interpolation against the input and produces it as
/// [`proc_macro2::TokenStream`].
///
/// Note: for returning tokens to the compiler in a procedural macro, use
/// `.into()` on the result to convert to [`proc_macro::TokenStream`].
///
/// [`TokenStream`]: https://docs.rs/proc-macro2/1.0/proc_macro2/struct.TokenStream.html
///
/// <br>
///
/// # Interpolation
///
/// Variable interpolation is done with `#var` (similar to `$var` in
/// `macro_rules!` macros). This grabs the `var` variable that is currently in
/// scope and inserts it in that location in the output tokens. Any type
/// implementing the [`ToTokens`] trait can be interpolated. This includes most
/// Rust primitive types as well as most of the syntax tree types from the [Syn]
/// crate.
///
/// [`ToTokens`]: trait.ToTokens.html
/// [Syn]: https://github.com/dtolnay/syn
///
/// Repetition is done using `#(...)*` or `#(...),*` again similar to
/// `macro_rules!`. This iterates through the elements of any variable
/// interpolated within the repetition and inserts a copy of the repetition body
/// for each one. The variables in an interpolation may be a `Vec`, slice,
/// `BTreeSet`, or any `Iterator`.
///
/// - `#(#var)*` — no separators
/// - `#(#var),*` — the character before the asterisk is used as a separator
/// - `#( struct #var; )*` — the repetition can contain other tokens
/// - `#( #k => println!("{}", #v), )*` — even multiple interpolations
///
/// <br>
///
/// # Hygiene
///
/// Any interpolated tokens preserve the `Span` information provided by their
/// `ToTokens` implementation. Tokens that originate within the `quote!`
/// invocation are spanned with [`Span::call_site()`].
///
/// [`Span::call_site()`]: https://docs.rs/proc-macro2/1.0/proc_macro2/struct.Span.html#method.call_site
///
/// A different span can be provided through the [`quote_spanned!`] macro.
///
/// [`quote_spanned!`]: macro.quote_spanned.html
///
/// <br>
///
/// # Return type
///
/// The macro evaluates to an expression of type `proc_macro2::TokenStream`.
/// Meanwhile Rust procedural macros are expected to return the type
/// `proc_macro::TokenStream`.
///
/// The difference between the two types is that `proc_macro` types are entirely
/// specific to procedural macros and cannot ever exist in code outside of a
/// procedural macro, while `proc_macro2` types may exist anywhere including
/// tests and non-macro code like main.rs and build.rs. This is why even the
/// procedural macro ecosystem is largely built around `proc_macro2`, because
/// that ensures the libraries are unit testable and accessible in non-macro
/// contexts.
///
/// There is a [`From`]-conversion in both directions so returning the output of
/// `quote!` from a procedural macro usually looks like `tokens.into()` or
/// `proc_macro::TokenStream::from(tokens)`.
///
/// [`From`]: https://doc.rust-lang.org/std/convert/trait.From.html
///
/// <br>
///
/// # Examples
///
/// ### Procedural macro
///
/// The structure of a basic procedural macro is as follows. Refer to the [Syn]
/// crate for further useful guidance on using `quote!` as part of a procedural
/// macro.
///
/// [Syn]: https://github.com/dtolnay/syn
///
/// ```
/// # #[cfg(any())]
/// extern crate proc_macro;
/// # extern crate proc_macro2;
///
/// # #[cfg(any())]
/// use proc_macro::TokenStream;
/// # use proc_macro2::TokenStream;
/// use quote::quote;
///
/// # const IGNORE_TOKENS: &'static str = stringify! {
/// #[proc_macro_derive(HeapSize)]
/// # };
/// pub fn derive_heap_size(input: TokenStream) -> TokenStream {
///     // Parse the input and figure out what implementation to generate...
///     # const IGNORE_TOKENS: &'static str = stringify! {
///     let name = /* ... */;
///     let expr = /* ... */;
///     # };
///     #
///     # let name = 0;
///     # let expr = 0;
///
///     let expanded = quote! {
///         // The generated impl.
///         impl heapsize::HeapSize for #name {
///             fn heap_size_of_children(&self) -> usize {
///                 #expr
///             }
///         }
///     };
///
///     // Hand the output tokens back to the compiler.
///     TokenStream::from(expanded)
/// }
/// ```
///
/// <p><br></p>
///
/// ### Combining quoted fragments
///
/// Usually you don't end up constructing an entire final `TokenStream` in one
/// piece. Different parts may come from different helper functions. The tokens
/// produced by `quote!` themselves implement `ToTokens` and so can be
/// interpolated into later `quote!` invocations to build up a final result.
///
/// ```
/// # use quote::quote;
/// #
/// let type_definition = quote! {...};
/// let methods = quote! {...};
///
/// let tokens = quote! {
///     #type_definition
///     #methods
/// };
/// ```
///
/// <p><br></p>
///
/// ### Constructing identifiers
///
/// Suppose we have an identifier `ident` which came from somewhere in a macro
/// input and we need to modify it in some way for the macro output. Let's
/// consider prepending the identifier with an underscore.
///
/// Simply interpolating the identifier next to an underscore will not have the
/// behavior of concatenating them. The underscore and the identifier will
/// continue to be two separate tokens as if you had written `_ x`.
///
/// ```
/// # use proc_macro2::{self as syn, Span};
/// # use quote::quote;
/// #
/// # let ident = syn::Ident::new("i", Span::call_site());
/// #
/// // incorrect
/// quote! {
///     let mut _#ident = 0;
/// }
/// # ;
/// ```
///
/// The solution is to build a new identifier token with the correct value. As
/// this is such a common case, the [`format_ident!`] macro provides a
/// convenient utility for doing so correctly.
///
/// ```
/// # use proc_macro2::{Ident, Span};
/// # use quote::{format_ident, quote};
/// #
/// # let ident = Ident::new("i", Span::call_site());
/// #
/// let varname = format_ident!("_{}", ident);
/// quote! {
///     let mut #varname = 0;
/// }
/// # ;
/// ```
///
/// Alternatively, the APIs provided by Syn and proc-macro2 can be used to
/// directly build the identifier. This is roughly equivalent to the above, but
/// will not handle `ident` being a raw identifier.
///
/// ```
/// # use proc_macro2::{self as syn, Span};
/// # use quote::quote;
/// #
/// # let ident = syn::Ident::new("i", Span::call_site());
/// #
/// let concatenated = format!("_{}", ident);
/// let varname = syn::Ident::new(&concatenated, ident.span());
/// quote! {
///     let mut #varname = 0;
/// }
/// # ;
/// ```
///
/// <p><br></p>
///
/// ### Making method calls
///
/// Let's say our macro requires some type specified in the macro input to have
/// a constructor called `new`. We have the type in a variable called
/// `field_type` of type `syn::Type` and want to invoke the constructor.
///
/// ```
/// # use quote::quote;
/// #
/// # let field_type = quote!(...);
/// #
/// // incorrect
/// quote! {
///     let value = #field_type::new();
/// }
/// # ;
/// ```
///
/// This works only sometimes. If `field_type` is `String`, the expanded code
/// contains `String::new()` which is fine. But if `field_type` is something
/// like `Vec<i32>` then the expanded code is `Vec<i32>::new()` which is invalid
/// syntax. Ordinarily in handwritten Rust we would write `Vec::<i32>::new()`
/// but for macros often the following is more convenient.
///
/// ```
/// # use quote::quote;
/// #
/// # let field_type = quote!(...);
/// #
/// quote! {
///     let value = <#field_type>::new();
/// }
/// # ;
/// ```
///
/// This expands to `<Vec<i32>>::new()` which behaves correctly.
///
/// A similar pattern is appropriate for trait methods.
///
/// ```
/// # use quote::quote;
/// #
/// # let field_type = quote!(...);
/// #
/// quote! {
///     let value = <#field_type as core::default::Default>::default();
/// }
/// # ;
/// ```
///
/// <p><br></p>
///
/// ### Interpolating text inside of doc comments
///
/// Neither doc comments nor string literals get interpolation behavior in
/// quote:
///
/// ```compile_fail
/// quote! {
///     /// try to interpolate: #ident
///     ///
///     /// ...
/// }
/// ```
///
/// ```compile_fail
/// quote! {
///     #[doc = "try to interpolate: #ident"]
/// }
/// ```
///
/// Macro calls in a doc attribute are not valid syntax:
///
/// ```compile_fail
/// quote! {
///     #[doc = concat!("try to interpolate: ", stringify!(#ident))]
/// }
/// ```
///
/// Instead the best way to build doc comments that involve variables is by
/// formatting the doc string literal outside of quote.
///
/// ```rust
/// # use proc_macro2::{Ident, Span};
/// # use quote::quote;
/// #
/// # const IGNORE: &str = stringify! {
/// let msg = format!(...);
/// # };
/// #
/// # let ident = Ident::new("var", Span::call_site());
/// # let msg = format!("try to interpolate: {}", ident);
/// quote! {
///     #[doc = #msg]
///     ///
///     /// ...
/// }
/// # ;
/// ```
///
/// <p><br></p>
///
/// ### Indexing into a tuple struct
///
/// When interpolating indices of a tuple or tuple struct, we need them not to
/// appears suffixed as integer literals by interpolating them as [`syn::Index`]
/// instead.
///
/// [`syn::Index`]: https://docs.rs/syn/1.0/syn/struct.Index.html
///
/// ```compile_fail
/// let i = 0usize..self.fields.len();
///
/// // expands to 0 + self.0usize.heap_size() + self.1usize.heap_size() + ...
/// // which is not valid syntax
/// quote! {
///     0 #( + self.#i.heap_size() )*
/// }
/// ```
///
/// ```
/// # use proc_macro2::{Ident, TokenStream};
/// # use quote::quote;
/// #
/// # mod syn {
/// #     use proc_macro2::{Literal, TokenStream};
/// #     use quote::{ToTokens, TokenStreamExt};
/// #
/// #     pub struct Index(usize);
/// #
/// #     impl From<usize> for Index {
/// #         fn from(i: usize) -> Self {
/// #             Index(i)
/// #         }
/// #     }
/// #
/// #     impl ToTokens for Index {
/// #         fn to_tokens(&self, tokens: &mut TokenStream) {
/// #             tokens.append(Literal::usize_unsuffixed(self.0));
/// #         }
/// #     }
/// # }
/// #
/// # struct Struct {
/// #     fields: Vec<Ident>,
/// # }
/// #
/// # impl Struct {
/// #     fn example(&self) -> TokenStream {
/// let i = (0..self.fields.len()).map(syn::Index::from);
///
/// // expands to 0 + self.0.heap_size() + self.1.heap_size() + ...
/// quote! {
///     0 #( + self.#i.heap_size() )*
/// }
/// #     }
/// # }
/// ```
#[macro_export]
macro_rules! quote {
    () => {
        $crate::__private::TokenStream::new()
    };
    ($($tt:tt)*) => {{
        let mut _s = $crate::__private::TokenStream::new();
        $crate::quote_each_token!(_s $($tt)*);
        _s
    }};
}

/// Same as `quote!`, but applies a given span to all tokens originating within
/// the macro invocation.
///
/// <br>
///
/// # Syntax
///
/// A span expression of type [`Span`], followed by `=>`, followed by the tokens
/// to quote. The span expression should be brief &mdash; use a variable for
/// anything more than a few characters. There should be no space before the
/// `=>` token.
///
/// [`Span`]: https://docs.rs/proc-macro2/1.0/proc_macro2/struct.Span.html
///
/// ```
/// # use proc_macro2::Span;
/// # use quote::quote_spanned;
/// #
/// # const IGNORE_TOKENS: &'static str = stringify! {
/// let span = /* ... */;
/// # };
/// # let span = Span::call_site();
/// # let init = 0;
///
/// // On one line, use parentheses.
/// let tokens = quote_spanned!(span=> Box::into_raw(Box::new(#init)));
///
/// // On multiple lines, place the span at the top and use braces.
/// let tokens = quote_spanned! {span=>
///     Box::into_raw(Box::new(#init))
/// };
/// ```
///
/// The lack of space before the `=>` should look jarring to Rust programmers
/// and this is intentional. The formatting is designed to be visibly
/// off-balance and draw the eye a particular way, due to the span expression
/// being evaluated in the context of the procedural macro and the remaining
/// tokens being evaluated in the generated code.
///
/// <br>
///
/// # Hygiene
///
/// Any interpolated tokens preserve the `Span` information provided by their
/// `ToTokens` implementation. Tokens that originate within the `quote_spanned!`
/// invocation are spanned with the given span argument.
///
/// <br>
///
/// # Example
///
/// The following procedural macro code uses `quote_spanned!` to assert that a
/// particular Rust type implements the [`Sync`] trait so that references can be
/// safely shared between threads.
///
/// [`Sync`]: https://doc.rust-lang.org/std/marker/trait.Sync.html
///
/// ```
/// # use quote::{quote_spanned, TokenStreamExt, ToTokens};
/// # use proc_macro2::{Span, TokenStream};
/// #
/// # struct Type;
/// #
/// # impl Type {
/// #     fn span(&self) -> Span {
/// #         Span::call_site()
/// #     }
/// # }
/// #
/// # impl ToTokens for Type {
/// #     fn to_tokens(&self, _tokens: &mut TokenStream) {}
/// # }
/// #
/// # let ty = Type;
/// # let call_site = Span::call_site();
/// #
/// let ty_span = ty.span();
/// let assert_sync = quote_spanned! {ty_span=>
///     struct _AssertSync where #ty: Sync;
/// };
/// ```
///
/// If the assertion fails, the user will see an error like the following. The
/// input span of their type is highlighted in the error.
///
/// ```text
/// error[E0277]: the trait bound `*const (): std::marker::Sync` is not satisfied
///   --> src/main.rs:10:21
///    |
/// 10 |     static ref PTR: *const () = &();
///    |                     ^^^^^^^^^ `*const ()` cannot be shared between threads safely
/// ```
///
/// In this example it is important for the where-clause to be spanned with the
/// line/column information of the user's input type so that error messages are
/// placed appropriately by the compiler.
#[macro_export]
macro_rules! quote_spanned {
    ($span:expr=>) => {{
        let _: $crate::__private::Span = $span;
        $crate::__private::TokenStream::new()
    }};
    ($span:expr=> $($tt:tt)*) => {{
        let mut _s = $crate::__private::TokenStream::new();
        let _span: $crate::__private::Span = $span;
        $crate::quote_each_token_spanned!(_s _span $($tt)*);
        _s
    }};
}

// Extract the names of all #metavariables and pass them to the $call macro.
//
// in:   pounded_var_names!(then!(...) a #b c #( #d )* #e)
// out:  then!(... b);
//       then!(... d);
//       then!(... e);
#[macro_export]
#[doc(hidden)]
macro_rules! pounded_var_names {
    ($call:ident! $extra:tt $($tts:tt)*) => {
        $crate::pounded_var_names_with_context!($call! $extra
            (@ $($tts)*)
            ($($tts)* @)
        )
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! pounded_var_names_with_context {
    ($call:ident! $extra:tt ($($b1:tt)*) ($($curr:tt)*)) => {
        $(
            $crate::pounded_var_with_context!($call! $extra $b1 $curr);
        )*
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! pounded_var_with_context {
    ($call:ident! $extra:tt $b1:tt ( $($inner:tt)* )) => {
        $crate::pounded_var_names!($call! $extra $($inner)*);
    };

    ($call:ident! $extra:tt $b1:tt [ $($inner:tt)* ]) => {
        $crate::pounded_var_names!($call! $extra $($inner)*);
    };

    ($call:ident! $extra:tt $b1:tt { $($inner:tt)* }) => {
        $crate::pounded_var_names!($call! $extra $($inner)*);
    };

    ($call:ident!($($extra:tt)*) # $var:ident) => {
        $crate::$call!($($extra)* $var);
    };

    ($call:ident! $extra:tt $b1:tt $curr:tt) => {};
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_bind_into_iter {
    ($has_iter:ident $var:ident) => {
        // `mut` may be unused if $var occurs multiple times in the list.
        #[allow(unused_mut)]
        let (mut $var, i) = $var.quote_into_iter();
        let $has_iter = $has_iter | i;
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_bind_next_or_break {
    ($var:ident) => {
        let $var = match $var.next() {
            Some(_x) => $crate::__private::RepInterp(_x),
            None => break,
        };
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_each_token {
    ($tokens:ident $($tts:tt)*) => {
        $crate::quote_tokens_with_context!($tokens
            (@ @ @ @ @ @ $($tts)*)
            (@ @ @ @ @ $($tts)* @)
            (@ @ @ @ $($tts)* @ @)
            (@ @ @ $(($tts))* @ @ @)
            (@ @ $($tts)* @ @ @ @)
            (@ $($tts)* @ @ @ @ @)
            ($($tts)* @ @ @ @ @ @)
        );
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_each_token_spanned {
    ($tokens:ident $span:ident $($tts:tt)*) => {
        $crate::quote_tokens_with_context_spanned!($tokens $span
            (@ @ @ @ @ @ $($tts)*)
            (@ @ @ @ @ $($tts)* @)
            (@ @ @ @ $($tts)* @ @)
            (@ @ @ $(($tts))* @ @ @)
            (@ @ $($tts)* @ @ @ @)
            (@ $($tts)* @ @ @ @ @)
            ($($tts)* @ @ @ @ @ @)
        );
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_tokens_with_context {
    ($tokens:ident
        ($($b3:tt)*) ($($b2:tt)*) ($($b1:tt)*)
        ($($curr:tt)*)
        ($($a1:tt)*) ($($a2:tt)*) ($($a3:tt)*)
    ) => {
        $(
            $crate::quote_token_with_context!($tokens $b3 $b2 $b1 $curr $a1 $a2 $a3);
        )*
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_tokens_with_context_spanned {
    ($tokens:ident $span:ident
        ($($b3:tt)*) ($($b2:tt)*) ($($b1:tt)*)
        ($($curr:tt)*)
        ($($a1:tt)*) ($($a2:tt)*) ($($a3:tt)*)
    ) => {
        $(
            $crate::quote_token_with_context_spanned!($tokens $span $b3 $b2 $b1 $curr $a1 $a2 $a3);
        )*
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_token_with_context {
    ($tokens:ident $b3:tt $b2:tt $b1:tt @ $a1:tt $a2:tt $a3:tt) => {};

    ($tokens:ident $b3:tt $b2:tt $b1:tt (#) ( $($inner:tt)* ) * $a3:tt) => {{
        use $crate::__private::ext::*;
        let has_iter = $crate::__private::ThereIsNoIteratorInRepetition;
        $crate::pounded_var_names!(quote_bind_into_iter!(has_iter) () $($inner)*);
        let _: $crate::__private::HasIterator = has_iter;
        // This is `while true` instead of `loop` because if there are no
        // iterators used inside of this repetition then the body would not
        // contain any `break`, so the compiler would emit unreachable code
        // warnings on anything below the loop. We use has_iter to detect and
        // fail to compile when there are no iterators, so here we just work
        // around the unneeded extra warning.
        while true {
            $crate::pounded_var_names!(quote_bind_next_or_break!() () $($inner)*);
            $crate::quote_each_token!($tokens $($inner)*);
        }
    }};
    ($tokens:ident $b3:tt $b2:tt # (( $($inner:tt)* )) * $a2:tt $a3:tt) => {};
    ($tokens:ident $b3:tt # ( $($inner:tt)* ) (*) $a1:tt $a2:tt $a3:tt) => {};

    ($tokens:ident $b3:tt $b2:tt $b1:tt (#) ( $($inner:tt)* ) $sep:tt *) => {{
        use $crate::__private::ext::*;
        let mut _i = 0usize;
        let has_iter = $crate::__private::ThereIsNoIteratorInRepetition;
        $crate::pounded_var_names!(quote_bind_into_iter!(has_iter) () $($inner)*);
        let _: $crate::__private::HasIterator = has_iter;
        while true {
            $crate::pounded_var_names!(quote_bind_next_or_break!() () $($inner)*);
            if _i > 0 {
                $crate::quote_token!($tokens $sep);
            }
            _i += 1;
            $crate::quote_each_token!($tokens $($inner)*);
        }
    }};
    ($tokens:ident $b3:tt $b2:tt # (( $($inner:tt)* )) $sep:tt * $a3:tt) => {};
    ($tokens:ident $b3:tt # ( $($inner:tt)* ) ($sep:tt) * $a2:tt $a3:tt) => {};
    ($tokens:ident # ( $($inner:tt)* ) * (*) $a1:tt $a2:tt $a3:tt) => {
        // https://github.com/dtolnay/quote/issues/130
        $crate::quote_token!($tokens *);
    };
    ($tokens:ident # ( $($inner:tt)* ) $sep:tt (*) $a1:tt $a2:tt $a3:tt) => {};

    ($tokens:ident $b3:tt $b2:tt $b1:tt (#) $var:ident $a2:tt $a3:tt) => {
        $crate::ToTokens::to_tokens(&$var, &mut $tokens);
    };
    ($tokens:ident $b3:tt $b2:tt # ($var:ident) $a1:tt $a2:tt $a3:tt) => {};
    ($tokens:ident $b3:tt $b2:tt $b1:tt ($curr:tt) $a1:tt $a2:tt $a3:tt) => {
        $crate::quote_token!($tokens $curr);
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_token_with_context_spanned {
    ($tokens:ident $span:ident $b3:tt $b2:tt $b1:tt @ $a1:tt $a2:tt $a3:tt) => {};

    ($tokens:ident $span:ident $b3:tt $b2:tt $b1:tt (#) ( $($inner:tt)* ) * $a3:tt) => {{
        use $crate::__private::ext::*;
        let has_iter = $crate::__private::ThereIsNoIteratorInRepetition;
        $crate::pounded_var_names!(quote_bind_into_iter!(has_iter) () $($inner)*);
        let _: $crate::__private::HasIterator = has_iter;
        // This is `while true` instead of `loop` because if there are no
        // iterators used inside of this repetition then the body would not
        // contain any `break`, so the compiler would emit unreachable code
        // warnings on anything below the loop. We use has_iter to detect and
        // fail to compile when there are no iterators, so here we just work
        // around the unneeded extra warning.
        while true {
            $crate::pounded_var_names!(quote_bind_next_or_break!() () $($inner)*);
            $crate::quote_each_token_spanned!($tokens $span $($inner)*);
        }
    }};
    ($tokens:ident $span:ident $b3:tt $b2:tt # (( $($inner:tt)* )) * $a2:tt $a3:tt) => {};
    ($tokens:ident $span:ident $b3:tt # ( $($inner:tt)* ) (*) $a1:tt $a2:tt $a3:tt) => {};

    ($tokens:ident $span:ident $b3:tt $b2:tt $b1:tt (#) ( $($inner:tt)* ) $sep:tt *) => {{
        use $crate::__private::ext::*;
        let mut _i = 0usize;
        let has_iter = $crate::__private::ThereIsNoIteratorInRepetition;
        $crate::pounded_var_names!(quote_bind_into_iter!(has_iter) () $($inner)*);
        let _: $crate::__private::HasIterator = has_iter;
        while true {
            $crate::pounded_var_names!(quote_bind_next_or_break!() () $($inner)*);
            if _i > 0 {
                $crate::quote_token_spanned!($tokens $span $sep);
            }
            _i += 1;
            $crate::quote_each_token_spanned!($tokens $span $($inner)*);
        }
    }};
    ($tokens:ident $span:ident $b3:tt $b2:tt # (( $($inner:tt)* )) $sep:tt * $a3:tt) => {};
    ($tokens:ident $span:ident $b3:tt # ( $($inner:tt)* ) ($sep:tt) * $a2:tt $a3:tt) => {};
    ($tokens:ident $span:ident # ( $($inner:tt)* ) * (*) $a1:tt $a2:tt $a3:tt) => {
        // https://github.com/dtolnay/quote/issues/130
        $crate::quote_token_spanned!($tokens $span *);
    };
    ($tokens:ident $span:ident # ( $($inner:tt)* ) $sep:tt (*) $a1:tt $a2:tt $a3:tt) => {};

    ($tokens:ident $span:ident $b3:tt $b2:tt $b1:tt (#) $var:ident $a2:tt $a3:tt) => {
        $crate::ToTokens::to_tokens(&$var, &mut $tokens);
    };
    ($tokens:ident $span:ident $b3:tt $b2:tt # ($var:ident) $a1:tt $a2:tt $a3:tt) => {};
    ($tokens:ident $span:ident $b3:tt $b2:tt $b1:tt ($curr:tt) $a1:tt $a2:tt $a3:tt) => {
        $crate::quote_token_spanned!($tokens $span $curr);
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_token {
    ($tokens:ident ( $($inner:tt)* )) => {
        $crate::__private::push_group(
            &mut $tokens,
            $crate::__private::Delimiter::Parenthesis,
            $crate::quote!($($inner)*),
        );
    };

    ($tokens:ident [ $($inner:tt)* ]) => {
        $crate::__private::push_group(
            &mut $tokens,
            $crate::__private::Delimiter::Bracket,
            $crate::quote!($($inner)*),
        );
    };

    ($tokens:ident { $($inner:tt)* }) => {
        $crate::__private::push_group(
            &mut $tokens,
            $crate::__private::Delimiter::Brace,
            $crate::quote!($($inner)*),
        );
    };

    ($tokens:ident +) => {
        $crate::__private::push_add(&mut $tokens);
    };

    ($tokens:ident +=) => {
        $crate::__private::push_add_eq(&mut $tokens);
    };

    ($tokens:ident &) => {
        $crate::__private::push_and(&mut $tokens);
    };

    ($tokens:ident &&) => {
        $crate::__private::push_and_and(&mut $tokens);
    };

    ($tokens:ident &=) => {
        $crate::__private::push_and_eq(&mut $tokens);
    };

    ($tokens:ident @) => {
        $crate::__private::push_at(&mut $tokens);
    };

    ($tokens:ident !) => {
        $crate::__private::push_bang(&mut $tokens);
    };

    ($tokens:ident ^) => {
        $crate::__private::push_caret(&mut $tokens);
    };

    ($tokens:ident ^=) => {
        $crate::__private::push_caret_eq(&mut $tokens);
    };

    ($tokens:ident :) => {
        $crate::__private::push_colon(&mut $tokens);
    };

    ($tokens:ident ::) => {
        $crate::__private::push_colon2(&mut $tokens);
    };

    ($tokens:ident ,) => {
        $crate::__private::push_comma(&mut $tokens);
    };

    ($tokens:ident /) => {
        $crate::__private::push_div(&mut $tokens);
    };

    ($tokens:ident /=) => {
        $crate::__private::push_div_eq(&mut $tokens);
    };

    ($tokens:ident .) => {
        $crate::__private::push_dot(&mut $tokens);
    };

    ($tokens:ident ..) => {
        $crate::__private::push_dot2(&mut $tokens);
    };

    ($tokens:ident ...) => {
        $crate::__private::push_dot3(&mut $tokens);
    };

    ($tokens:ident ..=) => {
        $crate::__private::push_dot_dot_eq(&mut $tokens);
    };

    ($tokens:ident =) => {
        $crate::__private::push_eq(&mut $tokens);
    };

    ($tokens:ident ==) => {
        $crate::__private::push_eq_eq(&mut $tokens);
    };

    ($tokens:ident >=) => {
        $crate::__private::push_ge(&mut $tokens);
    };

    ($tokens:ident >) => {
        $crate::__private::push_gt(&mut $tokens);
    };

    ($tokens:ident <=) => {
        $crate::__private::push_le(&mut $tokens);
    };

    ($tokens:ident <) => {
        $crate::__private::push_lt(&mut $tokens);
    };

    ($tokens:ident *=) => {
        $crate::__private::push_mul_eq(&mut $tokens);
    };

    ($tokens:ident !=) => {
        $crate::__private::push_ne(&mut $tokens);
    };

    ($tokens:ident |) => {
        $crate::__private::push_or(&mut $tokens);
    };

    ($tokens:ident |=) => {
        $crate::__private::push_or_eq(&mut $tokens);
    };

    ($tokens:ident ||) => {
        $crate::__private::push_or_or(&mut $tokens);
    };

    ($tokens:ident #) => {
        $crate::__private::push_pound(&mut $tokens);
    };

    ($tokens:ident ?) => {
        $crate::__private::push_question(&mut $tokens);
    };

    ($tokens:ident ->) => {
        $crate::__private::push_rarrow(&mut $tokens);
    };

    ($tokens:ident <-) => {
        $crate::__private::push_larrow(&mut $tokens);
    };

    ($tokens:ident %) => {
        $crate::__private::push_rem(&mut $tokens);
    };

    ($tokens:ident %=) => {
        $crate::__private::push_rem_eq(&mut $tokens);
    };

    ($tokens:ident =>) => {
        $crate::__private::push_fat_arrow(&mut $tokens);
    };

    ($tokens:ident ;) => {
        $crate::__private::push_semi(&mut $tokens);
    };

    ($tokens:ident <<) => {
        $crate::__private::push_shl(&mut $tokens);
    };

    ($tokens:ident <<=) => {
        $crate::__private::push_shl_eq(&mut $tokens);
    };

    ($tokens:ident >>) => {
        $crate::__private::push_shr(&mut $tokens);
    };

    ($tokens:ident >>=) => {
        $crate::__private::push_shr_eq(&mut $tokens);
    };

    ($tokens:ident *) => {
        $crate::__private::push_star(&mut $tokens);
    };

    ($tokens:ident -) => {
        $crate::__private::push_sub(&mut $tokens);
    };

    ($tokens:ident -=) => {
        $crate::__private::push_sub_eq(&mut $tokens);
    };

    ($tokens:ident $ident:ident) => {
        $crate::__private::push_ident(&mut $tokens, stringify!($ident));
    };

    ($tokens:ident $other:tt) => {
        $crate::__private::parse(&mut $tokens, stringify!($other));
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_token_spanned {
    ($tokens:ident $span:ident ( $($inner:tt)* )) => {
        $crate::__private::push_group_spanned(
            &mut $tokens,
            $span,
            $crate::__private::Delimiter::Parenthesis,
            $crate::quote_spanned!($span=> $($inner)*),
        );
    };

    ($tokens:ident $span:ident [ $($inner:tt)* ]) => {
        $crate::__private::push_group_spanned(
            &mut $tokens,
            $span,
            $crate::__private::Delimiter::Bracket,
            $crate::quote_spanned!($span=> $($inner)*),
        );
    };

    ($tokens:ident $span:ident { $($inner:tt)* }) => {
        $crate::__private::push_group_spanned(
            &mut $tokens,
            $span,
            $crate::__private::Delimiter::Brace,
            $crate::quote_spanned!($span=> $($inner)*),
        );
    };

    ($tokens:ident $span:ident +) => {
        $crate::__private::push_add_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident +=) => {
        $crate::__private::push_add_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident &) => {
        $crate::__private::push_and_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident &&) => {
        $crate::__private::push_and_and_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident &=) => {
        $crate::__private::push_and_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident @) => {
        $crate::__private::push_at_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident !) => {
        $crate::__private::push_bang_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ^) => {
        $crate::__private::push_caret_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ^=) => {
        $crate::__private::push_caret_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident :) => {
        $crate::__private::push_colon_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ::) => {
        $crate::__private::push_colon2_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ,) => {
        $crate::__private::push_comma_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident /) => {
        $crate::__private::push_div_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident /=) => {
        $crate::__private::push_div_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident .) => {
        $crate::__private::push_dot_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ..) => {
        $crate::__private::push_dot2_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ...) => {
        $crate::__private::push_dot3_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ..=) => {
        $crate::__private::push_dot_dot_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident =) => {
        $crate::__private::push_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ==) => {
        $crate::__private::push_eq_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident >=) => {
        $crate::__private::push_ge_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident >) => {
        $crate::__private::push_gt_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident <=) => {
        $crate::__private::push_le_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident <) => {
        $crate::__private::push_lt_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident *=) => {
        $crate::__private::push_mul_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident !=) => {
        $crate::__private::push_ne_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident |) => {
        $crate::__private::push_or_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident |=) => {
        $crate::__private::push_or_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ||) => {
        $crate::__private::push_or_or_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident #) => {
        $crate::__private::push_pound_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ?) => {
        $crate::__private::push_question_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ->) => {
        $crate::__private::push_rarrow_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident <-) => {
        $crate::__private::push_larrow_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident %) => {
        $crate::__private::push_rem_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident %=) => {
        $crate::__private::push_rem_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident =>) => {
        $crate::__private::push_fat_arrow_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident ;) => {
        $crate::__private::push_semi_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident <<) => {
        $crate::__private::push_shl_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident <<=) => {
        $crate::__private::push_shl_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident >>) => {
        $crate::__private::push_shr_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident >>=) => {
        $crate::__private::push_shr_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident *) => {
        $crate::__private::push_star_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident -) => {
        $crate::__private::push_sub_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident -=) => {
        $crate::__private::push_sub_eq_spanned(&mut $tokens, $span);
    };

    ($tokens:ident $span:ident $ident:ident) => {
        $crate::__private::push_ident_spanned(&mut $tokens, $span, stringify!($ident));
    };

    ($tokens:ident $span:ident $other:tt) => {
        $crate::__private::parse_spanned(&mut $tokens, $span, stringify!($other));
    };
}
