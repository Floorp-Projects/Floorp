//! A WebIDL lexer.

mod token;

use std::error::Error;
use std::fmt;
use std::i64;
use std::iter::Peekable;
use std::str::CharIndices;

pub use self::token::Token;

/// The type that serves as an `Item` for the lexer iterator. The `Ok` portion of the `Result`
/// refers to the starting and ending locations of the token as well as the token itself. The
/// `Err` portion refers to some generic error.
type Spanned<Token, Location, Error> = Result<(Location, Token, Location), Error>;

/// An enum of possible errors that can occur during lexing.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum LexicalErrorCode {
    /// Occurs when a block comment is not closed (e.g. `/* this is a comment`). Notably, this can
    /// only occur when the end of the file is reached as everything else will always be considered
    /// to be a part of the comment.
    ExpectedCommentBlockEnd,

    /// Occurs in the specific case of lexing a float literal of the form `-.` with no following
    /// decimal digits.
    ExpectedDecimalDigit,

    /// Occurs when `..` is lexed with no following `.`.
    ExpectedEllipsis,

    /// Occurs when lexing a float literal that does not provide an exponent after the `e` (e.g.
    /// (`582.13e`).
    ExpectedFloatExponent,

    /// Occurs when lexing a hexadecimal literal that does not provide hexadecimal digits after the
    /// `0x`.
    ExpectedHexadecimalDigit,

    /// Occurs when any leading substring of `Infinity` follows `-` but does not complete the
    /// keyword.
    ExpectedKeywordInfinity,

    /// Occurs when a string literal is not closed (e.g. `"this is a string`). Notably, this can
    /// only occur when the end of the file is reached as everything else will always be considered
    /// to be a part of the string.
    ExpectedStringLiteralEnd,
}

/// The error that is returned when an error occurs during lexing.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct LexicalError {
    /// The code that is used to distinguish different types of errors.
    pub code: LexicalErrorCode,

    /// The location offset from the beginning of the input string given to the lexer.
    pub location: usize,
}

impl fmt::Display for LexicalError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.description())
    }
}

impl Error for LexicalError {
    fn description(&self) -> &str {
        match self.code {
            LexicalErrorCode::ExpectedCommentBlockEnd => "expected end of comment block",
            LexicalErrorCode::ExpectedDecimalDigit => "expected decimal digit",
            LexicalErrorCode::ExpectedEllipsis => "expected ellipsis",
            LexicalErrorCode::ExpectedFloatExponent => "expected float exponent",
            LexicalErrorCode::ExpectedHexadecimalDigit => "expected hexadecimal digit",
            LexicalErrorCode::ExpectedKeywordInfinity => "expected \"Infinity\" keyword",
            LexicalErrorCode::ExpectedStringLiteralEnd => "expected end of string literal",
        }
    }
}

/// States representing a portion of the DFA used to represent the state machine for lexing float
/// literals.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
enum FloatLexState {
    /// At this state, it is assumed that a decimal point and at least one digit has occurred in the
    /// float literal. The exponent base is assumed to not have occurred yet.
    AfterDecimalPoint,

    /// At this state, it is assumed that the exponent base and at least one digit has occured in
    /// the float literal. A decimal point may or may not have already preceded.
    AfterExponentBase,

    /// At this state, it is assumed that neither a decimal point nor the exponent base have
    /// occurred. Digit(s) may or may not have already preceded.
    BeforeDecimalPoint,

    /// At this state, it is assumed that a decimal point has immediately preceded current location
    /// of lexing. Digit(s) may or may not have already preceded the decimal point.
    ImmediatelyAfterDecimalPoint,

    /// At this state, it is assumed that the exponent base has immediately preceded the current
    /// location of lexing. Digit(s) and a decimal point may or may not have already preceded the
    /// exponent base.
    ImmediatelyAfterExponentBase,

    /// At this state, it is assumed that the exponent base and either a `+` or `-` have immediately
    /// preceded the current location of lexing. Digit(s) and a decimal point may or may not have
    /// already preceded the exponent base.
    ImmediatelyAfterExponentSign,
}

/// A helper function for creating a lexical error.
fn create_error(code: LexicalErrorCode, location: usize) -> LexicalError {
    LexicalError { code, location }
}

