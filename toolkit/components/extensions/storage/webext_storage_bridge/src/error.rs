/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{error, fmt, result, str::Utf8Error, string::FromUtf16Error};

use golden_gate::Error as GoldenGateError;
use nserror::{
    nsresult, NS_ERROR_ALREADY_INITIALIZED, NS_ERROR_CANNOT_CONVERT_DATA,
    NS_ERROR_DOM_QUOTA_EXCEEDED_ERR, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG,
    NS_ERROR_NOT_IMPLEMENTED, NS_ERROR_NOT_INITIALIZED, NS_ERROR_UNEXPECTED,
};
use serde_json::error::Error as JsonError;
use webext_storage::error::Error as WebextStorageError;

/// A specialized `Result` type for extension storage operations.
pub type Result<T> = result::Result<T, Error>;

/// The error type for extension storage operations. Errors can be converted
/// into `nsresult` codes, and include more detailed messages that can be passed
/// to callbacks.
#[derive(Debug)]
pub enum Error {
    Nsresult(nsresult),
    WebextStorage(WebextStorageError),
    MigrationFailed(WebextStorageError),
    GoldenGate(GoldenGateError),
    MalformedString(Box<dyn error::Error + Send + Sync + 'static>),
    AlreadyConfigured,
    NotConfigured,
    AlreadyRan(&'static str),
    DidNotRun(&'static str),
    AlreadyTornDown,
    NotImplemented,
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::MalformedString(error) => Some(error.as_ref()),
            _ => None,
        }
    }
}

impl From<nsresult> for Error {
    fn from(result: nsresult) -> Error {
        Error::Nsresult(result)
    }
}

impl From<WebextStorageError> for Error {
    fn from(error: WebextStorageError) -> Error {
        Error::WebextStorage(error)
    }
}

impl From<GoldenGateError> for Error {
    fn from(error: GoldenGateError) -> Error {
        Error::GoldenGate(error)
    }
}

impl From<Utf8Error> for Error {
    fn from(error: Utf8Error) -> Error {
        Error::MalformedString(error.into())
    }
}

impl From<FromUtf16Error> for Error {
    fn from(error: FromUtf16Error) -> Error {
        Error::MalformedString(error.into())
    }
}

impl From<JsonError> for Error {
    fn from(error: JsonError) -> Error {
        Error::MalformedString(error.into())
    }
}

impl From<Error> for nsresult {
    fn from(error: Error) -> nsresult {
        match error {
            Error::Nsresult(result) => result,
            Error::WebextStorage(e) => match e {
                WebextStorageError::QuotaError(_) => NS_ERROR_DOM_QUOTA_EXCEEDED_ERR,
                _ => NS_ERROR_FAILURE,
            },
            Error::MigrationFailed(_) => NS_ERROR_CANNOT_CONVERT_DATA,
            Error::GoldenGate(error) => error.into(),
            Error::MalformedString(_) => NS_ERROR_INVALID_ARG,
            Error::AlreadyConfigured => NS_ERROR_ALREADY_INITIALIZED,
            Error::NotConfigured => NS_ERROR_NOT_INITIALIZED,
            Error::AlreadyRan(_) => NS_ERROR_UNEXPECTED,
            Error::DidNotRun(_) => NS_ERROR_UNEXPECTED,
            Error::AlreadyTornDown => NS_ERROR_UNEXPECTED,
            Error::NotImplemented => NS_ERROR_NOT_IMPLEMENTED,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::Nsresult(result) => write!(f, "Operation failed with {result}"),
            Error::WebextStorage(error) => error.fmt(f),
            Error::MigrationFailed(error) => write!(f, "Migration failed with {error}"),
            Error::GoldenGate(error) => error.fmt(f),
            Error::MalformedString(error) => error.fmt(f),
            Error::AlreadyConfigured => write!(f, "The storage area is already configured"),
            Error::NotConfigured => write!(
                f,
                "The storage area must be configured by calling `configure` first"
            ),
            Error::AlreadyRan(what) => write!(f, "`{what}` already ran on the background thread"),
            Error::DidNotRun(what) => write!(f, "`{what}` didn't run on the background thread"),
            Error::AlreadyTornDown => {
                write!(f, "Can't use a storage area that's already torn down")
            }
            Error::NotImplemented => write!(f, "Operation not implemented"),
        }
    }
}
