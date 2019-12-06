use std::fmt;

/// An error encountered while working with structured data.
#[derive(Debug)]
pub struct Error {
    inner: Inner
}

#[derive(Debug)]
enum Inner {
    #[cfg(feature = "std")]
    Boxed(std_support::BoxedError),
    Msg(&'static str),
    Fmt,
}

impl Error {
    /// Create an error from a message.
    pub fn msg(msg: &'static str) -> Self {
        Error {
            inner: Inner::Msg(msg),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::Inner::*;
        match &self.inner {
            #[cfg(feature = "std")]
            &Boxed(ref err) => err.fmt(f),
            &Msg(ref msg) => msg.fmt(f),
            &Fmt => fmt::Error.fmt(f),
        }
    }
}

impl From<fmt::Error> for Error {
    fn from(_: fmt::Error) -> Self {
        Error {
            inner: Inner::Fmt,
        }
    }
}

impl From<Error> for fmt::Error {
    fn from(_: Error) -> Self {
        fmt::Error
    }
}

#[cfg(feature = "std")]
mod std_support {
    use super::*;
    use std::{error, io};

    pub(super) type BoxedError = Box<error::Error + Send + Sync>;

    impl Error {
        /// Create an error from a standard error type.
        pub fn boxed<E>(err: E) -> Self
        where
            E: Into<BoxedError>,
        {
            Error {
                inner: Inner::Boxed(err.into())
            }
        }
    }

    impl error::Error for Error {
        fn description(&self) -> &str {
            "key values error"
        }
    }

    impl From<io::Error> for Error {
        fn from(err: io::Error) -> Self {
            Error::boxed(err)
        }
    }

    impl From<Error> for io::Error {
        fn from(err: Error) -> Self {
            io::Error::new(io::ErrorKind::Other, err)
        }
    }
}
