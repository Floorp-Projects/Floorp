/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use libc::uint16_t;
use nserror::{
    nsresult, NsresultExt, NS_ERROR_FAILURE, NS_ERROR_NOT_IMPLEMENTED, NS_ERROR_NO_INTERFACE,
    NS_ERROR_NULL_POINTER, NS_ERROR_UNEXPECTED,
};
use nsstring::nsCString;
use rkv::StoreError;
use std::{str::Utf8Error, string::FromUtf16Error, sync::PoisonError};

#[derive(Debug, Fail)]
pub enum KeyValueError {
    #[fail(display = "error converting string: {:?}", _0)]
    ConvertBytes(Utf8Error),

    #[fail(display = "error converting string: {:?}", _0)]
    ConvertString(FromUtf16Error),

    #[fail(display = "no interface '{}'", _0)]
    NoInterface(&'static str),

    // NB: We can avoid storing the nsCString error description
    // once nsresult is a real type with a Display implementation
    // per https://bugzilla.mozilla.org/show_bug.cgi?id=1513350.
    #[fail(display = "error result {}", _0)]
    Nsresult(nsCString, nsresult),

    #[fail(display = "arg is null")]
    NullPointer,

    #[fail(display = "poison error getting read/write lock")]
    PoisonError,

    #[fail(display = "error reading key/value pair")]
    Read,

    #[fail(display = "store error: {:?}", _0)]
    StoreError(StoreError),

    #[fail(display = "unsupported owned value type")]
    UnsupportedOwned,

    #[fail(display = "unexpected value")]
    UnexpectedValue,

    #[fail(display = "unsupported variant type: {}", _0)]
    UnsupportedVariant(uint16_t),
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
            KeyValueError::NoInterface(_) => NS_ERROR_NO_INTERFACE,
            KeyValueError::Nsresult(_, result) => result,
            KeyValueError::NullPointer => NS_ERROR_NULL_POINTER,
            KeyValueError::PoisonError => NS_ERROR_UNEXPECTED,
            KeyValueError::Read => NS_ERROR_FAILURE,
            KeyValueError::StoreError(_) => NS_ERROR_FAILURE,
            KeyValueError::UnsupportedOwned => NS_ERROR_NOT_IMPLEMENTED,
            KeyValueError::UnexpectedValue => NS_ERROR_UNEXPECTED,
            KeyValueError::UnsupportedVariant(_) => NS_ERROR_NOT_IMPLEMENTED,
        }
    }
}

impl From<StoreError> for KeyValueError {
    fn from(err: StoreError) -> KeyValueError {
        KeyValueError::StoreError(err)
    }
}

impl From<Utf8Error> for KeyValueError {
    fn from(err: Utf8Error) -> KeyValueError {
        KeyValueError::ConvertBytes(err)
    }
}

impl From<FromUtf16Error> for KeyValueError {
    fn from(err: FromUtf16Error) -> KeyValueError {
        KeyValueError::ConvertString(err)
    }
}

impl<T> From<PoisonError<T>> for KeyValueError {
    fn from(_err: PoisonError<T>) -> KeyValueError {
        KeyValueError::PoisonError
    }
}
