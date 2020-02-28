//! JavaScript lexer.

use crate::parser::Parser;
use ast::SourceLocation;
use bumpalo::{collections::String, Bump};
use generated_parser::{ParseError, Result, TerminalId, Token};
use std::convert::TryFrom;
use std::str::Chars;
use unic_ucd_ident::{is_id_continue, is_id_start};

pub struct Lexer<'alloc> {
    allocator: &'alloc Bump,

    /// Length of the input text, in UTF-8 bytes.
    source_length: usize,

    /// Iterator over the remaining not-yet-parsed input.
    chars: Chars<'alloc>,

    /// True if the current position is before the first
    /// token of a line (or on a line with no tokens).
    is_on_new_line: bool,
}

impl<'alloc> Lexer<'alloc> {
    pub fn new(allocator: &'alloc Bump, chars: Chars<'alloc>) -> Lexer<'alloc> {
        Self::with_offset(allocator, chars, 0)
    }

    /// Create a lexer for a part of a JS script or module. `offset` is the
    /// total length of all previous parts, in bytes; source locations for
    /// tokens created by the new lexer start counting from this number.
    pub fn with_offset(
        allocator: &'alloc Bump,
        chars: Chars<'alloc>,
        offset: usize,
    ) -> Lexer<'alloc> {
        let source_length = offset + chars.as_str().len();
        Lexer {
            allocator,
            source_length,
            chars,
            is_on_new_line: true,
        }
    }

    fn is_looking_at(&self, s: &str) -> bool {
        self.chars.as_str().starts_with(s)
    }

    pub fn offset(&self) -> usize {
        self.source_length - self.chars.as_str().len()
    }

    fn peek(&self) -> Option<char> {
        self.chars.as_str().chars().next()
    }

    fn double_peek(&self) -> Option<char> {
        let mut chars = self.chars.as_str().chars();
        chars.next();
        chars.next()
    }

    pub fn next<'parser>(&mut self, parser: &Parser<'parser>) -> Result<'alloc, Token<'alloc>> {
        let (loc, value, terminal_id) = self.advance_impl(parser)?;
        let is_on_new_line = self.is_on_new_line;
        self.is_on_new_line = false;
        Ok(Token {
            terminal_id,
            loc,
            is_on_new_line,
            value,
        })
    }

    fn unexpected_err(&mut self) -> ParseError<'alloc> {
        if let Some(ch) = self.peek() {
            ParseError::IllegalCharacter(ch)
        } else {
            ParseError::UnexpectedEnd
        }
    }
}

// ----------------------------------------------------------------------------
// 11.1 Unicode Format-Control Characters

/// U+200C ZERO WIDTH NON-JOINER, abbreviated in the spec as <ZWNJ>.
/// Specially permitted in identifiers.
const ZWNJ: char = '\u{200c}';

/// U+200D ZERO WIDTH JOINER, abbreviated as <ZWJ>.
/// Specially permitted in identifiers.
const ZWJ: char = '\u{200d}';

/// U+FEFF ZERO WIDTH NO-BREAK SPACE, abbreviated <ZWNBSP>.
/// Considered a whitespace character in JS.
const ZWNBSP: char = '\u{feff}';

// ----------------------------------------------------------------------------
// 11.2 White Space

/// U+0009 CHARACTER TABULATION, abbreviated <TAB>.
const TAB: char = '\u{9}';

/// U+000B VERTICAL TAB, abbreviated <VT>.
const VT: char = '\u{b}';

/// U+000C FORM FEED, abbreviated <FF>.
const FF: char = '\u{c}';

/// U+0020 SPACE, abbreviated <SP>.
const SP: char = '\u{20}';

/// U+00A0 NON-BREAKING SPACE, abbreviated <NBSP>.
const NBSP: char = '\u{a0}';

// ----------------------------------------------------------------------------
// 11.3 Line Terminators

///  U+000A LINE FEED, abbreviated in the spec as <LF>.
const LF: char = '\u{a}';

/// U+000D CARRIAGE RETURN, abbreviated in the spec as <CR>.
const CR: char = '\u{d}';

/// U+2028 LINE SEPARATOR, abbreviated <LS>.
const LS: char = '\u{2028}';

/// U+2029 PARAGRAPH SEPARATOR, abbreviated <PS>.
const PS: char = '\u{2029}';

// ----------------------------------------------------------------------------
// 11.4 Comments
//
// Comment::
//     MultiLineComment
//     SingleLineComment

