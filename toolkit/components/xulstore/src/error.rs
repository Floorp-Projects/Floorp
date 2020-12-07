/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nserror::{
    nsresult, NS_ERROR_FAILURE, NS_ERROR_ILLEGAL_VALUE, NS_ERROR_NOT_AVAILABLE, NS_ERROR_UNEXPECTED,
};
use rkv::{migrate::MigrateError as RkvMigrateError, StoreError as RkvStoreError};
use serde_json::Error as SerdeJsonError;
use std::{io::Error as IoError, str::Utf8Error, string::FromUtf16Error, sync::PoisonError};

pub(crate) type XULStoreResult<T> = Result<T, XULStoreError>;

#[derive(Debug, Fail)]
pub(crate) enum XULStoreError {
    #[fail(display = "error converting bytes: {:?}", _0)]
    ConvertBytes(Utf8Error),

    #[fail(display = "error converting string: {:?}", _0)]
    ConvertString(FromUtf16Error),

    #[fail(display = "I/O error: {:?}", _0)]
    IoError(IoError),

    #[fail(display = "iteration is finished")]
    IterationFinished,

    #[fail(display = "JSON error: {}", _0)]
    JsonError(SerdeJsonError),

    #[fail(display = "error result {}", _0)]
    NsResult(nsresult),

    #[fail(display = "poison error getting read/write lock")]
    PoisonError,

    #[fail(display = "migrate error: {:?}", _0)]
    RkvMigrateError(RkvMigrateError),

    #[fail(display = "store error: {:?}", _0)]
    RkvStoreError(RkvStoreError),

    #[fail(display = "id or attribute name too long")]
    IdAttrNameTooLong,

    #[fail(display = "unavailable")]
    Unavailable,

    #[fail(display = "unexpected key: {:?}", _0)]
    UnexpectedKey(String),

    #[fail(display = "unexpected value")]
    UnexpectedValue,
}

impl From<XULStoreError> for nsresult {
    fn from(err: XULStoreError) -> nsresult {
        match err {
            XULStoreError::ConvertBytes(_) => NS_ERROR_FAILURE,
            XULStoreError::ConvertString(_) => NS_ERROR_FAILURE,
            XULStoreError::IoError(_) => NS_ERROR_FAILURE,
            XULStoreError::IterationFinished => NS_ERROR_FAILURE,
            XULStoreError::JsonError(_) => NS_ERROR_FAILURE,
            XULStoreError::NsResult(result) => result,
            XULStoreError::PoisonError => NS_ERROR_UNEXPECTED,
            XULStoreError::RkvMigrateError(_) => NS_ERROR_FAILURE,
            XULStoreError::RkvStoreError(_) => NS_ERROR_FAILURE,
            XULStoreError::IdAttrNameTooLong => NS_ERROR_ILLEGAL_VALUE,
            XULStoreError::Unavailable => NS_ERROR_NOT_AVAILABLE,
            XULStoreError::UnexpectedKey(_) => NS_ERROR_UNEXPECTED,
            XULStoreError::UnexpectedValue => NS_ERROR_UNEXPECTED,
        }
    }
}

impl From<FromUtf16Error> for XULStoreError {
    fn from(err: FromUtf16Error) -> XULStoreError {
        XULStoreError::ConvertString(err)
    }
}

impl From<nsresult> for XULStoreError {
    fn from(result: nsresult) -> XULStoreError {
        XULStoreError::NsResult(result)
    }
}

impl<T> From<PoisonError<T>> for XULStoreError {
    fn from(_: PoisonError<T>) -> XULStoreError {
        XULStoreError::PoisonError
    }
}

impl From<RkvMigrateError> for XULStoreError {
    fn from(err: RkvMigrateError) -> XULStoreError {
        XULStoreError::RkvMigrateError(err)
    }
}

impl From<RkvStoreError> for XULStoreError {
    fn from(err: RkvStoreError) -> XULStoreError {
        XULStoreError::RkvStoreError(err)
    }
}

impl From<Utf8Error> for XULStoreError {
    fn from(err: Utf8Error) -> XULStoreError {
        XULStoreError::ConvertBytes(err)
    }
}

impl From<IoError> for XULStoreError {
    fn from(err: IoError) -> XULStoreError {
        XULStoreError::IoError(err)
    }
}

impl From<SerdeJsonError> for XULStoreError {
    fn from(err: SerdeJsonError) -> XULStoreError {
        XULStoreError::JsonError(err)
    }
}