/// The lexer that is used to perform lexical analysis on the WebIDL grammar. The lexer implements
/// the `Iterator` trait, so in order to retrieve the tokens, you simply have to iterate over it.
///
///
/// # Example
///
/// The following example shows how a string can be lexed into a `Vec<(usize, Token, usize)>`. Note
/// that since `\` is used to do multilines in this example, the locations may vary if you lex
/// directly from a string taken from a file.
///
/// ```
/// use webidl::*;
///
/// let lexer = Lexer::new("/* Example taken from emscripten site */\n\
///                         enum EnumClass_EnumWithinClass {\n\
///                             \"EnumClass::e_val\"\n\
///                         };\n\
///                         // This is a comment.\n\
///                         interface EnumClass {\n\
///                             void EnumClass();\n\
///
///                             EnumClass_EnumWithinClass GetEnum();\n\
///
///                             EnumNamespace_EnumInNamespace GetEnumFromNameSpace();\n\
///                         };");
/// assert_eq!(lexer.collect::<Vec<_>>(),
///            vec![Ok((41, Token::Enum, 45)),
///                 Ok((46, Token::Identifier("EnumClass_EnumWithinClass".to_string()), 71)),
///                 Ok((72, Token::LeftBrace, 73)),
///                 Ok((74, Token::StringLiteral("EnumClass::e_val".to_string()), 92)),
///                 Ok((93, Token::RightBrace, 94)),
///                 Ok((94, Token::Semicolon, 95)),
///                 Ok((118, Token::Interface, 127)),
///                 Ok((128, Token::Identifier("EnumClass".to_string()), 137)),
///                 Ok((138, Token::LeftBrace, 139)),
///                 Ok((140, Token::Void, 144)),
///                 Ok((145, Token::Identifier("EnumClass".to_string()), 154)),
///                 Ok((154, Token::LeftParenthesis, 155)),
///                 Ok((155, Token::RightParenthesis, 156)),
///                 Ok((156, Token::Semicolon, 157)),
///                 Ok((158, Token::Identifier("EnumClass_EnumWithinClass".to_string()), 183)),
///                 Ok((184, Token::Identifier("GetEnum".to_string()), 191)),
///                 Ok((191, Token::LeftParenthesis, 192)),
///                 Ok((192, Token::RightParenthesis, 193)),
///                 Ok((193, Token::Semicolon, 194)),
///                 Ok((195,
///                     Token::Identifier("EnumNamespace_EnumInNamespace".to_string()),
///                     224)),
///                 Ok((225, Token::Identifier("GetEnumFromNameSpace".to_string()), 245)),
///                 Ok((245, Token::LeftParenthesis, 246)),
///                 Ok((246, Token::RightParenthesis, 247)),
///                 Ok((247, Token::Semicolon, 248)),
///                 Ok((249, Token::RightBrace, 250)),
///                 Ok((250, Token::Semicolon, 251))]);
/// ```
///
/// # Errors
///
/// Because the lexer is implemented as an iterator over tokens, this means that you can continue
/// to get tokens even if a lexical error occurs. For example:
///
/// ```
/// use webidl::*;
///
/// let lexer = Lexer::new("identifier = 0xG");
/// assert_eq!(lexer.collect::<Vec<_>>(),
///            vec![Ok((0, Token::Identifier("identifier".to_string()), 10)),
///                 Ok((11, Token::Equals, 12)),
///                 Err(LexicalError {
///                         code: LexicalErrorCode::ExpectedHexadecimalDigit,
///                         location: 15,
///                     }),
///                 Ok((15, Token::Identifier("G".to_string()), 16))]);
///
/// ```
///
/// Of course, what follows after an error may just compound into more errors.
///
#[derive(Clone, Debug)]
pub struct Lexer<'input> {
    /// A peekable iterator of chars and their indices. This is created from the input string given
    /// by the user.
    chars: Peekable<CharIndices<'input>>,

    /// The original input string given by the user. Or alternatively, this may have been provided
    /// internally by `from_string`.
    input: &'input str,
}

impl<'input> Lexer<'input> {
    /// Produces an instance of the lexer with the lexical analysis to be performed on the `input`
    /// string. Note that no lexical analysis occurs until the lexer has been iterated over.
    pub fn new(input: &'input str) -> Self {
        Lexer {
            chars: input.char_indices().peekable(),
            input,
        }
    }

    /// A private function used by the iterator to retrieve the next token. This is essentially a
    /// finite state machine that matches over characters of the input. For the sake of readability,
    /// a lot of the lexing has been abstracted into separate functions.
    ///
    /// # Return Value
    ///
    /// The return value is an `Option`, with a value of `None` signifying EOF, whereas `Some` is
    /// a value of type `Spanned`.
    #[cfg_attr(rustfmt, rustfmt_skip)]
    #[allow(unknown_lints)]
    #[allow(cyclomatic_complexity)]
    fn get_next_token(&mut self) -> Option<<Self as Iterator>::Item> {
        loop {
            return match self.chars.next() {
                Some((_, '\t')) |
                Some((_, '\n')) |
                Some((_, '\r')) |
                Some((_, ' ')) => continue,
                Some((start, '_')) => {
                    match self.chars.peek() {
                        Some(&(_, c @ 'A'...'Z')) |
                        Some(&(_, c @ 'a'...'z')) => {
                            let mut identifier = "_".to_string();
                            identifier.push(c);
                            self.chars.next();
                            return self.lex_identifier_or_keyword(start, start + 2, identifier);
                        }
                        _ => Some(Ok((start, Token::OtherLiteral('_'), start + 1)))
                    }
                }
                Some((start, c @ 'A'...'Z')) |
                Some((start, c @ 'a'...'z')) => {
                    return self.lex_identifier_or_keyword(start, start + 1, c.to_string())
                }
                Some((start, c @ '0'...'9')) => {
                    return self.lex_integer_or_float_literal(start, start, "".to_string(), c)
                }
                Some((start, '"')) => return self.lex_string(start),
                Some((start, ':')) => return Some(Ok((start, Token::Colon, start + 1))),
                Some((start, ',')) => return Some(Ok((start, Token::Comma, start + 1))),
                Some((start, '=')) => return Some(Ok((start, Token::Equals, start + 1))),
                Some((start, '>')) => return Some(Ok((start, Token::GreaterThan, start + 1))),
                Some((start, '{')) => return Some(Ok((start, Token::LeftBrace, start + 1))),
                Some((start, '[')) => return Some(Ok((start, Token::LeftBracket, start + 1))),
                Some((start, '(')) => return Some(Ok((start, Token::LeftParenthesis, start + 1))),
                Some((start, '<')) => return Some(Ok((start, Token::LessThan, start + 1))),
                Some((start, '?')) => return Some(Ok((start, Token::QuestionMark, start + 1))),
                Some((start, '}')) => return Some(Ok((start, Token::RightBrace, start + 1))),
                Some((start, ']')) => return Some(Ok((start, Token::RightBracket, start + 1))),
                Some((start, ')')) => return Some(Ok((start, Token::RightParenthesis, start + 1))),
                Some((start, ';')) => return Some(Ok((start, Token::Semicolon, start + 1))),
                Some((start, '/')) => {
                    match self.chars.peek() {
                        Some(&(_, '/')) => {
                            self.lex_line_comment();
                            continue;
                        }
                        Some(&(_, '*')) => {
                            match self.lex_block_comment(start) {
                                Some(error) => return Some(error),
                                None => continue
                            }
                        }
                        _ => return Some(Ok((start, Token::OtherLiteral('/'), start + 1)))
                    }
                }
                Some((start, '.')) => {
                    match self.chars.peek() {
                        Some(&(_, '.')) => return self.lex_ellipsis(start),
                        Some(&(_, c @ '0'...'9')) => {
                            let mut float_literal = '.'.to_string();
                            float_literal.push(c);
                            self.chars.next();
                            return self.lex_float_literal(
                                start,
                                start + 2,
                                float_literal,
                                FloatLexState::ImmediatelyAfterDecimalPoint);
                        }
                        _ => return Some(Ok((start, Token::Period, start + 1))),
                    }
                }
                Some((start, '-')) => {
                    match self.chars.peek() {
                        Some(&(_, 'I')) => return self.lex_negative_infinity(start),
                        Some(&(_, '.')) => {
                            self.chars.next();
                            return self.lex_float_literal(
                                start,
                                start + 2,
                                "-.".to_string(),
                                FloatLexState::ImmediatelyAfterDecimalPoint);
                        }
                        Some(&(_, c @ '0'...'9')) => {
                            self.chars.next();
                            return self.lex_integer_or_float_literal(start,
                                                                     start + 1,
                                                                     "-".to_string(),
                                                                     c);
                        }
                        _ => return Some(Ok((start, Token::Hyphen, 1)))
                    }
                }
                Some((start, c)) => Some(Ok((start, Token::OtherLiteral(c), start + 1))),
                None => return None,
            }
        }
    }

