// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[cfg(feature = "parsing")]
use buffer::Cursor;
#[cfg(feature = "parsing")]
use synom::PResult;
#[cfg(feature = "parsing")]
use token::{Brace, Bracket, Paren};
#[cfg(feature = "parsing")]
use {parse_error, MacroDelimiter};

#[cfg(feature = "extra-traits")]
use std::hash::{Hash, Hasher};

#[cfg(any(feature = "parsing", feature = "extra-traits"))]
use proc_macro2::{Delimiter, TokenStream, TokenTree};

#[cfg(feature = "parsing")]
pub fn delimited(input: Cursor) -> PResult<(MacroDelimiter, TokenStream)> {
    if let Some((TokenTree::Group(g), rest)) = input.token_tree() {
        let span = g.span();
        let delimiter = match g.delimiter() {
            Delimiter::Parenthesis => MacroDelimiter::Paren(Paren(span)),
            Delimiter::Brace => MacroDelimiter::Brace(Brace(span)),
            Delimiter::Bracket => MacroDelimiter::Bracket(Bracket(span)),
            Delimiter::None => return parse_error(),
        };

        return Ok(((delimiter, g.stream().clone()), rest))
    }
    parse_error()
}

#[cfg(all(feature = "full", feature = "parsing"))]
pub fn braced(input: Cursor) -> PResult<(Brace, TokenStream)> {
    if let Some((TokenTree::Group(g), rest)) = input.token_tree() {
        if g.delimiter() == Delimiter::Brace {
            return Ok(((Brace(g.span()), g.stream().clone()), rest))
        }
    }
    parse_error()
}

#[cfg(all(feature = "full", feature = "parsing"))]
pub fn parenthesized(input: Cursor) -> PResult<(Paren, TokenStream)> {
    if let Some((TokenTree::Group(g), rest)) = input.token_tree() {
        if g.delimiter() == Delimiter::Parenthesis {
            return Ok(((Paren(g.span()), g.stream().clone()), rest))
        }
    }
    parse_error()
}

#[cfg(feature = "extra-traits")]
pub struct TokenTreeHelper<'a>(pub &'a TokenTree);

#[cfg(feature = "extra-traits")]
impl<'a> PartialEq for TokenTreeHelper<'a> {
    fn eq(&self, other: &Self) -> bool {
        use proc_macro2::Spacing;

        match (self.0, other.0) {
            (&TokenTree::Group(ref g1), &TokenTree::Group(ref g2)) => {
                match (g1.delimiter(), g2.delimiter()) {
                    (Delimiter::Parenthesis, Delimiter::Parenthesis)
                    | (Delimiter::Brace, Delimiter::Brace)
                    | (Delimiter::Bracket, Delimiter::Bracket)
                    | (Delimiter::None, Delimiter::None) => {}
                    _ => return false,
                }

                let s1 = g1.stream().clone().into_iter();
                let mut s2 = g2.stream().clone().into_iter();

                for item1 in s1 {
                    let item2 = match s2.next() {
                        Some(item) => item,
                        None => return false,
                    };
                    if TokenTreeHelper(&item1) != TokenTreeHelper(&item2) {
                        return false;
                    }
                }
                s2.next().is_none()
            }
            (&TokenTree::Op(ref o1), &TokenTree::Op(ref o2)) => {
                o1.op() == o2.op() && match (o1.spacing(), o2.spacing()) {
                    (Spacing::Alone, Spacing::Alone) | (Spacing::Joint, Spacing::Joint) => true,
                    _ => false,
                }
            }
            (&TokenTree::Literal(ref l1), &TokenTree::Literal(ref l2)) => {
                l1.to_string() == l2.to_string()
            }
            (&TokenTree::Term(ref s1), &TokenTree::Term(ref s2)) => s1.as_str() == s2.as_str(),
            _ => false,
        }
    }
}

#[cfg(feature = "extra-traits")]
impl<'a> Hash for TokenTreeHelper<'a> {
    fn hash<H: Hasher>(&self, h: &mut H) {
        use proc_macro2::Spacing;

        match *self.0 {
            TokenTree::Group(ref g) => {
                0u8.hash(h);
                match g.delimiter() {
                    Delimiter::Parenthesis => 0u8.hash(h),
                    Delimiter::Brace => 1u8.hash(h),
                    Delimiter::Bracket => 2u8.hash(h),
                    Delimiter::None => 3u8.hash(h),
                }

                for item in g.stream().clone() {
                    TokenTreeHelper(&item).hash(h);
                }
                0xffu8.hash(h); // terminator w/ a variant we don't normally hash
            }
            TokenTree::Op(ref op) => {
                1u8.hash(h);
                op.op().hash(h);
                match op.spacing() {
                    Spacing::Alone => 0u8.hash(h),
                    Spacing::Joint => 1u8.hash(h),
                }
            }
            TokenTree::Literal(ref lit) => (2u8, lit.to_string()).hash(h),
            TokenTree::Term(ref word) => (3u8, word.as_str()).hash(h),
        }
    }
}

#[cfg(feature = "extra-traits")]
pub struct TokenStreamHelper<'a>(pub &'a TokenStream);

#[cfg(feature = "extra-traits")]
impl<'a> PartialEq for TokenStreamHelper<'a> {
    fn eq(&self, other: &Self) -> bool {
        let left = self.0.clone().into_iter().collect::<Vec<_>>();
        let right = other.0.clone().into_iter().collect::<Vec<_>>();
        if left.len() != right.len() {
            return false;
        }
        for (a, b) in left.into_iter().zip(right) {
            if TokenTreeHelper(&a) != TokenTreeHelper(&b) {
                return false;
            }
        }
        true
    }
}

#[cfg(feature = "extra-traits")]
impl<'a> Hash for TokenStreamHelper<'a> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        let tts = self.0.clone().into_iter().collect::<Vec<_>>();
        tts.len().hash(state);
        for tt in tts {
            TokenTreeHelper(&tt).hash(state);
        }
    }
}
