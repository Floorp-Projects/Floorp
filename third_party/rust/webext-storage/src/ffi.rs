/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This module contains glue for the FFI, including error codes and a
/// `From<Error>` implementation for `ffi_support::ExternError`. These must be
/// implemented in this crate, and not `webext_storage_ffi`, because of Rust's
/// orphan rule for implementing traits (`webext_storage_ffi` can't implement
/// a trait in `ffi_support` for a type in `webext_storage`).
use ffi_support::{ErrorCode, ExternError};

use crate::error::{Error, QuotaReason};

mod error_codes {
    /// An unexpected error occurred which likely cannot be meaningfully handled
    /// by the application.
    pub const UNEXPECTED: i32 = 1;

    /// The application passed an invalid JSON string for a storage key or value.
    pub const INVALID_JSON: i32 = 2;

    /// The total number of bytes stored in the database for this extension,
    /// counting all key-value pairs serialized to JSON, exceeds the allowed limit.
    pub const QUOTA_TOTAL_BYTES_EXCEEDED: i32 = 32;

    /// A single key-value pair exceeds the allowed byte limit when serialized
    /// to JSON.
    pub const QUOTA_ITEM_BYTES_EXCEEDED: i32 = 32 + 1;

    /// The total number of key-value pairs stored for this extension exceeded the
    /// allowed limit.
    pub const QUOTA_MAX_ITEMS_EXCEEDED: i32 = 32 + 2;
}

impl From<Error> for ExternError {
    fn from(err: Error) -> ExternError {
        let code = ErrorCode::new(match &err {
            Error::JsonError(_) => error_codes::INVALID_JSON,
            Error::QuotaError(reason) => match reason {
                QuotaReason::ItemBytes => error_codes::QUOTA_ITEM_BYTES_EXCEEDED,
                QuotaReason::MaxItems => error_codes::QUOTA_MAX_ITEMS_EXCEEDED,
                QuotaReason::TotalBytes => error_codes::QUOTA_TOTAL_BYTES_EXCEEDED,
            },
            _ => error_codes::UNEXPECTED,
        });
        ExternError::new_error(code, err.to_string())
    }
}