    /// Continues the lexing of a block comment. The call assumption is that `/*` has already been
    /// lexed.
    fn lex_block_comment(&mut self, start: usize) -> Option<<Self as Iterator>::Item> {
        self.chars.next();
        let mut previous = start + 1;

        loop {
            previous += 1;

            match self.chars.next() {
                Some((_, '*')) => match self.chars.next() {
                    Some((_, '/')) => break,
                    Some(_) => continue,
                    None => {
                        return Some(Err(create_error(
                            LexicalErrorCode::ExpectedCommentBlockEnd,
                            previous + 1,
                        )))
                    }
                },
                Some(_) => continue,
                None => {
                    return Some(Err(create_error(
                        LexicalErrorCode::ExpectedCommentBlockEnd,
                        previous,
                    )))
                }
            }
        }

        None
    }

    /// Continues the lexing of ellipsis. The call assumption is that `..` has already been lexed.
    fn lex_ellipsis(&mut self, start: usize) -> Option<<Self as Iterator>::Item> {
        self.chars.next();

        match self.chars.peek() {
            Some(&(_, '.')) => {
                self.chars.next();
                Some(Ok((start, Token::Ellipsis, start + 3)))
            }
            _ => Some(Err(create_error(
                LexicalErrorCode::ExpectedEllipsis,
                start + 2,
            ))),
        }
    }

    /// A small DFA for lexing float literals. The call assumption differs depending on the initial
    /// state.
    fn lex_float_literal(
        &mut self,
        start: usize,
        mut offset: usize,
        mut float_literal: String,
        mut float_lex_state: FloatLexState,
    ) -> Option<<Self as Iterator>::Item> {
        loop {
            match float_lex_state {
                FloatLexState::BeforeDecimalPoint => match self.chars.peek() {
                    Some(&(_, c @ '0'...'9')) => {
                        self.push_next_char(&mut float_literal, c, &mut offset)
                    }
                    Some(&(_, 'e')) | Some(&(_, 'E')) => {
                        self.push_next_char(&mut float_literal, 'e', &mut offset);
                        float_lex_state = FloatLexState::ImmediatelyAfterExponentBase;
                    }
                    Some(&(_, '.')) => {
                        self.push_next_char(&mut float_literal, '.', &mut offset);
                        float_lex_state = FloatLexState::ImmediatelyAfterDecimalPoint;
                    }
                    _ => panic!(
                        "Integer literals should not be\
                         able to be lexed as float literals"
                    ),
                },
                FloatLexState::ImmediatelyAfterDecimalPoint => match self.chars.peek() {
                    Some(&(_, c @ '0'...'9')) => {
                        self.push_next_char(&mut float_literal, c, &mut offset);
                        float_lex_state = FloatLexState::AfterDecimalPoint;
                    }
                    Some(&(_, 'e')) | Some(&(_, 'E')) => {
                        if float_literal.chars().count() == 1 {
                            panic!(
                                "A leading decimal point followed by\
                                 an exponent should not be possible"
                            );
                        }

                        self.push_next_char(&mut float_literal, 'e', &mut offset);
                        float_lex_state = FloatLexState::ImmediatelyAfterExponentBase;
                    }
                    _ if float_literal.starts_with("-.") => {
                        return Some(Err(create_error(
                            LexicalErrorCode::ExpectedDecimalDigit,
                            offset,
                        )))
                    }
                    _ => {
                        return Some(Ok((
                            start,
                            Token::FloatLiteral(float_literal.parse().unwrap()),
                            offset,
                        )));
                    }
                },
                FloatLexState::AfterDecimalPoint => match self.chars.peek() {
                    Some(&(_, c @ '0'...'9')) => {
                        self.push_next_char(&mut float_literal, c, &mut offset);
                    }
                    Some(&(_, 'e')) | Some(&(_, 'E')) => {
                        self.push_next_char(&mut float_literal, 'e', &mut offset);
                        float_lex_state = FloatLexState::ImmediatelyAfterExponentBase;
                    }
                    _ => {
                        return Some(Ok((
                            start,
                            Token::FloatLiteral(float_literal.parse().unwrap()),
                            offset,
                        )));
                    }
                },
                FloatLexState::ImmediatelyAfterExponentBase => match self.chars.peek() {
                    Some(&(_, c @ '0'...'9')) => {
                        self.push_next_char(&mut float_literal, c, &mut offset);
                        float_lex_state = FloatLexState::AfterExponentBase;
                    }
                    Some(&(_, c @ '+')) | Some(&(_, c @ '-')) => {
                        self.push_next_char(&mut float_literal, c, &mut offset);
                        float_lex_state = FloatLexState::ImmediatelyAfterExponentSign;
                    }
                    _ => {
                        return Some(Err(create_error(
                            LexicalErrorCode::ExpectedFloatExponent,
                            offset,
                        )))
                    }
                },
                FloatLexState::ImmediatelyAfterExponentSign => match self.chars.peek() {
                    Some(&(_, c @ '0'...'9')) => {
                        self.push_next_char(&mut float_literal, c, &mut offset);
                        float_lex_state = FloatLexState::AfterExponentBase;
                    }
                    _ => {
                        return Some(Err(create_error(
                            LexicalErrorCode::ExpectedFloatExponent,
                            offset,
                        )))
                    }
                },
                FloatLexState::AfterExponentBase => match self.chars.peek() {
                    Some(&(_, c @ '0'...'9')) => {
                        self.push_next_char(&mut float_literal, c, &mut offset);
                    }
                    _ => {
                        return Some(Ok((
                            start,
                            Token::FloatLiteral(float_literal.parse().unwrap()),
                            offset,
                        )));
                    }
                },
            }
        }
    }

