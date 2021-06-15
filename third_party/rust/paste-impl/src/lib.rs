extern crate proc_macro;

mod enum_hack;
mod error;

use crate::error::{Error, Result};
use proc_macro::{
    token_stream, Delimiter, Group, Ident, Punct, Spacing, Span, TokenStream, TokenTree,
};
use proc_macro_hack::proc_macro_hack;
use std::iter::{self, FromIterator, Peekable};
use std::panic;

#[proc_macro]
pub fn item(input: TokenStream) -> TokenStream {
    expand_paste(input)
}

#[proc_macro]
pub fn item_with_macros(input: TokenStream) -> TokenStream {
    enum_hack::wrap(expand_paste(input))
}

#[proc_macro_hack]
pub fn expr(input: TokenStream) -> TokenStream {
    TokenStream::from(TokenTree::Group(Group::new(
        Delimiter::Brace,
        expand_paste(input),
    )))
}

#[doc(hidden)]
#[proc_macro_derive(EnumHack)]
pub fn enum_hack(input: TokenStream) -> TokenStream {
    enum_hack::extract(input)
}

fn expand_paste(input: TokenStream) -> TokenStream {
    let mut contains_paste = false;
    match expand(input, &mut contains_paste) {
        Ok(expanded) => expanded,
        Err(err) => err.to_compile_error(),
    }
}

fn expand(input: TokenStream, contains_paste: &mut bool) -> Result<TokenStream> {
    let mut expanded = TokenStream::new();
    let (mut prev_colon, mut colon) = (false, false);
    let mut prev_none_group = None::<Group>;
    let mut tokens = input.into_iter().peekable();
    loop {
        let token = tokens.next();
        if let Some(group) = prev_none_group.take() {
            if match (&token, tokens.peek()) {
                (Some(TokenTree::Punct(fst)), Some(TokenTree::Punct(snd))) => {
                    fst.as_char() == ':' && snd.as_char() == ':' && fst.spacing() == Spacing::Joint
                }
                _ => false,
            } {
                expanded.extend(group.stream());
                *contains_paste = true;
            } else {
                expanded.extend(iter::once(TokenTree::Group(group)));
            }
        }
        match token {
            Some(TokenTree::Group(group)) => {
                let delimiter = group.delimiter();
                let content = group.stream();
                let span = group.span();
                if delimiter == Delimiter::Bracket && is_paste_operation(&content) {
                    let segments = parse_bracket_as_segments(content, span)?;
                    let pasted = paste_segments(span, &segments)?;
                    expanded.extend(pasted);
                    *contains_paste = true;
                } else if is_none_delimited_flat_group(delimiter, &content) {
                    expanded.extend(content);
                    *contains_paste = true;
                } else {
                    let mut group_contains_paste = false;
                    let nested = expand(content, &mut group_contains_paste)?;
                    let group = if group_contains_paste {
                        let mut group = Group::new(delimiter, nested);
                        group.set_span(span);
                        *contains_paste = true;
                        group
                    } else {
                        group.clone()
                    };
                    if delimiter != Delimiter::None {
                        expanded.extend(iter::once(TokenTree::Group(group)));
                    } else if prev_colon {
                        expanded.extend(group.stream());
                        *contains_paste = true;
                    } else {
                        prev_none_group = Some(group);
                    }
                }
                prev_colon = false;
                colon = false;
            }
            Some(other) => {
                match &other {
                    TokenTree::Punct(punct) if punct.as_char() == ':' => {
                        prev_colon = colon;
                        colon = punct.spacing() == Spacing::Joint;
                    }
                    _ => {
                        prev_colon = false;
                        colon = false;
                    }
                }
                expanded.extend(iter::once(other));
            }
            None => return Ok(expanded),
        }
    }
}

// https://github.com/dtolnay/paste/issues/26
fn is_none_delimited_flat_group(delimiter: Delimiter, input: &TokenStream) -> bool {
    if delimiter != Delimiter::None {
        return false;
    }

    #[derive(PartialEq)]
    enum State {
        Init,
        Ident,
        Literal,
        Apostrophe,
        Lifetime,
        Colon1,
        Colon2,
    }

    let mut state = State::Init;
    for tt in input.clone() {
        state = match (state, &tt) {
            (State::Init, TokenTree::Ident(_)) => State::Ident,
            (State::Init, TokenTree::Literal(_)) => State::Literal,
            (State::Init, TokenTree::Punct(punct)) if punct.as_char() == '\'' => State::Apostrophe,
            (State::Apostrophe, TokenTree::Ident(_)) => State::Lifetime,
            (State::Ident, TokenTree::Punct(punct))
                if punct.as_char() == ':' && punct.spacing() == Spacing::Joint =>
            {
                State::Colon1
            }
            (State::Colon1, TokenTree::Punct(punct))
                if punct.as_char() == ':' && punct.spacing() == Spacing::Alone =>
            {
                State::Colon2
            }
            (State::Colon2, TokenTree::Ident(_)) => State::Ident,
            _ => return false,
        };
    }

    state == State::Ident || state == State::Literal || state == State::Lifetime
}

