#![deny(
    anonymous_parameters,
    clippy::all,
    const_err,
    illegal_floating_point_literal_pattern,
    late_bound_lifetime_arguments,
    path_statements,
    patterns_in_fns_without_body,
    rust_2018_idioms,
    trivial_casts,
    trivial_numeric_casts,
    unreachable_pub,
    unsafe_code,
    unused_extern_crates
)]
#![warn(
    clippy::dbg_macro,
    clippy::decimal_literal_representation,
    clippy::get_unwrap,
    clippy::nursery,
    clippy::print_stdout,
    clippy::todo,
    clippy::unimplemented,
    clippy::unnested_or_patterns,
    clippy::unwrap_used,
    clippy::use_debug,
    single_use_lifetimes,
    unused_qualifications,
    variant_size_differences
)]
#![allow(clippy::missing_const_for_fn, clippy::redundant_pub_crate)]

#[macro_use]
mod quote;

mod date;
mod datetime;
mod error;
mod format_description;
mod helpers;
mod offset;
mod serde_format_description;
mod time;
mod to_tokens;

use proc_macro::{TokenStream, TokenTree};

use self::error::Error;

macro_rules! impl_macros {
    ($($name:ident)*) => {$(
        #[proc_macro]
        pub fn $name(input: TokenStream) -> TokenStream {
            use crate::to_tokens::ToTokenTree;

            let mut iter = input.into_iter().peekable();
            match $name::parse(&mut iter) {
                Ok(value) => match iter.peek() {
                    Some(tree) => Error::UnexpectedToken { tree: tree.clone() }.to_compile_error(),
                    None => TokenStream::from(value.into_token_tree()),
                },
                Err(err) => err.to_compile_error(),
            }
        }
    )*};
}

impl_macros![date datetime offset time];

// TODO Gate this behind the the `formatting` or `parsing` feature flag when weak dependency
// features land.
#[proc_macro]
pub fn format_description(input: TokenStream) -> TokenStream {
    (|| {
        let (span, string) = helpers::get_string_literal(input)?;
        let items = format_description::parse(&string, span)?;

        Ok(quote! {{
            const DESCRIPTION: &[::time::format_description::FormatItem<'_>] = &[#S(
                items
                    .into_iter()
                    .map(|item| quote! { #S(item), })
                    .collect::<TokenStream>()
            )];
            DESCRIPTION
        }})
    })()
    .unwrap_or_else(|err: Error| err.to_compile_error())
}

#[proc_macro]
pub fn serde_format_description(input: TokenStream) -> TokenStream {
    (|| {
        let mut tokens = input.into_iter().peekable();
        // First, an identifier (the desired module name)
        let mod_name = match tokens.next() {
            Some(TokenTree::Ident(ident)) => Ok(ident),
            Some(tree) => Err(Error::UnexpectedToken { tree }),
            None => Err(Error::UnexpectedEndOfInput),
        }?;

        // Followed by a comma
        helpers::consume_punct(',', &mut tokens)?;

        // Then, the type to create serde serializers for (e.g., `OffsetDateTime`).
        let formattable = match tokens.next() {
            Some(tree @ TokenTree::Ident(_)) => Ok(tree),
            Some(tree) => Err(Error::UnexpectedToken { tree }),
            None => Err(Error::UnexpectedEndOfInput),
        }?;

        // Another comma
        helpers::consume_punct(',', &mut tokens)?;

        // Then, a string literal.
        let (span, format_string) = helpers::get_string_literal(tokens.collect())?;

        let items = format_description::parse(&format_string, span)?;
        let items: TokenStream = items.into_iter().map(|item| quote! { #S(item), }).collect();

        Ok(serde_format_description::build(
            mod_name,
            items,
            formattable,
            &String::from_utf8_lossy(&format_string),
        ))
    })()
    .unwrap_or_else(|err: Error| err.to_compile_error_standalone())
}
