/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use failure::{Backtrace, Context, Fail};
use std::{boxed::Box, fmt, result};

pub type Result<T> = result::Result<T, Error>;

#[derive(Debug)]
pub struct Error(Box<Context<ErrorKind>>);

impl Fail for Error {
    #[inline]
    fn cause(&self) -> Option<&dyn Fail> {
        self.0.cause()
    }

    #[inline]
    fn backtrace(&self) -> Option<&Backtrace> {
        self.0.backtrace()
    }
}

impl fmt::Display for Error {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&*self.0, f)
    }
}

impl Error {
    #[inline]
    pub fn kind(&self) -> &ErrorKind {
        &*self.0.get_context()
    }
}

impl From<ErrorKind> for Error {
    #[inline]
    fn from(kind: ErrorKind) -> Error {
        Error(Box::new(Context::new(kind)))
    }
}

impl From<Context<ErrorKind>> for Error {
    #[inline]
    fn from(inner: Context<ErrorKind>) -> Error {
        Error(Box::new(inner))
    }
}

#[derive(Debug, Fail)]
pub enum ErrorKind {
    #[fail(display = "Invalid auth secret")]
    InvalidAuthSecret,

    #[fail(display = "Invalid salt")]
    InvalidSalt,

    #[fail(display = "Invalid key length")]
    InvalidKeyLength,

    #[fail(display = "Invalid record size")]
    InvalidRecordSize,

    #[fail(display = "Invalid header size (too short)")]
    HeaderTooShort,

    #[fail(display = "Truncated ciphertext")]
    DecryptTruncated,

    #[fail(display = "Zero-length ciphertext")]
    ZeroCiphertext,

    #[fail(display = "Zero-length plaintext")]
    ZeroPlaintext,

    #[fail(display = "Block too short")]
    BlockTooShort,

    #[fail(display = "Invalid decryption padding")]
    DecryptPadding,

    #[fail(display = "Invalid encryption padding")]
    EncryptPadding,

    #[fail(display = "Could not decode base64 entry")]
    DecodeError,

    #[fail(display = "Crypto backend error")]
    CryptoError,

    #[cfg(feature = "backend-openssl")]
    #[fail(display = "OpenSSL error: {}", _0)]
    OpenSSLError(#[fail(cause)] openssl::error::ErrorStack),
}

impl From<base64::DecodeError> for Error {
    #[inline]
    fn from(_: base64::DecodeError) -> Error {
        ErrorKind::DecodeError.into()
    }
}

#[cfg(feature = "backend-openssl")]
macro_rules! impl_from_error {
    ($(($variant:ident, $type:ty)),+) => ($(
        impl From<$type> for ErrorKind {
            #[inline]
            fn from(e: $type) -> ErrorKind {
                ErrorKind::$variant(e)
            }
        }

        impl From<$type> for Error {
            #[inline]
            fn from(e: $type) -> Error {
                ErrorKind::from(e).into()
            }
        }
    )*);
}

#[cfg(feature = "backend-openssl")]
impl_from_error! {
    (OpenSSLError, ::openssl::error::ErrorStack)
}