struct LitStr {
    value: String,
    span: Span,
}

struct Colon {
    span: Span,
}

enum Segment {
    String(String),
    Apostrophe(Span),
    Env(LitStr),
    Modifier(Colon, Ident),
}

fn is_paste_operation(input: &TokenStream) -> bool {
    let mut tokens = input.clone().into_iter();

    match &tokens.next() {
        Some(TokenTree::Punct(punct)) if punct.as_char() == '<' => {}
        _ => return false,
    }

    let mut has_token = false;
    loop {
        match &tokens.next() {
            Some(TokenTree::Punct(punct)) if punct.as_char() == '>' => {
                return has_token && tokens.next().is_none();
            }
            Some(_) => has_token = true,
            None => return false,
        }
    }
}

fn parse_bracket_as_segments(input: TokenStream, scope: Span) -> Result<Vec<Segment>> {
    let mut tokens = input.into_iter().peekable();

    match &tokens.next() {
        Some(TokenTree::Punct(punct)) if punct.as_char() == '<' => {}
        Some(wrong) => return Err(Error::new(wrong.span(), "expected `<`")),
        None => return Err(Error::new(scope, "expected `[< ... >]`")),
    }

    let segments = parse_segments(&mut tokens, scope)?;

    match &tokens.next() {
        Some(TokenTree::Punct(punct)) if punct.as_char() == '>' => {}
        Some(wrong) => return Err(Error::new(wrong.span(), "expected `>`")),
        None => return Err(Error::new(scope, "expected `[< ... >]`")),
    }

    match tokens.next() {
        Some(unexpected) => Err(Error::new(
            unexpected.span(),
            "unexpected input, expected `[< ... >]`",
        )),
        None => Ok(segments),
    }
}

fn parse_segments(
    tokens: &mut Peekable<token_stream::IntoIter>,
    scope: Span,
) -> Result<Vec<Segment>> {
    let mut segments = Vec::new();
    while match tokens.peek() {
        None => false,
        Some(TokenTree::Punct(punct)) => punct.as_char() != '>',
        Some(_) => true,
    } {
        match tokens.next().unwrap() {
            TokenTree::Ident(ident) => {
                let mut fragment = ident.to_string();
                if fragment.starts_with("r#") {
                    fragment = fragment.split_off(2);
                }
                if fragment == "env"
                    && match tokens.peek() {
                        Some(TokenTree::Punct(punct)) => punct.as_char() == '!',
                        _ => false,
                    }
                {
                    tokens.next().unwrap(); // `!`
                    let expect_group = tokens.next();
                    let parenthesized = match &expect_group {
                        Some(TokenTree::Group(group))
                            if group.delimiter() == Delimiter::Parenthesis =>
                        {
                            group
                        }
                        Some(wrong) => return Err(Error::new(wrong.span(), "expected `(`")),
                        None => return Err(Error::new(scope, "expected `(` after `env!`")),
                    };
                    let mut inner = parenthesized.stream().into_iter();
                    let lit = match inner.next() {
                        Some(TokenTree::Literal(lit)) => lit,
                        Some(wrong) => {
                            return Err(Error::new(wrong.span(), "expected string literal"))
                        }
                        None => {
                            return Err(Error::new2(
                                ident.span(),
                                parenthesized.span(),
                                "expected string literal as argument to env! macro",
                            ))
                        }
                    };
                    let lit_string = lit.to_string();
                    if lit_string.starts_with('"')
                        && lit_string.ends_with('"')
                        && lit_string.len() >= 2
                    {
                        // TODO: maybe handle escape sequences in the string if
                        // someone has a use case.
                        segments.push(Segment::Env(LitStr {
                            value: lit_string[1..lit_string.len() - 1].to_owned(),
                            span: lit.span(),
                        }));
                    } else {
                        return Err(Error::new(lit.span(), "expected string literal"));
                    }
                    if let Some(unexpected) = inner.next() {
                        return Err(Error::new(
                            unexpected.span(),
                            "unexpected token in env! macro",
                        ));
                    }
                } else {
                    segments.push(Segment::String(fragment));
                }
            }
            TokenTree::Literal(lit) => {
                let mut lit_string = lit.to_string();
                if lit_string.contains(&['#', '\\', '.', '+'][..]) {
                    return Err(Error::new(lit.span(), "unsupported literal"));
                }
                lit_string = lit_string
                    .replace('"', "")
                    .replace('\'', "")
                    .replace('-', "_");
                segments.push(Segment::String(lit_string));
            }
            TokenTree::Punct(punct) => match punct.as_char() {
                '_' => segments.push(Segment::String("_".to_owned())),
                '\'' => segments.push(Segment::Apostrophe(punct.span())),
                ':' => {
                    let colon = Colon { span: punct.span() };
                    let ident = match tokens.next() {
                        Some(TokenTree::Ident(ident)) => ident,
                        wrong => {
                            let span = wrong.as_ref().map_or(scope, TokenTree::span);
                            return Err(Error::new(span, "expected identifier after `:`"));
                        }
                    };
                    segments.push(Segment::Modifier(colon, ident));
                }
                _ => return Err(Error::new(punct.span(), "unexpected punct")),
            },
            TokenTree::Group(group) => {
                if group.delimiter() == Delimiter::None {
                    let mut inner = group.stream().into_iter().peekable();
                    let nested = parse_segments(&mut inner, group.span())?;
                    if let Some(unexpected) = inner.next() {
                        return Err(Error::new(unexpected.span(), "unexpected token"));
                    }
                    segments.extend(nested);
                } else {
                    return Err(Error::new(group.span(), "unexpected token"));
                }
            }
        }
    }
    Ok(segments)
}