    /// Continues lexing a hexadecimal literal. The call assumption is that `0x` has already been
    /// lexed.
    fn lex_hexadecimal_literal(
        &mut self,
        start: usize,
        mut offset: usize,
        mut hexadecimal_literal: String,
    ) -> Option<<Self as Iterator>::Item> {
        offset += 2;
        self.chars.next();

        match self.chars.peek() {
            Some(&(_, c)) if c.is_digit(16) => loop {
                match self.chars.peek() {
                    Some(&(_, c)) if c.is_digit(16) => {
                        self.push_next_char(&mut hexadecimal_literal, c, &mut offset);
                    }
                    _ => {
                        let hexadecimal_literal =
                            i64::from_str_radix(&*hexadecimal_literal, 16).unwrap();
                        return Some(Ok((
                            start,
                            Token::IntegerLiteral(hexadecimal_literal),
                            offset,
                        )));
                    }
                }
            },
            _ => Some(Err(create_error(
                LexicalErrorCode::ExpectedHexadecimalDigit,
                offset,
            ))),
        }
    }

    /// Continues lexing an identifier. The call assumption is that the first letter or the first
    /// letter preceded by a `_` have already been lexed.
    fn lex_identifier(&mut self, offset: &mut usize, mut identifier: &mut String) {
        loop {
            match self.chars.peek() {
                Some(&(_, c @ 'A'...'Z'))
                | Some(&(_, c @ 'a'...'z'))
                | Some(&(_, c @ '0'...'9'))
                | Some(&(_, c @ '_'))
                | Some(&(_, c @ '-')) => {
                    self.push_next_char(&mut identifier, c, offset);
                }
                _ => break,
            }
        }
    }

    /// Continues lexing an identifier with the potential that it is a keyword. The call assumption
    /// is that the first letter or the first letter preceded by a `_` have already been lexed.
    fn lex_identifier_or_keyword(
        &mut self,
        start: usize,
        mut offset: usize,
        mut identifier: String,
    ) -> Option<<Self as Iterator>::Item> {
        self.lex_identifier(&mut offset, &mut identifier);

        let token = match &*identifier {
            "ArrayBuffer" => Token::ArrayBuffer,
            "ByteString" => Token::ByteString,
            "DataView" => Token::DataView,
            "DOMString" => Token::DOMString,
            "Error" => Token::Error,
            "Float32Array" => Token::Float32Array,
            "Float64Array" => Token::Float64Array,
            "FrozenArray" => Token::FrozenArray,
            "Infinity" => Token::PositiveInfinity,
            "Int16Array" => Token::Int16Array,
            "Int32Array" => Token::Int32Array,
            "Int8Array" => Token::Int8Array,
            "NaN" => Token::NaN,
            "Promise" => Token::Promise,
            "USVString" => Token::USVString,
            "Uint16Array" => Token::Uint16Array,
            "Uint32Array" => Token::Uint32Array,
            "Uint8Array" => Token::Uint8Array,
            "Uint8ClampedArray" => Token::Uint8ClampedArray,
            "any" => Token::Any,
            "attribute" => Token::Attribute,
            "boolean" => Token::Boolean,
            "byte" => Token::Byte,
            "callback" => Token::Callback,
            "const" => Token::Const,
            "deleter" => Token::Deleter,
            "dictionary" => Token::Dictionary,
            "double" => Token::Double,
            "enum" => Token::Enum,
            "false" => Token::False,
            "float" => Token::Float,
            "getter" => Token::Getter,
            "implements" => Token::Implements,
            "includes" => Token::Includes,
            "inherit" => Token::Inherit,
            "interface" => Token::Interface,
            "iterable" => Token::Iterable,
            "legacycaller" => Token::LegacyCaller,
            "long" => Token::Long,
            "maplike" => Token::Maplike,
            "mixin" => Token::Mixin,
            "namespace" => Token::Namespace,
            "null" => Token::Null,
            "object" => Token::Object,
            "octet" => Token::Octet,
            "optional" => Token::Optional,
            "or" => Token::Or,
            "partial" => Token::Partial,
            "readonly" => Token::ReadOnly,
            "record" => Token::Record,
            "required" => Token::Required,
            "sequence" => Token::Sequence,
            "setlike" => Token::Setlike,
            "setter" => Token::Setter,
            "short" => Token::Short,
            "static" => Token::Static,
            "stringifier" => Token::Stringifier,
            "symbol" => Token::Symbol,
            "typedef" => Token::Typedef,
            "true" => Token::True,
            "unrestricted" => Token::Unrestricted,
            "unsigned" => Token::Unsigned,
            "void" => Token::Void,
            _ => if identifier.starts_with('_') {
                Token::Identifier(identifier.split_at(1).1.to_string())
            } else {
                Token::Identifier(identifier)
            },
        };

        Some(Ok((start, token, offset)))
    }

