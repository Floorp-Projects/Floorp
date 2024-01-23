//! Traits for parsing the WebAssembly Text format
//!
//! This module contains the traits, abstractions, and utilities needed to
//! define custom parsers for WebAssembly text format items. This module exposes
//! a recursive descent parsing strategy and centers around the
//! [`Parse`](crate::parser::Parse) trait for defining new fragments of
//! WebAssembly text syntax.
//!
//! The top-level [`parse`](crate::parser::parse) function can be used to fully parse AST fragments:
//!
//! ```
//! use wast::Wat;
//! use wast::parser::{self, ParseBuffer};
//!
//! # fn foo() -> Result<(), wast::Error> {
//! let wat = "(module (func))";
//! let buf = ParseBuffer::new(wat)?;
//! let module = parser::parse::<Wat>(&buf)?;
//! # Ok(())
//! # }
//! ```
//!
//! and you can also define your own new syntax with the
//! [`Parse`](crate::parser::Parse) trait:
//!
//! ```
//! use wast::kw;
//! use wast::core::{Import, Func};
//! use wast::parser::{Parser, Parse, Result};
//!
//! // Fields of a WebAssembly which only allow imports and functions, and all
//! // imports must come before all the functions
//! struct OnlyImportsAndFunctions<'a> {
//!     imports: Vec<Import<'a>>,
//!     functions: Vec<Func<'a>>,
//! }
//!
//! impl<'a> Parse<'a> for OnlyImportsAndFunctions<'a> {
//!     fn parse(parser: Parser<'a>) -> Result<Self> {
//!         // While the second token is `import` (the first is `(`, so we care
//!         // about the second) we parse an `ast::ModuleImport` inside of
//!         // parentheses. The `parens` function here ensures that what we
//!         // parse inside of it is surrounded by `(` and `)`.
//!         let mut imports = Vec::new();
//!         while parser.peek2::<kw::import>()? {
//!             let import = parser.parens(|p| p.parse())?;
//!             imports.push(import);
//!         }
//!
//!         // Afterwards we assume everything else is a function. Note that
//!         // `parse` here is a generic function and type inference figures out
//!         // that we're parsing functions here and imports above.
//!         let mut functions = Vec::new();
//!         while !parser.is_empty() {
//!             let func = parser.parens(|p| p.parse())?;
//!             functions.push(func);
//!         }
//!
//!         Ok(OnlyImportsAndFunctions { imports, functions })
//!     }
//! }
//! ```
//!
//! This module is heavily inspired by [`syn`](https://docs.rs/syn) so you can
//! likely also draw inspiration from the excellent examples in the `syn` crate.

use crate::lexer::{Float, Integer, Lexer, Token, TokenKind};
use crate::token::Span;
use crate::Error;
use std::borrow::Cow;
use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::fmt;
use std::usize;

/// The maximum recursive depth of parens to parse.
///
/// This is sort of a fundamental limitation of the way this crate is
/// designed. Everything is done through recursive descent parsing which
/// means, well, that we're recursively going down the stack as we parse
/// nested data structures. While we can handle this for wasm expressions
/// since that's a pretty local decision, handling this for nested
/// modules/components which be far trickier. For now we just say that when
/// the parser goes too deep we return an error saying there's too many
/// nested items. It would be great to not return an error here, though!
pub(crate) const MAX_PARENS_DEPTH: usize = 100;

/// A top-level convenience parsing function that parses a `T` from `buf` and
/// requires that all tokens in `buf` are consume.
///
/// This generic parsing function can be used to parse any `T` implementing the
/// [`Parse`] trait. It is not used from [`Parse`] trait implementations.
///
/// # Examples
///
/// ```
/// use wast::Wat;
/// use wast::parser::{self, ParseBuffer};
///
/// # fn foo() -> Result<(), wast::Error> {
/// let wat = "(module (func))";
/// let buf = ParseBuffer::new(wat)?;
/// let module = parser::parse::<Wat>(&buf)?;
/// # Ok(())
/// # }
/// ```
///
/// or parsing simply a fragment
///
/// ```
/// use wast::parser::{self, ParseBuffer};
///
/// # fn foo() -> Result<(), wast::Error> {
/// let wat = "12";
/// let buf = ParseBuffer::new(wat)?;
/// let val = parser::parse::<u32>(&buf)?;
/// assert_eq!(val, 12);
/// # Ok(())
/// # }
/// ```
pub fn parse<'a, T: Parse<'a>>(buf: &'a ParseBuffer<'a>) -> Result<T> {
    let parser = buf.parser();
    let result = parser.parse()?;
    if parser.cursor().token()?.is_none() {
        Ok(result)
    } else {
        Err(parser.error("extra tokens remaining after parse"))
    }
}

