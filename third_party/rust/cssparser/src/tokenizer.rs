/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://drafts.csswg.org/css-syntax/#tokenization

use std::ops::Range;
use std::cell::Cell;
use std::char;
use std::ascii::AsciiExt;
use std::i32;

use compact_cow_str::CompactCowStr;
use self::Token::*;


/// One of the pieces the CSS input is broken into.
///
/// Some components use `Cow` in order to borrow from the original input string
/// and avoid allocating/copying when possible.
#[derive(PartialEq, Debug, Clone)]
pub enum Token<'a> {

    /// A [`<ident-token>`](https://drafts.csswg.org/css-syntax/#ident-token-diagram)
    Ident(CompactCowStr<'a>),

    /// A [`<at-keyword-token>`](https://drafts.csswg.org/css-syntax/#at-keyword-token-diagram)
    ///
    /// The value does not include the `@` marker.
    AtKeyword(CompactCowStr<'a>),

    /// A [`<hash-token>`](https://drafts.csswg.org/css-syntax/#hash-token-diagram) with the type flag set to "unrestricted"
    ///
    /// The value does not include the `#` marker.
    Hash(CompactCowStr<'a>),

    /// A [`<hash-token>`](https://drafts.csswg.org/css-syntax/#hash-token-diagram) with the type flag set to "id"
    ///
    /// The value does not include the `#` marker.
    IDHash(CompactCowStr<'a>),  // Hash that is a valid ID selector.

    /// A [`<string-token>`](https://drafts.csswg.org/css-syntax/#string-token-diagram)
    ///
    /// The value does not include the quotes.
    QuotedString(CompactCowStr<'a>),

    /// A [`<url-token>`](https://drafts.csswg.org/css-syntax/#url-token-diagram) or `url( <string-token> )` function
    ///
    /// The value does not include the `url(` `)` markers or the quotes.
    UnquotedUrl(CompactCowStr<'a>),

    /// A `<delim-token>`
    Delim(char),

    /// A [`<number-token>`](https://drafts.csswg.org/css-syntax/#number-token-diagram)
    Number {
        /// Whether the number had a `+` or `-` sign.
        ///
        /// This is used is some cases like the <An+B> micro syntax. (See the `parse_nth` function.)
        has_sign: bool,

        /// The value as a float
        value: f32,

        /// If the origin source did not include a fractional part, the value as an integer.
        int_value: Option<i32>,
    },

    /// A [`<percentage-token>`](https://drafts.csswg.org/css-syntax/#percentage-token-diagram)
    Percentage {
        /// Whether the number had a `+` or `-` sign.
        has_sign: bool,

        /// The value as a float, divided by 100 so that the nominal range is 0.0 to 1.0.
        unit_value: f32,

        /// If the origin source did not include a fractional part, the value as an integer.
        /// It is **not** divided by 100.
        int_value: Option<i32>,
    },

    /// A [`<dimension-token>`](https://drafts.csswg.org/css-syntax/#dimension-token-diagram)
    Dimension {
        /// Whether the number had a `+` or `-` sign.
        ///
        /// This is used is some cases like the <An+B> micro syntax. (See the `parse_nth` function.)
        has_sign: bool,

        /// The value as a float
        value: f32,

        /// If the origin source did not include a fractional part, the value as an integer.
        int_value: Option<i32>,

        /// The unit, e.g. "px" in `12px`
        unit: CompactCowStr<'a>
    },

    /// A [`<whitespace-token>`](https://drafts.csswg.org/css-syntax/#whitespace-token-diagram)
    WhiteSpace(&'a str),

    /// A comment.
    ///
    /// The CSS Syntax spec does not generate tokens for comments,
    /// But we do, because we can (borrowed &str makes it cheap).
    ///
    /// The value does not include the `/*` `*/` markers.
    Comment(&'a str),

    /// A `:` `<colon-token>`
    Colon,  // :

    /// A `;` `<semicolon-token>`
    Semicolon,  // ;

    /// A `,` `<comma-token>`
    Comma,  // ,

    /// A `~=` [`<include-match-token>`](https://drafts.csswg.org/css-syntax/#include-match-token-diagram)
    IncludeMatch,

    /// A `|=` [`<dash-match-token>`](https://drafts.csswg.org/css-syntax/#dash-match-token-diagram)
    DashMatch,

    /// A `^=` [`<prefix-match-token>`](https://drafts.csswg.org/css-syntax/#prefix-match-token-diagram)
    PrefixMatch,

    /// A `$=` [`<suffix-match-token>`](https://drafts.csswg.org/css-syntax/#suffix-match-token-diagram)
    SuffixMatch,

    /// A `*=` [`<substring-match-token>`](https://drafts.csswg.org/css-syntax/#substring-match-token-diagram)
    SubstringMatch,

    /// A `||` [`<column-token>`](https://drafts.csswg.org/css-syntax/#column-token-diagram)
    Column,

    /// A `<!--` [`<CDO-token>`](https://drafts.csswg.org/css-syntax/#CDO-token-diagram)
    CDO,

    /// A `-->` [`<CDC-token>`](https://drafts.csswg.org/css-syntax/#CDC-token-diagram)
    CDC,

    /// A [`<function-token>`](https://drafts.csswg.org/css-syntax/#function-token-diagram)
    ///
    /// The value (name) does not include the `(` marker.
    Function(CompactCowStr<'a>),

    /// A `<(-token>`
    ParenthesisBlock,

    /// A `<[-token>`
    SquareBracketBlock,

    /// A `<{-token>`
    CurlyBracketBlock,

    /// A `<bad-url-token>`
    ///
    /// This token always indicates a parse error.
    BadUrl,

    /// A `<bad-string-token>`
    ///
    /// This token always indicates a parse error.
    BadString,

    /// A `<)-token>`
    ///
    /// When obtained from one of the `Parser::next*` methods,
    /// this token is always unmatched and indicates a parse error.
    CloseParenthesis,

    /// A `<]-token>`
    ///
    /// When obtained from one of the `Parser::next*` methods,
    /// this token is always unmatched and indicates a parse error.
    CloseSquareBracket,

    /// A `<}-token>`
    ///
    /// When obtained from one of the `Parser::next*` methods,
    /// this token is always unmatched and indicates a parse error.
    CloseCurlyBracket,
}


impl<'a> Token<'a> {
    /// Return whether this token represents a parse error.
    ///
    /// `BadUrl` and `BadString` are tokenizer-level parse errors.
    ///
    /// `CloseParenthesis`, `CloseSquareBracket`, and `CloseCurlyBracket` are *unmatched*
    /// and therefore parse errors when returned by one of the `Parser::next*` methods.
    pub fn is_parse_error(&self) -> bool {
        matches!(
            *self,
            BadUrl | BadString | CloseParenthesis | CloseSquareBracket | CloseCurlyBracket
        )
    }
}


#[derive(Clone)]
pub struct Tokenizer<'a> {
    input: &'a str,
    /// Counted in bytes, not code points. From 0.
    position: usize,
    /// Cache for `source_location()`
    last_known_source_location: Cell<(SourcePosition, SourceLocation)>,
    var_functions: SeenStatus,
    viewport_percentages: SeenStatus,
}

#[derive(Copy, Clone, PartialEq, Eq)]
enum SeenStatus {
    DontCare,
    LookingForThem,
    SeenAtLeastOne,
}


impl<'a> Tokenizer<'a> {
    #[inline]
    pub fn new(input: &str) -> Tokenizer {
        Tokenizer {
            input: input,
            position: 0,
            last_known_source_location: Cell::new((SourcePosition(0),
                                                   SourceLocation { line: 1, column: 1 })),
            var_functions: SeenStatus::DontCare,
            viewport_percentages: SeenStatus::DontCare,
        }
    }

    #[inline]
    pub fn look_for_var_functions(&mut self) {
        self.var_functions = SeenStatus::LookingForThem;
    }

    #[inline]
    pub fn seen_var_functions(&mut self) -> bool {
        let seen = self.var_functions == SeenStatus::SeenAtLeastOne;
        self.var_functions = SeenStatus::DontCare;
        seen
    }

    #[inline]
    pub fn look_for_viewport_percentages(&mut self) {
        self.viewport_percentages = SeenStatus::LookingForThem;
    }

    #[inline]
    pub fn seen_viewport_percentages(&mut self) -> bool {
        let seen = self.viewport_percentages == SeenStatus::SeenAtLeastOne;
        self.viewport_percentages = SeenStatus::DontCare;
        seen
    }

    #[inline]
    pub fn next(&mut self) -> Result<Token<'a>, ()> {
        next_token(self)
    }

    #[inline]
    pub fn position(&self) -> SourcePosition {
        SourcePosition(self.position)
    }

    #[inline]
    pub fn reset(&mut self, new_position: SourcePosition) {
        self.position = new_position.0;
    }

    #[inline]
    pub fn slice_from(&self, start_pos: SourcePosition) -> &'a str {
        &self.input[start_pos.0..self.position]
    }

    #[inline]
    pub fn slice(&self, range: Range<SourcePosition>) -> &'a str {
        &self.input[range.start.0..range.end.0]
    }

    #[inline]
    pub fn current_source_location(&self) -> SourceLocation {
        let position = SourcePosition(self.position);
        self.source_location(position)
    }

    pub fn source_location(&self, position: SourcePosition) -> SourceLocation {
        let target = position.0;
        let mut location;
        let mut position;
        let (SourcePosition(last_known_position), last_known_location) =
            self.last_known_source_location.get();
        if target >= last_known_position {
            position = last_known_position;
            location = last_known_location;
        } else {
            // For now we’re only traversing the source *forwards* to count newlines.
            // So if the requested position is before the last known one,
            // start over from the beginning.
            position = 0;
            location = SourceLocation { line: 1, column: 1 };
        }
        let mut source = &self.input[position..target];
        while let Some(newline_position) = source.find(|c| matches!(c, '\n' | '\r' | '\x0C')) {
            let offset = newline_position +
                if source[newline_position..].starts_with("\r\n") { 2 } else { 1 };
            source = &source[offset..];
            position += offset;
            location.line += 1;
            location.column = 1;
        }
        debug_assert!(position <= target);
        location.column += (target - position) as u32;
        self.last_known_source_location.set((SourcePosition(target), location));
        location
    }

    #[inline]
    pub fn next_byte(&self) -> Option<u8> {
        if self.is_eof() {
            None
        } else {
            Some(self.input.as_bytes()[self.position])
        }
    }

    // If false, `tokenizer.next_char()` will not panic.
    #[inline]
    fn is_eof(&self) -> bool { !self.has_at_least(0) }

    // If true, the input has at least `n` bytes left *after* the current one.
    // That is, `tokenizer.char_at(n)` will not panic.
    #[inline]
    fn has_at_least(&self, n: usize) -> bool { self.position + n < self.input.len() }

    #[inline]
    pub fn advance(&mut self, n: usize) { self.position += n }

    // Assumes non-EOF
    #[inline]
    fn next_byte_unchecked(&self) -> u8 { self.byte_at(0) }

    #[inline]
    fn byte_at(&self, offset: usize) -> u8 {
        self.input.as_bytes()[self.position + offset]
    }

    #[inline]
    fn consume_byte(&mut self) -> u8 {
        self.position += 1;
        self.input.as_bytes()[self.position - 1]
    }

    #[inline]
    fn next_char(&self) -> char {
        self.input[self.position..].chars().next().unwrap()
    }

    #[inline]
    fn has_newline_at(&self, offset: usize) -> bool {
        self.position + offset < self.input.len() &&
        matches!(self.byte_at(offset), b'\n' | b'\r' | b'\x0C')
    }

    #[inline]
    fn consume_char(&mut self) -> char {
        let c = self.next_char();
        self.position += c.len_utf8();
        c
    }

    #[inline]
    fn starts_with(&self, needle: &[u8]) -> bool {
        self.input.as_bytes()[self.position..].starts_with(needle)
    }
}


#[derive(PartialEq, Eq, PartialOrd, Ord, Debug, Clone, Copy)]
pub struct SourcePosition(usize);


/// The line and column number for a given position within the input.
#[derive(PartialEq, Eq, Debug, Clone, Copy)]
pub struct SourceLocation {
    /// The line number, starting at 1 for the first line.
    pub line: u32,

    /// The column number within a line, starting at 1 for first the character of the line.
    pub column: u32,
}


fn next_token<'a>(tokenizer: &mut Tokenizer<'a>) -> Result<Token<'a>, ()> {
    if tokenizer.is_eof() {
        return Err(())
    }
    let b = tokenizer.next_byte_unchecked();
    let token = match_byte! { b,
        b'\t' | b'\n' | b' ' | b'\r' | b'\x0C' => {
            let start_position = tokenizer.position();
            tokenizer.advance(1);
            while !tokenizer.is_eof() {
                match tokenizer.next_byte_unchecked() {
                    b' ' | b'\t' | b'\n' | b'\r' | b'\x0C' => tokenizer.advance(1),
                    _ => break,
                }
            }
            WhiteSpace(tokenizer.slice_from(start_position))
        },
        b'"' => { consume_string(tokenizer, false) },
        b'#' => {
            tokenizer.advance(1);
            if is_ident_start(tokenizer) { IDHash(consume_name(tokenizer)) }
            else if !tokenizer.is_eof() && match tokenizer.next_byte_unchecked() {
                b'a'...b'z' | b'A'...b'Z' | b'0'...b'9' | b'-' | b'_' => true,
                b'\\' => !tokenizer.has_newline_at(1),
                _ => !b.is_ascii(),
            } { Hash(consume_name(tokenizer)) }
            else { Delim('#') }
        },
        b'$' => {
            if tokenizer.starts_with(b"$=") { tokenizer.advance(2); SuffixMatch }
            else { tokenizer.advance(1); Delim('$') }
        },
        b'\'' => { consume_string(tokenizer, true) },
        b'(' => { tokenizer.advance(1); ParenthesisBlock },
        b')' => { tokenizer.advance(1); CloseParenthesis },
        b'*' => {
            if tokenizer.starts_with(b"*=") { tokenizer.advance(2); SubstringMatch }
            else { tokenizer.advance(1); Delim('*') }
        },
        b'+' => {
            if (
                tokenizer.has_at_least(1)
                && matches!(tokenizer.byte_at(1), b'0'...b'9')
            ) || (
                tokenizer.has_at_least(2)
                && tokenizer.byte_at(1) == b'.'
                && matches!(tokenizer.byte_at(2), b'0'...b'9')
            ) {
                consume_numeric(tokenizer)
            } else {
                tokenizer.advance(1);
                Delim('+')
            }
        },
        b',' => { tokenizer.advance(1); Comma },
        b'-' => {
            if (
                tokenizer.has_at_least(1)
                && matches!(tokenizer.byte_at(1), b'0'...b'9')
            ) || (
                tokenizer.has_at_least(2)
                && tokenizer.byte_at(1) == b'.'
                && matches!(tokenizer.byte_at(2), b'0'...b'9')
            ) {
                consume_numeric(tokenizer)
            } else if tokenizer.starts_with(b"-->") {
                tokenizer.advance(3);
                CDC
            } else if is_ident_start(tokenizer) {
                consume_ident_like(tokenizer)
            } else {
                tokenizer.advance(1);
                Delim('-')
            }
        },
        b'.' => {
            if tokenizer.has_at_least(1)
                && matches!(tokenizer.byte_at(1), b'0'...b'9'
            ) {
                consume_numeric(tokenizer)
            } else {
                tokenizer.advance(1);
                Delim('.')
            }
        }
        b'/' => {
            if tokenizer.starts_with(b"/*") {
                tokenizer.advance(2);  // consume "/*"
                let start_position = tokenizer.position();
                let content;
                match tokenizer.input[tokenizer.position..].find("*/") {
                    Some(offset) => {
                        tokenizer.advance(offset);
                        content = tokenizer.slice_from(start_position);
                        tokenizer.advance(2);
                    }
                    None => {
                        tokenizer.position = tokenizer.input.len();
                        content = tokenizer.slice_from(start_position);
                    }
                }
                Comment(content)
            } else {
                tokenizer.advance(1);
                Delim('/')
            }
        }
        b'0'...b'9' => { consume_numeric(tokenizer) },
        b':' => { tokenizer.advance(1); Colon },
        b';' => { tokenizer.advance(1); Semicolon },
        b'<' => {
            if tokenizer.starts_with(b"<!--") {
                tokenizer.advance(4);
                CDO
            } else {
                tokenizer.advance(1);
                Delim('<')
            }
        },
        b'@' => {
            tokenizer.advance(1);
            if is_ident_start(tokenizer) { AtKeyword(consume_name(tokenizer)) }
            else { Delim('@') }
        },
        b'a'...b'z' | b'A'...b'Z' | b'_' | b'\0' => { consume_ident_like(tokenizer) },
        b'[' => { tokenizer.advance(1); SquareBracketBlock },
        b'\\' => {
            if !tokenizer.has_newline_at(1) { consume_ident_like(tokenizer) }
            else { tokenizer.advance(1); Delim('\\') }
        },
        b']' => { tokenizer.advance(1); CloseSquareBracket },
        b'^' => {
            if tokenizer.starts_with(b"^=") { tokenizer.advance(2); PrefixMatch }
            else { tokenizer.advance(1); Delim('^') }
        },
        b'{' => { tokenizer.advance(1); CurlyBracketBlock },
        b'|' => {
            if tokenizer.starts_with(b"|=") { tokenizer.advance(2); DashMatch }
            else if tokenizer.starts_with(b"||") { tokenizer.advance(2); Column }
            else { tokenizer.advance(1); Delim('|') }
        },
        b'}' => { tokenizer.advance(1); CloseCurlyBracket },
        b'~' => {
            if tokenizer.starts_with(b"~=") { tokenizer.advance(2); IncludeMatch }
            else { tokenizer.advance(1); Delim('~') }
        },
        _ => {
            if !b.is_ascii() {
                consume_ident_like(tokenizer)
            } else {
                tokenizer.advance(1);
                Delim(b as char)
            }
        },
    };
    Ok(token)
}


fn consume_string<'a>(tokenizer: &mut Tokenizer<'a>, single_quote: bool) -> Token<'a> {
    match consume_quoted_string(tokenizer, single_quote) {
        Ok(value) => QuotedString(value),
        Err(()) => BadString
    }
}


