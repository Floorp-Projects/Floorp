// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Parsing interface for parsing a token stream into a syntax tree node.
//!
//! Parsing in Syn is built on parser functions that take in a [`ParseStream`]
//! and produce a [`Result<T>`] where `T` is some syntax tree node. Underlying
//! these parser functions is a lower level mechanism built around the
//! [`Cursor`] type. `Cursor` is a cheaply copyable cursor over a range of
//! tokens in a token stream.
//!
//! [`ParseStream`]: type.ParseStream.html
//! [`Result<T>`]: type.Result.html
//! [`Cursor`]: ../buffer/index.html
//!
//! # Example
//!
//! Here is a snippet of parsing code to get a feel for the style of the
//! library. We define data structures for a subset of Rust syntax including
//! enums (not shown) and structs, then provide implementations of the [`Parse`]
//! trait to parse these syntax tree data structures from a token stream.
//!
//! Once `Parse` impls have been defined, they can be called conveniently from a
//! procedural macro through [`parse_macro_input!`] as shown at the bottom of
//! the snippet. If the caller provides syntactically invalid input to the
//! procedural macro, they will receive a helpful compiler error message
//! pointing out the exact token that triggered the failure to parse.
//!
//! [`parse_macro_input!`]: ../macro.parse_macro_input.html
//!
//! ```
//! #[macro_use]
//! extern crate syn;
//!
//! extern crate proc_macro;
//!
//! use proc_macro::TokenStream;
//! use syn::{token, Field, Ident};
//! use syn::parse::{Parse, ParseStream, Result};
//! use syn::punctuated::Punctuated;
//!
//! enum Item {
//!     Struct(ItemStruct),
//!     Enum(ItemEnum),
//! }
//!
//! struct ItemStruct {
//!     struct_token: Token![struct],
//!     ident: Ident,
//!     brace_token: token::Brace,
//!     fields: Punctuated<Field, Token![,]>,
//! }
//! #
//! # enum ItemEnum {}
//!
//! impl Parse for Item {
//!     fn parse(input: ParseStream) -> Result<Self> {
//!         let lookahead = input.lookahead1();
//!         if lookahead.peek(Token![struct]) {
//!             input.parse().map(Item::Struct)
//!         } else if lookahead.peek(Token![enum]) {
//!             input.parse().map(Item::Enum)
//!         } else {
//!             Err(lookahead.error())
//!         }
//!     }
//! }
//!
//! impl Parse for ItemStruct {
//!     fn parse(input: ParseStream) -> Result<Self> {
//!         let content;
//!         Ok(ItemStruct {
//!             struct_token: input.parse()?,
//!             ident: input.parse()?,
//!             brace_token: braced!(content in input),
//!             fields: content.parse_terminated(Field::parse_named)?,
//!         })
//!     }
//! }
//! #
//! # impl Parse for ItemEnum {
//! #     fn parse(input: ParseStream) -> Result<Self> {
//! #         unimplemented!()
//! #     }
//! # }
//!
//! # const IGNORE: &str = stringify! {
//! #[proc_macro]
//! # };
//! pub fn my_macro(tokens: TokenStream) -> TokenStream {
//!     let input = parse_macro_input!(tokens as Item);
//!
//!     /* ... */
//! #   "".parse().unwrap()
//! }
//! #
//! # fn main() {}
//! ```
//!
//! # The `syn::parse*` functions
//!
//! The [`syn::parse`], [`syn::parse2`], and [`syn::parse_str`] functions serve
//! as an entry point for parsing syntax tree nodes that can be parsed in an
//! obvious default way. These functions can return any syntax tree node that
//! implements the [`Parse`] trait, which includes most types in Syn.
//!
//! [`syn::parse`]: ../fn.parse.html
//! [`syn::parse2`]: ../fn.parse2.html
//! [`syn::parse_str`]: ../fn.parse_str.html
//! [`Parse`]: trait.Parse.html
//!
//! ```
//! use syn::Type;
//!
//! # fn run_parser() -> Result<(), syn::parse::Error> {
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
//! # The `Parser` trait
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
//! The `Parse` trait is not implemented in these cases because there is no good
//! behavior to consider the default.
//!
//! ```ignore
//! // Can't parse `Punctuated` without knowing whether trailing punctuation
//! // should be allowed in this context.
//! let path: Punctuated<PathSegment, Token![::]> = syn::parse(tokens)?;
//! ```
//!
//! In these cases the types provide a choice of parser functions rather than a
//! single `Parse` implementation, and those parser functions can be invoked
//! through the [`Parser`] trait.
//!
//! [`Parser`]: trait.Parser.html
//!
//! ```
//! #[macro_use]
//! extern crate syn;
//!
//! extern crate proc_macro2;
//!
//! use proc_macro2::TokenStream;
//! use syn::parse::Parser;
//! use syn::punctuated::Punctuated;
//! use syn::{Attribute, Expr, PathSegment};
//!
//! # fn run_parsers() -> Result<(), syn::parse::Error> {
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
//! let parser = Attribute::parse_outer;
//! let attrs = parser.parse(tokens)?;
//! #
//! #     Ok(())
//! # }
//! #
//! # fn main() {}
//! ```
//!
//! ---
//!
//! *This module is available if Syn is built with the `"parsing"` feature.*

