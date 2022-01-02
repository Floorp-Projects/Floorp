//! JavaScript lexer.

use crate::numeric_value::{parse_float, parse_int, NumericLiteralBase};
use crate::parser::Parser;
use crate::unicode::{is_id_continue, is_id_start};
use ast::arena;
use ast::source_atom_set::{CommonSourceAtomSetIndices, SourceAtomSet};
use ast::source_slice_list::SourceSliceList;
use ast::SourceLocation;
use bumpalo::{collections::String, Bump};
use generated_parser::{ParseError, Result, TerminalId, Token, TokenValue};
use std::cell::RefCell;
use std::convert::TryFrom;
use std::rc::Rc;
use std::str::Chars;

pub struct Lexer<'alloc> {
    allocator: &'alloc Bump,

    /// Next token to be returned.
    token: arena::Box<'alloc, Token>,

    /// Length of the input text, in UTF-8 bytes.
    source_length: usize,

    /// Iterator over the remaining not-yet-parsed input.
    chars: Chars<'alloc>,

    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,

    slices: Rc<RefCell<SourceSliceList<'alloc>>>,
}

enum NumericResult {
    Int {
        base: NumericLiteralBase,
    },
    Float,
    BigInt {
        #[allow(dead_code)]
        base: NumericLiteralBase,
    },
}

impl<'alloc> Lexer<'alloc> {
    pub fn new(
        allocator: &'alloc Bump,
        chars: Chars<'alloc>,
        atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
        slices: Rc<RefCell<SourceSliceList<'alloc>>>,
    ) -> Lexer<'alloc> {
        Self::with_offset(allocator, chars, 0, atoms, slices)
    }

    /// Create a lexer for a part of a JS script or module. `offset` is the
    /// total length of all previous parts, in bytes; source locations for
    /// tokens created by the new lexer start counting from this number.
    pub fn with_offset(
        allocator: &'alloc Bump,
        chars: Chars<'alloc>,
        offset: usize,
        atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
        slices: Rc<RefCell<SourceSliceList<'alloc>>>,
    ) -> Lexer<'alloc> {
        let source_length = offset + chars.as_str().len();
        let mut token = arena::alloc(allocator, new_token());
        token.is_on_new_line = true;
        Lexer {
            allocator,
            token,
            source_length,
            chars,
            atoms,
            slices,
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

    fn set_result(
        &mut self,
        terminal_id: TerminalId,
        loc: SourceLocation,
        value: TokenValue,
    ) -> Result<'alloc, ()> {
        self.token.terminal_id = terminal_id;
        self.token.loc = loc;
        self.token.value = value;
        Ok(())
    }

    #[inline]
    pub fn next<'parser>(
        &mut self,
        parser: &Parser<'parser>,
    ) -> Result<'alloc, arena::Box<'alloc, Token>> {
        let mut next_token = arena::alloc_with(self.allocator, || new_token());
        self.advance_impl(parser)?;
        std::mem::swap(&mut self.token, &mut next_token);
        Ok(next_token)
    }

    fn unexpected_err(&mut self) -> ParseError<'alloc> {
        if let Some(ch) = self.peek() {
            ParseError::IllegalCharacter(ch)
        } else {
            ParseError::UnexpectedEnd
        }
    }
}

