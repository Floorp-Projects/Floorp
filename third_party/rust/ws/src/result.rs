use std::borrow::Cow;
use std::convert::{From, Into};
use std::error::Error as StdError;
use std::fmt;
use std::io;
use std::result::Result as StdResult;
use std::str::Utf8Error;

use httparse;
use mio;
#[cfg(feature = "ssl")]
use openssl::ssl::{Error as SslError, HandshakeError as SslHandshakeError};
#[cfg(feature = "nativetls")]
use native_tls::{Error as SslError, HandshakeError as SslHandshakeError};
#[cfg(any(feature = "ssl", feature = "nativetls"))]
type HandshakeError = SslHandshakeError<mio::tcp::TcpStream>;

use communication::Command;

pub type Result<T> = StdResult<T, Error>;

/// The type of an error, which may indicate other kinds of errors as the underlying cause.
#[derive(Debug)]
pub enum Kind {
    /// Indicates an internal application error.
    /// If panic_on_internal is true, which is the default, then the application will panic.
    /// Otherwise the WebSocket will automatically attempt to send an Error (1011) close code.
    Internal,
    /// Indicates a state where some size limit has been exceeded, such as an inability to accept
    /// any more new connections.
    /// If a Connection is active, the WebSocket will automatically attempt to send
    /// a Size (1009) close code.
    Capacity,
    /// Indicates a violation of the WebSocket protocol.
    /// The WebSocket will automatically attempt to send a Protocol (1002) close code, or if
    /// this error occurs during a handshake, an HTTP 400 response will be generated.
    Protocol,
    /// Indicates that the WebSocket received data that should be utf8 encoded but was not.
    /// The WebSocket will automatically attempt to send a Invalid Frame Payload Data (1007) close
    /// code.
    Encoding(Utf8Error),
    /// Indicates an underlying IO Error.
    /// This kind of error will result in a WebSocket Connection disconnecting.
    Io(io::Error),
    /// Indicates a failure to parse an HTTP message.
    /// This kind of error should only occur during a WebSocket Handshake, and a HTTP 500 response
    /// will be generated.
    Http(httparse::Error),
    /// Indicates a failure to send a signal on the internal EventLoop channel. This means that
    /// the WebSocket is overloaded. In order to avoid this error, it is important to set
    /// `Settings::max_connections` and `Settings:queue_size` high enough to handle the load.
    /// If encountered, retuning from a handler method and waiting for the EventLoop to consume
    /// the queue may relieve the situation.
    Queue(mio::channel::SendError<Command>),
    /// Indicates a failure to perform SSL encryption.
    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    Ssl(SslError),
    /// Indicates a failure to perform SSL encryption.
    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    SslHandshake(HandshakeError),
    /// A custom error kind for use by applications. This error kind involves extra overhead
    /// because it will allocate the memory on the heap. The WebSocket ignores such errors by
    /// default, simply passing them to the Connection Handler.
    Custom(Box<StdError + Send + Sync>),
}

/// A struct indicating the kind of error that has occurred and any precise details of that error.
pub struct Error {
    pub kind: Kind,
    pub details: Cow<'static, str>,
}

impl Error {
    pub fn new<I>(kind: Kind, details: I) -> Error
    where
        I: Into<Cow<'static, str>>,
    {
        Error {
            kind,
            details: details.into(),
        }
    }

    pub fn into_box(self) -> Box<StdError> {
        match self.kind {
            Kind::Custom(err) => err,
            _ => Box::new(self),
        }
    }
}

impl fmt::Debug for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.details.len() > 0 {
            write!(f, "WS Error <{:?}>: {}", self.kind, self.details)
        } else {
            write!(f, "WS Error <{:?}>", self.kind)
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.details.len() > 0 {
            write!(f, "{}: {}", self.description(), self.details)
        } else {
            write!(f, "{}", self.description())
        }
    }
}

impl StdError for Error {
    fn description(&self) -> &str {
        match self.kind {
            Kind::Internal => "Internal Application Error",
            Kind::Capacity => "WebSocket at Capacity",
            Kind::Protocol => "WebSocket Protocol Error",
            Kind::Encoding(ref err) => err.description(),
            Kind::Io(ref err) => err.description(),
            Kind::Http(_) => "Unable to parse HTTP",
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Kind::Ssl(ref err) => err.description(),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Kind::SslHandshake(ref err) => err.description(),
            Kind::Queue(_) => "Unable to send signal on event loop",
            Kind::Custom(ref err) => err.description(),
        }
    }

    fn cause(&self) -> Option<&StdError> {
        match self.kind {
            Kind::Encoding(ref err) => Some(err),
            Kind::Io(ref err) => Some(err),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Kind::Ssl(ref err) => Some(err),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Kind::SslHandshake(ref err) => err.cause(),
            Kind::Custom(ref err) => Some(err.as_ref()),
            _ => None,
        }
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Error {
        Error::new(Kind::Io(err), "")
    }
}

impl From<httparse::Error> for Error {
    fn from(err: httparse::Error) -> Error {
        let details = match err {
            httparse::Error::HeaderName => "Invalid byte in header name.",
            httparse::Error::HeaderValue => "Invalid byte in header value.",
            httparse::Error::NewLine => "Invalid byte in new line.",
            httparse::Error::Status => "Invalid byte in Response status.",
            httparse::Error::Token => "Invalid byte where token is required.",
            httparse::Error::TooManyHeaders => {
                "Parsed more headers than provided buffer can contain."
            }
            httparse::Error::Version => "Invalid byte in HTTP version.",
        };

        Error::new(Kind::Http(err), details)
    }
}

impl From<mio::channel::SendError<Command>> for Error {
    fn from(err: mio::channel::SendError<Command>) -> Error {
        match err {
            mio::channel::SendError::Io(err) => Error::from(err),
            _ => Error::new(Kind::Queue(err), ""),
        }
    }
}

impl From<Utf8Error> for Error {
    fn from(err: Utf8Error) -> Error {
        Error::new(Kind::Encoding(err), "")
    }
}

#[cfg(any(feature = "ssl", feature = "nativetls"))]
impl From<SslError> for Error {
    fn from(err: SslError) -> Error {
        Error::new(Kind::Ssl(err), "")
    }
}

#[cfg(any(feature = "ssl", feature = "nativetls"))]
impl From<HandshakeError> for Error {
    fn from(err: HandshakeError) -> Error {
        Error::new(Kind::SslHandshake(err), "")
    }
}

impl<B> From<Box<B>> for Error
where
    B: StdError + Send + Sync + 'static,
{
    fn from(err: Box<B>) -> Error {
        Error::new(Kind::Custom(err), "")
    }
}
