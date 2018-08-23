use codec::{SendError, UserError};
use proto;

use std::{error, fmt, io};

pub use frame::Reason;

/// Represents HTTP/2.0 operation errors.
///
/// `Error` covers error cases raised by protocol errors caused by the
/// peer, I/O (transport) errors, and errors caused by the user of the library.
///
/// If the error was caused by the remote peer, then it will contain a
/// [`Reason`] which can be obtained with the [`reason`] function.
///
/// [`Reason`]: struct.Reason.html
/// [`reason`]: #method.reason
#[derive(Debug)]
pub struct Error {
    kind: Kind,
}

#[derive(Debug)]
enum Kind {
    /// An error caused by an action taken by the remote peer.
    ///
    /// This is either an error received by the peer or caused by an invalid
    /// action taken by the peer (i.e. a protocol error).
    Proto(Reason),

    /// An error resulting from an invalid action taken by the user of this
    /// library.
    User(UserError),

    /// An `io::Error` occurred while trying to read or write.
    Io(io::Error),
}

// ===== impl Error =====

impl Error {
    /// If the error was caused by the remote peer, the error reason.
    ///
    /// This is either an error received by the peer or caused by an invalid
    /// action taken by the peer (i.e. a protocol error).
    pub fn reason(&self) -> Option<Reason> {
        match self.kind {
            Kind::Proto(reason) => Some(reason),
            _ => None,
        }
    }
}

impl From<proto::Error> for Error {
    fn from(src: proto::Error) -> Error {
        use proto::Error::*;

        Error {
            kind: match src {
                Proto(reason) => Kind::Proto(reason),
                Io(e) => Kind::Io(e),
            },
        }
    }
}

impl From<io::Error> for Error {
    fn from(src: io::Error) -> Error {
        Error {
            kind: Kind::Io(src),
        }
    }
}

impl From<Reason> for Error {
    fn from(src: Reason) -> Error {
        Error {
            kind: Kind::Proto(src),
        }
    }
}

impl From<SendError> for Error {
    fn from(src: SendError) -> Error {
        match src {
            SendError::User(e) => e.into(),
            SendError::Connection(reason) => reason.into(),
            SendError::Io(e) => e.into(),
        }
    }
}

impl From<UserError> for Error {
    fn from(src: UserError) -> Error {
        Error {
            kind: Kind::User(src),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        use self::Kind::*;

        match self.kind {
            Proto(ref reason) => write!(fmt, "protocol error: {}", reason),
            User(ref e) => write!(fmt, "user error: {}", e),
            Io(ref e) => fmt::Display::fmt(e, fmt),
        }
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        use self::Kind::*;

        match self.kind {
            Io(ref e) => error::Error::description(e),
            Proto(ref reason) => reason.description(),
            User(ref user) => user.description(),
        }
    }
}
