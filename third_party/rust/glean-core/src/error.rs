use std::ffi::OsString;
use std::fmt::{self, Display};
use std::io;
use std::result;

use failure::{self, Backtrace, Context, Fail};

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
/// This list is intended to grow over time and it is not recommended to
/// exhaustively match against it.
///
/// It is used with the [`Error`] struct.
///
/// [`Error`]: std.struct.Error.html
#[derive(Debug, Fail)]
pub enum ErrorKind {
    /// Lifetime conversion failed
    #[fail(display = "Lifetime conversion from {} failed", _0)]
    Lifetime(i32),

    /// FFI-Support error
    #[fail(display = "Invalid handle: {}", _0)]
    Handle(HandleError),

    /// IO error
    #[fail(display = "An I/O error occurred: {}", _0)]
    IoError(io::Error),

    /// IO error
    #[fail(display = "An Rkv error occurred: {}", _0)]
    Rkv(StoreError),

    /// JSON error
    #[fail(display = "A JSON error occurred: {}", _0)]
    Json(serde_json::error::Error),

    /// TimeUnit conversion failed
    #[fail(display = "TimeUnit conversion from {} failed", _0)]
    TimeUnit(i32),

    /// MemoryUnit conversion failed
    #[fail(display = "MemoryUnit conversion from {} failed", _0)]
    MemoryUnit(i32),

    /// HistogramType conversion failed
    #[fail(display = "HistogramType conversion from {} failed", _0)]
    HistogramType(i32),

    /// OsString conversion failed
    #[fail(display = "OsString conversion from {:?} failed", _0)]
    OsString(OsString),

    /// Unknown error
    #[fail(display = "Invalid  UTF-8 byte sequence in string.")]
    Utf8Error,
}

/// A specialized [`Error`] type for this crate's operations.
///
/// [`Error`]: https://doc.rust-lang.org/stable/std/error/trait.Error.html
#[derive(Debug)]
pub struct Error {
    inner: Context<ErrorKind>,
}

impl Error {
    /// Access the [`ErrorKind`] member.
    ///
    /// [`ErrorKind`]: enum.ErrorKind.html
    pub fn kind(&self) -> &ErrorKind {
        &*self.inner.get_context()
    }

    /// Return a new UTF-8 error
    ///
    /// This is exposed in order to expose conversion errors on the FFI layer.
    pub fn utf8_error() -> Error {
        Error {
            inner: Context::new(ErrorKind::Utf8Error),
        }
    }
}

impl Fail for Error {
    fn cause(&self) -> Option<&dyn Fail> {
        self.inner.cause()
    }

    fn backtrace(&self) -> Option<&Backtrace> {
        self.inner.backtrace()
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.inner, f)
    }
}

impl From<ErrorKind> for Error {
    fn from(kind: ErrorKind) -> Error {
        let inner = Context::new(kind);
        Error { inner }
    }
}

impl From<Context<ErrorKind>> for Error {
    fn from(inner: Context<ErrorKind>) -> Error {
        Error { inner }
    }
}

impl From<HandleError> for Error {
    fn from(error: HandleError) -> Error {
        Error {
            inner: Context::new(ErrorKind::Handle(error)),
        }
    }
}

impl From<io::Error> for Error {
    fn from(error: io::Error) -> Error {
        Error {
            inner: Context::new(ErrorKind::IoError(error)),
        }
    }
}

impl From<StoreError> for Error {
    fn from(error: StoreError) -> Error {
        Error {
            inner: Context::new(ErrorKind::Rkv(error)),
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
            inner: Context::new(ErrorKind::Json(error)),
        }
    }
}

impl From<OsString> for Error {
    fn from(error: OsString) -> Error {
        Error {
            inner: Context::new(ErrorKind::OsString(error)),
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
