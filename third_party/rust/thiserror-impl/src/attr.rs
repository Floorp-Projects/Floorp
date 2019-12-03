use proc_macro2::{Delimiter, Group, TokenStream, TokenTree};
use quote::{format_ident, quote, ToTokens};
use std::iter::once;
use syn::parse::{Nothing, ParseStream};
use syn::{
    braced, bracketed, parenthesized, token, Attribute, Error, Ident, Index, LitInt, LitStr,
    Result, Token,
};

pub struct Attrs<'a> {
    pub display: Option<Display<'a>>,
    pub source: Option<&'a Attribute>,
    pub backtrace: Option<&'a Attribute>,
    pub from: Option<&'a Attribute>,
}

#[derive(Clone)]
pub struct Display<'a> {
    pub original: &'a Attribute,
    pub fmt: LitStr,
    pub args: TokenStream,
    pub was_shorthand: bool,
    pub has_bonus_display: bool,
}

pub fn get(input: &[Attribute]) -> Result<Attrs> {
    let mut attrs = Attrs {
        display: None,
        source: None,
        backtrace: None,
        from: None,
    };

    for attr in input {
        if attr.path.is_ident("error") {
            let display = parse_display(attr)?;
            if attrs.display.is_some() {
                return Err(Error::new_spanned(
                    attr,
                    "only one #[error(...)] attribute is allowed",
                ));
            }
            attrs.display = Some(display);
        } else if attr.path.is_ident("source") {
            require_empty_attribute(attr)?;
            if attrs.source.is_some() {
                return Err(Error::new_spanned(attr, "duplicate #[source] attribute"));
            }
            attrs.source = Some(attr);
        } else if attr.path.is_ident("backtrace") {
            require_empty_attribute(attr)?;
            if attrs.backtrace.is_some() {
                return Err(Error::new_spanned(attr, "duplicate #[backtrace] attribute"));
            }
            attrs.backtrace = Some(attr);
        } else if attr.path.is_ident("from") {
            if !attr.tokens.is_empty() {
                // Assume this is meant for derive_more crate or something.
                continue;
            }
            if attrs.from.is_some() {
                return Err(Error::new_spanned(attr, "duplicate #[from] attribute"));
            }
            attrs.from = Some(attr);
        }
    }

    Ok(attrs)
}

fn parse_display(attr: &Attribute) -> Result<Display> {
    attr.parse_args_with(|input: ParseStream| {
        let mut display = Display {
            original: attr,
            fmt: input.parse()?,
            args: parse_token_expr(input, false)?,
            was_shorthand: false,
            has_bonus_display: false,
        };
        display.expand_shorthand();
        Ok(display)
    })
}

fn parse_token_expr(input: ParseStream, mut last_is_comma: bool) -> Result<TokenStream> {
    let mut tokens = TokenStream::new();
    while !input.is_empty() {
        if last_is_comma && input.peek(Token![.]) {
            if input.peek2(Ident) {
                input.parse::<Token![.]>()?;
                last_is_comma = false;
                continue;
            }
            if input.peek2(LitInt) {
                input.parse::<Token![.]>()?;
                let int: Index = input.parse()?;
                let ident = format_ident!("_{}", int.index, span = int.span);
                tokens.extend(once(TokenTree::Ident(ident)));
                last_is_comma = false;
                continue;
            }
        }
        last_is_comma = input.peek(Token![,]);
        let token: TokenTree = if input.peek(token::Paren) {
            let content;
            let delimiter = parenthesized!(content in input);
            let nested = parse_token_expr(&content, true)?;
            let mut group = Group::new(Delimiter::Parenthesis, nested);
            group.set_span(delimiter.span);
            TokenTree::Group(group)
        } else if input.peek(token::Brace) {
            let content;
            let delimiter = braced!(content in input);
            let nested = parse_token_expr(&content, true)?;
            let mut group = Group::new(Delimiter::Brace, nested);
            group.set_span(delimiter.span);
            TokenTree::Group(group)
        } else if input.peek(token::Bracket) {
            let content;
            let delimiter = bracketed!(content in input);
            let nested = parse_token_expr(&content, true)?;
            let mut group = Group::new(Delimiter::Bracket, nested);
            group.set_span(delimiter.span);
            TokenTree::Group(group)
        } else {
            input.parse()?
        };
        tokens.extend(once(token));
    }
    Ok(tokens)
}

fn require_empty_attribute(attr: &Attribute) -> Result<()> {
    syn::parse2::<Nothing>(attr.tokens.clone())?;
    Ok(())
}

impl ToTokens for Display<'_> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let fmt = &self.fmt;
        let args = &self.args;
        if self.was_shorthand && fmt.value() == "{}" {
            let arg = args.clone().into_iter().nth(1).unwrap();
            tokens.extend(quote! {
                std::fmt::Display::fmt(#arg, __formatter)
            });
        } else if self.was_shorthand && fmt.value() == "{:?}" {
            let arg = args.clone().into_iter().nth(1).unwrap();
            tokens.extend(quote! {
                std::fmt::Debug::fmt(#arg, __formatter)
            });
        } else {
            tokens.extend(quote! {
                write!(__formatter, #fmt #args)
            });
        }
    }
}
