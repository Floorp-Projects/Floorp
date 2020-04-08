// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::HFrame;
use crate::Res;
use neqo_common::{qtrace, Encoder};
use neqo_transport::{Connection, StreamType};

pub const HTTP3_UNI_STREAM_TYPE_CONTROL: u64 = 0x0;

// The local control stream, responsible for encoding frames and sending them
#[derive(Default, Debug)]
pub struct ControlStreamLocal {
    stream_id: Option<u64>,
    buf: Vec<u8>,
}

impl ::std::fmt::Display for ControlStreamLocal {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Local control stream {:?}", self.stream_id)
    }
}

impl ControlStreamLocal {
    pub fn queue_frame(&mut self, f: HFrame) {
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        self.buf.append(&mut enc.into());
    }

    pub fn send(&mut self, conn: &mut Connection) -> Res<()> {
        if let Some(stream_id) = self.stream_id {
            if !self.buf.is_empty() {
                qtrace!([self], "sending data.");
                let sent = conn.stream_send(stream_id, &self.buf[..])?;
                if sent == self.buf.len() {
                    self.buf.clear();
                } else {
                    let b = self.buf.split_off(sent);
                    self.buf = b;
                }
            }
        }
        Ok(())
    }

    pub fn create(&mut self, conn: &mut Connection) -> Res<()> {
        qtrace!([self], "Create a control stream.");
        self.stream_id = Some(conn.stream_create(StreamType::UniDi)?);
        let mut enc = Encoder::default();
        enc.encode_varint(HTTP3_UNI_STREAM_TYPE_CONTROL);
        self.buf.append(&mut enc.into());
        Ok(())
    }
}
