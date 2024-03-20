//! Error handling for application manifest embedding.

use std::fmt::{self, Display, Formatter};
use std::io::{self, ErrorKind};

/// The error type which is returned when application manifest embedding fails.
#[derive(Debug)]
pub struct Error {
    repr: Repr,
}

#[derive(Debug)]
enum Repr {
    IoError(io::Error),
    UnknownTarget,
}

impl Error {
    pub(crate) fn unknown_target() -> Error {
        Error {
            repr: Repr::UnknownTarget,
        }
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        match self.repr {
            Repr::IoError(ref e) => write!(f, "I/O error: {}", e),
            Repr::UnknownTarget => f.write_str("unknown target"),
        }
    }
}

impl std::error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self.repr {
            Repr::IoError(ref e) => Some(e),
            _ => None,
        }
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Self {
        Error { repr: Repr::IoError(e) }
    }
}

impl From<Error> for io::Error {
    fn from(e: Error) -> Self {
        match e.repr {
            Repr::IoError(ioe) => ioe,
            _ => io::Error::new(ErrorKind::Other, e),
        }
    }
}
