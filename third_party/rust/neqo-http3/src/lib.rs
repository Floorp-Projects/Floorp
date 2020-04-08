// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::use_self)]

mod client_events;
mod connection;
pub mod connection_client;
mod connection_server;
mod control_stream_local;
mod control_stream_remote;
pub mod hframe;
mod hsettings_frame;
mod response_stream;
pub mod server;
mod server_connection_events;
mod server_events;
mod stream_type_reader;
mod transaction_client;
pub mod transaction_server;
//pub mod server;

use neqo_qpack::Error as QpackError;
pub use neqo_transport::Output;
use neqo_transport::{AppError, Error as TransportError};

pub use client_events::Http3ClientEvent;
pub use connection::Http3State;
pub use connection_client::Http3Client;
pub use neqo_qpack::Header;
pub use server::Http3Server;
pub use server_events::Http3ServerEvent;
pub use transaction_server::TransactionServer;

type Res<T> = Result<T, Error>;

#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    HttpNoError,
    HttpGeneralProtocolError,
    HttpInternalError,
    HttpStreamCreationError,
    HttpClosedCriticalStream,
    HttpFrameUnexpected,
    HttpFrameError,
    HttpExcessiveLoad,
    HttpIdError,
    HttpSettingsError,
    HttpMissingSettings,
    HttpRequestRejected,
    HttpRequestCancelled,
    HttpRequestIncomplete,
    HttpEarlyResponse,
    HttpConnectError,
    HttpVersionFallback,
    QpackError(neqo_qpack::Error),

    // Internal errors from here.
    AlreadyClosed,
    DecodingFrame,
    InvalidStreamId,
    NoMoreData,
    NotEnoughData,
    TransportError(TransportError),
    Unavailable,
    Unexpected,
    InvalidResumptionToken,
}

impl Error {
    pub fn code(&self) -> AppError {
        match self {
            Self::HttpNoError => 0x100,
            Self::HttpGeneralProtocolError => 0x101,
            Self::HttpInternalError => 0x102,
            Self::HttpStreamCreationError => 0x103,
            Self::HttpClosedCriticalStream => 0x104,
            Self::HttpFrameUnexpected => 0x105,
            Self::HttpFrameError => 0x106,
            Self::HttpExcessiveLoad => 0x107,
            Self::HttpIdError => 0x108,
            Self::HttpSettingsError => 0x109,
            Self::HttpMissingSettings => 0x10a,
            Self::HttpRequestRejected => 0x10b,
            Self::HttpRequestCancelled => 0x10c,
            Self::HttpRequestIncomplete => 0x10d,
            Self::HttpEarlyResponse => 0x10e,
            Self::HttpConnectError => 0x10f,
            Self::HttpVersionFallback => 0x110,
            Self::QpackError(e) => e.code(),
            // These are all internal errors.
            _ => 3,
        }
    }
}

impl From<TransportError> for Error {
    fn from(err: TransportError) -> Self {
        Self::TransportError(err)
    }
}

impl From<QpackError> for Error {
    fn from(err: QpackError) -> Self {
        Self::QpackError(err)
    }
}

impl From<AppError> for Error {
    fn from(error: AppError) -> Self {
        match error {
            0x100 => Self::HttpNoError,
            0x101 => Self::HttpGeneralProtocolError,
            0x102 => Self::HttpInternalError,
            0x103 => Self::HttpStreamCreationError,
            0x104 => Self::HttpClosedCriticalStream,
            0x105 => Self::HttpFrameUnexpected,
            0x106 => Self::HttpFrameError,
            0x107 => Self::HttpExcessiveLoad,
            0x108 => Self::HttpIdError,
            0x109 => Self::HttpSettingsError,
            0x10a => Self::HttpMissingSettings,
            0x10b => Self::HttpRequestRejected,
            0x10c => Self::HttpRequestCancelled,
            0x10d => Self::HttpRequestIncomplete,
            0x10e => Self::HttpEarlyResponse,
            0x10f => Self::HttpConnectError,
            0x110 => Self::HttpVersionFallback,
            0x200 => Self::QpackError(QpackError::DecompressionFailed),
            0x201 => Self::QpackError(QpackError::EncoderStream),
            0x202 => Self::QpackError(QpackError::DecoderStream),
            _ => Self::HttpInternalError,
        }
    }
}

impl ::std::error::Error for Error {
    fn source(&self) -> Option<&(dyn ::std::error::Error + 'static)> {
        match self {
            Self::TransportError(e) => Some(e),
            Self::QpackError(e) => Some(e),
            _ => None,
        }
    }
}

impl ::std::fmt::Display for Error {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "HTTP/3 error: {:?}", self)
    }
}