/// Return `Err(())` on syntax error (ie. unescaped newline)
fn consume_quoted_string<'a>(tokenizer: &mut Tokenizer<'a>, single_quote: bool)
                             -> Result<CompactCowStr<'a>, ()> {
    tokenizer.advance(1);  // Skip the initial quote
    // start_pos is at code point boundary, after " or '
    let start_pos = tokenizer.position();
    let mut string_bytes;
    loop {
        if tokenizer.is_eof() {
            return Ok(tokenizer.slice_from(start_pos).into())
        }
        match_byte! { tokenizer.next_byte_unchecked(),
            b'"' => {
                if !single_quote {
                    let value = tokenizer.slice_from(start_pos);
                    tokenizer.advance(1);
                    return Ok(value.into())
                }
            }
            b'\'' => {
                if single_quote {
                    let value = tokenizer.slice_from(start_pos);
                    tokenizer.advance(1);
                    return Ok(value.into())
                }
            }
            b'\\' | b'\0' => {
                // * The tokenizer’s input is UTF-8 since it’s `&str`.
                // * start_pos is at a code point boundary
                // * so is the current position (which is before '\\' or '\0'
                //
                // So `string_bytes` is well-formed UTF-8.
                string_bytes = tokenizer.slice_from(start_pos).as_bytes().to_owned();
                break
            }
            b'\n' | b'\r' | b'\x0C' => { return Err(()) },
            _ => {}
        }
        tokenizer.consume_byte();
    }

    while !tokenizer.is_eof() {
        if matches!(tokenizer.next_byte_unchecked(), b'\n' | b'\r' | b'\x0C') {
            return Err(());
        }
        let b = tokenizer.consume_byte();
        match_byte! { b,
            b'"' => {
                if !single_quote {
                    break;
                }
            }
            b'\'' => {
                if single_quote {
                    break;
                }
            }
            b'\\' => {
                if !tokenizer.is_eof() {
                    match tokenizer.next_byte_unchecked() {
                        // Escaped newline
                        b'\n' | b'\x0C' => tokenizer.advance(1),
                        b'\r' => {
                            tokenizer.advance(1);
                            if tokenizer.next_byte() == Some(b'\n') {
                                tokenizer.advance(1);
                            }
                        }
                        // This pushes one well-formed code point
                        _ => consume_escape_and_write(tokenizer, &mut string_bytes)
                    }
                }
                // else: escaped EOF, do nothing.
                continue;
            }
            b'\0' => {
                string_bytes.extend("\u{FFFD}".as_bytes());
                continue;
            }
            _ => {},
        }

        // If this byte is part of a multi-byte code point,
        // we’ll end up copying the whole code point before this loop does something else.
        string_bytes.push(b);
    }

    Ok(
        // string_bytes is well-formed UTF-8, see other comments.
        unsafe { from_utf8_release_unchecked(string_bytes) }.into()
    )
}