use std::cell::Cell;
use std::fmt::Display;
use std::marker::PhantomData;
use std::mem;
use std::ops::Deref;
use std::rc::Rc;
use std::str::FromStr;

#[cfg(all(
    not(all(target_arch = "wasm32", target_os = "unknown")),
    feature = "proc-macro"
))]
use proc_macro;
use proc_macro2::{self, Delimiter, Group, Literal, Punct, Span, TokenStream, TokenTree};

use buffer::{Cursor, TokenBuffer};
use error;
use lookahead;
use private;
use punctuated::Punctuated;
use token::Token;

pub use error::{Error, Result};
pub use lookahead::{Lookahead1, Peek};

/// Parsing interface implemented by all types that can be parsed in a default
/// way from a token stream.
pub trait Parse: Sized {
    fn parse(input: ParseStream) -> Result<Self>;
}

/// Input to a Syn parser function.
///
/// See the methods of this type under the documentation of [`ParseBuffer`]. For
/// an overview of parsing in Syn, refer to the [module documentation].
///
/// [module documentation]: index.html
pub type ParseStream<'a> = &'a ParseBuffer<'a>;

/// Cursor position within a buffered token stream.
///
/// This type is more commonly used through the type alias [`ParseStream`] which
/// is an alias for `&ParseBuffer`.
///
/// `ParseStream` is the input type for all parser functions in Syn. They have
/// the signature `fn(ParseStream) -> Result<T>`.
pub struct ParseBuffer<'a> {
    scope: Span,
    // Instead of Cell<Cursor<'a>> so that ParseBuffer<'a> is covariant in 'a.
    // The rest of the code in this module needs to be careful that only a
    // cursor derived from this `cell` is ever assigned to this `cell`.
    //
    // Cell<Cursor<'a>> cannot be covariant in 'a because then we could take a
    // ParseBuffer<'a>, upcast to ParseBuffer<'short> for some lifetime shorter
    // than 'a, and then assign a Cursor<'short> into the Cell.
    //
    // By extension, it would not be safe to expose an API that accepts a
    // Cursor<'a> and trusts that it lives as long as the cursor currently in
    // the cell.
    cell: Cell<Cursor<'static>>,
    marker: PhantomData<Cursor<'a>>,
    unexpected: Rc<Cell<Option<Span>>>,
}

impl<'a> Drop for ParseBuffer<'a> {
    fn drop(&mut self) {
        if !self.is_empty() && self.unexpected.get().is_none() {
            self.unexpected.set(Some(self.cursor().span()));
        }
    }
}

