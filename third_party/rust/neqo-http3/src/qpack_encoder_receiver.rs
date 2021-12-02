// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    AppError, Error, Http3StreamType, HttpRecvStream, ReceiveOutput, RecvStream, Res, ResetType,
};
use neqo_qpack::QPackEncoder;
use neqo_transport::Connection;
use std::cell::RefCell;
use std::rc::Rc;

#[derive(Debug)]
pub struct EncoderRecvStream {
    stream_id: u64,
    encoder: Rc<RefCell<QPackEncoder>>,
}

impl EncoderRecvStream {
    pub fn new(stream_id: u64, encoder: Rc<RefCell<QPackEncoder>>) -> Self {
        Self { stream_id, encoder }
    }
}

impl RecvStream for EncoderRecvStream {
    fn stream_reset(&mut self, _error: AppError, _reset_type: ResetType) -> Res<()> {
        Err(Error::HttpClosedCriticalStream)
    }

    fn receive(&mut self, conn: &mut Connection) -> Res<ReceiveOutput> {
        self.encoder.borrow_mut().receive(conn, self.stream_id)?;
        Ok(ReceiveOutput::NoOutput)
    }

    fn done(&self) -> bool {
        false
    }

    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::Encoder
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpRecvStream> {
        None
    }
}
