// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]
// This is because of Encoder and Decoder structs. TODO: think about a better namings for crate and structs.
#![allow(clippy::module_name_repetitions)]
// We need this because of TransportError.
#![allow(clippy::pub_enum_variant_names)]

pub mod decoder;
mod decoder_instructions;
pub mod encoder;
mod encoder_instructions;
mod header_block;
pub mod huffman;
mod huffman_decode_helper;
pub mod huffman_table;
mod prefix;
mod qpack_send_buf;
pub mod reader;
mod static_table;
mod table;

pub type Header = (String, String);
type Res<T> = Result<T, Error>;

#[derive(Debug)]
enum QPackSide {
    Encoder,
    Decoder,
}

impl ::std::fmt::Display for QPackSide {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:?}", self)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    DecompressionFailed,
    EncoderStream,
    DecoderStream,
    ClosedCriticalStream,

    // These are internal errors, they will be transfromed into one of the above.
    HeaderLookup,
    NoMoreData,
    IntegerOverflow,
    WrongStreamCount,
    Internal,
    Decoding, // this will be translated into Encoder/DecoderStreamError or DecompressionFailed depending on the caller

    TransportError(neqo_transport::Error),
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
