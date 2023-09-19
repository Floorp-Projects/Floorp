//! Definition of a lexer for the WebAssembly text format.
//!
//! This module provides a [`Lexer`][] type which is an iterate over the raw
//! tokens of a WebAssembly text file. A [`Lexer`][] accounts for every single
//! byte in a WebAssembly text field, returning tokens even for comments and
//! whitespace. Typically you'll ignore comments and whitespace, however.
//!
//! If you'd like to iterate over the tokens in a file you can do so via:
//!
//! ```
//! # fn foo() -> Result<(), wast::Error> {
//! use wast::lexer::Lexer;
//!
//! let wat = "(module (func $foo))";
//! for token in Lexer::new(wat).iter(0) {
//!     println!("{:?}", token?);
//! }
//! # Ok(())
//! # }
//! ```
//!
//! Note that you'll typically not use this module but will rather use
//! [`ParseBuffer`](crate::parser::ParseBuffer) instead.
//!
//! [`Lexer`]: crate::lexer::Lexer

use crate::token::Span;
use crate::Error;
use std::borrow::Cow;
use std::char;
use std::fmt;
use std::slice;
use std::str;

/// A structure used to lex the s-expression syntax of WAT files.
///
/// This structure is used to generate [`Token`] items, which should account for
/// every single byte of the input as we iterate over it. A [`LexError`] is
/// returned for any non-lexable text.
#[derive(Clone)]
pub struct Lexer<'a> {
    input: &'a str,
    allow_confusing_unicode: bool,
}

/// A single token parsed from a `Lexer`.
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct Token {
    /// The kind of token this represents, such as whether it's whitespace, a
    /// keyword, etc.
    pub kind: TokenKind,
    /// The byte offset within the original source for where this token came
    /// from.
    pub offset: usize,
    /// The byte length of this token as it resides in the original source.
    //
    // NB: this is `u32` to enable packing `Token` into two pointers of size.
    // This does limit a single token to being at most 4G large, but that seems
    // probably ok.
    pub len: u32,
}

const _: () = {
    assert!(std::mem::size_of::<Token>() <= std::mem::size_of::<u64>() * 2);
};

/// Classification of what was parsed from the input stream.
///
/// This enumeration contains all kinds of fragments, including comments and
/// whitespace.
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum TokenKind {
    /// A line comment, preceded with `;;`
    LineComment,

    /// A block comment, surrounded by `(;` and `;)`. Note that these can be
    /// nested.
    BlockComment,

    /// A fragment of source that represents whitespace.
    Whitespace,

    /// A left-parenthesis, including the source text for where it comes from.
    LParen,
    /// A right-parenthesis, including the source text for where it comes from.
    RParen,

    /// A string literal, which is actually a list of bytes.
    String,

    /// An identifier (like `$foo`).
    ///
    /// All identifiers start with `$` and the payload here is the original
    /// source text.
    Id,

    /// A keyword, or something that starts with an alphabetic character.
    ///
    /// The payload here is the original source text.
    Keyword,

    /// A reserved series of `idchar` symbols. Unknown what this is meant to be
    /// used for, you'll probably generate an error about an unexpected token.
    Reserved,

    /// An integer.
    Integer(IntegerKind),

    /// A float.
    Float(FloatKind),
}

/// Description of the parsed integer from the source.
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct IntegerKind {
    sign: Option<SignToken>,
    has_underscores: bool,
    hex: bool,
}

/// Description of a parsed float from the source.
#[allow(missing_docs)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum FloatKind {
    #[doc(hidden)]
    Inf { negative: bool },
    #[doc(hidden)]
    Nan { negative: bool },
    #[doc(hidden)]
    NanVal {
        negative: bool,
        has_underscores: bool,
    },
    #[doc(hidden)]
    Normal { has_underscores: bool, hex: bool },
}

enum ReservedKind {
    String,
    Idchars,
    Reserved,
}

/// Errors that can be generated while lexing.
///
/// All lexing errors have line/colum/position information as well as a
/// `LexError` indicating what kind of error happened while lexing.
#[derive(Debug, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum LexError {
    /// A dangling block comment was found with an unbalanced `(;` which was
    /// never terminated in the file.
    DanglingBlockComment,

    /// An unexpected character was encountered when generally parsing and
    /// looking for something else.
    Unexpected(char),

    /// An invalid `char` in a string literal was found.
    InvalidStringElement(char),

    /// An invalid string escape letter was found (the thing after the `\` in
    /// string literals)
    InvalidStringEscape(char),

    /// An invalid hexadecimal digit was found.
    InvalidHexDigit(char),

    /// An invalid base-10 digit was found.
    InvalidDigit(char),

    /// Parsing expected `wanted` but ended up finding `found` instead where the
    /// two characters aren't the same.
    Expected {
        /// The character that was expected to be found
        wanted: char,
        /// The character that was actually found
        found: char,
    },

    /// We needed to parse more but EOF (or end of the string) was encountered.
    UnexpectedEof,

    /// A number failed to parse because it was too big to fit within the target
    /// type.
    NumberTooBig,

    /// An invalid unicode value was found in a `\u{...}` escape in a string,
    /// only valid unicode scalars can be escaped that way.
    InvalidUnicodeValue(u32),

    /// A lone underscore was found when parsing a number, since underscores
    /// should always be preceded and succeeded with a digit of some form.
    LoneUnderscore,

    /// A "confusing" unicode character is present in a comment or a string
    /// literal, such as a character that changes the direction text is
    /// typically displayed in editors. This could cause the human-read
    /// version to behave differently than the compiler-visible version, so
    /// these are simply rejected for now.
    ConfusingUnicode(char),
}

