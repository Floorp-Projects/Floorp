// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::client_events::Http3ClientEvents;
use crate::push_controller::{PushController, RecvPushEvents};
use crate::recv_message::{MessageType, RecvMessage};
use crate::{Error, Http3StreamType, HttpRecvStream, ReceiveOutput, RecvStream, Res, ResetType};
use neqo_common::{Decoder, IncrementalDecoderUint};
use neqo_qpack::decoder::QPackDecoder;
use neqo_transport::{AppError, Connection};
use std::cell::RefCell;
use std::fmt::Display;
use std::mem;
use std::rc::Rc;

#[derive(Debug)]
enum PushStreamState {
    ReadPushId(IncrementalDecoderUint),
    ReadResponse { push_id: u64, response: RecvMessage },
    Closed,
}

impl PushStreamState {
    pub fn push_id(&self) -> Option<u64> {
        match self {
            Self::ReadResponse { push_id, .. } => Some(*push_id),
            _ => None,
        }
    }
}

// The `PushController` keeps information about all push streams. Each push stream is responsible for contacting the
// `PushController` to consult it about the push state and to inform it when the push stream is done (this are signal
// from the peer: stream has been closed or reset). `PushController` handles CANCEL_PUSH frames and canceling
// push from applications and PUSH_PROMISE frames.
//
// `PushStream` is responsible for reading from a push stream. It is used for reading push_id as well.
// It is created when a new push stream is received.
//
// After push_id has been read, The `PushController` is informed about the stream and its push_id. This is done by
// calling `add_new_push_stream`. `add_new_push_stream` may return an error (this will be a connection error)
// or a bool. true means that the streams should continue and false means that the stream should be reset(the stream
// will be canceled if the push has been canceled already (CANCEL_PUSH frame or canceling push from the application)
//
// `PushStreams` are kept in Http3Connection::recv_streams the same as a normal request/response stream.
// Http3Connection and read_data is responsible for reading the push data.
//
// PushHeaderReady and PushDataReadable are posted through the `PushController` that may decide to postpone them if
// a push_promise has not been received for the stream.
//
// `PushStream` is responsible for removing itself from the `PushController`.
//
// `PushStream` may be reset from the peer in the same way as a request stream. The `PushStream` informs the
// `PushController` that will set the push state to closed and remove any push events.

#[derive(Debug)]
pub(crate) struct PushStream {
    state: PushStreamState,
    stream_id: u64,
    push_handler: Rc<RefCell<PushController>>,
    qpack_decoder: Rc<RefCell<QPackDecoder>>,
    events: Http3ClientEvents,
}

impl PushStream {
    pub fn new(
        stream_id: u64,
        push_handler: Rc<RefCell<PushController>>,
        qpack_decoder: Rc<RefCell<QPackDecoder>>,
        events: Http3ClientEvents,
    ) -> Self {
        Self {
            state: PushStreamState::ReadPushId(IncrementalDecoderUint::default()),
            stream_id,
            push_handler,
            qpack_decoder,
            events,
        }
    }

    fn push_id_decoded(&mut self, push_id: u64, conn: &mut Connection) -> Res<()> {
        if self
            .push_handler
            .borrow_mut()
            .add_new_push_stream(push_id, self.stream_id)?
        {
            self.state = PushStreamState::ReadResponse {
                push_id,
                response: RecvMessage::new(
                    MessageType::Response,
                    self.stream_id,
                    Rc::clone(&self.qpack_decoder),
                    Box::new(RecvPushEvents::new(push_id, Rc::clone(&self.push_handler))),
                    None,
                ),
            };
        } else {
            mem::drop(conn.stream_stop_sending(self.stream_id, Error::HttpRequestCancelled.code()));
            self.state = PushStreamState::Closed;
        }
        Ok(())
    }
}

impl Display for PushStream {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Push stream {:?}", self.stream_id)
    }
}

impl RecvStream for PushStream {
    fn receive(&mut self, conn: &mut Connection) -> Res<ReceiveOutput> {
        loop {
            match &mut self.state {
                PushStreamState::ReadPushId(id_reader) => {
                    let to_read = id_reader.min_remaining();
                    let mut buf = vec![0; to_read];
                    match conn.stream_recv(self.stream_id, &mut buf[..])? {
                        (_, true) => {
                            self.state = PushStreamState::Closed;
                            return Err(Error::HttpGeneralProtocol);
                        }
                        (0, false) => return Ok(ReceiveOutput::NoOutput),
                        (amount, false) => {
                            if let Some(p) = id_reader.consume(&mut Decoder::from(&buf[..amount])) {
                                self.push_id_decoded(p, conn)?;
                                if self.done() {
                                    return Ok(ReceiveOutput::NoOutput);
                                }
                            }
                        }
                    }
                }
                PushStreamState::ReadResponse { response, push_id } => {
                    response.receive(conn)?;
                    if response.done() {
                        self.push_handler.borrow_mut().close(*push_id);
                        self.state = PushStreamState::Closed;
                    }
                    return Ok(ReceiveOutput::NoOutput);
                }
                PushStreamState::Closed => return Ok(ReceiveOutput::NoOutput),
            }
        }
    }

    fn done(&self) -> bool {
        matches!(self.state, PushStreamState::Closed)
    }

    fn stream_reset(&mut self, app_error: AppError, reset_type: ResetType) -> Res<()> {
        if !self.done() {
            self.qpack_decoder
                .borrow_mut()
                .cancel_stream(self.stream_id);
        }
        match reset_type {
            ResetType::App => {}
            t => {
                if let Some(push_id) = self.state.push_id() {
                    self.push_handler
                        .borrow_mut()
                        .push_stream_reset(push_id, app_error, t);
                }
            }
        }
        self.state = PushStreamState::Closed;
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
        if let PushStreamState::ReadResponse { response, push_id } = &mut self.state {
            let res = response.read_data(conn, buf);
            if response.done() {
                self.push_handler.borrow_mut().close(*push_id);
            }
            res
        } else {
            Err(Error::InvalidStreamId)
        }
    }
}