/// Cursor state associated with speculative parsing.
///
/// This type is the input of the closure provided to [`ParseStream::step`].
///
/// [`ParseStream::step`]: struct.ParseBuffer.html#method.step
///
/// # Example
///
/// ```
/// # extern crate proc_macro2;
/// # extern crate syn;
/// #
/// use proc_macro2::TokenTree;
/// use syn::parse::{ParseStream, Result};
///
/// // This function advances the stream past the next occurrence of `@`. If
/// // no `@` is present in the stream, the stream position is unchanged and
/// // an error is returned.
/// fn skip_past_next_at(input: ParseStream) -> Result<()> {
///     input.step(|cursor| {
///         let mut rest = *cursor;
///         while let Some((tt, next)) = rest.token_tree() {
///             match tt {
///                 TokenTree::Punct(ref punct) if punct.as_char() == '@' => {
///                     return Ok(((), next));
///                 }
///                 _ => rest = next,
///             }
///         }
///         Err(cursor.error("no `@` was found after this point"))
///     })
/// }
/// #
/// # fn main() {}
/// ```
#[derive(Copy, Clone)]
pub struct StepCursor<'c, 'a> {
    scope: Span,
    // This field is covariant in 'c.
    cursor: Cursor<'c>,
    // This field is contravariant in 'c. Together these make StepCursor
    // invariant in 'c. Also covariant in 'a. The user cannot cast 'c to a
    // different lifetime but can upcast into a StepCursor with a shorter
    // lifetime 'a.
    //
    // As long as we only ever construct a StepCursor for which 'c outlives 'a,
    // this means if ever a StepCursor<'c, 'a> exists we are guaranteed that 'c
    // outlives 'a.
    marker: PhantomData<fn(Cursor<'c>) -> Cursor<'a>>,
}

impl<'c, 'a> Deref for StepCursor<'c, 'a> {
    type Target = Cursor<'c>;

