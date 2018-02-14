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
use proc_macro2::{Delimiter, TokenNode, TokenStream, TokenTree};

#[cfg(feature = "parsing")]
pub fn delimited(input: Cursor) -> PResult<(MacroDelimiter, TokenStream)> {
    match input.token_tree() {
        Some((
            TokenTree {
                span,
                kind: TokenNode::Group(delimiter, tts),
            },
            rest,
        )) => {
            let delimiter = match delimiter {
                Delimiter::Parenthesis => MacroDelimiter::Paren(Paren(span)),
                Delimiter::Brace => MacroDelimiter::Brace(Brace(span)),
                Delimiter::Bracket => MacroDelimiter::Bracket(Bracket(span)),
                Delimiter::None => return parse_error(),
            };
            Ok(((delimiter, tts), rest))
        }
        _ => parse_error(),
    }
}

#[cfg(all(feature = "full", feature = "parsing"))]
pub fn braced(input: Cursor) -> PResult<(Brace, TokenStream)> {
    match input.token_tree() {
        Some((
            TokenTree {
                span,
                kind: TokenNode::Group(Delimiter::Brace, tts),
            },
            rest,
        )) => Ok(((Brace(span), tts), rest)),
        _ => parse_error(),
    }
}

#[cfg(all(feature = "full", feature = "parsing"))]
pub fn parenthesized(input: Cursor) -> PResult<(Paren, TokenStream)> {
    match input.token_tree() {
        Some((
            TokenTree {
                span,
                kind: TokenNode::Group(Delimiter::Parenthesis, tts),
            },
            rest,
        )) => Ok(((Paren(span), tts), rest)),
        _ => parse_error(),
    }
}

#[cfg(feature = "extra-traits")]
pub struct TokenTreeHelper<'a>(pub &'a TokenTree);

#[cfg(feature = "extra-traits")]
impl<'a> PartialEq for TokenTreeHelper<'a> {
    fn eq(&self, other: &Self) -> bool {
        use proc_macro2::Spacing;

        match (&self.0.kind, &other.0.kind) {
            (&TokenNode::Group(d1, ref s1), &TokenNode::Group(d2, ref s2)) => {
                match (d1, d2) {
                    (Delimiter::Parenthesis, Delimiter::Parenthesis)
                    | (Delimiter::Brace, Delimiter::Brace)
                    | (Delimiter::Bracket, Delimiter::Bracket)
                    | (Delimiter::None, Delimiter::None) => {}
                    _ => return false,
                }

                let s1 = s1.clone().into_iter();
                let mut s2 = s2.clone().into_iter();

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
            (&TokenNode::Op(o1, k1), &TokenNode::Op(o2, k2)) => {
                o1 == o2 && match (k1, k2) {
                    (Spacing::Alone, Spacing::Alone) | (Spacing::Joint, Spacing::Joint) => true,
                    _ => false,
                }
            }
            (&TokenNode::Literal(ref l1), &TokenNode::Literal(ref l2)) => {
                l1.to_string() == l2.to_string()
            }
            (&TokenNode::Term(ref s1), &TokenNode::Term(ref s2)) => s1.as_str() == s2.as_str(),
            _ => false,
        }
    }
}

#[cfg(feature = "extra-traits")]
impl<'a> Hash for TokenTreeHelper<'a> {
    fn hash<H: Hasher>(&self, h: &mut H) {
        use proc_macro2::Spacing;

        match self.0.kind {
            TokenNode::Group(delim, ref stream) => {
                0u8.hash(h);
                match delim {
                    Delimiter::Parenthesis => 0u8.hash(h),
                    Delimiter::Brace => 1u8.hash(h),
                    Delimiter::Bracket => 2u8.hash(h),
                    Delimiter::None => 3u8.hash(h),
                }

                for item in stream.clone() {
                    TokenTreeHelper(&item).hash(h);
                }
                0xffu8.hash(h); // terminator w/ a variant we don't normally hash
            }
            TokenNode::Op(op, kind) => {
                1u8.hash(h);
                op.hash(h);
                match kind {
                    Spacing::Alone => 0u8.hash(h),
                    Spacing::Joint => 1u8.hash(h),
                }
            }
            TokenNode::Literal(ref lit) => (2u8, lit.to_string()).hash(h),
            TokenNode::Term(ref word) => (3u8, word.as_str()).hash(h),
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
