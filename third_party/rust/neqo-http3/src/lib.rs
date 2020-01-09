// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]

mod client_events;
mod connection;
pub mod connection_client;
mod connection_server;
mod control_stream_local;
mod control_stream_remote;
pub mod hframe;
mod hsettings_frame;
pub mod server;
mod server_connection_events;
mod server_events;
mod stream_type_reader;
mod transaction_client;
pub mod transaction_server;
//pub mod server;

use neqo_qpack;
use neqo_transport;
pub use neqo_transport::Output;

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
    TransportError(neqo_transport::Error),
    Unavailable,
    Unexpected,
    InvalidResumptionToken,
}

impl Error {
    pub fn code(&self) -> neqo_transport::AppError {
        match self {
            Error::HttpNoError => 0x100,
            Error::HttpGeneralProtocolError => 0x101,
            Error::HttpInternalError => 0x102,
            Error::HttpStreamCreationError => 0x103,
            Error::HttpClosedCriticalStream => 0x104,
            Error::HttpFrameUnexpected => 0x105,
            Error::HttpFrameError => 0x106,
            Error::HttpExcessiveLoad => 0x107,
            Error::HttpIdError => 0x108,
            Error::HttpSettingsError => 0x109,
            Error::HttpMissingSettings => 0x10a,
            Error::HttpRequestRejected => 0x10b,
            Error::HttpRequestCancelled => 0x10c,
            Error::HttpRequestIncomplete => 0x10d,
            Error::HttpEarlyResponse => 0x10e,
            Error::HttpConnectError => 0x10f,
            Error::HttpVersionFallback => 0x110,
            Error::QpackError(e) => e.code(),
            // These are all internal errors.
            _ => 3,
        }
    }

    pub fn from_code(error: neqo_transport::AppError) -> Error {
        match error {
            0x100 => Error::HttpNoError,
            0x101 => Error::HttpGeneralProtocolError,
            0x102 => Error::HttpInternalError,
            0x103 => Error::HttpStreamCreationError,
            0x104 => Error::HttpClosedCriticalStream,
            0x105 => Error::HttpFrameUnexpected,
            0x106 => Error::HttpFrameError,
            0x107 => Error::HttpExcessiveLoad,
            0x108 => Error::HttpIdError,
            0x109 => Error::HttpSettingsError,
            0x10a => Error::HttpMissingSettings,
            0x10b => Error::HttpRequestRejected,
            0x10c => Error::HttpRequestCancelled,
            0x10d => Error::HttpRequestIncomplete,
            0x10e => Error::HttpEarlyResponse,
            0x10f => Error::HttpConnectError,
            0x110 => Error::HttpVersionFallback,
            0x200 => Error::QpackError(neqo_qpack::Error::DecompressionFailed),
            0x201 => Error::QpackError(neqo_qpack::Error::EncoderStreamError),
            0x202 => Error::QpackError(neqo_qpack::Error::DecoderStreamError),
            _ => Error::HttpInternalError,
        }
    }
}

impl From<neqo_transport::Error> for Error {
    fn from(err: neqo_transport::Error) -> Self {
        Error::TransportError(err)
    }
}

impl From<neqo_qpack::Error> for Error {
    fn from(err: neqo_qpack::Error) -> Self {
        Error::QpackError(err)
    }
}

impl ::std::error::Error for Error {
    fn source(&self) -> Option<&(dyn ::std::error::Error + 'static)> {
        match self {
            Error::TransportError(e) => Some(e),
            Error::QpackError(e) => Some(e),
            _ => None,
        }
    }
}

impl ::std::fmt::Display for Error {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "HTTP/3 error: {:?}", self)
    }
}
