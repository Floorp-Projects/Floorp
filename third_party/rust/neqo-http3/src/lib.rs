// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]
#![allow(clippy::pub_enum_variant_names)]

mod client_events;
mod connection;
pub mod connection_client;
mod connection_server;
mod control_stream_local;
mod control_stream_remote;
pub mod hframe;
mod push_controller;
mod push_stream;
mod qlog;
mod recv_message;
mod send_message;
pub mod server;
mod server_connection_events;
mod server_events;
mod settings;
mod stream_type_reader;

use neqo_qpack::decoder::QPackDecoder;
use neqo_qpack::Error as QpackError;
pub use neqo_transport::Output;
use neqo_transport::{AppError, Connection, Error as TransportError};
use std::fmt::Debug;

pub use client_events::Http3ClientEvent;
pub use connection::Http3State;
pub use connection_client::Http3Client;
pub use connection_client::Http3Parameters;
pub use hframe::HFrameReader;
pub use neqo_qpack::Header;
pub use server::Http3Server;
pub use server_events::{ClientRequestStream, Http3ServerEvent};
pub use settings::HttpZeroRttChecker;

type Res<T> = Result<T, Error>;

#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    HttpNoError,
    HttpGeneralProtocol,
    HttpGeneralProtocolStream, //this is the same as the above but it should only close a stream not a connection.
    // When using this error, you need to provide a value that is unique, which
    // will allow the specific error to be identified.  This will be validated in CI.
    HttpInternal(u16),
    HttpStreamCreation,
    HttpClosedCriticalStream,
    HttpFrameUnexpected,
    HttpFrame,
    HttpExcessiveLoad,
    HttpId,
    HttpSettings,
    HttpMissingSettings,
    HttpRequestRejected,
    HttpRequestCancelled,
    HttpRequestIncomplete,
    HttpConnect,
    HttpVersionFallback,
    QpackError(neqo_qpack::Error),

    // Internal errors from here.
    AlreadyClosed,
    AlreadyInitialized,
    DecodingFrame,
    HttpGoaway,
    Internal,
    InvalidResumptionToken,
    InvalidStreamId,
    InvalidState,
    NoMoreData,
    NotEnoughData,
    TransportError(TransportError),
    Unavailable,
    Unexpected,
    StreamLimitError,
    TransportStreamDoesNotExist,
    InvalidInput,
    FatalError,
    InvalidHeader,
}

impl Error {
    #[must_use]
    pub fn code(&self) -> AppError {
        match self {
            Self::HttpNoError => 0x100,
            Self::HttpGeneralProtocol | Self::HttpGeneralProtocolStream | Self::InvalidHeader => {
                0x101
            }
            Self::HttpInternal(..) => 0x102,
            Self::HttpStreamCreation => 0x103,
            Self::HttpClosedCriticalStream => 0x104,
            Self::HttpFrameUnexpected => 0x105,
            Self::HttpFrame => 0x106,
            Self::HttpExcessiveLoad => 0x107,
            Self::HttpId => 0x108,
            Self::HttpSettings => 0x109,
            Self::HttpMissingSettings => 0x10a,
            Self::HttpRequestRejected => 0x10b,
            Self::HttpRequestCancelled => 0x10c,
            Self::HttpRequestIncomplete => 0x10d,
            Self::HttpConnect => 0x10f,
            Self::HttpVersionFallback => 0x110,
            Self::QpackError(e) => e.code(),
            // These are all internal errors.
            _ => 3,
        }
    }

    #[must_use]
    pub fn connection_error(&self) -> bool {
        matches!(
            self,
            Self::HttpGeneralProtocol
                | Self::HttpInternal(..)
                | Self::HttpStreamCreation
                | Self::HttpClosedCriticalStream
                | Self::HttpFrameUnexpected
                | Self::HttpFrame
                | Self::HttpExcessiveLoad
                | Self::HttpId
                | Self::HttpSettings
                | Self::HttpMissingSettings
                | Self::QpackError(QpackError::EncoderStream)
                | Self::QpackError(QpackError::DecoderStream)
        )
    }

    #[must_use]
    pub fn stream_reset_error(&self) -> bool {
        matches!(self, Self::HttpGeneralProtocolStream | Self::InvalidHeader)
    }

