/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#![deny(warnings)]

//! A procedural macro as a syntactical sugar to `gecko_profiler_label!` macro.
//! You can use this macro on top of functions to automatically append the
//! label frame to the function.
//!
//! Example usage:
//! ```rust
//! #[gecko_profiler_fn_label(DOM)]
//! fn foo(bar: u32) -> u32 {
//!     bar
//! }
//!
//! #[gecko_profiler_fn_label(Javascript, IonMonkey)]
//! pub fn bar(baz: i8) -> i8 {
//!     baz
//! }
//! ```
//!
//! See the documentation of `gecko_profiler_label!` macro to learn more about
//! its parameters.

extern crate proc_macro;

use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, ItemFn};

#[proc_macro_attribute]
pub fn gecko_profiler_fn_label(attrs: TokenStream, input: TokenStream) -> TokenStream {
    let mut attr_args = Vec::new();
    let attr_parser = syn::meta::parser(|meta| {
        attr_args.push(meta.path);
        Ok(())
    });
    parse_macro_input!(attrs with attr_parser);
    let input = parse_macro_input!(input as ItemFn);

    if attr_args.is_empty() || attr_args.len() > 2 {
        panic!("Expected one or two arguments as ProfilingCategory or ProfilingCategoryPair but {} arguments provided!", attr_args.len());
    }

    let category_name = &attr_args[0];
    // Try to get the subcategory if possible. Otherwise, use `None`.
    let subcategory_if_provided = match attr_args.get(1) {
        Some(subcategory) => quote!(, #subcategory),
        None => quote!(),
    };

    let ItemFn {
        attrs,
        vis,
        sig,
        block,
    } = input;
    let stmts = &block.stmts;

    let new_fn = quote! {
        #(#attrs)* #vis #sig {
          gecko_profiler_label!(#category_name#subcategory_if_provided);
          #(#stmts)*
        }
    };

    new_fn.into()
}