    fn deref(&self) -> &Self::Target {
        &self.cursor
    }
}

impl<'c, 'a> StepCursor<'c, 'a> {
    /// Triggers an error at the current position of the parse stream.
    ///
    /// The `ParseStream::step` invocation will return this same error without
    /// advancing the stream state.
    pub fn error<T: Display>(self, message: T) -> Error {
        error::new_at(self.scope, self.cursor, message)
    }
}

impl private {
    pub fn advance_step_cursor<'c, 'a>(proof: StepCursor<'c, 'a>, to: Cursor<'c>) -> Cursor<'a> {
        // Refer to the comments within the StepCursor definition. We use the
        // fact that a StepCursor<'c, 'a> exists as proof that 'c outlives 'a.
        // Cursor is covariant in its lifetime parameter so we can cast a
        // Cursor<'c> to one with the shorter lifetime Cursor<'a>.
        let _ = proof;
        unsafe { mem::transmute::<Cursor<'c>, Cursor<'a>>(to) }
    }
}

fn skip(input: ParseStream) -> bool {
    input
        .step(|cursor| {
            if let Some((_lifetime, rest)) = cursor.lifetime() {
                Ok((true, rest))
            } else if let Some((_token, rest)) = cursor.token_tree() {
                Ok((true, rest))
            } else {
                Ok((false, *cursor))
            }
        }).unwrap()
}

impl private {
    pub fn new_parse_buffer(
        scope: Span,
        cursor: Cursor,
        unexpected: Rc<Cell<Option<Span>>>,
    ) -> ParseBuffer {
        ParseBuffer {
            scope: scope,
            // See comment on `cell` in the struct definition.
            cell: Cell::new(unsafe { mem::transmute::<Cursor, Cursor<'static>>(cursor) }),
            marker: PhantomData,
            unexpected: unexpected,
        }
    }

    pub fn get_unexpected(buffer: &ParseBuffer) -> Rc<Cell<Option<Span>>> {
        buffer.unexpected.clone()
    }
}

impl<'a> ParseBuffer<'a> {
    /// Parses a syntax tree node of type `T`, advancing the position of our
    /// parse stream past it.
    pub fn parse<T: Parse>(&self) -> Result<T> {
        T::parse(self)
    }

    /// Calls the given parser function to parse a syntax tree node of type `T`
    /// from this stream.
    ///
    /// # Example
    ///
    /// The parser below invokes [`Attribute::parse_outer`] to parse a vector of
    /// zero or more outer attributes.
    ///
    /// [`Attribute::parse_outer`]: ../struct.Attribute.html#method.parse_outer
    ///
    /// ```
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::{Attribute, Ident};
    /// use syn::parse::{Parse, ParseStream, Result};
    ///
    /// // Parses a unit struct with attributes.
    /// //
    /// //     #[path = "s.tmpl"]
    /// //     struct S;
    /// struct UnitStruct {
    ///     attrs: Vec<Attribute>,
    ///     struct_token: Token![struct],
    ///     name: Ident,
    ///     semi_token: Token![;],
    /// }
    ///
    /// impl Parse for UnitStruct {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         Ok(UnitStruct {
    ///             attrs: input.call(Attribute::parse_outer)?,
    ///             struct_token: input.parse()?,
    ///             name: input.parse()?,
    ///             semi_token: input.parse()?,
    ///         })
    ///     }
    /// }
    /// #
    /// # fn main() {}
    /// ```
    pub fn call<T>(&self, function: fn(ParseStream) -> Result<T>) -> Result<T> {
        function(self)
    }

    /// Looks at the next token in the parse stream to determine whether it
    /// matches the requested type of token.
    ///
    /// Does not advance the position of the parse stream.
    ///
    /// # Syntax
    ///
    /// Note that this method does not use turbofish syntax. Pass the peek type
    /// inside of parentheses.
    ///
    /// - `input.peek(Token![struct])`
    /// - `input.peek(Token![==])`
    /// - `input.peek(Ident)`
    /// - `input.peek(Lifetime)`
    /// - `input.peek(token::Brace)`
    ///
    /// # Example
    ///
    /// In this example we finish parsing the list of supertraits when the next
    /// token in the input is either `where` or an opening curly brace.
    ///
    /// ```
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::{token, Generics, Ident, TypeParamBound};
    /// use syn::parse::{Parse, ParseStream, Result};
    /// use syn::punctuated::Punctuated;
    ///
    /// // Parses a trait definition containing no associated items.
    /// //
    /// //     trait Marker<'de, T>: A + B<'de> where Box<T>: Clone {}
    /// struct MarkerTrait {
    ///     trait_token: Token![trait],
    ///     ident: Ident,
    ///     generics: Generics,
    ///     colon_token: Option<Token![:]>,
    ///     supertraits: Punctuated<TypeParamBound, Token![+]>,
    ///     brace_token: token::Brace,
    /// }
    ///
    /// impl Parse for MarkerTrait {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         let trait_token: Token![trait] = input.parse()?;
    ///         let ident: Ident = input.parse()?;
    ///         let mut generics: Generics = input.parse()?;
    ///         let colon_token: Option<Token![:]> = input.parse()?;
    ///
    ///         let mut supertraits = Punctuated::new();
    ///         if colon_token.is_some() {
    ///             loop {
    ///                 supertraits.push_value(input.parse()?);
    ///                 if input.peek(Token![where]) || input.peek(token::Brace) {
    ///                     break;
    ///                 }
    ///                 supertraits.push_punct(input.parse()?);
    ///             }
    ///         }
    ///
    ///         generics.where_clause = input.parse()?;
    ///         let content;
    ///         let empty_brace_token = braced!(content in input);
    ///
    ///         Ok(MarkerTrait {
    ///             trait_token: trait_token,
    ///             ident: ident,
    ///             generics: generics,
    ///             colon_token: colon_token,
    ///             supertraits: supertraits,
    ///             brace_token: empty_brace_token,
    ///         })
    ///     }
    /// }
    /// #
    /// # fn main() {}
    /// ```
    pub fn peek<T: Peek>(&self, token: T) -> bool {
        let _ = token;
        T::Token::peek(self.cursor())
    }

    /// Looks at the second-next token in the parse stream.
    ///
    /// This is commonly useful as a way to implement contextual keywords.
    ///
    /// # Example
    ///
    /// This example needs to use `peek2` because the symbol `union` is not a
    /// keyword in Rust. We can't use just `peek` and decide to parse a union if
    /// the very next token is `union`, because someone is free to write a `mod
    /// union` and a macro invocation that looks like `union::some_macro! { ...
    /// }`. In other words `union` is a contextual keyword.
    ///
    /// ```
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::{Ident, ItemUnion, Macro};
    /// use syn::parse::{Parse, ParseStream, Result};
    ///
    /// // Parses either a union or a macro invocation.
    /// enum UnionOrMacro {
    ///     // union MaybeUninit<T> { uninit: (), value: T }
    ///     Union(ItemUnion),
    ///     // lazy_static! { ... }
    ///     Macro(Macro),
    /// }
    ///
    /// impl Parse for UnionOrMacro {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         if input.peek(Token![union]) && input.peek2(Ident) {
    ///             input.parse().map(UnionOrMacro::Union)
    ///         } else {
    ///             input.parse().map(UnionOrMacro::Macro)
    ///         }
    ///     }
    /// }
    /// #
    /// # fn main() {}
    /// ```
    pub fn peek2<T: Peek>(&self, token: T) -> bool {
        let ahead = self.fork();
        skip(&ahead) && ahead.peek(token)
    }

    /// Looks at the third-next token in the parse stream.
    pub fn peek3<T: Peek>(&self, token: T) -> bool {
        let ahead = self.fork();
        skip(&ahead) && skip(&ahead) && ahead.peek(token)
    }

    /// Parses zero or more occurrences of `T` separated by punctuation of type
    /// `P`, with optional trailing punctuation.
    ///
    /// Parsing continues until the end of this parse stream. The entire content
    /// of this parse stream must consist of `T` and `P`.
    ///
    /// # Example
    ///
    /// ```rust
    /// # #[macro_use]
    /// # extern crate quote;
    /// #
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::{token, Ident, Type};
    /// use syn::parse::{Parse, ParseStream, Result};
    /// use syn::punctuated::Punctuated;
    ///
    /// // Parse a simplified tuple struct syntax like:
    /// //
    /// //     struct S(A, B);
    /// struct TupleStruct {
    ///     struct_token: Token![struct],
    ///     ident: Ident,
    ///     paren_token: token::Paren,
    ///     fields: Punctuated<Type, Token![,]>,
    ///     semi_token: Token![;],
    /// }
    ///
    /// impl Parse for TupleStruct {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         let content;
    ///         Ok(TupleStruct {
    ///             struct_token: input.parse()?,
    ///             ident: input.parse()?,
    ///             paren_token: parenthesized!(content in input),
    ///             fields: content.parse_terminated(Type::parse)?,
    ///             semi_token: input.parse()?,
    ///         })
    ///     }
    /// }
    /// #
    /// # fn main() {
    /// #     let input = quote! {
    /// #         struct S(A, B);
    /// #     };
    /// #     syn::parse2::<TupleStruct>(input).unwrap();
    /// # }
    /// ```
    pub fn parse_terminated<T, P: Parse>(
        &self,
        parser: fn(ParseStream) -> Result<T>,
    ) -> Result<Punctuated<T, P>> {
        Punctuated::parse_terminated_with(self, parser)
    }

    /// Returns whether there are tokens remaining in this stream.
    ///
    /// This method returns true at the end of the content of a set of
    /// delimiters, as well as at the very end of the complete macro input.
    ///
    /// # Example
    ///
    /// ```rust
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::{token, Ident, Item};
    /// use syn::parse::{Parse, ParseStream, Result};
    ///
    /// // Parses a Rust `mod m { ... }` containing zero or more items.
    /// struct Mod {
    ///     mod_token: Token![mod],
    ///     name: Ident,
    ///     brace_token: token::Brace,
    ///     items: Vec<Item>,
    /// }
    ///
    /// impl Parse for Mod {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         let content;
    ///         Ok(Mod {
    ///             mod_token: input.parse()?,
    ///             name: input.parse()?,
    ///             brace_token: braced!(content in input),
    ///             items: {
    ///                 let mut items = Vec::new();
    ///                 while !content.is_empty() {
    ///                     items.push(content.parse()?);
    ///                 }
    ///                 items
    ///             },
    ///         })
    ///     }
    /// }
    /// #
    /// # fn main() {}
    pub fn is_empty(&self) -> bool {
        self.cursor().eof()
    }

    /// Constructs a helper for peeking at the next token in this stream and
    /// building an error message if it is not one of a set of expected tokens.
    ///
    /// # Example
    ///
    /// ```
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::{ConstParam, Ident, Lifetime, LifetimeDef, TypeParam};
    /// use syn::parse::{Parse, ParseStream, Result};
    ///
    /// // A generic parameter, a single one of the comma-separated elements inside
    /// // angle brackets in:
    /// //
    /// //     fn f<T: Clone, 'a, 'b: 'a, const N: usize>() { ... }
    /// //
    /// // On invalid input, lookahead gives us a reasonable error message.
    /// //
    /// //     error: expected one of: identifier, lifetime, `const`
    /// //       |
    /// //     5 |     fn f<!Sized>() {}
    /// //       |          ^
    /// enum GenericParam {
    ///     Type(TypeParam),
    ///     Lifetime(LifetimeDef),
    ///     Const(ConstParam),
    /// }
    ///
    /// impl Parse for GenericParam {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         let lookahead = input.lookahead1();
    ///         if lookahead.peek(Ident) {
    ///             input.parse().map(GenericParam::Type)
    ///         } else if lookahead.peek(Lifetime) {
    ///             input.parse().map(GenericParam::Lifetime)
    ///         } else if lookahead.peek(Token![const]) {
    ///             input.parse().map(GenericParam::Const)
    ///         } else {
    ///             Err(lookahead.error())
    ///         }
    ///     }
    /// }
    /// #
    /// # fn main() {}
    /// ```
    pub fn lookahead1(&self) -> Lookahead1<'a> {
        lookahead::new(self.scope, self.cursor())
    }

    /// Forks a parse stream so that parsing tokens out of either the original
    /// or the fork does not advance the position of the other.
    ///
    /// # Performance
    ///
    /// Forking a parse stream is a cheap fixed amount of work and does not
    /// involve copying token buffers. Where you might hit performance problems
    /// is if your macro ends up parsing a large amount of content more than
    /// once.
    ///
    /// ```
    /// # use syn::Expr;
    /// # use syn::parse::{ParseStream, Result};
    /// #
    /// # fn bad(input: ParseStream) -> Result<Expr> {
    /// // Do not do this.
    /// if input.fork().parse::<Expr>().is_ok() {
    ///     return input.parse::<Expr>();
    /// }
    /// # unimplemented!()
    /// # }
    /// ```
    ///
    /// As a rule, avoid parsing an unbounded amount of tokens out of a forked
    /// parse stream. Only use a fork when the amount of work performed against
    /// the fork is small and bounded.
    ///
    /// For a lower level but occasionally more performant way to perform
    /// speculative parsing, consider using [`ParseStream::step`] instead.
    ///
    /// [`ParseStream::step`]: #method.step
    ///
    /// # Example
    ///
    /// The parse implementation shown here parses possibly restricted `pub`
    /// visibilities.
    ///
    /// - `pub`
    /// - `pub(crate)`
    /// - `pub(self)`
    /// - `pub(super)`
    /// - `pub(in some::path)`
    ///
    /// To handle the case of visibilities inside of tuple structs, the parser
    /// needs to distinguish parentheses that specify visibility restrictions
    /// from parentheses that form part of a tuple type.
    ///
    /// ```
    /// # struct A;
    /// # struct B;
    /// # struct C;
    /// #
    /// struct S(pub(crate) A, pub (B, C));
    /// ```
    ///
    /// In this example input the first tuple struct element of `S` has
    /// `pub(crate)` visibility while the second tuple struct element has `pub`
    /// visibility; the parentheses around `(B, C)` are part of the type rather
    /// than part of a visibility restriction.
    ///
    /// The parser uses a forked parse stream to check the first token inside of
    /// parentheses after the `pub` keyword. This is a small bounded amount of
    /// work performed against the forked parse stream.
    ///
    /// ```
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::{token, Ident, Path};
    /// use syn::ext::IdentExt;
    /// use syn::parse::{Parse, ParseStream, Result};
    ///
    /// struct PubVisibility {
    ///     pub_token: Token![pub],
    ///     restricted: Option<Restricted>,
    /// }
    ///
    /// struct Restricted {
    ///     paren_token: token::Paren,
    ///     in_token: Option<Token![in]>,
    ///     path: Path,
    /// }
    ///
    /// impl Parse for PubVisibility {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         let pub_token: Token![pub] = input.parse()?;
    ///
    ///         if input.peek(token::Paren) {
    ///             let ahead = input.fork();
    ///             let mut content;
    ///             parenthesized!(content in ahead);
    ///
    ///             if content.peek(Token![crate])
    ///                 || content.peek(Token![self])
    ///                 || content.peek(Token![super])
    ///             {
    ///                 return Ok(PubVisibility {
    ///                     pub_token: pub_token,
    ///                     restricted: Some(Restricted {
    ///                         paren_token: parenthesized!(content in input),
    ///                         in_token: None,
    ///                         path: Path::from(content.call(Ident::parse_any)?),
    ///                     }),
    ///                 });
    ///             } else if content.peek(Token![in]) {
    ///                 return Ok(PubVisibility {
    ///                     pub_token: pub_token,
    ///                     restricted: Some(Restricted {
    ///                         paren_token: parenthesized!(content in input),
    ///                         in_token: Some(content.parse()?),
    ///                         path: content.call(Path::parse_mod_style)?,
    ///                     }),
    ///                 });
    ///             }
    ///         }
    ///
    ///         Ok(PubVisibility {
    ///             pub_token: pub_token,
    ///             restricted: None,
    ///         })
    ///     }
    /// }
    /// #
    /// # fn main() {}
    /// ```
    pub fn fork(&self) -> Self {
        ParseBuffer {
            scope: self.scope,
            cell: self.cell.clone(),
            marker: PhantomData,
            // Not the parent's unexpected. Nothing cares whether the clone
            // parses all the way.
            unexpected: Rc::new(Cell::new(None)),
        }
    }

    /// Triggers an error at the current position of the parse stream.
    ///
    /// # Example
    ///
    /// ```
    /// #[macro_use]
    /// extern crate syn;
    ///
    /// use syn::Expr;
    /// use syn::parse::{Parse, ParseStream, Result};
    ///
    /// // Some kind of loop: `while` or `for` or `loop`.
    /// struct Loop {
    ///     expr: Expr,
    /// }
    ///
    /// impl Parse for Loop {
    ///     fn parse(input: ParseStream) -> Result<Self> {
    ///         if input.peek(Token![while])
    ///             || input.peek(Token![for])
    ///             || input.peek(Token![loop])
    ///         {
    ///             Ok(Loop {
    ///                 expr: input.parse()?,
    ///             })
    ///         } else {
    ///             Err(input.error("expected some kind of loop"))
    ///         }
    ///     }
    /// }
    /// #
    /// # fn main() {}
    /// ```
    pub fn error<T: Display>(&self, message: T) -> Error {
        error::new_at(self.scope, self.cursor(), message)
    }

    /// Speculatively parses tokens from this parse stream, advancing the
    /// position of this stream only if parsing succeeds.
    ///
    /// This is a powerful low-level API used for defining the `Parse` impls of
    /// the basic built-in token types. It is not something that will be used
    /// widely outside of the Syn codebase.
    ///
    /// # Example
    ///
    /// ```
    /// # extern crate proc_macro2;
    /// # extern crate syn;
    /// #
    /// use proc_macro2::TokenTree;
    /// use syn::parse::{ParseStream, Result};
    ///
    /// // This function advances the stream past the next occurrence of `@`. If
    /// // no `@` is present in the stream, the stream position is unchanged and
    /// // an error is returned.
    /// fn skip_past_next_at(input: ParseStream) -> Result<()> {
    ///     input.step(|cursor| {
    ///         let mut rest = *cursor;
    ///         while let Some((tt, next)) = cursor.token_tree() {
    ///             match tt {
    ///                 TokenTree::Punct(ref punct) if punct.as_char() == '@' => {
    ///                     return Ok(((), next));
    ///                 }
    ///                 _ => rest = next,
    ///             }
    ///         }
    ///         Err(cursor.error("no `@` was found after this point"))
    ///     })
    /// }
    /// #
    /// # fn main() {}
    /// ```
    pub fn step<F, R>(&self, function: F) -> Result<R>
    where
        F: for<'c> FnOnce(StepCursor<'c, 'a>) -> Result<(R, Cursor<'c>)>,
    {
        // Since the user's function is required to work for any 'c, we know
        // that the Cursor<'c> they return is either derived from the input
        // StepCursor<'c, 'a> or from a Cursor<'static>.
        //
        // It would not be legal to write this function without the invariant
        // lifetime 'c in StepCursor<'c, 'a>. If this function were written only
        // in terms of 'a, the user could take our ParseBuffer<'a>, upcast it to
        // a ParseBuffer<'short> which some shorter lifetime than 'a, invoke
        // `step` on their ParseBuffer<'short> with a closure that returns
        // Cursor<'short>, and we would wrongly write that Cursor<'short> into
        // the Cell intended to hold Cursor<'a>.
        //
        // In some cases it may be necessary for R to contain a Cursor<'a>.
        // Within Syn we solve this using `private::advance_step_cursor` which
        // uses the existence of a StepCursor<'c, 'a> as proof that it is safe
        // to cast from Cursor<'c> to Cursor<'a>. If needed outside of Syn, it
        // would be safe to expose that API as a method on StepCursor.
        let (node, rest) = function(StepCursor {
            scope: self.scope,
            cursor: self.cell.get(),
            marker: PhantomData,
        })?;
        self.cell.set(rest);
        Ok(node)
    }