#[inline]
fn is_ident_start(tokenizer: &mut Tokenizer) -> bool {
    !tokenizer.is_eof() && match_byte! { tokenizer.next_byte_unchecked(),
        b'a'...b'z' | b'A'...b'Z' | b'_' | b'\0' => { true },
        b'-' => {
            tokenizer.has_at_least(1) && match_byte! { tokenizer.byte_at(1),
                b'a'...b'z' | b'A'...b'Z' | b'-' | b'_' | b'\0' => {
                    true
                }
                b'\\' => { !tokenizer.has_newline_at(1) }
                b => { !b.is_ascii() },
            }
        },
        b'\\' => { !tokenizer.has_newline_at(1) },
        b => { !b.is_ascii() },
    }
}


fn consume_ident_like<'a>(tokenizer: &mut Tokenizer<'a>) -> Token<'a> {
    let value = consume_name(tokenizer);
    if !tokenizer.is_eof() && tokenizer.next_byte_unchecked() == b'(' {
        tokenizer.advance(1);
        if value.eq_ignore_ascii_case("url") {
            consume_unquoted_url(tokenizer).unwrap_or(Function(value))
        } else {
            if tokenizer.var_functions == SeenStatus::LookingForThem &&
                value.eq_ignore_ascii_case("var") {
                tokenizer.var_functions = SeenStatus::SeenAtLeastOne;
            }
            Function(value)
        }
    } else {
        Ident(value)
    }
}

