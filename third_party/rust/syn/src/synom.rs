// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Parsing interface for parsing a token stream into a syntax tree node.
//!
//! Parsing in Syn is built on parser functions that take in a [`Cursor`] and
//! produce a [`PResult<T>`] where `T` is some syntax tree node. `Cursor` is a
//! cheaply copyable cursor over a range of tokens in a token stream, and
//! `PResult` is a result that packages together a parsed syntax tree node `T`
//! with a stream of remaining unparsed tokens after `T` represented as another
//! `Cursor`, or a [`ParseError`] if parsing failed.
//!
//! [`Cursor`]: ../buffer/index.html
//! [`PResult<T>`]: type.PResult.html
//! [`ParseError`]: struct.ParseError.html
//!
//! This `Cursor`- and `PResult`-based interface is convenient for parser
//! combinators and parser implementations, but not necessarily when you just
//! have some tokens that you want to parse. For that we expose the following
//! two entry points.
//!
//! ## The `syn::parse*` functions
//!
//! The [`syn::parse`], [`syn::parse2`], and [`syn::parse_str`] functions serve
//! as an entry point for parsing syntax tree nodes that can be parsed in an
//! obvious default way. These functions can return any syntax tree node that
//! implements the [`Synom`] trait, which includes most types in Syn.
//!
//! [`syn::parse`]: ../fn.parse.html
//! [`syn::parse2`]: ../fn.parse2.html
//! [`syn::parse_str`]: ../fn.parse_str.html
//! [`Synom`]: trait.Synom.html
//!
//! ```
//! use syn::Type;
//!
//! # fn run_parser() -> Result<(), syn::synom::ParseError> {
//! let t: Type = syn::parse_str("std::collections::HashMap<String, Value>")?;
//! #     Ok(())
//! # }
//! #
//! # fn main() {
//! #     run_parser().unwrap();
//! # }
//! ```
//!
//! The [`parse_quote!`] macro also uses this approach.
//!
//! [`parse_quote!`]: ../macro.parse_quote.html
//!
//! ## The `Parser` trait
//!
//! Some types can be parsed in several ways depending on context. For example
//! an [`Attribute`] can be either "outer" like `#[...]` or "inner" like
//! `#![...]` and parsing the wrong one would be a bug. Similarly [`Punctuated`]
//! may or may not allow trailing punctuation, and parsing it the wrong way
//! would either reject valid input or accept invalid input.
//!
//! [`Attribute`]: ../struct.Attribute.html
//! [`Punctuated`]: ../punctuated/index.html
//!
//! The `Synom` trait is not implemented in these cases because there is no good
//! behavior to consider the default.
//!
//! ```ignore
//! // Can't parse `Punctuated` without knowing whether trailing punctuation
//! // should be allowed in this context.
//! let path: Punctuated<PathSegment, Token![::]> = syn::parse(tokens)?;
//! ```
//!
//! In these cases the types provide a choice of parser functions rather than a
//! single `Synom` implementation, and those parser functions can be invoked
//! through the [`Parser`] trait.
//!
//! [`Parser`]: trait.Parser.html
//!
//! ```
//! # #[macro_use]
//! # extern crate syn;
//! #
//! # extern crate proc_macro2;
//! # use proc_macro2::TokenStream;
//! #
//! use syn::synom::Parser;
//! use syn::punctuated::Punctuated;
//! use syn::{PathSegment, Expr, Attribute};
//!
//! # fn run_parsers() -> Result<(), syn::synom::ParseError> {
//! #     let tokens = TokenStream::new().into();
//! // Parse a nonempty sequence of path segments separated by `::` punctuation
//! // with no trailing punctuation.
//! let parser = Punctuated::<PathSegment, Token![::]>::parse_separated_nonempty;
//! let path = parser.parse(tokens)?;
//!
//! #     let tokens = TokenStream::new().into();
//! // Parse a possibly empty sequence of expressions terminated by commas with
//! // an optional trailing punctuation.
//! let parser = Punctuated::<Expr, Token![,]>::parse_terminated;
//! let args = parser.parse(tokens)?;
//!
//! #     let tokens = TokenStream::new().into();
//! // Parse zero or more outer attributes but not inner attributes.
//! named!(outer_attrs -> Vec<Attribute>, many0!(Attribute::parse_outer));
//! let attrs = outer_attrs.parse(tokens)?;
//! #
//! #     Ok(())
//! # }
//! #
//! # fn main() {}
//! ```
//!
//! # Implementing a parser function
//!
//! Parser functions are usually implemented using the [`nom`]-style parser
//! combinator macros provided by Syn, but may also be implemented without
//! macros be using the low-level [`Cursor`] API directly.
//!
//! [`nom`]: https://github.com/Geal/nom
//!
//! The following parser combinator macros are available and a `Synom` parsing
//! example is provided for each one.
//!
//! - [`alt!`](../macro.alt.html)
//! - [`braces!`](../macro.braces.html)
//! - [`brackets!`](../macro.brackets.html)
//! - [`call!`](../macro.call.html)
//! - [`cond!`](../macro.cond.html)
//! - [`cond_reduce!`](../macro.cond_reduce.html)
//! - [`custom_keyword!`](../macro.custom_keyword.html)
//! - [`do_parse!`](../macro.do_parse.html)
//! - [`epsilon!`](../macro.epsilon.html)
//! - [`input_end!`](../macro.input_end.html)
//! - [`keyword!`](../macro.keyword.html)
//! - [`many0!`](../macro.many0.html)
//! - [`map!`](../macro.map.html)
//! - [`not!`](../macro.not.html)
//! - [`option!`](../macro.option.html)
//! - [`parens!`](../macro.parens.html)
//! - [`punct!`](../macro.punct.html)
//! - [`reject!`](../macro.reject.html)
//! - [`switch!`](../macro.switch.html)
//! - [`syn!`](../macro.syn.html)
//! - [`tuple!`](../macro.tuple.html)
//! - [`value!`](../macro.value.html)
//!
//! *This module is available if Syn is built with the `"parsing"` feature.*

