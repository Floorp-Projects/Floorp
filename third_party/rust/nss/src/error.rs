/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[derive(Debug, thiserror::Error)]
pub enum ErrorKind {
    #[error("NSS could not be initialized")]
    NSSInitFailure,
    #[error("NSS error: {0} {1}")]
    NSSError(i32, String),
    #[error("Internal crypto error")]
    InternalError,
    #[error("Conversion error: {0}")]
    ConversionError(#[from] std::num::TryFromIntError),
    #[error("Base64 decode error: {0}")]
    Base64Decode(#[from] base64::DecodeError),
}

error_support::define_error! {
    ErrorKind {
        (Base64Decode, base64::DecodeError),
        (ConversionError, std::num::TryFromIntError),
    }
}
