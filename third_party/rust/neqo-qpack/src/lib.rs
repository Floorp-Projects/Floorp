// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]
// This is because of Encoder and Decoder structs. TODO: think about a better namings for crate and structs.
#![allow(clippy::module_name_repetitions)]

pub mod decoder;
mod decoder_instructions;
pub mod encoder;
mod encoder_instructions;
mod header_block;
pub mod huffman;
mod huffman_decode_helper;
pub mod huffman_table;
mod prefix;
mod qlog;
mod qpack_send_buf;
pub mod reader;
mod static_table;
mod stats;
mod table;

pub use decoder::QPackDecoder;
pub use encoder::QPackEncoder;
pub use stats::Stats;

type Res<T> = Result<T, Error>;

#[derive(Debug, PartialEq, PartialOrd, Ord, Eq, Clone, Copy)]
pub struct QpackSettings {
    pub max_table_size_decoder: u64,
    pub max_table_size_encoder: u64,
    pub max_blocked_streams: u16,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    DecompressionFailed,
    EncoderStream,
    DecoderStream,
    ClosedCriticalStream,
    InternalError(u16),

    // These are internal errors, they will be transformed into one of the above.
    NeedMoreData, // Return when an input stream does not have more data that a decoder needs.(It does not mean that a stream is closed.)
    HeaderLookup,
    HuffmanDecompressionFailed,
    ToStringFailed,
    ChangeCapacity,
    DynamicTableFull,
    IncrementAck,
    IntegerOverflow,
    WrongStreamCount,
    Decoding, // Decoding internal error that is not one of the above.
    EncoderStreamBlocked,
    Internal,

    TransportError(neqo_transport::Error),
    QlogError,
}

impl Error {
    #[must_use]
    pub fn code(&self) -> neqo_transport::AppError {
        match self {
            Self::DecompressionFailed => 0x200,
            Self::EncoderStream => 0x201,
            Self::DecoderStream => 0x202,
            Self::ClosedCriticalStream => 0x104,
            // These are all internal errors.
            _ => 3,
        }
    }

    /// # Errors
    ///   Any error is mapped to the indicated type.
    fn map_error<R>(r: Result<R, Self>, err: Self) -> Result<R, Self> {
        r.map_err(|e| {
            if matches!(e, Self::ClosedCriticalStream) {
                e
            } else {
                err
            }
        })
    }
}

impl ::std::error::Error for Error {
    fn source(&self) -> Option<&(dyn ::std::error::Error + 'static)> {
        match self {
            Self::TransportError(e) => Some(e),
            _ => None,
        }
    }
}

impl ::std::fmt::Display for Error {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "QPACK error: {:?}", self)
    }
}

impl From<neqo_transport::Error> for Error {
    fn from(err: neqo_transport::Error) -> Self {
        Self::TransportError(err)
    }
}

impl From<::qlog::Error> for Error {
    fn from(_err: ::qlog::Error) -> Self {
        Self::QlogError
    }
}
