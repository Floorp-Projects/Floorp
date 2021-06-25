// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    AppError, Error, Http3StreamType, HttpRecvStream, ReceiveOutput, RecvStream, Res, ResetType,
};
use neqo_qpack::QPackDecoder;
use neqo_transport::Connection;
use std::cell::RefCell;
use std::rc::Rc;

#[derive(Debug)]
pub struct DecoderRecvStream {
    stream_id: u64,
    decoder: Rc<RefCell<QPackDecoder>>,
}

impl DecoderRecvStream {
    pub fn new(stream_id: u64, decoder: Rc<RefCell<QPackDecoder>>) -> Self {
        Self { stream_id, decoder }
    }
}

impl RecvStream for DecoderRecvStream {
    fn stream_reset(&mut self, _error: AppError, _reset_type: ResetType) -> Res<()> {
        Err(Error::HttpClosedCriticalStream)
    }

    fn receive(&mut self, conn: &mut Connection) -> Res<ReceiveOutput> {
        Ok(ReceiveOutput::UnblockedStreams(
            self.decoder.borrow_mut().receive(conn, self.stream_id)?,
        ))
    }

    fn done(&self) -> bool {
        false
    }

    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::Decoder
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpRecvStream> {
        None
    }
}