/// Returns an empty token which is meant as a place holder to be mutated later.
fn new_token() -> Token {
    Token::basic_token(TerminalId::End, SourceLocation::default())
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
                    self.token.is_on_new_line = true;
                }
                _ => {}
            }
        }
        Err(ParseError::UnterminatedMultiLineComment.into())
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
        self.token.is_on_new_line = true;
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
                        return Err(ParseError::InvalidEscapeSequence.into());
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
                return Err(ParseError::UnexpectedEnd.into());
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
                            return Err(ParseError::IllegalCharacter(value).into());
                        }
                        builder.push_different(value);
                    }

                    other if is_identifier_start(other) => {
                        builder.push_matching(other);
                    }

                    other => {
                        return Err(ParseError::IllegalCharacter(other).into());
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
    fn identifier_tail(&mut self, start: usize, builder: AutoCow<'alloc>) -> Result<'alloc, ()> {
        let (has_different, text) = self.identifier_name_tail(builder)?;

        // https://tc39.es/ecma262/#sec-keywords-and-reserved-words
        //
        // keywords in the grammar match literal sequences of specific
        // SourceCharacter elements. A code point in a keyword cannot be
        // expressed by a `\` UnicodeEscapeSequence.
        let (id, value) = if has_different {
            // Always return `NameWithEscape`.
            //
            // Error check against reserved word should be handled in the
            // consumer.
            (TerminalId::NameWithEscape, self.string_to_token_value(text))
        } else {
            match &text as &str {
                "as" => (
                    TerminalId::As,
                    TokenValue::Atom(CommonSourceAtomSetIndices::as_()),
                ),
                "async" => {
                    /*
                    (
                        TerminalId::Async,
                        TokenValue::Atom(CommonSourceAtomSetIndices::async_()),
                    ),
                    */
                    return Err(ParseError::NotImplemented(
                        "async cannot be handled in parser due to multiple lookahead",
                    )
                    .into());
                }
                "await" => {
                    /*
                    (
                        TerminalId::Await,
                        TokenValue::Atom(CommonSourceAtomSetIndices::await_()),
                    ),
                     */
                    return Err(
                        ParseError::NotImplemented("await cannot be handled in parser").into(),
                    );
                }
                "break" => (
                    TerminalId::Break,
                    TokenValue::Atom(CommonSourceAtomSetIndices::break_()),
                ),
                "case" => (
                    TerminalId::Case,
                    TokenValue::Atom(CommonSourceAtomSetIndices::case()),
                ),
                "catch" => (
                    TerminalId::Catch,
                    TokenValue::Atom(CommonSourceAtomSetIndices::catch()),
                ),
                "class" => (
                    TerminalId::Class,
                    TokenValue::Atom(CommonSourceAtomSetIndices::class()),
                ),
                "const" => (
                    TerminalId::Const,
                    TokenValue::Atom(CommonSourceAtomSetIndices::const_()),
                ),
                "continue" => (
                    TerminalId::Continue,
                    TokenValue::Atom(CommonSourceAtomSetIndices::continue_()),
                ),
                "debugger" => (
                    TerminalId::Debugger,
                    TokenValue::Atom(CommonSourceAtomSetIndices::debugger()),
                ),
                "default" => (
                    TerminalId::Default,
                    TokenValue::Atom(CommonSourceAtomSetIndices::default()),
                ),
                "delete" => (
                    TerminalId::Delete,
                    TokenValue::Atom(CommonSourceAtomSetIndices::delete()),
                ),
                "do" => (
                    TerminalId::Do,
                    TokenValue::Atom(CommonSourceAtomSetIndices::do_()),
                ),
                "else" => (
                    TerminalId::Else,
                    TokenValue::Atom(CommonSourceAtomSetIndices::else_()),
                ),
                "enum" => (
                    TerminalId::Enum,
                    TokenValue::Atom(CommonSourceAtomSetIndices::enum_()),
                ),
                "export" => (
                    TerminalId::Export,
                    TokenValue::Atom(CommonSourceAtomSetIndices::export()),
                ),
                "extends" => (
                    TerminalId::Extends,
                    TokenValue::Atom(CommonSourceAtomSetIndices::extends()),
                ),
                "finally" => (
                    TerminalId::Finally,
                    TokenValue::Atom(CommonSourceAtomSetIndices::finally()),
                ),
                "for" => (
                    TerminalId::For,
                    TokenValue::Atom(CommonSourceAtomSetIndices::for_()),
                ),
                "from" => (
                    TerminalId::From,
                    TokenValue::Atom(CommonSourceAtomSetIndices::from()),
                ),
                "function" => (
                    TerminalId::Function,
                    TokenValue::Atom(CommonSourceAtomSetIndices::function()),
                ),
                "get" => (
                    TerminalId::Get,
                    TokenValue::Atom(CommonSourceAtomSetIndices::get()),
                ),
                "if" => (
                    TerminalId::If,
                    TokenValue::Atom(CommonSourceAtomSetIndices::if_()),
                ),
                "implements" => (
                    TerminalId::Implements,
                    TokenValue::Atom(CommonSourceAtomSetIndices::implements()),
                ),
                "import" => (
                    TerminalId::Import,
                    TokenValue::Atom(CommonSourceAtomSetIndices::import()),
                ),
                "in" => (
                    TerminalId::In,
                    TokenValue::Atom(CommonSourceAtomSetIndices::in_()),
                ),
                "instanceof" => (
                    TerminalId::Instanceof,
                    TokenValue::Atom(CommonSourceAtomSetIndices::instanceof()),
                ),
                "interface" => (
                    TerminalId::Interface,
                    TokenValue::Atom(CommonSourceAtomSetIndices::interface()),
                ),
                "let" => {
                    /*
                    (
                        TerminalId::Let,
                        TokenValue::Atom(CommonSourceAtomSetIndices::let_()),
                    ),
                    */
                    return Err(ParseError::NotImplemented(
                        "let cannot be handled in parser due to multiple lookahead",
                    )
                    .into());
                }
                "new" => (
                    TerminalId::New,
                    TokenValue::Atom(CommonSourceAtomSetIndices::new_()),
                ),
                "of" => (
                    TerminalId::Of,
                    TokenValue::Atom(CommonSourceAtomSetIndices::of()),
                ),
                "package" => (
                    TerminalId::Package,
                    TokenValue::Atom(CommonSourceAtomSetIndices::package()),
                ),
                "private" => (
                    TerminalId::Private,
                    TokenValue::Atom(CommonSourceAtomSetIndices::private()),
                ),
                "protected" => (
                    TerminalId::Protected,
                    TokenValue::Atom(CommonSourceAtomSetIndices::protected()),
                ),
                "public" => (
                    TerminalId::Public,
                    TokenValue::Atom(CommonSourceAtomSetIndices::public()),
                ),
                "return" => (
                    TerminalId::Return,
                    TokenValue::Atom(CommonSourceAtomSetIndices::return_()),
                ),
                "set" => (
                    TerminalId::Set,
                    TokenValue::Atom(CommonSourceAtomSetIndices::set()),
                ),
                "static" => (
                    TerminalId::Static,
                    TokenValue::Atom(CommonSourceAtomSetIndices::static_()),
                ),
                "super" => (
                    TerminalId::Super,
                    TokenValue::Atom(CommonSourceAtomSetIndices::super_()),
                ),
                "switch" => (
                    TerminalId::Switch,
                    TokenValue::Atom(CommonSourceAtomSetIndices::switch()),
                ),
                "target" => (
                    TerminalId::Target,
                    TokenValue::Atom(CommonSourceAtomSetIndices::target()),
                ),
                "this" => (
                    TerminalId::This,
                    TokenValue::Atom(CommonSourceAtomSetIndices::this()),
                ),
                "throw" => (
                    TerminalId::Throw,
                    TokenValue::Atom(CommonSourceAtomSetIndices::throw()),
                ),
                "try" => (
                    TerminalId::Try,
                    TokenValue::Atom(CommonSourceAtomSetIndices::try_()),
                ),
                "typeof" => (
                    TerminalId::Typeof,
                    TokenValue::Atom(CommonSourceAtomSetIndices::typeof_()),
                ),
                "var" => (
                    TerminalId::Var,
                    TokenValue::Atom(CommonSourceAtomSetIndices::var()),
                ),
                "void" => (
                    TerminalId::Void,
                    TokenValue::Atom(CommonSourceAtomSetIndices::void()),
                ),
                "while" => (
                    TerminalId::While,
                    TokenValue::Atom(CommonSourceAtomSetIndices::while_()),
                ),
                "with" => (
                    TerminalId::With,
                    TokenValue::Atom(CommonSourceAtomSetIndices::with()),
                ),
                "yield" => {
                    /*
                    (
                        TerminalId::Yield,
                        TokenValue::Atom(CommonSourceAtomSetIndices::yield_()),
                    ),
                     */
                    return Err(
                        ParseError::NotImplemented("yield cannot be handled in parser").into(),
                    );
                }
                "null" => (
                    TerminalId::NullLiteral,
                    TokenValue::Atom(CommonSourceAtomSetIndices::null()),
                ),
                "true" => (
                    TerminalId::BooleanLiteral,
                    TokenValue::Atom(CommonSourceAtomSetIndices::true_()),
                ),
                "false" => (
                    TerminalId::BooleanLiteral,
                    TokenValue::Atom(CommonSourceAtomSetIndices::false_()),
                ),
                _ => (TerminalId::Name, self.string_to_token_value(text)),
            }
        };

        self.set_result(id, SourceLocation::new(start, self.offset()), value)
    }

    /// ```text
    /// PrivateIdentifier::
    ///     `#` IdentifierName
    /// ```
    fn private_identifier(&mut self, start: usize, builder: AutoCow<'alloc>) -> Result<'alloc, ()> {
        let name = self.identifier_name(builder)?;
        let value = self.string_to_token_value(name);
        self.set_result(
            TerminalId::PrivateIdentifier,
            SourceLocation::new(start, self.offset()),
            value,
        )
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
                return Err(ParseError::InvalidEscapeSequence.into());
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
                        return Err(ParseError::InvalidEscapeSequence.into());
                    }
                }
                value
            }
            _ => self.hex_4_digits()?,
        };

        Ok(value)
    }
}

