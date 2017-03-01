//! When serializing or deserializing JSON goes wrong.

use std::error;
use std::fmt::{self, Debug, Display};
use std::io;
use std::result;

use serde::de;
use serde::ser;

/// This type represents all possible errors that can occur when serializing or
/// deserializing JSON data.
pub struct Error {
    err: Box<ErrorImpl>,
}

/// Alias for a `Result` with the error type `serde_json::Error`.
pub type Result<T> = result::Result<T, Error>;

enum ErrorImpl {
    /// The JSON value had some syntatic error.
    Syntax(ErrorCode, usize, usize),

    /// Some IO error occurred when serializing or deserializing a value.
    Io(io::Error),
}

// Not public API. Should be pub(crate).
#[doc(hidden)]
#[derive(Clone, PartialEq, Debug)]
pub enum ErrorCode {
    /// Catchall for syntax error messages
    Message(String),

    /// EOF while parsing a list.
    EOFWhileParsingList,

    /// EOF while parsing an object.
    EOFWhileParsingObject,

    /// EOF while parsing a string.
    EOFWhileParsingString,

    /// EOF while parsing a JSON value.
    EOFWhileParsingValue,

    /// Expected this character to be a `':'`.
    ExpectedColon,

    /// Expected this character to be either a `','` or a `]`.
    ExpectedListCommaOrEnd,

    /// Expected this character to be either a `','` or a `}`.
    ExpectedObjectCommaOrEnd,

    /// Expected to parse either a `true`, `false`, or a `null`.
    ExpectedSomeIdent,

    /// Expected this character to start a JSON value.
    ExpectedSomeValue,

    /// Expected this character to start a JSON string.
    ExpectedSomeString,

    /// Invalid hex escape code.
    InvalidEscape,

    /// Invalid number.
    InvalidNumber,

    /// Number is bigger than the maximum value of its type.
    NumberOutOfRange,

    /// Invalid unicode code point.
    InvalidUnicodeCodePoint,

    /// Object key is not a string.
    KeyMustBeAString,

    /// Lone leading surrogate in hex escape.
    LoneLeadingSurrogateInHexEscape,

    /// JSON has non-whitespace trailing characters after the value.
    TrailingCharacters,

    /// Unexpected end of hex excape.
    UnexpectedEndOfHexEscape,

    /// Encountered nesting of JSON maps and arrays more than 128 layers deep.
    RecursionLimitExceeded,
}

impl Error {
    // Not public API. Should be pub(crate).
    #[doc(hidden)]
    pub fn syntax(code: ErrorCode, line: usize, col: usize) -> Self {
        Error {
            err: Box::new(ErrorImpl::Syntax(code, line, col)),
        }
    }

    // Not public API. Should be pub(crate).
    #[doc(hidden)]
    pub fn fix_position<F>(self, f: F) -> Self
        where F: FnOnce(ErrorCode) -> Error
    {
        if let ErrorImpl::Syntax(code, 0, 0) = *self.err {
            f(code)
        } else {
            self
        }
    }
}

impl Display for ErrorCode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ErrorCode::Message(ref msg) => write!(f, "{}", msg),
            ErrorCode::EOFWhileParsingList => {
                f.write_str("EOF while parsing a list")
            }
            ErrorCode::EOFWhileParsingObject => {
                f.write_str("EOF while parsing an object")
            }
            ErrorCode::EOFWhileParsingString => {
                f.write_str("EOF while parsing a string")
            }
            ErrorCode::EOFWhileParsingValue => {
                f.write_str("EOF while parsing a value")
            }
            ErrorCode::ExpectedColon => {
                f.write_str("expected `:`")
            }
            ErrorCode::ExpectedListCommaOrEnd => {
                f.write_str("expected `,` or `]`")
            }
            ErrorCode::ExpectedObjectCommaOrEnd => {
                f.write_str("expected `,` or `}`")
            }
            ErrorCode::ExpectedSomeIdent => {
                f.write_str("expected ident")
            }
            ErrorCode::ExpectedSomeValue => {
                f.write_str("expected value")
            }
            ErrorCode::ExpectedSomeString => {
                f.write_str("expected string")
            }
            ErrorCode::InvalidEscape => {
                f.write_str("invalid escape")
            }
            ErrorCode::InvalidNumber => {
                f.write_str("invalid number")
            }
            ErrorCode::NumberOutOfRange => {
                f.write_str("number out of range")
            }
            ErrorCode::InvalidUnicodeCodePoint => {
                f.write_str("invalid unicode code point")
            }
            ErrorCode::KeyMustBeAString => {
                f.write_str("key must be a string")
            }
            ErrorCode::LoneLeadingSurrogateInHexEscape => {
                f.write_str("lone leading surrogate in hex escape")
            }
            ErrorCode::TrailingCharacters => {
                f.write_str("trailing characters")
            }
            ErrorCode::UnexpectedEndOfHexEscape => {
                f.write_str("unexpected end of hex escape")
            }
            ErrorCode::RecursionLimitExceeded => {
                f.write_str("recursion limit exceeded")
            }
        }
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        match *self.err {
            ErrorImpl::Syntax(..) => {
                // If you want a better message, use Display::fmt or to_string().
                "JSON error"
            }
            ErrorImpl::Io(ref error) => error::Error::description(error),
        }
    }

    fn cause(&self) -> Option<&error::Error> {
        match *self.err {
            ErrorImpl::Io(ref error) => Some(error),
            _ => None,
        }
    }
}

impl Display for Error {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self.err {
            ErrorImpl::Syntax(ref code, line, col) => {
                if line == 0 && col == 0 {
                    write!(fmt, "{}", code)
                } else {
                    write!(fmt, "{} at line {} column {}", code, line, col)
                }
            }
            ErrorImpl::Io(ref error) => fmt::Display::fmt(error, fmt),
        }
    }
}

// Remove two layers of verbosity from the debug representation. Humans often
// end up seeing this representation because it is what unwrap() shows.
impl Debug for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        match *self.err {
            ErrorImpl::Syntax(ref code, ref line, ref col) => {
                formatter.debug_tuple("Syntax")
                    .field(code)
                    .field(line)
                    .field(col)
                    .finish()
            }
            ErrorImpl::Io(ref io) => {
                formatter.debug_tuple("Io")
                    .field(io)
                    .finish()
            }
        }
    }
}

impl From<ErrorImpl> for Error {
    fn from(error: ErrorImpl)  -> Error {
        Error {
            err: Box::new(error),
        }
    }
}

impl From<io::Error> for Error {
    fn from(error: io::Error) -> Error {
        Error {
            err: Box::new(ErrorImpl::Io(error)),
        }
    }
}

impl From<de::value::Error> for Error {
    fn from(error: de::value::Error) -> Error {
        Error {
            err: Box::new(ErrorImpl::Syntax(ErrorCode::Message(error.to_string()), 0, 0)),
        }
    }
}

impl de::Error for Error {
    fn custom<T: Display>(msg: T) -> Error {
        Error {
            err: Box::new(ErrorImpl::Syntax(ErrorCode::Message(msg.to_string()), 0, 0)),
        }
    }
}

impl ser::Error for Error {
    fn custom<T: Display>(msg: T) -> Error {
        Error {
            err: Box::new(ErrorImpl::Syntax(ErrorCode::Message(msg.to_string()), 0, 0)),
        }
    }
}
