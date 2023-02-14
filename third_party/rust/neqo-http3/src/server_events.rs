// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::connection::{Http3State, WebTransportSessionAcceptAction};
use crate::connection_server::Http3ServerHandler;
use crate::{
    features::extended_connect::SessionCloseReason, Http3StreamInfo, Http3StreamType, Priority, Res,
};
use neqo_common::{qdebug, qinfo, Encoder, Header};
use neqo_transport::server::ActiveConnectionRef;
use neqo_transport::{AppError, Connection, DatagramTracking, StreamId, StreamType};

use std::cell::RefCell;
use std::collections::VecDeque;
use std::convert::TryFrom;
use std::ops::{Deref, DerefMut};
use std::rc::Rc;

#[derive(Debug, Clone)]
pub struct StreamHandler {
    pub conn: ActiveConnectionRef,
    pub handler: Rc<RefCell<Http3ServerHandler>>,
    pub stream_info: Http3StreamInfo,
}

impl ::std::fmt::Display for StreamHandler {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        let conn: &Connection = &self.conn.borrow();
        write!(f, "conn={} stream_info={:?}", conn, self.stream_info)
    }
}

impl std::hash::Hash for StreamHandler {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.conn.hash(state);
        state.write_u64(self.stream_info.stream_id().as_u64());
        state.finish();
    }
}

impl PartialEq for StreamHandler {
    fn eq(&self, other: &Self) -> bool {
        self.conn == other.conn && self.stream_info.stream_id() == other.stream_info.stream_id()
    }
}

impl Eq for StreamHandler {}

impl StreamHandler {
    pub fn stream_id(&self) -> StreamId {
        self.stream_info.stream_id()
    }

    /// Supply a response header to a request.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn send_headers(&mut self, headers: &[Header]) -> Res<()> {
        self.handler.borrow_mut().send_headers(
            self.stream_id(),
            headers,
            &mut self.conn.borrow_mut(),
        )
    }

    /// Supply response data to a request.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn send_data(&mut self, buf: &[u8]) -> Res<usize> {
        self.handler
            .borrow_mut()
            .send_data(self.stream_id(), buf, &mut self.conn.borrow_mut())
    }

    /// Close sending side.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn stream_close_send(&mut self) -> Res<()> {
        self.handler
            .borrow_mut()
            .stream_close_send(self.stream_id(), &mut self.conn.borrow_mut())
    }

    /// Request a peer to stop sending a stream.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn stream_stop_sending(&mut self, app_error: AppError) -> Res<()> {
        qdebug!(
            [self],
            "stop sending stream_id:{} error:{}.",
            self.stream_info.stream_id(),
            app_error
        );
        self.handler.borrow_mut().stream_stop_sending(
            self.stream_info.stream_id(),
            app_error,
            &mut self.conn.borrow_mut(),
        )
    }

    /// Reset sending side of a stream.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn stream_reset_send(&mut self, app_error: AppError) -> Res<()> {
        qdebug!(
            [self],
            "reset send stream_id:{} error:{}.",
            self.stream_info.stream_id(),
            app_error
        );
        self.handler.borrow_mut().stream_reset_send(
            self.stream_info.stream_id(),
            app_error,
            &mut self.conn.borrow_mut(),
        )
    }

    /// Reset a stream/request.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore
    pub fn cancel_fetch(&mut self, app_error: AppError) -> Res<()> {
        qdebug!([self], "reset error:{}.", app_error);
        self.handler.borrow_mut().cancel_fetch(
            self.stream_info.stream_id(),
            app_error,
            &mut self.conn.borrow_mut(),
        )
    }
}

#[derive(Debug, Clone)]
pub struct Http3OrWebTransportStream {
    stream_handler: StreamHandler,
}

impl ::std::fmt::Display for Http3OrWebTransportStream {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Stream server {:?}", self.stream_handler)
    }
}