#[cfg(feature = "proc-macro")]
use proc_macro;
use proc_macro2::{Delimiter, Group, Ident, Literal, Punct, TokenStream, TokenTree};

use error::parse_error;
pub use error::{PResult, ParseError};

use buffer::{Cursor, TokenBuffer};

/// Parsing interface implemented by all types that can be parsed in a default
/// way from a token stream.
///
/// Refer to the [module documentation] for details about parsing in Syn.
///
/// [module documentation]: index.html
///
/// *This trait is available if Syn is built with the `"parsing"` feature.*
pub trait Synom: Sized {
    fn parse(input: Cursor) -> PResult<Self>;

    /// A short name of the type being parsed.
    ///
    /// The description should only be used for a simple name.  It should not
    /// contain newlines or sentence-ending punctuation, to facilitate embedding in
    /// larger user-facing strings.  Syn will use this description when building
    /// error messages about parse failures.
    ///
    /// # Examples
    ///
    /// ```
    /// # use syn::buffer::Cursor;
    /// # use syn::synom::{Synom, PResult};
    /// #
    /// struct ExprMacro {
    ///     // ...
    /// }
    ///
    /// impl Synom for ExprMacro {
    /// #   fn parse(input: Cursor) -> PResult<Self> { unimplemented!() }
    ///     // fn parse(...) -> ... { ... }
    ///
    ///     fn description() -> Option<&'static str> {
    ///         // Will result in messages like
    ///         //
    ///         //     "failed to parse macro invocation expression: $reason"
    ///         Some("macro invocation expression")
    ///     }
    /// }
    /// ```
    fn description() -> Option<&'static str> {
        None
    }
}

impl Synom for TokenStream {
    fn parse(input: Cursor) -> PResult<Self> {
        Ok((input.token_stream(), Cursor::empty()))
    }

    fn description() -> Option<&'static str> {
        Some("arbitrary token stream")
    }
}

impl Synom for TokenTree {
    fn parse(input: Cursor) -> PResult<Self> {
        match input.token_tree() {
            Some((tt, rest)) => Ok((tt, rest)),
            None => parse_error(),
        }
    }

    fn description() -> Option<&'static str> {
        Some("token tree")
    }
}

impl Synom for Group {
    fn parse(input: Cursor) -> PResult<Self> {
        for delim in &[Delimiter::Parenthesis, Delimiter::Brace, Delimiter::Bracket] {
            match input.group(*delim) {
                Some((inside, span, rest)) => {
                    let mut group = Group::new(*delim, inside.token_stream());
                    group.set_span(span);
                    return Ok((group, rest));
                }
                None => {}
            }
        }
        parse_error()
    }

    fn description() -> Option<&'static str> {
        Some("group token")
    }
}