fn consume_name<'a>(tokenizer: &mut Tokenizer<'a>) -> CompactCowStr<'a> {
    // start_pos is the end of the previous token, therefore at a code point boundary
    let start_pos = tokenizer.position();
    let mut value_bytes;
    loop {
        if tokenizer.is_eof() {
            return tokenizer.slice_from(start_pos).into()
        }
        match_byte! { tokenizer.next_byte_unchecked(),
            b'a'...b'z' | b'A'...b'Z' | b'0'...b'9' | b'_' | b'-' => { tokenizer.advance(1) },
            b'\\' | b'\0' => {
                // * The tokenizer’s input is UTF-8 since it’s `&str`.
                // * start_pos is at a code point boundary
                // * so is the current position (which is before '\\' or '\0'
                //
                // So `value_bytes` is well-formed UTF-8.
                value_bytes = tokenizer.slice_from(start_pos).as_bytes().to_owned();
                break
            }
            b => {
                if b.is_ascii() {
                    return tokenizer.slice_from(start_pos).into();
                }
                tokenizer.advance(1);
            }
        }
    }

    while !tokenizer.is_eof() {
        let b = tokenizer.next_byte_unchecked();
        match_byte! { b,
            b'a'...b'z' | b'A'...b'Z' | b'0'...b'9' | b'_' | b'-'  => {
                tokenizer.advance(1);
                value_bytes.push(b)  // ASCII
            }
            b'\\' => {
                if tokenizer.has_newline_at(1) { break }
                tokenizer.advance(1);
                // This pushes one well-formed code point
                consume_escape_and_write(tokenizer, &mut value_bytes)
            }
            b'\0' => {
                tokenizer.advance(1);
                value_bytes.extend("\u{FFFD}".as_bytes());
            },
            _ => {
                if b.is_ascii() {
                    break;
                }
                tokenizer.advance(1);
                // This byte *is* part of a multi-byte code point,
                // we’ll end up copying the whole code point before this loop does something else.
                value_bytes.push(b)
            }
        }
    }
    // string_bytes is well-formed UTF-8, see other comments.
    unsafe { from_utf8_release_unchecked(value_bytes) }.into()
}