fn paste_segments(span: Span, segments: &[Segment]) -> Result<TokenStream> {
    let mut evaluated = Vec::new();
    let mut is_lifetime = false;

    for segment in segments {
        match segment {
            Segment::String(segment) => {
                evaluated.push(segment.clone());
            }
            Segment::Apostrophe(span) => {
                if is_lifetime {
                    return Err(Error::new(*span, "unexpected lifetime"));
                }
                is_lifetime = true;
            }
            Segment::Env(var) => {
                let resolved = match std::env::var(&var.value) {
                    Ok(resolved) => resolved,
                    Err(_) => {
                        return Err(Error::new(
                            var.span,
                            &format!("no such env var: {:?}", var.value),
                        ));
                    }
                };
                let resolved = resolved.replace('-', "_");
                evaluated.push(resolved);
            }
            Segment::Modifier(colon, ident) => {
                let last = match evaluated.pop() {
                    Some(last) => last,
                    None => {
                        return Err(Error::new2(colon.span, ident.span(), "unexpected modifier"))
                    }
                };
                match ident.to_string().as_str() {
                    "lower" => {
                        evaluated.push(last.to_lowercase());
                    }
                    "upper" => {
                        evaluated.push(last.to_uppercase());
                    }
                    "snake" => {
                        let mut acc = String::new();
                        let mut prev = '_';
                        for ch in last.chars() {
                            if ch.is_uppercase() && prev != '_' {
                                acc.push('_');
                            }
                            acc.push(ch);
                            prev = ch;
                        }
                        evaluated.push(acc.to_lowercase());
                    }
                    "camel" => {
                        let mut acc = String::new();
                        let mut prev = '_';
                        for ch in last.chars() {
                            if ch != '_' {
                                if prev == '_' {
                                    for chu in ch.to_uppercase() {
                                        acc.push(chu);
                                    }
                                } else if prev.is_uppercase() {
                                    for chl in ch.to_lowercase() {
                                        acc.push(chl);
                                    }
                                } else {
                                    acc.push(ch);
                                }
                            }
                            prev = ch;
                        }
                        evaluated.push(acc);
                    }
                    _ => {
                        return Err(Error::new2(
                            colon.span,
                            ident.span(),
                            "unsupported modifier",
                        ));
                    }
                }
            }
        }
    }

    let pasted = evaluated.into_iter().collect::<String>();
    let ident = match panic::catch_unwind(|| Ident::new(&pasted, span)) {
        Ok(ident) => TokenTree::Ident(ident),
        Err(_) => {
            return Err(Error::new(
                span,
                &format!("`{:?}` is not a valid identifier", pasted),
            ));
        }
    };
    let tokens = if is_lifetime {
        let apostrophe = TokenTree::Punct(Punct::new('\'', Spacing::Joint));
        vec![apostrophe, ident]
    } else {
        vec![ident]
    };
    Ok(TokenStream::from_iter(tokens))
}
