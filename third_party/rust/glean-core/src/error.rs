use std::ffi::OsString;
use std::fmt::{self, Display};
use std::io;
use std::result;

use ffi_support::{handle_map::HandleError, ExternError};

use rkv::error::StoreError;

/// A specialized [`Result`] type for this crate's operations.
///
/// This is generally used to avoid writing out [Error] directly and
/// is otherwise a direct mapping to [`Result`].
///
/// [`Result`]: https://doc.rust-lang.org/stable/std/result/enum.Result.html
/// [`Error`]: std.struct.Error.html
pub type Result<T> = result::Result<T, Error>;

/// A list enumerating the categories of errors in this crate.
///
/// [`Error`]: https://doc.rust-lang.org/stable/std/error/trait.Error.html
///
/// This list is intended to grow over time and it is not recommended to
/// exhaustively match against it.
#[derive(Debug)]
pub enum ErrorKind {
    /// Lifetime conversion failed
    Lifetime(i32),

    /// FFI-Support error
    Handle(HandleError),

    /// IO error
    IoError(io::Error),

    /// IO error
    Rkv(StoreError),

    /// JSON error
    Json(serde_json::error::Error),

    /// TimeUnit conversion failed
    TimeUnit(i32),

    /// MemoryUnit conversion failed
    MemoryUnit(i32),

    /// HistogramType conversion failed
    HistogramType(i32),

    /// OsString conversion failed
    OsString(OsString),

    /// Unknown error
    Utf8Error,

    #[doc(hidden)]
    __NonExhaustive,
}

/// A specialized [`Error`] type for this crate's operations.
///
/// [`Error`]: https://doc.rust-lang.org/stable/std/error/trait.Error.html
#[derive(Debug)]
pub struct Error {
    kind: ErrorKind,
}

impl Error {
    /// Return a new UTF-8 error
    ///
    /// This is exposed in order to expose conversion errors on the FFI layer.
    pub fn utf8_error() -> Error {
        Error {
            kind: ErrorKind::Utf8Error,
        }
    }
}

impl std::error::Error for Error {}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use ErrorKind::*;
        match &self.kind {
            Lifetime(l) => write!(f, "Lifetime conversion from {} failed", l),
            Handle(e) => write!(f, "Invalid handle: {}", e),
            IoError(e) => write!(f, "An I/O error occurred: {}", e),
            Rkv(e) => write!(f, "An Rkv error occurred: {}", e),
            Json(e) => write!(f, "A JSON error occurred: {}", e),
            TimeUnit(t) => write!(f, "TimeUnit conversion from {} failed", t),
            MemoryUnit(m) => write!(f, "MemoryUnit conversion from {} failed", m),
            HistogramType(h) => write!(f, "HistogramType conversion from {} failed", h),
            OsString(s) => write!(f, "OsString conversion from {:?} failed", s),
            Utf8Error => write!(f, "Invalid  UTF-8 byte sequence in string."),
            __NonExhaustive => write!(f, "Unknown error"),
        }
    }
}

impl From<ErrorKind> for Error {
    fn from(kind: ErrorKind) -> Error {
        Error { kind }
    }
}

impl From<HandleError> for Error {
    fn from(error: HandleError) -> Error {
        Error {
            kind: ErrorKind::Handle(error),
        }
    }
}

impl From<io::Error> for Error {
    fn from(error: io::Error) -> Error {
        Error {
            kind: ErrorKind::IoError(error),
        }
    }
}

impl From<StoreError> for Error {
    fn from(error: StoreError) -> Error {
        Error {
            kind: ErrorKind::Rkv(error),
        }
    }
}

impl From<Error> for ExternError {
    fn from(error: Error) -> ExternError {
        ffi_support::ExternError::new_error(ffi_support::ErrorCode::new(42), format!("{}", error))
    }
}

impl From<serde_json::error::Error> for Error {
    fn from(error: serde_json::error::Error) -> Error {
        Error {
            kind: ErrorKind::Json(error),
        }
    }
}

impl From<OsString> for Error {
    fn from(error: OsString) -> Error {
        Error {
            kind: ErrorKind::OsString(error),
        }
    }
}

/// To satisfy integer conversion done by the macros on the FFI side, we need to be able to turn
/// something infallible into an error.
/// This will never actually be reached, as an integer-to-integer conversion is infallible.
impl From<std::convert::Infallible> for Error {
    fn from(_: std::convert::Infallible) -> Error {
        unreachable!()
    }
}