/// A trait for parsing a fragment of syntax in a recursive descent fashion.
///
/// The [`Parse`] trait is main abstraction you'll be working with when defining
/// custom parser or custom syntax for your WebAssembly text format (or when
/// using the official format items). Almost all items in the
/// [`core`](crate::core) module implement the [`Parse`] trait, and you'll
/// commonly use this with:
///
/// * The top-level [`parse`] function to parse an entire input.
/// * The intermediate [`Parser::parse`] function to parse an item out of an
///   input stream and then parse remaining items.
///
/// Implementation of [`Parse`] take a [`Parser`] as input and will mutate the
/// parser as they parse syntax. Once a token is consume it cannot be
/// "un-consumed". Utilities such as [`Parser::peek`] and [`Parser::lookahead1`]
/// can be used to determine what to parse next.
///
/// ## When to parse `(` and `)`?
///
/// Conventionally types are not responsible for parsing their own `(` and `)`
/// tokens which surround the type. For example WebAssembly imports look like:
///
/// ```text
/// (import "foo" "bar" (func (type 0)))
/// ```
///
/// but the [`Import`](crate::core::Import) type parser looks like:
///
/// ```
/// # use wast::kw;
/// # use wast::parser::{Parser, Parse, Result};
/// # struct Import<'a>(&'a str);
/// impl<'a> Parse<'a> for Import<'a> {
///     fn parse(parser: Parser<'a>) -> Result<Self> {
///         parser.parse::<kw::import>()?;
///         // ...
/// # panic!()
///     }
/// }
/// ```
///
/// It is assumed here that the `(` and `)` tokens which surround an `import`
/// statement in the WebAssembly text format are parsed by the parent item
/// parsing `Import`.
///
/// Note that this is just a convention, so it's not necessarily required for
/// all types. It's recommended that your types stick to this convention where
/// possible to avoid nested calls to [`Parser::parens`] or accidentally trying
/// to parse too many parenthesis.
///
/// # Examples
///
/// Let's say you want to define your own WebAssembly text format which only
/// contains imports and functions. You also require all imports to be listed
/// before all functions. An example [`Parse`] implementation might look like:
///
/// ```
/// use wast::core::{Import, Func};
/// use wast::kw;
/// use wast::parser::{Parser, Parse, Result};
///
/// // Fields of a WebAssembly which only allow imports and functions, and all
/// // imports must come before all the functions
/// struct OnlyImportsAndFunctions<'a> {
///     imports: Vec<Import<'a>>,
///     functions: Vec<Func<'a>>,
/// }
///
/// impl<'a> Parse<'a> for OnlyImportsAndFunctions<'a> {
///     fn parse(parser: Parser<'a>) -> Result<Self> {
///         // While the second token is `import` (the first is `(`, so we care
///         // about the second) we parse an `ast::ModuleImport` inside of
///         // parentheses. The `parens` function here ensures that what we
///         // parse inside of it is surrounded by `(` and `)`.
///         let mut imports = Vec::new();
///         while parser.peek2::<kw::import>()? {
///             let import = parser.parens(|p| p.parse())?;
///             imports.push(import);
///         }
///
///         // Afterwards we assume everything else is a function. Note that
///         // `parse` here is a generic function and type inference figures out
///         // that we're parsing functions here and imports above.
///         let mut functions = Vec::new();
///         while !parser.is_empty() {
///             let func = parser.parens(|p| p.parse())?;
///             functions.push(func);
///         }
///
///         Ok(OnlyImportsAndFunctions { imports, functions })
///     }
/// }
/// ```
pub trait Parse<'a>: Sized {
    /// Attempts to parse `Self` from `parser`, returning an error if it could
    /// not be parsed.
    ///
    /// This method will mutate the state of `parser` after attempting to parse
    /// an instance of `Self`. If an error happens then it is likely fatal and
    /// there is no guarantee of how many tokens have been consumed from
    /// `parser`.
    ///
    /// As recommended in the documentation of [`Parse`], implementations of
    /// this function should not start out by parsing `(` and `)` tokens, but
    /// rather parents calling recursive parsers should parse the `(` and `)`
    /// tokens for their child item that's being parsed.
    ///
    /// # Errors
    ///
    /// This function will return an error if `Self` could not be parsed. Note
    /// that creating an [`Error`] is not exactly a cheap operation, so
    /// [`Error`] is typically fatal and propagated all the way back to the top
    /// parse call site.
    fn parse(parser: Parser<'a>) -> Result<Self>;
}

impl<'a, T> Parse<'a> for Box<T>
where
    T: Parse<'a>,
{
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(Box::new(parser.parse()?))
    }
}

/// A trait for types which be used to "peek" to see if they're the next token
/// in an input stream of [`Parser`].
///
/// Often when implementing [`Parse`] you'll need to query what the next token
/// in the stream is to figure out what to parse next. This [`Peek`] trait
/// defines the set of types that can be tested whether they're the next token
/// in the input stream.
///
/// Implementations of [`Peek`] should only be present on types that consume
/// exactly one token (not zero, not more, exactly one). Types implementing
/// [`Peek`] should also typically implement [`Parse`] should also typically
/// implement [`Parse`].
///
/// See the documentation of [`Parser::peek`] for example usage.
pub trait Peek {
    /// Tests to see whether this token is the first token within the [`Cursor`]
    /// specified.
    ///
    /// Returns `true` if [`Parse`] for this type is highly likely to succeed
    /// failing no other error conditions happening (like an integer literal
    /// being too big).
    fn peek(cursor: Cursor<'_>) -> Result<bool>;

    /// The same as `peek`, except it checks the token immediately following
    /// the current token.
    fn peek2(mut cursor: Cursor<'_>) -> Result<bool> {
        match cursor.token()? {
            Some(token) => cursor.advance_past(&token),
            None => return Ok(false),
        }
        Self::peek(cursor)
    }

    /// Returns a human-readable name of this token to display when generating
    /// errors about this token missing.
    fn display() -> &'static str;
}

/// A convenience type definition for `Result` where the error is hardwired to
/// [`Error`].
pub type Result<T, E = Error> = std::result::Result<T, E>;

/// A low-level buffer of tokens which represents a completely lexed file.
///
/// A `ParseBuffer` will immediately lex an entire file and then store all
/// tokens internally. A `ParseBuffer` only used to pass to the top-level
/// [`parse`] function.
pub struct ParseBuffer<'a> {
    lexer: Lexer<'a>,
    cur: Cell<Position>,
    known_annotations: RefCell<HashMap<String, usize>>,
    depth: Cell<usize>,
    strings: RefCell<Vec<Box<[u8]>>>,
}