impl Http3OrWebTransportStream {
    pub(crate) fn new(
        conn: ActiveConnectionRef,
        handler: Rc<RefCell<Http3ServerHandler>>,
        stream_info: Http3StreamInfo,
    ) -> Self {
        Self {
            stream_handler: StreamHandler {
                conn,
                handler,
                stream_info,
            },
        }
    }

    /// Supply a response header to a request.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn send_headers(&mut self, headers: &[Header]) -> Res<()> {
        self.stream_handler.send_headers(headers)
    }

    /// Supply response data to a request.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn send_data(&mut self, data: &[u8]) -> Res<usize> {
        qinfo!([self], "Set new response.");
        self.stream_handler.send_data(data)
    }

    /// Close sending side.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn stream_close_send(&mut self) -> Res<()> {
        qinfo!([self], "Set new response.");
        self.stream_handler.stream_close_send()
    }
}

impl Deref for Http3OrWebTransportStream {
    type Target = StreamHandler;
    #[must_use]
    fn deref(&self) -> &Self::Target {
        &self.stream_handler
    }
}

impl DerefMut for Http3OrWebTransportStream {
    fn deref_mut(&mut self) -> &mut StreamHandler {
        &mut self.stream_handler
    }
}

impl std::hash::Hash for Http3OrWebTransportStream {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.stream_handler.hash(state);
        state.finish();
    }
}

impl PartialEq for Http3OrWebTransportStream {
    fn eq(&self, other: &Self) -> bool {
        self.stream_handler == other.stream_handler
    }
}

impl Eq for Http3OrWebTransportStream {}

#[derive(Debug, Clone)]
pub struct WebTransportRequest {
    stream_handler: StreamHandler,
}

impl ::std::fmt::Display for WebTransportRequest {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "WebTransport session {}", self.stream_handler)
    }
}

impl WebTransportRequest {
    pub(crate) fn new(
        conn: ActiveConnectionRef,
        handler: Rc<RefCell<Http3ServerHandler>>,
        stream_id: StreamId,
    ) -> Self {
        Self {
            stream_handler: StreamHandler {
                conn,
                handler,
                stream_info: Http3StreamInfo::new(stream_id, Http3StreamType::Http),
            },
        }
    }

    #[must_use]
    pub fn state(&self) -> Http3State {
        self.stream_handler.handler.borrow().state()
    }

    /// Respond to a `WebTransport` session request.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn response(&mut self, accept: &WebTransportSessionAcceptAction) -> Res<()> {
        qinfo!([self], "Set a response for a WebTransport session.");
        self.stream_handler
            .handler
            .borrow_mut()
            .webtransport_session_accept(
                &mut self.stream_handler.conn.borrow_mut(),
                self.stream_handler.stream_info.stream_id(),
                accept,
            )
    }

    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    /// Also return an error if the stream was closed on the transport layer,
    /// but that information is not yet consumed on the  http/3 layer.
    pub fn close_session(&mut self, error: u32, message: &str) -> Res<()> {
        self.stream_handler
            .handler
            .borrow_mut()
            .webtransport_close_session(
                &mut self.stream_handler.conn.borrow_mut(),
                self.stream_handler.stream_info.stream_id(),
                error,
                message,
            )
    }

    #[must_use]
    pub fn stream_id(&self) -> StreamId {
        self.stream_handler.stream_id()
    }

    /// Close sending side.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    pub fn create_stream(&mut self, stream_type: StreamType) -> Res<Http3OrWebTransportStream> {
        let session_id = self.stream_handler.stream_id();
        let id = self
            .stream_handler
            .handler
            .borrow_mut()
            .webtransport_create_stream(
                &mut self.stream_handler.conn.borrow_mut(),
                session_id,
                stream_type,
            )?;

        Ok(Http3OrWebTransportStream::new(
            self.stream_handler.conn.clone(),
            self.stream_handler.handler.clone(),
            Http3StreamInfo::new(id, Http3StreamType::WebTransport(session_id)),
        ))
    }

    /// Send `WebTransport` datagram.
    /// # Errors
    /// It may return `InvalidStreamId` if a stream does not exist anymore.
    /// The function returns `TooMuchData` if the supply buffer is bigger than
    /// the allowed remote datagram size.
    pub fn send_datagram(&mut self, buf: &[u8], id: impl Into<DatagramTracking>) -> Res<()> {
        let session_id = self.stream_handler.stream_id();
        self.stream_handler
            .handler
            .borrow_mut()
            .webtransport_send_datagram(
                &mut self.stream_handler.conn.borrow_mut(),
                session_id,
                buf,
                id,
            )
    }

    #[must_use]
    pub fn remote_datagram_size(&self) -> u64 {
        self.stream_handler.conn.borrow().remote_datagram_size()
    }

    /// Returns the current max size of a datagram that can fit into a packet.
    /// The value will change over time depending on the encoded size of the
    /// packet number, ack frames, etc.
    /// # Errors
    /// The function returns `NotAvailable` if datagrams are not enabled.
    /// # Panics
    /// This cannot panic. The max varint length is 8.
    pub fn max_datagram_size(&self) -> Res<u64> {
        let max_size = self.stream_handler.conn.borrow().max_datagram_size()?;
        Ok(max_size
            - u64::try_from(Encoder::varint_len(
                self.stream_handler.stream_id().as_u64(),
            ))
            .unwrap())
    }
}