    /// Continues lexing either an integer or float literal. The call assumption is that `c` is
    /// a decimal digit.
    fn lex_integer_or_float_literal(
        &mut self,
        start: usize,
        mut offset: usize,
        mut literal: String,
        c: char,
    ) -> Option<<Self as Iterator>::Item> {
        match c {
            '0' => match self.chars.peek() {
                Some(&(_, 'x')) | Some(&(_, 'X')) => {
                    self.lex_hexadecimal_literal(start, offset, literal)
                }
                Some(&(_, c @ '0'...'9')) => {
                    offset += 2;
                    literal.push(c);

                    if c > '7' {
                        if !self.lookahead_for_decimal_point() {
                            return Some(Ok((start, Token::IntegerLiteral(0), offset - 1)));
                        }

                        self.chars.next();
                        return self.lex_float_literal(
                            start,
                            offset,
                            literal,
                            FloatLexState::BeforeDecimalPoint,
                        );
                    }

                    self.chars.next();

                    loop {
                        match self.chars.peek() {
                            Some(&(_, c @ '0'...'9')) => {
                                if c > '7' {
                                    if !self.lookahead_for_decimal_point() {
                                        let literal = i64::from_str_radix(&*literal, 8).unwrap();
                                        return Some(Ok((
                                            start,
                                            Token::IntegerLiteral(literal),
                                            offset,
                                        )));
                                    }

                                    self.push_next_char(&mut literal, c, &mut offset);

                                    return self.lex_float_literal(
                                        start,
                                        offset,
                                        literal,
                                        FloatLexState::BeforeDecimalPoint,
                                    );
                                }

                                self.push_next_char(&mut literal, c, &mut offset);

                                if c > '7' {
                                    return self.lex_float_literal(
                                        start,
                                        offset,
                                        literal,
                                        FloatLexState::BeforeDecimalPoint,
                                    );
                                }
                            }
                            Some(&(_, '.')) => {
                                self.push_next_char(&mut literal, '.', &mut offset);

                                return self.lex_float_literal(
                                    start,
                                    offset,
                                    literal,
                                    FloatLexState::ImmediatelyAfterDecimalPoint,
                                );
                            }
                            Some(&(_, 'e')) | Some(&(_, 'E')) => {
                                self.push_next_char(&mut literal, 'e', &mut offset);

                                return self.lex_float_literal(
                                    start,
                                    offset,
                                    literal,
                                    FloatLexState::ImmediatelyAfterExponentBase,
                                );
                            }
                            _ => {
                                let literal = i64::from_str_radix(&*literal, 8).unwrap();
                                return Some(Ok((start, Token::IntegerLiteral(literal), offset)));
                            }
                        }
                    }
                }
                Some(&(_, '.')) => {
                    self.chars.next();
                    self.lex_float_literal(
                        start,
                        start + 2,
                        "0.".to_string(),
                        FloatLexState::ImmediatelyAfterDecimalPoint,
                    )
                }
                Some(&(_, 'e')) | Some(&(_, 'E')) => {
                    self.chars.next();
                    self.lex_float_literal(
                        start,
                        start + 2,
                        "0e".to_string(),
                        FloatLexState::ImmediatelyAfterExponentBase,
                    )
                }
                _ => Some(Ok((start, Token::IntegerLiteral(0), start + 1))),
            },
            c => {
                literal.push(c);
                offset += 1;

                loop {
                    match self.chars.peek() {
                        Some(&(_, c @ '0'...'9')) => {
                            self.push_next_char(&mut literal, c, &mut offset)
                        }
                        Some(&(_, '.')) => {
                            self.push_next_char(&mut literal, '.', &mut offset);

                            return self.lex_float_literal(
                                start,
                                offset,
                                literal,
                                FloatLexState::ImmediatelyAfterDecimalPoint,
                            );
                        }
                        Some(&(_, 'e')) | Some(&(_, 'E')) => {
                            self.push_next_char(&mut literal, 'e', &mut offset);

                            return self.lex_float_literal(
                                start,
                                offset,
                                literal,
                                FloatLexState::ImmediatelyAfterExponentBase,
                            );
                        }
                        _ => {
                            let literal = literal.parse::<i64>().unwrap();
                            return Some(Ok((start, Token::IntegerLiteral(literal), offset)));
                        }
                    }
                }
            }
        }
    }

    /// Continues lexing a line comment. The call assumption is that `//` have already been lexed.
    fn lex_line_comment(&mut self) {
        while let Some(&(_, c)) = self.chars.peek() {
            if c == '\n' {
                break;
            }

            self.chars.next();
        }
    }

    /// Continues lexing negative infinity. The call assumption is that `-` has already been
    /// lexed with the assumption that "I" follows.
    fn lex_negative_infinity(&mut self, start: usize) -> Option<<Self as Iterator>::Item> {
        let infinity = self.chars
            .clone()
            .take(8)
            .map(|(_, c)| c)
            .collect::<String>();

        if infinity == "Infinity" {
            for _ in 0..8 {
                self.chars.next();
            }

            Some(Ok((start, Token::NegativeInfinity, start + 9)))
        } else {
            let char_count = infinity.chars().count();

            for _ in char_count..8 {
                self.chars.next();
            }

            Some(Err(LexicalError {
                code: LexicalErrorCode::ExpectedKeywordInfinity,
                location: start + char_count,
            }))
        }
    }

