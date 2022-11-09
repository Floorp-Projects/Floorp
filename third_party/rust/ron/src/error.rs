use serde::{de, ser};
use std::{error::Error as StdError, fmt, io, str::Utf8Error, string::FromUtf8Error};

/// This type represents all possible errors that can occur when
/// serializing or deserializing RON data.
#[derive(Clone, Debug, PartialEq)]
pub struct Error {
    pub code: ErrorCode,
    pub position: Position,
}

pub type Result<T> = std::result::Result<T, Error>;

#[derive(Clone, Debug, PartialEq)]
pub enum ErrorCode {
    Io(String),
    Message(String),
    Base64Error(base64::DecodeError),
    Eof,
    ExpectedArray,
    ExpectedArrayEnd,
    ExpectedAttribute,
    ExpectedAttributeEnd,
    ExpectedBoolean,
    ExpectedComma,
    ExpectedChar,
    ExpectedFloat,
    FloatUnderscore,
    ExpectedInteger,
    ExpectedOption,
    ExpectedOptionEnd,
    ExpectedMap,
    ExpectedMapColon,
    ExpectedMapEnd,
    ExpectedStructName {
        expected: &'static str,
        found: String,
    },
    ExpectedStruct,
    ExpectedNamedStruct(&'static str),
    ExpectedStructEnd,
    ExpectedUnit,
    ExpectedString,
    ExpectedStringEnd,
    ExpectedIdentifier,

    InvalidEscape(&'static str),

    IntegerOutOfBounds,

    NoSuchExtension(String),

    UnclosedBlockComment,
    UnderscoreAtBeginning,
    UnexpectedByte(char),

    Utf8Error(Utf8Error),
    TrailingCharacters,

    #[doc(hidden)]
    __Nonexhaustive,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if (self.position == Position { line: 0, col: 0 }) {
            write!(f, "{}", self.code)
        } else {
            write!(f, "{}: {}", self.position, self.code)
        }
    }
}

impl fmt::Display for ErrorCode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            ErrorCode::Io(ref s) => f.write_str(s),
            ErrorCode::Message(ref s) => f.write_str(s),
            ErrorCode::Base64Error(ref e) => fmt::Display::fmt(e, f),
            ErrorCode::Eof => f.write_str("Unexpected end of RON"),
            ErrorCode::ExpectedArray => f.write_str("Expected opening `[`"),
            ErrorCode::ExpectedArrayEnd => f.write_str("Expected closing `]`"),
            ErrorCode::ExpectedAttribute => f.write_str("Expected an `#![enable(...)]` attribute"),
            ErrorCode::ExpectedAttributeEnd => {
                f.write_str("Expected closing `)]` after the enable attribute")
            }
            ErrorCode::ExpectedBoolean => f.write_str("Expected boolean"),
            ErrorCode::ExpectedComma => f.write_str("Expected comma"),
            ErrorCode::ExpectedChar => f.write_str("Expected char"),
            ErrorCode::ExpectedFloat => f.write_str("Expected float"),
            ErrorCode::FloatUnderscore => f.write_str("Unexpected underscore in float"),
            ErrorCode::ExpectedInteger => f.write_str("Expected integer"),
            ErrorCode::ExpectedOption => f.write_str("Expected option"),
            ErrorCode::ExpectedOptionEnd => f.write_str("Expected closing `)`"),
            ErrorCode::ExpectedMap => f.write_str("Expected opening `{`"),
            ErrorCode::ExpectedMapColon => f.write_str("Expected colon"),
            ErrorCode::ExpectedMapEnd => f.write_str("Expected closing `}`"),
            ErrorCode::ExpectedStructName {
                expected,
                ref found,
            } => write!(f, "Expected struct '{}' but found '{}'", expected, found),
            ErrorCode::ExpectedStruct => f.write_str("Expected opening `(`"),
            ErrorCode::ExpectedNamedStruct(name) => {
                write!(f, "Expected opening `(` for struct '{}'", name)
            }
            ErrorCode::ExpectedStructEnd => f.write_str("Expected closing `)`"),
            ErrorCode::ExpectedUnit => f.write_str("Expected unit"),
            ErrorCode::ExpectedString => f.write_str("Expected string"),
            ErrorCode::ExpectedStringEnd => f.write_str("Expected end of string"),
            ErrorCode::ExpectedIdentifier => f.write_str("Expected identifier"),
            ErrorCode::InvalidEscape(e) => write!(f, "Invalid escape sequence '{}'", e),
            ErrorCode::IntegerOutOfBounds => f.write_str("Integer is out of bounds"),
            ErrorCode::NoSuchExtension(ref name) => write!(f, "No RON extension '{}'", name),
            ErrorCode::Utf8Error(ref e) => fmt::Display::fmt(e, f),
            ErrorCode::UnclosedBlockComment => f.write_str("Unclosed block comment"),
            ErrorCode::UnderscoreAtBeginning => {
                f.write_str("Unexpected leading underscore in an integer")
            }
            ErrorCode::UnexpectedByte(ref byte) => write!(f, "Unexpected byte {:?}", byte),
            ErrorCode::TrailingCharacters => f.write_str("Non-whitespace trailing characters"),
            _ => f.write_str("Unknown ErrorCode"),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Position {
    pub line: usize,
    pub col: usize,
}

impl fmt::Display for Position {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

impl de::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error {
            code: ErrorCode::Message(msg.to_string()),
            position: Position { line: 0, col: 0 },
        }
    }
}

impl ser::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error {
            code: ErrorCode::Message(msg.to_string()),
            position: Position { line: 0, col: 0 },
        }
    }
}

impl StdError for Error {}

impl From<Utf8Error> for ErrorCode {
    fn from(e: Utf8Error) -> Self {
        ErrorCode::Utf8Error(e)
    }
}

impl From<FromUtf8Error> for ErrorCode {
    fn from(e: FromUtf8Error) -> Self {
        ErrorCode::Utf8Error(e.utf8_error())
    }
}

impl From<Utf8Error> for Error {
    fn from(e: Utf8Error) -> Self {
        Error {
            code: ErrorCode::Utf8Error(e),
            position: Position { line: 0, col: 0 },
        }
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Self {
        Error {
            code: ErrorCode::Io(e.to_string()),
            position: Position { line: 0, col: 0 },
        }
    }
}