/// A sign token for an integer.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SignToken {
    /// Plus sign: "+",
    Plus,
    /// Minus sign: "-",
    Minus,
}

/// A fully parsed integer from a source string with a payload ready to parse
/// into an integral type.
#[derive(Debug, PartialEq)]
pub struct Integer<'a> {
    sign: Option<SignToken>,
    val: Cow<'a, str>,
    hex: bool,
}

/// Possible parsed float values
#[derive(Debug, PartialEq, Eq)]
pub enum Float<'a> {
    /// A float `NaN` representation
    Nan {
        /// The specific bits to encode for this float, optionally
        val: Option<Cow<'a, str>>,
        /// Whether or not this is a negative `NaN` or not.
        negative: bool,
    },
    /// An float infinite representation,
    Inf {
        #[allow(missing_docs)]
        negative: bool,
    },
    /// A parsed and separated floating point value
    Val {
        /// Whether or not the `integral` and `decimal` are specified in hex
        hex: bool,
        /// The float parts before the `.`
        integral: Cow<'a, str>,
        /// The float parts after the `.`
        decimal: Option<Cow<'a, str>>,
        /// The exponent to multiple this `integral.decimal` portion of the
        /// float by. If `hex` is true this is `2^exponent` and otherwise it's
        /// `10^exponent`
        exponent: Option<Cow<'a, str>>,
    },
}

// https://webassembly.github.io/spec/core/text/values.html#text-idchar
macro_rules! idchars {
    () => {
        b'0'..=b'9'
        | b'A'..=b'Z'
        | b'a'..=b'z'
        | b'!'
        | b'#'
        | b'$'
        | b'%'
        | b'&'
        | b'\''
        | b'*'
        | b'+'
        | b'-'
        | b'.'
        | b'/'
        | b':'
        | b'<'
        | b'='
        | b'>'
        | b'?'
        | b'@'
        | b'\\'
        | b'^'
        | b'_'
        | b'`'
        | b'|'
        | b'~'
    }
}

