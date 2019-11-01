// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_common::{qdebug, Decoder, IncrementalDecoder, IncrementalDecoderResult};
use neqo_transport::Connection;

#[derive(Debug)]
pub struct NewStreamTypeReader {
    reader: IncrementalDecoder,
    fin: bool,
}

impl NewStreamTypeReader {
    pub fn new() -> NewStreamTypeReader {
        NewStreamTypeReader {
            reader: IncrementalDecoder::decode_varint(),
            fin: false,
        }
    }
    pub fn get_type(&mut self, conn: &mut Connection, stream_id: u64) -> Option<u64> {
        // On any error we will only close this stream!
        loop {
            let to_read = self.reader.min_remaining();
            let mut buf = vec![0; to_read];
            match conn.stream_recv(stream_id, &mut buf[..]) {
                Ok((_, true)) => {
                    self.fin = true;
                    return None;
                }
                Ok((0, false)) => {
                    return None;
                }
                Ok((amount, false)) => {
                    let mut dec = Decoder::from(&buf[..amount]);
                    match self.reader.consume(&mut dec) {
                        IncrementalDecoderResult::Uint(v) => {
                            return Some(v);
                        }
                        IncrementalDecoderResult::InProgress => {}
                        _ => {
                            return None;
                        }
                    }
                }
                Err(e) => {
                    qdebug!([conn] "Error reading stream type for stream {}: {:?}", stream_id, e);
                    self.fin = true;
                    return None;
                }
            }
        }
    }

    pub fn fin(&self) -> bool {
        self.fin
    }
}