fn byte_to_hex_digit(b: u8) -> Option<u32> {
    Some(match_byte! { b,
        b'0' ... b'9' => { b - b'0' },
        b'a' ... b'f' => { b - b'a' + 10 },
        b'A' ... b'F' => { b - b'A' + 10 },
        _ => {
            return None
        }
    } as u32)
}

fn byte_to_decimal_digit(b: u8) -> Option<u32> {
    if b >= b'0' && b <= b'9' {
        Some((b - b'0') as u32)
    } else {
        None
    }
}

fn consume_numeric<'a>(tokenizer: &mut Tokenizer<'a>) -> Token<'a> {
    // Parse [+-]?\d*(\.\d+)?([eE][+-]?\d+)?
    // But this is always called so that there is at least one digit in \d*(\.\d+)?

    // Do all the math in f64 so that large numbers overflow to +/-inf
    // and i32::{MIN, MAX} are within range.

    let (has_sign, sign) = match tokenizer.next_byte_unchecked() {
        b'-' => (true, -1.),
        b'+' => (true, 1.),
        _ => (false, 1.),
    };
    if has_sign {
        tokenizer.advance(1);
    }

    let mut integral_part: f64 = 0.;
    while let Some(digit) = byte_to_decimal_digit(tokenizer.next_byte_unchecked()) {
        integral_part = integral_part * 10. + digit as f64;
        tokenizer.advance(1);
        if tokenizer.is_eof() {
            break
        }
    }

    let mut is_integer = true;

    let mut fractional_part: f64 = 0.;
    if tokenizer.has_at_least(1) && tokenizer.next_byte_unchecked() == b'.'
            && matches!(tokenizer.byte_at(1), b'0'...b'9') {
        is_integer = false;
        tokenizer.advance(1);  // Consume '.'
        let mut factor = 0.1;
        while let Some(digit) = byte_to_decimal_digit(tokenizer.next_byte_unchecked()) {
            fractional_part += digit as f64 * factor;
            factor *= 0.1;
            tokenizer.advance(1);
            if tokenizer.is_eof() {
                break
            }
        }
    }

    let mut value = sign * (integral_part + fractional_part);

    if tokenizer.has_at_least(1)
       && matches!(tokenizer.next_byte_unchecked(), b'e' | b'E') {

        if matches!(tokenizer.byte_at(1), b'0'...b'9') ||
           (tokenizer.has_at_least(2)
            && matches!(tokenizer.byte_at(1), b'+' | b'-')
            && matches!(tokenizer.byte_at(2), b'0'...b'9'))
        {
            is_integer = false;
            tokenizer.advance(1);
            let (has_sign, sign) = match tokenizer.next_byte_unchecked() {
                b'-' => (true, -1.),
                b'+' => (true, 1.),
                _ => (false, 1.),
            };
            if has_sign {
                tokenizer.advance(1);
            }
            let mut exponent: f64 = 0.;
            while let Some(digit) = byte_to_decimal_digit(tokenizer.next_byte_unchecked()) {
                exponent = exponent * 10. + digit as f64;
                tokenizer.advance(1);
                if tokenizer.is_eof() {
                    break
                }
            }
            value *= f64::powf(10., sign * exponent);
        }
    }

    let int_value = if is_integer {
        Some(if value >= i32::MAX as f64 {
            i32::MAX
        } else if value <= i32::MIN as f64 {
            i32::MIN
        } else {
            value as i32
        })
    } else {
        None
    };

    if !tokenizer.is_eof() && tokenizer.next_byte_unchecked() == b'%' {
        tokenizer.advance(1);
        return Percentage {
            unit_value: (value / 100.) as f32,
            int_value: int_value,
            has_sign: has_sign,
        }
    }
    let value = value as f32;
    if is_ident_start(tokenizer) {
        let unit = consume_name(tokenizer);
        if tokenizer.viewport_percentages == SeenStatus::LookingForThem {
            if unit.eq_ignore_ascii_case("vh") ||
               unit.eq_ignore_ascii_case("vw") ||
               unit.eq_ignore_ascii_case("vmin") ||
               unit.eq_ignore_ascii_case("vmax") {
                   tokenizer.viewport_percentages = SeenStatus::SeenAtLeastOne;
           }
        }
        Dimension {
            value: value,
            int_value: int_value,
            has_sign: has_sign,
            unit: unit,
        }
    } else {
        Number {
            value: value,
            int_value: int_value,
            has_sign: has_sign,
        }
    }
}


