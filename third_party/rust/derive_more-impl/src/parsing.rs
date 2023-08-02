//! Common parsing utilities for derive macros.
//!
//! Fair parsing of [`syn::Type`] and [`syn::Expr`] requires [`syn`]'s `full`
//! feature to be enabled, which unnecessary increases compile times. As we
//! don't have complex AST manipulation, usually requiring only understanding
//! where syntax item begins and ends, simpler manual parsing is implemented.

use proc_macro2::{Delimiter, Spacing, TokenStream};
use quote::ToTokens;
use syn::{
    buffer::Cursor,
    parse::{Parse, ParseStream},
    punctuated::Punctuated,
    spanned::Spanned as _,
    token, Error, Ident, Result,
};

/// [`syn::Type`] [`Parse`]ing polyfill.
#[derive(Clone, Debug)]
pub(crate) enum Type {
    /// [`syn::Type::Tuple`] [`Parse`]ing polyfill.
    Tuple {
        paren: token::Paren,
        items: Punctuated<TokenStream, token::Comma>,
    },

    /// Every other [`syn::Type`] variant.
    Other(TokenStream),
}

impl Type {
    /// Creates a [`Type::Tuple`] from the provided [`Iterator`] of [`TokenStream`]s.
    pub(crate) fn tuple<T: ToTokens>(items: impl IntoIterator<Item = T>) -> Self {
        Self::Tuple {
            paren: token::Paren::default(),
            items: items.into_iter().map(ToTokens::into_token_stream).collect(),
        }
    }
}

impl Parse for Type {
    fn parse(input: ParseStream) -> Result<Self> {
        input.step(|c| {
            let outer = *c;

            if let Some((mut cursor, paren_span, next_item)) =
                outer.group(Delimiter::Parenthesis)
            {
                let mut items = Punctuated::new();
                while !cursor.eof() {
                    let (stream, c) = Self::parse_other(cursor).ok_or_else(|| {
                        Error::new(cursor.span(), "failed to parse type")
                    })?;
                    items.push_value(stream);
                    cursor = c;
                    if let Some((p, c)) = punct(',')(cursor) {
                        items.push_punct(token::Comma(p.span()));
                        cursor = c;
                    }
                }
                // `(Type)` is equivalent to `Type`, so isn't top-level tuple.
                if items.len() == 1 && !items.trailing_punct() {
                    let stream = outer
                        .token_tree()
                        .unwrap_or_else(|| unreachable!())
                        .0
                        .into_token_stream();
                    Ok((Type::Other(stream), next_item))
                } else {
                    Ok((
                        Type::Tuple {
                            paren: token::Paren(paren_span),
                            items,
                        },
                        next_item,
                    ))
                }
            } else {
                Self::parse_other(outer)
                    .map(|(s, c)| (Self::Other(s), c))
                    .ok_or_else(|| Error::new(outer.span(), "failed to parse type"))
            }
        })
    }
}

impl ToTokens for Type {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match self {
            Type::Tuple { paren, items } => {
                paren.surround(tokens, |tokens| items.to_tokens(tokens))
            }
            Type::Other(other) => other.to_tokens(tokens),
        }
    }
}

impl Type {
    /// Parses a single [`Type::Other`].
    pub fn parse_other(c: Cursor<'_>) -> Option<(TokenStream, Cursor<'_>)> {
        take_until1(
            alt([&mut balanced_pair(punct('<'), punct('>')), &mut token_tree]),
            punct(','),
        )(c)
    }
}

/// [`syn::Expr`] [`Parse`]ing polyfill.
#[derive(Debug)]
pub(crate) enum Expr {
    /// [`syn::Expr::Path`] of length 1 [`Parse`]ing polyfill.
    Ident(Ident),

    /// Every other [`syn::Expr`] variant.
    Other(TokenStream),
}

impl Expr {
    /// Returns an [`Ident`] in case this [`Expr`] is represented only by it.
    pub(crate) fn ident(&self) -> Option<&Ident> {
        match self {
            Self::Ident(ident) => Some(ident),
            Self::Other(_) => None,
        }
    }
}

impl Parse for Expr {
    fn parse(input: ParseStream) -> Result<Self> {
        if let Ok(ident) = input.step(|c| {
            c.ident()
                .filter(|(_, c)| c.eof() || punct(',')(*c).is_some())
                .ok_or_else(|| Error::new(c.span(), "expected `ident(,|eof)`"))
        }) {
            Ok(Self::Ident(ident))
        } else {
            input.step(|c| {
                take_until1(
                    alt([
                        &mut seq([
                            &mut colon2,
                            &mut balanced_pair(punct('<'), punct('>')),
                        ]),
                        &mut seq([
                            &mut balanced_pair(punct('<'), punct('>')),
                            &mut colon2,
                        ]),
                        &mut balanced_pair(punct('|'), punct('|')),
                        &mut token_tree,
                    ]),
                    punct(','),
                )(*c)
                .map(|(stream, cursor)| (Self::Other(stream), cursor))
                .ok_or_else(|| Error::new(c.span(), "failed to parse expression"))
            })
        }
    }
}