/// The current position within a `Lexer` that we're at. This simultaneously
/// stores the byte position that the lexer was last positioned at as well as
/// the next significant token.
///
/// Note that "significant" here does not mean that `token` is the next token
/// to be lexed at `offset`. Instead it's the next non-whitespace,
/// non-annotation, non-coment token. This simple cache-of-sorts avoids
/// re-parsing tokens the majority of the time, or at least that's the
/// intention.
///
/// If `token` is set to `None` then it means that either it hasn't been
/// calculated at or the lexer is at EOF. Basically it means go talk to the
/// lexer.
#[derive(Copy, Clone)]
struct Position {
    offset: usize,
    token: Option<Token>,
}

/// An in-progress parser for the tokens of a WebAssembly text file.
///
/// A `Parser` is argument to the [`Parse`] trait and is now the input stream is
/// interacted with to parse new items. Cloning [`Parser`] or copying a parser
/// refers to the same stream of tokens to parse, you cannot clone a [`Parser`]
/// and clone two items.
///
/// For more information about a [`Parser`] see its methods.
#[derive(Copy, Clone)]
pub struct Parser<'a> {
    buf: &'a ParseBuffer<'a>,
}

/// A helpful structure to perform a lookahead of one token to determine what to
/// parse.
///
/// For more information see the [`Parser::lookahead1`] method.
pub struct Lookahead1<'a> {
    parser: Parser<'a>,
    attempts: Vec<&'static str>,
}

/// An immutable cursor into a list of tokens.
///
/// This cursor cannot be mutated but can be used to parse more tokens in a list
/// of tokens. Cursors are created from the [`Parser::step`] method. This is a
/// very low-level parsing structure and you likely won't use it much.
#[derive(Copy, Clone)]
pub struct Cursor<'a> {
    parser: Parser<'a>,
    pos: Position,
}

impl ParseBuffer<'_> {
    /// Creates a new [`ParseBuffer`] by lexing the given `input` completely.
    ///
    /// # Errors
    ///
    /// Returns an error if `input` fails to lex.
    pub fn new(input: &str) -> Result<ParseBuffer<'_>> {
        ParseBuffer::new_with_lexer(Lexer::new(input))
    }

    /// Creates a new [`ParseBuffer`] by lexing the given `input` completely.
    ///
    /// # Errors
    ///
    /// Returns an error if `input` fails to lex.
    pub fn new_with_lexer(lexer: Lexer<'_>) -> Result<ParseBuffer<'_>> {
        Ok(ParseBuffer {
            lexer,
            depth: Cell::new(0),
            cur: Cell::new(Position {
                offset: 0,
                token: None,
            }),
            known_annotations: Default::default(),
            strings: Default::default(),
        })
    }

    fn parser(&self) -> Parser<'_> {
        Parser { buf: self }
    }

    /// Stores an owned allocation in this `Parser` to attach the lifetime of
    /// the vector to `self`.
    ///
    /// This will return a reference to `s`, but one that's safely rooted in the
    /// `Parser`.
    fn push_str(&self, s: Vec<u8>) -> &[u8] {
        let s = Box::from(s);
        let ret = &*s as *const [u8];
        self.strings.borrow_mut().push(s);
        // This should be safe in that the address of `ret` isn't changing as
        // it's on the heap itself. Additionally the lifetime of this return
        // value is tied to the lifetime of `self` (nothing is deallocated
        // early), so it should be safe to say the two have the same lifetime.
        unsafe { &*ret }
    }

    /// Lexes the next "significant" token from the `pos` specified.
    ///
    /// This will skip irrelevant tokens such as whitespace, comments, and
    /// unknown annotations.
    fn advance_token(&self, mut pos: usize) -> Result<Option<Token>> {
        let token = loop {
            let token = match self.lexer.parse(&mut pos)? {
                Some(token) => token,
                None => return Ok(None),
            };
            match token.kind {
                // Always skip whitespace and comments.
                TokenKind::Whitespace | TokenKind::LineComment | TokenKind::BlockComment => {
                    continue
                }

                // If an lparen is seen then this may be skipped if it's an
                // annotation of the form `(@foo ...)`. In this situation
                // everything up to and including the closing rparen is skipped.
                //
                // Note that the annotation is only skipped if it's an unknown
                // annotation as known annotations are specifically registered
                // as "someone's gonna parse this".
                TokenKind::LParen => {
                    if let Some(annotation) = self.lexer.annotation(pos) {
                        match self.known_annotations.borrow().get(annotation) {
                            Some(0) | None => {
                                self.skip_annotation(&mut pos)?;
                                continue;
                            }
                            Some(_) => {}
                        }
                    }
                    break token;
                }
                _ => break token,
            }
        };
        Ok(Some(token))
    }

    fn skip_annotation(&self, pos: &mut usize) -> Result<()> {
        let mut depth = 1;
        let span = Span { offset: *pos };
        loop {
            let token = match self.lexer.parse(pos)? {
                Some(token) => token,
                None => {
                    break Err(Error::new(span, "unclosed annotation".to_string()));
                }
            };
            match token.kind {
                TokenKind::LParen => depth += 1,
                TokenKind::RParen => {
                    depth -= 1;
                    if depth == 0 {
                        break Ok(());
                    }
                }
                _ => {}
            }
        }
    }
}

