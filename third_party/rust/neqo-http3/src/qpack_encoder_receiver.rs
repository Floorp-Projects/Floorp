// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{CloseType, Error, Http3StreamType, ReceiveOutput, RecvStream, Res, Stream};
use neqo_qpack::QPackEncoder;
use neqo_transport::{Connection, StreamId};
use std::cell::RefCell;
use std::rc::Rc;

#[derive(Debug)]
pub struct EncoderRecvStream {
    stream_id: StreamId,
    encoder: Rc<RefCell<QPackEncoder>>,
}

impl EncoderRecvStream {
    pub fn new(stream_id: StreamId, encoder: Rc<RefCell<QPackEncoder>>) -> Self {
        Self { stream_id, encoder }
    }
}

impl Stream for EncoderRecvStream {
    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::Encoder
    }
}

impl RecvStream for EncoderRecvStream {
    fn reset(&mut self, _close_type: CloseType) -> Res<()> {
        Err(Error::HttpClosedCriticalStream)
    }

    fn receive(&mut self, conn: &mut Connection) -> Res<(ReceiveOutput, bool)> {
        self.encoder.borrow_mut().receive(conn, self.stream_id)?;
        Ok((ReceiveOutput::NoOutput, false))
    }
}