impl<'alloc> Lexer<'alloc> {
    /// Skip a *MultiLineComment*.
    ///
    /// ```text
    /// MultiLineComment ::
    ///     `/*` MultiLineCommentChars? `*/`
    ///
    /// MultiLineCommentChars ::
    ///     MultiLineNotAsteriskChar MultiLineCommentChars?
    ///     `*` PostAsteriskCommentChars?
    ///
    /// PostAsteriskCommentChars ::
    ///     MultiLineNotForwardSlashOrAsteriskChar MultiLineCommentChars?
    ///     `*` PostAsteriskCommentChars?
    ///
    /// MultiLineNotAsteriskChar ::
    ///     SourceCharacter but not `*`
    ///
    /// MultiLineNotForwardSlashOrAsteriskChar ::
    ///     SourceCharacter but not one of `/` or `*`
    /// ```
    ///
    /// (B.1.3 splits MultiLineComment into two nonterminals: MultiLineComment
    /// and SingleLineDelimitedComment. The point of that is to help specify
    /// that a SingleLineHTMLCloseComment must occur at the start of a line. We
    /// use `is_on_new_line` for that.)
    ///
    fn skip_multi_line_comment(&mut self, builder: &mut AutoCow<'alloc>) -> Result<'alloc, ()> {
        while let Some(ch) = self.chars.next() {
            match ch {
                '*' if self.peek() == Some('/') => {
                    self.chars.next();
                    *builder = AutoCow::new(&self);
                    return Ok(());
                }
                CR | LF | PS | LS => {
                    self.is_on_new_line = true;
                }
                _ => {}
            }
        }
        Err(ParseError::UnterminatedMultiLineComment)
    }

    /// Skip a *SingleLineComment* and the following *LineTerminatorSequence*,
    /// if any.
    ///
    /// ```text
    /// SingleLineComment ::
    ///     `//` SingleLineCommentChars?
    ///
    /// SingleLineCommentChars ::
    ///     SingleLineCommentChar SingleLineCommentChars?
    ///
    /// SingleLineCommentChar ::
    ///     SourceCharacter but not LineTerminator
    /// ```
    fn skip_single_line_comment(&mut self, builder: &mut AutoCow<'alloc>) {
        while let Some(ch) = self.chars.next() {
            match ch {
                CR | LF | LS | PS => break,
                _ => continue,
            }
        }
        *builder = AutoCow::new(&self);
        self.is_on_new_line = true;
    }
}

// ----------------------------------------------------------------------------
// 11.6 Names and Keywords

/// True if `c` is a one-character *IdentifierStart*.
///
/// ```text
/// IdentifierStart ::
///     UnicodeIDStart
///     `$`
///     `_`
///     `\` UnicodeEscapeSequence
///
/// UnicodeIDStart ::
///     > any Unicode code point with the Unicode property "ID_Start"
/// ```
fn is_identifier_start(c: char) -> bool {
    // Escaped case is handled separately.
    if c.is_ascii() {
        c == '$' || c == '_' || c.is_ascii_alphabetic()
    } else {
        is_id_start(c)
    }
}

/// True if `c` is a one-character *IdentifierPart*.
///
/// ```text
/// IdentifierPart ::
///     UnicodeIDContinue
///     `$`
///     `\` UnicodeEscapeSequence
///     <ZWNJ>
///     <ZWJ>
///
/// UnicodeIDContinue ::
///     > any Unicode code point with the Unicode property "ID_Continue"
/// ```
fn is_identifier_part(c: char) -> bool {
    // Escaped case is handled separately.
    if c.is_ascii() {
        c == '$' || c == '_' || c.is_ascii_alphanumeric()
    } else {
        is_id_continue(c) || c == ZWNJ || c == ZWJ
    }
}

impl<'alloc> Lexer<'alloc> {
    /// Scan the rest of an IdentifierName, having already parsed the initial
    /// IdentifierStart and stored it in `builder`.
    ///
    /// On success, this returns `Ok((has_escapes, str))`, where `has_escapes`
    /// is true if the identifier contained any UnicodeEscapeSequences, and
    /// `str` is the un-escaped IdentifierName, including the IdentifierStart,
    /// on success.
    ///
    /// ```text
    /// IdentifierName ::
    ///     IdentifierStart
    ///     IdentifierName IdentifierPart
    /// ```
    fn identifier_name_tail(
        &mut self,
        mut builder: AutoCow<'alloc>,
    ) -> Result<'alloc, (bool, &'alloc str)> {
        while let Some(ch) = self.peek() {
            if !is_identifier_part(ch) {
                if ch == '\\' {
                    self.chars.next();
                    builder.force_allocation_without_current_ascii_char(&self);

                    let value = self.unicode_escape_sequence_after_backslash()?;
                    if !is_identifier_part(value) {
                        return Err(ParseError::InvalidEscapeSequence);
                    }

                    builder.push_different(value);
                    continue;
                }

                break;
            }
            self.chars.next();
            builder.push_matching(ch);
        }
        let has_different = builder.has_different();
        Ok((has_different, builder.finish(&self)))
    }

    fn identifier_name(&mut self, mut builder: AutoCow<'alloc>) -> Result<'alloc, &'alloc str> {
        match self.chars.next() {
            None => {
                return Err(ParseError::UnexpectedEnd);
            }
            Some(c) => {
                match c {
                    '$' | '_' | 'a'..='z' | 'A'..='Z' => {
                        builder.push_matching(c);
                    }

                    '\\' => {
                        builder.force_allocation_without_current_ascii_char(&self);

                        let value = self.unicode_escape_sequence_after_backslash()?;
                        if !is_identifier_start(value) {
                            return Err(ParseError::IllegalCharacter(value));
                        }
                        builder.push_different(value);
                    }

                    other if is_identifier_start(other) => {
                        builder.push_matching(other);
                    }

                    other => {
                        return Err(ParseError::IllegalCharacter(other));
                    }
                }
                self.identifier_name_tail(builder)
                    .map(|(_has_escapes, name)| name)
            }
        }
    }

    /// Finish scanning an *IdentifierName* or keyword, having already scanned
    /// the *IdentifierStart* and pushed it to `builder`.
    ///
    /// `start` is the offset of the *IdentifierStart*.
    ///
    /// The lexer doesn't know the syntactic context, so it always identifies
    /// possible keywords. It's up to the parser to understand that, for
    /// example, `TerminalId::If` is not a keyword when it's used as a property
    /// or method name.
    ///
    /// If the source string contains no escape and it matches to possible
    /// keywords (including contextual keywords), the result is corresponding
    /// `TerminalId`.  For example, if the source string is "yield", the result
    /// is `TerminalId::Yield`.
    ///
    /// If the source string contains no escape sequence and also it doesn't
    /// match to any possible keywords, the result is `TerminalId::Name`.
    ///
    /// If the source string contains at least one escape sequence,
    /// the result is always `TerminalId::NameWithEscape`, regardless of the
    /// StringValue of it. For example, if the source string is "\u{79}ield",
    /// the result is `TerminalId::NameWithEscape`, and the StringValue is
    /// "yield".
    fn identifier_tail(
        &mut self,
        start: usize,
        builder: AutoCow<'alloc>,
    ) -> Result<'alloc, (SourceLocation, Option<&'alloc str>, TerminalId)> {
        let (has_different, text) = self.identifier_name_tail(builder)?;

        // https://tc39.es/ecma262/#sec-keywords-and-reserved-words
        //
        // keywords in the grammar match literal sequences of specific
        // SourceCharacter elements. A code point in a keyword cannot be
        // expressed by a `\` UnicodeEscapeSequence.
        let id = if has_different {
            // Always return `NameWithEscape`.
            //
            // Error check against reserved word should be handled in the
            // consumer.
            TerminalId::NameWithEscape
        } else {
            match &text as &str {
                "as" => TerminalId::As,
                "async" => {
                    //TerminalId::Async
                    return Err(ParseError::NotImplemented(
                        "async cannot be handled in parser due to multiple lookahead",
                    ));
                }
                "await" => {
                    //TerminalId::Await
                    return Err(ParseError::NotImplemented(
                        "await cannot be handled in parser",
                    ));
                }
                "break" => TerminalId::Break,
                "case" => TerminalId::Case,
                "catch" => TerminalId::Catch,
                "class" => TerminalId::Class,
                "const" => TerminalId::Const,
                "continue" => TerminalId::Continue,
                "debugger" => TerminalId::Debugger,
                "default" => TerminalId::Default,
                "delete" => TerminalId::Delete,
                "do" => TerminalId::Do,
                "else" => TerminalId::Else,
                "export" => TerminalId::Export,
                "extends" => TerminalId::Extends,
                "finally" => TerminalId::Finally,
                "for" => TerminalId::For,
                "from" => TerminalId::From,
                "function" => TerminalId::Function,
                "get" => TerminalId::Get,
                "if" => TerminalId::If,
                "implements" => TerminalId::Implements,
                "import" => TerminalId::Import,
                "in" => TerminalId::In,
                "instanceof" => TerminalId::Instanceof,
                "interface" => TerminalId::Interface,
                "let" => {
                    //TerminalId::Let,
                    return Err(ParseError::NotImplemented(
                        "let cannot be handled in parser due to multiple lookahead",
                    ));
                }
                "new" => TerminalId::New,
                "of" => TerminalId::Of,
                "package" => TerminalId::Package,
                "private" => TerminalId::Private,
                "protected" => TerminalId::Protected,
                "public" => TerminalId::Public,
                "return" => TerminalId::Return,
                "set" => TerminalId::Set,
                "static" => TerminalId::Static,
                "super" => TerminalId::Super,
                "switch" => TerminalId::Switch,
                "target" => TerminalId::Target,
                "this" => TerminalId::This,
                "throw" => TerminalId::Throw,
                "try" => TerminalId::Try,
                "typeof" => TerminalId::Typeof,
                "var" => TerminalId::Var,
                "void" => TerminalId::Void,
                "while" => TerminalId::While,
                "with" => TerminalId::With,
                "yield" => {
                    //TerminalId::Yield
                    return Err(ParseError::NotImplemented(
                        "yield cannot be handled in parser",
                    ));
                }
                "null" => TerminalId::NullLiteral,
                "true" | "false" => TerminalId::BooleanLiteral,
                _ => TerminalId::Name,
            }
        };

        Ok((SourceLocation::new(start, self.offset()), Some(text), id))
    }

    /// ```text
    /// PrivateIdentifier::
    ///     `#` IdentifierName
    /// ```
    fn private_identifier(
        &mut self,
        start: usize,
        builder: AutoCow<'alloc>,
    ) -> Result<'alloc, (SourceLocation, Option<&'alloc str>, TerminalId)> {
        let name = self.identifier_name(builder)?;
        Ok((
            SourceLocation::new(start, self.offset()),
            Some(name),
            TerminalId::PrivateIdentifier,
        ))
    }

    /// ```text
    /// UnicodeEscapeSequence::
    ///     `u` Hex4Digits
    ///     `u{` CodePoint `}`
    /// ```
    fn unicode_escape_sequence_after_backslash(&mut self) -> Result<'alloc, char> {
        match self.chars.next() {
            Some('u') => {}
            _ => {
                return Err(ParseError::InvalidEscapeSequence);
            }
        }
        self.unicode_escape_sequence_after_backslash_and_u()
    }

    fn unicode_escape_sequence_after_backslash_and_u(&mut self) -> Result<'alloc, char> {
        let value = match self.peek() {
            Some('{') => {
                self.chars.next();

                let value = self.code_point()?;
                match self.chars.next() {
                    Some('}') => {}
                    _ => {
                        return Err(ParseError::InvalidEscapeSequence);
                    }
                }
                value
            }
            _ => self.hex_4_digits()?,
        };

        Ok(value)
    }
}

