// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::{HFrame, HFrameReader};
use crate::{
    AppError, Error, Http3StreamType, HttpRecvStream, ReceiveOutput, RecvStream, Res, ResetType,
};
use neqo_common::qdebug;
use neqo_transport::Connection;

/// The remote control stream is responsible only for reading frames. The frames are handled by `Http3Connection`.
#[derive(Debug)]
pub(crate) struct ControlStreamRemote {
    stream_id: u64,
    frame_reader: HFrameReader,
}

impl ::std::fmt::Display for ControlStreamRemote {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 remote control stream {:?}", self.stream_id)
    }
}

impl ControlStreamRemote {
    pub fn new(stream_id: u64) -> Self {
        Self {
            stream_id,
            frame_reader: HFrameReader::new(),
        }
    }

    /// Check if a stream is the control stream and read received data.
    pub fn receive_single(&mut self, conn: &mut Connection) -> Res<Option<HFrame>> {
        qdebug!([self], "Receiving data.");
        match self.frame_reader.receive(conn, self.stream_id)? {
            (_, true) => Err(Error::HttpClosedCriticalStream),
            (s, false) => Ok(s),
        }
    }
}

impl RecvStream for ControlStreamRemote {
    fn stream_reset(&mut self, _error: AppError, _reset_type: ResetType) -> Res<()> {
        Err(Error::HttpClosedCriticalStream)
    }

    fn receive(&mut self, conn: &mut Connection) -> Res<ReceiveOutput> {
        let mut control_frames = Vec::new();

        loop {
            if let Some(f) = self.receive_single(conn)? {
                control_frames.push(f);
            } else {
                return Ok(ReceiveOutput::ControlFrames(control_frames));
            }
        }
    }

    fn done(&self) -> bool {
        false
    }

    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::Control
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpRecvStream> {
        None
    }
}
