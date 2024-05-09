/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

use error_support::{ErrorHandling, GetErrorHandling};

/// Errors we return via the public interface.
#[derive(Debug, thiserror::Error)]
pub enum RelevancyApiError {
    #[error("Unexpected Error: {reason}")]
    Unexpected { reason: String },
}

/// Errors we use internally
#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("Error opening database: {0}")]
    OpenDatabase(#[from] sql_support::open_database::Error),

    #[error("Sql error: {0}")]
    SqlError(#[from] rusqlite::Error),

    #[error("Error fetching interest data")]
    FetchInterestDataError,

    #[error("Interrupted")]
    Interrupted(#[from] interrupt_support::Interrupted),

    #[error("Invalid interest code: {0}")]
    InvalidInterestCode(u32),

    #[error("Remote Setting Error: {0}")]
    RemoteSettingsError(#[from] remote_settings::RemoteSettingsError),

    #[error("Serde Json Error: {0}")]
    SerdeJsonError(#[from] serde_json::Error),

    #[error("Base64 Decode Error: {0}")]
    Base64DecodeError(String),
}

/// Result enum for the public API
pub type ApiResult<T> = std::result::Result<T, RelevancyApiError>;

/// Result enum for internal functions
pub type Result<T> = std::result::Result<T, Error>;

// Define how our internal errors are handled and converted to external errors
// See `support/error/README.md` for how this works, especially the warning about PII.
impl GetErrorHandling for Error {
    type ExternalError = RelevancyApiError;

    fn get_error_handling(&self) -> ErrorHandling<Self::ExternalError> {
        ErrorHandling::convert(RelevancyApiError::Unexpected {
            reason: self.to_string(),
        })
    }
}
