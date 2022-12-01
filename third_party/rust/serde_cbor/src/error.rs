//! When serializing or deserializing CBOR goes wrong.
use core::fmt;
use core::result;
use serde::de;
use serde::ser;
#[cfg(feature = "std")]
use std::error;
#[cfg(feature = "std")]
use std::io;

/// This type represents all possible errors that can occur when serializing or deserializing CBOR
/// data.
pub struct Error(ErrorImpl);

/// Alias for a `Result` with the error type `serde_cbor::Error`.
pub type Result<T> = result::Result<T, Error>;

/// Categorizes the cause of a `serde_cbor::Error`.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Category {
    /// The error was caused by a failure to read or write bytes on an IO stream.
    Io,
    /// The error was caused by input that was not syntactically valid CBOR.
    Syntax,
    /// The error was caused by input data that was semantically incorrect.
    Data,
    /// The error was caused by prematurely reaching the end of the input data.
    Eof,
}

impl Error {
    /// The byte offset at which the error occurred.
    pub fn offset(&self) -> u64 {
        self.0.offset
    }

    pub(crate) fn syntax(code: ErrorCode, offset: u64) -> Error {
        Error(ErrorImpl { code, offset })
    }

    #[cfg(feature = "std")]
    pub(crate) fn io(error: io::Error) -> Error {
        Error(ErrorImpl {
            code: ErrorCode::Io(error),
            offset: 0,
        })
    }

    #[cfg(all(not(feature = "std"), feature = "unsealed_read_write"))]
    /// Creates an error signalling that the underlying `Read` encountered an I/O error.
    pub fn io() -> Error {
        Error(ErrorImpl {
            code: ErrorCode::Io,
            offset: 0,
        })
    }

    #[cfg(feature = "unsealed_read_write")]
    /// Creates an error signalling that the scratch buffer was too small to fit the data.
    pub fn scratch_too_small(offset: u64) -> Error {
        Error(ErrorImpl {
            code: ErrorCode::ScratchTooSmall,
            offset,
        })
    }

    #[cfg(not(feature = "unsealed_read_write"))]
    pub(crate) fn scratch_too_small(offset: u64) -> Error {
        Error(ErrorImpl {
            code: ErrorCode::ScratchTooSmall,
            offset,
        })
    }

    #[cfg(feature = "unsealed_read_write")]
    /// Creates an error with a custom message.
    ///
    /// **Note**: When the "std" feature is disabled, the message will be discarded.
    pub fn message<T: fmt::Display>(_msg: T) -> Error {
        #[cfg(not(feature = "std"))]
        {
            Error(ErrorImpl {
                code: ErrorCode::Message,
                offset: 0,
            })
        }
        #[cfg(feature = "std")]
        {
            Error(ErrorImpl {
                code: ErrorCode::Message(_msg.to_string()),
                offset: 0,
            })
        }
    }

    #[cfg(not(feature = "unsealed_read_write"))]
    pub(crate) fn message<T: fmt::Display>(_msg: T) -> Error {
        #[cfg(not(feature = "std"))]
        {
            Error(ErrorImpl {
                code: ErrorCode::Message,
                offset: 0,
            })
        }
        #[cfg(feature = "std")]
        {
            Error(ErrorImpl {
                code: ErrorCode::Message(_msg.to_string()),
                offset: 0,
            })
        }
    }

    #[cfg(feature = "unsealed_read_write")]
    /// Creates an error signalling that the underlying read
    /// encountered an end of input.
    pub fn eof(offset: u64) -> Error {
        Error(ErrorImpl {
            code: ErrorCode::EofWhileParsingValue,
            offset,
        })
    }

    /// Categorizes the cause of this error.
    pub fn classify(&self) -> Category {
        match self.0.code {
            #[cfg(feature = "std")]
            ErrorCode::Message(_) => Category::Data,
            #[cfg(not(feature = "std"))]
            ErrorCode::Message => Category::Data,
            #[cfg(feature = "std")]
            ErrorCode::Io(_) => Category::Io,
            #[cfg(not(feature = "std"))]
            ErrorCode::Io => Category::Io,
            ErrorCode::ScratchTooSmall => Category::Io,
            ErrorCode::EofWhileParsingValue
            | ErrorCode::EofWhileParsingArray
            | ErrorCode::EofWhileParsingMap => Category::Eof,
            ErrorCode::LengthOutOfRange
            | ErrorCode::InvalidUtf8
            | ErrorCode::UnassignedCode
            | ErrorCode::UnexpectedCode
            | ErrorCode::TrailingData
            | ErrorCode::ArrayTooShort
            | ErrorCode::ArrayTooLong
            | ErrorCode::RecursionLimitExceeded
            | ErrorCode::WrongEnumFormat
            | ErrorCode::WrongStructFormat => Category::Syntax,
        }
    }