#[inline]
unsafe fn from_utf8_release_unchecked(string_bytes: Vec<u8>) -> String {
    if cfg!(debug_assertions) {
        String::from_utf8(string_bytes).unwrap()
    } else {
        String::from_utf8_unchecked(string_bytes)
    }
}

fn consume_unquoted_url<'a>(tokenizer: &mut Tokenizer<'a>) -> Result<Token<'a>, ()> {
    // This is only called after "url(", so the current position is a code point boundary.
    for (offset, c) in tokenizer.input[tokenizer.position..].bytes().enumerate() {
        match_byte! { c,
            b' ' | b'\t' | b'\n' | b'\r' | b'\x0C' => {},
            b'"' | b'\'' => { return Err(()) },  // Do not advance
            b')' => {
                tokenizer.advance(offset + 1);
                return Ok(UnquotedUrl("".into()));
            }
            _ => {
                tokenizer.advance(offset);
                // This function only consumed ASCII (whitespace) bytes,
                // so the current position is a code point boundary.
                return Ok(consume_unquoted_url_internal(tokenizer))
            }
        }
    }
    tokenizer.position = tokenizer.input.len();
    return Ok(UnquotedUrl("".into()));

    fn consume_unquoted_url_internal<'a>(tokenizer: &mut Tokenizer<'a>) -> Token<'a> {
        // This function is only called with start_pos at a code point boundary.
        let start_pos = tokenizer.position();
        let mut string_bytes: Vec<u8>;
        loop {
            if tokenizer.is_eof() {
                return UnquotedUrl(tokenizer.slice_from(start_pos).into())
            }
            match_byte! { tokenizer.next_byte_unchecked(),
                b' ' | b'\t' | b'\n' | b'\r' | b'\x0C' => {
                    let value = tokenizer.slice_from(start_pos);
                    tokenizer.advance(1);
                    return consume_url_end(tokenizer, value.into())
                }
                b')' => {
                    let value = tokenizer.slice_from(start_pos);
                    tokenizer.advance(1);
                    return UnquotedUrl(value.into())
                }
                b'\x01'...b'\x08' | b'\x0B' | b'\x0E'...b'\x1F' | b'\x7F'  // non-printable
                    | b'"' | b'\'' | b'(' => {
                    tokenizer.advance(1);
                    return consume_bad_url(tokenizer)
                },
                b'\\' | b'\0' => {
                    // * The tokenizer’s input is UTF-8 since it’s `&str`.
                    // * start_pos is at a code point boundary
                    // * so is the current position (which is before '\\' or '\0'
                    //
                    // So `string_bytes` is well-formed UTF-8.
                    string_bytes = tokenizer.slice_from(start_pos).as_bytes().to_owned();
                    break
                }
                _ => {
                    tokenizer.consume_byte();
                }
            }
        }
        while !tokenizer.is_eof() {
            match_byte! { tokenizer.consume_byte(),
                b' ' | b'\t' | b'\n' | b'\r' | b'\x0C' => {
                    return consume_url_end(
                        tokenizer,
                        // string_bytes is well-formed UTF-8, see other comments.
                        unsafe { from_utf8_release_unchecked(string_bytes) }.into()
                    )
                }
                b')' => {
                    break;
                }
                b'\x01'...b'\x08' | b'\x0B' | b'\x0E'...b'\x1F' | b'\x7F'  // non-printable
                    | b'"' | b'\'' | b'(' => {
                    return consume_bad_url(tokenizer);
                }
                b'\\' => {
                    if tokenizer.has_newline_at(0) {
                        return consume_bad_url(tokenizer)
                    }

                    // This pushes one well-formed code point to string_bytes
                    consume_escape_and_write(tokenizer, &mut string_bytes)
                },
                b'\0' => {
                    string_bytes.extend("\u{FFFD}".as_bytes());
                }
                // If this byte is part of a multi-byte code point,
                // we’ll end up copying the whole code point before this loop does something else.
                b => { string_bytes.push(b) }
            }
        }
        UnquotedUrl(
            // string_bytes is well-formed UTF-8, see other comments.
            unsafe { from_utf8_release_unchecked(string_bytes) }.into()
        )
    }

    fn consume_url_end<'a>(tokenizer: &mut Tokenizer<'a>, string: CompactCowStr<'a>) -> Token<'a> {
        while !tokenizer.is_eof() {
            match_byte! { tokenizer.consume_byte(),
                b' ' | b'\t' | b'\n' | b'\r' | b'\x0C' => {},
                b')' => { break },
                _ => {
                    return consume_bad_url(tokenizer);
                }
            }
        }
        UnquotedUrl(string)
    }

    fn consume_bad_url<'a>(tokenizer: &mut Tokenizer<'a>) -> Token<'a> {
        // Consume up to the closing )
        while !tokenizer.is_eof() {
            match_byte! { tokenizer.consume_byte(),
                b')' => { break },
                b'\\' => {
                    tokenizer.advance(1); // Skip an escaped ')' or '\'
                }
                _ => {},
            }
        }
        BadUrl
    }
}

