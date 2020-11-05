/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub type Result<T> = std::result::Result<T, Error>;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("Invalid auth secret")]
    InvalidAuthSecret,

    #[error("Invalid salt")]
    InvalidSalt,

    #[error("Invalid key length")]
    InvalidKeyLength,

    #[error("Invalid record size")]
    InvalidRecordSize,

    #[error("Invalid header size (too short)")]
    HeaderTooShort,

    #[error("Truncated ciphertext")]
    DecryptTruncated,

    #[error("Zero-length ciphertext")]
    ZeroCiphertext,

    #[error("Zero-length plaintext")]
    ZeroPlaintext,

    #[error("Block too short")]
    BlockTooShort,

    #[error("Invalid decryption padding")]
    DecryptPadding,

    #[error("Invalid encryption padding")]
    EncryptPadding,

    #[error("Could not decode base64 entry")]
    DecodeError(#[from] base64::DecodeError),

    #[error("Crypto backend error")]
    CryptoError,

    #[cfg(feature = "backend-openssl")]
    #[error("OpenSSL error: {0}")]
    OpenSSLError(#[from] openssl::error::ErrorStack),
}
