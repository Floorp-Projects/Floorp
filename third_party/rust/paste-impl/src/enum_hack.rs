use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

use proc_macro2::{Ident, Span, TokenStream, TokenTree};
use quote::quote;
use syn::parse::{Parse, ParseStream, Result};
use syn::{braced, parenthesized, parse_macro_input, Token};

pub fn wrap(output: TokenStream) -> TokenStream {
    let mut hasher = DefaultHasher::default();
    output.to_string().hash(&mut hasher);
    let mangled_name = format!("_paste_{}", hasher.finish());
    let ident = Ident::new(&mangled_name, Span::call_site());

    quote! {
        #[derive(paste::EnumHack)]
        enum #ident {
            Value = (stringify! {
                #output
            }, 0).1,
        }
    }
}

struct EnumHack {
    token_stream: TokenStream,
}

impl Parse for EnumHack {
    fn parse(input: ParseStream) -> Result<Self> {
        input.parse::<Token![enum]>()?;
        input.parse::<Ident>()?;

        let braces;
        braced!(braces in input);
        braces.parse::<Ident>()?;
        braces.parse::<Token![=]>()?;

        let parens;
        parenthesized!(parens in braces);
        parens.parse::<Ident>()?;
        parens.parse::<Token![!]>()?;

        let inner;
        braced!(inner in parens);
        let token_stream: TokenStream = inner.parse()?;

        parens.parse::<Token![,]>()?;
        parens.parse::<TokenTree>()?;
        braces.parse::<Token![.]>()?;
        braces.parse::<TokenTree>()?;
        braces.parse::<Token![,]>()?;

        Ok(EnumHack { token_stream })
    }
}

pub fn extract(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let inner = parse_macro_input!(input as EnumHack);
    proc_macro::TokenStream::from(inner.token_stream)
}
