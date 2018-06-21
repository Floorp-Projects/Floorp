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
//! #     let tokens = TokenStream::empty().into();
//! // Parse a nonempty sequence of path segments separated by `::` punctuation
//! // with no trailing punctuation.
//! let parser = Punctuated::<PathSegment, Token![::]>::parse_separated_nonempty;
//! let path = parser.parse(tokens)?;
//!
//! #     let tokens = TokenStream::empty().into();
//! // Parse a possibly empty sequence of expressions terminated by commas with
//! // an optional trailing punctuation.
//! let parser = Punctuated::<Expr, Token![,]>::parse_terminated;
//! let args = parser.parse(tokens)?;
//!
//! #     let tokens = TokenStream::empty().into();
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
use proc_macro2;

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

impl Synom for proc_macro2::TokenStream {
    fn parse(input: Cursor) -> PResult<Self> {
        Ok((input.token_stream(), Cursor::empty()))
    }

    fn description() -> Option<&'static str> {
        Some("arbitrary token stream")
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
    fn parse2(self, tokens: proc_macro2::TokenStream) -> Result<Self::Output, ParseError>;

    /// Parse tokens of source code into the chosen syntax tree node.
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

impl<F, T> Parser for F where F: FnOnce(Cursor) -> PResult<T> {
    type Output = T;

    fn parse2(self, tokens: proc_macro2::TokenStream) -> Result<T, ParseError> {
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
