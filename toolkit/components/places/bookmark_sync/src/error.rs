/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{error, fmt, result, string::FromUtf16Error};

use nserror::{
    nsresult, NS_ERROR_ABORT, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG, NS_ERROR_STORAGE_BUSY,
    NS_ERROR_UNEXPECTED,
};

pub type Result<T> = result::Result<T, Error>;

#[derive(Debug)]
pub enum Error {
    Dogear(dogear::Error),
    Storage(storage::Error),
    InvalidLocalRoots,
    InvalidRemoteRoots,
    Nsresult(nsresult),
    UnknownItemType(i64),
    UnknownItemKind(i64),
    MalformedString(Box<dyn error::Error + Send + Sync + 'static>),
    MergeConflict,
    UnknownItemValidity(i64),
    DidNotRun,
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::Dogear(err) => Some(err),
            Error::Storage(err) => Some(err),
            _ => None,
        }
    }
}

impl From<dogear::Error> for Error {
    fn from(err: dogear::Error) -> Error {
        Error::Dogear(err)
    }
}

impl From<storage::Error> for Error {
    fn from(err: storage::Error) -> Error {
        Error::Storage(err)
    }
}

impl From<nsresult> for Error {
    fn from(result: nsresult) -> Error {
        Error::Nsresult(result)
    }
}

impl From<FromUtf16Error> for Error {
    fn from(error: FromUtf16Error) -> Error {
        Error::MalformedString(error.into())
    }
}

impl From<Error> for nsresult {
    fn from(error: Error) -> nsresult {
        match error {
            Error::Dogear(err) => match err.kind() {
                dogear::ErrorKind::Abort => NS_ERROR_ABORT,
                _ => NS_ERROR_FAILURE,
            },
            Error::InvalidLocalRoots | Error::InvalidRemoteRoots | Error::DidNotRun => {
                NS_ERROR_UNEXPECTED
            }
            Error::Storage(err) => err.into(),
            Error::Nsresult(result) => result.clone(),
            Error::UnknownItemType(_)
            | Error::UnknownItemKind(_)
            | Error::MalformedString(_)
            | Error::UnknownItemValidity(_) => NS_ERROR_INVALID_ARG,
            Error::MergeConflict => NS_ERROR_STORAGE_BUSY,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::Dogear(err) => err.fmt(f),
            Error::Storage(err) => err.fmt(f),
            Error::InvalidLocalRoots => f.write_str("The Places roots are invalid"),
            Error::InvalidRemoteRoots => {
                f.write_str("The roots in the mirror database are invalid")
            }
            Error::Nsresult(result) => write!(f, "Operation failed with {}", result.error_name()),
            Error::UnknownItemType(typ) => write!(f, "Unknown item type {} in Places", typ),
            Error::UnknownItemKind(kind) => write!(f, "Unknown item kind {} in mirror", kind),
            Error::MalformedString(err) => err.fmt(f),
            Error::MergeConflict => f.write_str("Local tree changed during merge"),
            Error::UnknownItemValidity(validity) => {
                write!(f, "Unknown item validity {} in database", validity)
            }
            Error::DidNotRun => write!(f, "Failed to run merge on storage thread"),
        }
    }
}
