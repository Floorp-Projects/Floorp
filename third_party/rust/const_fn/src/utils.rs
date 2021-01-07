use proc_macro::{Delimiter, Group, Ident, Punct, Spacing, Span, TokenStream, TokenTree};
use std::iter::FromIterator;

use crate::Result;

macro_rules! error {
    ($span:expr, $msg:expr) => {{
        crate::error::Error::new($span, $msg)
    }};
    ($span:expr, $($tt:tt)*) => {
        error!($span, format!($($tt)*))
    };
}

pub(crate) fn tt_span(tt: Option<&TokenTree>) -> Span {
    tt.map_or_else(Span::call_site, TokenTree::span)
}

pub(crate) fn parse_as_empty(mut tokens: impl Iterator<Item = TokenTree>) -> Result<()> {
    match tokens.next() {
        Some(tt) => Err(error!(tt.span(), "unexpected token: {}", tt)),
        None => Ok(()),
    }
}

// (`#[cfg(<tokens>)]`, `#[cfg(not(<tokens>))]`)
pub(crate) fn cfg_attrs(tokens: TokenStream) -> (TokenStream, TokenStream) {
    let f = |tokens| {
        let tokens = TokenStream::from_iter(vec![
            TokenTree::Ident(Ident::new("cfg", Span::call_site())),
            TokenTree::Group(Group::new(Delimiter::Parenthesis, tokens)),
        ]);
        TokenStream::from_iter(vec![
            TokenTree::Punct(Punct::new('#', Spacing::Alone)),
            TokenTree::Group(Group::new(Delimiter::Bracket, tokens)),
        ])
    };

    let cfg_not = TokenTree::Group(Group::new(Delimiter::Parenthesis, tokens.clone()));
    let cfg_not = TokenStream::from_iter(vec![
        TokenTree::Ident(Ident::new("not", Span::call_site())),
        cfg_not,
    ]);

    (f(tokens), f(cfg_not))
}
