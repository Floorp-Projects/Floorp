/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// `Result` is unused with default features, but used with `feature=crypto`
#[cfg(feature = "crypto")]
pub type Result<T> = std::result::Result<T, SyncTraitsError>;

// Concrete errors returned by this module. Note that the sync15 crate has
// the ability to turn these errors into a `sync15::Error`, so most consumers
// should leverage that rather than create an error variant specific to this
// crate.
#[derive(Debug, thiserror::Error)]
pub enum SyncTraitsError {
    #[cfg(feature = "crypto")]
    #[error("Key {0} had wrong length, got {1}, expected {2}")]
    BadKeyLength(&'static str, usize, usize),

    #[cfg(feature = "crypto")]
    #[error("SHA256 HMAC Mismatch error")]
    HmacMismatch,

    #[cfg(feature = "crypto")]
    #[error("Crypto/NSS error: {0}")]
    CryptoError(#[from] rc_crypto::Error),

    #[cfg(feature = "crypto")]
    #[error("Base64 decode error: {0}")]
    Base64Decode(#[from] base64::DecodeError),

    #[error("JSON error: {0}")]
    JsonError(#[from] serde_json::Error),

    #[error("Bad cleartext UTF8: {0}")]
    BadCleartextUtf8(#[from] std::string::FromUtf8Error),

    #[cfg(feature = "crypto")]
    #[error("HAWK error: {0}")]
    HawkError(#[from] rc_crypto::hawk::Error),
}
