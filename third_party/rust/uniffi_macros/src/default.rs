/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::util::kw;
use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::{
    bracketed, parenthesized,
    parse::{Nothing, Parse, ParseStream},
    token::{Bracket, Paren},
    Lit,
};

/// Default value
#[derive(Clone)]
pub enum DefaultValue {
    Literal(Lit),
    None(kw::None),
    Some {
        some: kw::Some,
        paren: Paren,
        inner: Box<DefaultValue>,
    },
    EmptySeq(Bracket),
}

impl ToTokens for DefaultValue {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match self {
            DefaultValue::Literal(lit) => lit.to_tokens(tokens),
            DefaultValue::None(kw) => kw.to_tokens(tokens),
            DefaultValue::Some { inner, .. } => tokens.extend(quote! { Some(#inner) }),
            DefaultValue::EmptySeq(_) => tokens.extend(quote! { [] }),
        }
    }
}

impl Parse for DefaultValue {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(kw::None) {
            let none_kw: kw::None = input.parse()?;
            Ok(Self::None(none_kw))
        } else if lookahead.peek(kw::Some) {
            let some: kw::Some = input.parse()?;
            let content;
            let paren = parenthesized!(content in input);
            Ok(Self::Some {
                some,
                paren,
                inner: content.parse()?,
            })
        } else if lookahead.peek(Bracket) {
            let content;
            let bracket = bracketed!(content in input);
            content.parse::<Nothing>()?;
            Ok(Self::EmptySeq(bracket))
        } else {
            Ok(Self::Literal(input.parse()?))
        }
    }
}

impl DefaultValue {
    fn metadata_calls(&self) -> syn::Result<TokenStream> {
        match self {
            DefaultValue::Literal(Lit::Int(i)) if !i.suffix().is_empty() => Err(
                syn::Error::new_spanned(i, "integer literals with suffix not supported here"),
            ),
            DefaultValue::Literal(Lit::Float(f)) if !f.suffix().is_empty() => Err(
                syn::Error::new_spanned(f, "float literals with suffix not supported here"),
            ),

            DefaultValue::Literal(Lit::Str(s)) => Ok(quote! {
                .concat_value(::uniffi::metadata::codes::LIT_STR)
                .concat_str(#s)
            }),
            DefaultValue::Literal(Lit::Int(i)) => {
                let digits = i.base10_digits();
                Ok(quote! {
                    .concat_value(::uniffi::metadata::codes::LIT_INT)
                    .concat_str(#digits)
                })
            }
            DefaultValue::Literal(Lit::Float(f)) => {
                let digits = f.base10_digits();
                Ok(quote! {
                    .concat_value(::uniffi::metadata::codes::LIT_FLOAT)
                    .concat_str(#digits)
                })
            }
            DefaultValue::Literal(Lit::Bool(b)) => Ok(quote! {
                .concat_value(::uniffi::metadata::codes::LIT_BOOL)
                .concat_bool(#b)
            }),

            DefaultValue::Literal(_) => Err(syn::Error::new_spanned(
                self,
                "this type of literal is not currently supported as a default",
            )),

            DefaultValue::EmptySeq(_) => Ok(quote! {
                .concat_value(::uniffi::metadata::codes::LIT_EMPTY_SEQ)
            }),

            DefaultValue::None(_) => Ok(quote! {
                .concat_value(::uniffi::metadata::codes::LIT_NONE)
            }),

            DefaultValue::Some { inner, .. } => {
                let inner_calls = inner.metadata_calls()?;
                Ok(quote! {
                    .concat_value(::uniffi::metadata::codes::LIT_SOME)
                    #inner_calls
                })
            }
        }
    }
}

pub fn default_value_metadata_calls(default: &Option<DefaultValue>) -> syn::Result<TokenStream> {
    Ok(match default {
        Some(default) => {
            let metadata_calls = default.metadata_calls()?;
            quote! {
                .concat_bool(true)
                #metadata_calls
            }
        }
        None => quote! { .concat_bool(false) },
    })
}