    /// Provides low-level access to the token representation underlying this
    /// parse stream.
    ///
    /// Cursors are immutable so no operations you perform against the cursor
    /// will affect the state of this parse stream.
    pub fn cursor(&self) -> Cursor<'a> {
        self.cell.get()
    }

    fn check_unexpected(&self) -> Result<()> {
        match self.unexpected.get() {
            Some(span) => Err(Error::new(span, "unexpected token")),
            None => Ok(()),
        }
    }
}

impl<T: Parse> Parse for Box<T> {
    fn parse(input: ParseStream) -> Result<Self> {
        input.parse().map(Box::new)
    }
}

impl<T: Parse + Token> Parse for Option<T> {
    fn parse(input: ParseStream) -> Result<Self> {
        if T::peek(input.cursor()) {
            Ok(Some(input.parse()?))
        } else {
            Ok(None)
        }
    }
}

impl Parse for TokenStream {
    fn parse(input: ParseStream) -> Result<Self> {
        input.step(|cursor| Ok((cursor.token_stream(), Cursor::empty())))
    }
}

impl Parse for TokenTree {
    fn parse(input: ParseStream) -> Result<Self> {
        input.step(|cursor| match cursor.token_tree() {
            Some((tt, rest)) => Ok((tt, rest)),
            None => Err(cursor.error("expected token tree")),
        })
    }
}

impl Parse for Group {
    fn parse(input: ParseStream) -> Result<Self> {
        input.step(|cursor| {
            for delim in &[Delimiter::Parenthesis, Delimiter::Brace, Delimiter::Bracket] {
                if let Some((inside, span, rest)) = cursor.group(*delim) {
                    let mut group = Group::new(*delim, inside.token_stream());
                    group.set_span(span);
                    return Ok((group, rest));
                }
            }
            Err(cursor.error("expected group token"))
        })
    }
}

impl Parse for Punct {
    fn parse(input: ParseStream) -> Result<Self> {
        input.step(|cursor| match cursor.punct() {
            Some((punct, rest)) => Ok((punct, rest)),
            None => Err(cursor.error("expected punctuation token")),
        })
    }
}

impl Parse for Literal {
    fn parse(input: ParseStream) -> Result<Self> {
        input.step(|cursor| match cursor.literal() {
            Some((literal, rest)) => Ok((literal, rest)),
            None => Err(cursor.error("expected literal token")),
        })
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
    fn parse2(self, tokens: TokenStream) -> Result<Self::Output>;

    /// Parse tokens of source code into the chosen syntax tree node.
    ///
    /// *This method is available if Syn is built with both the `"parsing"` and
    /// `"proc-macro"` features.*
    #[cfg(all(
        not(all(target_arch = "wasm32", target_os = "unknown")),
        feature = "proc-macro"
    ))]
    fn parse(self, tokens: proc_macro::TokenStream) -> Result<Self::Output> {
        self.parse2(proc_macro2::TokenStream::from(tokens))
    }

    /// Parse a string of Rust code into the chosen syntax tree node.
    ///
    /// # Hygiene
    ///
    /// Every span in the resulting syntax tree will be set to resolve at the
    /// macro call site.
    fn parse_str(self, s: &str) -> Result<Self::Output> {
        self.parse2(proc_macro2::TokenStream::from_str(s)?)
    }
}

fn tokens_to_parse_buffer(tokens: &TokenBuffer) -> ParseBuffer {
    let scope = Span::call_site();
    let cursor = tokens.begin();
    let unexpected = Rc::new(Cell::new(None));
    private::new_parse_buffer(scope, cursor, unexpected)
}

impl<F, T> Parser for F
where
    F: FnOnce(ParseStream) -> Result<T>,
{
    type Output = T;

    fn parse2(self, tokens: TokenStream) -> Result<T> {
        let buf = TokenBuffer::new2(tokens);
        let state = tokens_to_parse_buffer(&buf);
        let node = self(&state)?;
        state.check_unexpected()?;
        if state.is_empty() {
            Ok(node)
        } else {
            Err(state.error("unexpected token"))
        }
    }
}
