/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use rc_crypto::hawk;
use std::string;
#[derive(Debug, thiserror::Error)]
pub enum ErrorKind {
    #[error("Server asked the client to back off, please wait {0} seconds to try again")]
    BackoffError(u64),

    #[error("Unknown OAuth State")]
    UnknownOAuthState,

    #[error("Multiple OAuth scopes requested")]
    MultipleScopesRequested,

    #[error("No cached token for scope {0}")]
    NoCachedToken(String),

    #[error("No cached scoped keys for scope {0}")]
    NoScopedKey(String),

    #[error("No stored refresh token")]
    NoRefreshToken,

    #[error("No stored session token")]
    NoSessionToken,

    #[error("No stored migration data")]
    NoMigrationData,

    #[error("No stored current device id")]
    NoCurrentDeviceId,

    #[error("Device target is unknown (Device ID: {0})")]
    UnknownTargetDevice(String),

    #[error("Unrecoverable server error {0}")]
    UnrecoverableServerError(&'static str),

    #[error("Illegal state: {0}")]
    IllegalState(&'static str),

    #[error("Unknown command: {0}")]
    UnknownCommand(String),

    #[error("Send Tab diagnosis error: {0}")]
    SendTabDiagnosisError(&'static str),

    #[error("Cannot xor arrays with different lengths: {0} and {1}")]
    XorLengthMismatch(usize, usize),

    #[error("Origin mismatch")]
    OriginMismatch,

    #[error("Remote key and local key mismatch")]
    MismatchedKeys,

    #[error("Could not find a suitable anon_id key")]
    NoAnonIdKey,

    #[error("Client: {0} is not allowed to request scope: {1}")]
    ScopeNotAllowed(String, String),

    #[error("Unsupported command: {0}")]
    UnsupportedCommand(&'static str),

    #[error("Missing URL parameter: {0}")]
    MissingUrlParameter(&'static str),

    #[error("Null pointer passed to FFI")]
    NullPointer,

    #[error("Invalid buffer length: {0}")]
    InvalidBufferLength(i32),

    #[error("Too many calls to auth introspection endpoint")]
    AuthCircuitBreakerError,

    #[error("Remote server error: '{code}' '{errno}' '{error}' '{message}' '{info}'")]
    RemoteError {
        code: u64,
        errno: u64,
        error: String,
        message: String,
        info: String,
    },

    // Basically reimplement error_chain's foreign_links. (Ugh, this sucks).
    #[error("Crypto/NSS error: {0}")]
    CryptoError(#[from] rc_crypto::Error),

    #[error("http-ece encryption error: {0}")]
    EceError(#[from] rc_crypto::ece::Error),

    #[error("Hex decode error: {0}")]
    HexDecodeError(#[from] hex::FromHexError),

    #[error("Base64 decode error: {0}")]
    Base64Decode(#[from] base64::DecodeError),

    #[error("JSON error: {0}")]
    JsonError(#[from] serde_json::Error),

    #[error("JWCrypto error: {0}")]
    JwCryptoError(#[from] jwcrypto::JwCryptoError),

    #[error("UTF8 decode error: {0}")]
    UTF8DecodeError(#[from] string::FromUtf8Error),

    #[error("Network error: {0}")]
    RequestError(#[from] viaduct::Error),

    #[error("Malformed URL error: {0}")]
    MalformedUrl(#[from] url::ParseError),

    #[error("Unexpected HTTP status: {0}")]
    UnexpectedStatus(#[from] viaduct::UnexpectedStatus),

    #[error("Sync15 error: {0}")]
    SyncError(#[from] sync15::Error),

    #[error("HAWK error: {0}")]
    HawkError(#[from] hawk::Error),

    #[error("Protobuf decode error: {0}")]
    ProtobufDecodeError(#[from] prost::DecodeError),
}

error_support::define_error! {
    ErrorKind {
        (CryptoError, rc_crypto::Error),
        (EceError, rc_crypto::ece::Error),
        (HexDecodeError, hex::FromHexError),
        (Base64Decode, base64::DecodeError),
        (JsonError, serde_json::Error),
        (JwCryptoError, jwcrypto::JwCryptoError),
        (UTF8DecodeError, std::string::FromUtf8Error),
        (RequestError, viaduct::Error),
        (UnexpectedStatus, viaduct::UnexpectedStatus),
        (MalformedUrl, url::ParseError),
        (SyncError, sync15::Error),
        (ProtobufDecodeError, prost::DecodeError),
    }
}

error_support::define_error_conversions! {
    ErrorKind {
        (HawkError, hawk::Error),
    }
}