impl<'a> Parser<'a> {
    /// Returns whether there are no more `Token` tokens to parse from this
    /// [`Parser`].
    ///
    /// This indicates that either we've reached the end of the input, or we're
    /// a sub-[`Parser`] inside of a parenthesized expression and we've hit the
    /// `)` token.
    ///
    /// Note that if `false` is returned there *may* be more comments. Comments
    /// and whitespace are not considered for whether this parser is empty.
    pub fn is_empty(self) -> bool {
        match self.cursor().token() {
            Ok(Some(token)) => matches!(token.kind, TokenKind::RParen),
            Ok(None) => true,
            Err(_) => false,
        }
    }

    pub(crate) fn has_meaningful_tokens(self) -> bool {
        self.buf.lexer.iter(0).any(|t| match t {
            Ok(token) => !matches!(
                token.kind,
                TokenKind::Whitespace | TokenKind::LineComment | TokenKind::BlockComment
            ),
            Err(_) => true,
        })
    }

    /// Parses a `T` from this [`Parser`].
    ///
    /// This method has a trivial definition (it simply calls
    /// [`T::parse`](Parse::parse)) but is here for syntactic purposes. This is
    /// what you'll call 99% of the time in a [`Parse`] implementation in order
    /// to parse sub-items.
    ///
    /// Typically you always want to use `?` with the result of this method, you
    /// should not handle errors and decide what else to parse. To handle
    /// branches in parsing, use [`Parser::peek`].
    ///
    /// # Examples
    ///
    /// A good example of using `parse` is to see how the [`TableType`] type is
    /// parsed in this crate. A [`TableType`] is defined in the official
    /// specification as [`tabletype`][spec] and is defined as:
    ///
    /// [spec]: https://webassembly.github.io/spec/core/text/types.html#table-types
    ///
    /// ```text
    /// tabletype ::= lim:limits et:reftype
    /// ```
    ///
    /// so to parse a [`TableType`] we recursively need to parse a [`Limits`]
    /// and a [`RefType`]
    ///
    /// ```
    /// # use wast::core::*;
    /// # use wast::parser::*;
    /// struct TableType<'a> {
    ///     limits: Limits,
    ///     elem: RefType<'a>,
    /// }
    ///
    /// impl<'a> Parse<'a> for TableType<'a> {
    ///     fn parse(parser: Parser<'a>) -> Result<Self> {
    ///         // parse the `lim` then `et` in sequence
    ///         Ok(TableType {
    ///             limits: parser.parse()?,
    ///             elem: parser.parse()?,
    ///         })
    ///     }
    /// }
    /// ```
    ///
    /// [`Limits`]: crate::core::Limits
    /// [`TableType`]: crate::core::TableType
    /// [`RefType`]: crate::core::RefType
    pub fn parse<T: Parse<'a>>(self) -> Result<T> {
        T::parse(self)
    }

    /// Performs a cheap test to see whether the current token in this stream is
    /// `T`.
    ///
    /// This method can be used to efficiently determine what next to parse. The
    /// [`Peek`] trait is defined for types which can be used to test if they're
    /// the next item in the input stream.
    ///
    /// Nothing is actually parsed in this method, nor does this mutate the
    /// state of this [`Parser`]. Instead, this simply performs a check.
    ///
    /// This method is frequently combined with the [`Parser::lookahead1`]
    /// method to automatically produce nice error messages if some tokens
    /// aren't found.
    ///
    /// # Examples
    ///
    /// For an example of using the `peek` method let's take a look at parsing
    /// the [`Limits`] type. This is [defined in the official spec][spec] as:
    ///
    /// ```text
    /// limits ::= n:u32
    ///          | n:u32 m:u32
    /// ```
    ///
    /// which means that it's either one `u32` token or two, so we need to know
    /// whether to consume two tokens or one:
    ///
    /// ```
    /// # use wast::parser::*;
    /// struct Limits {
    ///     min: u32,
    ///     max: Option<u32>,
    /// }
    ///
    /// impl<'a> Parse<'a> for Limits {
    ///     fn parse(parser: Parser<'a>) -> Result<Self> {
    ///         // Always parse the first number...
    ///         let min = parser.parse()?;
    ///
    ///         // ... and then test if there's a second number before parsing
    ///         let max = if parser.peek::<u32>()? {
    ///             Some(parser.parse()?)
    ///         } else {
    ///             None
    ///         };
    ///
    ///         Ok(Limits { min, max })
    ///     }
    /// }
    /// ```
    ///
    /// [spec]: https://webassembly.github.io/spec/core/text/types.html#limits
    /// [`Limits`]: crate::core::Limits
    pub fn peek<T: Peek>(self) -> Result<bool> {
        T::peek(self.cursor())
    }

    /// Same as the [`Parser::peek`] method, except checks the next token, not
    /// the current token.
    pub fn peek2<T: Peek>(self) -> Result<bool> {
        T::peek2(self.cursor())
    }

    /// Same as the [`Parser::peek2`] method, except checks the next next token,
    /// not the next token.
    pub fn peek3<T: Peek>(self) -> Result<bool> {
        let mut cursor = self.cursor();
        match cursor.token()? {
            Some(token) => cursor.advance_past(&token),
            None => return Ok(false),
        }
        match cursor.token()? {
            Some(token) => cursor.advance_past(&token),
            None => return Ok(false),
        }
        T::peek(cursor)
    }

    /// A helper structure to perform a sequence of `peek` operations and if
    /// they all fail produce a nice error message.
    ///
    /// This method purely exists for conveniently producing error messages and
    /// provides no functionality that [`Parser::peek`] doesn't already give.
    /// The [`Lookahead1`] structure has one main method [`Lookahead1::peek`],
    /// which is the same method as [`Parser::peek`]. The difference is that the
    /// [`Lookahead1::error`] method needs no arguments.
    ///
    /// # Examples
    ///
    /// Let's look at the parsing of [`Index`]. This type is either a `u32` or
    /// an [`Id`] and is used in name resolution primarily. The [official
    /// grammar for an index][spec] is:
    ///
    /// ```text
    /// idx ::= x:u32
    ///       | v:id
    /// ```
    ///
    /// Which is to say that an index is either a `u32` or an [`Id`]. When
    /// parsing an [`Index`] we can do:
    ///
    /// ```
    /// # use wast::token::*;
    /// # use wast::parser::*;
    /// enum Index<'a> {
    ///     Num(u32),
    ///     Id(Id<'a>),
    /// }
    ///
    /// impl<'a> Parse<'a> for Index<'a> {
    ///     fn parse(parser: Parser<'a>) -> Result<Self> {
    ///         let mut l = parser.lookahead1();
    ///         if l.peek::<Id>()? {
    ///             Ok(Index::Id(parser.parse()?))
    ///         } else if l.peek::<u32>()? {
    ///             Ok(Index::Num(parser.parse()?))
    ///         } else {
    ///             // produces error message of `expected identifier or u32`
    ///             Err(l.error())
    ///         }
    ///     }
    /// }
    /// ```
    ///
    /// [spec]: https://webassembly.github.io/spec/core/text/modules.html#indices
    /// [`Index`]: crate::token::Index
    /// [`Id`]: crate::token::Id
    pub fn lookahead1(self) -> Lookahead1<'a> {
        Lookahead1 {
            attempts: Vec::new(),
            parser: self,
        }
    }

    /// Parse an item surrounded by parentheses.
    ///
    /// WebAssembly's text format is all based on s-expressions, so naturally
    /// you're going to want to parse a lot of parenthesized things! As noted in
    /// the documentation of [`Parse`] you typically don't parse your own
    /// surrounding `(` and `)` tokens, but the parser above you parsed them for
    /// you. This is method method the parser above you uses.
    ///
    /// This method will parse a `(` token, and then call `f` on a sub-parser
    /// which when finished asserts that a `)` token is the next token. This
    /// requires that `f` consumes all tokens leading up to the paired `)`.
    ///
    /// Usage will often simply be `parser.parens(|p| p.parse())?` to
    /// automatically parse a type within parentheses, but you can, as always,
    /// go crazy and do whatever you'd like too.
    ///
    /// # Examples
    ///
    /// A good example of this is to see how a `Module` is parsed. This isn't
    /// the exact definition, but it's close enough!
    ///
    /// ```
    /// # use wast::kw;
    /// # use wast::core::*;
    /// # use wast::parser::*;
    /// struct Module<'a> {
    ///     fields: Vec<ModuleField<'a>>,
    /// }
    ///
    /// impl<'a> Parse<'a> for Module<'a> {
    ///     fn parse(parser: Parser<'a>) -> Result<Self> {
    ///         // Modules start out with a `module` keyword
    ///         parser.parse::<kw::module>()?;
    ///
    ///         // And then everything else is `(field ...)`, so while we've got
    ///         // items left we continuously parse parenthesized items.
    ///         let mut fields = Vec::new();
    ///         while !parser.is_empty() {
    ///             fields.push(parser.parens(|p| p.parse())?);
    ///         }
    ///         Ok(Module { fields })
    ///     }
    /// }
    /// ```
    pub fn parens<T>(self, f: impl FnOnce(Parser<'a>) -> Result<T>) -> Result<T> {
        self.buf.depth.set(self.buf.depth.get() + 1);
        let before = self.buf.cur.get();
        let res = self.step(|cursor| {
            let mut cursor = match cursor.lparen()? {
                Some(rest) => rest,
                None => return Err(cursor.error("expected `(`")),
            };
            cursor.parser.buf.cur.set(cursor.pos);
            let result = f(cursor.parser)?;

            // Reset our cursor's state to whatever the current state of the
            // parser is.
            cursor.pos = cursor.parser.buf.cur.get();

            match cursor.rparen()? {
                Some(rest) => Ok((result, rest)),
                None => Err(cursor.error("expected `)`")),
            }
        });
        self.buf.depth.set(self.buf.depth.get() - 1);
        if res.is_err() {
            self.buf.cur.set(before);
        }
        res
    }

    /// Return the depth of nested parens we've parsed so far.
    ///
    /// This is a low-level method that is only useful for implementing
    /// recursion limits in custom parsers.
    pub fn parens_depth(&self) -> usize {
        self.buf.depth.get()
    }

    /// Checks that the parser parens depth hasn't exceeded the maximum depth.
    pub(crate) fn depth_check(&self) -> Result<()> {
        if self.parens_depth() > MAX_PARENS_DEPTH {
            Err(self.error("item nesting too deep"))
        } else {
            Ok(())
        }
    }

    fn cursor(self) -> Cursor<'a> {
        Cursor {
            parser: self,
            pos: self.buf.cur.get(),
        }
    }

    /// A low-level parsing method you probably won't use.
    ///
    /// This is used to implement parsing of the most primitive types in the
    /// [`core`](crate::core) module. You probably don't want to use this, but
    /// probably want to use something like [`Parser::parse`] or
    /// [`Parser::parens`].
    pub fn step<F, T>(self, f: F) -> Result<T>
    where
        F: FnOnce(Cursor<'a>) -> Result<(T, Cursor<'a>)>,
    {
        let (result, cursor) = f(self.cursor())?;
        self.buf.cur.set(cursor.pos);
        Ok(result)
    }

    /// Creates an error whose line/column information is pointing at the
    /// current token.
    ///
    /// This is used to produce human-readable error messages which point to the
    /// right location in the input stream, and the `msg` here is arbitrary text
    /// used to associate with the error and indicate why it was generated.
    pub fn error(self, msg: impl fmt::Display) -> Error {
        self.error_at(self.cursor().cur_span(), msg)
    }

    /// Creates an error whose line/column information is pointing at the
    /// given span.
    pub fn error_at(self, span: Span, msg: impl fmt::Display) -> Error {
        Error::parse(span, self.buf.lexer.input(), msg.to_string())
    }

    /// Returns the span of the current token
    pub fn cur_span(&self) -> Span {
        self.cursor().cur_span()
    }

    /// Returns the span of the previous token
    pub fn prev_span(&self) -> Span {
        self.cursor()
            .prev_span()
            .unwrap_or_else(|| Span::from_offset(0))
    }

    /// Registers a new known annotation with this parser to allow parsing
    /// annotations with this name.
    ///
    /// [WebAssembly annotations][annotation] are a proposal for the text format
    /// which allows decorating the text format with custom structured
    /// information. By default all annotations are ignored when parsing, but
    /// the whole purpose of them is to sometimes parse them!
    ///
    /// To support parsing text annotations this method is used to allow
    /// annotations and their tokens to *not* be skipped. Once an annotation is
    /// registered with this method, then while the return value has not been
    /// dropped (e.g. the scope of where this function is called) annotations
    /// with the name `annotation` will be parse of the token stream and not
    /// implicitly skipped.
    ///
    /// # Skipping annotations
    ///
    /// The behavior of skipping unknown/unregistered annotations can be
    /// somewhat subtle and surprising, so if you're interested in parsing
    /// annotations it's important to point out the importance of this method
    /// and where to call it.
    ///
    /// Generally when parsing tokens you'll be bottoming out in various
    /// `Cursor` methods. These are all documented as advancing the stream as
    /// much as possible to the next token, skipping "irrelevant stuff" like
    /// comments, whitespace, etc. The `Cursor` methods will also skip unknown
    /// annotations. This means that if you parse *any* token, it will skip over
    /// any number of annotations that are unknown at all times.
    ///
    /// To parse an annotation you must, before parsing any token of the
    /// annotation, register the annotation via this method. This includes the
    /// beginning `(` token, which is otherwise skipped if the annotation isn't
    /// marked as registered. Typically parser parse the *contents* of an
    /// s-expression, so this means that the outer parser of an s-expression
    /// must register the custom annotation name, rather than the inner parser.
    ///
    /// # Return
    ///
    /// This function returns an RAII guard which, when dropped, will unregister
    /// the `annotation` given. Parsing `annotation` is only supported while the
    /// returned value is still alive, and once dropped the parser will go back
    /// to skipping annotations with the name `annotation`.
    ///
    /// # Example
    ///
    /// Let's see an example of how the `@name` annotation is parsed for modules
    /// to get an idea of how this works:
    ///
    /// ```
    /// # use wast::kw;
    /// # use wast::token::NameAnnotation;
    /// # use wast::parser::*;
    /// struct Module<'a> {
    ///     name: Option<NameAnnotation<'a>>,
    /// }
    ///
    /// impl<'a> Parse<'a> for Module<'a> {
    ///     fn parse(parser: Parser<'a>) -> Result<Self> {
    ///         // Modules start out with a `module` keyword
    ///         parser.parse::<kw::module>()?;
    ///
    ///         // Next may be `(@name "foo")`. Typically this annotation would
    ///         // skipped, but we don't want it skipped, so we register it.
    ///         // Note that the parse implementation of
    ///         // `Option<NameAnnotation>` is the one that consumes the
    ///         // parentheses here.
    ///         let _r = parser.register_annotation("name");
    ///         let name = parser.parse()?;
    ///
    ///         // ... and normally you'd otherwise parse module fields here ...
    ///
    ///         Ok(Module { name })
    ///     }
    /// }
    /// ```
    ///
    /// Another example is how we parse the `@custom` annotation. Note that this
    /// is parsed as part of `ModuleField`, so note how the annotation is
    /// registered *before* we parse the parentheses of the annotation.
    ///
    /// ```
    /// # use wast::{kw, annotation};
    /// # use wast::core::Custom;
    /// # use wast::parser::*;
    /// struct Module<'a> {
    ///     fields: Vec<ModuleField<'a>>,
    /// }
    ///
    /// impl<'a> Parse<'a> for Module<'a> {
    ///     fn parse(parser: Parser<'a>) -> Result<Self> {
    ///         // Modules start out with a `module` keyword
    ///         parser.parse::<kw::module>()?;
    ///
    ///         // register the `@custom` annotation *first* before we start
    ///         // parsing fields, because each field is contained in
    ///         // parentheses and to parse the parentheses of an annotation we
    ///         // have to known to not skip it.
    ///         let _r = parser.register_annotation("custom");
    ///
    ///         let mut fields = Vec::new();
    ///         while !parser.is_empty() {
    ///             fields.push(parser.parens(|p| p.parse())?);
    ///         }
    ///         Ok(Module { fields })
    ///     }
    /// }
    ///
    /// enum ModuleField<'a> {
    ///     Custom(Custom<'a>),
    ///     // ...
    /// }
    ///
    /// impl<'a> Parse<'a> for ModuleField<'a> {
    ///     fn parse(parser: Parser<'a>) -> Result<Self> {
    ///         // Note that because we have previously registered the `@custom`
    ///         // annotation with the parser we known that `peek` methods like
    ///         // this, working on the annotation token, are enabled to ever
    ///         // return `true`.
    ///         if parser.peek::<annotation::custom>()? {
    ///             return Ok(ModuleField::Custom(parser.parse()?));
    ///         }
    ///
    ///         // .. typically we'd parse other module fields here...
    ///
    ///         Err(parser.error("unknown module field"))
    ///     }
    /// }
    /// ```
    ///
    /// [annotation]: https://github.com/WebAssembly/annotations
    pub fn register_annotation<'b>(self, annotation: &'b str) -> impl Drop + 'b
    where
        'a: 'b,
    {
        let mut annotations = self.buf.known_annotations.borrow_mut();
        if !annotations.contains_key(annotation) {
            annotations.insert(annotation.to_string(), 0);
        }
        *annotations.get_mut(annotation).unwrap() += 1;

        return RemoveOnDrop(self, annotation);

        struct RemoveOnDrop<'a>(Parser<'a>, &'a str);

        impl Drop for RemoveOnDrop<'_> {
            fn drop(&mut self) {
                let mut annotations = self.0.buf.known_annotations.borrow_mut();
                let slot = annotations.get_mut(self.1).unwrap();
                *slot -= 1;
            }
        }
    }
}

