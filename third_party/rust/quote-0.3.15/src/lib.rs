//! Quasi-quoting without a Syntex dependency, intended for use with [Macros
//! 1.1](https://github.com/rust-lang/rfcs/blob/master/text/1681-macros-1.1.md).
//!
//! ```toml
//! [dependencies]
//! quote = "0.3"
//! ```
//!
//! ```rust,ignore
//! #[macro_use]
//! extern crate quote;
//! ```
//!
//! Interpolation is done with `#var`:
//!
//! ```text
//! let tokens = quote! {
//!     struct SerializeWith #generics #where_clause {
//!         value: &'a #field_ty,
//!         phantom: ::std::marker::PhantomData<#item_ty>,
//!     }
//!
//!     impl #generics serde::Serialize for SerializeWith #generics #where_clause {
//!         fn serialize<S>(&self, s: &mut S) -> Result<(), S::Error>
//!             where S: serde::Serializer
//!         {
//!             #path(self.value, s)
//!         }
//!     }
//!
//!     SerializeWith {
//!         value: #value,
//!         phantom: ::std::marker::PhantomData::<#item_ty>,
//!     }
//! };
//! ```
//!
//! Repetition is done using `#(...)*` or `#(...),*` very similar to `macro_rules!`:
//!
//! - `#(#var)*` - no separators
//! - `#(#var),*` - the character before the asterisk is used as a separator
//! - `#( struct #var; )*` - the repetition can contain other things
//! - `#( #k => println!("{}", #v), )*` - even multiple interpolations
//!
//! The return type of `quote!` is `quote::Tokens`. Tokens can be interpolated into
//! other quotes:
//!
//! ```text
//! let t = quote! { /* ... */ };
//! return quote! { /* ... */ #t /* ... */ };
//! ```
//!
//! Call `to_string()` or `as_str()` on a Tokens to get a `String` or `&str` of Rust
//! code.
//!
//! The `quote!` macro relies on deep recursion so some large invocations may fail
//! with "recursion limit reached" when you compile. If it fails, bump up the
//! recursion limit by adding `#![recursion_limit = "128"]` to your crate. An even
//! higher limit may be necessary for especially large invocations.

mod tokens;
pub use tokens::Tokens;

mod to_tokens;
pub use to_tokens::{ToTokens, ByteStr, Hex};

mod ident;
pub use ident::Ident;

/// The whole point.
#[macro_export]
macro_rules! quote {
    () => {
        $crate::Tokens::new()
    };

    ($($tt:tt)+) => {
        {
            let mut _s = $crate::Tokens::new();
            quote_each_token!(_s $($tt)*);
            _s
        }
    };
}

// Extract the names of all #metavariables and pass them to the $finish macro.
//
// in:   pounded_var_names!(then () a #b c #( #d )* #e)
// out:  then!(() b d e)
#[macro_export]
#[doc(hidden)]
macro_rules! pounded_var_names {
    ($finish:ident ($($found:ident)*) # ( $($inner:tt)* ) $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) # [ $($inner:tt)* ] $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) # { $($inner:tt)* } $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) # $first:ident $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)* $first) $($rest)*)
    };

    ($finish:ident ($($found:ident)*) ( $($inner:tt)* ) $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) [ $($inner:tt)* ] $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) { $($inner:tt)* } $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) $ignore:tt $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($rest)*)
    };

    ($finish:ident ($($found:ident)*)) => {
        $finish!(() $($found)*)
    };
}

// in:   nested_tuples_pat!(() a b c d e)
// out:  ((((a b) c) d) e)
//
// in:   nested_tuples_pat!(() a)
// out:  a
#[macro_export]
#[doc(hidden)]
macro_rules! nested_tuples_pat {
    (()) => {
        &()
    };

    (() $first:ident $($rest:ident)*) => {
        nested_tuples_pat!(($first) $($rest)*)
    };

    (($pat:pat) $first:ident $($rest:ident)*) => {
        nested_tuples_pat!((($pat, $first)) $($rest)*)
    };

    (($done:pat)) => {
        $done
    };
}

// in:   multi_zip_expr!(() a b c d e)
// out:  a.into_iter().zip(b).zip(c).zip(d).zip(e)
//
// in:   multi_zip_iter!(() a)
// out:  a
#[macro_export]
#[doc(hidden)]
macro_rules! multi_zip_expr {
    (()) => {
        &[]
    };

    (() $single:ident) => {
        $single
    };

    (() $first:ident $($rest:ident)*) => {
        multi_zip_expr!(($first.into_iter()) $($rest)*)
    };

    (($zips:expr) $first:ident $($rest:ident)*) => {
        multi_zip_expr!(($zips.zip($first)) $($rest)*)
    };

    (($done:expr)) => {
        $done
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_each_token {
    ($tokens:ident) => {};

    ($tokens:ident # ! $($rest:tt)*) => {
        $tokens.append("#");
        $tokens.append("!");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # ( $($inner:tt)* ) * $($rest:tt)*) => {
        for pounded_var_names!(nested_tuples_pat () $($inner)*)
        in pounded_var_names!(multi_zip_expr () $($inner)*) {
            quote_each_token!($tokens $($inner)*);
        }
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # ( $($inner:tt)* ) $sep:tt * $($rest:tt)*) => {
        for (_i, pounded_var_names!(nested_tuples_pat () $($inner)*))
        in pounded_var_names!(multi_zip_expr () $($inner)*).into_iter().enumerate() {
            if _i > 0 {
                $tokens.append(stringify!($sep));
            }
            quote_each_token!($tokens $($inner)*);
        }
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # [ $($inner:tt)* ] $($rest:tt)*) => {
        $tokens.append("#");
        $tokens.append("[");
        quote_each_token!($tokens $($inner)*);
        $tokens.append("]");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # $first:ident $($rest:tt)*) => {
        $crate::ToTokens::to_tokens(&$first, &mut $tokens);
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident ( $($first:tt)* ) $($rest:tt)*) => {
        $tokens.append("(");
        quote_each_token!($tokens $($first)*);
        $tokens.append(")");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident [ $($first:tt)* ] $($rest:tt)*) => {
        $tokens.append("[");
        quote_each_token!($tokens $($first)*);
        $tokens.append("]");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident { $($first:tt)* } $($rest:tt)*) => {
        $tokens.append("{");
        quote_each_token!($tokens $($first)*);
        $tokens.append("}");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident $first:tt $($rest:tt)*) => {
        $tokens.append(stringify!($first));
        quote_each_token!($tokens $($rest)*);
    };
}