enum NumericType {
    Normal,
    BigInt,
}

impl<'alloc> Lexer<'alloc> {
    // ------------------------------------------------------------------------
    // 11.8.3 Numeric Literals

    /// Advance over decimal digits in the input, returning true if any were
    /// found.
    ///
    /// ```text
    /// NumericLiteralSeparator::
    ///     `_`
    ///
    /// DecimalDigits ::
    ///     DecimalDigit
    ///     DecimalDigits NumericLiteralSeparator? DecimalDigit
    ///
    /// DecimalDigit :: one of
    ///     `0` `1` `2` `3` `4` `5` `6` `7` `8` `9`
    /// ```
    fn decimal_digits(&mut self) -> Result<'alloc, bool> {
        if let Some('0'..='9') = self.peek() {
            self.chars.next();
        } else {
            return Ok(false);
        }

        self.decimal_digits_after_first_digit()?;
        Ok(true)
    }

    fn decimal_digits_after_first_digit(&mut self) -> Result<'alloc, ()> {
        while let Some(next) = self.peek() {
            match next {
                '_' => {
                    self.chars.next();

                    if let Some('0'..='9') = self.peek() {
                        self.chars.next();
                    } else {
                        return Err(self.unexpected_err());
                    }
                }
                '0'..='9' => {
                    self.chars.next();
                }
                _ => break,
            }
        }
        Ok(())
    }

    /// Skip an ExponentPart, if present.
    ///
    /// ```text
    /// ExponentPart ::
    ///     ExponentIndicator SignedInteger
    ///
    /// ExponentIndicator :: one of
    ///     `e` `E`
    ///
    /// SignedInteger ::
    ///     DecimalDigits
    ///     `+` DecimalDigits
    ///     `-` DecimalDigits
    /// ```
    fn optional_exponent(&mut self) -> Result<'alloc, ()> {
        if let Some('e') | Some('E') = self.peek() {
            self.chars.next();

            if let Some('+') | Some('-') = self.peek() {
                self.chars.next();
            }
            if !self.decimal_digits()? {
                // require at least one digit
                return Err(self.unexpected_err());
            }
        }
        Ok(())
    }

    /// ```text
    /// HexDigit :: one of
    ///     `0` `1` `2` `3` `4` `5` `6` `7` `8` `9` `a` `b` `c` `d` `e` `f` `A` `B` `C` `D` `E` `F`
    /// ```
    fn hex_digit(&mut self) -> Result<'alloc, u32> {
        match self.chars.next() {
            None => Err(ParseError::InvalidEscapeSequence),
            Some(c @ '0'..='9') => Ok(c as u32 - '0' as u32),
            Some(c @ 'a'..='f') => Ok(10 + (c as u32 - 'a' as u32)),
            Some(c @ 'A'..='F') => Ok(10 + (c as u32 - 'A' as u32)),
            Some(other) => Err(ParseError::IllegalCharacter(other)),
        }
    }

    fn code_point_to_char(value: u32) -> Result<'alloc, char> {
        if 0xd800 <= value && value <= 0xdfff {
            Err(ParseError::NotImplemented(
                "unicode escape sequences (surrogates)",
            ))
        } else {
            char::try_from(value).map_err(|_| ParseError::InvalidEscapeSequence)
        }
    }

    /// ```text
    /// Hex4Digits ::
    ///     HexDigit HexDigit HexDigit HexDigit
    /// ```
    fn hex_4_digits(&mut self) -> Result<'alloc, char> {
        let mut value = 0;
        for _ in 0..4 {
            value = (value << 4) | self.hex_digit()?;
        }
        Self::code_point_to_char(value)
    }

    /// ```text
    /// CodePoint ::
    ///     HexDigits but only if MV of HexDigits ≤ 0x10FFFF
    ///
    /// HexDigits ::
    ///    HexDigit
    ///    HexDigits HexDigit
    /// ```
    fn code_point(&mut self) -> Result<'alloc, char> {
        let mut value = self.hex_digit()?;

        loop {
            let next = match self.peek() {
                None => {
                    return Err(ParseError::InvalidEscapeSequence);
                }
                Some(c @ '0'..='9') => c as u32 - '0' as u32,
                Some(c @ 'a'..='f') => 10 + (c as u32 - 'a' as u32),
                Some(c @ 'A'..='F') => 10 + (c as u32 - 'A' as u32),
                Some(_) => break,
            };
            self.chars.next();
            value = (value << 4) | next;
            if value > 0x10FFFF {
                return Err(ParseError::InvalidEscapeSequence);
            }
        }

        Self::code_point_to_char(value)
    }

    /// Scan a NumericLiteral (defined in 11.8.3, extended by B.1.1) after
    /// having already consumed the first character, which was `0`.
    ///
    /// ```text
    /// NumericLiteral ::
    ///     DecimalLiteral
    ///     DecimalBigIntegerLiteral
    ///     NonDecimalIntegerLiteral
    ///     NonDecimalIntegerLiteral BigIntLiteralSuffix
    ///
    /// DecimalBigIntegerLiteral ::
    ///     `0` BigIntLiteralSuffix
    ///     NonZeroDigit DecimalDigits? BigIntLiteralSuffix
    ///
    /// NonDecimalIntegerLiteral ::
    ///     BinaryIntegerLiteral
    ///     OctalIntegerLiteral
    ///     HexIntegerLiteral
    ///
    /// BigIntLiteralSuffix ::
    ///     `n`
    /// ```
    fn numeric_literal_starting_with_zero(&mut self) -> Result<'alloc, NumericType> {
        match self.peek() {
            // BinaryIntegerLiteral ::
            //     `0b` BinaryDigits
            //     `0B` BinaryDigits
            //
            // BinaryDigits ::
            //     BinaryDigit
            //     BinaryDigits NumericLiteralSeparator? BinaryDigit
            //
            // BinaryDigit :: one of
            //     `0` `1`
            Some('b') | Some('B') => {
                self.chars.next();

                if let Some('0'..='1') = self.peek() {
                    self.chars.next();
                } else {
                    return Err(self.unexpected_err());
                }

                while let Some(next) = self.peek() {
                    match next {
                        '_' => {
                            self.chars.next();

                            if let Some('0'..='1') = self.peek() {
                                self.chars.next();
                            } else {
                                return Err(self.unexpected_err());
                            }
                        }
                        '0'..='1' => {
                            self.chars.next();
                        }
                        _ => break,
                    }
                }

                if let Some('n') = self.peek() {
                    self.chars.next();
                    self.check_after_numeric_literal()?;
                    return Ok(NumericType::BigInt);
                }
            }

            // OctalIntegerLiteral ::
            //     `0o` OctalDigits
            //     `0O` OctalDigits
            //
            // OctalDigits ::
            //     OctalDigit
            //     OctalDigits NumericLiteralSeparator? OctalDigit
            //
            // OctalDigit :: one of
            //     `0` `1` `2` `3` `4` `5` `6` `7`
            //
            Some('o') | Some('O') => {
                self.chars.next();

                if let Some('0'..='7') = self.peek() {
                    self.chars.next();
                } else {
                    return Err(self.unexpected_err());
                }

                while let Some(next) = self.peek() {
                    match next {
                        '_' => {
                            self.chars.next();

                            if let Some('0'..='7') = self.peek() {
                                self.chars.next();
                            } else {
                                return Err(self.unexpected_err());
                            }
                        }
                        '0'..='7' => {
                            self.chars.next();
                        }
                        _ => break,
                    }
                }

                if let Some('n') = self.peek() {
                    self.chars.next();
                    self.check_after_numeric_literal()?;
                    return Ok(NumericType::BigInt);
                }
            }

            // HexIntegerLiteral ::
            //     `0x` HexDigits
            //     `0X` HexDigits
            //
            // HexDigits ::
            //     HexDigit
            //     HexDigits NumericLiteralSeparator? HexDigit
            //
            // HexDigit :: one of
            //     `0` `1` `2` `3` `4` `5` `6` `7` `8` `9` `a` `b` `c` `d` `e` `f` `A` `B` `C` `D` `E` `F`
            Some('x') | Some('X') => {
                self.chars.next();

                if let Some('0'..='9') | Some('a'..='f') | Some('A'..='F') = self.peek() {
                    self.chars.next();
                } else {
                    return Err(self.unexpected_err());
                }

                while let Some(next) = self.peek() {
                    match next {
                        '_' => {
                            self.chars.next();

                            if let Some('0'..='9') | Some('a'..='f') | Some('A'..='F') = self.peek()
                            {
                                self.chars.next();
                            } else {
                                return Err(self.unexpected_err());
                            }
                        }
                        '0'..='9' | 'a'..='f' | 'A'..='F' => {
                            self.chars.next();
                        }
                        _ => break,
                    }
                }

                if let Some('n') = self.peek() {
                    self.chars.next();
                    self.check_after_numeric_literal()?;
                    return Ok(NumericType::BigInt);
                }
            }

            Some('.') | Some('e') | Some('E') => {
                return self.decimal_literal();
            }

            Some('n') => {
                self.chars.next();
                self.check_after_numeric_literal()?;
                return Ok(NumericType::BigInt);
            }

            Some('0'..='9') => {
                // This is almost always the token `0` in practice.
                //
                // In nonstrict code, as a legacy feature, other numbers
                // starting with `0` are allowed. If /0[0-7]+/ matches, it's a
                // LegacyOctalIntegerLiteral; but if we see an `8` or `9` in
                // the number, it's decimal. Decimal numbers can have a decimal
                // point and/or ExponentPart; octals can't.
                //
                // Neither is allowed with a BigIntLiteralSuffix `n`.
                //
                // LegacyOctalIntegerLiteral ::
                //     `0` OctalDigit
                //     LegacyOctalIntegerLiteral OctalDigit
                //
                // NonOctalDecimalIntegerLiteral ::
                //     `0` NonOctalDigit
                //     LegacyOctalLikeDecimalIntegerLiteral NonOctalDigit
                //     NonOctalDecimalIntegerLiteral DecimalDigit
                //
                // LegacyOctalLikeDecimalIntegerLiteral ::
                //     `0` OctalDigit
                //     LegacyOctalLikeDecimalIntegerLiteral OctalDigit
                //
                // NonOctalDigit :: one of
                //     `8` `9`
                //

                // TODO: implement `strict_mode` check
                // let strict_mode = true;
                // if !strict_mode {
                //     // TODO: Distinguish between Octal and NonOctalDecimal.
                //     // TODO: Support NonOctalDecimal followed by a decimal
                //     //       point and/or ExponentPart.
                //     self.decimal_digits()?;
                // }
                return Err(ParseError::NotImplemented("LegacyOctalIntegerLiteral"));
            }

            _ => {}
        }

        self.check_after_numeric_literal()?;
        Ok(NumericType::Normal)
    }

    /// Scan a NumericLiteral (defined in 11.8.3, extended by B.1.1).
    fn decimal_literal(&mut self) -> Result<'alloc, NumericType> {
        // DecimalLiteral ::
        //     DecimalIntegerLiteral `.` DecimalDigits? ExponentPart?
        //     `.` DecimalDigits ExponentPart?
        //     DecimalIntegerLiteral ExponentPart?
        //
        // DecimalIntegerLiteral ::
        //     `0`   #see `numeric_literal_starting_with_zero`
        //     NonZeroDigit
        //     NonZeroDigit NumericLiteralSeparator? DecimalDigits
        //     NonOctalDecimalIntegerLiteral  #see `numeric_literal_
        //                                    #     starting_with_zero`
        //
        // NonZeroDigit :: one of
        //     `1` `2` `3` `4` `5` `6` `7` `8` `9`

        self.decimal_digits()?;
        self.decimal_literal_after_digits()
    }

    /// Scan a NumericLiteral (defined in 11.8.3, extended by B.1.1) after
    /// having already consumed the first character, which is a decimal digit.
    fn decimal_literal_after_first_digit(&mut self) -> Result<'alloc, NumericType> {
        self.decimal_digits_after_first_digit()?;
        self.decimal_literal_after_digits()
    }

    fn decimal_literal_after_digits(&mut self) -> Result<'alloc, NumericType> {
        match self.peek() {
            Some('.') => {
                self.chars.next();
                self.decimal_digits()?;
            }
            Some('n') => {
                self.chars.next();
                self.check_after_numeric_literal()?;
                return Ok(NumericType::BigInt);
            }
            _ => {}
        }
        self.optional_exponent()?;
        self.check_after_numeric_literal()?;
        Ok(NumericType::Normal)
    }

    fn check_after_numeric_literal(&self) -> Result<'alloc, ()> {
        // The SourceCharacter immediately following a
        // NumericLiteral must not be an IdentifierStart or
        // DecimalDigit. (11.8.3)
        if let Some(ch) = self.peek() {
            if is_identifier_start(ch) || ch.is_digit(10) {
                return Err(ParseError::IllegalCharacter(ch));
            }
        }

        Ok(())
    }

    // ------------------------------------------------------------------------
    // 11.8.4 String Literals (as extended by B.1.2)

    /// Scan an LineContinuation or EscapeSequence in a string literal, having
    /// already consumed the initial backslash character.
    ///
    /// ```text
    /// LineContinuation ::
    ///     `\` LineTerminatorSequence
    ///
    /// EscapeSequence ::
    ///     CharacterEscapeSequence
    ///     (in strict mode code) `0` [lookahead ∉ DecimalDigit]
    ///     (in non-strict code) LegacyOctalEscapeSequence
    ///     HexEscapeSequence
    ///     UnicodeEscapeSequence
    ///
    /// CharacterEscapeSequence ::
    ///     SingleEscapeCharacter
    ///     NonEscapeCharacter
    ///
    /// SingleEscapeCharacter :: one of
    ///     `'` `"` `\` `b` `f` `n` `r` `t` `v`
    ///
    /// LegacyOctalEscapeSequence ::
    ///     OctalDigit [lookahead ∉ OctalDigit]
    ///     ZeroToThree OctalDigit [lookahead ∉ OctalDigit]
    ///     FourToSeven OctalDigit
    ///     ZeroToThree OctalDigit OctalDigit
    ///
    /// ZeroToThree :: one of
    ///     `0` `1` `2` `3`
    ///
    /// FourToSeven :: one of
    ///     `4` `5` `6` `7`
    /// ```
    fn escape_sequence(&mut self, text: &mut String<'alloc>) -> Result<'alloc, ()> {
        match self.chars.next() {
            None => {
                return Err(ParseError::UnterminatedString);
            }
            Some(c) => match c {
                LF | LS | PS => {
                    // LineContinuation. Ignore it.
                    //
                    // Don't set is_on_new_line because this LineContinuation
                    // has no bearing on whether the current string literal was
                    // the first token on the line where it started.
                }

                CR => {
                    // LineContinuation. Check for the sequence \r\n; otherwise
                    // ignore it.
                    if self.peek() == Some(LF) {
                        self.chars.next();
                    }
                }

                '\'' | '"' | '\\' => {
                    text.push(c);
                }

                'b' => {
                    text.push('\u{8}');
                }

                'f' => {
                    text.push(FF);
                }

                'n' => {
                    text.push(LF);
                }

                'r' => {
                    text.push(CR);
                }

                't' => {
                    text.push(TAB);
                }

                'v' => {
                    text.push(VT);
                }

                'x' => {
                    // HexEscapeSequence ::
                    //     `x` HexDigit HexDigit
                    let mut value = self.hex_digit()?;
                    value = (value << 4) | self.hex_digit()?;
                    match char::try_from(value) {
                        Err(_) => {
                            return Err(ParseError::InvalidEscapeSequence);
                        }
                        Ok(c) => {
                            text.push(c);
                        }
                    }
                }

                'u' => {
                    let c = self.unicode_escape_sequence_after_backslash_and_u()?;
                    text.push(c);
                }

                '0' => {
                    // In strict mode code and in template literals, the
                    // relevant production is
                    //
                    //     EscapeSequence ::
                    //         `0` [lookahead <! DecimalDigit]
                    //
                    // In non-strict StringLiterals, `\0` begins a
                    // LegacyOctalEscapeSequence which may contain more digits.
                    match self.peek() {
                        Some('0'..='7') => {
                            return Err(ParseError::NotImplemented(
                                "legacy octal escape sequence in string",
                            ));
                        }
                        Some('8'..='9') => {
                            return Err(ParseError::NotImplemented(
                                "digit immediately following \\0 escape sequence",
                            ));
                        }
                        _ => {}
                    }
                    text.push('\0');
                }

                '1'..='7' => {
                    return Err(ParseError::NotImplemented(
                        "legacy octal escape sequence in string",
                    ));
                }

                other => {
                    // "\8" and "\9" are invalid per spec, but SpiderMonkey and
                    // V8 accept them, and JSC accepts them in non-strict mode.
                    // "\8" is "8" and "\9" is "9".
                    text.push(other);
                }
            },
        }
        Ok(())
    }

    /// Scan a string literal, having already consumed the starting quote
    /// character `delimiter`.
    ///
    /// ```text
    /// StringLiteral ::
    ///     `"` DoubleStringCharacters? `"`
    ///     `'` SingleStringCharacters? `'`
    ///
    /// DoubleStringCharacters ::
    ///     DoubleStringCharacter DoubleStringCharacters?
    ///
    /// SingleStringCharacters ::
    ///     SingleStringCharacter SingleStringCharacters?
    ///
    /// DoubleStringCharacter ::
    ///     SourceCharacter but not one of `"` or `\` or LineTerminator
    ///     <LS>
    ///     <PS>
    ///     `\` EscapeSequence
    ///     LineContinuation
    ///
    /// SingleStringCharacter ::
    ///     SourceCharacter but not one of `'` or `\` or LineTerminator
    ///     <LS>
    ///     <PS>
    ///     `\` EscapeSequence
    ///     LineContinuation
    /// ```
    fn string_literal(
        &mut self,
        delimiter: char,
    ) -> Result<'alloc, (SourceLocation, Option<&'alloc str>, TerminalId)> {
        let offset = self.offset() - 1;
        let mut builder = AutoCow::new(&self);
        loop {
            match self.chars.next() {
                None | Some('\r') | Some('\n') => {
                    return Err(ParseError::UnterminatedString);
                }

                Some(c @ '"') | Some(c @ '\'') => {
                    if c == delimiter {
                        return Ok((
                            SourceLocation::new(offset, self.offset()),
                            Some(builder.finish_without_push(&self)),
                            TerminalId::StringLiteral,
                        ));
                    } else {
                        builder.push_matching(c);
                    }
                }

                Some('\\') => {
                    let text = builder.get_mut_string_without_current_ascii_char(&self);
                    self.escape_sequence(text)?;
                }

                Some(other) => {
                    // NonEscapeCharacter ::
                    //     SourceCharacter but not one of EscapeCharacter or LineTerminator
                    //
                    // EscapeCharacter ::
                    //     SingleEscapeCharacter
                    //     DecimalDigit
                    //     `x`
                    //     `u`
                    builder.push_matching(other);
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // 11.8.5 Regular Expression Literals

    fn regular_expression_backslash_sequence(
        &mut self,
        text: &mut String<'alloc>,
    ) -> Result<'alloc, ()> {
        text.push('\\');
        match self.chars.next() {
            None | Some(CR) | Some(LF) | Some(LS) | Some(PS) => Err(ParseError::UnterminatedRegExp),
            Some(c) => {
                text.push(c);
                Ok(())
            }
        }
    }

    // See 12.2.8 and 11.8.5 sections.
    fn regular_expression_literal(
        &mut self,
        builder: &mut AutoCow<'alloc>,
    ) -> Result<'alloc, (SourceLocation, Option<&'alloc str>, TerminalId)> {
        let offset = self.offset();

        loop {
            match self.chars.next() {
                None | Some(CR) | Some(LF) | Some(LS) | Some(PS) => {
                    return Err(ParseError::UnterminatedRegExp);
                }
                Some('/') => {
                    break;
                }
                Some('[') => {
                    // RegularExpressionClass.
                    builder.push_matching('[');
                    loop {
                        match self.chars.next() {
                            None | Some(CR) | Some(LF) | Some(LS) | Some(PS) => {
                                return Err(ParseError::UnterminatedRegExp);
                            }
                            Some(']') => {
                                break;
                            }
                            Some('\\') => {
                                let text = builder.get_mut_string_without_current_ascii_char(&self);
                                self.regular_expression_backslash_sequence(text)?;
                            }
                            Some(ch) => {
                                builder.push_matching(ch);
                            }
                        }
                    }
                    builder.push_matching(']');
                }
                Some('\\') => {
                    let text = builder.get_mut_string_without_current_ascii_char(&self);
                    self.regular_expression_backslash_sequence(text)?;
                }
                Some(ch) => {
                    builder.push_matching(ch);
                }
            }
        }
        builder.push_matching('/');
        let mut flag_text = AutoCow::new(&self);
        while let Some(ch) = self.peek() {
            match ch {
                '$' | '_' | 'a'..='z' | 'A'..='Z' | '0'..='9' => {
                    self.chars.next();
                    builder.push_matching(ch);
                    flag_text.push_matching(ch);
                }
                _ => break,
            }
        }

        // 12.2.8.2.1 Assert literal is a RegularExpressionLiteral.
        let literal = builder.finish(&self);

        // 12.2.8.2.2 Check that only gimsuy flags are mentioned at most once.
        let gimsuy_mask: u32 = ['g', 'i', 'm', 's', 'u', 'y']
            .iter()
            .map(|x| 1 << ((*x as u8) - ('a' as u8)))
            .sum();
        let mut flag_text_set: u32 = 0;
        for ch in flag_text.finish(&self).chars() {
            if !ch.is_ascii_lowercase() {
                return Err(ParseError::NotImplemented(
                    "Unexpected flag in regular expression literal",
                ));
            }
            let ch_mask = 1 << ((ch as u8) - ('a' as u8));
            if ch_mask & gimsuy_mask == 0 {
                return Err(ParseError::NotImplemented(
                    "Unexpected flag in regular expression literal",
                ));
            }
            if flag_text_set & ch_mask != 0 {
                return Err(ParseError::NotImplemented(
                    "Flag is mentioned twice in regular expression literal",
                ));
            }
            flag_text_set |= ch_mask;
        }

        // TODO: 12.2.8.2.4 and 12.2.8.2.5 Check that the body matches the
        // grammar defined in 21.2.1.

        Ok((
            SourceLocation::new(offset, self.offset()),
            Some(literal),
            TerminalId::RegularExpressionLiteral,
        ))
    }

    // ------------------------------------------------------------------------
    // 11.8.6 Template Literal Lexical Components

    /// Parse a template literal component token, having already consumed the
    /// starting `` ` `` or `}` character. On success, the `id` of the returned
    /// `Token` is `subst` (if the token ends with `${`) or `tail` (if the
    /// token ends with `` ` ``).
    ///
    /// ```text
    /// NoSubstitutionTemplate ::
    ///   ``` TemplateCharacters? ```
    ///
    /// TemplateHead ::
    ///   ``` TemplateCharacters? `${`
    ///
    /// TemplateMiddle ::
    ///   `}` TemplateCharacters? `${`
    ///
    /// TemplateTail ::
    ///   `}` TemplateCharacters? ```
    ///
    /// TemplateCharacters ::
    ///   TemplateCharacter TemplateCharacters?
    /// ```
    fn template_part(
        &mut self,
        start: usize,
        subst: TerminalId,
        tail: TerminalId,
    ) -> Result<'alloc, (SourceLocation, Option<&'alloc str>, TerminalId)> {
        let mut builder = AutoCow::new(&self);
        while let Some(ch) = self.chars.next() {
            // TemplateCharacter ::
            //   `$` [lookahead != `{` ]
            //   `\` EscapeSequence
            //   `\` NotEscapeSequence
            //   LineContinuation
            //   LineTerminatorSequence
            //   SourceCharacter but not one of ``` or `\` or `$` or LineTerminator
            //
            // NotEscapeSequence ::
            //   `0` DecimalDigit
            //   DecimalDigit but not `0`
            //   `x` [lookahead <! HexDigit]
            //   `x` HexDigit [lookahead <! HexDigit]
            //   `u` [lookahead <! HexDigit] [lookahead != `{`]
            //   `u` HexDigit [lookahead <! HexDigit]
            //   `u` HexDigit HexDigit [lookahead <! HexDigit]
            //   `u` HexDigit HexDigit HexDigit [lookahead <! HexDigit]
            //   `u` `{` [lookahead <! HexDigit]
            //   `u` `{` NotCodePoint [lookahead <! HexDigit]
            //   `u` `{` CodePoint [lookahead <! HexDigit] [lookahead != `}`]
            //
            // NotCodePoint ::
            //   HexDigits [> but only if MV of |HexDigits| > 0x10FFFF ]
            //
            // CodePoint ::
            //   HexDigits [> but only if MV of |HexDigits| ≤ 0x10FFFF ]
            if ch == '$' && self.peek() == Some('{') {
                self.chars.next();
                return Ok((
                    SourceLocation::new(start, self.offset()),
                    Some(builder.finish_without_push(&self)),
                    subst,
                ));
            }
            if ch == '`' {
                return Ok((
                    SourceLocation::new(start, self.offset()),
                    Some(builder.finish_without_push(&self)),
                    tail,
                ));
            }
            // TODO: Support escape sequences.
            if ch == '\\' {
                let text = builder.get_mut_string_without_current_ascii_char(&self);
                self.escape_sequence(text)?;
            } else {
                builder.push_matching(ch);
            }
        }
        Err(ParseError::UnterminatedString)
    }

    fn advance_impl<'parser>(
        &mut self,
        parser: &Parser<'parser>,
    ) -> Result<'alloc, (SourceLocation, Option<&'alloc str>, TerminalId)> {
        let mut builder = AutoCow::new(&self);
        let mut start = self.offset();
        while let Some(c) = self.chars.next() {
            match c {
                // 11.2 White Space
                //
                // WhiteSpace ::
                //     <TAB>
                //     <VT>
                //     <FF>
                //     <SP>
                //     <NBSP>
                //     <ZWNBSP>
                //     <USP>
                TAB |
                VT |
                FF |
                SP |
                NBSP |
                ZWNBSP |
                '\u{1680}' | // Ogham space mark (in <USP>)
                '\u{2000}' ..= '\u{200a}' | // typesetting spaces (in <USP>)
                '\u{202f}' | // Narrow no-break space (in <USP>)
                '\u{205f}' | // Medium mathematical space (in <USP>)
                '\u{3000}' // Ideographic space (in <USP>)
                    => {
                    // TODO - The spec uses <USP> to stand for any character
                    // with category "Space_Separator" (Zs). New Unicode
                    // standards may add characters to this set. This should therefore be
                    // implemented using the Unicode database somehow.
                    builder = AutoCow::new(&self);
                    start = self.offset();
                    continue;
                }

                // 11.3 Line Terminators
                //
                // LineTerminator ::
                //     <LF>
                //     <CR>
                //     <LS>
                //     <PS>
                LF | CR | LS | PS => {
                    self.is_on_new_line = true;
                    builder = AutoCow::new(&self);
                    start = self.offset();
                    continue;
                }

                '0' => {
                    match self.numeric_literal_starting_with_zero()? {
                        NumericType::Normal => {
                            return Ok((
                                SourceLocation::new(start, self.offset()),
                                Some(builder.finish(&self)),
                                TerminalId::NumericLiteral,
                            ));
                        }
                        NumericType::BigInt => {
                            return Ok((
                                SourceLocation::new(start, self.offset()),
                                Some(builder.finish_without_push(&self)),
                                TerminalId::BigIntLiteral,
                            ));
                        }
                    }
                }

                '1'..='9' => {
                    match self.decimal_literal_after_first_digit()? {
                        NumericType::Normal => {
                            return Ok((
                                SourceLocation::new(start, self.offset()),
                                Some(builder.finish(&self)),
                                TerminalId::NumericLiteral,
                            ));
                        }
                        NumericType::BigInt => {
                            return Ok((
                                SourceLocation::new(start, self.offset()),
                                Some(builder.finish_without_push(&self)),
                                TerminalId::BigIntLiteral,
                            ));
                        }
                    }
                }

                '"' | '\'' => {
                    return self.string_literal(c);
                }

                '`' => {
                    return self.template_part(start, TerminalId::TemplateHead, TerminalId::NoSubstitutionTemplate);
                }

                '!' => match self.peek() {
                    Some('=') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::StrictNotEqual));
                            }
                            _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LaxNotEqual)),
                        }
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LogicalNot)),
                },

                '%' => match self.peek() {
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::RemainderAssign));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Remainder)),
                },

                '&' => match self.peek() {
                    Some('&') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LogicalAnd));
                    }
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::BitwiseAndAssign));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::BitwiseAnd)),
                },

                '*' => match self.peek() {
                    Some('*') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::ExponentiateAssign));
                            }
                            _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Exponentiate)),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::MultiplyAssign));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Star)),
                },

                '+' => match self.peek() {
                    Some('+') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Increment));
                    }
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::AddAssign));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Plus)),
                },

                '-' => match self.peek() {
                    Some('-') => {
                        self.chars.next();
                        match self.peek() {
                            Some('>') if self.is_on_new_line => {
                                // B.1.3 SingleLineHTMLCloseComment
                                // TODO: Limit this to Script (not Module).
                                self.skip_single_line_comment(&mut builder);
                                continue;
                            }
                            _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Decrement)),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::SubtractAssign));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Minus)),
                },

                '.' => match self.peek() {
                    Some('.') => {
                        self.chars.next();
                        match self.peek() {
                            Some('.') => {
                                self.chars.next();
                                return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Ellipsis));
                            }
                            _ => return Err(ParseError::IllegalCharacter('.')),
                        }
                    }
                    Some('0'..='9') => {
                        self.decimal_digits()?;
                        self.optional_exponent()?;

                        // The SourceCharacter immediately following a
                        // NumericLiteral must not be an IdentifierStart or
                        // DecimalDigit. (11.8.3)
                        if let Some(ch) = self.peek() {
                            if is_identifier_start(ch) || ch.is_digit(10) {
                                return Err(ParseError::IllegalCharacter(ch));
                            }
                        }

                        return Ok((SourceLocation::new(start, self.offset()), Some(builder.finish(&self)), TerminalId::NumericLiteral));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Dot)),
                },

                '/' => match self.peek() {
                    Some('/') => {
                        // SingleLineComment :: `//` SingleLineCommentChars?
                        self.chars.next();
                        self.skip_single_line_comment(&mut builder);
                        start = self.offset();
                        continue;
                    }
                    Some('*') => {
                        self.chars.next();
                        self.skip_multi_line_comment(&mut builder)?;
                        start = self.offset();
                        continue;
                    }
                    _ => {
                        if parser.can_accept_terminal(TerminalId::Divide) {
                            match self.peek() {
                                Some('=') => {
                                    self.chars.next();
                                    return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::DivideAssign));
                                }
                                _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Divide)),
                            }
                        }
                        builder.push_matching('/');
                        return self.regular_expression_literal(&mut builder);
                    }
                },

                '}' => {
                    if parser.can_accept_terminal(TerminalId::TemplateMiddle) {
                        return self.template_part(start, TerminalId::TemplateMiddle, TerminalId::TemplateTail);
                    }
                    return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::CloseBrace));
                }

                '<' => match self.peek() {
                    Some('<') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LeftShiftAssign));
                            }
                            _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LeftShift)),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LessThanOrEqualTo));
                    }
                    Some('!') if self.is_looking_at("!--") => {
                        // B.1.3 SingleLineHTMLOpenComment. Note that the above
                        // `is_looking_at` test peeked ahead at the next three
                        // characters of input. This lookahead is necessary
                        // because `x<!--` has a comment but `x<!-y` does not.
                        //
                        // TODO: Limit this to Script (not Module).
                        self.skip_single_line_comment(&mut builder);
                        start = self.offset();
                        continue;
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LessThan)),
                },

                '=' => match self.peek() {
                    Some('=') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::StrictEqual));
                            }
                            _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LaxEqual)),
                        }
                    }
                    Some('>') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Arrow));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::EqualSign)),
                },

                '>' => match self.peek() {
                    Some('>') => {
                        self.chars.next();
                        match self.peek() {
                            Some('>') => {
                                self.chars.next();
                                match self.peek() {
                                    Some('=') => {
                                        self.chars.next();
                                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::UnsignedRightShiftAssign));
                                    }
                                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::UnsignedRightShift)),
                                }
                            }
                            Some('=') => {
                                self.chars.next();
                                return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::SignedRightShiftAssign));
                            }
                            _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::SignedRightShift)),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::GreaterThanOrEqualTo));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::GreaterThan)),
                },

                '^' => match self.peek() {
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::BitwiseXorAssign));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::BitwiseXor)),
                },

                '|' => match self.peek() {
                    Some('|') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::LogicalOr));
                    }
                    Some('=') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::BitwiseOrAssign));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::BitwiseOr)),
                },

                '?' => match self.peek() {
                    Some('?') => {
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Coalesce));
                    }
                    Some('.') => {
                        if let Some('0'..='9') = self.double_peek() {
                            return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::QuestionMark))
                        }
                        self.chars.next();
                        return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::OptionalChain));
                    }
                    _ => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::QuestionMark)),
                }

                '(' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::OpenParenthesis)),
                ')' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::CloseParenthesis)),
                ',' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Comma)),
                ':' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Colon)),
                ';' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::Semicolon)),
                '[' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::OpenBracket)),
                ']' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::CloseBracket)),
                '{' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::OpenBrace)),
                '~' => return Ok((SourceLocation::new(start, self.offset()), None, TerminalId::BitwiseNot)),

                // Idents
                '$' | '_' | 'a'..='z' | 'A'..='Z' => {
                    builder.push_matching(c);
                    return self.identifier_tail(start, builder);
                }

                '\\' => {
                    builder.force_allocation_without_current_ascii_char(&self);

                    let value = self.unicode_escape_sequence_after_backslash()?;
                    if !is_identifier_start(value) {
                        return Err(ParseError::IllegalCharacter(value));
                    }
                    builder.push_different(value);

                    return self.identifier_tail(start, builder);
                }

                '#' => {
                    if start == 0 {
                        // https://tc39.es/proposal-hashbang/out.html
                        // HashbangComment ::
                        //     `#!` SingleLineCommentChars?
                        if let Some('!') = self.peek() {
                            self.skip_single_line_comment(&mut builder);
                            start = self.offset();
                            continue;
                        }
                    }

                    builder.push_matching(c);
                    return self.private_identifier(start, builder);
                }

                other if is_identifier_start(other) => {
                    builder.push_matching(other);
                    return self.identifier_tail(start, builder);
                }

                other => {
                    return Err(ParseError::IllegalCharacter(other));
                }
            }
        }
        Ok((
            SourceLocation::new(start, self.offset()),
            None,
            TerminalId::End,
        ))
    }
}

