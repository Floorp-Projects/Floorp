// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::{HFrame, HFrameReader};
use crate::{Error, Res};
use neqo_common::{qdebug, qinfo};
use neqo_transport::Connection;

// The remote control stream is responsible only for reading frames. The frames are handled by Http3Connection
#[derive(Debug)]
pub struct ControlStreamRemote {
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
    pub fn new() -> ControlStreamRemote {
        ControlStreamRemote {
            stream_id: None,
            frame_reader: HFrameReader::new(),
            fin: false,
        }
    }

    pub fn add_remote_stream(&mut self, stream_id: u64) -> Res<()> {
        qinfo!([self] "A new control stream {}.", stream_id);
        if self.stream_id.is_some() {
            qdebug!([self] "A control stream already exists");
            return Err(Error::HttpStreamCreationError);
        }
        self.stream_id = Some(stream_id);
        Ok(())
    }

    pub fn receive_if_this_stream(&mut self, conn: &mut Connection, stream_id: u64) -> Res<bool> {
        if let Some(id) = self.stream_id {
            if id == stream_id {
                qdebug!([self] "Receiving data.");
                self.fin = self.frame_reader.receive(conn, stream_id)?;
                return Ok(true);
            }
        }
        Ok(false)
    }

    pub fn recvd_fin(&self) -> bool {
        self.fin
    }

    pub fn frame_reader_done(&self) -> bool {
        self.frame_reader.done()
    }

    pub fn get_frame(&mut self) -> Res<HFrame> {
        self.frame_reader.get_frame()
    }
}