impl<'a> Lexer<'a> {
    /// Creates a new lexer which will lex the `input` source string.
    pub fn new(input: &str) -> Lexer<'_> {
        Lexer {
            input,
            allow_confusing_unicode: false,
        }
    }

    /// Returns the original source input that we're lexing.
    pub fn input(&self) -> &'a str {
        self.input
    }

    /// Configures whether "confusing" unicode characters are allowed while
    /// lexing.
    ///
    /// If allowed then no error will happen if these characters are found, but
    /// otherwise if disallowed a lex error will be produced when these
    /// characters are found. Confusing characters are denied by default.
    ///
    /// For now "confusing characters" are primarily related to the "trojan
    /// source" problem where it refers to characters which cause humans to read
    /// text differently than this lexer, such as characters that alter the
    /// left-to-right display of the source code.
    pub fn allow_confusing_unicode(&mut self, allow: bool) -> &mut Self {
        self.allow_confusing_unicode = allow;
        self
    }

    /// Lexes the next at the byte position `pos` in the input.
    ///
    /// Returns `Some` if a token is found or `None` if we're at EOF.
    ///
    /// The `pos` argument will be updated to point to the next token on a
    /// successful parse.
    ///
    /// # Errors
    ///
    /// Returns an error if the input is malformed.
    pub fn parse(&self, pos: &mut usize) -> Result<Option<Token>, Error> {
        let offset = *pos;
        Ok(match self.parse_kind(pos)? {
            Some(kind) => Some(Token {
                kind,
                offset,
                len: (*pos - offset).try_into().unwrap(),
            }),
            None => None,
        })
    }

    fn parse_kind(&self, pos: &mut usize) -> Result<Option<TokenKind>, Error> {
        let start = *pos;
        // This `match` generally parses the grammar specified at
        //
        // https://webassembly.github.io/spec/core/text/lexical.html#text-token
        let remaining = &self.input.as_bytes()[start..];
        let byte = match remaining.first() {
            Some(b) => b,
            None => return Ok(None),
        };

        match byte {
            // Open-parens check the next character to see if this is the start
            // of a block comment, otherwise it's just a bland left-paren
            // token.
            b'(' => match remaining.get(1) {
                Some(b';') => {
                    let mut level = 1;
                    // Note that we're doing a byte-level search here for the
                    // close-delimiter of `;)`. The actual source text is utf-8
                    // encode in `remaining` but due to how utf-8 works we
                    // can safely search for an ASCII byte since it'll never
                    // otherwise appear in the middle of a codepoint and if we
                    // find it then it's guaranteed to be the right byte.
                    //
                    // Mainly we're avoiding the overhead of decoding utf-8
                    // characters into a Rust `char` since it's otherwise
                    // unnecessary work.
                    let mut iter = remaining[2..].iter();
                    while let Some(ch) = iter.next() {
                        match ch {
                            b'(' => {
                                if let Some(b';') = iter.as_slice().first() {
                                    level += 1;
                                    iter.next();
                                }
                            }
                            b';' => {
                                if let Some(b')') = iter.as_slice().first() {
                                    level -= 1;
                                    iter.next();
                                    if level == 0 {
                                        let len = remaining.len() - iter.as_slice().len();
                                        let comment = &self.input[start..][..len];
                                        *pos += len;
                                        self.check_confusing_comment(*pos, comment)?;
                                        return Ok(Some(TokenKind::BlockComment));
                                    }
                                }
                            }
                            _ => {}
                        }
                    }
                    Err(self.error(start, LexError::DanglingBlockComment))
                }
                _ => {
                    *pos += 1;

                    Ok(Some(TokenKind::LParen))
                }
            },

            b')' => {
                *pos += 1;
                Ok(Some(TokenKind::RParen))
            }

            // https://webassembly.github.io/spec/core/text/lexical.html#white-space
            b' ' | b'\n' | b'\r' | b'\t' => {
                self.skip_ws(pos);
                Ok(Some(TokenKind::Whitespace))
            }

            c @ (idchars!() | b'"') => {
                let (kind, src) = self.parse_reserved(pos)?;
                match kind {
                    // If the reserved token was simply a single string then
                    // that is converted to a standalone string token
                    ReservedKind::String => return Ok(Some(TokenKind::String)),

                    // If only idchars were consumed then this could be a
                    // specific kind of standalone token we're interested in.
                    ReservedKind::Idchars => {
                        // https://webassembly.github.io/spec/core/text/values.html#integers
                        if let Some(ret) = self.classify_number(src) {
                            return Ok(Some(ret));
                        // https://webassembly.github.io/spec/core/text/values.html#text-id
                        } else if *c == b'$' && src.len() > 1 {
                            return Ok(Some(TokenKind::Id));
                        // https://webassembly.github.io/spec/core/text/lexical.html#text-keyword
                        } else if b'a' <= *c && *c <= b'z' {
                            return Ok(Some(TokenKind::Keyword));
                        }
                    }

                    // ... otherwise this was a conglomeration of idchars,
                    // strings, or just idchars that don't match a prior rule,
                    // meaning this falls through to the fallback `Reserved`
                    // token.
                    ReservedKind::Reserved => {}
                }

                Ok(Some(TokenKind::Reserved))
            }

            // This could be a line comment, otherwise `;` is a reserved token.
            // The second byte is checked to see if it's a `;;` line comment
            //
            // Note that this character being considered as part of a
            // `reserved` token is part of the annotations proposal.
            b';' => match remaining.get(1) {
                Some(b';') => {
                    let comment = self.split_until(pos, b'\n');
                    self.check_confusing_comment(*pos, comment)?;
                    Ok(Some(TokenKind::LineComment))
                }
                _ => {
                    *pos += 1;
                    Ok(Some(TokenKind::Reserved))
                }
            },

            // Other known reserved tokens other than `;`
            //
            // Note that these characters being considered as part of a
            // `reserved` token is part of the annotations proposal.
            b',' | b'[' | b']' | b'{' | b'}' => {
                *pos += 1;
                Ok(Some(TokenKind::Reserved))
            }

            _ => {
                let ch = self.input[start..].chars().next().unwrap();
                Err(self.error(*pos, LexError::Unexpected(ch)))
            }
        }
    }

    fn split_until(&self, pos: &mut usize, byte: u8) -> &'a str {
        let remaining = &self.input[*pos..];
        let byte_pos = memchr::memchr(byte, remaining.as_bytes()).unwrap_or(remaining.len());
        *pos += byte_pos;
        &remaining[..byte_pos]
    }

    fn skip_ws(&self, pos: &mut usize) {
        // This table is a byte lookup table to determine whether a byte is a
        // whitespace byte. There are only 4 whitespace bytes for the `*.wat`
        // format right now which are ' ', '\t', '\r', and '\n'. These 4 bytes
        // have a '1' in the table below.
        //
        // Due to how utf-8 works (our input is guaranteed to be utf-8) it is
        // known that if these bytes are found they're guaranteed to be the
        // whitespace byte, so they can be safely skipped and we don't have to
        // do full utf-8 decoding. This means that the goal of this function is
        // to find the first non-whitespace byte in `remaining`.
        //
        // For now this lookup table seems to be the fastest, but projects like
        // https://github.com/lemire/despacer show other simd algorithms which
        // can possibly accelerate this even more. Note that `*.wat` files often
        // have a lot of whitespace so this function is typically quite hot when
        // parsing inputs.
        #[rustfmt::skip]
        const WS: [u8; 256] = [
            //                                   \t \n       \r
            /* 0x00 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,
            /* 0x10 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //        ' '
            /* 0x20 */ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0x30 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0x40 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0x50 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0x60 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0x70 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0x80 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0x90 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0xa0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0xb0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0xc0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0xd0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0xe0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            /* 0xf0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ];
        let remaining = &self.input[*pos..];
        let non_ws_pos = remaining
            .as_bytes()
            .iter()
            .position(|b| WS[*b as usize] != 1)
            .unwrap_or(remaining.len());
        *pos += non_ws_pos;
    }

    /// Splits off a "reserved" token which is then further processed later on
    /// to figure out which kind of token it is `depending on `ReservedKind`.
    ///
    /// For more information on this method see the clarification at
    /// <https://github.com/WebAssembly/spec/pull/1499> but the general gist is
    /// that this is parsing the grammar:
    ///
    /// ```text
    /// reserved := (idchar | string)+
    /// ```
    ///
    /// which means that it is eating any number of adjacent string/idchar
    /// tokens (e.g. `a"b"c`) and returning the classification of what was
    /// eaten. The classification assists in determining what the actual token
    /// here eaten looks like.
    fn parse_reserved(&self, pos: &mut usize) -> Result<(ReservedKind, &'a str), Error> {
        let mut idchars = false;
        let mut strings = 0u32;
        let start = *pos;
        while let Some(byte) = self.input.as_bytes().get(*pos) {
            match byte {
                // Normal `idchars` production which appends to the reserved
                // token that's being produced.
                idchars!() => {
                    idchars = true;
                    *pos += 1;
                }

                // https://webassembly.github.io/spec/core/text/values.html#text-string
                b'"' => {
                    strings += 1;
                    *pos += 1;
                    let mut it = self.input[*pos..].chars();
                    let result = Lexer::parse_str(&mut it, self.allow_confusing_unicode);
                    *pos = self.input.len() - it.as_str().len();
                    match result {
                        Ok(_) => {}
                        Err(e) => {
                            let err_pos = match &e {
                                LexError::UnexpectedEof => self.input.len(),
                                _ => self.input[..*pos].char_indices().next_back().unwrap().0,
                            };
                            return Err(self.error(err_pos, e));
                        }
                    }
                }

                // Nothing else is considered part of a reserved token
                _ => break,
            }
        }
        let ret = &self.input[start..*pos];
        Ok(match (idchars, strings) {
            (false, 0) => unreachable!(),
            (false, 1) => (ReservedKind::String, ret),
            (true, 0) => (ReservedKind::Idchars, ret),
            _ => (ReservedKind::Reserved, ret),
        })
    }

    fn classify_number(&self, src: &str) -> Option<TokenKind> {
        let (sign, num) = if let Some(stripped) = src.strip_prefix('+') {
            (Some(SignToken::Plus), stripped)
        } else if let Some(stripped) = src.strip_prefix('-') {
            (Some(SignToken::Minus), stripped)
        } else {
            (None, src)
        };

        let negative = sign == Some(SignToken::Minus);

        // Handle `inf` and `nan` which are special numbers here
        if num == "inf" {
            return Some(TokenKind::Float(FloatKind::Inf { negative }));
        } else if num == "nan" {
            return Some(TokenKind::Float(FloatKind::Nan { negative }));
        } else if let Some(stripped) = num.strip_prefix("nan:0x") {
            let mut it = stripped.as_bytes().iter();
            let has_underscores = skip_underscores(&mut it, |x| char::from(x).is_ascii_hexdigit())?;
            if it.next().is_some() {
                return None;
            }
            return Some(TokenKind::Float(FloatKind::NanVal {
                negative,
                has_underscores,
            }));
        }

        // Figure out if we're a hex number or not
        let test_valid: fn(u8) -> bool;
        let (mut it, hex) = if let Some(stripped) = num.strip_prefix("0x") {
            test_valid = |x: u8| char::from(x).is_ascii_hexdigit();
            (stripped.as_bytes().iter(), true)
        } else {
            test_valid = |x: u8| char::from(x).is_ascii_digit();
            (num.as_bytes().iter(), false)
        };

        // Evaluate the first part, moving out all underscores
        let mut has_underscores = skip_underscores(&mut it, test_valid)?;

        match it.clone().next() {
            // If we're followed by something this may be a float so keep going.
            Some(_) => {}

            // Otherwise this is a valid integer literal!
            None => {
                return Some(TokenKind::Integer(IntegerKind {
                    has_underscores,
                    sign,
                    hex,
                }))
            }
        }

        // A number can optionally be after the decimal so only actually try to
        // parse one if it's there.
        if it.clone().next() == Some(&b'.') {
            it.next();
            match it.clone().next() {
                Some(c) if test_valid(*c) => {
                    if skip_underscores(&mut it, test_valid)? {
                        has_underscores = true;
                    }
                }
                Some(_) | None => {}
            }
        };

        // Figure out if there's an exponential part here to make a float, and
        // if so parse it but defer its actual calculation until later.
        match (hex, it.next()) {
            (true, Some(b'p')) | (true, Some(b'P')) | (false, Some(b'e')) | (false, Some(b'E')) => {
                match it.clone().next() {
                    Some(b'-') => {
                        it.next();
                    }
                    Some(b'+') => {
                        it.next();
                    }
                    _ => {}
                }
                if skip_underscores(&mut it, |x| char::from(x).is_ascii_digit())? {
                    has_underscores = true;
                }
            }
            (_, None) => {}
            _ => return None,
        }

        // We should have eaten everything by now, if not then this is surely
        // not a float or integer literal.
        if it.next().is_some() {
            return None;
        }

        return Some(TokenKind::Float(FloatKind::Normal {
            has_underscores,
            hex,
        }));

        fn skip_underscores<'a>(
            it: &mut slice::Iter<'_, u8>,
            good: fn(u8) -> bool,
        ) -> Option<bool> {
            let mut last_underscore = false;
            let mut has_underscores = false;
            let first = *it.next()?;
            if !good(first) {
                return None;
            }
            while let Some(c) = it.clone().next() {
                if *c == b'_' && !last_underscore {
                    has_underscores = true;
                    it.next();
                    last_underscore = true;
                    continue;
                }
                if !good(*c) {
                    break;
                }
                last_underscore = false;
                it.next();
            }
            if last_underscore {
                return None;
            }
            Some(has_underscores)
        }
    }

    /// Verifies that `comment`, which is about to be returned, has a "confusing
    /// unicode character" in it and should instead be transformed into an
    /// error.
    fn check_confusing_comment(&self, end: usize, comment: &str) -> Result<(), Error> {
        if self.allow_confusing_unicode {
            return Ok(());
        }

        // In an effort to avoid utf-8 decoding the entire `comment` the search
        // here is a bit more optimized. This checks for the `0xe2` byte because
        // in the utf-8 encoding that's the leading encoding byte for all
        // "confusing characters". Each instance of 0xe2 is checked to see if it
        // starts a confusing character, and if so that's returned.
        //
        // Also note that 0xe2 will never be found in the middle of a codepoint,
        // it's always the start of a codepoint. This means that if our special
        // characters show up they're guaranteed to start with 0xe2 bytes.
        let bytes = comment.as_bytes();
        for pos in memchr::Memchr::new(0xe2, bytes) {
            if let Some(c) = comment[pos..].chars().next() {
                if is_confusing_unicode(c) {
                    // Note that `self.cur()` accounts for already having
                    // parsed `comment`, so we move backwards to where
                    // `comment` started and then add the index within
                    // `comment`.
                    let pos = end - comment.len() + pos;
                    return Err(self.error(pos, LexError::ConfusingUnicode(c)));
                }
            }
        }

        Ok(())
    }

    fn parse_str(
        it: &mut str::Chars<'a>,
        allow_confusing_unicode: bool,
    ) -> Result<Cow<'a, [u8]>, LexError> {
        enum State {
            Start,
            String(Vec<u8>),
        }
        let orig = it.as_str();
        let mut state = State::Start;
        loop {
            match it.next().ok_or(LexError::UnexpectedEof)? {
                '"' => break,
                '\\' => {
                    match state {
                        State::String(_) => {}
                        State::Start => {
                            let pos = orig.len() - it.as_str().len() - 1;
                            state = State::String(orig[..pos].as_bytes().to_vec());
                        }
                    }
                    let buf = match &mut state {
                        State::String(b) => b,
                        State::Start => unreachable!(),
                    };
                    match it.next().ok_or(LexError::UnexpectedEof)? {
                        '"' => buf.push(b'"'),
                        '\'' => buf.push(b'\''),
                        't' => buf.push(b'\t'),
                        'n' => buf.push(b'\n'),
                        'r' => buf.push(b'\r'),
                        '\\' => buf.push(b'\\'),
                        'u' => {
                            Lexer::must_eat_char(it, '{')?;
                            let n = Lexer::hexnum(it)?;
                            let c = char::from_u32(n).ok_or(LexError::InvalidUnicodeValue(n))?;
                            buf.extend(c.encode_utf8(&mut [0; 4]).as_bytes());
                            Lexer::must_eat_char(it, '}')?;
                        }
                        c1 if c1.is_ascii_hexdigit() => {
                            let c2 = Lexer::hexdigit(it)?;
                            buf.push(to_hex(c1) * 16 + c2);
                        }
                        c => return Err(LexError::InvalidStringEscape(c)),
                    }
                }
                c if (c as u32) < 0x20 || c as u32 == 0x7f => {
                    return Err(LexError::InvalidStringElement(c))
                }
                c if !allow_confusing_unicode && is_confusing_unicode(c) => {
                    return Err(LexError::ConfusingUnicode(c))
                }
                c => match &mut state {
                    State::Start => {}
                    State::String(v) => {
                        v.extend(c.encode_utf8(&mut [0; 4]).as_bytes());
                    }
                },
            }
        }
        match state {
            State::Start => Ok(orig[..orig.len() - it.as_str().len() - 1].as_bytes().into()),
            State::String(s) => Ok(s.into()),
        }
    }

    fn hexnum(it: &mut str::Chars<'_>) -> Result<u32, LexError> {
        let n = Lexer::hexdigit(it)?;
        let mut last_underscore = false;
        let mut n = n as u32;
        while let Some(c) = it.clone().next() {
            if c == '_' {
                it.next();
                last_underscore = true;
                continue;
            }
            if !c.is_ascii_hexdigit() {
                break;
            }
            last_underscore = false;
            it.next();
            n = n
                .checked_mul(16)
                .and_then(|n| n.checked_add(to_hex(c) as u32))
                .ok_or(LexError::NumberTooBig)?;
        }
        if last_underscore {
            return Err(LexError::LoneUnderscore);
        }
        Ok(n)
    }

    /// Reads a hexidecimal digit from the input stream, returning where it's
    /// defined and the hex value. Returns an error on EOF or an invalid hex
    /// digit.
    fn hexdigit(it: &mut str::Chars<'_>) -> Result<u8, LexError> {
        let ch = Lexer::must_char(it)?;
        if ch.is_ascii_hexdigit() {
            Ok(to_hex(ch))
        } else {
            Err(LexError::InvalidHexDigit(ch))
        }
    }

    /// Reads the next character from the input string and where it's located,
    /// returning an error if the input stream is empty.
    fn must_char(it: &mut str::Chars<'_>) -> Result<char, LexError> {
        it.next().ok_or(LexError::UnexpectedEof)
    }

    /// Expects that a specific character must be read next
    fn must_eat_char(it: &mut str::Chars<'_>, wanted: char) -> Result<(), LexError> {
        let found = Lexer::must_char(it)?;
        if wanted == found {
            Ok(())
        } else {
            Err(LexError::Expected { wanted, found })
        }
    }

    /// Creates an error at `pos` with the specified `kind`
    fn error(&self, pos: usize, kind: LexError) -> Error {
        Error::lex(Span { offset: pos }, self.input, kind)
    }

    /// Returns an iterator over all tokens in the original source string
    /// starting at the `pos` specified.
    pub fn iter(&self, mut pos: usize) -> impl Iterator<Item = Result<Token, Error>> + '_ {
        std::iter::from_fn(move || self.parse(&mut pos).transpose())
    }

    /// Returns whether an annotation is present at `pos` and the name of the
    /// annotation.
    pub fn annotation(&self, mut pos: usize) -> Option<&'a str> {
        let bytes = self.input.as_bytes();
        // Quickly reject anything that for sure isn't an annotation since this
        // method is used every time an lparen is parsed.
        if bytes.get(pos) != Some(&b'@') {
            return None;
        }
        match self.parse(&mut pos) {
            Ok(Some(token)) => {
                match token.kind {
                    TokenKind::Reserved => {}
                    _ => return None,
                }
                if token.len == 1 {
                    None // just the `@` character isn't a valid annotation
                } else {
                    Some(&token.src(self.input)[1..])
                }
            }
            Ok(None) | Err(_) => None,
        }
    }
}

impl Token {
    /// Returns the original source text for this token.
    pub fn src<'a>(&self, s: &'a str) -> &'a str {
        &s[self.offset..][..self.len.try_into().unwrap()]
    }

    /// Returns the identifier, without the leading `$` symbol, that this token
    /// represents.
    ///
    /// Should only be used with `TokenKind::Id`.
    pub fn id<'a>(&self, s: &'a str) -> &'a str {
        &self.src(s)[1..]
    }

    /// Returns the keyword this token represents.
    ///
    /// Should only be used with [`TokenKind::Keyword`].
    pub fn keyword<'a>(&self, s: &'a str) -> &'a str {
        self.src(s)
    }

    /// Returns the reserved string this token represents.
    ///
    /// Should only be used with [`TokenKind::Reserved`].
    pub fn reserved<'a>(&self, s: &'a str) -> &'a str {
        self.src(s)
    }

    /// Returns the parsed string that this token represents.
    ///
    /// This returns either a raw byte slice into the source if that's possible
    /// or an owned representation to handle escaped characters and such.
    ///
    /// Should only be used with [`TokenKind::String`].
    pub fn string<'a>(&self, s: &'a str) -> Cow<'a, [u8]> {
        let mut ch = self.src(s).chars();
        ch.next().unwrap();
        Lexer::parse_str(&mut ch, true).unwrap()
    }

    /// Returns the decomposed float token that this represents.
    ///
    /// This will slice up the float token into its component parts and return a
    /// description of the float token in the source.
    ///
    /// Should only be used with [`TokenKind::Float`].
    pub fn float<'a>(&self, s: &'a str, kind: FloatKind) -> Float<'a> {
        match kind {
            FloatKind::Inf { negative } => Float::Inf { negative },
            FloatKind::Nan { negative } => Float::Nan {
                val: None,
                negative,
            },
            FloatKind::NanVal {
                negative,
                has_underscores,
            } => {
                let src = self.src(s);
                let src = if src.starts_with("n") { src } else { &src[1..] };
                let mut val = Cow::Borrowed(src.strip_prefix("nan:0x").unwrap());
                if has_underscores {
                    *val.to_mut() = val.replace("_", "");
                }
                Float::Nan {
                    val: Some(val),
                    negative,
                }
            }
            FloatKind::Normal {
                has_underscores,
                hex,
            } => {
                let src = self.src(s);
                let (integral, decimal, exponent) = match src.find('.') {
                    Some(i) => {
                        let integral = &src[..i];
                        let rest = &src[i + 1..];
                        let exponent = if hex {
                            rest.find('p').or_else(|| rest.find('P'))
                        } else {
                            rest.find('e').or_else(|| rest.find('E'))
                        };
                        match exponent {
                            Some(i) => (integral, Some(&rest[..i]), Some(&rest[i + 1..])),
                            None => (integral, Some(rest), None),
                        }
                    }
                    None => {
                        let exponent = if hex {
                            src.find('p').or_else(|| src.find('P'))
                        } else {
                            src.find('e').or_else(|| src.find('E'))
                        };
                        match exponent {
                            Some(i) => (&src[..i], None, Some(&src[i + 1..])),
                            None => (src, None, None),
                        }
                    }
                };
                let mut integral = Cow::Borrowed(integral.strip_prefix('+').unwrap_or(integral));
                let mut decimal = decimal.and_then(|s| {
                    if s.is_empty() {
                        None
                    } else {
                        Some(Cow::Borrowed(s))
                    }
                });
                let mut exponent =
                    exponent.map(|s| Cow::Borrowed(s.strip_prefix('+').unwrap_or(s)));
                if has_underscores {
                    *integral.to_mut() = integral.replace("_", "");
                    if let Some(decimal) = &mut decimal {
                        *decimal.to_mut() = decimal.replace("_", "");
                    }
                    if let Some(exponent) = &mut exponent {
                        *exponent.to_mut() = exponent.replace("_", "");
                    }
                }
                if hex {
                    *integral.to_mut() = integral.replace("0x", "");
                }
                Float::Val {
                    hex,
                    integral,
                    decimal,
                    exponent,
                }
            }
        }
    }

    /// Returns the decomposed integer token that this represents.
    ///
    /// This will slice up the integer token into its component parts and
    /// return a description of the integer token in the source.
    ///
    /// Should only be used with [`TokenKind::Integer`].
    pub fn integer<'a>(&self, s: &'a str, kind: IntegerKind) -> Integer<'a> {
        let src = self.src(s);
        let val = match kind.sign {
            Some(SignToken::Plus) => src.strip_prefix('+').unwrap(),
            Some(SignToken::Minus) => src,
            None => src,
        };
        let mut val = Cow::Borrowed(val);
        if kind.has_underscores {
            *val.to_mut() = val.replace("_", "");
        }
        if kind.hex {
            *val.to_mut() = val.replace("0x", "");
        }
        Integer {
            sign: kind.sign,
            hex: kind.hex,
            val,
        }
    }
}

impl<'a> Integer<'a> {
    /// Returns the sign token for this integer.
    pub fn sign(&self) -> Option<SignToken> {
        self.sign
    }

    /// Returns the value string that can be parsed for this integer, as well
    /// as the base that it should be parsed in
    pub fn val(&self) -> (&str, u32) {
        (&self.val, if self.hex { 16 } else { 10 })
    }
}

fn to_hex(c: char) -> u8 {
    match c {
        'a'..='f' => c as u8 - b'a' + 10,
        'A'..='F' => c as u8 - b'A' + 10,
        _ => c as u8 - b'0',
    }
}

impl fmt::Display for LexError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use LexError::*;
        match self {
            DanglingBlockComment => f.write_str("unterminated block comment")?,
            Unexpected(c) => write!(f, "unexpected character '{}'", escape_char(*c))?,
            InvalidStringElement(c) => {
                write!(f, "invalid character in string '{}'", escape_char(*c))?
            }
            InvalidStringEscape(c) => write!(f, "invalid string escape '{}'", escape_char(*c))?,
            InvalidHexDigit(c) => write!(f, "invalid hex digit '{}'", escape_char(*c))?,
            InvalidDigit(c) => write!(f, "invalid decimal digit '{}'", escape_char(*c))?,
            Expected { wanted, found } => write!(
                f,
                "expected '{}' but found '{}'",
                escape_char(*wanted),
                escape_char(*found)
            )?,
            UnexpectedEof => write!(f, "unexpected end-of-file")?,
            NumberTooBig => f.write_str("number is too big to parse")?,
            InvalidUnicodeValue(c) => write!(f, "invalid unicode scalar value 0x{:x}", c)?,
            LoneUnderscore => write!(f, "bare underscore in numeric literal")?,
            ConfusingUnicode(c) => write!(f, "likely-confusing unicode character found {:?}", c)?,
        }
        Ok(())
    }
}

fn escape_char(c: char) -> String {
    match c {
        '\t' => String::from("\\t"),
        '\r' => String::from("\\r"),
        '\n' => String::from("\\n"),
        '\\' => String::from("\\\\"),
        '\'' => String::from("\\\'"),
        '\"' => String::from("\""),
        '\x20'..='\x7e' => String::from(c),
        _ => c.escape_unicode().to_string(),
    }
}

/// This is an attempt to protect agains the "trojan source" [1] problem where
/// unicode characters can cause editors to render source code differently
/// for humans than the compiler itself sees.
///
/// To mitigate this issue, and because it's relatively rare in practice,
/// this simply rejects characters of that form.
///
/// [1]: https://www.trojansource.codes/
fn is_confusing_unicode(ch: char) -> bool {
    matches!(
        ch,
        '\u{202a}'
            | '\u{202b}'
            | '\u{202d}'
            | '\u{202e}'
            | '\u{2066}'
            | '\u{2067}'
            | '\u{2068}'
            | '\u{206c}'
            | '\u{2069}'
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ws_smoke() {
        fn get_whitespace(input: &str) -> &str {
            let token = get_token(input);
            match token.kind {
                TokenKind::Whitespace => token.src(input),
                other => panic!("unexpected {:?}", other),
            }
        }
        assert_eq!(get_whitespace(" "), " ");
        assert_eq!(get_whitespace("  "), "  ");
        assert_eq!(get_whitespace("  \n "), "  \n ");
        assert_eq!(get_whitespace("  x"), "  ");
        assert_eq!(get_whitespace("  ;"), "  ");
    }

    #[test]
    fn line_comment_smoke() {
        fn get_line_comment(input: &str) -> &str {
            let token = get_token(input);
            match token.kind {
                TokenKind::LineComment => token.src(input),
                other => panic!("unexpected {:?}", other),
            }
        }
        assert_eq!(get_line_comment(";;"), ";;");
        assert_eq!(get_line_comment(";; xyz"), ";; xyz");
        assert_eq!(get_line_comment(";; xyz\nabc"), ";; xyz");
        assert_eq!(get_line_comment(";;\nabc"), ";;");
        assert_eq!(get_line_comment(";;   \nabc"), ";;   ");
    }

    #[test]
    fn block_comment_smoke() {
        fn get_block_comment(input: &str) -> &str {
            let token = get_token(input);
            match token.kind {
                TokenKind::BlockComment => token.src(input),
                other => panic!("unexpected {:?}", other),
            }
        }
        assert_eq!(get_block_comment("(;;)"), "(;;)");
        assert_eq!(get_block_comment("(; ;)"), "(; ;)");
        assert_eq!(get_block_comment("(; (;;) ;)"), "(; (;;) ;)");
    }

    fn get_token(input: &str) -> Token {
        Lexer::new(input)
            .parse(&mut 0)
            .expect("no first token")
            .expect("no token")
    }

    #[test]
    fn lparen() {
        assert_eq!(get_token("((").kind, TokenKind::LParen);
    }

    #[test]
    fn rparen() {
        assert_eq!(get_token(")(").kind, TokenKind::RParen);
    }

    #[test]
    fn strings() {
        fn get_string(input: &str) -> Vec<u8> {
            let token = get_token(input);
            match token.kind {
                TokenKind::String => token.string(input).to_vec(),
                other => panic!("not keyword {:?}", other),
            }
        }
        assert_eq!(&*get_string("\"\""), b"");
        assert_eq!(&*get_string("\"a\""), b"a");
        assert_eq!(&*get_string("\"a b c d\""), b"a b c d");
        assert_eq!(&*get_string("\"\\\"\""), b"\"");
        assert_eq!(&*get_string("\"\\'\""), b"'");
        assert_eq!(&*get_string("\"\\n\""), b"\n");
        assert_eq!(&*get_string("\"\\t\""), b"\t");
        assert_eq!(&*get_string("\"\\r\""), b"\r");
        assert_eq!(&*get_string("\"\\\\\""), b"\\");
        assert_eq!(&*get_string("\"\\01\""), &[1]);
        assert_eq!(&*get_string("\"\\u{1}\""), &[1]);
        assert_eq!(
            &*get_string("\"\\u{0f3}\""),
            '\u{0f3}'.encode_utf8(&mut [0; 4]).as_bytes()
        );
        assert_eq!(
            &*get_string("\"\\u{0_f_3}\""),
            '\u{0f3}'.encode_utf8(&mut [0; 4]).as_bytes()
        );

        for i in 0..=255i32 {
            let s = format!("\"\\{:02x}\"", i);
            assert_eq!(&*get_string(&s), &[i as u8]);
        }
    }

    #[test]
    fn id() {
        fn get_id(input: &str) -> &str {
            let token = get_token(input);
            match token.kind {
                TokenKind::Id => token.id(input),
                other => panic!("not id {:?}", other),
            }
        }
        assert_eq!(get_id("$x"), "x");
        assert_eq!(get_id("$xyz"), "xyz");
        assert_eq!(get_id("$x_z"), "x_z");
        assert_eq!(get_id("$0^"), "0^");
        assert_eq!(get_id("$0^;;"), "0^");
        assert_eq!(get_id("$0^ ;;"), "0^");
    }

    #[test]
    fn keyword() {
        fn get_keyword(input: &str) -> &str {
            let token = get_token(input);
            match token.kind {
                TokenKind::Keyword => token.keyword(input),
                other => panic!("not keyword {:?}", other),
            }
        }
        assert_eq!(get_keyword("x"), "x");
        assert_eq!(get_keyword("xyz"), "xyz");
        assert_eq!(get_keyword("x_z"), "x_z");
        assert_eq!(get_keyword("x_z "), "x_z");
        assert_eq!(get_keyword("x_z "), "x_z");
    }

    #[test]
    fn reserved() {
        fn get_reserved(input: &str) -> &str {
            let token = get_token(input);
            match token.kind {
                TokenKind::Reserved => token.reserved(input),
                other => panic!("not reserved {:?}", other),
            }
        }
        assert_eq!(get_reserved("$ "), "$");
        assert_eq!(get_reserved("^_x "), "^_x");
    }

    #[test]
    fn integer() {
        fn get_integer(input: &str) -> String {
            let token = get_token(input);
            match token.kind {
                TokenKind::Integer(i) => token.integer(input, i).val.to_string(),
                other => panic!("not integer {:?}", other),
            }
        }
        assert_eq!(get_integer("1"), "1");
        assert_eq!(get_integer("0"), "0");
        assert_eq!(get_integer("-1"), "-1");
        assert_eq!(get_integer("+1"), "1");
        assert_eq!(get_integer("+1_000"), "1000");
        assert_eq!(get_integer("+1_0_0_0"), "1000");
        assert_eq!(get_integer("+0x10"), "10");
        assert_eq!(get_integer("-0x10"), "-10");
        assert_eq!(get_integer("0x10"), "10");
    }

    #[test]
    fn float() {
        fn get_float(input: &str) -> Float<'_> {
            let token = get_token(input);
            match token.kind {
                TokenKind::Float(f) => token.float(input, f),
                other => panic!("not float {:?}", other),
            }
        }
        assert_eq!(
            get_float("nan"),
            Float::Nan {
                val: None,
                negative: false
            },
        );
        assert_eq!(
            get_float("-nan"),
            Float::Nan {
                val: None,
                negative: true,
            },
        );
        assert_eq!(
            get_float("+nan"),
            Float::Nan {
                val: None,
                negative: false,
            },
        );
        assert_eq!(
            get_float("+nan:0x1"),
            Float::Nan {
                val: Some("1".into()),
                negative: false,
            },
        );
        assert_eq!(
            get_float("nan:0x7f_ffff"),
            Float::Nan {
                val: Some("7fffff".into()),
                negative: false,
            },
        );
        assert_eq!(get_float("inf"), Float::Inf { negative: false });
        assert_eq!(get_float("-inf"), Float::Inf { negative: true });
        assert_eq!(get_float("+inf"), Float::Inf { negative: false });

        assert_eq!(
            get_float("1.2"),
            Float::Val {
                integral: "1".into(),
                decimal: Some("2".into()),
                exponent: None,
                hex: false,
            },
        );
        assert_eq!(
            get_float("1.2e3"),
            Float::Val {
                integral: "1".into(),
                decimal: Some("2".into()),
                exponent: Some("3".into()),
                hex: false,
            },
        );
        assert_eq!(
            get_float("-1_2.1_1E+0_1"),
            Float::Val {
                integral: "-12".into(),
                decimal: Some("11".into()),
                exponent: Some("01".into()),
                hex: false,
            },
        );
        assert_eq!(
            get_float("+1_2.1_1E-0_1"),
            Float::Val {
                integral: "12".into(),
                decimal: Some("11".into()),
                exponent: Some("-01".into()),
                hex: false,
            },
        );
        assert_eq!(
            get_float("0x1_2.3_4p5_6"),
            Float::Val {
                integral: "12".into(),
                decimal: Some("34".into()),
                exponent: Some("56".into()),
                hex: true,
            },
        );
        assert_eq!(
            get_float("+0x1_2.3_4P-5_6"),
            Float::Val {
                integral: "12".into(),
                decimal: Some("34".into()),
                exponent: Some("-56".into()),
                hex: true,
            },
        );
        assert_eq!(
            get_float("1."),
            Float::Val {
                integral: "1".into(),
                decimal: None,
                exponent: None,
                hex: false,
            },
        );
        assert_eq!(
            get_float("0x1p-24"),
            Float::Val {
                integral: "1".into(),
                decimal: None,
                exponent: Some("-24".into()),
                hex: true,
            },
        );
    }
}