    /// Continues lexing a string literal. The call assumption is that `"` has already been lexed.
    fn lex_string(&mut self, start: usize) -> Option<<Self as Iterator>::Item> {
        let mut previous = start;
        let mut string = String::new();

        loop {
            previous += 1;

            match self.chars.next() {
                Some((_, '"')) => {
                    return Some(Ok((start, Token::StringLiteral(string), previous + 1)))
                }
                Some((_, c)) => {
                    string.push(c);
                }
                None => {
                    return Some(Err(create_error(
                        LexicalErrorCode::ExpectedStringLiteralEnd,
                        previous + 1,
                    )))
                }
            }
        }
    }

    /// A helper function that will lookahead at the following characters to see if a decimal point
    /// follows digits. This is needed particularly in the case when lexing `01238` or similar.
    fn lookahead_for_decimal_point(&mut self) -> bool {
        let mut chars = self.chars.clone();

        loop {
            match chars.next() {
                Some((_, '0'...'9')) => continue,
                Some((_, '.')) => return true,
                _ => return false,
            }
        }
    }

    /// A helper function for performing the repetitive task of proceeding to the next character,
    /// appending a character to a token, and incrementing the offset.
    fn push_next_char(&mut self, token: &mut String, next_char: char, offset: &mut usize) {
        self.chars.next();
        token.push(next_char);
        *offset += 1;
    }
}

impl<'input> Iterator for Lexer<'input> {
    type Item = Spanned<Token, usize, LexicalError>;

