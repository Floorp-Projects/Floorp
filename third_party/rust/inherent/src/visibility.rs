use proc_macro2::TokenStream;
use quote::ToTokens;
use syn::ext::IdentExt;
use syn::parse::{Parse, ParseStream, Result};
use syn::{Ident, Path, Token, VisPublic, VisRestricted};

pub struct Visibility {
    variant: syn::Visibility,
}

impl Parse for Visibility {
    fn parse(input: ParseStream) -> Result<Self> {
        let lookahead = input.lookahead1();

        let variant = if input.is_empty() {
            syn::Visibility::Inherited
        } else if lookahead.peek(Token![pub]) {
            syn::Visibility::Public(VisPublic {
                pub_token: input.parse()?,
            })
        } else if lookahead.peek(Token![crate])
            || lookahead.peek(Token![self])
            || lookahead.peek(Token![super])
        {
            syn::Visibility::Restricted(VisRestricted {
                pub_token: Default::default(),
                paren_token: Default::default(),
                in_token: None,
                path: Box::new(Path::from(input.call(Ident::parse_any)?)),
            })
        } else if lookahead.peek(Token![in]) {
            syn::Visibility::Restricted(VisRestricted {
                pub_token: Default::default(),
                paren_token: Default::default(),
                in_token: Some(input.parse()?),
                path: Box::new(input.call(Path::parse_mod_style)?),
            })
        } else {
            return Err(lookahead.error());
        };

        Ok(Visibility { variant })
    }
}

impl ToTokens for Visibility {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        self.variant.to_tokens(tokens);
    }
}
