// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::frames::HFrame;
use crate::{BufferedStream, Http3StreamType, RecvStream, Res};
use neqo_common::{qtrace, Encoder};
use neqo_transport::{Connection, StreamId, StreamType};
use std::collections::{HashMap, VecDeque};
use std::convert::TryFrom;

pub const HTTP3_UNI_STREAM_TYPE_CONTROL: u64 = 0x0;

/// The local control stream, responsible for encoding frames and sending them
#[derive(Debug)]
pub(crate) struct ControlStreamLocal {
    stream: BufferedStream,
    /// `stream_id`s of outstanding request streams
    outstanding_priority_update: VecDeque<StreamId>,
}

impl ::std::fmt::Display for ControlStreamLocal {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Local control stream {:?}", self.stream)
    }
}

impl ControlStreamLocal {
    pub fn new() -> Self {
        Self {
            stream: BufferedStream::default(),
            outstanding_priority_update: VecDeque::new(),
        }
    }

    /// Add a new frame that needs to be send.
    pub fn queue_frame(&mut self, f: &HFrame) {
        let mut enc = Encoder::default();
        f.encode(&mut enc);
        self.stream.buffer(enc.as_ref());
    }

    pub fn queue_update_priority(&mut self, stream_id: StreamId) {
        self.outstanding_priority_update.push_back(stream_id);
    }

    /// Send control data if available.
    pub fn send(
        &mut self,
        conn: &mut Connection,
        recv_conn: &mut HashMap<StreamId, Box<dyn RecvStream>>,
    ) -> Res<()> {
        self.stream.send_buffer(conn)?;
        self.send_priority_update(conn, recv_conn)
    }

    fn send_priority_update(
        &mut self,
        conn: &mut Connection,
        recv_conn: &mut HashMap<StreamId, Box<dyn RecvStream>>,
    ) -> Res<()> {
        // send all necessary priority updates
        while let Some(update_id) = self.outstanding_priority_update.pop_front() {
            let Some(update_stream) = recv_conn.get_mut(&update_id) else {
                continue;
            };

            // can assert and unwrap here, because priority updates can only be added to
            // HttpStreams in [Http3Connection::queue_update_priority}
            debug_assert!(matches!(
                update_stream.stream_type(),
                Http3StreamType::Http | Http3StreamType::Push
            ));
            let stream = update_stream.http_stream().unwrap();

            // in case multiple priority_updates were issued, ignore now irrelevant
            if let Some(hframe) = stream.priority_update_frame() {
                let mut enc = Encoder::new();
                hframe.encode(&mut enc);
                if self.stream.send_atomic(conn, enc.as_ref())? {
                    stream.priority_update_sent();
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
        self.stream.init(conn.stream_create(StreamType::UniDi)?);
        self.stream
            .buffer(&[u8::try_from(HTTP3_UNI_STREAM_TYPE_CONTROL).unwrap()]);
        Ok(())
    }

    #[must_use]
    pub fn stream_id(&self) -> Option<StreamId> {
        (&self.stream).into()
    }
}
