/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nserror::{
    nsresult, NS_ERROR_FAILURE, NS_ERROR_NOT_IMPLEMENTED, NS_ERROR_NO_INTERFACE,
    NS_ERROR_NULL_POINTER, NS_ERROR_UNEXPECTED,
};
use nsstring::nsCString;
use rkv::{MigrateError, StoreError};
use std::{io::Error as IoError, str::Utf8Error, string::FromUtf16Error, sync::PoisonError};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum KeyValueError {
    #[error("error converting string: {0:?}")]
    ConvertBytes(#[from] Utf8Error),

    #[error("error converting string: {0:?}")]
    ConvertString(#[from] FromUtf16Error),

    #[error("I/O error: {0:?}")]
    IoError(#[from] IoError),

    #[error("migrate error: {0:?}")]
    MigrateError(#[from] MigrateError),

    #[error("no interface '{0}'")]
    NoInterface(&'static str),

    // NB: We can avoid storing the nsCString error description
    // once nsresult is a real type with a Display implementation
    // per https://bugzilla.mozilla.org/show_bug.cgi?id=1513350.
    #[error("error result {0}")]
    Nsresult(nsCString, nsresult),

    #[error("arg is null")]
    NullPointer,

    #[error("poison error getting read/write lock")]
    PoisonError,

    #[error("error reading key/value pair")]
    Read,

    #[error("store error: {0:?}")]
    StoreError(#[from] StoreError),

    #[error("unsupported owned value type")]
    UnsupportedOwned,

    #[error("unexpected value")]
    UnexpectedValue,

    #[error("unsupported variant type: {0}")]
    UnsupportedVariant(u16),
}

impl From<nsresult> for KeyValueError {
    fn from(result: nsresult) -> KeyValueError {
        KeyValueError::Nsresult(result.error_name(), result)
    }
}

impl From<KeyValueError> for nsresult {
    fn from(err: KeyValueError) -> nsresult {
        match err {
            KeyValueError::ConvertBytes(_) => NS_ERROR_FAILURE,
            KeyValueError::ConvertString(_) => NS_ERROR_FAILURE,
            KeyValueError::IoError(_) => NS_ERROR_FAILURE,
            KeyValueError::NoInterface(_) => NS_ERROR_NO_INTERFACE,
            KeyValueError::Nsresult(_, result) => result,
            KeyValueError::NullPointer => NS_ERROR_NULL_POINTER,
            KeyValueError::PoisonError => NS_ERROR_UNEXPECTED,
            KeyValueError::Read => NS_ERROR_FAILURE,
            KeyValueError::StoreError(_) => NS_ERROR_FAILURE,
            KeyValueError::MigrateError(_) => NS_ERROR_FAILURE,
            KeyValueError::UnsupportedOwned => NS_ERROR_NOT_IMPLEMENTED,
            KeyValueError::UnexpectedValue => NS_ERROR_UNEXPECTED,
            KeyValueError::UnsupportedVariant(_) => NS_ERROR_NOT_IMPLEMENTED,
        }
    }
}

impl<T> From<PoisonError<T>> for KeyValueError {
    fn from(_err: PoisonError<T>) -> KeyValueError {
        KeyValueError::PoisonError
    }
}
