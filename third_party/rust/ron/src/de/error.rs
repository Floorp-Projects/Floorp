use serde::de;
use std::{error::Error as StdError, fmt, io, str::Utf8Error, string::FromUtf8Error};

use crate::parse::Position;

/// Deserialization result.
pub type Result<T> = std::result::Result<T, Error>;

#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    IoError(String),
    Message(String),
    Parser(ParseError, Position),
}

#[derive(Clone, Debug, PartialEq)]
pub enum ParseError {
    Base64Error(base64::DecodeError),
    Eof,
    ExpectedArray,
    ExpectedArrayEnd,
    ExpectedAttribute,
    ExpectedAttributeEnd,
    ExpectedBoolean,
    ExpectedComma,
    ExpectedEnum,
    ExpectedChar,
    ExpectedFloat,
    ExpectedInteger,
    ExpectedOption,
    ExpectedOptionEnd,
    ExpectedMap,
    ExpectedMapColon,
    ExpectedMapEnd,
    ExpectedStruct,
    ExpectedStructEnd,
    ExpectedUnit,
    ExpectedStructName,
    ExpectedString,
    ExpectedStringEnd,
    ExpectedIdentifier,

    InvalidEscape(&'static str),

    NoSuchExtension(String),

    UnclosedBlockComment,
    UnderscoreAtBeginning,
    UnexpectedByte(char),

    Utf8Error(Utf8Error),
    TrailingCharacters,

    #[doc(hidden)]
    __NonExhaustive,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Error::IoError(ref s) => write!(f, "{}", s),
            Error::Message(ref s) => write!(f, "{}", s),
            Error::Parser(_, pos) => write!(f, "{}: {}", pos, self.description()),
        }
    }
}

impl de::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error::Message(msg.to_string())
    }
}

impl StdError for Error {
    fn description(&self) -> &str {
        match *self {
            Error::IoError(ref s) => s,
            Error::Message(ref e) => e,
            Error::Parser(ref kind, _) => match *kind {
                ParseError::Base64Error(ref e) => e.description(),
                ParseError::Eof => "Unexpected end of file",
                ParseError::ExpectedArray => "Expected array",
                ParseError::ExpectedArrayEnd => "Expected end of array",
                ParseError::ExpectedAttribute => "Expected an enable attribute",
                ParseError::ExpectedAttributeEnd => {
                    "Expected closing `)` and `]` after the attribute"
                }
                ParseError::ExpectedBoolean => "Expected boolean",
                ParseError::ExpectedComma => "Expected comma",
                ParseError::ExpectedEnum => "Expected enum",
                ParseError::ExpectedChar => "Expected char",
                ParseError::ExpectedFloat => "Expected float",
                ParseError::ExpectedInteger => "Expected integer",
                ParseError::ExpectedOption => "Expected option",
                ParseError::ExpectedOptionEnd => "Expected end of option",
                ParseError::ExpectedMap => "Expected map",
                ParseError::ExpectedMapColon => "Expected colon",
                ParseError::ExpectedMapEnd => "Expected end of map",
                ParseError::ExpectedStruct => "Expected struct",
                ParseError::ExpectedStructEnd => "Expected end of struct",
                ParseError::ExpectedUnit => "Expected unit",
                ParseError::ExpectedStructName => "Expected struct name",
                ParseError::ExpectedString => "Expected string",
                ParseError::ExpectedStringEnd => "Expected string end",
                ParseError::ExpectedIdentifier => "Expected identifier",

                ParseError::InvalidEscape(_) => "Invalid escape sequence",

                ParseError::NoSuchExtension(_) => "No such RON extension",

                ParseError::Utf8Error(ref e) => e.description(),
                ParseError::UnclosedBlockComment => "Unclosed block comment",
                ParseError::UnderscoreAtBeginning => "Found underscore at the beginning",
                ParseError::UnexpectedByte(_) => "Unexpected byte",
                ParseError::TrailingCharacters => "Non-whitespace trailing characters",

                ParseError::__NonExhaustive => unimplemented!(),
            },
        }
    }
}

impl From<Utf8Error> for ParseError {
    fn from(e: Utf8Error) -> Self {
        ParseError::Utf8Error(e)
    }
}

impl From<FromUtf8Error> for ParseError {
    fn from(e: FromUtf8Error) -> Self {
        ParseError::Utf8Error(e.utf8_error())
    }
}

impl From<Utf8Error> for Error {
    fn from(e: Utf8Error) -> Self {
        Error::Parser(ParseError::Utf8Error(e), Position { line: 0, col: 0 })
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Self {
        Error::IoError(e.description().to_string())
    }
}