    /// Returns true if this error was caused by a failure to read or write bytes on an IO stream.
    pub fn is_io(&self) -> bool {
        match self.classify() {
            Category::Io => true,
            _ => false,
        }
    }

    /// Returns true if this error was caused by input that was not syntactically valid CBOR.
    pub fn is_syntax(&self) -> bool {
        match self.classify() {
            Category::Syntax => true,
            _ => false,
        }
    }

    /// Returns true if this error was caused by data that was semantically incorrect.
    pub fn is_data(&self) -> bool {
        match self.classify() {
            Category::Data => true,
            _ => false,
        }
    }

    /// Returns true if this error was caused by prematurely reaching the end of the input data.
    pub fn is_eof(&self) -> bool {
        match self.classify() {
            Category::Eof => true,
            _ => false,
        }
    }

    /// Returns true if this error was caused by the scratch buffer being too small.
    ///
    /// Note this being `true` implies that `is_io()` is also `true`.
    pub fn is_scratch_too_small(&self) -> bool {
        match self.0.code {
            ErrorCode::ScratchTooSmall => true,
            _ => false,
        }
    }
}

#[cfg(feature = "std")]
impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self.0.code {
            ErrorCode::Io(ref err) => Some(err),
            _ => None,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.0.offset == 0 {
            fmt::Display::fmt(&self.0.code, f)
        } else {
            write!(f, "{} at offset {}", self.0.code, self.0.offset)
        }
    }
}

impl fmt::Debug for Error {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.0, fmt)
    }
}

impl de::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Error {
        Error::message(msg)
    }

    fn invalid_type(unexp: de::Unexpected<'_>, exp: &dyn de::Expected) -> Error {
        if let de::Unexpected::Unit = unexp {
            Error::custom(format_args!("invalid type: null, expected {}", exp))
        } else {
            Error::custom(format_args!("invalid type: {}, expected {}", unexp, exp))
        }
    }
}

impl ser::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Error {
        Error::message(msg)
    }
}

#[cfg(feature = "std")]
impl From<io::Error> for Error {
    fn from(e: io::Error) -> Error {
        Error::io(e)
    }
}

#[cfg(not(feature = "std"))]
impl From<core::fmt::Error> for Error {
    fn from(_: core::fmt::Error) -> Error {
        Error(ErrorImpl {
            code: ErrorCode::Message,
            offset: 0,
        })
    }
}

#[derive(Debug)]
struct ErrorImpl {
    code: ErrorCode,
    offset: u64,
}

#[derive(Debug)]
pub(crate) enum ErrorCode {
    #[cfg(feature = "std")]
    Message(String),
    #[cfg(not(feature = "std"))]
    Message,
    #[cfg(feature = "std")]
    Io(io::Error),
    #[allow(unused)]
    #[cfg(not(feature = "std"))]
    Io,
    ScratchTooSmall,
    EofWhileParsingValue,
    EofWhileParsingArray,
    EofWhileParsingMap,
    LengthOutOfRange,
    InvalidUtf8,
    UnassignedCode,
    UnexpectedCode,
    TrailingData,
    ArrayTooShort,
    ArrayTooLong,
    RecursionLimitExceeded,
    WrongEnumFormat,
    WrongStructFormat,
}

impl fmt::Display for ErrorCode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            #[cfg(feature = "std")]
            ErrorCode::Message(ref msg) => f.write_str(msg),
            #[cfg(not(feature = "std"))]
            ErrorCode::Message => f.write_str("Unknown error"),
            #[cfg(feature = "std")]
            ErrorCode::Io(ref err) => fmt::Display::fmt(err, f),
            #[cfg(not(feature = "std"))]
            ErrorCode::Io => f.write_str("Unknown I/O error"),
            ErrorCode::ScratchTooSmall => f.write_str("Scratch buffer too small"),
            ErrorCode::EofWhileParsingValue => f.write_str("EOF while parsing a value"),
            ErrorCode::EofWhileParsingArray => f.write_str("EOF while parsing an array"),
            ErrorCode::EofWhileParsingMap => f.write_str("EOF while parsing a map"),
            ErrorCode::LengthOutOfRange => f.write_str("length out of range"),
            ErrorCode::InvalidUtf8 => f.write_str("invalid UTF-8"),
            ErrorCode::UnassignedCode => f.write_str("unassigned type"),
            ErrorCode::UnexpectedCode => f.write_str("unexpected code"),
            ErrorCode::TrailingData => f.write_str("trailing data"),
            ErrorCode::ArrayTooShort => f.write_str("array too short"),
            ErrorCode::ArrayTooLong => f.write_str("array too long"),
            ErrorCode::RecursionLimitExceeded => f.write_str("recursion limit exceeded"),
            ErrorCode::WrongEnumFormat => f.write_str("wrong enum format"),
            ErrorCode::WrongStructFormat => f.write_str("wrong struct format"),
        }
    }
}
