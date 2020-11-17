#[cfg(feature = "failure")]
use failure::{Backtrace, Context, Fail};
use std;
use std::fmt::{self, Display};

#[derive(Debug)]
pub struct Error {
    #[cfg(feature = "failure")]
    inner: Context<ErrorKind>,
    #[cfg(not(feature = "failure"))]
    inner: ErrorKind,
}

// To suppress false positives from cargo-clippy
#[cfg_attr(feature = "cargo-clippy", allow(empty_line_after_outer_attr))]
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum ErrorKind {
    BadAbsolutePath,
    BadRelativePath,
    CannotFindBinaryPath,
    CannotGetCurrentDir,
    CannotCanonicalize,
}

#[cfg(feature = "failure")]
impl Fail for ErrorKind {}

impl Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let display = match *self {
            ErrorKind::BadAbsolutePath => "Bad absolute path",
            ErrorKind::BadRelativePath => "Bad relative path",
            ErrorKind::CannotFindBinaryPath => "Cannot find binary path",
            ErrorKind::CannotGetCurrentDir => "Cannot get current directory",
            ErrorKind::CannotCanonicalize => "Cannot canonicalize path",
        };
        f.write_str(display)
    }
}

#[cfg(feature = "failure")]
impl Fail for Error {
    fn cause(&self) -> Option<&dyn Fail> {
        self.inner.cause()
    }

    fn backtrace(&self) -> Option<&Backtrace> {
        self.inner.backtrace()
    }
}

#[cfg(not(feature = "failure"))]
impl std::error::Error for Error {}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        Display::fmt(&self.inner, f)
    }
}

impl Error {
    pub fn kind(&self) -> ErrorKind {
        #[cfg(feature = "failure")]
        {
            *self.inner.get_context()
        }
        #[cfg(not(feature = "failure"))]
        {
            self.inner
        }
    }
}

impl From<ErrorKind> for Error {
    fn from(kind: ErrorKind) -> Error {
        Error {
            #[cfg(feature = "failure")]
            inner: Context::new(kind),
            #[cfg(not(feature = "failure"))]
            inner: kind,
        }
    }
}

#[cfg(feature = "failure")]
impl From<Context<ErrorKind>> for Error {
    fn from(inner: Context<ErrorKind>) -> Error {
        Error { inner }
    }
}

pub type Result<T> = std::result::Result<T, Error>;