impl Deref for WebTransportRequest {
    type Target = StreamHandler;
    #[must_use]
    fn deref(&self) -> &Self::Target {
        &self.stream_handler
    }
}

impl DerefMut for WebTransportRequest {
    fn deref_mut(&mut self) -> &mut StreamHandler {
        &mut self.stream_handler
    }
}

impl std::hash::Hash for WebTransportRequest {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.stream_handler.hash(state);
        state.finish();
    }
}

impl PartialEq for WebTransportRequest {
    fn eq(&self, other: &Self) -> bool {
        self.stream_handler == other.stream_handler
    }
}

impl Eq for WebTransportRequest {}

#[derive(Debug, Clone)]
pub enum WebTransportServerEvent {
    NewSession {
        session: WebTransportRequest,
        headers: Vec<Header>,
    },
    SessionClosed {
        session: WebTransportRequest,
        reason: SessionCloseReason,
        headers: Option<Vec<Header>>,
    },
    NewStream(Http3OrWebTransportStream),
    Datagram {
        session: WebTransportRequest,
        datagram: Vec<u8>,
    },
}

#[derive(Debug, Clone)]
pub enum Http3ServerEvent {
    /// Headers are ready.
    Headers {
        stream: Http3OrWebTransportStream,
        headers: Vec<Header>,
        fin: bool,
    },
    /// Request data is ready.
    Data {
        stream: Http3OrWebTransportStream,
        data: Vec<u8>,
        fin: bool,
    },
    DataWritable {
        stream: Http3OrWebTransportStream,
    },
    StreamReset {
        stream: Http3OrWebTransportStream,
        error: AppError,
    },
    StreamStopSending {
        stream: Http3OrWebTransportStream,
        error: AppError,
    },
    /// When individual connection change state. It is only used for tests.
    StateChange {
        conn: ActiveConnectionRef,
        state: Http3State,
    },
    PriorityUpdate {
        stream_id: StreamId,
        priority: Priority,
    },
    WebTransport(WebTransportServerEvent),
}

#[derive(Debug, Default, Clone)]
pub struct Http3ServerEvents {
    events: Rc<RefCell<VecDeque<Http3ServerEvent>>>,
}

impl Http3ServerEvents {
    fn insert(&self, event: Http3ServerEvent) {
        self.events.borrow_mut().push_back(event);
    }

