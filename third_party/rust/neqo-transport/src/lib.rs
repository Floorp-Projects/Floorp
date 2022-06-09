// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::use_self)]

use neqo_common::qinfo;
use neqo_crypto::Error as CryptoError;

mod ackrate;
mod addr_valid;
mod cc;
mod cid;
mod connection;
mod crypto;
mod dump;
mod events;
mod fc;
mod frame;
mod pace;
mod packet;
mod path;
mod qlog;
mod quic_datagrams;
mod recovery;
mod recv_stream;
mod rtt;
mod send_stream;
mod sender;
pub mod server;
mod stats;
pub mod stream_id;
pub mod streams;
pub mod tparams;
mod tracking;

pub use self::cc::CongestionControlAlgorithm;
pub use self::cid::{
    ConnectionId, ConnectionIdDecoder, ConnectionIdGenerator, ConnectionIdRef,
    EmptyConnectionIdGenerator, RandomConnectionIdGenerator,
};
pub use self::connection::{
    params::ConnectionParameters, params::ACK_RATIO_SCALE, Connection, Output, State, ZeroRttState,
};
pub use self::events::{ConnectionEvent, ConnectionEvents};
pub use self::frame::CloseError;
pub use self::packet::QuicVersion;
pub use self::stats::Stats;
pub use self::stream_id::{StreamId, StreamType};

pub use self::recv_stream::RECV_BUFFER_SIZE;
pub use self::send_stream::SEND_BUFFER_SIZE;

pub type TransportError = u64;
const ERROR_APPLICATION_CLOSE: TransportError = 12;
const ERROR_AEAD_LIMIT_REACHED: TransportError = 15;

#[derive(Clone, Debug, PartialEq, PartialOrd, Ord, Eq)]
pub enum Error {
    NoError,
    // Each time tihe error is return a different parameter is supply.
    // This will be use to distinguish each occurance of this error.
    InternalError(u16),
    ConnectionRefused,
    FlowControlError,
    StreamLimitError,
    StreamStateError,
    FinalSizeError,
    FrameEncodingError,
    TransportParameterError,
    ProtocolViolation,
    InvalidToken,
    ApplicationError,
    CryptoError(CryptoError),
    QlogError,
    CryptoAlert(u8),
    EchRetry(Vec<u8>),

    // All internal errors from here.  Please keep these sorted.
    AckedUnsentPacket,
    ConnectionIdLimitExceeded,
    ConnectionIdsExhausted,
    ConnectionState,
    DecodingFrame,
    DecryptError,
    HandshakeFailed,
    IdleTimeout,
    IntegerOverflow,
    InvalidInput,
    InvalidMigration,
    InvalidPacket,
    InvalidResumptionToken,
    InvalidRetry,
    InvalidStreamId,
    KeysDiscarded,
    /// Packet protection keys are exhausted.
    /// Also used when too many key updates have happened.
    KeysExhausted,
    /// Packet protection keys aren't available yet for the identified space.
    KeysPending(crypto::CryptoSpace),
    /// An attempt to update keys can be blocked if
    /// a packet sent with the current keys hasn't been acknowledged.
    KeyUpdateBlocked,
    NoAvailablePath,
    NoMoreData,
    NotConnected,
    PacketNumberOverlap,
    PeerApplicationError(AppError),
    PeerError(TransportError),
    StatelessReset,
    TooMuchData,
    UnexpectedMessage,
    UnknownConnectionId,
    UnknownFrameType,
    VersionNegotiation,
    WrongRole,
    NotAvailable,
}

impl Error {
    pub fn code(&self) -> TransportError {
        match self {
            Self::NoError
            | Self::IdleTimeout
            | Self::PeerError(_)
            | Self::PeerApplicationError(_) => 0,
            Self::ConnectionRefused => 2,
            Self::FlowControlError => 3,
            Self::StreamLimitError => 4,
            Self::StreamStateError => 5,
            Self::FinalSizeError => 6,
            Self::FrameEncodingError => 7,
            Self::TransportParameterError => 8,
            Self::ProtocolViolation => 10,
            Self::InvalidToken => 11,
            Self::KeysExhausted => ERROR_AEAD_LIMIT_REACHED,
            Self::ApplicationError => ERROR_APPLICATION_CLOSE,
            Self::NoAvailablePath => 16,
            Self::CryptoAlert(a) => 0x100 + u64::from(*a),
            // As we have a special error code for ECH fallbacks, we lose the alert.
            // Send the server "ech_required" directly.
            Self::EchRetry(_) => 0x100 + 121,
            // All the rest are internal errors.
            _ => 1,
        }
    }
}

impl From<CryptoError> for Error {
    fn from(err: CryptoError) -> Self {
        qinfo!("Crypto operation failed {:?}", err);
        match err {
            CryptoError::EchRetry(config) => Self::EchRetry(config),
            _ => Self::CryptoError(err),
        }
    }
}

impl From<::qlog::Error> for Error {
    fn from(_err: ::qlog::Error) -> Self {
        Self::QlogError
    }
}

impl From<std::num::TryFromIntError> for Error {
    fn from(_: std::num::TryFromIntError) -> Self {
        Self::IntegerOverflow
    }
}

impl ::std::error::Error for Error {
    fn source(&self) -> Option<&(dyn ::std::error::Error + 'static)> {
        match self {
            Self::CryptoError(e) => Some(e),
            _ => None,
        }
    }
}

impl ::std::fmt::Display for Error {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Transport error: {:?}", self)
    }
}

pub type AppError = u64;

#[derive(Clone, Debug, PartialEq, PartialOrd, Ord, Eq)]
pub enum ConnectionError {
    Transport(Error),
    Application(AppError),
}

impl ConnectionError {
    pub fn app_code(&self) -> Option<AppError> {
        match self {
            Self::Application(e) => Some(*e),
            _ => None,
        }
    }
}

impl From<CloseError> for ConnectionError {
    fn from(err: CloseError) -> Self {
        match err {
            CloseError::Transport(c) => Self::Transport(Error::PeerError(c)),
            CloseError::Application(c) => Self::Application(c),
        }
    }
}

pub type Res<T> = std::result::Result<T, Error>;
