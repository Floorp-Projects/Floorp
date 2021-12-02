// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::priority::PriorityHandler;
use crate::push_controller::{PushController, RecvPushEvents};
use crate::recv_message::{MessageType, RecvMessage};
use crate::{Http3StreamType, HttpRecvStream, Priority, ReceiveOutput, RecvStream, Res, ResetType};
use neqo_qpack::decoder::QPackDecoder;
use neqo_transport::{AppError, Connection};
use std::cell::RefCell;
use std::fmt::Display;
use std::rc::Rc;

// The `PushController` keeps information about all push streams. Each push stream is responsible for contacting the
// `PushController` to consult it about the push state and to inform it when the push stream is done (this are signal
// from the peer: stream has been closed or reset). `PushController` handles CANCEL_PUSH frames and canceling
// push from applications and PUSH_PROMISE frames.
//
// `PushStream` is responsible for reading from a push stream.
// It is created when a new push stream is received.
//
// `PushStreams` are kept in Http3Connection::recv_streams the same as a normal request/response stream.
// Http3Connection and read_data is responsible for reading the push data.
//
// PushHeaderReady and PushDataReadable are posted through the `PushController` that may decide to postpone them if
// a push_promise has not been yet received for the stream.
//
// `PushStream` is responsible for removing itself from the `PushController`.
//
// `PushStream` may be reset from the peer in the same way as a request stream. The `PushStream` informs the
// `PushController` that will set the push state to closed and remove any push events.

#[derive(Debug)]
pub(crate) struct PushStream {
    stream_id: u64,
    push_id: u64,
    response: RecvMessage,
    push_handler: Rc<RefCell<PushController>>,
}

impl PushStream {
    pub fn new(
        stream_id: u64,
        push_id: u64,
        push_handler: Rc<RefCell<PushController>>,
        qpack_decoder: Rc<RefCell<QPackDecoder>>,
        priority: Priority,
    ) -> Self {
        Self {
            response: RecvMessage::new(
                MessageType::Response,
                stream_id,
                qpack_decoder,
                Box::new(RecvPushEvents::new(push_id, push_handler.clone())),
                None,
                PriorityHandler::new(true, priority),
            ),
            stream_id,
            push_id,
            push_handler,
        }
    }
}

impl Display for PushStream {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(
            f,
            "Push stream {:?} push_id={}",
            self.stream_id, self.push_id
        )
    }
}

impl RecvStream for PushStream {
    fn receive(&mut self, conn: &mut Connection) -> Res<ReceiveOutput> {
        self.response.receive(conn)?;
        if self.response.done() {
            self.push_handler.borrow_mut().close(self.push_id);
        }
        Ok(ReceiveOutput::NoOutput)
    }

    fn done(&self) -> bool {
        self.response.done()
    }

    fn stream_reset(&mut self, app_error: AppError, reset_type: ResetType) -> Res<()> {
        match reset_type {
            ResetType::App => {}
            t => {
                self.push_handler
                    .borrow_mut()
                    .push_stream_reset(self.push_id, app_error, t);
            }
        }
        self.response.stream_reset(app_error, reset_type)?;
        Ok(())
    }

    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::Push
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpRecvStream> {
        Some(self)
    }
}

impl HttpRecvStream for PushStream {
    fn header_unblocked(&mut self, conn: &mut Connection) -> Res<()> {
        self.receive(conn)?;
        Ok(())
    }

    fn read_data(&mut self, conn: &mut Connection, buf: &mut [u8]) -> Res<(usize, bool)> {
        let res = self.response.read_data(conn, buf);
        if self.response.done() {
            self.push_handler.borrow_mut().close(self.push_id);
        }
        res
    }

    fn priority_handler_mut(&mut self) -> &mut PriorityHandler {
        self.response.priority_handler_mut()
    }
}