impl ToTokens for Expr {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match self {
            Self::Ident(ident) => ident.to_tokens(tokens),
            Self::Other(other) => other.to_tokens(tokens),
        }
    }
}

/// Result of parsing.
type ParsingResult<'a> = Option<(TokenStream, Cursor<'a>)>;

/// Tries to parse a [`syn::token::Colon2`].
pub fn colon2(c: Cursor<'_>) -> ParsingResult<'_> {
    seq([
        &mut punct_with_spacing(':', Spacing::Joint),
        &mut punct(':'),
    ])(c)
}

/// Tries to parse a [`punct`] with [`Spacing`].
pub fn punct_with_spacing(
    p: char,
    spacing: Spacing,
) -> impl FnMut(Cursor<'_>) -> ParsingResult<'_> {
    move |c| {
        c.punct().and_then(|(punct, c)| {
            (punct.as_char() == p && punct.spacing() == spacing)
                .then(|| (punct.into_token_stream(), c))
        })
    }
}

/// Tries to parse a [`Punct`].
///
/// [`Punct`]: proc_macro2::Punct
pub fn punct(p: char) -> impl FnMut(Cursor<'_>) -> ParsingResult<'_> {
    move |c| {
        c.punct().and_then(|(punct, c)| {
            (punct.as_char() == p).then(|| (punct.into_token_stream(), c))
        })
    }
}

/// Tries to parse any [`TokenTree`].
///
/// [`TokenTree`]: proc_macro2::TokenTree
pub fn token_tree(c: Cursor<'_>) -> ParsingResult<'_> {
    c.token_tree().map(|(tt, c)| (tt.into_token_stream(), c))
}

/// Parses until balanced amount of `open` and `close` or eof.
///
/// [`Cursor`] should be pointing **right after** the first `open`ing.
pub fn balanced_pair(
    mut open: impl FnMut(Cursor<'_>) -> ParsingResult<'_>,
    mut close: impl FnMut(Cursor<'_>) -> ParsingResult<'_>,
) -> impl FnMut(Cursor<'_>) -> ParsingResult<'_> {
    move |c| {
        let (mut out, mut c) = open(c)?;
        let mut count = 1;

        while count != 0 {
            let (stream, cursor) = if let Some(closing) = close(c) {
                count -= 1;
                closing
            } else if let Some(opening) = open(c) {
                count += 1;
                opening
            } else {
                let (tt, c) = c.token_tree()?;
                (tt.into_token_stream(), c)
            };
            out.extend(stream);
            c = cursor;
        }

        Some((out, c))
    }
}

/// Tries to execute the provided sequence of `parsers`.
pub fn seq<const N: usize>(
    mut parsers: [&mut dyn FnMut(Cursor<'_>) -> ParsingResult<'_>; N],
) -> impl FnMut(Cursor<'_>) -> ParsingResult<'_> + '_ {
    move |c| {
        parsers
            .iter_mut()
            .fold(Some((TokenStream::new(), c)), |out, parser| {
                let (mut out, mut c) = out?;
                let (stream, cursor) = parser(c)?;
                out.extend(stream);
                c = cursor;
                Some((out, c))
            })
    }
}

/// Tries to execute the first successful parser.
pub fn alt<const N: usize>(
    mut parsers: [&mut dyn FnMut(Cursor<'_>) -> ParsingResult<'_>; N],
) -> impl FnMut(Cursor<'_>) -> ParsingResult<'_> + '_ {
    move |c| {
        parsers
            .iter_mut()
            .find_map(|parser| parser(c).map(|(s, c)| (s, c)))
    }
}

/// Parses with `basic` while `until` fails. Returns [`None`] in case
/// `until` succeeded initially or `basic` never succeeded. Doesn't consume
/// tokens parsed by `until`.
pub fn take_until1<P, U>(
    mut parser: P,
    mut until: U,
) -> impl FnMut(Cursor<'_>) -> ParsingResult<'_>
where
    P: FnMut(Cursor<'_>) -> ParsingResult<'_>,
    U: FnMut(Cursor<'_>) -> ParsingResult<'_>,
{
    move |mut cursor| {
        let mut out = TokenStream::new();
        let mut parsed = false;

        loop {
            if cursor.eof() || until(cursor).is_some() {
                return parsed.then_some((out, cursor));
            }

            let (stream, c) = parser(cursor)?;
            out.extend(stream);
            cursor = c;
            parsed = true;
        }
    }
}

#[cfg(test)]
mod spec {
    use std::{fmt::Debug, str::FromStr};

    use itertools::Itertools as _;
    use proc_macro2::TokenStream;
    use quote::ToTokens;
    use syn::{
        parse::{Parse, Parser as _},
        punctuated::Punctuated,
        token::Comma,
    };

    use super::{Expr, Type};

    fn assert<'a, T: Debug + Parse + ToTokens>(
        input: &'a str,
        parsed: impl AsRef<[&'a str]>,
    ) {
        let parsed = parsed.as_ref();
        let punctuated = Punctuated::<T, Comma>::parse_terminated
            .parse2(TokenStream::from_str(input).unwrap())
            .unwrap();

        assert_eq!(
            parsed.len(),
            punctuated.len(),
            "Wrong length\n\
             Expected: {parsed:?}\n\
             Found: {punctuated:?}",
        );

        punctuated
            .iter()
            .map(|ty| ty.to_token_stream().to_string())
            .zip(parsed)
            .enumerate()
            .for_each(|(i, (found, expected))| {
                assert_eq!(
                    *expected, &found,
                    "Mismatch at index {i}\n\
                     Expected: {parsed:?}\n\
                     Found: {punctuated:?}",
                );
            });
    }

    mod tuple {
        use super::*;

        #[test]
        fn zst_is_tuple() {
            let zst = "()";
            match syn::parse_str::<Type>(zst).unwrap() {
                Type::Tuple { items, .. } => {
                    assert!(items.is_empty(), "Expected empty tuple, found: {items:?}");
                }
                other => panic!("Expected `Type::Tuple {{ .. }}`, found: {other:?}"),
            }
        }

        #[test]
        fn group_not_tuple() {
            let group = "(Type)";
            match syn::parse_str::<Type>(group).unwrap() {
                Type::Other(tokens) => {
                    assert_eq!(tokens.to_string(), group);
                }
                tuple => panic!("Expected `Type::Other(_)`, found: {tuple:?}"),
            }
        }

        #[test]
        fn single_element_tuple() {
            let tuple = "(Type,)";
            match syn::parse_str::<Type>(tuple).unwrap() {
                Type::Tuple { items, .. } => {
                    assert_eq!(
                        items.len(),
                        1,
                        "Expected empty tuple, found: {items:?}",
                    );
                    assert_eq!(items.first().unwrap().to_string(), "Type");
                }
                other => panic!("Expected `Type::Tuple {{ .. }}`, found: {other:?}"),
            }
        }

        #[test]
        fn cases() {
            let cases = [
                "[Type ; 3]",
                "fn (usize) -> bool",
                "for <'a > fn (&'a usize) -> bool",
                "(Type)",
                "path :: to :: Type",
                "path :: to :: Generic < Type >",
                "< Type as Trait >:: Assoc",
                "< Type as Trait >:: Assoc < GAT >",
                "* const ()",
                "* mut ()",
                "& i32",
                "&'static str",
                "& [str]",
                "dyn Trait",
                "dyn Trait + Send",
                "()",
                "(Type ,)",
                "(Type , Type)",
                "(Type , Type ,)",
            ];

            assert::<Type>("", []);
            for i in 1..4 {
                for permutations in cases.into_iter().permutations(i) {
                    let mut input = permutations.join(",");
                    assert::<Type>(&input, &permutations);
                    input.push(',');
                    assert::<Type>(&input, &permutations);
                }
            }
        }
    }

    mod expr {
        use super::*;

        #[test]
        fn cases() {
            let cases = [
                "ident",
                "[a , b , c , d]",
                "counter += 1",
                "async { fut . await }",
                "a < b",
                "a > b",
                "{ let x = (a , b) ; }",
                "invoke (a , b)",
                "foo as f64",
                "| a , b | a + b",
                "obj . k",
                "for pat in expr { break pat ; }",
                "if expr { true } else { false }",
                "vector [2]",
                "1",
                "\"foo\"",
                "loop { break i ; }",
                "format ! (\"{}\" , q)",
                "match n { Some (n) => { } , None => { } }",
                "x . foo ::< T > (a , b)",
                "x . foo ::< T < [T < T >; if a < b { 1 } else { 2 }] >, { a < b } > (a , b)",
                "(a + b)",
                "i32 :: MAX",
                "1 .. 2",
                "& a",
                "[0u8 ; N]",
                "(a , b , c , d)",
                "< Ty as Trait > :: T",
                "< Ty < Ty < T >, { a < b } > as Trait < T > > :: T",
            ];

            assert::<Expr>("", []);
            for i in 1..4 {
                for permutations in cases.into_iter().permutations(i) {
                    let mut input = permutations.clone().join(",");
                    assert::<Expr>(&input, &permutations);
                    input.push(',');
                    assert::<Expr>(&input, &permutations);
                }
            }
        }
    }
}
