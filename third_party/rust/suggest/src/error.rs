/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use error_support::{ErrorHandling, GetErrorHandling};
use remote_settings::RemoteSettingsError;

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
    RemoteSettings(#[from] RemoteSettingsError),

    #[error("Operation interrupted")]
    Interrupted(#[from] interrupt_support::Interrupted),

    #[error("SuggestStoreBuilder {0}")]
    SuggestStoreBuilder(String),
}

/// The error type for all Suggest component operations. These errors are
/// exposed to your application, which should handle them as needed.
#[derive(Debug, thiserror::Error)]
#[non_exhaustive]
pub enum SuggestApiError {
    #[error("Network error: {reason}")]
    Network { reason: String },
    // The server requested a backoff after too many requests
    #[error("Backoff")]
    Backoff { seconds: u64 },
    // The application interrupted a request
    #[error("Interrupted")]
    Interrupted,
    #[error("Other error: {reason}")]
    Other { reason: String },
}

// Define how our internal errors are handled and converted to external errors
// See `support/error/README.md` for how this works, especially the warning about PII.
impl GetErrorHandling for Error {
    type ExternalError = SuggestApiError;

    fn get_error_handling(&self) -> ErrorHandling<Self::ExternalError> {
        match self {
            // Do nothing for interrupted errors, this is just normal operation.
            Self::Interrupted(_) => ErrorHandling::convert(SuggestApiError::Interrupted),
            // Network errors are expected to happen in practice.  Let's log, but not report them.
            Self::RemoteSettings(RemoteSettingsError::RequestError(
                viaduct::Error::NetworkError(e),
            )) => ErrorHandling::convert(SuggestApiError::Network {
                reason: e.to_string(),
            })
            .log_warning(),
            // Backoff error shouldn't happen in practice, so let's report them for now.
            // If these do happen in practice and we decide that there is a valid reason for them,
            // then consider switching from reporting to Sentry to counting in Glean.
            Self::RemoteSettings(RemoteSettingsError::BackoffError(seconds)) => {
                ErrorHandling::convert(SuggestApiError::Backoff { seconds: *seconds })
                    .report_error("suggest-backoff")
            }
            _ => ErrorHandling::convert(SuggestApiError::Other {
                reason: self.to_string(),
            })
            .report_error("suggest-unexpected"),
        }
    }
}
