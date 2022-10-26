/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[derive(Debug, thiserror::Error)]
pub enum TabsError {
    #[cfg(feature = "full-sync")]
    #[error("Error synchronizing: {0}")]
    SyncAdapterError(#[from] sync15::Error),

    // Note we are abusing this as a kind of "mis-matched feature" error.
    // This works because when `full-sync` isn't enabled we don't actually
    // handle any sync15 errors as the bridged-engine never returns them.
    #[cfg(not(feature = "full-sync"))]
    #[error("Sync feature is disabled: {0}")]
    SyncAdapterError(String),

    #[error("Sync reset error: {0}")]
    SyncResetError(#[from] anyhow::Error),

    #[error("Error parsing JSON data: {0}")]
    JsonError(#[from] serde_json::Error),

    #[error("Missing SyncUnlockInfo Local ID")]
    MissingLocalIdError,

    #[error("Error parsing URL: {0}")]
    UrlParseError(#[from] url::ParseError),

    #[error("Error executing SQL: {0}")]
    SqlError(#[from] rusqlite::Error),

    #[error("Error opening database: {0}")]
    OpenDatabaseError(#[from] sql_support::open_database::Error),
}

pub type Result<T> = std::result::Result<T, TabsError>;
