// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::{HFrame, HFrameReader};
use crate::{Error, Res};
use neqo_common::{qdebug, qinfo};
use neqo_transport::Connection;

/// The remote control stream is responsible only for reading frames. The frames are handled by `Http3Connection`.
#[derive(Debug)]
pub(crate) struct ControlStreamRemote {
    stream_id: Option<u64>,
    frame_reader: HFrameReader,
    fin: bool,
}

impl ::std::fmt::Display for ControlStreamRemote {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 remote control stream {:?}", self.stream_id)
    }
}

impl ControlStreamRemote {
    pub fn new() -> Self {
        Self {
            stream_id: None,
            frame_reader: HFrameReader::new(),
            fin: false,
        }
    }

    /// A remote control stream has been received. Inform `ControlStreamRemote`.
    pub fn add_remote_stream(&mut self, stream_id: u64) -> Res<()> {
        qinfo!([self], "A new control stream {}.", stream_id);
        if self.stream_id.is_some() {
            qdebug!([self], "A control stream already exists");
            return Err(Error::HttpStreamCreation);
        }
        self.stream_id = Some(stream_id);
        Ok(())
    }

    /// Check if `stream_id` is the remote control stream.
    pub fn is_recv_stream(&self, stream_id: u64) -> bool {
        matches!(self.stream_id, Some(id) if id == stream_id)
    }

    /// Check if a stream is the control stream and read received data.
    pub fn receive(&mut self, conn: &mut Connection) -> Res<Option<HFrame>> {
        assert!(self.stream_id.is_some());
        qdebug!([self], "Receiving data.");
        match self.frame_reader.receive(conn, self.stream_id.unwrap())? {
            (_, true) => {
                self.fin = true;
                Err(Error::HttpClosedCriticalStream)
            }
            (s, false) => Ok(s),
        }
    }

    #[must_use]
    pub fn stream_id(&self) -> Option<u64> {
        self.stream_id
    }
}
