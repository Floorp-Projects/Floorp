/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use error_support::{ErrorHandling, GetErrorHandling};
use interrupt_support::Interrupted;
use std::fmt;

/// Result enum used by all implementation functions in this crate.
/// These wll be automagically turned into `WebExtStorageApiError` at the
/// FFI layer.
pub type Result<T> = std::result::Result<T, Error>;

#[derive(Debug, Clone, Copy)]
pub enum QuotaReason {
    TotalBytes,
    ItemBytes,
    MaxItems,
}

impl fmt::Display for QuotaReason {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            QuotaReason::ItemBytes => write!(f, "ItemBytes"),
            QuotaReason::MaxItems => write!(f, "MaxItems"),
            QuotaReason::TotalBytes => write!(f, "TotalBytes"),
        }
    }
}

#[derive(Debug, thiserror::Error)]
pub enum WebExtStorageApiError {
    #[error("Unexpected webext-storage error: {reason}")]
    UnexpectedError { reason: String },

    #[error("Error parsing JSON data: {reason}")]
    JsonError { reason: String },

    #[error("Quota exceeded: {reason}")]
    QuotaError { reason: QuotaReason },
}

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("Quota exceeded: {0:?}")]
    QuotaError(QuotaReason),

    #[error("Error parsing JSON data: {0}")]
    JsonError(#[from] serde_json::Error),

    #[error("Error executing SQL: {0}")]
    SqlError(#[from] rusqlite::Error),

    #[error("A connection of this type is already open")]
    ConnectionAlreadyOpen,

    #[error("An invalid connection type was specified")]
    InvalidConnectionType,

    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),

    #[error("Operation interrupted")]
    InterruptedError(#[from] Interrupted),

    #[error("Tried to close connection on wrong StorageApi instance")]
    WrongApiForClose,

    // This will happen if you provide something absurd like
    // "/" or "" as your database path. For more subtley broken paths,
    // we'll likely return an IoError.
    #[error("Illegal database path: {0:?}")]
    IllegalDatabasePath(std::path::PathBuf),

    #[error("UTF8 Error: {0}")]
    Utf8Error(#[from] std::str::Utf8Error),

    #[error("Error opening database: {0}")]
    OpenDatabaseError(#[from] sql_support::open_database::Error),

    // When trying to close a connection but we aren't the exclusive owner of the containing Arc<>
    #[error("Other shared references to this connection are alive")]
    OtherConnectionReferencesExist,

    #[error("The storage database has been closed")]
    DatabaseConnectionClosed,

    #[error("Sync Error: {0}")]
    SyncError(String),
}

impl GetErrorHandling for Error {
    type ExternalError = WebExtStorageApiError;

    fn get_error_handling(&self) -> ErrorHandling<Self::ExternalError> {
        match self {
            Error::QuotaError(reason) => {
                log::info!("webext-storage-quota-error");
                ErrorHandling::convert(WebExtStorageApiError::QuotaError { reason: *reason })
            }
            Error::JsonError(e) => {
                log::info!("webext-storage-json-error");
                ErrorHandling::convert(WebExtStorageApiError::JsonError {
                    reason: e.to_string(),
                })
            }
            _ => {
                log::info!("webext-storage-unexpected-error");
                ErrorHandling::convert(WebExtStorageApiError::UnexpectedError {
                    reason: self.to_string(),
                })
            }
        }
    }
}

impl From<Error> for WebExtStorageApiError {
    fn from(err: Error) -> WebExtStorageApiError {
        match err {
            Error::JsonError(e) => WebExtStorageApiError::JsonError {
                reason: e.to_string(),
            },
            Error::QuotaError(reason) => WebExtStorageApiError::QuotaError { reason },
            _ => WebExtStorageApiError::UnexpectedError {
                reason: err.to_string(),
            },
        }
    }
}

impl From<rusqlite::Error> for WebExtStorageApiError {
    fn from(value: rusqlite::Error) -> Self {
        WebExtStorageApiError::UnexpectedError {
            reason: value.to_string(),
        }
    }
}

impl From<serde_json::Error> for WebExtStorageApiError {
    fn from(value: serde_json::Error) -> Self {
        WebExtStorageApiError::JsonError {
            reason: value.to_string(),
        }
    }
}