    fn next(&mut self) -> Option<Self::Item> {
        self.get_next_token()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn assert_lex(input: &str, expected: Vec<Spanned<Token, usize, LexicalError>>) {
        let lexer = Lexer::new(input);
        assert_eq!(lexer.collect::<Vec<_>>(), expected);
    }

    #[test]
    fn lex_colon() {
        assert_lex(":", vec![Ok((0, Token::Colon, 1))]);
    }

    #[test]
    fn lex_comma() {
        assert_lex(",", vec![Ok((0, Token::Comma, 1))]);
    }

    #[test]
    fn lex_comment() {
        assert_lex("// this is a comment", vec![]);
        assert_lex("/* this is a comment */", vec![]);
        assert_lex(
            "/* this is a comment",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedCommentBlockEnd, 20)),
            ],
        );
        assert_lex(
            "/* this is a comment*",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedCommentBlockEnd, 21)),
            ],
        );
    }

    #[test]
    fn lex_ellipsis() {
        assert_lex("...", vec![Ok((0, Token::Ellipsis, 3))]);
        assert_lex(
            "..",
            vec![Err(create_error(LexicalErrorCode::ExpectedEllipsis, 2))],
        );
    }

    #[test]
    fn lex_equals() {
        assert_lex("=", vec![Ok((0, Token::Equals, 1))]);
    }

    #[test]
    fn lex_float_literal() {
        // With leading 0
        assert_lex("0.", vec![Ok((0, Token::FloatLiteral(0.0), 2))]);
        assert_lex("000051.", vec![Ok((0, Token::FloatLiteral(51.0), 7))]);
        assert_lex("05162.", vec![Ok((0, Token::FloatLiteral(5162.0), 6))]);
        assert_lex("099.", vec![Ok((0, Token::FloatLiteral(99.0), 4))]);
        assert_lex(
            "04624.51235",
            vec![Ok((0, Token::FloatLiteral(4624.51235), 11))],
        );
        assert_lex("0.987", vec![Ok((0, Token::FloatLiteral(0.987), 5))]);
        assert_lex("0.55e10", vec![Ok((0, Token::FloatLiteral(0.55e10), 7))]);
        assert_lex("0612e61", vec![Ok((0, Token::FloatLiteral(612e61), 7))]);
        assert_lex("0e-1", vec![Ok((0, Token::FloatLiteral(0e-1), 4))]);
        assert_lex("041e+9", vec![Ok((0, Token::FloatLiteral(41e+9), 6))]);
        assert_lex(
            "021e",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedFloatExponent, 4)),
            ],
        );
        assert_lex(
            "01e+",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedFloatExponent, 4)),
            ],
        );
        assert_lex(
            "01e-",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedFloatExponent, 4)),
            ],
        );

        // Without leading 0
        assert_lex("5162.", vec![Ok((0, Token::FloatLiteral(5162.0), 5))]);
        assert_lex("99.", vec![Ok((0, Token::FloatLiteral(99.0), 3))]);
        assert_lex(
            "4624.51235",
            vec![Ok((0, Token::FloatLiteral(4624.51235), 10))],
        );
        assert_lex("612e61", vec![Ok((0, Token::FloatLiteral(612e61), 6))]);
        assert_lex("41e+9", vec![Ok((0, Token::FloatLiteral(41e+9), 5))]);
        assert_lex(
            "21e",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedFloatExponent, 3)),
            ],
        );
        assert_lex(
            "1e+",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedFloatExponent, 3)),
            ],
        );
        assert_lex(
            "1e-",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedFloatExponent, 3)),
            ],
        );

        // With leading decimal point
        assert_lex(".5", vec![Ok((0, Token::FloatLiteral(0.5), 2))]);
        assert_lex(".612e-10", vec![Ok((0, Token::FloatLiteral(0.612e-10), 8))]);

        // With leading negative sign
        assert_lex("-700.5", vec![Ok((0, Token::FloatLiteral(-700.5), 6))]);
        assert_lex("-9.e2", vec![Ok((0, Token::FloatLiteral(-9.0e2), 5))]);
        assert_lex("-.5e1", vec![Ok((0, Token::FloatLiteral(-0.5e1), 5))]);
        assert_lex("-.0", vec![Ok((0, Token::FloatLiteral(0.0), 3))]);
        assert_lex(
            "-.",
            vec![Err(create_error(LexicalErrorCode::ExpectedDecimalDigit, 2))],
        );
    }

    #[test]
    fn lex_greater_than() {
        assert_lex(">", vec![Ok((0, Token::GreaterThan, 1))]);
    }

    #[test]
    fn lex_hyphen() {
        assert_lex("-", vec![Ok((0, Token::Hyphen, 1))]);
    }

    #[test]
    fn lex_identifier() {
        assert_lex(
            "_identifier",
            vec![Ok((0, Token::Identifier("identifier".to_string()), 11))],
        );
        assert_lex(
            "_Identifier",
            vec![Ok((0, Token::Identifier("Identifier".to_string()), 11))],
        );
        assert_lex(
            "identifier",
            vec![Ok((0, Token::Identifier("identifier".to_string()), 10))],
        );
        assert_lex(
            "Identifier",
            vec![Ok((0, Token::Identifier("Identifier".to_string()), 10))],
        );
        assert_lex(
            "z0123",
            vec![Ok((0, Token::Identifier("z0123".to_string()), 5))],
        );
        assert_lex(
            "i-d-e_t0123",
            vec![Ok((0, Token::Identifier("i-d-e_t0123".to_string()), 11))],
        );
    }

    #[test]
    fn lex_integer_literal() {
        // Decimal
        assert_lex("1", vec![Ok((0, Token::IntegerLiteral(1), 1))]);
        assert_lex("9624", vec![Ok((0, Token::IntegerLiteral(9624), 4))]);
        assert_lex("-1", vec![Ok((0, Token::IntegerLiteral(-1), 2))]);
        assert_lex("-9624", vec![Ok((0, Token::IntegerLiteral(-9624), 5))]);

        // Hexadecimal
        assert_lex("0x0", vec![Ok((0, Token::IntegerLiteral(0x0), 3))]);
        assert_lex(
            "0x1234FF",
            vec![Ok((0, Token::IntegerLiteral(0x1234FF), 8))],
        );
        assert_lex(
            "0x",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedHexadecimalDigit, 2)),
            ],
        );
        assert_lex("-0x0", vec![Ok((0, Token::IntegerLiteral(0x0), 4))]);
        assert_lex(
            "-0x1234FF",
            vec![Ok((0, Token::IntegerLiteral(-0x1234FF), 9))],
        );
        assert_lex(
            "-0x",
            vec![
                Err(create_error(LexicalErrorCode::ExpectedHexadecimalDigit, 3)),
            ],
        );

        // Octal
        assert_lex("0", vec![Ok((0, Token::IntegerLiteral(0), 1))]);
        assert_lex("0624", vec![Ok((0, Token::IntegerLiteral(0o624), 4))]);
        assert_lex("-0624", vec![Ok((0, Token::IntegerLiteral(-0o624), 5))]);

        // Octal integer literal followed by non-octal digits.
        assert_lex(
            "08",
            vec![
                Ok((0, Token::IntegerLiteral(0), 1)),
                Ok((1, Token::IntegerLiteral(8), 2)),
            ],
        );
        assert_lex(
            "01238",
            vec![
                Ok((0, Token::IntegerLiteral(0o123), 4)),
                Ok((4, Token::IntegerLiteral(8), 5)),
            ],
        );
    }

    #[test]
    fn lex_keyword() {
        assert_lex("ArrayBuffer", vec![Ok((0, Token::ArrayBuffer, 11))]);
        assert_lex("ByteString", vec![Ok((0, Token::ByteString, 10))]);
        assert_lex("DataView", vec![Ok((0, Token::DataView, 8))]);
        assert_lex("DOMString", vec![Ok((0, Token::DOMString, 9))]);
        assert_lex("Error", vec![Ok((0, Token::Error, 5))]);
        assert_lex("Float32Array", vec![Ok((0, Token::Float32Array, 12))]);
        assert_lex("Float64Array", vec![Ok((0, Token::Float64Array, 12))]);
        assert_lex("FrozenArray", vec![Ok((0, Token::FrozenArray, 11))]);
        assert_lex("Infinity", vec![Ok((0, Token::PositiveInfinity, 8))]);
        assert_lex("Int16Array", vec![Ok((0, Token::Int16Array, 10))]);
        assert_lex("Int32Array", vec![Ok((0, Token::Int32Array, 10))]);
        assert_lex("Int8Array", vec![Ok((0, Token::Int8Array, 9))]);
        assert_lex("NaN", vec![Ok((0, Token::NaN, 3))]);
        assert_lex("Promise", vec![Ok((0, Token::Promise, 7))]);
        assert_lex("USVString", vec![Ok((0, Token::USVString, 9))]);
        assert_lex("Uint16Array", vec![Ok((0, Token::Uint16Array, 11))]);
        assert_lex("Uint32Array", vec![Ok((0, Token::Uint32Array, 11))]);
        assert_lex("Uint8Array", vec![Ok((0, Token::Uint8Array, 10))]);
        assert_lex(
            "Uint8ClampedArray",
            vec![Ok((0, Token::Uint8ClampedArray, 17))],
        );
        assert_lex("any", vec![Ok((0, Token::Any, 3))]);
        assert_lex("attribute", vec![Ok((0, Token::Attribute, 9))]);
        assert_lex("boolean", vec![Ok((0, Token::Boolean, 7))]);
        assert_lex("byte", vec![Ok((0, Token::Byte, 4))]);
        assert_lex("callback", vec![Ok((0, Token::Callback, 8))]);
        assert_lex("const", vec![Ok((0, Token::Const, 5))]);
        assert_lex("deleter", vec![Ok((0, Token::Deleter, 7))]);
        assert_lex("dictionary", vec![Ok((0, Token::Dictionary, 10))]);
        assert_lex("double", vec![Ok((0, Token::Double, 6))]);
        assert_lex("enum", vec![Ok((0, Token::Enum, 4))]);
        assert_lex("false", vec![Ok((0, Token::False, 5))]);
        assert_lex("float", vec![Ok((0, Token::Float, 5))]);
        assert_lex("getter", vec![Ok((0, Token::Getter, 6))]);
        assert_lex("implements", vec![Ok((0, Token::Implements, 10))]);
        assert_lex("includes", vec![Ok((0, Token::Includes, 8))]);
        assert_lex("inherit", vec![Ok((0, Token::Inherit, 7))]);
        assert_lex("interface", vec![Ok((0, Token::Interface, 9))]);
        assert_lex("iterable", vec![Ok((0, Token::Iterable, 8))]);
        assert_lex("legacycaller", vec![Ok((0, Token::LegacyCaller, 12))]);
        assert_lex("long", vec![Ok((0, Token::Long, 4))]);
        assert_lex("maplike", vec![Ok((0, Token::Maplike, 7))]);
        assert_lex("mixin", vec![Ok((0, Token::Mixin, 5))]);
        assert_lex("namespace", vec![Ok((0, Token::Namespace, 9))]);
        assert_lex("null", vec![Ok((0, Token::Null, 4))]);
        assert_lex("object", vec![Ok((0, Token::Object, 6))]);
        assert_lex("octet", vec![Ok((0, Token::Octet, 5))]);
        assert_lex("optional", vec![Ok((0, Token::Optional, 8))]);
        assert_lex("or", vec![Ok((0, Token::Or, 2))]);
        assert_lex("partial", vec![Ok((0, Token::Partial, 7))]);
        assert_lex("readonly", vec![Ok((0, Token::ReadOnly, 8))]);
        assert_lex("record", vec![Ok((0, Token::Record, 6))]);
        assert_lex("required", vec![Ok((0, Token::Required, 8))]);
        assert_lex("sequence", vec![Ok((0, Token::Sequence, 8))]);
        assert_lex("setlike", vec![Ok((0, Token::Setlike, 7))]);
        assert_lex("setter", vec![Ok((0, Token::Setter, 6))]);
        assert_lex("short", vec![Ok((0, Token::Short, 5))]);
        assert_lex("static", vec![Ok((0, Token::Static, 6))]);
        assert_lex("stringifier", vec![Ok((0, Token::Stringifier, 11))]);
        assert_lex("symbol", vec![Ok((0, Token::Symbol, 6))]);
        assert_lex("true", vec![Ok((0, Token::True, 4))]);
        assert_lex("typedef", vec![Ok((0, Token::Typedef, 7))]);
        assert_lex("unsigned", vec![Ok((0, Token::Unsigned, 8))]);
        assert_lex("unrestricted", vec![Ok((0, Token::Unrestricted, 12))]);
        assert_lex("void", vec![Ok((0, Token::Void, 4))]);
    }

    #[test]
    fn lex_left_brace() {
        assert_lex("{", vec![Ok((0, Token::LeftBrace, 1))]);
    }

    #[test]
    fn lex_left_bracket() {
        assert_lex("[", vec![Ok((0, Token::LeftBracket, 1))]);
    }

    #[test]
    fn lex_left_parenthesis() {
        assert_lex("(", vec![Ok((0, Token::LeftParenthesis, 1))]);
    }

    #[test]
    fn lex_less_than() {
        assert_lex("<", vec![Ok((0, Token::LessThan, 1))]);
    }

    #[test]
    fn lex_negative_infinity() {
        assert_lex("-Infinity", vec![Ok((0, Token::NegativeInfinity, 9))]);
        assert_lex(
            "-Infinity;",
            vec![
                Ok((0, Token::NegativeInfinity, 9)),
                Ok((9, Token::Semicolon, 10)),
            ],
        );
    }

    #[test]
    fn lex_other_literal() {
        assert_lex("%", vec![Ok((0, Token::OtherLiteral('%'), 1))]);
        assert_lex("/", vec![Ok((0, Token::OtherLiteral('/'), 1))]);
        assert_lex("!", vec![Ok((0, Token::OtherLiteral('!'), 1))]);
        assert_lex("_", vec![Ok((0, Token::OtherLiteral('_'), 1))]);
    }

    #[test]
    fn lex_period() {
        assert_lex(".", vec![Ok((0, Token::Period, 1))]);
    }

    #[test]
    fn lex_question_mark() {
        assert_lex("?", vec![Ok((0, Token::QuestionMark, 1))]);
    }

    #[test]
    fn lex_right_brace() {
        assert_lex("}", vec![Ok((0, Token::RightBrace, 1))]);
    }

    #[test]
    fn lex_right_bracket() {
        assert_lex("]", vec![Ok((0, Token::RightBracket, 1))]);
    }

    #[test]
    fn lex_right_parenthesis() {
        assert_lex(")", vec![Ok((0, Token::RightParenthesis, 1))]);
    }

    #[test]
    fn lex_semicolon() {
        assert_lex(";", vec![Ok((0, Token::Semicolon, 1))]);
    }

    #[test]
    fn lex_string() {
        assert_lex(
            r#""this is a string""#,
            vec![
                Ok((0, Token::StringLiteral("this is a string".to_string()), 18)),
            ],
        );
        assert_lex(
            r#""this is a string"#,
            vec![
                Err(create_error(LexicalErrorCode::ExpectedStringLiteralEnd, 18)),
            ],
        );
    }

    #[test]
    fn lex_whitespace() {
        assert_lex("      \n \t", vec![]);
        assert_lex("\r\n", vec![]);
    }
}
