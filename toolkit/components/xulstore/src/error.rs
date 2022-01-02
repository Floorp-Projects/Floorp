/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nserror::{
    nsresult, NS_ERROR_FAILURE, NS_ERROR_ILLEGAL_VALUE, NS_ERROR_NOT_AVAILABLE, NS_ERROR_UNEXPECTED,
};
use rkv::{MigrateError as RkvMigrateError, StoreError as RkvStoreError};
use serde_json::Error as SerdeJsonError;
use std::{io::Error as IoError, str::Utf8Error, string::FromUtf16Error, sync::PoisonError};
use thiserror::Error;

pub(crate) type XULStoreResult<T> = Result<T, XULStoreError>;

#[derive(Debug, Error)]
pub(crate) enum XULStoreError {
    #[error("error converting bytes: {0:?}")]
    ConvertBytes(#[from] Utf8Error),

    #[error("error converting string: {0:?}")]
    ConvertString(#[from] FromUtf16Error),

    #[error("I/O error: {0:?}")]
    IoError(#[from] IoError),

    #[error("iteration is finished")]
    IterationFinished,

    #[error("JSON error: {0}")]
    JsonError(#[from] SerdeJsonError),

    #[error("error result {0}")]
    NsResult(#[from] nsresult),

    #[error("poison error getting read/write lock")]
    PoisonError,

    #[error("migrate error: {0:?}")]
    RkvMigrateError(#[from] RkvMigrateError),

    #[error("store error: {0:?}")]
    RkvStoreError(#[from] RkvStoreError),

    #[error("id or attribute name too long")]
    IdAttrNameTooLong,

    #[error("unavailable")]
    Unavailable,

    #[error("unexpected key: {0:?}")]
    UnexpectedKey(String),

    #[error("unexpected value")]
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

impl<T> From<PoisonError<T>> for XULStoreError {
    fn from(_: PoisonError<T>) -> XULStoreError {
        XULStoreError::PoisonError
    }
}
