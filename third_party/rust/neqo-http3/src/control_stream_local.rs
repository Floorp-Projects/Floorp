// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::HFrame;
use crate::{Http3StreamType, RecvStream, Res};
use neqo_common::{qtrace, Encoder};
use neqo_transport::{Connection, StreamType};
use std::collections::{HashMap, VecDeque};
use std::convert::TryFrom;

pub const HTTP3_UNI_STREAM_TYPE_CONTROL: u64 = 0x0;

// The local control stream, responsible for encoding frames and sending them
#[derive(Debug)]
pub(crate) struct ControlStreamLocal {
    stream_id: Option<u64>,
    buf: Vec<u8>,
    // `stream_id`s of outstanding request streams
    outstanding_priority_update: VecDeque<u64>,
}

impl ::std::fmt::Display for ControlStreamLocal {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Local control stream {:?}", self.stream_id)
    }
}

impl ControlStreamLocal {
    pub fn new() -> Self {
        Self {
            stream_id: None,
            buf: vec![u8::try_from(HTTP3_UNI_STREAM_TYPE_CONTROL).unwrap()],
            outstanding_priority_update: VecDeque::new(),
        }
    }

    /// Add a new frame that needs to be send.
    pub fn queue_frame(&mut self, f: &HFrame) {
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        self.buf.append(&mut enc.into());
    }

    pub fn queue_update_priority(&mut self, stream_id: u64) {
        self.outstanding_priority_update.push_back(stream_id);
    }

    /// Send control data if available.
    pub fn send(
        &mut self,
        conn: &mut Connection,
        recv_conn: &mut HashMap<u64, Box<dyn RecvStream>>,
    ) -> Res<()> {
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
            // only send priority updates if all buffer data has been sent
            if self.buf.is_empty() {
                self.send_priority_update(stream_id, conn, recv_conn)?;
            }
        }
        Ok(())
    }

    fn send_priority_update(
        &mut self,
        stream_id: u64,
        conn: &mut Connection,
        recv_conn: &mut HashMap<u64, Box<dyn RecvStream>>,
    ) -> Res<()> {
        // send all necessary priority updates
        while let Some(update_id) = self.outstanding_priority_update.pop_front() {
            let update_stream = match recv_conn.get_mut(&update_id) {
                Some(update_stream) => update_stream,
                None => continue,
            };

            // can assert and unwrap here, because priority updates can only be added to
            // HttpStreams in [Http3Connection::queue_update_priority}
            debug_assert!(matches!(
                update_stream.stream_type(),
                Http3StreamType::Http | Http3StreamType::Push
            ));
            let priority_handler = update_stream.http_stream().unwrap().priority_handler_mut();

            // in case multiple priority_updates were issued, ignore now irrelevant
            if let Some(hframe) = priority_handler.maybe_encode_frame(update_id) {
                let mut enc = Encoder::new();
                hframe.encode(&mut enc);
                if conn.stream_send_atomic(stream_id, &enc)? {
                    priority_handler.priority_update_sent();
                } else {
                    self.outstanding_priority_update.push_front(update_id);
                    break;
                }
            }
        }
        Ok(())
    }

    /// Create a control stream.
    pub fn create(&mut self, conn: &mut Connection) -> Res<()> {
        qtrace!([self], "Create a control stream.");
        self.stream_id = Some(conn.stream_create(StreamType::UniDi)?);
        Ok(())
    }

    #[must_use]
    pub fn stream_id(&self) -> Option<u64> {
        self.stream_id
    }
}