    /// Take all events
    pub fn events(&self) -> impl Iterator<Item = Http3ServerEvent> {
        self.events.replace(VecDeque::new()).into_iter()
    }

    /// Whether there is request pending.
    pub fn has_events(&self) -> bool {
        !self.events.borrow().is_empty()
    }

    /// Take the next event if present.
    pub fn next_event(&self) -> Option<Http3ServerEvent> {
        self.events.borrow_mut().pop_front()
    }

    /// Insert a `Headers` event.
    pub(crate) fn headers(
        &self,
        request: Http3OrWebTransportStream,
        headers: Vec<Header>,
        fin: bool,
    ) {
        self.insert(Http3ServerEvent::Headers {
            stream: request,
            headers,
            fin,
        });
    }

    /// Insert a `StateChange` event.
    pub(crate) fn connection_state_change(&self, conn: ActiveConnectionRef, state: Http3State) {
        self.insert(Http3ServerEvent::StateChange { conn, state });
    }

    /// Insert a `Data` event.
    pub(crate) fn data(
        &self,
        conn: ActiveConnectionRef,
        handler: Rc<RefCell<Http3ServerHandler>>,
        stream_info: Http3StreamInfo,
        data: Vec<u8>,
        fin: bool,
    ) {
        self.insert(Http3ServerEvent::Data {
            stream: Http3OrWebTransportStream::new(conn, handler, stream_info),
            data,
            fin,
        });
    }

    pub(crate) fn data_writable(
        &self,
        conn: ActiveConnectionRef,
        handler: Rc<RefCell<Http3ServerHandler>>,
        stream_info: Http3StreamInfo,
    ) {
        self.insert(Http3ServerEvent::DataWritable {
            stream: Http3OrWebTransportStream::new(conn, handler, stream_info),
        });
    }

    pub(crate) fn stream_reset(
        &self,
        conn: ActiveConnectionRef,
        handler: Rc<RefCell<Http3ServerHandler>>,
        stream_info: Http3StreamInfo,
        error: AppError,
    ) {
        self.insert(Http3ServerEvent::StreamReset {
            stream: Http3OrWebTransportStream::new(conn, handler, stream_info),
            error,
        });
    }

    pub(crate) fn stream_stop_sending(
        &self,
        conn: ActiveConnectionRef,
        handler: Rc<RefCell<Http3ServerHandler>>,
        stream_info: Http3StreamInfo,
        error: AppError,
    ) {
        self.insert(Http3ServerEvent::StreamStopSending {
            stream: Http3OrWebTransportStream::new(conn, handler, stream_info),
            error,
        });
    }

    pub(crate) fn priority_update(&self, stream_id: StreamId, priority: Priority) {
        self.insert(Http3ServerEvent::PriorityUpdate {
            stream_id,
            priority,
        });
    }

    pub(crate) fn webtransport_new_session(
        &self,
        session: WebTransportRequest,
        headers: Vec<Header>,
    ) {
        self.insert(Http3ServerEvent::WebTransport(
            WebTransportServerEvent::NewSession { session, headers },
        ));
    }

    pub(crate) fn webtransport_session_closed(
        &self,
        session: WebTransportRequest,
        reason: SessionCloseReason,
        headers: Option<Vec<Header>>,
    ) {
        self.insert(Http3ServerEvent::WebTransport(
            WebTransportServerEvent::SessionClosed {
                session,
                reason,
                headers,
            },
        ));
    }

    pub(crate) fn webtransport_new_stream(&self, stream: Http3OrWebTransportStream) {
        self.insert(Http3ServerEvent::WebTransport(
            WebTransportServerEvent::NewStream(stream),
        ));
    }

    pub(crate) fn webtransport_datagram(&self, session: WebTransportRequest, datagram: Vec<u8>) {
        self.insert(Http3ServerEvent::WebTransport(
            WebTransportServerEvent::Datagram { session, datagram },
        ));
    }
}