impl<'a> Cursor<'a> {
    /// Returns the span of the next `Token` token.
    ///
    /// Does not take into account whitespace or comments.
    pub fn cur_span(&self) -> Span {
        let offset = match self.token() {
            Ok(Some(t)) => t.offset,
            Ok(None) => self.parser.buf.lexer.input().len(),
            Err(_) => self.pos.offset,
        };
        Span { offset }
    }

    /// Returns the span of the previous `Token` token.
    ///
    /// Does not take into account whitespace or comments.
    pub(crate) fn prev_span(&self) -> Option<Span> {
        // TODO
        Some(Span {
            offset: self.pos.offset,
        })
        // let (token, _) = self.parser.buf.tokens.get(self.cur.checked_sub(1)?)?;
        // Some(Span {
        //     offset: token.offset,
        // })
    }

    /// Same as [`Parser::error`], but works with the current token in this
    /// [`Cursor`] instead.
    pub fn error(&self, msg: impl fmt::Display) -> Error {
        self.parser.error_at(self.cur_span(), msg)
    }

    /// Tests whether the next token is an lparen
    pub fn peek_lparen(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::LParen,
                ..
            })
        ))
    }

    /// Tests whether the next token is an rparen
    pub fn peek_rparen(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::RParen,
                ..
            })
        ))
    }

    /// Tests whether the next token is an id
    pub fn peek_id(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::Id,
                ..
            })
        ))
    }

    /// Tests whether the next token is reserved
    pub fn peek_reserved(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::Reserved,
                ..
            })
        ))
    }

    /// Tests whether the next token is a keyword
    pub fn peek_keyword(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::Keyword,
                ..
            })
        ))
    }

    /// Tests whether the next token is an integer
    pub fn peek_integer(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::Integer(_),
                ..
            })
        ))
    }

    /// Tests whether the next token is a float
    pub fn peek_float(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::Float(_),
                ..
            })
        ))
    }

    /// Tests whether the next token is a string
    pub fn peek_string(self) -> Result<bool> {
        Ok(matches!(
            self.token()?,
            Some(Token {
                kind: TokenKind::String,
                ..
            })
        ))
    }

    /// Attempts to advance this cursor if the current token is a `(`.
    ///
    /// If the current token is `(`, returns a new [`Cursor`] pointing at the
    /// rest of the tokens in the stream. Otherwise returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn lparen(mut self) -> Result<Option<Self>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        match token.kind {
            TokenKind::LParen => {}
            _ => return Ok(None),
        }
        self.advance_past(&token);
        Ok(Some(self))
    }

    /// Attempts to advance this cursor if the current token is a `)`.
    ///
    /// If the current token is `)`, returns a new [`Cursor`] pointing at the
    /// rest of the tokens in the stream. Otherwise returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn rparen(mut self) -> Result<Option<Self>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        match token.kind {
            TokenKind::RParen => {}
            _ => return Ok(None),
        }
        self.advance_past(&token);
        Ok(Some(self))
    }

    /// Attempts to advance this cursor if the current token is a
    /// [`Token::Id`](crate::lexer::Token)
    ///
    /// If the current token is `Id`, returns the identifier minus the leading
    /// `$` character as well as a new [`Cursor`] pointing at the rest of the
    /// tokens in the stream. Otherwise returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn id(mut self) -> Result<Option<(&'a str, Self)>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        match token.kind {
            TokenKind::Id => {}
            _ => return Ok(None),
        }
        self.advance_past(&token);
        Ok(Some((token.id(self.parser.buf.lexer.input()), self)))
    }

    /// Attempts to advance this cursor if the current token is a
    /// [`Token::Keyword`](crate::lexer::Token)
    ///
    /// If the current token is `Keyword`, returns the keyword as well as a new
    /// [`Cursor`] pointing at the rest of the tokens in the stream. Otherwise
    /// returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn keyword(mut self) -> Result<Option<(&'a str, Self)>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        match token.kind {
            TokenKind::Keyword => {}
            _ => return Ok(None),
        }
        self.advance_past(&token);
        Ok(Some((token.keyword(self.parser.buf.lexer.input()), self)))
    }

    /// Attempts to advance this cursor if the current token is a
    /// [`Token::Reserved`](crate::lexer::Token)
    ///
    /// If the current token is `Reserved`, returns the reserved token as well
    /// as a new [`Cursor`] pointing at the rest of the tokens in the stream.
    /// Otherwise returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn reserved(mut self) -> Result<Option<(&'a str, Self)>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        match token.kind {
            TokenKind::Reserved => {}
            _ => return Ok(None),
        }
        self.advance_past(&token);
        Ok(Some((token.reserved(self.parser.buf.lexer.input()), self)))
    }

    /// Attempts to advance this cursor if the current token is a
    /// [`Token::Integer`](crate::lexer::Token)
    ///
    /// If the current token is `Integer`, returns the integer as well as a new
    /// [`Cursor`] pointing at the rest of the tokens in the stream. Otherwise
    /// returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn integer(mut self) -> Result<Option<(Integer<'a>, Self)>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        let i = match token.kind {
            TokenKind::Integer(i) => i,
            _ => return Ok(None),
        };
        self.advance_past(&token);
        Ok(Some((
            token.integer(self.parser.buf.lexer.input(), i),
            self,
        )))
    }

    /// Attempts to advance this cursor if the current token is a
    /// [`Token::Float`](crate::lexer::Token)
    ///
    /// If the current token is `Float`, returns the float as well as a new
    /// [`Cursor`] pointing at the rest of the tokens in the stream. Otherwise
    /// returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn float(mut self) -> Result<Option<(Float<'a>, Self)>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        let f = match token.kind {
            TokenKind::Float(f) => f,
            _ => return Ok(None),
        };
        self.advance_past(&token);
        Ok(Some((token.float(self.parser.buf.lexer.input(), f), self)))
    }

    /// Attempts to advance this cursor if the current token is a
    /// [`Token::String`](crate::lexer::Token)
    ///
    /// If the current token is `String`, returns the byte value of the string
    /// as well as a new [`Cursor`] pointing at the rest of the tokens in the
    /// stream. Otherwise returns `None`.
    ///
    /// This function will automatically skip over any comments, whitespace, or
    /// unknown annotations.
    pub fn string(mut self) -> Result<Option<(&'a [u8], Self)>> {
        let token = match self.token()? {
            Some(token) => token,
            None => return Ok(None),
        };
        match token.kind {
            TokenKind::String => {}
            _ => return Ok(None),
        }
        let string = match token.string(self.parser.buf.lexer.input()) {
            Cow::Borrowed(s) => s,
            Cow::Owned(s) => self.parser.buf.push_str(s),
        };
        self.advance_past(&token);
        Ok(Some((string, self)))
    }

    /// Attempts to advance this cursor if the current token is a
    /// [`Token::LineComment`](crate::lexer::Token) or a
    /// [`Token::BlockComment`](crate::lexer::Token)
    ///
    /// This function will only skip whitespace, no other tokens.
    pub fn comment(mut self) -> Result<Option<(&'a str, Self)>> {
        let start = self.pos.offset;
        self.pos.token = None;
        let comment = loop {
            let token = match self.parser.buf.lexer.parse(&mut self.pos.offset)? {
                Some(token) => token,
                None => return Ok(None),
            };
            match token.kind {
                TokenKind::LineComment | TokenKind::BlockComment => {
                    break token.src(self.parser.buf.lexer.input());
                }
                TokenKind::Whitespace => {}
                _ => {
                    self.pos.offset = start;
                    return Ok(None);
                }
            }
        };
        Ok(Some((comment, self)))
    }

    fn token(&self) -> Result<Option<Token>> {
        match self.pos.token {
            Some(token) => Ok(Some(token)),
            None => self.parser.buf.advance_token(self.pos.offset),
        }
    }

    fn advance_past(&mut self, token: &Token) {
        self.pos.offset = token.offset + (token.len as usize);
        self.pos.token = self
            .parser
            .buf
            .advance_token(self.pos.offset)
            .unwrap_or(None);
    }
}

