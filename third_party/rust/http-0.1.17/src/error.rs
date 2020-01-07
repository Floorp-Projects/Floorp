use std::error;
use std::fmt;
use std::result;

use header;
use method;
use status;
use uri;

/// A generic "error" for HTTP connections
///
/// This error type is less specific than the error returned from other
/// functions in this crate, but all other errors can be converted to this
/// error. Consumers of this crate can typically consume and work with this form
/// of error for conversions with the `?` operator.
#[derive(Debug)]
pub struct Error {
    inner: ErrorKind,
}

/// A `Result` typedef to use with the `http::Error` type
pub type Result<T> = result::Result<T, Error>;

#[derive(Debug)]
enum ErrorKind {
    StatusCode(status::InvalidStatusCode),
    Method(method::InvalidMethod),
    Uri(uri::InvalidUri),
    UriShared(uri::InvalidUriBytes),
    UriParts(uri::InvalidUriParts),
    HeaderName(header::InvalidHeaderName),
    HeaderNameShared(header::InvalidHeaderNameBytes),
    HeaderValue(header::InvalidHeaderValue),
    HeaderValueShared(header::InvalidHeaderValueBytes),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        error::Error::description(self).fmt(f)
    }
}

impl Error {
    /// Return true if the underlying error has the same type as T.
    pub fn is<T: error::Error + 'static>(&self) -> bool {
        self.get_ref().is::<T>()
    }

    /// Return a reference to the lower level, inner error.
    pub fn get_ref(&self) -> &(error::Error + 'static) {
        use self::ErrorKind::*;

        match self.inner {
            StatusCode(ref e) => e,
            Method(ref e) => e,
            Uri(ref e) => e,
            UriShared(ref e) => e,
            UriParts(ref e) => e,
            HeaderName(ref e) => e,
            HeaderNameShared(ref e) => e,
            HeaderValue(ref e) => e,
            HeaderValueShared(ref e) => e,
        }
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        use self::ErrorKind::*;

        match self.inner {
            StatusCode(ref e) => e.description(),
            Method(ref e) => e.description(),
            Uri(ref e) => e.description(),
            UriShared(ref e) => e.description(),
            UriParts(ref e) => e.description(),
            HeaderName(ref e) => e.description(),
            HeaderNameShared(ref e) => e.description(),
            HeaderValue(ref e) => e.description(),
            HeaderValueShared(ref e) => e.description(),
        }
    }

    // Return any available cause from the inner error. Note the inner error is
    // not itself the cause.
    #[allow(deprecated)]
    fn cause(&self) -> Option<&error::Error> {
        self.get_ref().cause()
    }
}

impl From<status::InvalidStatusCode> for Error {
    fn from(err: status::InvalidStatusCode) -> Error {
        Error { inner: ErrorKind::StatusCode(err) }
    }
}

impl From<method::InvalidMethod> for Error {
    fn from(err: method::InvalidMethod) -> Error {
        Error { inner: ErrorKind::Method(err) }
    }
}

impl From<uri::InvalidUri> for Error {
    fn from(err: uri::InvalidUri) -> Error {
        Error { inner: ErrorKind::Uri(err) }
    }
}

impl From<uri::InvalidUriBytes> for Error {
    fn from(err: uri::InvalidUriBytes) -> Error {
        Error { inner: ErrorKind::UriShared(err) }
    }
}

impl From<uri::InvalidUriParts> for Error {
    fn from(err: uri::InvalidUriParts) -> Error {
        Error { inner: ErrorKind::UriParts(err) }
    }
}

impl From<header::InvalidHeaderName> for Error {
    fn from(err: header::InvalidHeaderName) -> Error {
        Error { inner: ErrorKind::HeaderName(err) }
    }
}

impl From<header::InvalidHeaderNameBytes> for Error {
    fn from(err: header::InvalidHeaderNameBytes) -> Error {
        Error { inner: ErrorKind::HeaderNameShared(err) }
    }
}

impl From<header::InvalidHeaderValue> for Error {
    fn from(err: header::InvalidHeaderValue) -> Error {
        Error { inner: ErrorKind::HeaderValue(err) }
    }
}

impl From<header::InvalidHeaderValueBytes> for Error {
    fn from(err: header::InvalidHeaderValueBytes) -> Error {
        Error { inner: ErrorKind::HeaderValueShared(err) }
    }
}

// A crate-private type until we can use !.
//
// Being crate-private, we should be able to swap the type out in a
// backwards compatible way.
pub enum Never {}

impl From<Never> for Error {
    fn from(never: Never) -> Error {
        match never {}
    }
}

impl fmt::Debug for Never {
    fn fmt(&self, _f: &mut fmt::Formatter) -> fmt::Result {
        match *self {}
    }
}

impl fmt::Display for Never {
    fn fmt(&self, _f: &mut fmt::Formatter) -> fmt::Result {
        match *self {}
    }
}

impl error::Error for Never {
    fn description(&self) -> &str {
        match *self {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn inner_error_is_invalid_status_code() {
        if let Err(e) = status::StatusCode::from_u16(6666) {
            let err: Error = e.into();
            let ie = err.get_ref();
            assert!(!ie.is::<header::InvalidHeaderValue>());
            assert!( ie.is::<status::InvalidStatusCode>());
            ie.downcast_ref::<status::InvalidStatusCode>().unwrap();

            assert!(!err.is::<header::InvalidHeaderValue>());
            assert!( err.is::<status::InvalidStatusCode>());
        } else {
            panic!("Bad status allowed!");
        }
    }
}
