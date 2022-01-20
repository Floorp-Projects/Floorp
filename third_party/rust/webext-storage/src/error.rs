/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use interrupt_support::Interrupted;
use sync15_traits::bridged_engine;

#[derive(Debug)]
pub enum QuotaReason {
    TotalBytes,
    ItemBytes,
    MaxItems,
}

#[derive(Debug, thiserror::Error)]
pub enum ErrorKind {
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

    #[error("Database cannot be upgraded")]
    DatabaseUpgradeError,

    #[error("Database version {0} is not supported")]
    UnsupportedDatabaseVersion(i64),

    #[error("{0}")]
    IncomingPayloadError(#[from] bridged_engine::PayloadError),
}

error_support::define_error! {
    ErrorKind {
        (JsonError, serde_json::Error),
        (SqlError, rusqlite::Error),
        (IoError, std::io::Error),
        (InterruptedError, Interrupted),
        (Utf8Error, std::str::Utf8Error),
        (IncomingPayloadError, bridged_engine::PayloadError)
    }
}
