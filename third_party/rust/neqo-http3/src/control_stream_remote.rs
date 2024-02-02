// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_common::qdebug;
use neqo_transport::{Connection, StreamId};

use crate::{
    frames::{FrameReader, HFrame, StreamReaderConnectionWrapper},
    CloseType, Error, Http3StreamType, ReceiveOutput, RecvStream, Res, Stream,
};

/// The remote control stream is responsible only for reading frames. The frames are handled by
/// `Http3Connection`.
#[derive(Debug)]
pub(crate) struct ControlStreamRemote {
    stream_id: StreamId,
    frame_reader: FrameReader,
}

impl ::std::fmt::Display for ControlStreamRemote {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 remote control stream {:?}", self.stream_id)
    }
}

impl ControlStreamRemote {
    pub fn new(stream_id: StreamId) -> Self {
        Self {
            stream_id,
            frame_reader: FrameReader::new(),
        }
    }

    /// Check if a stream is the control stream and read received data.
    pub fn receive_single(&mut self, conn: &mut Connection) -> Res<Option<HFrame>> {
        qdebug!([self], "Receiving data.");
        match self
            .frame_reader
            .receive(&mut StreamReaderConnectionWrapper::new(
                conn,
                self.stream_id,
            ))? {
            (_, true) => Err(Error::HttpClosedCriticalStream),
            (s, false) => {
                qdebug!([self], "received {:?}", s);
                Ok(s)
            }
        }
    }
}

impl Stream for ControlStreamRemote {
    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::Control
    }
}

impl RecvStream for ControlStreamRemote {
    fn reset(&mut self, _close_type: CloseType) -> Res<()> {
        Err(Error::HttpClosedCriticalStream)
    }

    #[allow(clippy::vec_init_then_push)] // Clippy fail.
    fn receive(&mut self, conn: &mut Connection) -> Res<(ReceiveOutput, bool)> {
        let mut control_frames = Vec::new();

        loop {
            if let Some(f) = self.receive_single(conn)? {
                control_frames.push(f);
            } else {
                return Ok((ReceiveOutput::ControlFrames(control_frames), false));
            }
        }
    }
}
