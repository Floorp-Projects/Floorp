// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{frames::reader::FrameDecoder, Error, Res};
use neqo_common::{Decoder, Encoder};
use std::convert::TryFrom;

pub(crate) type WebTransportFrameType = u64;

const WT_FRAME_CLOSE_SESSION: WebTransportFrameType = 0x2843;
const WT_FRAME_CLOSE_MAX_MESSAGE_SIZE: u64 = 1024;

#[derive(PartialEq, Eq, Debug)]
pub enum WebTransportFrame {
    CloseSession { error: u32, message: String },
}

impl WebTransportFrame {
    pub fn encode(&self, enc: &mut Encoder) {
        enc.encode_varint(WT_FRAME_CLOSE_SESSION);
        let WebTransportFrame::CloseSession { error, message } = &self;
        enc.encode_varint(4 + message.len() as u64);
        enc.encode_uint(4, *error);
        enc.encode(message.as_bytes());
    }
}

impl FrameDecoder<WebTransportFrame> for WebTransportFrame {
    fn decode(
        frame_type: u64,
        frame_len: u64,
        data: Option<&[u8]>,
    ) -> Res<Option<WebTransportFrame>> {
        if let Some(payload) = data {
            let mut dec = Decoder::from(payload);
            if frame_type == WT_FRAME_CLOSE_SESSION {
                if frame_len > WT_FRAME_CLOSE_MAX_MESSAGE_SIZE + 4 {
                    return Err(Error::HttpMessageError);
                }
                let error =
                    u32::try_from(dec.decode_uint(4).ok_or(Error::HttpMessageError)?).unwrap();
                let Ok(message) = String::from_utf8(dec.decode_remainder().to_vec()) else {
                    return Err(Error::HttpMessageError);
                };
                Ok(Some(WebTransportFrame::CloseSession { error, message }))
            } else {
                Ok(None)
            }
        } else {
            Ok(None)
        }
    }

    fn is_known_type(frame_type: u64) -> bool {
        frame_type == WT_FRAME_CLOSE_SESSION
    }
}