    /// # Panics
    /// On unexpected errors, in debug mode.
    #[must_use]
    pub fn map_stream_send_errors(err: &TransportError) -> Self {
        match err {
            TransportError::InvalidStreamId | TransportError::FinalSizeError => {
                Error::TransportStreamDoesNotExist
            }
            TransportError::InvalidInput => Error::InvalidInput,
            _ => {
                debug_assert!(false, "Unexpected error");
                Error::TransportStreamDoesNotExist
            }
        }
    }

    /// # Panics
    /// On unexpected errors, in debug mode.
    #[must_use]
    pub fn map_stream_create_errors(err: &TransportError) -> Self {
        match err {
            TransportError::ConnectionState => Error::Unavailable,
            TransportError::StreamLimitError => Error::StreamLimitError,
            _ => {
                debug_assert!(false, "Unexpected error");
                Error::TransportStreamDoesNotExist
            }
        }
    }

    /// # Panics
    /// On unexpected errors, in debug mode.
    #[must_use]
    pub fn map_stream_recv_errors(err: &TransportError) -> Self {
        match err {
            TransportError::NoMoreData => {
                debug_assert!(
                    false,
                    "Do not call stream_recv if FIN has been previously read"
                );
            }
            TransportError::InvalidStreamId => {}
            _ => {
                debug_assert!(false, "Unexpected error");
            }
        };
        Error::TransportStreamDoesNotExist
    }

    #[must_use]
    pub fn map_set_resumption_errors(err: &TransportError) -> Self {
        match err {
            TransportError::ConnectionState => Error::InvalidState,
            _ => Error::InvalidResumptionToken,
        }
    }

    /// # Errors
    ///   Any error is mapped to the indicated type.
    /// # Panics
    /// On internal errors, in debug mode.
    fn map_error<R>(r: Result<R, impl Into<Self>>, err: Self) -> Result<R, Self> {
        r.map_err(|e| {
            debug_assert!(!matches!(e.into(), Self::HttpInternal(..)));
            debug_assert!(!matches!(err, Self::HttpInternal(..)));
            err
        })
    }
}

impl From<TransportError> for Error {
    fn from(err: TransportError) -> Self {
        Self::TransportError(err)
    }
}

impl From<QpackError> for Error {
    fn from(err: QpackError) -> Self {
        match err {
            QpackError::ClosedCriticalStream => Error::HttpClosedCriticalStream,
            e => Self::QpackError(e),
        }
    }
}

impl From<AppError> for Error {
    fn from(error: AppError) -> Self {
        match error {
            0x100 => Self::HttpNoError,
            0x101 => Self::HttpGeneralProtocol,
            0x103 => Self::HttpStreamCreation,
            0x104 => Self::HttpClosedCriticalStream,
            0x105 => Self::HttpFrameUnexpected,
            0x106 => Self::HttpFrame,
            0x107 => Self::HttpExcessiveLoad,
            0x108 => Self::HttpId,
            0x109 => Self::HttpSettings,
            0x10a => Self::HttpMissingSettings,
            0x10b => Self::HttpRequestRejected,
            0x10c => Self::HttpRequestCancelled,
            0x10d => Self::HttpRequestIncomplete,
            0x10f => Self::HttpConnect,
            0x110 => Self::HttpVersionFallback,
            0x200 => Self::QpackError(QpackError::DecompressionFailed),
            0x201 => Self::QpackError(QpackError::EncoderStream),
            0x202 => Self::QpackError(QpackError::DecoderStream),
            _ => Self::HttpInternal(0),
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

pub trait RecvStream: Debug {
    fn stream_reset(&self, error: AppError, decoder: &mut QPackDecoder, reset_type: ResetType);
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn receive(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()>;
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn header_unblocked(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()>;
    fn done(&self) -> bool;
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn read_data(
        &mut self,
        conn: &mut Connection,
        decoder: &mut QPackDecoder,
        buf: &mut [u8],
    ) -> Res<(usize, bool)>;
}

pub(crate) trait RecvMessageEvents: Debug {
    fn header_ready(&self, stream_id: u64, headers: Vec<Header>, interim: bool, fin: bool);
    fn data_readable(&self, stream_id: u64);
    fn reset(&self, stream_id: u64, error: AppError, local: bool);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ResetType {
    App,
    Remote,
    Local,
}