impl Lookahead1<'_> {
    /// Attempts to see if `T` is the next token in the [`Parser`] this
    /// [`Lookahead1`] references.
    ///
    /// For more information see [`Parser::lookahead1`] and [`Parser::peek`]
    pub fn peek<T: Peek>(&mut self) -> Result<bool> {
        Ok(if self.parser.peek::<T>()? {
            true
        } else {
            self.attempts.push(T::display());
            false
        })
    }

    /// Generates an error message saying that one of the tokens passed to
    /// [`Lookahead1::peek`] method was expected.
    ///
    /// Before calling this method you should call [`Lookahead1::peek`] for all
    /// possible tokens you'd like to parse.
    pub fn error(self) -> Error {
        match self.attempts.len() {
            0 => {
                if self.parser.is_empty() {
                    self.parser.error("unexpected end of input")
                } else {
                    self.parser.error("unexpected token")
                }
            }
            1 => {
                let message = format!("unexpected token, expected {}", self.attempts[0]);
                self.parser.error(&message)
            }
            2 => {
                let message = format!(
                    "unexpected token, expected {} or {}",
                    self.attempts[0], self.attempts[1]
                );
                self.parser.error(&message)
            }
            _ => {
                let join = self.attempts.join(", ");
                let message = format!("unexpected token, expected one of: {}", join);
                self.parser.error(&message)
            }
        }
    }
}

impl<'a, T: Peek + Parse<'a>> Parse<'a> for Option<T> {
    fn parse(parser: Parser<'a>) -> Result<Option<T>> {
        if parser.peek::<T>()? {
            Ok(Some(parser.parse()?))
        } else {
            Ok(None)
        }
    }
}