impl<'alloc> Lexer<'alloc> {
    // ------------------------------------------------------------------------
    // 11.8.3 Numeric Literals

    /// Advance over decimal digits in the input.
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
    fn decimal_digits(&mut self) -> Result<'alloc, ()> {
        if let Some('0'..='9') = self.peek() {
            self.chars.next();
        } else {
            return Err(self.unexpected_err().into());
        }

        self.decimal_digits_after_first_digit()?;
        Ok(())
    }

    fn optional_decimal_digits(&mut self) -> Result<'alloc, ()> {
        if let Some('0'..='9') = self.peek() {
            self.chars.next();
        } else {
            return Ok(());
        }

        self.decimal_digits_after_first_digit()?;
        Ok(())
    }

    fn decimal_digits_after_first_digit(&mut self) -> Result<'alloc, ()> {
        while let Some(next) = self.peek() {
            match next {
                '_' => {
                    self.chars.next();

                    if let Some('0'..='9') = self.peek() {
                        self.chars.next();
                    } else {
                        return Err(self.unexpected_err().into());
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
    fn optional_exponent(&mut self) -> Result<'alloc, bool> {
        if let Some('e') | Some('E') = self.peek() {
            self.chars.next();
            self.decimal_exponent()?;
            return Ok(true);
        }

        Ok(false)
    }

    fn decimal_exponent(&mut self) -> Result<'alloc, ()> {
        if let Some('+') | Some('-') = self.peek() {
            self.chars.next();
        }

        self.decimal_digits()?;

        Ok(())
    }

    /// ```text
    /// HexDigit :: one of
    ///     `0` `1` `2` `3` `4` `5` `6` `7` `8` `9` `a` `b` `c` `d` `e` `f` `A` `B` `C` `D` `E` `F`
    /// ```
    fn hex_digit(&mut self) -> Result<'alloc, u32> {
        match self.chars.next() {
            None => Err(ParseError::InvalidEscapeSequence.into()),
            Some(c @ '0'..='9') => Ok(c as u32 - '0' as u32),
            Some(c @ 'a'..='f') => Ok(10 + (c as u32 - 'a' as u32)),
            Some(c @ 'A'..='F') => Ok(10 + (c as u32 - 'A' as u32)),
            Some(other) => Err(ParseError::IllegalCharacter(other).into()),
        }
    }

    fn code_point_to_char(value: u32) -> Result<'alloc, char> {
        if 0xd800 <= value && value <= 0xdfff {
            Err(ParseError::NotImplemented("unicode escape sequences (surrogates)").into())
        } else {
            char::try_from(value).map_err(|_| ParseError::InvalidEscapeSequence.into())
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
                    return Err(ParseError::InvalidEscapeSequence.into());
                }
                Some(c @ '0'..='9') => c as u32 - '0' as u32,
                Some(c @ 'a'..='f') => 10 + (c as u32 - 'a' as u32),
                Some(c @ 'A'..='F') => 10 + (c as u32 - 'A' as u32),
                Some(_) => break,
            };
            self.chars.next();
            value = (value << 4) | next;
            if value > 0x10FFFF {
                return Err(ParseError::InvalidEscapeSequence.into());
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
    fn numeric_literal_starting_with_zero(&mut self) -> Result<'alloc, NumericResult> {
        let mut base = NumericLiteralBase::Decimal;
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

                base = NumericLiteralBase::Binary;

                if let Some('0'..='1') = self.peek() {
                    self.chars.next();
                } else {
                    return Err(self.unexpected_err().into());
                }

                while let Some(next) = self.peek() {
                    match next {
                        '_' => {
                            self.chars.next();

                            if let Some('0'..='1') = self.peek() {
                                self.chars.next();
                            } else {
                                return Err(self.unexpected_err().into());
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
                    return Ok(NumericResult::BigInt { base });
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

                base = NumericLiteralBase::Octal;

                if let Some('0'..='7') = self.peek() {
                    self.chars.next();
                } else {
                    return Err(self.unexpected_err().into());
                }

                while let Some(next) = self.peek() {
                    match next {
                        '_' => {
                            self.chars.next();

                            if let Some('0'..='7') = self.peek() {
                                self.chars.next();
                            } else {
                                return Err(self.unexpected_err().into());
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
                    return Ok(NumericResult::BigInt { base });
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

                base = NumericLiteralBase::Hex;

                if let Some('0'..='9') | Some('a'..='f') | Some('A'..='F') = self.peek() {
                    self.chars.next();
                } else {
                    return Err(self.unexpected_err().into());
                }

                while let Some(next) = self.peek() {
                    match next {
                        '_' => {
                            self.chars.next();

                            if let Some('0'..='9') | Some('a'..='f') | Some('A'..='F') = self.peek()
                            {
                                self.chars.next();
                            } else {
                                return Err(self.unexpected_err().into());
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
                    return Ok(NumericResult::BigInt { base });
                }
            }

            Some('.') => {
                self.chars.next();
                return self.decimal_literal_after_decimal_point_after_digits();
            }

            Some('e') | Some('E') => {
                self.chars.next();
                self.decimal_exponent()?;
                return Ok(NumericResult::Float);
            }

            Some('n') => {
                self.chars.next();
                self.check_after_numeric_literal()?;
                return Ok(NumericResult::BigInt { base });
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
                return Err(ParseError::NotImplemented("LegacyOctalIntegerLiteral").into());
            }

            _ => {}
        }

        self.check_after_numeric_literal()?;
        Ok(NumericResult::Int { base })
    }

    /// Scan a NumericLiteral (defined in 11.8.3, extended by B.1.1) after
    /// having already consumed the first character, which is a decimal digit.
    fn decimal_literal_after_first_digit(&mut self) -> Result<'alloc, NumericResult> {
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

        self.decimal_digits_after_first_digit()?;
        match self.peek() {
            Some('.') => {
                self.chars.next();
                return self.decimal_literal_after_decimal_point_after_digits();
            }
            Some('n') => {
                self.chars.next();
                self.check_after_numeric_literal()?;
                return Ok(NumericResult::BigInt {
                    base: NumericLiteralBase::Decimal,
                });
            }
            _ => {}
        }

        let has_exponent = self.optional_exponent()?;
        self.check_after_numeric_literal()?;

        let result = if has_exponent {
            NumericResult::Float
        } else {
            NumericResult::Int {
                base: NumericLiteralBase::Decimal,
            }
        };

        Ok(result)
    }

    fn decimal_literal_after_decimal_point(&mut self) -> Result<'alloc, NumericResult> {
        // The parts after `.` in
        //
        //     `.` DecimalDigits ExponentPart?
        self.decimal_digits()?;
        self.optional_exponent()?;
        self.check_after_numeric_literal()?;

        Ok(NumericResult::Float)
    }

    fn decimal_literal_after_decimal_point_after_digits(
        &mut self,
    ) -> Result<'alloc, NumericResult> {
        // The parts after `.` in
        //
        // DecimalLiteral ::
        //     DecimalIntegerLiteral `.` DecimalDigits? ExponentPart?
        self.optional_decimal_digits()?;
        self.optional_exponent()?;
        self.check_after_numeric_literal()?;

        Ok(NumericResult::Float)
    }

    fn check_after_numeric_literal(&self) -> Result<'alloc, ()> {
        // The SourceCharacter immediately following a
        // NumericLiteral must not be an IdentifierStart or
        // DecimalDigit. (11.8.3)
        if let Some(ch) = self.peek() {
            if is_identifier_start(ch) || ch.is_digit(10) {
                return Err(ParseError::IllegalCharacter(ch).into());
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
                return Err(ParseError::UnterminatedString.into());
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
                            return Err(ParseError::InvalidEscapeSequence.into());
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
                            )
                            .into());
                        }
                        Some('8'..='9') => {
                            return Err(ParseError::NotImplemented(
                                "digit immediately following \\0 escape sequence",
                            )
                            .into());
                        }
                        _ => {}
                    }
                    text.push('\0');
                }

                '1'..='7' => {
                    return Err(ParseError::NotImplemented(
                        "legacy octal escape sequence in string",
                    )
                    .into());
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
    fn string_literal(&mut self, delimiter: char) -> Result<'alloc, ()> {
        let offset = self.offset() - 1;
        let mut builder = AutoCow::new(&self);
        loop {
            match self.chars.next() {
                None | Some('\r') | Some('\n') => {
                    return Err(ParseError::UnterminatedString.into());
                }

                Some(c @ '"') | Some(c @ '\'') => {
                    if c == delimiter {
                        let value = self.string_to_token_value(builder.finish_without_push(&self));
                        return self.set_result(
                            TerminalId::StringLiteral,
                            SourceLocation::new(offset, self.offset()),
                            value,
                        );
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

    fn regular_expression_backslash_sequence(&mut self) -> Result<'alloc, ()> {
        match self.chars.next() {
            None | Some(CR) | Some(LF) | Some(LS) | Some(PS) => {
                Err(ParseError::UnterminatedRegExp.into())
            }
            Some(_) => Ok(()),
        }
    }

    // See 12.2.8 and 11.8.5 sections.
    fn regular_expression_literal(&mut self, builder: &mut AutoCow<'alloc>) -> Result<'alloc, ()> {
        let offset = self.offset();

        loop {
            match self.chars.next() {
                None | Some(CR) | Some(LF) | Some(LS) | Some(PS) => {
                    return Err(ParseError::UnterminatedRegExp.into());
                }
                Some('/') => {
                    break;
                }
                Some('[') => {
                    // RegularExpressionClass.
                    loop {
                        match self.chars.next() {
                            None | Some(CR) | Some(LF) | Some(LS) | Some(PS) => {
                                return Err(ParseError::UnterminatedRegExp.into());
                            }
                            Some(']') => {
                                break;
                            }
                            Some('\\') => {
                                self.regular_expression_backslash_sequence()?;
                            }
                            Some(_) => {}
                        }
                    }
                }
                Some('\\') => {
                    self.regular_expression_backslash_sequence()?;
                }
                Some(_) => {}
            }
        }
        let mut flag_text = AutoCow::new(&self);
        while let Some(ch) = self.peek() {
            match ch {
                '$' | '_' | 'a'..='z' | 'A'..='Z' | '0'..='9' => {
                    self.chars.next();
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
                )
                .into());
            }
            let ch_mask = 1 << ((ch as u8) - ('a' as u8));
            if ch_mask & gimsuy_mask == 0 {
                return Err(ParseError::NotImplemented(
                    "Unexpected flag in regular expression literal",
                )
                .into());
            }
            if flag_text_set & ch_mask != 0 {
                return Err(ParseError::NotImplemented(
                    "Flag is mentioned twice in regular expression literal",
                )
                .into());
            }
            flag_text_set |= ch_mask;
        }

        // TODO: 12.2.8.2.4 and 12.2.8.2.5 Check that the body matches the
        // grammar defined in 21.2.1.

        let value = self.slice_to_token_value(literal);
        self.set_result(
            TerminalId::RegularExpressionLiteral,
            SourceLocation::new(offset, self.offset()),
            value,
        )
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
    ) -> Result<'alloc, ()> {
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
                let value = self.string_to_token_value(builder.finish_without_push(&self));
                return self.set_result(subst, SourceLocation::new(start, self.offset()), value);
            }
            if ch == '`' {
                let value = self.string_to_token_value(builder.finish_without_push(&self));
                return self.set_result(tail, SourceLocation::new(start, self.offset()), value);
            }
            // TODO: Support escape sequences.
            if ch == '\\' {
                let text = builder.get_mut_string_without_current_ascii_char(&self);
                self.escape_sequence(text)?;
            } else {
                builder.push_matching(ch);
            }
        }
        Err(ParseError::UnterminatedString.into())
    }

    fn advance_impl<'parser>(&mut self, parser: &Parser<'parser>) -> Result<'alloc, ()> {
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
                    self.token.is_on_new_line = true;
                    builder = AutoCow::new(&self);
                    start = self.offset();
                    continue;
                }

                '0' => {
                    let result = self.numeric_literal_starting_with_zero()?;
                    return Ok(self.numeric_result_to_advance_result(builder.finish(&self), start, result)?);
                }

                '1'..='9' => {
                    let result = self.decimal_literal_after_first_digit()?;
                    return Ok(self.numeric_result_to_advance_result(builder.finish(&self), start, result)?);
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
                                return self.set_result(
                                    TerminalId::StrictNotEqual,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::LaxNotEqual,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            ),
                        }
                    }
                    _ => return self.set_result(
                        TerminalId::LogicalNot,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '%' => match self.peek() {
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::RemainderAssign,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::Remainder,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '&' => match self.peek() {
                    Some('&') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::LogicalAndAssign,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::LogicalAnd,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            )
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::BitwiseAndAssign,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::BitwiseAnd,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '*' => match self.peek() {
                    Some('*') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::ExponentiateAssign,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::Exponentiate,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            ),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::MultiplyAssign,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::Star,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '+' => match self.peek() {
                    Some('+') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::Increment,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::AddAssign,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::Plus,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '-' => match self.peek() {
                    Some('-') => {
                        self.chars.next();
                        match self.peek() {
                            Some('>') if self.token.is_on_new_line => {
                                // B.1.3 SingleLineHTMLCloseComment
                                // TODO: Limit this to Script (not Module).
                                self.skip_single_line_comment(&mut builder);
                                continue;
                            }
                            _ => return self.set_result(
                                TerminalId::Decrement,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            ),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::SubtractAssign,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::Minus,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '.' => match self.peek() {
                    Some('.') => {
                        self.chars.next();
                        match self.peek() {
                            Some('.') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::Ellipsis,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return Err(ParseError::IllegalCharacter('.').into()),
                        }
                    }
                    Some('0'..='9') => {
                        let result = self.decimal_literal_after_decimal_point()?;
                        return Ok(self.numeric_result_to_advance_result(builder.finish(&self), start, result)?);
                    }
                    _ => return self.set_result(
                        TerminalId::Dot,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
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
                                    return self.set_result(
                                        TerminalId::DivideAssign,
                                        SourceLocation::new(start, self.offset()),
                                        TokenValue::None,
                                    );
                                }
                                _ => return self.set_result(
                                    TerminalId::Divide,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                ),
                            }
                        }
                        return self.regular_expression_literal(&mut builder);
                    }
                },

                '}' => {
                    if parser.can_accept_terminal(TerminalId::TemplateMiddle) {
                        return self.template_part(start, TerminalId::TemplateMiddle, TerminalId::TemplateTail);
                    }
                    return self.set_result(
                        TerminalId::CloseBrace,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    );
                }

                '<' => match self.peek() {
                    Some('<') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::LeftShiftAssign,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::LeftShift,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            ),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::LessThanOrEqualTo,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
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
                    _ => return self.set_result(
                        TerminalId::LessThan,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '=' => match self.peek() {
                    Some('=') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::StrictEqual,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::LaxEqual,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            ),
                        }
                    }
                    Some('>') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::Arrow,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::EqualSign,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
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
                                        return self.set_result(
                                            TerminalId::UnsignedRightShiftAssign,
                                            SourceLocation::new(start, self.offset()),
                                            TokenValue::None,
                                        );
                                    }
                                    _ => return self.set_result(
                                        TerminalId::UnsignedRightShift,
                                        SourceLocation::new(start, self.offset()),
                                        TokenValue::None,
                                    ),
                                }
                            }
                            Some('=') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::SignedRightShiftAssign,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::SignedRightShift,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            ),
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::GreaterThanOrEqualTo,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::GreaterThan,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '^' => match self.peek() {
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::BitwiseXorAssign,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::BitwiseXor,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '|' => match self.peek() {
                    Some('|') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::LogicalOrAssign,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::LogicalOr,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            )
                        }
                    }
                    Some('=') => {
                        self.chars.next();
                        return self.set_result(
                            TerminalId::BitwiseOrAssign,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::BitwiseOr,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                },

                '?' => match self.peek() {
                    Some('?') => {
                        self.chars.next();
                        match self.peek() {
                            Some('=') => {
                                self.chars.next();
                                return self.set_result(
                                    TerminalId::CoalesceAssign,
                                    SourceLocation::new(start, self.offset()),
                                    TokenValue::None,
                                );
                            }
                            _ => return self.set_result(
                                TerminalId::Coalesce,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            )
                        }
                    }
                    Some('.') => {
                        if let Some('0'..='9') = self.double_peek() {
                            return self.set_result(
                                TerminalId::QuestionMark,
                                SourceLocation::new(start, self.offset()),
                                TokenValue::None,
                            )
                        }
                        self.chars.next();
                        return self.set_result(
                            TerminalId::OptionalChain,
                            SourceLocation::new(start, self.offset()),
                            TokenValue::None,
                        );
                    }
                    _ => return self.set_result(
                        TerminalId::QuestionMark,
                        SourceLocation::new(start, self.offset()),
                        TokenValue::None,
                    ),
                }

                '(' => return self.set_result(
                    TerminalId::OpenParenthesis,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                ')' => return self.set_result(
                    TerminalId::CloseParenthesis,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                ',' => return self.set_result(
                    TerminalId::Comma,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                ':' => return self.set_result(
                    TerminalId::Colon,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                ';' => return self.set_result(
                    TerminalId::Semicolon,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                '[' => return self.set_result(
                    TerminalId::OpenBracket,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                ']' => return self.set_result(
                    TerminalId::CloseBracket,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                '{' => return self.set_result(
                    TerminalId::OpenBrace,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),
                '~' => return self.set_result(
                    TerminalId::BitwiseNot,
                    SourceLocation::new(start, self.offset()),
                    TokenValue::None,
                ),

                // Idents
                '$' | '_' | 'a'..='z' | 'A'..='Z' => {
                    builder.push_matching(c);
                    return self.identifier_tail(start, builder);
                }

                '\\' => {
                    builder.force_allocation_without_current_ascii_char(&self);

                    let value = self.unicode_escape_sequence_after_backslash()?;
                    if !is_identifier_start(value) {
                        return Err(ParseError::IllegalCharacter(value).into());
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
                    return Err(ParseError::IllegalCharacter(other).into());
                }
            }
        }
        self.set_result(
            TerminalId::End,
            SourceLocation::new(start, self.offset()),
            TokenValue::None,
        )
    }

    fn string_to_token_value(&mut self, s: &'alloc str) -> TokenValue {
        let index = self.atoms.borrow_mut().insert(s);
        TokenValue::Atom(index)
    }

    fn slice_to_token_value(&mut self, s: &'alloc str) -> TokenValue {
        let index = self.slices.borrow_mut().push(s);
        TokenValue::Slice(index)
    }

    fn numeric_result_to_advance_result(
        &mut self,
        s: &'alloc str,
        start: usize,
        result: NumericResult,
    ) -> Result<'alloc, ()> {
        let (terminal_id, value) = match result {
            NumericResult::Int { base } => {
                let n = parse_int(s, base).map_err(|s| ParseError::NotImplemented(s))?;
                (TerminalId::NumericLiteral, TokenValue::Number(n))
            }
            NumericResult::Float => {
                let n = parse_float(s).map_err(|s| ParseError::NotImplemented(s))?;
                (TerminalId::NumericLiteral, TokenValue::Number(n))
            }
            NumericResult::BigInt { .. } => {
                // FIXME
                (TerminalId::BigIntLiteral, self.string_to_token_value(s))
            }
        };

        self.set_result(
            terminal_id,
            SourceLocation::new(start, self.offset()),
            value,
        )
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