struct AutoCow<'alloc> {
    start: &'alloc str,
    value: Option<String<'alloc>>,
}

impl<'alloc> AutoCow<'alloc> {
    fn new(lexer: &Lexer<'alloc>) -> Self {
        AutoCow {
            start: lexer.chars.as_str(),
            value: None,
        }
    }

    // Push a char that matches lexer.chars.next()
    fn push_matching(&mut self, c: char) {
        if let Some(text) = &mut self.value {
            text.push(c);
        }
    }

    // Push a different character than lexer.chars.next().
    // force_allocation_without_current_ascii_char must be called before this.
    fn push_different(&mut self, c: char) {
        debug_assert!(self.value.is_some());
        self.value.as_mut().unwrap().push(c)
    }

    // Force allocation of a String, excluding the current ASCII character,
    // and return the reference to it
    fn get_mut_string_without_current_ascii_char<'b>(
        &'b mut self,
        lexer: &'_ Lexer<'alloc>,
    ) -> &'b mut String<'alloc> {
        self.force_allocation_without_current_ascii_char(lexer);
        self.value.as_mut().unwrap()
    }

    // Force allocation of a String, excluding the current ASCII character.
    fn force_allocation_without_current_ascii_char(&mut self, lexer: &'_ Lexer<'alloc>) {
        if self.value.is_some() {
            return;
        }

        self.value = Some(String::from_str_in(
            &self.start[..self.start.len() - lexer.chars.as_str().len() - 1],
            lexer.allocator,
        ));
    }

    // Check if the string contains a different character, such as an escape
    // sequence
    fn has_different(&self) -> bool {
        self.value.is_some()
    }

    fn finish(&mut self, lexer: &Lexer<'alloc>) -> &'alloc str {
        match self.value.take() {
            Some(arena_string) => arena_string.into_bump_str(),
            None => &self.start[..self.start.len() - lexer.chars.as_str().len()],
        }
    }

    // Just like finish, but without pushing current char.
    fn finish_without_push(&mut self, lexer: &Lexer<'alloc>) -> &'alloc str {
        match self.value.take() {
            Some(arena_string) => arena_string.into_bump_str(),
            None => &self.start[..self.start.len() - lexer.chars.as_str().len() - 1],
        }
    }
}
