/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{error, fmt, result, str::Utf8Error, sync::PoisonError};

use nserror::{
    nsresult, NS_ERROR_ALREADY_INITIALIZED, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG,
    NS_ERROR_NOT_AVAILABLE, NS_ERROR_UNEXPECTED,
};

pub type Result<T> = result::Result<T, Error>;

#[derive(Debug)]
pub enum Error {
    /// A wrapped FxA error.
    FxAError(fxa_client::Error),

    /// A wrapped XPCOM error.
    Nsresult(nsresult),

    /// A punt already ran.
    AlreadyRan(&'static str),

    /// A punt didn't run on the background task queue.
    DidNotRun(&'static str),

    /// A punt was already torn down when used.
    AlreadyTornDown,

    /// A Gecko string couldn't be converted to UTF-8.
    MalformedString(Box<dyn error::Error + Send + Sync + 'static>),

    /// The FxA object has already been initialized.
    AlreadyInitialized,

    /// The FxA object has not been initialized before use.
    Unavailable,

    /// Failure acquiring a lock on the FxA object in the background thread.
    PoisonError,
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::MalformedString(error) => Some(error.as_ref()),
            _ => None,
        }
    }
}

impl From<Error> for nsresult {
    fn from(error: Error) -> nsresult {
        match error {
            Error::FxAError(_) => NS_ERROR_FAILURE,
            Error::AlreadyRan(_)
            | Error::DidNotRun(_)
            | Error::AlreadyTornDown
            | Error::PoisonError => NS_ERROR_UNEXPECTED,
            Error::Nsresult(result) => result,
            Error::MalformedString(_) => NS_ERROR_INVALID_ARG,
            Error::AlreadyInitialized => NS_ERROR_ALREADY_INITIALIZED,
            Error::Unavailable => NS_ERROR_NOT_AVAILABLE,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::FxAError(error) => error.fmt(f),
            Error::Nsresult(result) => write!(f, "Operation failed with {}", result.error_name()),
            Error::AlreadyRan(what) => write!(f, "`{}` already ran on the background thread", what),
            Error::DidNotRun(what) => write!(f, "Failed to run `{}` on background thread", what),
            Error::AlreadyTornDown => {
                write!(f, "Can't use a storage area that's already torn down")
            }
            Error::MalformedString(error) => error.fmt(f),
            Error::AlreadyInitialized => f.write_str("The resource has already been initialized"),
            Error::Unavailable => f.write_str("A resource for this operation is unavailable"),
            Error::PoisonError => f.write_str("Error getting read/write lock"),
        }
    }
}

impl From<nsresult> for Error {
    fn from(result: nsresult) -> Error {
        Error::Nsresult(result)
    }
}

impl From<Utf8Error> for Error {
    fn from(error: Utf8Error) -> Error {
        Error::MalformedString(error.into())
    }
}

impl<T> From<PoisonError<T>> for Error {
    fn from(_error: PoisonError<T>) -> Error {
        Error::PoisonError
    }
}

impl From<fxa_client::error::Error> for Error {
    fn from(result: fxa_client::error::Error) -> Error {
        Error::FxAError(result)
    }
}
