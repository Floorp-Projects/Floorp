/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("[no-sentry] Illegal characters in request header '{0}'")]
    RequestHeaderError(crate::HeaderName),

    #[error("[no-sentry] Backend error: {0}")]
    BackendError(String),

    #[error("[no-sentry] Network error: {0}")]
    NetworkError(String),

    #[error("The rust-components network backend must be initialized before use!")]
    BackendNotInitialized,

    #[error("Backend already initialized.")]
    SetBackendError,

    /// Note: we return this if the server returns a bad URL with
    /// its response. This *probably* should never happen, but who knows.
    #[error("[no-sentry] URL Parse Error: {0}")]
    UrlError(#[source] url::ParseError),

    #[error("[no-sentry] Validation error: URL does not use TLS protocol.")]
    NonTlsUrl,
}

impl From<url::ParseError> for Error {
    fn from(u: url::ParseError) -> Self {
        Error::UrlError(u)
    }
}

/// This error is returned as the `Err` result from
/// [`Response::require_success`].
///
/// Note that it's not a variant on `Error` to distinguish between errors
/// caused by the network, and errors returned from the server.
#[derive(thiserror::Error, Debug, Clone)]
#[error("Error: {method} {url} returned {status}")]
pub struct UnexpectedStatus {
    pub status: u16,
    pub method: crate::Method,
    pub url: url::Url,
}