// (value, number of digits up to 6)
fn consume_hex_digits<'a>(tokenizer: &mut Tokenizer<'a>) -> (u32, u32) {
    let mut value = 0;
    let mut digits = 0;
    while digits < 6 && !tokenizer.is_eof() {
        match byte_to_hex_digit(tokenizer.next_byte_unchecked()) {
            Some(digit) => {
                value = value * 16 + digit;
                digits += 1;
                tokenizer.advance(1);
            }
            None => break
        }
    }
    (value, digits)
}


// Same constraints as consume_escape except it writes into `bytes` the result
// instead of returning it.
fn consume_escape_and_write(tokenizer: &mut Tokenizer, bytes: &mut Vec<u8>) {
    bytes.extend(consume_escape(tokenizer).encode_utf8(&mut [0; 4]).as_bytes())
}

// Assumes that the U+005C REVERSE SOLIDUS (\) has already been consumed
// and that the next input character has already been verified
// to not be a newline.
fn consume_escape(tokenizer: &mut Tokenizer) -> char {
    if tokenizer.is_eof() { return '\u{FFFD}' }  // Escaped EOF
    match_byte! { tokenizer.next_byte_unchecked(),
        b'0'...b'9' | b'A'...b'F' | b'a'...b'f' => {
            let (c, _) = consume_hex_digits(tokenizer);
            if !tokenizer.is_eof() {
                match tokenizer.next_byte_unchecked() {
                    b' ' | b'\t' | b'\n' | b'\x0C' => tokenizer.advance(1),
                    b'\r' => {
                        tokenizer.advance(1);
                        if !tokenizer.is_eof() && tokenizer.next_byte_unchecked() == b'\n' {
                            tokenizer.advance(1);
                        }
                    }
                    _ => ()
                }
            }
            static REPLACEMENT_CHAR: char = '\u{FFFD}';
            if c != 0 {
                let c = char::from_u32(c);
                c.unwrap_or(REPLACEMENT_CHAR)
            } else {
                REPLACEMENT_CHAR
            }
        },
        b'\0' => {
            tokenizer.advance(1);
            '\u{FFFD}'
        }
        _ => { tokenizer.consume_char() }
    }
}
