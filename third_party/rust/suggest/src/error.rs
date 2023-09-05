/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/// A list of errors that are internal to the component. This is the error
/// type for private and crate-internal methods, and is never returned to the
/// application.
#[derive(Debug, thiserror::Error)]
pub(crate) enum Error {
    #[error("Error opening database: {0}")]
    OpenDatabase(#[from] sql_support::open_database::Error),

    #[error("Error executing SQL: {0}")]
    Sql(#[from] rusqlite::Error),

    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),

    #[error("Error from Remote Settings: {0}")]
    RemoteSettings(#[from] remote_settings::RemoteSettingsError),

    #[error("Operation interrupted")]
    Interrupted(#[from] interrupt_support::Interrupted),
}

/// The error type for all Suggest component operations. These errors are
/// exposed to your application, which should handle them as needed.
#[derive(Debug, thiserror::Error)]
#[non_exhaustive]
pub enum SuggestApiError {
    #[error("Other error: {reason}")]
    Other { reason: String },
}

impl From<Error> for SuggestApiError {
    /// Converts an internal component error to a public application error.
    fn from(error: Error) -> Self {
        Self::Other {
            reason: error.to_string(),
        }
    }
}