impl Synom for Ident {
    fn parse(input: Cursor) -> PResult<Self> {
        let (ident, rest) = match input.ident() {
            Some(ident) => ident,
            _ => return parse_error(),
        };
        match &ident.to_string()[..] {
			"_"
			// From https://doc.rust-lang.org/grammar.html#keywords
			| "abstract" | "alignof" | "as" | "become" | "box" | "break" | "const"
			| "continue" | "crate" | "do" | "else" | "enum" | "extern" | "false" | "final"
			| "fn" | "for" | "if" | "impl" | "in" | "let" | "loop" | "macro" | "match"
			| "mod" | "move" | "mut" | "offsetof" | "override" | "priv" | "proc" | "pub"
			| "pure" | "ref" | "return" | "Self" | "self" | "sizeof" | "static" | "struct"
			| "super" | "trait" | "true" | "type" | "typeof" | "unsafe" | "unsized" | "use"
			| "virtual" | "where" | "while" | "yield" => return parse_error(),
			_ => {}
		}

        Ok((ident, rest))
    }

    fn description() -> Option<&'static str> {
        Some("identifier")
    }
}

impl Synom for Punct {
    fn parse(input: Cursor) -> PResult<Self> {
        match input.punct() {
            Some((punct, rest)) => Ok((punct, rest)),
            None => parse_error(),
        }
    }

    fn description() -> Option<&'static str> {
        Some("punctuation token")
    }
}

impl Synom for Literal {
    fn parse(input: Cursor) -> PResult<Self> {
        match input.literal() {
            Some((literal, rest)) => Ok((literal, rest)),
            None => parse_error(),
        }
    }

    fn description() -> Option<&'static str> {
        Some("literal token")
    }
}

/// Parser that can parse Rust tokens into a particular syntax tree node.
///
/// Refer to the [module documentation] for details about parsing in Syn.
///
/// [module documentation]: index.html
///
/// *This trait is available if Syn is built with the `"parsing"` feature.*
pub trait Parser: Sized {
    type Output;

    /// Parse a proc-macro2 token stream into the chosen syntax tree node.
    fn parse2(self, tokens: TokenStream) -> Result<Self::Output, ParseError>;

    /// Parse tokens of source code into the chosen syntax tree node.
    ///
    /// *This method is available if Syn is built with both the `"parsing"` and
    /// `"proc-macro"` features.*
    #[cfg(feature = "proc-macro")]
    fn parse(self, tokens: proc_macro::TokenStream) -> Result<Self::Output, ParseError> {
        self.parse2(tokens.into())
    }

    /// Parse a string of Rust code into the chosen syntax tree node.
    ///
    /// # Hygiene
    ///
    /// Every span in the resulting syntax tree will be set to resolve at the
    /// macro call site.
    fn parse_str(self, s: &str) -> Result<Self::Output, ParseError> {
        match s.parse() {
            Ok(tts) => self.parse2(tts),
            Err(_) => Err(ParseError::new("error while lexing input string")),
        }
    }
}

impl<F, T> Parser for F
where
    F: FnOnce(Cursor) -> PResult<T>,
{
    type Output = T;

    fn parse2(self, tokens: TokenStream) -> Result<T, ParseError> {
        let buf = TokenBuffer::new2(tokens);
        let (t, rest) = self(buf.begin())?;
        if rest.eof() {
            Ok(t)
        } else if rest == buf.begin() {
            // parsed nothing
            Err(ParseError::new("failed to parse anything"))
        } else {
            Err(ParseError::new("failed to parse all tokens"))
        }
    }
}

/// Extension traits that are made available within the `call!` parser.
///
/// *This module is available if Syn is built with the `"parsing"` feature.*
pub mod ext {
    use super::*;
    use proc_macro2::Ident;

    /// Additional parsing methods for `Ident`.
    ///
    /// This trait is sealed and cannot be implemented for types outside of Syn.
    ///
    /// *This trait is available if Syn is built with the `"parsing"` feature.*
    pub trait IdentExt: Sized + private::Sealed {
        /// Parses any identifier including keywords.
        ///
        /// This is useful when parsing a DSL which allows Rust keywords as
        /// identifiers.
        ///
        /// ```rust
        /// #[macro_use]
        /// extern crate syn;
        ///
        /// use syn::Ident;
        ///
        /// // Parses input that looks like `name = NAME` where `NAME` can be
        /// // any identifier.
        /// //
        /// // Examples:
        /// //
        /// //     name = anything
        /// //     name = impl
        /// named!(parse_dsl -> Ident, do_parse!(
        ///     custom_keyword!(name) >>
        ///     punct!(=) >>
        ///     name: call!(Ident::parse_any) >>
        ///     (name)
        /// ));
        /// #
        /// # fn main() {}
        /// ```
        fn parse_any(input: Cursor) -> PResult<Self>;
    }

    impl IdentExt for Ident {
        fn parse_any(input: Cursor) -> PResult<Self> {
            input.ident().map_or_else(parse_error, Ok)
        }
    }

    mod private {
        use proc_macro2::Ident;

        pub trait Sealed {}

        impl Sealed for Ident {}
    }
}
