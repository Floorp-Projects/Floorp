// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::client_events::{Http3ClientEvent, Http3ClientEvents};
use crate::connection::{Http3Connection, Http3State, RequestDescription};
use crate::frames::HFrame;
use crate::push_controller::{PushController, RecvPushEvents};
use crate::recv_message::{RecvMessage, RecvMessageInfo};
use crate::request_target::AsRequestTarget;
use crate::settings::HSettings;
use crate::{
    Http3Parameters, Http3StreamType, NewStreamType, Priority, PriorityHandler, ReceiveOutput,
};
use neqo_common::{
    event::Provider as EventProvider, hex, hex_with_len, qdebug, qinfo, qlog::NeqoQlog, qtrace,
    Datagram, Decoder, Encoder, Header, MessageType, Role,
};
use neqo_crypto::{agent::CertificateInfo, AuthenticationStatus, ResumptionToken, SecretAgentInfo};
use neqo_qpack::Stats as QpackStats;
use neqo_transport::{
    AppError, Connection, ConnectionEvent, ConnectionId, ConnectionIdGenerator, Output,
    QuicVersion, Stats as TransportStats, StreamId, StreamType, ZeroRttState,
};
use std::cell::RefCell;
use std::fmt::Debug;
use std::fmt::Display;
use std::mem;
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::Instant;

use crate::{Error, Res};

// This is used for filtering send_streams and recv_Streams with a stream_ids greater than or equal a given id.
// Only the same type (bidirectional or unidirectionsl) streams are filtered.
fn id_gte<U>(base: StreamId) -> impl FnMut((&StreamId, &U)) -> Option<StreamId> + 'static
where
    U: ?Sized,
{
    move |(id, _)| {
        if *id >= base && !(id.is_bidi() ^ base.is_bidi()) {
            Some(*id)
        } else {
            None
        }
    }
}

fn alpn_from_quic_version(version: QuicVersion) -> &'static str {
    match version {
        QuicVersion::Version1 => "h3",
        QuicVersion::Draft29 => "h3-29",
        QuicVersion::Draft30 => "h3-30",
        QuicVersion::Draft31 => "h3-31",
        QuicVersion::Draft32 => "h3-32",
    }
}

pub struct Http3Client {
    conn: Connection,
    base_handler: Http3Connection,
    events: Http3ClientEvents,
    push_handler: Rc<RefCell<PushController>>,
}

impl Display for Http3Client {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 client")
    }
}

impl Http3Client {
    /// # Errors
    /// Making a `neqo-transport::connection` may produce an error. This can only be a crypto error if
    /// the socket can't be created or configured.
    pub fn new(
        server_name: &str,
        cid_manager: Rc<RefCell<dyn ConnectionIdGenerator>>,
        local_addr: SocketAddr,
        remote_addr: SocketAddr,
        http3_parameters: Http3Parameters,
        now: Instant,
    ) -> Res<Self> {
        Ok(Self::new_with_conn(
            Connection::new_client(
                server_name,
                &[alpn_from_quic_version(
                    http3_parameters
                        .get_connection_parameters()
                        .get_quic_version(),
                )],
                cid_manager,
                local_addr,
                remote_addr,
                *http3_parameters.get_connection_parameters(),
                now,
            )?,
            http3_parameters,
        ))
    }

    #[must_use]
    pub fn new_with_conn(c: Connection, http3_parameters: Http3Parameters) -> Self {
        let events = Http3ClientEvents::default();
        let mut base_handler = Http3Connection::new(http3_parameters, Role::Client);
        if http3_parameters.get_webtransport() {
            base_handler.set_features_listener(events.clone());
        }
        Self {
            conn: c,
            events: events.clone(),
            push_handler: Rc::new(RefCell::new(PushController::new(
                http3_parameters.get_max_concurrent_push_streams(),
                events,
            ))),
            base_handler,
        }
    }

    #[must_use]
    pub fn role(&self) -> Role {
        self.conn.role()
    }

    #[must_use]
    pub fn state(&self) -> Http3State {
        self.base_handler.state()
    }

    #[must_use]
    pub fn tls_info(&self) -> Option<&SecretAgentInfo> {
        self.conn.tls_info()
    }

    /// Get the peer's certificate.
    #[must_use]
    pub fn peer_certificate(&self) -> Option<CertificateInfo> {
        self.conn.peer_certificate()
    }

    /// This called when peer certificates have been verified.
    pub fn authenticated(&mut self, status: AuthenticationStatus, now: Instant) {
        self.conn.authenticated(status, now);
    }

    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.conn.set_qlog(qlog);
    }

    /// Enable encrypted client hello (ECH).
    ///
    /// # Errors
    /// Fails when the configuration provided is bad.
    pub fn enable_ech(&mut self, ech_config_list: impl AsRef<[u8]>) -> Res<()> {
        self.conn.client_enable_ech(ech_config_list)?;
        Ok(())
    }

    /// Get the connection id, which is useful for disambiguating connections to
    /// the same origin.
    #[must_use]
    pub fn connection_id(&self) -> &ConnectionId {
        self.conn.odcid().expect("Client always has odcid")
    }

    fn encode_resumption_token(&self, token: &ResumptionToken) -> Option<ResumptionToken> {
        self.base_handler.get_settings().map(|settings| {
            let mut enc = Encoder::default();
            settings.encode_frame_contents(&mut enc);
            enc.encode(token.as_ref());
            ResumptionToken::new(enc.into(), token.expiration_time())
        })
    }

    /// Get a resumption token.  The correct way to obtain a resumption token is
    /// waiting for the `Http3ClientEvent::ResumptionToken` event.  However, some
    /// servers don't send `NEW_TOKEN` frames and so that event might be slow in
    /// arriving.  This is especially a problem for short-lived connections, where
    /// the connection is closed before any events are released.  This retrieves
    /// the token, without waiting for the `NEW_TOKEN` frame to arrive.
    pub fn take_resumption_token(&mut self, now: Instant) -> Option<ResumptionToken> {
        self.conn
            .take_resumption_token(now)
            .and_then(|t| self.encode_resumption_token(&t))
    }

    /// This may be call if an application has a resumption token. This must be called before connection starts.
    /// # Errors
    /// An error is return if token cannot be decoded or a connection is is a wrong state.
    /// # Panics
    /// On closing if the base handler can't handle it (debug only).
    pub fn enable_resumption(&mut self, now: Instant, token: impl AsRef<[u8]>) -> Res<()> {
        if self.base_handler.state != Http3State::Initializing {
            return Err(Error::InvalidState);
        }
        let mut dec = Decoder::from(token.as_ref());
        let settings_slice = match dec.decode_vvec() {
            Some(v) => v,
            None => return Err(Error::InvalidResumptionToken),
        };
        qtrace!([self], "  settings {}", hex_with_len(&settings_slice));
        let mut dec_settings = Decoder::from(settings_slice);
        let mut settings = HSettings::default();
        Error::map_error(
            settings.decode_frame_contents(&mut dec_settings),
            Error::InvalidResumptionToken,
        )?;
        let tok = dec.decode_remainder();
        qtrace!([self], "  Transport token {}", hex(&tok));
        self.conn.enable_resumption(now, tok)?;
        if self.conn.state().closed() {
            let state = self.conn.state().clone();
            let res = self
                .base_handler
                .handle_state_change(&mut self.conn, &state);
            debug_assert_eq!(Ok(true), res);
            return Err(Error::FatalError);
        }
        if *self.conn.zero_rtt_state() == ZeroRttState::Sending {
            self.base_handler
                .set_0rtt_settings(&mut self.conn, settings)?;
            self.events
                .connection_state_change(self.base_handler.state());
            self.push_handler
                .borrow_mut()
                .maybe_send_max_push_id_frame(&mut self.base_handler);
        }
        Ok(())
    }

    /// This is call to close a connection.
    pub fn close<S>(&mut self, now: Instant, error: AppError, msg: S)
    where
        S: AsRef<str> + Display,
    {
        qinfo!([self], "Close the connection error={} msg={}.", error, msg);
        if !matches!(
            self.base_handler.state,
            Http3State::Closing(_) | Http3State::Closed(_)
        ) {
            self.push_handler.borrow_mut().clear();
            self.conn.close(now, error, msg);
            self.base_handler.close(error);
            self.events
                .connection_state_change(self.base_handler.state());
        }
    }

    /// Attempt to force a key update.
    /// # Errors
    /// If the connection isn't confirmed, or there is an outstanding key update, this
    /// returns `Err(Error::TransportError(neqo_transport::Error::KeyUpdateBlocked))`.
    pub fn initiate_key_update(&mut self) -> Res<()> {
        self.conn.initiate_key_update()?;
        Ok(())
    }

    // API: Request/response

    /// This is call to make a new http request. Each request can have headers and they are added when request
    /// is created. A response body may be added by calling `send_data`.
    /// # Errors
    /// If a new stream cannot be created an error will be return.
    /// # Panics
    /// `SendMessage` implements `http_stream` so it will not panic.
    pub fn fetch<'x, 't: 'x, T>(
        &mut self,
        now: Instant,
        method: &'t str,
        target: &'t T,
        headers: &'t [Header],
        priority: Priority,
    ) -> Res<StreamId>
    where
        T: AsRequestTarget<'x> + ?Sized + Debug,
    {
        let output = self.base_handler.fetch(
            &mut self.conn,
            Box::new(self.events.clone()),
            Box::new(self.events.clone()),
            Some(Rc::clone(&self.push_handler)),
            &RequestDescription {
                method,
                connect_type: None,
                target,
                headers,
                priority,
            },
        );
        if let Err(e) = &output {
            if e.connection_error() {
                self.close(now, e.code(), "");
            }
        }
        output
    }

    /// Send an [`PRIORITY_UPDATE`-frame][1] on next `Http3Client::process_output()` call.
    /// Returns if the priority got changed.
    /// # Errors
    /// `InvalidStreamId` if the stream does not exist
    ///
    /// [1]: https://datatracker.ietf.org/doc/html/draft-kazuho-httpbis-priority-04#section-5.2
    pub fn priority_update(&mut self, stream_id: StreamId, priority: Priority) -> Res<bool> {
        self.base_handler.queue_update_priority(stream_id, priority)
    }

    /// An application may reset a stream(request).
    /// Both sides, sending and receiving side, will be closed.
    /// # Errors
    /// An error will be return if a stream does not exist.
    pub fn cancel_fetch(&mut self, stream_id: StreamId, error: AppError) -> Res<()> {
        qinfo!([self], "reset_stream {} error={}.", stream_id, error);
        self.base_handler
            .cancel_fetch(stream_id, error, &mut self.conn)
    }

    /// This is call when application is done sending a request.
    /// # Errors
    /// An error will be return if stream does not exist.
    pub fn stream_close_send(&mut self, stream_id: StreamId) -> Res<()> {
        qinfo!([self], "Close sending side stream={}.", stream_id);
        self.base_handler
            .stream_close_send(&mut self.conn, stream_id)
    }

    /// # Errors
    /// An error will be return if a stream does not exist.
    pub fn stream_reset_send(&mut self, stream_id: StreamId, error: AppError) -> Res<()> {
        qinfo!([self], "stream_reset_send {} error={}.", stream_id, error);
        self.base_handler
            .stream_reset_send(&mut self.conn, stream_id, error)
    }

    /// # Errors
    /// An error will be return if a stream does not exist.
    pub fn stream_stop_sending(&mut self, stream_id: StreamId, error: AppError) -> Res<()> {
        qinfo!([self], "stream_stop_sending {} error={}.", stream_id, error);
        self.base_handler
            .stream_stop_sending(&mut self.conn, stream_id, error)
    }

    /// To supply a request body this function is called (headers are supplied through the `fetch` function.)
    /// # Errors
    /// `InvalidStreamId` if the stream does not exist,
    /// `AlreadyClosed` if the stream has already been closed.
    /// `TransportStreamDoesNotExist` if the transport stream does not exist (this may happen if `process_output`
    /// has not been called when needed, and HTTP3 layer has not picked up the info that the stream has been closed.)
    /// `InvalidInput` if an empty buffer has been supplied.
    pub fn send_data(&mut self, stream_id: StreamId, buf: &[u8]) -> Res<usize> {
        qinfo!(
            [self],
            "send_data from stream {} sending {} bytes.",
            stream_id,
            buf.len()
        );
        self.base_handler
            .send_streams
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?
            .send_data(&mut self.conn, buf)
    }

    /// Response data are read directly into a buffer supplied as a parameter of this function to avoid copying
    /// data.
    /// # Errors
    /// It returns an error if a stream does not exist or an error happen while reading a stream, e.g.
    /// early close, protocol error, etc.
    pub fn read_data(
        &mut self,
        now: Instant,
        stream_id: StreamId,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        qinfo!([self], "read_data from stream {}.", stream_id);
        let res = self.base_handler.read_data(&mut self.conn, stream_id, buf);
        if let Err(e) = &res {
            if e.connection_error() {
                self.close(now, e.code(), "");
            }
        }
        res
    }

    // API: Push streams

    /// Cancel a push
    /// # Errors
    /// `InvalidStreamId` if the stream does not exist.
    pub fn cancel_push(&mut self, push_id: u64) -> Res<()> {
        self.push_handler
            .borrow_mut()
            .cancel(push_id, &mut self.conn, &mut self.base_handler)
    }

    /// Push response data are read directly into a buffer supplied as a parameter of this function
    /// to avoid copying data.
    /// # Errors
    /// It returns an error if a stream does not exist(`InvalidStreamId`) or an error has happened while
    /// reading a stream, e.g. early close, protocol error, etc.
    pub fn push_read_data(
        &mut self,
        now: Instant,
        push_id: u64,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        let stream_id = self
            .push_handler
            .borrow_mut()
            .get_active_stream_id(push_id)
            .ok_or(Error::InvalidStreamId)?;
        self.conn.stream_keep_alive(stream_id, true)?;
        self.read_data(now, stream_id, buf)
    }

    // API WebTransport

    /// # Errors
    /// If `WebTransport` cannot be created, e.g. the `WebTransport` support is
    /// not negotiated or the HTTP/3 connection is closed.
    pub fn webtransport_create_session<'x, 't: 'x, T>(
        &mut self,
        now: Instant,
        target: &'t T,
        headers: &'t [Header],
    ) -> Res<StreamId>
    where
        T: AsRequestTarget<'x> + ?Sized + Debug,
    {
        let output = self.base_handler.webtransport_create_session(
            &mut self.conn,
            Box::new(self.events.clone()),
            target,
            headers,
        );

        if let Err(e) = &output {
            if e.connection_error() {
                self.close(now, e.code(), "");
            }
        }
        output
    }

    /// Close `WebTransport` cleanly
    /// # Errors
    /// `InvalidStreamId` if the stream does not exist,
    /// `TransportStreamDoesNotExist` if the transport stream does not exist (this may happen if `process_output`
    /// has not been called when needed, and HTTP3 layer has not picked up the info that the stream has been closed.)
    /// `InvalidInput` if an empty buffer has been supplied.
    pub fn webtransport_close_session(
        &mut self,
        session_id: StreamId,
        error: u32,
        message: &str,
    ) -> Res<()> {
        self.base_handler
            .webtransport_close_session(&mut self.conn, session_id, error, message)
    }

    /// # Errors
    /// This may return an error if the particular session does not exist
    /// or the connection is not in the active state.
    pub fn webtransport_create_stream(
        &mut self,
        session_id: StreamId,
        stream_type: StreamType,
    ) -> Res<StreamId> {
        self.base_handler.webtransport_create_stream_local(
            &mut self.conn,
            session_id,
            stream_type,
            Box::new(self.events.clone()),
            Box::new(self.events.clone()),
        )
    }

    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        qtrace!([self], "Process.");
        if let Some(d) = dgram {
            self.process_input(d, now);
        }
        self.process_output(now)
    }

    /// Supply an incoming QUIC packet.
    pub fn process_input(&mut self, dgram: Datagram, now: Instant) {
        qtrace!([self], "Process input.");
        self.conn.process_input(dgram, now);
        self.process_http3(now);
    }

    // Only used by neqo-interop
    pub fn conn(&mut self) -> &mut Connection {
        &mut self.conn
    }

    /// Process HTTP3 layer.
    fn process_http3(&mut self, now: Instant) {
        qtrace!([self], "Process http3 internal.");
        match self.base_handler.state() {
            Http3State::ZeroRtt | Http3State::Connected | Http3State::GoingAway(..) => {
                let res = self.check_connection_events();
                if self.check_result(now, &res) {
                    return;
                }
                self.push_handler
                    .borrow_mut()
                    .maybe_send_max_push_id_frame(&mut self.base_handler);
                let res = self.base_handler.process_sending(&mut self.conn);
                self.check_result(now, &res);
            }
            Http3State::Closed { .. } => {}
            _ => {
                let res = self.check_connection_events();
                let _ = self.check_result(now, &res);
            }
        }
    }

    /// Get packet that need to be written into a UDP socket or a timer value if there is no data to send.
    /// This function should be called repeatedly until timer value is returned.
    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!([self], "Process output.");

        // Maybe send() stuff on http3-managed streams
        self.process_http3(now);

        let out = self.conn.process_output(now);

        // Update H3 for any transport state changes and events
        self.process_http3(now);

        out
    }

    // This function takes the provided result and check for an error.
    // An error results in closing the connection.
    fn check_result<ERR>(&mut self, now: Instant, res: &Res<ERR>) -> bool {
        match &res {
            Err(Error::HttpGoaway) => {
                qinfo!([self], "Connection error: goaway stream_id increased.");
                self.close(
                    now,
                    Error::HttpGeneralProtocol.code(),
                    "Connection error: goaway stream_id increased",
                );
                true
            }
            Err(e) => {
                qinfo!([self], "Connection error: {}.", e);
                self.close(now, e.code(), &format!("{}", e));
                true
            }
            _ => false,
        }
    }

    // If this return an error the connection must be closed.
    fn check_connection_events(&mut self) -> Res<()> {
        qtrace!([self], "Check connection events.");
        while let Some(e) = self.conn.next_event() {
            qdebug!([self], "check_connection_events - event {:?}.", e);
            match e {
                ConnectionEvent::NewStream { stream_id } => {
                    self.base_handler.add_new_stream(stream_id);
                }
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    if let Some(s) = self.base_handler.send_streams.get_mut(&stream_id) {
                        s.stream_writable();
                    }
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    self.handle_stream_readable(stream_id)?;
                }
                ConnectionEvent::RecvStreamReset {
                    stream_id,
                    app_error,
                } => self
                    .base_handler
                    .handle_stream_reset(stream_id, app_error, &mut self.conn)?,
                ConnectionEvent::SendStreamStopSending {
                    stream_id,
                    app_error,
                } => self.base_handler.handle_stream_stop_sending(
                    stream_id,
                    app_error,
                    &mut self.conn,
                )?,

                ConnectionEvent::SendStreamCreatable { stream_type } => {
                    self.events.new_requests_creatable(stream_type);
                }
                ConnectionEvent::AuthenticationNeeded => self.events.authentication_needed(),
                ConnectionEvent::EchFallbackAuthenticationNeeded { public_name } => {
                    self.events.ech_fallback_authentication_needed(public_name);
                }
                ConnectionEvent::StateChange(state) => {
                    if self
                        .base_handler
                        .handle_state_change(&mut self.conn, &state)?
                    {
                        self.events
                            .connection_state_change(self.base_handler.state());
                    }
                }
                ConnectionEvent::ZeroRttRejected => {
                    self.base_handler.handle_zero_rtt_rejected()?;
                    self.events.zero_rtt_rejected();
                    self.push_handler.borrow_mut().handle_zero_rtt_rejected();
                }
                ConnectionEvent::ResumptionToken(token) => {
                    if let Some(t) = self.encode_resumption_token(&token) {
                        self.events.resumption_token(t);
                    }
                }
                ConnectionEvent::SendStreamComplete { .. }
                | ConnectionEvent::Datagram { .. }
                | ConnectionEvent::OutgoingDatagramOutcome { .. }
                | ConnectionEvent::IncomingDatagramDropped => {}
            }
        }
        Ok(())
    }

    fn handle_stream_readable(&mut self, stream_id: StreamId) -> Res<()> {
        match self
            .base_handler
            .handle_stream_readable(&mut self.conn, stream_id)?
        {
            ReceiveOutput::NewStream(NewStreamType::Push(push_id)) => {
                self.handle_new_push_stream(stream_id, push_id)
            }
            ReceiveOutput::NewStream(NewStreamType::Http) => Err(Error::HttpStreamCreation),
            ReceiveOutput::NewStream(NewStreamType::WebTransportStream(session_id)) => {
                self.base_handler.webtransport_create_stream_remote(
                    StreamId::from(session_id),
                    stream_id,
                    Box::new(self.events.clone()),
                    Box::new(self.events.clone()),
                )?;
                let res = self
                    .base_handler
                    .handle_stream_readable(&mut self.conn, stream_id)?;
                debug_assert!(matches!(res, ReceiveOutput::NoOutput));
                Ok(())
            }
            ReceiveOutput::ControlFrames(control_frames) => {
                for f in control_frames {
                    match f {
                        HFrame::CancelPush { push_id } => self
                            .push_handler
                            .borrow_mut()
                            .handle_cancel_push(push_id, &mut self.conn, &mut self.base_handler),
                        HFrame::MaxPushId { .. }
                        | HFrame::PriorityUpdateRequest { .. }
                        | HFrame::PriorityUpdatePush { .. } => Err(Error::HttpFrameUnexpected),
                        HFrame::Goaway { stream_id } => self.handle_goaway(stream_id),
                        _ => {
                            unreachable!(
                                "we should only put MaxPushId, Goaway and PriorityUpdates into control_frames."
                            );
                        }
                    }?;
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }

    fn handle_new_push_stream(&mut self, stream_id: StreamId, push_id: u64) -> Res<()> {
        if !self.push_handler.borrow().can_receive_push() {
            return Err(Error::HttpId);
        }

        // Add a new push stream to `PushController`. `add_new_push_stream` may return an error
        // (this will be a connection error) or a bool.
        // If false is returned that means that the stream should be reset because the push has
        // been already canceled (CANCEL_PUSH frame or canceling push from the application).
        if !self
            .push_handler
            .borrow_mut()
            .add_new_push_stream(push_id, stream_id)?
        {
            // We are not interested in the result of stream_stop_sending, we are not interested
            // in this stream.
            mem::drop(
                self.conn
                    .stream_stop_sending(stream_id, Error::HttpRequestCancelled.code()),
            );
            return Ok(());
        }

        self.base_handler.add_recv_stream(
            stream_id,
            Box::new(RecvMessage::new(
                &RecvMessageInfo {
                    message_type: MessageType::Response,
                    stream_type: Http3StreamType::Push,
                    stream_id,
                    header_frame_type_read: false,
                },
                Rc::clone(&self.base_handler.qpack_decoder),
                Box::new(RecvPushEvents::new(push_id, Rc::clone(&self.push_handler))),
                None,
                // TODO: think about the right prority for the push streams.
                PriorityHandler::new(true, Priority::default()),
            )),
        );
        let res = self
            .base_handler
            .handle_stream_readable(&mut self.conn, stream_id)?;
        debug_assert!(matches!(res, ReceiveOutput::NoOutput));
        Ok(())
    }

    fn handle_goaway(&mut self, goaway_stream_id: StreamId) -> Res<()> {
        qinfo!([self], "handle_goaway {}", goaway_stream_id);

        if goaway_stream_id.is_uni() || goaway_stream_id.is_server_initiated() {
            return Err(Error::HttpId);
        }

        match self.base_handler.state {
            Http3State::Connected => {
                self.base_handler.state = Http3State::GoingAway(goaway_stream_id);
            }
            Http3State::GoingAway(ref mut stream_id) => {
                if goaway_stream_id > *stream_id {
                    return Err(Error::HttpGoaway);
                }
                *stream_id = goaway_stream_id;
            }
            Http3State::Closing(..) | Http3State::Closed(..) => {}
            _ => unreachable!("Should not receive Goaway frame in this state."),
        }

        // Issue reset events for streams >= goaway stream id
        let send_ids: Vec<StreamId> = self
            .base_handler
            .send_streams
            .iter()
            .filter_map(id_gte(goaway_stream_id))
            .collect();
        for id in send_ids {
            // We do not care about streams that are going to be closed.
            mem::drop(self.base_handler.handle_stream_stop_sending(
                id,
                Error::HttpRequestRejected.code(),
                &mut self.conn,
            ));
        }

        let recv_ids: Vec<StreamId> = self
            .base_handler
            .recv_streams
            .iter()
            .filter_map(id_gte(goaway_stream_id))
            .collect();
        for id in recv_ids {
            // We do not care about streams that are going to be closed.
            mem::drop(self.base_handler.handle_stream_reset(
                id,
                Error::HttpRequestRejected.code(),
                &mut self.conn,
            ));
        }

        self.events.goaway_received();

        Ok(())
    }

    /// Increases `max_stream_data` for a `stream_id`.
    /// # Errors
    /// Returns `InvalidStreamId` if a stream does not exist or the receiving
    /// side is closed.
    pub fn set_stream_max_data(&mut self, stream_id: StreamId, max_data: u64) -> Res<()> {
        self.conn.set_stream_max_data(stream_id, max_data)?;
        Ok(())
    }

    #[must_use]
    pub fn qpack_decoder_stats(&self) -> QpackStats {
        self.base_handler.qpack_decoder.borrow().stats()
    }

    #[must_use]
    pub fn qpack_encoder_stats(&self) -> QpackStats {
        self.base_handler.qpack_encoder.borrow().stats()
    }

    #[must_use]
    pub fn transport_stats(&self) -> TransportStats {
        self.conn.stats()
    }

    #[must_use]
    pub fn webtransport_enabled(&self) -> bool {
        self.base_handler.webtransport_enabled()
    }
}

impl EventProvider for Http3Client {
    type Event = Http3ClientEvent;

    /// Return true if there are outstanding events.
    fn has_events(&self) -> bool {
        self.events.has_events()
    }

    /// Get events that indicate state changes on the connection. This method
    /// correctly handles cases where handling one event can obsolete
    /// previously-queued events, or cause new events to be generated.
    fn next_event(&mut self) -> Option<Self::Event> {
        self.events.next_event()
    }
}

#[cfg(test)]
mod tests {
    use super::{
        AuthenticationStatus, Connection, Error, HSettings, Header, Http3Client, Http3ClientEvent,
        Http3Parameters, Http3State, Rc, RefCell,
    };
    use crate::frames::{HFrame, H3_FRAME_TYPE_SETTINGS, H3_RESERVED_FRAME_TYPES};
    use crate::qpack_encoder_receiver::EncoderRecvStream;
    use crate::settings::{HSetting, HSettingType, H3_RESERVED_SETTINGS};
    use crate::{Http3Server, Priority, RecvStream};
    use neqo_common::{event::Provider, qtrace, Datagram, Decoder, Encoder};
    use neqo_crypto::{AllowZeroRtt, AntiReplay, ResumptionToken};
    use neqo_qpack::{encoder::QPackEncoder, QpackSettings};
    use neqo_transport::tparams::{self, TransportParameter};
    use neqo_transport::{
        ConnectionError, ConnectionEvent, ConnectionParameters, Output, State, StreamId,
        StreamType, RECV_BUFFER_SIZE, SEND_BUFFER_SIZE,
    };
    use std::convert::TryFrom;
    use std::mem;
    use std::time::Duration;
    use test_fixture::{
        addr, anti_replay, default_server_h3, fixture_init, now, CountingConnectionIdGenerator,
        DEFAULT_ALPN_H3, DEFAULT_KEYS, DEFAULT_SERVER_NAME,
    };

    fn assert_closed(client: &Http3Client, expected: &Error) {
        match client.state() {
            Http3State::Closing(err) | Http3State::Closed(err) => {
                assert_eq!(err, ConnectionError::Application(expected.code()));
            }
            _ => panic!("Wrong state {:?}", client.state()),
        };
    }

    /// Create a http3 client with default configuration.
    pub fn default_http3_client() -> Http3Client {
        default_http3_client_param(100)
    }

    pub fn default_http3_client_param(max_table_size: u64) -> Http3Client {
        fixture_init();
        Http3Client::new(
            DEFAULT_SERVER_NAME,
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            addr(),
            addr(),
            Http3Parameters::default()
                .max_table_size_encoder(max_table_size)
                .max_table_size_decoder(max_table_size)
                .max_blocked_streams(100)
                .max_concurrent_push_streams(5),
            now(),
        )
        .expect("create a default client")
    }

    const CONTROL_STREAM_TYPE: &[u8] = &[0x0];

    // Encoder stream data
    const ENCODER_STREAM_DATA: &[u8] = &[0x2];
    const ENCODER_STREAM_CAP_INSTRUCTION: &[u8] = &[0x3f, 0x45];

    // Encoder stream data with a change capacity instruction(0x3f, 0x45 = change capacity to 100)
    // This data will be send when 0-RTT is used and we already have a max_table_capacity from
    // resumed settings.
    const ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION: &[u8] = &[0x2, 0x3f, 0x45];

    const ENCODER_STREAM_DATA_WITH_CAP_INST_AND_ENCODING_INST: &[u8] = &[
        0x2, 0x3f, 0x45, 0x67, 0xa7, 0xd4, 0xe5, 0x1c, 0x85, 0xb1, 0x1f, 0x86, 0xa7, 0xd7, 0x71,
        0xd1, 0x69, 0x7f,
    ];

    // Decoder stream data
    const DECODER_STREAM_DATA: &[u8] = &[0x3];

    const PUSH_STREAM_TYPE: &[u8] = &[0x1];

    const CLIENT_SIDE_CONTROL_STREAM_ID: StreamId = StreamId::new(2);
    const CLIENT_SIDE_ENCODER_STREAM_ID: StreamId = StreamId::new(6);
    const CLIENT_SIDE_DECODER_STREAM_ID: StreamId = StreamId::new(10);

    struct TestServer {
        settings: HFrame,
        conn: Connection,
        control_stream_id: Option<StreamId>,
        encoder: Rc<RefCell<QPackEncoder>>,
        encoder_receiver: EncoderRecvStream,
        encoder_stream_id: Option<StreamId>,
        decoder_stream_id: Option<StreamId>,
    }

    impl TestServer {
        pub fn new() -> Self {
            Self::new_with_settings(&[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ])
        }

        pub fn new_with_settings(server_settings: &[HSetting]) -> Self {
            fixture_init();
            let max_table_size = server_settings
                .iter()
                .find(|s| s.setting_type == HSettingType::MaxTableCapacity)
                .map_or(100, |s| s.value);
            let max_blocked_streams = u16::try_from(
                server_settings
                    .iter()
                    .find(|s| s.setting_type == HSettingType::BlockedStreams)
                    .map_or(100, |s| s.value),
            )
            .unwrap();
            let qpack = Rc::new(RefCell::new(QPackEncoder::new(
                &QpackSettings {
                    max_table_size_encoder: max_table_size,
                    max_table_size_decoder: max_table_size,
                    max_blocked_streams,
                },
                true,
            )));
            Self {
                settings: HFrame::Settings {
                    settings: HSettings::new(server_settings),
                },
                conn: default_server_h3(),
                control_stream_id: None,
                encoder: Rc::clone(&qpack),
                encoder_receiver: EncoderRecvStream::new(CLIENT_SIDE_DECODER_STREAM_ID, qpack),
                encoder_stream_id: None,
                decoder_stream_id: None,
            }
        }

        pub fn new_with_conn(conn: Connection) -> Self {
            let qpack = Rc::new(RefCell::new(QPackEncoder::new(
                &QpackSettings {
                    max_table_size_encoder: 128,
                    max_table_size_decoder: 128,
                    max_blocked_streams: 0,
                },
                true,
            )));
            Self {
                settings: HFrame::Settings {
                    settings: HSettings::new(&[]),
                },
                conn,
                control_stream_id: None,
                encoder: Rc::clone(&qpack),
                encoder_receiver: EncoderRecvStream::new(CLIENT_SIDE_DECODER_STREAM_ID, qpack),
                encoder_stream_id: None,
                decoder_stream_id: None,
            }
        }

        pub fn create_qpack_streams(&mut self) {
            // Create a QPACK encoder stream
            self.encoder_stream_id = Some(self.conn.stream_create(StreamType::UniDi).unwrap());
            self.encoder
                .borrow_mut()
                .add_send_stream(self.encoder_stream_id.unwrap());
            self.encoder
                .borrow_mut()
                .send_encoder_updates(&mut self.conn)
                .unwrap();

            // Create decoder stream
            self.decoder_stream_id = Some(self.conn.stream_create(StreamType::UniDi).unwrap());
            assert_eq!(
                self.conn
                    .stream_send(self.decoder_stream_id.unwrap(), DECODER_STREAM_DATA)
                    .unwrap(),
                1
            );
        }

        pub fn create_control_stream(&mut self) {
            // Create control stream
            let control = self.conn.stream_create(StreamType::UniDi).unwrap();
            qtrace!(["TestServer"], "control stream: {}", control);
            self.control_stream_id = Some(control);
            // Send stream type on the control stream.
            assert_eq!(
                self.conn
                    .stream_send(self.control_stream_id.unwrap(), CONTROL_STREAM_TYPE)
                    .unwrap(),
                1
            );

            // Encode a settings frame and send it.
            let mut enc = Encoder::default();
            self.settings.encode(&mut enc);
            assert_eq!(
                self.conn
                    .stream_send(self.control_stream_id.unwrap(), &enc[..])
                    .unwrap(),
                enc[..].len()
            );
        }

        pub fn check_client_control_qpack_streams_no_resumption(&mut self) {
            self.check_client_control_qpack_streams(
                ENCODER_STREAM_DATA,
                EXPECTED_REQUEST_HEADER_FRAME,
                false,
                true,
            );
        }

        pub fn check_control_qpack_request_streams_resumption(
            &mut self,
            expect_encoder_stream_data: &[u8],
            expect_request_header: &[u8],
            expect_request: bool,
        ) {
            self.check_client_control_qpack_streams(
                expect_encoder_stream_data,
                expect_request_header,
                expect_request,
                false,
            );
        }

        // Check that server has received correct settings and qpack streams.
        pub fn check_client_control_qpack_streams(
            &mut self,
            expect_encoder_stream_data: &[u8],
            expect_request_header: &[u8],
            expect_request: bool,
            expect_connected: bool,
        ) {
            let mut connected = false;
            let mut control_stream = false;
            let mut qpack_decoder_stream = false;
            let mut qpack_encoder_stream = false;
            let mut request = false;
            while let Some(e) = self.conn.next_event() {
                match e {
                    ConnectionEvent::NewStream { stream_id }
                    | ConnectionEvent::SendStreamWritable { stream_id } => {
                        if expect_request {
                            assert!(matches!(stream_id.as_u64(), 2 | 6 | 10 | 0));
                        } else {
                            assert!(matches!(stream_id.as_u64(), 2 | 6 | 10));
                        }
                    }
                    ConnectionEvent::RecvStreamReadable { stream_id } => {
                        if stream_id == CLIENT_SIDE_CONTROL_STREAM_ID {
                            self.check_control_stream();
                            control_stream = true;
                        } else if stream_id == CLIENT_SIDE_ENCODER_STREAM_ID {
                            // the qpack encoder stream
                            self.read_and_check_stream_data(
                                stream_id,
                                expect_encoder_stream_data,
                                false,
                            );
                            qpack_encoder_stream = true;
                        } else if stream_id == CLIENT_SIDE_DECODER_STREAM_ID {
                            // the qpack decoder stream
                            self.read_and_check_stream_data(stream_id, DECODER_STREAM_DATA, false);
                            qpack_decoder_stream = true;
                        } else if stream_id == 0 {
                            assert!(expect_request);
                            self.read_and_check_stream_data(stream_id, expect_request_header, true);
                            request = true;
                        } else {
                            panic!("unexpected event");
                        }
                    }
                    ConnectionEvent::StateChange(State::Connected) => connected = true,
                    ConnectionEvent::StateChange(_)
                    | ConnectionEvent::SendStreamCreatable { .. } => {}
                    _ => panic!("unexpected event"),
                }
            }
            assert_eq!(connected, expect_connected);
            assert!(control_stream);
            assert!(qpack_encoder_stream);
            assert!(qpack_decoder_stream);
            assert_eq!(request, expect_request);
        }

        // Check that the control stream contains default values.
        // Expect a SETTINGS frame, some grease, and a MAX_PUSH_ID frame.
        // The default test configuration uses:
        //  - max_table_capacity = 100
        //  - max_blocked_streams = 100
        // and a maximum of 5 push streams.
        fn check_control_stream(&mut self) {
            let mut buf = [0_u8; 100];
            let (amount, fin) = self
                .conn
                .stream_recv(CLIENT_SIDE_CONTROL_STREAM_ID, &mut buf)
                .unwrap();
            let mut dec = Decoder::from(&buf[..amount]);
            assert_eq!(dec.decode_varint().unwrap(), 0); // control stream type
            assert_eq!(dec.decode_varint().unwrap(), 4); // SETTINGS
            assert_eq!(
                dec.decode_vvec().unwrap(),
                &[1, 0x40, 0x64, 7, 0x40, 0x64, 0xab, 0x60, 0x37, 0x42, 0x00]
            );

            assert_eq!((dec.decode_varint().unwrap() - 0x21) % 0x1f, 0); // Grease
            assert!(dec.decode_vvec().unwrap().len() < 8);

            assert_eq!(dec.decode_varint().unwrap(), 0xd); // MAX_PUSH_ID
            assert_eq!(dec.decode_vvec().unwrap(), &[5]);

            assert_eq!(dec.remaining(), 0);
            assert!(!fin);
        }

        pub fn read_and_check_stream_data(
            &mut self,
            stream_id: StreamId,
            expected_data: &[u8],
            expected_fin: bool,
        ) {
            let mut buf = [0_u8; 100];
            let (amount, fin) = self.conn.stream_recv(stream_id, &mut buf).unwrap();
            assert_eq!(fin, expected_fin);
            assert_eq!(amount, expected_data.len());
            assert_eq!(&buf[..amount], expected_data);
        }

        pub fn encode_headers(
            &mut self,
            stream_id: StreamId,
            headers: &[Header],
            encoder: &mut Encoder,
        ) {
            let header_block =
                self.encoder
                    .borrow_mut()
                    .encode_header_block(&mut self.conn, headers, stream_id);
            let hframe = HFrame::Headers {
                header_block: header_block.to_vec(),
            };
            hframe.encode(encoder);
        }

        pub fn set_max_uni_stream(&mut self, max_stream: u64) {
            self.conn
                .set_local_tparam(
                    tparams::INITIAL_MAX_STREAMS_UNI,
                    TransportParameter::Integer(max_stream),
                )
                .unwrap();
        }
    }

    fn handshake_only(client: &mut Http3Client, server: &mut TestServer) -> Output {
        assert_eq!(client.state(), Http3State::Initializing);
        let out = client.process(None, now());
        assert_eq!(client.state(), Http3State::Initializing);

        assert_eq!(*server.conn.state(), State::Init);
        let out = server.conn.process(out.dgram(), now());
        assert_eq!(*server.conn.state(), State::Handshaking);

        let out = client.process(out.dgram(), now());
        let out = server.conn.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());

        let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
        assert!(client.events().any(authentication_needed));
        client.authenticated(AuthenticationStatus::Ok, now());
        out
    }

    // Perform only Quic transport handshake.
    fn connect_only_transport_with(client: &mut Http3Client, server: &mut TestServer) {
        let out = handshake_only(client, server);

        let out = client.process(out.dgram(), now());
        let connected = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::Connected));
        assert!(client.events().any(connected));

        assert_eq!(client.state(), Http3State::Connected);
        server.conn.process_input(out.dgram().unwrap(), now());
        assert!(server.conn.state().connected());
    }

    // Perform only Quic transport handshake.
    fn connect_only_transport() -> (Http3Client, TestServer) {
        let mut client = default_http3_client();
        let mut server = TestServer::new();
        connect_only_transport_with(&mut client, &mut server);
        (client, server)
    }

    fn send_and_receive_client_settings(client: &mut Http3Client, server: &mut TestServer) {
        // send and receive client settings
        let dgram = client.process(None, now()).dgram();
        server.conn.process_input(dgram.unwrap(), now());
        server.check_client_control_qpack_streams_no_resumption();
    }

    // Perform Quic transport handshake and exchange Http3 settings.
    fn connect_with(client: &mut Http3Client, server: &mut TestServer) {
        connect_only_transport_with(client, server);

        send_and_receive_client_settings(client, server);

        server.create_control_stream();

        server.create_qpack_streams();
        // Send the server's control and qpack streams data.
        let dgram = server.conn.process(None, now()).dgram();
        client.process_input(dgram.unwrap(), now());

        // assert no error occured.
        assert_eq!(client.state(), Http3State::Connected);
    }

    // Perform Quic transport handshake and exchange Http3 settings.
    fn connect_with_connection_parameters(
        server_conn_params: ConnectionParameters,
    ) -> (Http3Client, TestServer) {
        // connecting with default max_table_size
        let mut client = default_http3_client_param(100);
        let server = Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN_H3,
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            server_conn_params,
        )
        .unwrap();
        let mut server = TestServer::new_with_conn(server);
        connect_with(&mut client, &mut server);
        (client, server)
    }

    // Perform Quic transport handshake and exchange Http3 settings.
    fn connect() -> (Http3Client, TestServer) {
        let mut client = default_http3_client();
        let mut server = TestServer::new();
        connect_with(&mut client, &mut server);
        (client, server)
    }

    // Fetch request fetch("GET", "https", "something.com", "/", headers).
    fn make_request(
        client: &mut Http3Client,
        close_sending_side: bool,
        headers: &[Header],
    ) -> StreamId {
        let request_stream_id = client
            .fetch(
                now(),
                "GET",
                "https://something.com/",
                headers,
                Priority::default(),
            )
            .unwrap();
        if close_sending_side {
            client.stream_close_send(request_stream_id).unwrap();
        }
        request_stream_id
    }

    // For fetch request fetch("GET", "https", "something.com", "/", &[])
    // the following request header frame will be sent:
    const EXPECTED_REQUEST_HEADER_FRAME: &[u8] = &[
        0x01, 0x10, 0x00, 0x00, 0xd1, 0xd7, 0x50, 0x89, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x2e,
        0x43, 0xd3, 0xc1,
    ];

    // For fetch request fetch("GET", "https", "something.com", "/", &[(String::from("myheaders", "myvalue"))])
    // the following request header frame will be sent:
    const EXPECTED_REQUEST_HEADER_FRAME_VERSION2: &[u8] = &[
        0x01, 0x11, 0x02, 0x80, 0xd1, 0xd7, 0x50, 0x89, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x2e,
        0x43, 0xd3, 0xc1, 0x10,
    ];

    const HTTP_HEADER_FRAME_0: &[u8] = &[0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x30];

    // The response header from HTTP_HEADER_FRAME (0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x30) are
    // decoded into:
    fn check_response_header_0(header: &[Header]) {
        let expected_response_header_0 = &[
            Header::new(":status", "200"),
            Header::new("content-length", "0"),
        ];
        assert_eq!(header, expected_response_header_0);
    }

    const HTTP_RESPONSE_1: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x37, // the first data frame
        0x0, 0x3, 0x61, 0x62, 0x63, // the second data frame
        0x0, 0x4, 0x64, 0x65, 0x66, 0x67,
    ];

    const HTTP_RESPONSE_HEADER_ONLY_1: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x37,
    ];
    const HTTP_RESPONSE_DATA_FRAME_1_ONLY_1: &[u8] = &[0x0, 0x3, 0x61, 0x62, 0x63];

    const HTTP_RESPONSE_DATA_FRAME_2_ONLY_1: &[u8] = &[0x0, 0x4, 0x64, 0x65, 0x66, 0x67];

    // The response header from HTTP_RESPONSE_1 (0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x36) are
    // decoded into:
    fn check_response_header_1(header: &[Header]) {
        let expected_response_header_1 = &[
            Header::new(":status", "200"),
            Header::new("content-length", "7"),
        ];
        assert_eq!(header, expected_response_header_1);
    }

    const EXPECTED_RESPONSE_DATA_1: &[u8] = &[0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67];

    const HTTP_RESPONSE_2: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // the data frame
        0x0, 0x3, 0x61, 0x62, 0x63,
    ];

    const HTTP_RESPONSE_HEADER_ONLY_2: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
    ];

    const HTTP_RESPONSE_DATA_FRAME_ONLY_2: &[u8] = &[
        // the data frame
        0x0, 0x3, 0x61, 0x62, 0x63,
    ];

    // The response header from HTTP_RESPONSE_2 (0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x36) are
    // decoded into:
    fn check_response_header_2(header: &[Header]) {
        let expected_response_header_2 = &[
            Header::new(":status", "200"),
            Header::new("content-length", "3"),
        ];
        assert_eq!(header, expected_response_header_2);
    }

    // The data frame payload from HTTP_RESPONSE_2 is:
    const EXPECTED_RESPONSE_DATA_2_FRAME_1: &[u8] = &[0x61, 0x62, 0x63];

    fn make_request_and_exchange_pkts(
        client: &mut Http3Client,
        server: &mut TestServer,
        close_sending_side: bool,
    ) -> StreamId {
        let request_stream_id = make_request(client, close_sending_side, &[]);

        let dgram = client.process(None, now()).dgram();
        server.conn.process_input(dgram.unwrap(), now());

        // find the new request/response stream and send frame v on it.
        while let Some(e) = server.conn.next_event() {
            match e {
                ConnectionEvent::NewStream { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(stream_id.stream_type(), StreamType::BiDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    if stream_id == CLIENT_SIDE_ENCODER_STREAM_ID {
                        server.read_and_check_stream_data(
                            stream_id,
                            ENCODER_STREAM_CAP_INSTRUCTION,
                            false,
                        );
                    } else {
                        assert_eq!(stream_id, request_stream_id);
                        server.read_and_check_stream_data(
                            stream_id,
                            EXPECTED_REQUEST_HEADER_FRAME,
                            close_sending_side,
                        );
                    }
                }
                _ => {}
            }
        }
        let dgram = server.conn.process_output(now()).dgram();
        if let Some(d) = dgram {
            client.process_input(d, now());
        }
        request_stream_id
    }

    fn connect_and_send_request(close_sending_side: bool) -> (Http3Client, TestServer, StreamId) {
        let (mut client, mut server) = connect();
        let request_stream_id =
            make_request_and_exchange_pkts(&mut client, &mut server, close_sending_side);
        assert_eq!(request_stream_id, 0);

        (client, server, request_stream_id)
    }

    fn server_send_response_and_exchange_packet(
        client: &mut Http3Client,
        server: &mut TestServer,
        stream_id: StreamId,
        response: &[u8],
        close_stream: bool,
    ) {
        let _ = server.conn.stream_send(stream_id, response).unwrap();
        if close_stream {
            server.conn.stream_close_send(stream_id).unwrap();
        }
        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));
    }

    const PUSH_PROMISE_DATA: &[u8] = &[
        0x00, 0x00, 0xd1, 0xd7, 0x50, 0x89, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x2e, 0x43, 0xd3,
        0xc1,
    ];

    fn check_pushpromise_header(header: &[Header]) {
        let expected_response_header_1 = &[
            Header::new(":method", "GET"),
            Header::new(":scheme", "https"),
            Header::new(":authority", "something.com"),
            Header::new(":path", "/"),
        ];
        assert_eq!(header, expected_response_header_1);
    }

    // Send a push promise with push_id and request_stream_id.
    fn send_push_promise(conn: &mut Connection, stream_id: StreamId, push_id: u64) {
        let frame = HFrame::PushPromise {
            push_id,
            header_block: PUSH_PROMISE_DATA.to_vec(),
        };
        let mut d = Encoder::default();
        frame.encode(&mut d);
        let _ = conn.stream_send(stream_id, &d).unwrap();
    }

    fn send_push_data_and_exchange_packets(
        client: &mut Http3Client,
        server: &mut TestServer,
        push_id: u8,
        close_push_stream: bool,
    ) -> StreamId {
        let push_stream_id = send_push_data(&mut server.conn, push_id, close_push_stream);

        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));

        push_stream_id
    }

    fn send_push_promise_and_exchange_packets(
        client: &mut Http3Client,
        server: &mut TestServer,
        stream_id: StreamId,
        push_id: u64,
    ) {
        send_push_promise(&mut server.conn, stream_id, push_id);

        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));
    }

    fn send_cancel_push_and_exchange_packets(
        client: &mut Http3Client,
        server: &mut TestServer,
        push_id: u64,
    ) {
        let frame = HFrame::CancelPush { push_id };
        let mut d = Encoder::default();
        frame.encode(&mut d);
        server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &d)
            .unwrap();

        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));
    }

    const PUSH_DATA: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x34, // the data frame.
        0x0, 0x4, 0x61, 0x62, 0x63, 0x64,
    ];

    // The response header from PUSH_DATA (0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x34) are
    // decoded into:
    fn check_push_response_header(header: &[Header]) {
        let expected_push_response_header = vec![
            Header::new(":status", "200"),
            Header::new("content-length", "4"),
        ];
        assert_eq!(header, &expected_push_response_header[..]);
    }

    // The data frame payload from PUSH_DATA is:
    const EXPECTED_PUSH_RESPONSE_DATA_FRAME: &[u8] = &[0x61, 0x62, 0x63, 0x64];

    // Send push data on a push stream:
    //  1) push_stream_type PUSH_STREAM_TYPE
    //  2) push_id
    //  3) PUSH_DATA that contains encoded headers and a data frame.
    // This function can only handle small push_id numbers that fit in a varint of length 1 byte.
    fn send_data_on_push(
        conn: &mut Connection,
        push_stream_id: StreamId,
        push_id: u8,
        data: &[u8],
        close_push_stream: bool,
    ) {
        // send data
        let _ = conn.stream_send(push_stream_id, PUSH_STREAM_TYPE).unwrap();
        let _ = conn.stream_send(push_stream_id, &[push_id]).unwrap();
        let _ = conn.stream_send(push_stream_id, data).unwrap();
        if close_push_stream {
            conn.stream_close_send(push_stream_id).unwrap();
        }
    }

    // Send push data on a push stream:
    //  1) push_stream_type PUSH_STREAM_TYPE
    //  2) push_id
    //  3) PUSH_DATA that contains encoded headers and a data frame.
    // This function can only handle small push_id numbers that fit in a varint of length 1 byte.
    fn send_push_data(conn: &mut Connection, push_id: u8, close_push_stream: bool) -> StreamId {
        send_push_with_data(conn, push_id, PUSH_DATA, close_push_stream)
    }

    // Send push data on a push stream:
    //  1) push_stream_type PUSH_STREAM_TYPE
    //  2) push_id
    //  3) and supplied push data.
    // This function can only handle small push_id numbers that fit in a varint of length 1 byte.
    fn send_push_with_data(
        conn: &mut Connection,
        push_id: u8,
        data: &[u8],
        close_push_stream: bool,
    ) -> StreamId {
        // create a push stream
        let push_stream_id = conn.stream_create(StreamType::UniDi).unwrap();
        // send data
        send_data_on_push(conn, push_stream_id, push_id, data, close_push_stream);
        push_stream_id
    }

    struct PushPromiseInfo {
        pub push_id: u64,
        pub ref_stream_id: StreamId,
    }

    // Helper function: read response when a server sends:
    // - HTTP_RESPONSE_2 on the request_stream_id stream,
    // - a number of push promises described by a list of PushPromiseInfo.
    // - and a push streams with push_id in the push_streams list.
    // All push stream contain PUSH_DATA that decodes to headers (that can be checked by calling
    // check_push_response_header) and EXPECTED_PUSH_RESPONSE_DATA_FRAME
    fn read_response_and_push_events(
        client: &mut Http3Client,
        push_promises: &[PushPromiseInfo],
        push_streams: &[u64],
        response_stream_id: StreamId,
    ) {
        let mut num_push_promises = 0;
        let mut num_push_stream_headers = 0;
        let mut num_push_stream_data = 0;
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::PushPromise {
                    push_id,
                    request_stream_id,
                    headers,
                } => {
                    assert!(push_promises
                        .iter()
                        .any(|p| p.push_id == push_id && p.ref_stream_id == request_stream_id));
                    check_pushpromise_header(&headers[..]);
                    num_push_promises += 1;
                }
                Http3ClientEvent::PushHeaderReady {
                    push_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert!(push_streams.contains(&push_id));
                    check_push_response_header(&headers);
                    num_push_stream_headers += 1;
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::PushDataReadable { push_id } => {
                    assert!(push_streams.contains(&push_id));
                    let mut buf = [0_u8; 100];
                    let (amount, fin) = client.push_read_data(now(), push_id, &mut buf).unwrap();
                    assert!(fin);
                    assert_eq!(amount, EXPECTED_PUSH_RESPONSE_DATA_FRAME.len());
                    assert_eq!(&buf[..amount], EXPECTED_PUSH_RESPONSE_DATA_FRAME);
                    num_push_stream_data += 1;
                }
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, response_stream_id);
                    check_response_header_2(&headers);
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, response_stream_id);
                    let mut buf = [0_u8; 100];
                    let (amount, _) = client.read_data(now(), stream_id, &mut buf).unwrap();
                    assert_eq!(amount, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                    assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_2_FRAME_1);
                }
                _ => {}
            }
        }

        assert_eq!(num_push_promises, push_promises.len());
        assert_eq!(num_push_stream_headers, push_streams.len());
        assert_eq!(num_push_stream_data, push_streams.len());
    }

    // Client: Test receiving a new control stream and a SETTINGS frame.
    #[test]
    fn test_client_connect_and_exchange_qpack_and_control_streams() {
        mem::drop(connect());
    }

    // Client: Test that the connection will be closed if control stream
    // has been closed.
    #[test]
    fn test_client_close_control_stream() {
        let (mut client, mut server) = connect();
        server
            .conn
            .stream_close_send(server.control_stream_id.unwrap())
            .unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpClosedCriticalStream);
    }

    // Client: Test that the connection will be closed if the local control stream
    // has been reset.
    #[test]
    fn test_client_reset_control_stream() {
        let (mut client, mut server) = connect();
        server
            .conn
            .stream_reset_send(server.control_stream_id.unwrap(), Error::HttpNoError.code())
            .unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpClosedCriticalStream);
    }

    // Client: Test that the connection will be closed if the server side encoder stream
    // has been reset.
    #[test]
    fn test_client_reset_server_side_encoder_stream() {
        let (mut client, mut server) = connect();
        server
            .conn
            .stream_reset_send(server.encoder_stream_id.unwrap(), Error::HttpNoError.code())
            .unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpClosedCriticalStream);
    }

    // Client: Test that the connection will be closed if the server side decoder stream
    // has been reset.
    #[test]
    fn test_client_reset_server_side_decoder_stream() {
        let (mut client, mut server) = connect();
        server
            .conn
            .stream_reset_send(server.decoder_stream_id.unwrap(), Error::HttpNoError.code())
            .unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpClosedCriticalStream);
    }

    // Client: Test that the connection will be closed if the local control stream
    // has received a stop_sending.
    #[test]
    fn test_client_stop_sending_control_stream() {
        let (mut client, mut server) = connect();
        server
            .conn
            .stream_stop_sending(CLIENT_SIDE_CONTROL_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpClosedCriticalStream);
    }

    // Client: Test that the connection will be closed if the client side encoder stream
    // has received a stop_sending.
    #[test]
    fn test_client_stop_sending_encoder_stream() {
        let (mut client, mut server) = connect();
        server
            .conn
            .stream_stop_sending(CLIENT_SIDE_ENCODER_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpClosedCriticalStream);
    }

    // Client: Test that the connection will be closed if the client side decoder stream
    // has received a stop_sending.
    #[test]
    fn test_client_stop_sending_decoder_stream() {
        let (mut client, mut server) = connect();
        server
            .conn
            .stream_stop_sending(CLIENT_SIDE_DECODER_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpClosedCriticalStream);
    }

    // Client: test missing SETTINGS frame
    // (the first frame sent is a garbage frame).
    #[test]
    fn test_client_missing_settings() {
        let (mut client, mut server) = connect_only_transport();
        // Create server control stream.
        let control_stream = server.conn.stream_create(StreamType::UniDi).unwrap();
        // Send a HEADERS frame instead (which contains garbage).
        let sent = server
            .conn
            .stream_send(control_stream, &[0x0, 0x1, 0x3, 0x0, 0x1, 0x2]);
        assert_eq!(sent, Ok(6));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpMissingSettings);
    }

    // Client: receiving SETTINGS frame twice causes connection close
    // with error HTTP_UNEXPECTED_FRAME.
    #[test]
    fn test_client_receive_settings_twice() {
        let (mut client, mut server) = connect();
        // send the second SETTINGS frame.
        let sent = server.conn.stream_send(
            server.control_stream_id.unwrap(),
            &[0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64],
        );
        assert_eq!(sent, Ok(8));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpFrameUnexpected);
    }

    fn test_wrong_frame_on_control_stream(v: &[u8]) {
        let (mut client, mut server) = connect();

        // send a frame that is not allowed on the control stream.
        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), v)
            .unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        assert_closed(&client, &Error::HttpFrameUnexpected);
    }

    // send DATA frame on a cortrol stream
    #[test]
    fn test_data_frame_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x0, 0x2, 0x1, 0x2]);
    }

    // send HEADERS frame on a cortrol stream
    #[test]
    fn test_headers_frame_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x1, 0x2, 0x1, 0x2]);
    }

    // send PUSH_PROMISE frame on a cortrol stream
    #[test]
    fn test_push_promise_frame_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x5, 0x2, 0x1, 0x2]);
    }

    // send PRIORITY_UPDATE frame on a control stream to the client
    #[test]
    fn test_priority_update_request_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x80, 0x0f, 0x07, 0x00, 0x01, 0x03]);
    }

    #[test]
    fn test_priority_update_push_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x80, 0x0f, 0x07, 0x01, 0x01, 0x03]);
    }

    fn test_wrong_frame_on_push_stream(v: &[u8]) {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        send_push_promise(&mut server.conn, request_stream_id, 0);
        // Create a push stream
        let push_stream_id = server.conn.stream_create(StreamType::UniDi).unwrap();

        // Send the push stream type byte, push_id and frame v.
        let _ = server
            .conn
            .stream_send(push_stream_id, &[0x01, 0x0])
            .unwrap();
        let _ = server.conn.stream_send(push_stream_id, v).unwrap();

        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));

        assert_closed(&client, &Error::HttpFrameUnexpected);
    }

    #[test]
    fn test_cancel_push_frame_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0x3, 0x1, 0x5]);
    }

    #[test]
    fn test_settings_frame_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0x4, 0x4, 0x6, 0x4, 0x8, 0x4]);
    }

    #[test]
    fn test_push_promise_frame_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0x5, 0x2, 0x1, 0x2]);
    }

    #[test]
    fn test_priority_update_request_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0x80, 0x0f, 0x07, 0x00, 0x01, 0x03]);
    }

    #[test]
    fn test_priority_update_push_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0x80, 0x0f, 0x07, 0x01, 0x01, 0x03]);
    }

    #[test]
    fn test_goaway_frame_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0x7, 0x1, 0x5]);
    }

    #[test]
    fn test_max_push_id_frame_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0xd, 0x1, 0x5]);
    }

    // send DATA frame before a header frame
    #[test]
    fn test_data_frame_on_push_stream() {
        test_wrong_frame_on_push_stream(&[0x0, 0x2, 0x1, 0x2]);
    }

    // Client: receive unknown stream type
    // This function also tests getting stream id that does not fit into a single byte.
    #[test]
    fn test_client_received_unknown_stream() {
        let (mut client, mut server) = connect();

        // create a stream with unknown type.
        let new_stream_id = server.conn.stream_create(StreamType::UniDi).unwrap();
        let _ = server
            .conn
            .stream_send(new_stream_id, &[0x41, 0x19, 0x4, 0x4, 0x6, 0x0, 0x8, 0x0])
            .unwrap();
        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));

        // check for stop-sending with Error::HttpStreamCreation.
        let mut stop_sending_event_found = false;
        while let Some(e) = server.conn.next_event() {
            if let ConnectionEvent::SendStreamStopSending {
                stream_id,
                app_error,
            } = e
            {
                stop_sending_event_found = true;
                assert_eq!(stream_id, new_stream_id);
                assert_eq!(app_error, Error::HttpStreamCreation.code());
            }
        }
        assert!(stop_sending_event_found);
        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test wrong frame on req/rec stream
    fn test_wrong_frame_on_request_stream(v: &[u8]) {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        let _ = server.conn.stream_send(request_stream_id, v).unwrap();

        // Generate packet with the above bad h3 input
        let out = server.conn.process(None, now());
        // Process bad input and close the connection.
        mem::drop(client.process(out.dgram(), now()));

        assert_closed(&client, &Error::HttpFrameUnexpected);
    }

    #[test]
    fn test_cancel_push_frame_on_request_stream() {
        test_wrong_frame_on_request_stream(&[0x3, 0x1, 0x5]);
    }

    #[test]
    fn test_settings_frame_on_request_stream() {
        test_wrong_frame_on_request_stream(&[0x4, 0x4, 0x6, 0x4, 0x8, 0x4]);
    }

    #[test]
    fn test_goaway_frame_on_request_stream() {
        test_wrong_frame_on_request_stream(&[0x7, 0x1, 0x5]);
    }

    #[test]
    fn test_max_push_id_frame_on_request_stream() {
        test_wrong_frame_on_request_stream(&[0xd, 0x1, 0x5]);
    }

    #[test]
    fn test_priority_update_request_on_request_stream() {
        test_wrong_frame_on_request_stream(&[0x80, 0x0f, 0x07, 0x00, 0x01, 0x03]);
    }

    #[test]
    fn test_priority_update_push_on_request_stream() {
        test_wrong_frame_on_request_stream(&[0x80, 0x0f, 0x07, 0x01, 0x01, 0x03]);
    }

    // Test reading of a slowly streamed frame. bytes are received one by one
    #[test]
    fn test_frame_reading() {
        let (mut client, mut server) = connect_only_transport();

        // create a control stream.
        let control_stream = server.conn.stream_create(StreamType::UniDi).unwrap();

        // send the stream type
        let mut sent = server.conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // start sending SETTINGS frame
        sent = server.conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x6]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x8]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        assert_eq!(client.state(), Http3State::Connected);

        // Now test PushPromise
        sent = server.conn.stream_send(control_stream, &[0x5]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x5]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x61]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x62]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x63]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        sent = server.conn.stream_send(control_stream, &[0x64]);
        assert_eq!(sent, Ok(1));
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // PUSH_PROMISE on a control stream will cause an error
        assert_closed(&client, &Error::HttpFrameUnexpected);
    }

    #[test]
    fn fetch_basic() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // send response - 200  Content-Length: 7
        // with content: 'abcdefg'.
        // The content will be send in 2 DATA frames.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_1,
            true,
        );

        let http_events = client.events().collect::<Vec<_>>();
        assert_eq!(http_events.len(), 2);
        for e in http_events {
            match e {
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_response_header_1(&headers);
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0_u8; 100];
                    let (amount, fin) = client.read_data(now(), stream_id, &mut buf).unwrap();
                    assert!(fin);
                    assert_eq!(amount, EXPECTED_RESPONSE_DATA_1.len());
                    assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_1);
                }
                _ => {}
            }
        }

        // after this stream will be removed from hcoon. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0_u8; 100];
        let res = client.read_data(now(), request_stream_id, &mut buf);
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    /// Force both endpoints into an idle state.
    /// Do this by opening unidirectional streams at both endpoints and sending
    /// a partial unidirectional stream type (which the receiver has to buffer),
    /// then delivering packets out of order.
    /// This forces the receiver to create an acknowledgment, which will allow
    /// the peer to become idle.
    fn force_idle(client: &mut Http3Client, server: &mut TestServer) {
        // Send a partial unidirectional stream ID.
        // Note that this can't close the stream as that causes the receiver
        // to send `MAX_STREAMS`, which would prevent it from becoming idle.
        fn dgram(c: &mut Connection) -> Datagram {
            let stream = c.stream_create(StreamType::UniDi).unwrap();
            let _ = c.stream_send(stream, &[0xc0]).unwrap();
            c.process_output(now()).dgram().unwrap()
        }

        let d1 = dgram(&mut client.conn);
        let d2 = dgram(&mut client.conn);
        server.conn.process_input(d2, now());
        server.conn.process_input(d1, now());
        let d3 = dgram(&mut server.conn);
        let d4 = dgram(&mut server.conn);
        client.process_input(d4, now());
        client.process_input(d3, now());
        let ack = client.process_output(now()).dgram();
        server.conn.process_input(ack.unwrap(), now());
    }

    /// The client should keep a connection alive if it has unanswered requests.
    #[test]
    fn fetch_keep_alive() {
        let (mut client, mut server, _request_stream_id) = connect_and_send_request(true);
        force_idle(&mut client, &mut server);

        let idle_timeout = ConnectionParameters::default().get_idle_timeout();
        assert_eq!(client.process_output(now()).callback(), idle_timeout / 2);
    }

    // Helper function: read response when a server sends HTTP_RESPONSE_2.
    fn read_response(
        client: &mut Http3Client,
        server: &mut Connection,
        request_stream_id: StreamId,
    ) {
        let out = server.process(None, now());
        client.process(out.dgram(), now());

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_response_header_2(&headers);
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0_u8; 100];
                    let (amount, fin) = client.read_data(now(), stream_id, &mut buf).unwrap();
                    assert!(fin);
                    assert_eq!(amount, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                    assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_2_FRAME_1);
                }
                _ => {}
            }
        }

        // after this stream will be removed from client. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0_u8; 100];
        let res = client.read_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    // Data sent with a request:
    const REQUEST_BODY: &[u8] = &[0x64, 0x65, 0x66];
    // Corresponding data frame that server will receive.
    const EXPECTED_REQUEST_BODY_FRAME: &[u8] = &[0x0, 0x3, 0x64, 0x65, 0x66];

    // Send a request with the request body.
    #[test]
    fn fetch_with_data() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(client.events().any(data_writable));
        let sent = client.send_data(request_stream_id, REQUEST_BODY).unwrap();
        assert_eq!(sent, REQUEST_BODY.len());
        client.stream_close_send(request_stream_id).unwrap();

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        // find the new request/response stream and send response on it.
        while let Some(e) = server.conn.next_event() {
            match e {
                ConnectionEvent::NewStream { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(stream_id.stream_type(), StreamType::BiDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);

                    // Read request body.
                    let mut buf = [0_u8; 100];
                    let (amount, fin) = server.conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert!(fin);
                    assert_eq!(amount, EXPECTED_REQUEST_BODY_FRAME.len());
                    assert_eq!(&buf[..amount], EXPECTED_REQUEST_BODY_FRAME);

                    // send response - 200  Content-Length: 3
                    // with content: 'abc'.
                    let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_2).unwrap();
                    server.conn.stream_close_send(stream_id).unwrap();
                }
                _ => {}
            }
        }

        read_response(&mut client, &mut server.conn, request_stream_id);
    }

    // send a request with request body containing request_body. We expect to receive expected_data_frame_header.
    fn fetch_with_data_length_xbytes(request_body: &[u8], expected_data_frame_header: &[u8]) {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(client.events().any(data_writable));
        let sent = client.send_data(request_stream_id, request_body);
        assert_eq!(sent, Ok(request_body.len()));

        // Close stream.
        client.stream_close_send(request_stream_id).unwrap();

        // We need to loop a bit until all data has been sent.
        let mut out = client.process(None, now());
        for _i in 0..20 {
            out = server.conn.process(out.dgram(), now());
            out = client.process(out.dgram(), now());
        }

        // check request body is received.
        // Then send a response.
        while let Some(e) = server.conn.next_event() {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                if stream_id == request_stream_id {
                    // Read the DATA frame.
                    let mut buf = vec![1_u8; RECV_BUFFER_SIZE];
                    let (amount, fin) = server.conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert!(fin);
                    assert_eq!(
                        amount,
                        request_body.len() + expected_data_frame_header.len()
                    );

                    // Check the DATA frame header
                    assert_eq!(
                        &buf[..expected_data_frame_header.len()],
                        expected_data_frame_header
                    );

                    // Check data.
                    assert_eq!(&buf[expected_data_frame_header.len()..amount], request_body);

                    // send response - 200  Content-Length: 3
                    // with content: 'abc'.
                    let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_2).unwrap();
                    server.conn.stream_close_send(stream_id).unwrap();
                }
            }
        }

        read_response(&mut client, &mut server.conn, request_stream_id);
    }

    // send a request with 63 bytes. The DATA frame length field will still have 1 byte.
    #[test]
    fn fetch_with_data_length_63bytes() {
        fetch_with_data_length_xbytes(&[0_u8; 63], &[0x0, 0x3f]);
    }

    // send a request with 64 bytes. The DATA frame length field will need 2 byte.
    #[test]
    fn fetch_with_data_length_64bytes() {
        fetch_with_data_length_xbytes(&[0_u8; 64], &[0x0, 0x40, 0x40]);
    }

    // send a request with 16383 bytes. The DATA frame length field will still have 2 byte.
    #[test]
    fn fetch_with_data_length_16383bytes() {
        fetch_with_data_length_xbytes(&[0_u8; 16383], &[0x0, 0x7f, 0xff]);
    }

    // send a request with 16384 bytes. The DATA frame length field will need 4 byte.
    #[test]
    fn fetch_with_data_length_16384bytes() {
        fetch_with_data_length_xbytes(&[0_u8; 16384], &[0x0, 0x80, 0x0, 0x40, 0x0]);
    }

    // Send 2 data frames so that the second one cannot fit into the send_buf and it is only
    // partialy sent. We check that the sent data is correct.
    #[allow(clippy::useless_vec)]
    fn fetch_with_two_data_frames(
        first_frame: &[u8],
        expected_first_data_frame_header: &[u8],
        expected_second_data_frame_header: &[u8],
        expected_second_data_frame: &[u8],
    ) {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(client.events().any(data_writable));

        // Send the first frame.
        let sent = client.send_data(request_stream_id, first_frame);
        assert_eq!(sent, Ok(first_frame.len()));

        // The second frame cannot fit.
        let sent = client.send_data(request_stream_id, &vec![0_u8; SEND_BUFFER_SIZE]);
        assert_eq!(sent, Ok(expected_second_data_frame.len()));

        // Close stream.
        client.stream_close_send(request_stream_id).unwrap();

        let mut out = client.process(None, now());
        // We need to loop a bit until all data has been sent. Once for every 1K
        // of data.
        for _i in 0..SEND_BUFFER_SIZE / 1000 {
            out = server.conn.process(out.dgram(), now());
            out = client.process(out.dgram(), now());
        }

        //  check received frames and send a response.
        while let Some(e) = server.conn.next_event() {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                if stream_id == request_stream_id {
                    // Read DATA frames.
                    let mut buf = vec![1_u8; RECV_BUFFER_SIZE];
                    let (amount, fin) = server.conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert!(fin);
                    assert_eq!(
                        amount,
                        expected_first_data_frame_header.len()
                            + first_frame.len()
                            + expected_second_data_frame_header.len()
                            + expected_second_data_frame.len()
                    );

                    // Check the first DATA frame header
                    let end = expected_first_data_frame_header.len();
                    assert_eq!(&buf[..end], expected_first_data_frame_header);

                    // Check the first frame data.
                    let start = end;
                    let end = end + first_frame.len();
                    assert_eq!(&buf[start..end], first_frame);

                    // Check the second DATA frame header
                    let start2 = end;
                    let end2 = end + expected_second_data_frame_header.len();
                    assert_eq!(&buf[start2..end2], expected_second_data_frame_header);

                    // Check the second frame data.
                    let start3 = end2;
                    let end3 = end2 + expected_second_data_frame.len();
                    assert_eq!(&buf[start3..end3], expected_second_data_frame);

                    // send response - 200  Content-Length: 3
                    // with content: 'abc'.
                    let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_2).unwrap();
                    server.conn.stream_close_send(stream_id).unwrap();
                }
            }
        }

        read_response(&mut client, &mut server.conn, request_stream_id);
    }

    fn alloc_buffer(size: usize) -> (Vec<u8>, Vec<u8>) {
        let data_frame = HFrame::Data { len: size as u64 };
        let mut enc = Encoder::default();
        data_frame.encode(&mut enc);

        (vec![0_u8; size], enc.to_vec())
    }

    // Send 2 frames. For the second one we can only send 63 bytes.
    // After the first frame there is exactly 63+2 bytes left in the send buffer.
    #[test]
    fn fetch_two_data_frame_second_63bytes() {
        let (buf, hdr) = alloc_buffer(SEND_BUFFER_SIZE - 88);
        fetch_with_two_data_frames(&buf, &hdr, &[0x0, 0x3f], &[0_u8; 63]);
    }

    // Send 2 frames. For the second one we can only send 63 bytes.
    // After the first frame there is exactly 63+3 bytes left in the send buffer,
    // but we can only send 63 bytes.
    #[test]
    fn fetch_two_data_frame_second_63bytes_place_for_66() {
        let (buf, hdr) = alloc_buffer(SEND_BUFFER_SIZE - 89);
        fetch_with_two_data_frames(&buf, &hdr, &[0x0, 0x3f], &[0_u8; 63]);
    }

    // Send 2 frames. For the second one we can only send 64 bytes.
    // After the first frame there is exactly 64+3 bytes left in the send buffer,
    // but we can only send 64 bytes.
    #[test]
    fn fetch_two_data_frame_second_64bytes_place_for_67() {
        let (buf, hdr) = alloc_buffer(SEND_BUFFER_SIZE - 90);
        fetch_with_two_data_frames(&buf, &hdr, &[0x0, 0x40, 0x40], &[0_u8; 64]);
    }

    // Send 2 frames. For the second one we can only send 16383 bytes.
    // After the first frame there is exactly 16383+3 bytes left in the send buffer.
    #[test]
    fn fetch_two_data_frame_second_16383bytes() {
        let (buf, hdr) = alloc_buffer(SEND_BUFFER_SIZE - 16409);
        fetch_with_two_data_frames(&buf, &hdr, &[0x0, 0x7f, 0xff], &[0_u8; 16383]);
    }

    // Send 2 frames. For the second one we can only send 16383 bytes.
    // After the first frame there is exactly 16383+4 bytes left in the send buffer, but we can only send 16383 bytes.
    #[test]
    fn fetch_two_data_frame_second_16383bytes_place_for_16387() {
        let (buf, hdr) = alloc_buffer(SEND_BUFFER_SIZE - 16410);
        fetch_with_two_data_frames(&buf, &hdr, &[0x0, 0x7f, 0xff], &[0_u8; 16383]);
    }

    // Send 2 frames. For the second one we can only send 16383 bytes.
    // After the first frame there is exactly 16383+5 bytes left in the send buffer, but we can only send 16383 bytes.
    #[test]
    fn fetch_two_data_frame_second_16383bytes_place_for_16388() {
        let (buf, hdr) = alloc_buffer(SEND_BUFFER_SIZE - 16411);
        fetch_with_two_data_frames(&buf, &hdr, &[0x0, 0x7f, 0xff], &[0_u8; 16383]);
    }

    // Send 2 frames. For the second one we can send 16384 bytes.
    // After the first frame there is exactly 16384+5 bytes left in the send buffer, but we can send 16384 bytes.
    #[test]
    fn fetch_two_data_frame_second_16384bytes_place_for_16389() {
        let (buf, hdr) = alloc_buffer(SEND_BUFFER_SIZE - 16412);
        fetch_with_two_data_frames(&buf, &hdr, &[0x0, 0x80, 0x0, 0x40, 0x0], &[0_u8; 16384]);
    }

    // Test receiving STOP_SENDING with the HttpNoError error code.
    #[test]
    fn test_stop_sending_early_response() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Stop sending with early_response.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpNoError.code())
        );

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );

        let mut stop_sending = false;
        let mut response_headers = false;
        let mut response_body = false;
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpNoError.code());
                    // assert that we cannot send any more request data.
                    assert_eq!(
                        Err(Error::InvalidStreamId),
                        client.send_data(request_stream_id, &[0_u8; 10])
                    );
                    stop_sending = true;
                }
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_response_header_2(&headers);
                    assert!(!fin);
                    assert!(!interim);
                    response_headers = true;
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0_u8; 100];
                    let (amount, fin) = client.read_data(now(), stream_id, &mut buf).unwrap();
                    assert!(fin);
                    assert_eq!(amount, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                    assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_2_FRAME_1);
                    response_body = true;
                }
                _ => {}
            }
        }
        assert!(response_headers);
        assert!(response_body);
        assert!(stop_sending);

        // after this stream will be removed from client. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0_u8; 100];
        let res = client.read_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    // Server sends stop sending and reset.
    #[test]
    fn test_stop_sending_other_error_with_reset() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Stop sending with RequestRejected.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestRejected.code())
        );
        // also reset with RequestRejected.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_reset_send(request_stream_id, Error::HttpRequestRejected.code())
        );

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut reset = false;
        let mut stop_sending = false;
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestRejected.code());
                    stop_sending = true;
                }
                Http3ClientEvent::Reset {
                    stream_id,
                    error,
                    local,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestRejected.code());
                    assert!(!local);
                    reset = true;
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        assert!(reset);
        assert!(stop_sending);

        // after this stream will be removed from client. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0_u8; 100];
        let res = client.read_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    // Server sends stop sending with RequestRejected, but it does not send reset.
    #[test]
    fn test_stop_sending_other_error_wo_reset() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Stop sending with RequestRejected.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestRejected.code())
        );

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut stop_sending = false;

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestRejected.code());
                    stop_sending = true;
                }
                Http3ClientEvent::Reset { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        assert!(stop_sending);

        // after this we can still read from a stream.
        let mut buf = [0_u8; 100];
        let res = client.read_data(now(), request_stream_id, &mut buf);
        assert!(res.is_ok());

        client.close(now(), 0, "");
    }

    // Server sends stop sending and reset. We have some events for that stream already
    // in client.events. The events will be removed.
    #[test]
    fn test_stop_sending_and_reset_other_error_with_events() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            false,
        );
        // At this moment we have some new events, i.e. a HeadersReady event

        // Send a stop sending and reset.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestCancelled.code())
        );
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_reset_send(request_stream_id, Error::HttpRequestCancelled.code())
        );

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut reset = false;

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
                }
                Http3ClientEvent::Reset {
                    stream_id,
                    error,
                    local,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
                    assert!(!local);
                    reset = true;
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        assert!(reset);

        // after this stream will be removed from client. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0_u8; 100];
        let res = client.read_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    // Server sends stop sending with code that is not HttpNoError.
    // We have some events for that stream already in the client.events.
    // The events will be removed.
    #[test]
    fn test_stop_sending_other_error_with_events() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            false,
        );
        // At this moment we have some new event, i.e. a HeadersReady event

        // Send a stop sending.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestCancelled.code())
        );

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut stop_sending = false;
        let mut header_ready = false;

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
                    stop_sending = true;
                }
                Http3ClientEvent::Reset { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    header_ready = true;
                }
                _ => {}
            }
        }

        assert!(stop_sending);
        assert!(header_ready);

        // after this, we can sill read data from a sttream.
        let mut buf = [0_u8; 100];
        let (amount, fin) = client
            .read_data(now(), request_stream_id, &mut buf)
            .unwrap();
        assert!(!fin);
        assert_eq!(amount, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
        assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_2_FRAME_1);

        client.close(now(), 0, "");
    }

    // Server sends a reset. We will close sending side as well.
    #[test]
    fn test_reset_wo_stop_sending() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Send a reset.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_reset_send(request_stream_id, Error::HttpRequestCancelled.code())
        );

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut reset = false;

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset {
                    stream_id,
                    error,
                    local,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
                    assert!(!local);
                    reset = true;
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        assert!(reset);

        // after this stream will be removed from client. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0_u8; 100];
        let res = client.read_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    fn test_incomplet_frame(buf: &[u8], error: &Error) {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            buf,
            true,
        );

        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::DataReadable { stream_id } = e {
                assert_eq!(stream_id, request_stream_id);
                let mut buf_res = [0_u8; 100];
                let res = client.read_data(now(), stream_id, &mut buf_res);
                assert!(res.is_err());
                assert_eq!(res.unwrap_err(), Error::HttpFrame);
            }
        }
        assert_closed(&client, error);
    }

    // Incomplete DATA frame
    #[test]
    fn test_incomplet_data_frame() {
        test_incomplet_frame(&HTTP_RESPONSE_2[..12], &Error::HttpFrame);
    }

    // Incomplete HEADERS frame
    #[test]
    fn test_incomplet_headers_frame() {
        test_incomplet_frame(&HTTP_RESPONSE_2[..7], &Error::HttpFrame);
    }

    #[test]
    fn test_incomplet_unknown_frame() {
        test_incomplet_frame(&[0x21], &Error::HttpFrame);
    }

    // test goaway
    #[test]
    fn test_goaway() {
        let (mut client, mut server) = connect();
        let request_stream_id_1 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_1, 0);
        let request_stream_id_2 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_2, 4);
        let request_stream_id_3 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_3, 8);

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &[0x7, 0x1, 0x8])
            .unwrap();

        // find the new request/response stream and send frame v on it.
        while let Some(e) = server.conn.next_event() {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                let mut buf = [0_u8; 100];
                let _ = server.conn.stream_recv(stream_id, &mut buf).unwrap();
                if (stream_id == request_stream_id_1) || (stream_id == request_stream_id_2) {
                    // send response - 200  Content-Length: 7
                    // with content: 'abcdefg'.
                    // The content will be send in 2 DATA frames.
                    let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_1).unwrap();
                    server.conn.stream_close_send(stream_id).unwrap();
                }
            }
        }
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut stream_reset = false;
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { headers, fin, .. } => {
                    check_response_header_1(&headers);
                    assert!(!fin);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert!(
                        (stream_id == request_stream_id_1) || (stream_id == request_stream_id_2)
                    );
                    let mut buf = [0_u8; 100];
                    assert_eq!(
                        (EXPECTED_RESPONSE_DATA_1.len(), true),
                        client.read_data(now(), stream_id, &mut buf).unwrap()
                    );
                }
                Http3ClientEvent::Reset {
                    stream_id,
                    error,
                    local,
                } => {
                    assert_eq!(stream_id, request_stream_id_3);
                    assert_eq!(error, Error::HttpRequestRejected.code());
                    assert!(!local);
                    stream_reset = true;
                }
                _ => {}
            }
        }

        assert!(stream_reset);
        assert_eq!(client.state(), Http3State::GoingAway(StreamId::new(8)));

        // Check that a new request cannot be made.
        assert_eq!(
            client.fetch(
                now(),
                "GET",
                &("https", "something.com", "/"),
                &[],
                Priority::default()
            ),
            Err(Error::AlreadyClosed)
        );

        client.close(now(), 0, "");
    }

    #[test]
    fn multiple_goaways() {
        let (mut client, mut server) = connect();
        let request_stream_id_1 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_1, 0);
        let request_stream_id_2 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_2, 4);
        let request_stream_id_3 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_3, 8);

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        // First send a Goaway frame with an higher number
        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &[0x7, 0x1, 0x8])
            .unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Check that there is one reset for stream_id 8
        let mut stream_reset_1 = 0;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::Reset {
                stream_id,
                error,
                local,
            } = e
            {
                assert_eq!(stream_id, request_stream_id_3);
                assert_eq!(error, Error::HttpRequestRejected.code());
                assert!(!local);
                stream_reset_1 += 1;
            }
        }

        assert_eq!(stream_reset_1, 1);
        assert_eq!(client.state(), Http3State::GoingAway(StreamId::new(8)));

        // Server sends another GOAWAY frame
        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &[0x7, 0x1, 0x4])
            .unwrap();

        // Send response for stream 0
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id_1,
            HTTP_RESPONSE_1,
            true,
        );

        let mut stream_reset_2 = 0;
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { headers, fin, .. } => {
                    check_response_header_1(&headers);
                    assert!(!fin);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert!(stream_id == request_stream_id_1);
                    let mut buf = [0_u8; 100];
                    assert_eq!(
                        (EXPECTED_RESPONSE_DATA_1.len(), true),
                        client.read_data(now(), stream_id, &mut buf).unwrap()
                    );
                }
                Http3ClientEvent::Reset {
                    stream_id,
                    error,
                    local,
                } => {
                    assert_eq!(stream_id, request_stream_id_2);
                    assert_eq!(error, Error::HttpRequestRejected.code());
                    assert!(!local);
                    stream_reset_2 += 1;
                }
                _ => {}
            }
        }

        assert_eq!(stream_reset_2, 1);
        assert_eq!(client.state(), Http3State::GoingAway(StreamId::new(4)));
    }

    #[test]
    fn multiple_goaways_stream_id_increased() {
        let (mut client, mut server) = connect();
        let request_stream_id_1 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_1, 0);
        let request_stream_id_2 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_2, 4);
        let request_stream_id_3 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_3, 8);

        // First send a Goaway frame with a smaller number
        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &[0x7, 0x1, 0x4])
            .unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        assert_eq!(client.state(), Http3State::GoingAway(StreamId::new(4)));

        // Now send a Goaway frame with an higher number
        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &[0x7, 0x1, 0x8])
            .unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        assert_closed(&client, &Error::HttpGeneralProtocol);
    }

    #[test]
    fn goaway_wrong_stream_id() {
        let (mut client, mut server) = connect();

        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &[0x7, 0x1, 0x9])
            .unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        assert_closed(&client, &Error::HttpId);
    }

    // Close stream before headers.
    #[test]
    fn test_stream_fin_wo_headers() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        // send fin before sending any data.
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv HeaderReady wo headers with fin.
        let e = client.events().next().unwrap();
        assert_eq!(
            e,
            Http3ClientEvent::Reset {
                stream_id: request_stream_id,
                error: Error::HttpGeneralProtocolStream.code(),
                local: true,
            }
        );

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Close stream imemediately after headers.
    #[test]
    fn test_stream_fin_after_headers() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_HEADER_ONLY_2,
            true,
        );

        // Recv HeaderReady with headers and fin.
        let e = client.events().next().unwrap();
        if let Http3ClientEvent::HeaderReady {
            stream_id,
            headers,
            interim,
            fin,
        } = e
        {
            assert_eq!(stream_id, request_stream_id);
            check_response_header_2(&headers);
            assert!(fin);
            assert!(!interim);
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers, read headers and than close stream.
    // We should get HeaderReady and a DataReadable
    #[test]
    fn test_stream_fin_after_headers_are_read_wo_data_frame() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        // Send some good data wo fin
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_HEADER_ONLY_2,
            false,
        );

        // Recv headers wo fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_response_header_2(&headers);
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not receive a DataGeadable event!");
                }
                _ => {}
            };
        }

        // ok NOW send fin
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv DataReadable wo data with fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { .. } => {
                    panic!("We should not get another HeaderReady!");
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0_u8; 100];
                    let res = client.read_data(now(), stream_id, &mut buf);
                    let (len, fin) = res.expect("should read");
                    assert_eq!(0, len);
                    assert!(fin);
                }
                _ => {}
            };
        }

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers and an empty data frame, then close the stream.
    #[test]
    fn test_stream_fin_after_headers_and_a_empty_data_frame() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send headers.
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_RESPONSE_HEADER_ONLY_2)
            .unwrap();
        // Send an empty data frame.
        let _ = server
            .conn
            .stream_send(request_stream_id, &[0x00, 0x00])
            .unwrap();
        // ok NOW send fin
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv HeaderReady with fin.
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_response_header_2(&headers);
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0_u8; 100];
                    assert_eq!(Ok((0, true)), client.read_data(now(), stream_id, &mut buf));
                }
                _ => {}
            };
        }

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), request_stream_id, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers and an empty data frame. Read headers and then close the stream.
    // We should get a HeaderReady without fin and a DataReadable wo data and with fin.
    #[test]
    fn test_stream_fin_after_headers_an_empty_data_frame_are_read() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        // Send some good data wo fin
        // Send headers.
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_RESPONSE_HEADER_ONLY_2)
            .unwrap();
        // Send an empty data frame.
        let _ = server
            .conn
            .stream_send(request_stream_id, &[0x00, 0x00])
            .unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv headers wo fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_response_header_2(&headers);
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not receive a DataGeadable event!");
                }
                _ => {}
            };
        }

        // ok NOW send fin
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv no data, but do get fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { .. } => {
                    panic!("We should not get another HeaderReady!");
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0_u8; 100];
                    let res = client.read_data(now(), stream_id, &mut buf);
                    let (len, fin) = res.expect("should read");
                    assert_eq!(0, len);
                    assert!(fin);
                }
                _ => {}
            };
        }

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_stream_fin_after_a_data_frame() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        // Send some good data wo fin
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            false,
        );

        // Recv some good data wo fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    interim,
                    fin,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_response_header_2(&headers);
                    assert!(!fin);
                    assert!(!interim);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0_u8; 100];
                    let res = client.read_data(now(), stream_id, &mut buf);
                    let (len, fin) = res.expect("should have data");
                    assert_eq!(len, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                    assert_eq!(&buf[..len], EXPECTED_RESPONSE_DATA_2_FRAME_1);
                    assert!(!fin);
                }
                _ => {}
            };
        }

        // ok NOW send fin
        server.conn.stream_close_send(request_stream_id).unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // fin wo data should generate DataReadable
        let e = client.events().next().unwrap();
        if let Http3ClientEvent::DataReadable { stream_id } = e {
            assert_eq!(stream_id, request_stream_id);
            let mut buf = [0; 100];
            let res = client.read_data(now(), stream_id, &mut buf);
            let (len, fin) = res.expect("should read");
            assert_eq!(0, len);
            assert!(fin);
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_multiple_data_frames() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send two data frames with fin
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_1,
            true,
        );

        // Read first frame
        match client.events().nth(1).unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0_u8; 100];
                assert_eq!(
                    (EXPECTED_RESPONSE_DATA_1.len(), true),
                    client.read_data(now(), stream_id, &mut buf).unwrap()
                );
            }
            x => {
                panic!("event {:?}", x);
            }
        }

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_receive_grease_before_response() {
        // Construct an unknown frame.
        const UNKNOWN_FRAME_LEN: usize = 832;

        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
        enc.encode_varint(1028_u64); // Arbitrary type.
        enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
        let mut buf: Vec<_> = enc.into();
        buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
        let _ = server.conn.stream_send(request_stream_id, &buf).unwrap();

        // Send a headers and a data frame with fin
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );

        // Read first frame
        match client.events().nth(1).unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0_u8; 100];
                let (len, fin) = client.read_data(now(), stream_id, &mut buf).unwrap();
                assert_eq!(len, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                assert_eq!(&buf[..len], EXPECTED_RESPONSE_DATA_2_FRAME_1);
                assert!(fin);
            }
            x => {
                panic!("event {:?}", x);
            }
        }
        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_read_frames_header_blocked() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let headers = vec![
            Header::new(":status", "200"),
            Header::new("my-header", "my-header"),
            Header::new("content-length", "3"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        // Send the encoder instructions, but delay them so that the stream is blocked on decoding headers.
        let encoder_inst_pkt = server.conn.process(None, now());

        // Send response
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        let d_frame = HFrame::Data { len: 3 };
        d_frame.encode(&mut d);
        d.encode(&[0x61, 0x62, 0x63]);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            true,
        );

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!client.events().any(header_ready_event));

        // Let client receive the encoder instructions.
        mem::drop(client.process(encoder_inst_pkt.dgram(), now()));

        let out = server.conn.process(None, now());
        mem::drop(client.process(out.dgram(), now()));
        mem::drop(client.process(None, now()));

        let mut recv_header = false;
        let mut recv_data = false;
        // Now the stream is unblocked and both headers and data will be consumed.
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id, .. } => {
                    assert_eq!(stream_id, request_stream_id);
                    recv_header = true;
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    recv_data = true;
                    assert_eq!(stream_id, request_stream_id);
                }
                x => {
                    panic!("event {:?}", x);
                }
            }
        }
        assert!(recv_header && recv_data);
    }

    #[test]
    fn test_read_frames_header_blocked_with_fin_after_headers() {
        let (mut hconn, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut hconn, &mut server);

        let sent_headers = vec![
            Header::new(":status", "200"),
            Header::new("my-header", "my-header"),
            Header::new("content-length", "0"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &sent_headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        // Send the encoder instructions, but delay them so that the stream is blocked on decoding headers.
        let encoder_inst_pkt = server.conn.process(None, now());

        let mut d = Encoder::default();
        hframe.encode(&mut d);

        server_send_response_and_exchange_packet(
            &mut hconn,
            &mut server,
            request_stream_id,
            &d,
            true,
        );

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!hconn.events().any(header_ready_event));

        // Let client receive the encoder instructions.
        let _out = hconn.process(encoder_inst_pkt.dgram(), now());

        let mut recv_header = false;
        // Now the stream is unblocked. After headers we will receive a fin.
        while let Some(e) = hconn.next_event() {
            if let Http3ClientEvent::HeaderReady {
                stream_id,
                headers,
                interim,
                fin,
            } = e
            {
                assert_eq!(stream_id, request_stream_id);
                assert_eq!(headers.as_ref(), sent_headers);
                assert!(fin);
                assert!(!interim);
                recv_header = true;
            } else {
                panic!("event {:?}", e);
            }
        }
        assert!(recv_header);
    }

    fn exchange_token(client: &mut Http3Client, server: &mut Connection) -> ResumptionToken {
        server.send_ticket(now(), &[]).expect("can send ticket");
        let out = server.process_output(now());
        assert!(out.as_dgram_ref().is_some());
        client.process_input(out.dgram().unwrap(), now());
        // We do not have a token so we need to wait for a resumption token timer to trigger.
        client.process_output(now() + Duration::from_millis(250));
        assert_eq!(client.state(), Http3State::Connected);
        client
            .events()
            .find_map(|e| {
                if let Http3ClientEvent::ResumptionToken(token) = e {
                    Some(token)
                } else {
                    None
                }
            })
            .unwrap()
    }

    fn start_with_0rtt() -> (Http3Client, TestServer) {
        let (mut client, mut server) = connect();
        let token = exchange_token(&mut client, &mut server.conn);

        let mut client = default_http3_client();

        let server = TestServer::new();

        assert_eq!(client.state(), Http3State::Initializing);
        client
            .enable_resumption(now(), &token)
            .expect("Set resumption token.");

        assert_eq!(client.state(), Http3State::ZeroRtt);
        let zerortt_event = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::ZeroRtt));
        assert!(client.events().any(zerortt_event));

        (client, server)
    }

    #[test]
    fn zero_rtt_negotiated() {
        let (mut client, mut server) = start_with_0rtt();

        let out = client.process(None, now());

        assert_eq!(client.state(), Http3State::ZeroRtt);
        assert_eq!(*server.conn.state(), State::Init);
        let out = server.conn.process(out.dgram(), now());

        // Check that control and qpack streams are received and a
        // SETTINGS frame has been received.
        // Also qpack encoder stream will send "change capacity" instruction because it has
        // the peer settings already.
        server.check_control_qpack_request_streams_resumption(
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
            EXPECTED_REQUEST_HEADER_FRAME,
            false,
        );

        assert_eq!(*server.conn.state(), State::Handshaking);
        let out = client.process(out.dgram(), now());
        assert_eq!(client.state(), Http3State::Connected);

        mem::drop(server.conn.process(out.dgram(), now()));
        assert!(server.conn.state().connected());

        assert!(client.tls_info().unwrap().resumed());
        assert!(server.conn.tls_info().unwrap().resumed());
    }

    #[test]
    fn zero_rtt_send_request() {
        let (mut client, mut server) = start_with_0rtt();

        let request_stream_id =
            make_request(&mut client, true, &[Header::new("myheaders", "myvalue")]);
        assert_eq!(request_stream_id, 0);

        let out = client.process(None, now());

        assert_eq!(client.state(), Http3State::ZeroRtt);
        assert_eq!(*server.conn.state(), State::Init);
        let out = server.conn.process(out.dgram(), now());

        // Check that control and qpack streams are received and a
        // SETTINGS frame has been received.
        // Also qpack encoder stream will send "change capacity" instruction because it has
        // the peer settings already.
        server.check_control_qpack_request_streams_resumption(
            ENCODER_STREAM_DATA_WITH_CAP_INST_AND_ENCODING_INST,
            EXPECTED_REQUEST_HEADER_FRAME_VERSION2,
            true,
        );

        assert_eq!(*server.conn.state(), State::Handshaking);
        let out = client.process(out.dgram(), now());
        assert_eq!(client.state(), Http3State::Connected);
        let out = server.conn.process(out.dgram(), now());
        assert!(server.conn.state().connected());
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());

        // After the server has been connected, send a response.
        let res = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_2);
        assert_eq!(res, Ok(HTTP_RESPONSE_2.len()));
        server.conn.stream_close_send(request_stream_id).unwrap();

        read_response(&mut client, &mut server.conn, request_stream_id);

        assert!(client.tls_info().unwrap().resumed());
        assert!(server.conn.tls_info().unwrap().resumed());
    }

    #[test]
    fn zero_rtt_before_resumption_token() {
        let mut client = default_http3_client();
        assert!(client
            .fetch(
                now(),
                "GET",
                &("https", "something.com", "/"),
                &[],
                Priority::default()
            )
            .is_err());
    }

    #[test]
    fn zero_rtt_send_reject() {
        let (mut client, mut server) = connect();
        let token = exchange_token(&mut client, &mut server.conn);

        let mut client = default_http3_client();
        let mut server = Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN_H3,
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            ConnectionParameters::default(),
        )
        .unwrap();
        // Using a freshly initialized anti-replay context
        // should result in the server rejecting 0-RTT.
        let ar = AntiReplay::new(now(), test_fixture::ANTI_REPLAY_WINDOW, 1, 3)
            .expect("setup anti-replay");
        server
            .server_enable_0rtt(&ar, AllowZeroRtt {})
            .expect("enable 0-RTT");

        assert_eq!(client.state(), Http3State::Initializing);
        client
            .enable_resumption(now(), &token)
            .expect("Set resumption token.");
        let zerortt_event = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::ZeroRtt));
        assert!(client.events().any(zerortt_event));

        // Send ClientHello.
        let client_hs = client.process(None, now());
        assert!(client_hs.as_dgram_ref().is_some());

        // Create a request
        let request_stream_id = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id, 0);

        let client_0rtt = client.process(None, now());
        assert!(client_0rtt.as_dgram_ref().is_some());

        let server_hs = server.process(client_hs.dgram(), now());
        assert!(server_hs.as_dgram_ref().is_some()); // Should produce ServerHello etc...
        let server_ignored = server.process(client_0rtt.dgram(), now());
        assert!(server_ignored.as_dgram_ref().is_none());

        // The server shouldn't receive that 0-RTT data.
        let recvd_stream_evt = |e| matches!(e, ConnectionEvent::NewStream { .. });
        assert!(!server.events().any(recvd_stream_evt));

        // Client should get a rejection.
        let client_out = client.process(server_hs.dgram(), now());
        assert!(client_out.as_dgram_ref().is_some());
        let recvd_0rtt_reject = |e| e == Http3ClientEvent::ZeroRttRejected;
        assert!(client.events().any(recvd_0rtt_reject));

        // ...and the client stream should be gone.
        let res = client.stream_close_send(request_stream_id);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        // Client will send Setting frame and open new qpack streams.
        mem::drop(server.process(client_out.dgram(), now()));
        TestServer::new_with_conn(server).check_client_control_qpack_streams_no_resumption();

        // Check that we can send a request and that the stream_id starts again from 0.
        assert_eq!(make_request(&mut client, false, &[]), 0);
    }

    // Connect to a server, get token and reconnect using 0-rtt. Seerver sends new Settings.
    fn zero_rtt_change_settings(
        original_settings: &[HSetting],
        resumption_settings: &[HSetting],
        expected_client_state: &Http3State,
        expected_encoder_stream_data: &[u8],
    ) {
        let mut client = default_http3_client();
        let mut server = TestServer::new_with_settings(original_settings);
        // Connect and get a token
        connect_with(&mut client, &mut server);
        let token = exchange_token(&mut client, &mut server.conn);

        let mut client = default_http3_client();
        let mut server = TestServer::new_with_settings(resumption_settings);
        assert_eq!(client.state(), Http3State::Initializing);
        client
            .enable_resumption(now(), &token)
            .expect("Set resumption token.");
        assert_eq!(client.state(), Http3State::ZeroRtt);
        let out = client.process(None, now());

        assert_eq!(client.state(), Http3State::ZeroRtt);
        assert_eq!(*server.conn.state(), State::Init);
        let out = server.conn.process(out.dgram(), now());

        // Check that control and qpack streams anda SETTINGS frame are received.
        // Also qpack encoder stream will send "change capacity" instruction because it has
        // the peer settings already.
        server.check_control_qpack_request_streams_resumption(
            expected_encoder_stream_data,
            EXPECTED_REQUEST_HEADER_FRAME,
            false,
        );

        assert_eq!(*server.conn.state(), State::Handshaking);
        let out = client.process(out.dgram(), now());
        assert_eq!(client.state(), Http3State::Connected);

        mem::drop(server.conn.process(out.dgram(), now()));
        assert!(server.conn.state().connected());

        assert!(client.tls_info().unwrap().resumed());
        assert!(server.conn.tls_info().unwrap().resumed());

        // Send new settings.
        let control_stream = server.conn.stream_create(StreamType::UniDi).unwrap();
        let mut enc = Encoder::default();
        server.settings.encode(&mut enc);
        let mut sent = server.conn.stream_send(control_stream, CONTROL_STREAM_TYPE);
        assert_eq!(sent.unwrap(), CONTROL_STREAM_TYPE.len());
        sent = server.conn.stream_send(control_stream, &enc);
        assert_eq!(sent.unwrap(), enc.len());

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        assert_eq!(&client.state(), expected_client_state);
        assert!(server.conn.state().connected());
    }

    #[test]
    fn zero_rtt_new_server_setting_are_the_same() {
        // Send a new server settings that are the same as the old one.
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Connected,
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_omit_max_table() {
        // Send a new server settings without MaxTableCapacity
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Closing(ConnectionError::Application(265)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_omit_blocked_streams() {
        // Send a new server settings without BlockedStreams
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Closing(ConnectionError::Application(265)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_omit_header_list_size() {
        // Send a new server settings without MaxHeaderListSize
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
            ],
            &Http3State::Connected,
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_max_table_size_bigger() {
        // Send a new server settings MaxTableCapacity=200
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 200),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Closing(ConnectionError::Application(514)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_max_table_size_smaller() {
        // Send a new server settings MaxTableCapacity=50
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 50),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Closing(ConnectionError::Application(265)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_blocked_streams_bigger() {
        // Send a new server settings withBlockedStreams=200
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 200),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Connected,
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_blocked_streams_smaller() {
        // Send a new server settings withBlockedStreams=50
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 50),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Closing(ConnectionError::Application(265)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_max_header_size_bigger() {
        // Send a new server settings with MaxHeaderListSize=20000
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 20000),
            ],
            &Http3State::Connected,
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_new_server_setting_max_headers_size_smaller() {
        // Send the new server settings with MaxHeaderListSize=5000
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 5000),
            ],
            &Http3State::Closing(ConnectionError::Application(265)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_max_table_size_first_omitted() {
        // send server original settings without MaxTableCapacity
        // send new server setting with MaxTableCapacity
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Connected,
            ENCODER_STREAM_DATA,
        );
    }

    #[test]
    fn zero_rtt_blocked_streams_first_omitted() {
        // Send server original settings without BlockedStreams
        // Send the new server settings with BlockedStreams
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Connected,
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn zero_rtt_max_header_size_first_omitted() {
        // Send server settings without MaxHeaderListSize
        // Send new settings with MaxHeaderListSize.
        zero_rtt_change_settings(
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 10000),
            ],
            &[
                HSetting::new(HSettingType::MaxTableCapacity, 100),
                HSetting::new(HSettingType::BlockedStreams, 100),
                HSetting::new(HSettingType::MaxHeaderListSize, 10000),
            ],
            &Http3State::Closing(ConnectionError::Application(265)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn test_trailers_with_fin_after_headers() {
        // Make a new connection.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send HEADER frame.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_HEADER_FRAME_0,
            false,
        );

        // Check response headers.
        let mut response_headers = false;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::HeaderReady {
                stream_id,
                headers,
                interim,
                fin,
            } = e
            {
                assert_eq!(stream_id, request_stream_id);
                check_response_header_0(&headers);
                assert!(!fin);
                assert!(!interim);
                response_headers = true;
            }
        }
        assert!(response_headers);

        // Send trailers
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_HEADER_FRAME_0,
            true,
        );

        let events: Vec<Http3ClientEvent> = client.events().collect();

        // We already had HeaderReady
        let header_ready: fn(&Http3ClientEvent) -> _ =
            |e| matches!(*e, Http3ClientEvent::HeaderReady { .. });
        assert!(!events.iter().any(header_ready));

        // Check that we have a DataReady event. Reading from the stream will return fin=true.
        let data_readable: fn(&Http3ClientEvent) -> _ =
            |e| matches!(*e, Http3ClientEvent::DataReadable { .. });
        assert!(events.iter().any(data_readable));
        let mut buf = [0_u8; 100];
        let (len, fin) = client
            .read_data(now(), request_stream_id, &mut buf)
            .unwrap();
        assert_eq!(0, len);
        assert!(fin);
    }

    #[test]
    fn test_trailers_with_later_fin_after_headers() {
        // Make a new connection.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send HEADER frame.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_HEADER_FRAME_0,
            false,
        );

        // Check response headers.
        let mut response_headers = false;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::HeaderReady {
                stream_id,
                headers,
                interim,
                fin,
            } = e
            {
                assert_eq!(stream_id, request_stream_id);
                check_response_header_0(&headers);
                assert!(!fin);
                assert!(!interim);
                response_headers = true;
            }
        }
        assert!(response_headers);

        // Send trailers
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_HEADER_FRAME_0,
            false,
        );

        // Check that we do not have a DataReady event.
        let data_readable = |e| matches!(e, Http3ClientEvent::DataReadable { .. });
        assert!(!client.events().any(data_readable));

        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let events: Vec<Http3ClientEvent> = client.events().collect();

        // We already had HeaderReady
        let header_ready: fn(&Http3ClientEvent) -> _ =
            |e| matches!(*e, Http3ClientEvent::HeaderReady { .. });
        assert!(!events.iter().any(header_ready));

        // Check that we have a DataReady event. Reading from the stream will return fin=true.
        let data_readable_fn: fn(&Http3ClientEvent) -> _ =
            |e| matches!(*e, Http3ClientEvent::DataReadable { .. });
        assert!(events.iter().any(data_readable_fn));
        let mut buf = [0_u8; 100];
        let (len, fin) = client
            .read_data(now(), request_stream_id, &mut buf)
            .unwrap();
        assert_eq!(0, len);
        assert!(fin);
    }

    #[test]
    fn test_data_after_trailers_after_headers() {
        // Make a new connection.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send HEADER frame.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_HEADER_FRAME_0,
            false,
        );

        // Check response headers.
        let mut response_headers = false;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::HeaderReady {
                stream_id,
                headers,
                interim,
                fin,
            } = e
            {
                assert_eq!(stream_id, request_stream_id);
                check_response_header_0(&headers);
                assert!(!fin);
                assert!(!interim);
                response_headers = true;
            }
        }
        assert!(response_headers);

        // Send trailers
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_HEADER_FRAME_0,
            false,
        );

        // Check that we do not have a DataReady event.
        let data_readable = |e| matches!(e, Http3ClientEvent::DataReadable { .. });
        assert!(!client.events().any(data_readable));

        // Send Data frame.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &[0x0, 0x3, 0x61, 0x62, 0x63], // a data frame
            false,
        );

        assert_closed(&client, &Error::HttpFrameUnexpected);
    }

    #[test]
    fn transport_stream_readable_event_after_all_data() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Send headers.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            false,
        );

        // Send an empty data frame and a fin
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &[0x0, 0x0],
            true,
        );

        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Ok((3, true))
        );

        client.process(None, now());
    }

    #[test]
    fn no_data_ready_events_after_fin() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // send response - 200  Content-Length: 7
        // with content: 'abcdefg'.
        // The content will be send in 2 DATA frames.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_1,
            true,
        );

        let data_readable_event = |e| matches!(e, Http3ClientEvent::DataReadable { stream_id } if stream_id == request_stream_id);
        assert!(client.events().any(data_readable_event));

        let mut buf = [0_u8; 100];
        assert_eq!(
            (EXPECTED_RESPONSE_DATA_1.len(), true),
            client
                .read_data(now(), request_stream_id, &mut buf)
                .unwrap()
        );

        assert!(!client.events().any(data_readable_event));
    }

    #[test]
    fn reading_small_chunks_of_data() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // send response - 200  Content-Length: 7
        // with content: 'abcdefg'.
        // The content will be send in 2 DATA frames.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_1,
            true,
        );

        let data_readable_event = |e| matches!(e, Http3ClientEvent::DataReadable { stream_id } if stream_id == request_stream_id);
        assert!(client.events().any(data_readable_event));

        let mut buf1 = [0_u8; 1];
        assert_eq!(
            (1, false),
            client
                .read_data(now(), request_stream_id, &mut buf1)
                .unwrap()
        );
        assert!(!client.events().any(data_readable_event));

        // Now read only until the end of the first frame. The firs framee has 3 bytes.
        let mut buf2 = [0_u8; 2];
        assert_eq!(
            (2, false),
            client
                .read_data(now(), request_stream_id, &mut buf2)
                .unwrap()
        );
        assert!(!client.events().any(data_readable_event));

        // Read a half of the second frame.
        assert_eq!(
            (2, false),
            client
                .read_data(now(), request_stream_id, &mut buf2)
                .unwrap()
        );
        assert!(!client.events().any(data_readable_event));

        // Read the rest.
        // Read a half of the second frame.
        assert_eq!(
            (2, true),
            client
                .read_data(now(), request_stream_id, &mut buf2)
                .unwrap()
        );
        assert!(!client.events().any(data_readable_event));
    }

    #[test]
    fn zero_length_data_at_end() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // send response - 200  Content-Length: 7
        // with content: 'abcdefg'.
        // The content will be send in 2 DATA frames.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_1,
            false,
        );
        // Send a zero-length frame at the end of the stream.
        let _ = server.conn.stream_send(request_stream_id, &[0, 0]).unwrap();
        server.conn.stream_close_send(request_stream_id).unwrap();
        let dgram = server.conn.process_output(now()).dgram();
        client.process_input(dgram.unwrap(), now());

        let data_readable_event = |e: &_| matches!(e, Http3ClientEvent::DataReadable { stream_id } if *stream_id == request_stream_id);
        assert_eq!(client.events().filter(data_readable_event).count(), 1);

        let mut buf = [0_u8; 10];
        assert_eq!(
            (7, true),
            client
                .read_data(now(), request_stream_id, &mut buf)
                .unwrap()
        );
        assert!(!client.events().any(|e| data_readable_event(&e)));
    }

    #[test]
    fn stream_blocked_no_remote_encoder_stream() {
        let (mut client, mut server) = connect_only_transport();

        send_and_receive_client_settings(&mut client, &mut server);

        server.create_control_stream();
        // Send the server's control stream data.
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        server.create_qpack_streams();
        let qpack_pkt1 = server.conn.process(None, now());
        // delay delivery of this packet.

        let request_stream_id = make_request(&mut client, true, &[]);
        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        setup_server_side_encoder(&mut client, &mut server);

        let headers = vec![
            Header::new(":status", "200"),
            Header::new("my-header", "my-header"),
            Header::new("content-length", "3"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        // Send the encoder instructions,
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Send response
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        let d_frame = HFrame::Data { len: 3 };
        d_frame.encode(&mut d);
        d.encode(&[0x61, 0x62, 0x63]);
        let _ = server.conn.stream_send(request_stream_id, &d[..]).unwrap();
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        mem::drop(client.process(out.dgram(), now()));

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!client.events().any(header_ready_event));

        // Let client receive the encoder instructions.
        mem::drop(client.process(qpack_pkt1.dgram(), now()));

        assert!(client.events().any(header_ready_event));
    }

    // Client: receive a push stream
    #[test]
    fn push_single() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send a push promise.
        send_push_promise(&mut server.conn, request_stream_id, 0);

        // create a push stream.
        let _ = send_push_data(&mut server.conn, 0, true);

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );

        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 0,
                ref_stream_id: request_stream_id,
            }],
            &[0],
            request_stream_id,
        );

        assert_eq!(client.state(), Http3State::Connected);

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));
    }

    /// We can't keep the connection alive on the basis of a push promise,
    /// nor do we want to if the push promise is not interesting to the client.
    /// We do the next best thing, which is keep any push stream alive if the
    /// client reads from it.
    #[test]
    fn push_keep_alive() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        let idle_timeout = ConnectionParameters::default().get_idle_timeout();

        // Promise a push and deliver, but don't close the stream.
        send_push_promise(&mut server.conn, request_stream_id, 0);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );
        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 0,
                ref_stream_id: request_stream_id,
            }],
            &[], // No push streams yet.
            request_stream_id,
        );

        // The client will become idle here.
        force_idle(&mut client, &mut server);
        assert_eq!(client.process_output(now()).callback(), idle_timeout);

        // Reading push data will stop the client from being idle.
        let _ = send_push_data(&mut server.conn, 0, false);
        let dgram = server.conn.process_output(now()).dgram();
        client.process_input(dgram.unwrap(), now());

        let mut buf = [0; 16];
        let (read, fin) = client.push_read_data(now(), 0, &mut buf).unwrap();
        assert!(read < buf.len());
        assert!(!fin);

        force_idle(&mut client, &mut server);
        assert_eq!(client.process_output(now()).callback(), idle_timeout / 2);
    }

    #[test]
    fn push_multiple() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send a push promise.
        send_push_promise(&mut server.conn, request_stream_id, 0);
        send_push_promise(&mut server.conn, request_stream_id, 1);

        // create a push stream.
        let _ = send_push_data(&mut server.conn, 0, true);

        // create a second push stream.
        let _ = send_push_data(&mut server.conn, 1, true);

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );

        read_response_and_push_events(
            &mut client,
            &[
                PushPromiseInfo {
                    push_id: 0,
                    ref_stream_id: request_stream_id,
                },
                PushPromiseInfo {
                    push_id: 1,
                    ref_stream_id: request_stream_id,
                },
            ],
            &[0, 1],
            request_stream_id,
        );

        assert_eq!(client.state(), Http3State::Connected);

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));
        assert_eq!(client.cancel_push(1), Err(Error::InvalidStreamId));
    }

    #[test]
    fn push_after_headers() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send response headers
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_RESPONSE_HEADER_ONLY_2)
            .unwrap();

        // Send a push promise.
        send_push_promise(&mut server.conn, request_stream_id, 0);

        // create a push stream.
        let _ = send_push_data(&mut server.conn, 0, true);

        // Send response data
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_DATA_FRAME_ONLY_2,
            true,
        );

        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 0,
                ref_stream_id: request_stream_id,
            }],
            &[0],
            request_stream_id,
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    #[test]
    fn push_after_response() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send response headers and data frames
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_RESPONSE_2)
            .unwrap();

        // Send a push promise.
        send_push_promise(&mut server.conn, request_stream_id, 0);
        // create a push stream.
        send_push_data_and_exchange_packets(&mut client, &mut server, 0, true);

        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 0,
                ref_stream_id: request_stream_id,
            }],
            &[0],
            request_stream_id,
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    fn check_push_events(client: &mut Http3Client) -> bool {
        let any_push_event = |e| {
            matches!(
                e,
                Http3ClientEvent::PushPromise { .. }
                    | Http3ClientEvent::PushHeaderReady { .. }
                    | Http3ClientEvent::PushDataReadable { .. }
            )
        };
        client.events().any(any_push_event)
    }

    fn check_data_readable(client: &mut Http3Client) -> bool {
        let any_data_event = |e| matches!(e, Http3ClientEvent::DataReadable { .. });
        client.events().any(any_data_event)
    }

    fn check_header_ready(client: &mut Http3Client) -> bool {
        let any_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        client.events().any(any_event)
    }

    fn check_header_ready_and_push_promise(client: &mut Http3Client) -> bool {
        let any_event = |e| {
            matches!(
                e,
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::PushPromise { .. }
            )
        };
        client.events().any(any_event)
    }

    #[test]
    fn push_stream_before_promise() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // create a push stream.
        send_push_data_and_exchange_packets(&mut client, &mut server, 0, true);

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Now send push_promise
        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 0);

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );

        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 0,
                ref_stream_id: request_stream_id,
            }],
            &[0],
            request_stream_id,
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test receiving pushes out of order.
    // Push_id 5 is received first, therefore Push_id 3 will be in the PushState:Init state.
    // Start push_id 3 by receiving a push_promise and then a push stream with the push_id 3.
    #[test]
    fn push_out_of_order_1() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 5);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 3);
        // Start a push stream with push_id 3.
        send_push_data_and_exchange_packets(&mut client, &mut server, 3, true);

        assert_eq!(client.state(), Http3State::Connected);

        read_response_and_push_events(
            &mut client,
            &[
                PushPromiseInfo {
                    push_id: 5,
                    ref_stream_id: request_stream_id,
                },
                PushPromiseInfo {
                    push_id: 3,
                    ref_stream_id: request_stream_id,
                },
            ],
            &[3],
            request_stream_id,
        );
        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test receiving pushes out of order.
    // Push_id 5 is received first, therefore Push_id 3 will be in the PushState:Init state.
    // Start push_id 3 by receiving a push stream with push_id 3 and then a push_promise.
    #[test]
    fn push_out_of_order_2() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 5);

        send_push_data_and_exchange_packets(&mut client, &mut server, 3, true);
        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 3);

        read_response_and_push_events(
            &mut client,
            &[
                PushPromiseInfo {
                    push_id: 5,
                    ref_stream_id: request_stream_id,
                },
                PushPromiseInfo {
                    push_id: 3,
                    ref_stream_id: request_stream_id,
                },
            ],
            &[3],
            request_stream_id,
        );
        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test receiving pushes out of order.
    // Push_id 5 is received first and read so that it is removed from the list,
    // therefore Push_id 3 will be in the PushState:Init state.
    // Start push_id 3 by receiving a push stream with the push_id 3 and then a push_promise.
    #[test]
    fn push_out_of_order_3() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 5);
        send_push_data_and_exchange_packets(&mut client, &mut server, 5, true);
        assert_eq!(client.state(), Http3State::Connected);

        // Read push stream with push_id 5 to make it change to closed state.
        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 5,
                ref_stream_id: request_stream_id,
            }],
            &[5],
            request_stream_id,
        );

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 3);
        send_push_data_and_exchange_packets(&mut client, &mut server, 3, true);

        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 3,
                ref_stream_id: request_stream_id,
            }],
            &[3],
            request_stream_id,
        );
        assert_eq!(client.state(), Http3State::Connected);
    }

    // The next test is for receiving a second PushPromise when Push is in the PushPromise state.
    #[test]
    fn multiple_push_promise() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 5);

        // make a second request.
        let request_stream_id_2 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_2, 4);

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id_2, 5);

        read_response_and_push_events(
            &mut client,
            &[
                PushPromiseInfo {
                    push_id: 5,
                    ref_stream_id: request_stream_id,
                },
                PushPromiseInfo {
                    push_id: 5,
                    ref_stream_id: request_stream_id_2,
                },
            ],
            &[],
            request_stream_id,
        );
        assert_eq!(client.state(), Http3State::Connected);
    }

    // The next test is for receiving a second PushPromise when Push is in the Active state.
    #[test]
    fn multiple_push_promise_active() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 5);
        send_push_data_and_exchange_packets(&mut client, &mut server, 5, true);

        // make a second request.
        let request_stream_id_2 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_2, 4);

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id_2, 5);

        read_response_and_push_events(
            &mut client,
            &[
                PushPromiseInfo {
                    push_id: 5,
                    ref_stream_id: request_stream_id,
                },
                PushPromiseInfo {
                    push_id: 5,
                    ref_stream_id: request_stream_id_2,
                },
            ],
            &[5],
            request_stream_id,
        );
        assert_eq!(client.state(), Http3State::Connected);
    }

    // The next test is for receiving a second PushPromise when the push is already closed.
    // PushPromise will be ignored for the push streams that are consumed.
    #[test]
    fn multiple_push_promise_closed() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 5);
        // Start a push stream with push_id 5.
        send_push_data_and_exchange_packets(&mut client, &mut server, 5, true);

        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 5,
                ref_stream_id: request_stream_id,
            }],
            &[5],
            request_stream_id,
        );

        // make a second request.
        let request_stream_id_2 = make_request(&mut client, false, &[]);
        assert_eq!(request_stream_id_2, 4);

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id_2, 5);

        // Check that we do not have a Http3ClientEvent::PushPromise.
        let push_event = |e| matches!(e, Http3ClientEvent::PushPromise { .. });
        assert!(!client.events().any(push_event));
    }

    //  Test that max_push_id is enforced when a push promise frame is received.
    #[test]
    fn exceed_max_push_id_promise() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send a push promise. max_push_id is set to 5, to trigger an error we send push_id=6.
        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 6);

        assert_closed(&client, &Error::HttpId);
    }

    // Test that max_push_id is enforced when a push stream is received.
    #[test]
    fn exceed_max_push_id_push_stream() {
        // Connect and send a request
        let (mut client, mut server) = connect();

        // Send a push stream. max_push_id is set to 5, to trigger an error we send push_id=6.
        send_push_data_and_exchange_packets(&mut client, &mut server, 6, true);

        assert_closed(&client, &Error::HttpId);
    }

    // Test that max_push_id is enforced when a cancel push frame is received.
    #[test]
    fn exceed_max_push_id_cancel_push() {
        // Connect and send a request
        let (mut client, mut server, _request_stream_id) = connect_and_send_request(true);

        // Send CANCEL_PUSH for push_id 6.
        send_cancel_push_and_exchange_packets(&mut client, &mut server, 6);

        assert_closed(&client, &Error::HttpId);
    }

    // Test that max_push_id is enforced when an app calls cancel_push.
    #[test]
    fn exceed_max_push_id_cancel_api() {
        // Connect and send a request
        let (mut client, _, _) = connect_and_send_request(true);

        assert_eq!(client.cancel_push(6), Err(Error::HttpId));
        assert_eq!(client.state(), Http3State::Connected);
    }

    #[test]
    fn test_max_push_id_frame_update_is_sent() {
        const MAX_PUSH_ID_FRAME: &[u8] = &[0xd, 0x1, 0x8];

        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send 3 push promises.
        send_push_promise(&mut server.conn, request_stream_id, 0);
        send_push_promise(&mut server.conn, request_stream_id, 1);
        send_push_promise(&mut server.conn, request_stream_id, 2);

        // create 3 push streams.
        send_push_data(&mut server.conn, 0, true);
        send_push_data(&mut server.conn, 1, true);
        send_push_data_and_exchange_packets(&mut client, &mut server, 2, true);

        read_response_and_push_events(
            &mut client,
            &[
                PushPromiseInfo {
                    push_id: 0,
                    ref_stream_id: request_stream_id,
                },
                PushPromiseInfo {
                    push_id: 1,
                    ref_stream_id: request_stream_id,
                },
                PushPromiseInfo {
                    push_id: 2,
                    ref_stream_id: request_stream_id,
                },
            ],
            &[0, 1, 2],
            request_stream_id,
        );

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));

        // Check max_push_id frame has been received
        let control_stream_readable =
            |e| matches!(e, ConnectionEvent::RecvStreamReadable{stream_id: x} if x == 2);
        assert!(server.conn.events().any(control_stream_readable));
        let mut buf = [0_u8; 100];
        let (amount, fin) = server.conn.stream_recv(StreamId::new(2), &mut buf).unwrap();
        assert!(!fin);

        assert_eq!(amount, MAX_PUSH_ID_FRAME.len());
        assert_eq!(&buf[..3], MAX_PUSH_ID_FRAME);

        // Check that we can send push_id=8 now
        send_push_promise(&mut server.conn, request_stream_id, 8);
        send_push_data(&mut server.conn, 8, true);

        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));

        assert_eq!(client.state(), Http3State::Connected);

        read_response_and_push_events(
            &mut client,
            &[PushPromiseInfo {
                push_id: 8,
                ref_stream_id: request_stream_id,
            }],
            &[8],
            request_stream_id,
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test that 2 push streams with the same push_id are caught.
    #[test]
    fn duplicate_push_stream() {
        // Connect and send a request
        let (mut client, mut server, _request_stream_id) = connect_and_send_request(true);

        // Start a push stream with push_id 0.
        send_push_data_and_exchange_packets(&mut client, &mut server, 0, true);

        // Send it again
        send_push_data_and_exchange_packets(&mut client, &mut server, 0, true);

        assert_closed(&client, &Error::HttpId);
    }

    // Test that 2 push streams with the same push_id are caught.
    #[test]
    fn duplicate_push_stream_active() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise(&mut server.conn, request_stream_id, 0);
        send_push_data_and_exchange_packets(&mut client, &mut server, 0, true);
        // Now the push_stream is in the PushState::Active state

        send_push_data_and_exchange_packets(&mut client, &mut server, 0, true);

        assert_closed(&client, &Error::HttpId);
    }

    fn assert_stop_sending_event(
        server: &mut TestServer,
        push_stream_id: StreamId,
        expected_error: u64,
    ) {
        assert!(server.conn.events().any(|e| matches!(
            e,
            ConnectionEvent::SendStreamStopSending {
                stream_id,
                app_error,
            } if stream_id == push_stream_id && app_error == expected_error
        )));
    }

    // Test CANCEL_PUSH frame: after cancel push any new PUSH_PROMISE or push stream will be ignored.
    #[test]
    fn cancel_push_ignore_promise() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_cancel_push_and_exchange_packets(&mut client, &mut server, 0);

        send_push_promise(&mut server.conn, request_stream_id, 0);
        // Start a push stream with push_id 0.
        let push_stream_id =
            send_push_data_and_exchange_packets(&mut client, &mut server, 0, false);

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        // Check that the push has been canceled by the client.
        assert_stop_sending_event(
            &mut server,
            push_stream_id,
            Error::HttpRequestCancelled.code(),
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test CANCEL_PUSH frame: after cancel push any already received PUSH_PROMISE or push stream
    // events will be removed.
    #[test]
    fn cancel_push_removes_push_events() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise(&mut server.conn, request_stream_id, 0);
        let push_stream_id =
            send_push_data_and_exchange_packets(&mut client, &mut server, 0, false);

        send_cancel_push_and_exchange_packets(&mut client, &mut server, 0);

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        // Check that the push has been canceled by the client.
        assert_stop_sending_event(
            &mut server,
            push_stream_id,
            Error::HttpRequestCancelled.code(),
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test CANCEL_PUSH frame: after cancel push any already received push stream will be canceled.
    #[test]
    fn cancel_push_frame_after_push_stream() {
        // Connect and send a request
        let (mut client, mut server, _) = connect_and_send_request(true);

        // Start a push stream with push_id 0.
        let push_stream_id =
            send_push_data_and_exchange_packets(&mut client, &mut server, 0, false);

        send_cancel_push_and_exchange_packets(&mut client, &mut server, 0);

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        // Check that the push has been canceled by the client.
        assert_stop_sending_event(
            &mut server,
            push_stream_id,
            Error::HttpRequestCancelled.code(),
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test a push stream reset after a new PUSH_PROMISE or/and push stream. The events will be ignored.
    #[test]
    fn cancel_push_stream_after_push_promise_and_push_stream() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise(&mut server.conn, request_stream_id, 0);
        // Start a push stream with push_id 0.
        let push_stream_id =
            send_push_data_and_exchange_packets(&mut client, &mut server, 0, false);

        server
            .conn
            .stream_reset_send(push_stream_id, Error::HttpRequestCancelled.code())
            .unwrap();
        let out = server.conn.process(None, now()).dgram();
        client.process(out, now());

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test that a PUSH_PROMISE will be ignored after a push stream reset.
    #[test]
    fn cancel_push_stream_before_push_promise() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Start a push stream with push_id 0.
        let push_stream_id =
            send_push_data_and_exchange_packets(&mut client, &mut server, 0, false);

        server
            .conn
            .stream_reset_send(push_stream_id, Error::HttpRequestCancelled.code())
            .unwrap();
        let out = server.conn.process(None, now()).dgram();
        client.process(out, now());

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 0);

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test that push_promise events will be removed after application calls cancel_push.
    #[test]
    fn app_cancel_push_after_push_promise() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 0);

        assert!(client.cancel_push(0).is_ok());

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test that push_promise and push data events will be removed after application calls cancel_push.
    #[test]
    fn app_cancel_push_after_push_promise_and_push_stream() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 0);
        let push_stream_id =
            send_push_data_and_exchange_packets(&mut client, &mut server, 0, false);

        assert!(client.cancel_push(0).is_ok());
        let out = client.process(None, now()).dgram();
        mem::drop(server.conn.process(out, now()));

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        // Check that the push has been canceled by the client.
        assert_stop_sending_event(
            &mut server,
            push_stream_id,
            Error::HttpRequestCancelled.code(),
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    // Test that push_promise events will be ignored after application calls cancel_push.
    #[test]
    fn app_cancel_push_before_push_promise() {
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 0);
        let push_stream_id =
            send_push_data_and_exchange_packets(&mut client, &mut server, 0, false);

        assert!(client.cancel_push(0).is_ok());
        let out = client.process(None, now()).dgram();
        mem::drop(server.conn.process(out, now()));

        send_push_promise_and_exchange_packets(&mut client, &mut server, request_stream_id, 0);

        // Assert that we do not have any push event.
        assert!(!check_push_events(&mut client));

        // Check that the push has been closed, e.g. calling cancel_push should return InvalidStreamId.
        assert_eq!(client.cancel_push(0), Err(Error::InvalidStreamId));

        // Check that the push has been canceled by the client.
        assert_stop_sending_event(
            &mut server,
            push_stream_id,
            Error::HttpRequestCancelled.code(),
        );

        assert_eq!(client.state(), Http3State::Connected);
    }

    fn setup_server_side_encoder_param(
        client: &mut Http3Client,
        server: &mut TestServer,
        max_blocked_streams: u64,
    ) {
        server
            .encoder
            .borrow_mut()
            .set_max_capacity(max_blocked_streams)
            .unwrap();
        server
            .encoder
            .borrow_mut()
            .set_max_blocked_streams(100)
            .unwrap();
        server
            .encoder
            .borrow_mut()
            .send_encoder_updates(&mut server.conn)
            .unwrap();
        let out = server.conn.process(None, now());
        mem::drop(client.process(out.dgram(), now()));
    }

    fn setup_server_side_encoder(client: &mut Http3Client, server: &mut TestServer) {
        setup_server_side_encoder_param(client, server, 100);
    }

    fn send_push_promise_using_encoder(
        client: &mut Http3Client,
        server: &mut TestServer,
        stream_id: StreamId,
        push_id: u64,
    ) -> Option<Datagram> {
        send_push_promise_using_encoder_with_custom_headers(
            client,
            server,
            stream_id,
            push_id,
            Header::new("my-header", "my-value"),
        )
    }

    fn send_push_promise_using_encoder_with_custom_headers(
        client: &mut Http3Client,
        server: &mut TestServer,
        stream_id: StreamId,
        push_id: u64,
        additional_header: Header,
    ) -> Option<Datagram> {
        let mut headers = vec![
            Header::new(":method", "GET"),
            Header::new(":scheme", "https"),
            Header::new(":authority", "something.com"),
            Header::new(":path", "/"),
            Header::new("content-length", "3"),
        ];
        headers.push(additional_header);

        let encoded_headers =
            server
                .encoder
                .borrow_mut()
                .encode_header_block(&mut server.conn, &headers, stream_id);
        let push_promise_frame = HFrame::PushPromise {
            push_id,
            header_block: encoded_headers.to_vec(),
        };

        // Send the encoder instructions, but delay them so that the stream is blocked on decoding headers.
        let encoder_inst_pkt = server.conn.process(None, now()).dgram();
        assert!(encoder_inst_pkt.is_some());

        let mut d = Encoder::default();
        push_promise_frame.encode(&mut d);
        server_send_response_and_exchange_packet(client, server, stream_id, &d, false);

        encoder_inst_pkt
    }

    #[test]
    fn push_promise_header_decoder_block() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let encoder_inst_pkt =
            send_push_promise_using_encoder(&mut client, &mut server, request_stream_id, 0);

        // PushPromise is blocked wathing for encoder instructions.
        assert!(!check_push_events(&mut client));

        // Let client receive the encoder instructions.
        let _out = client.process(encoder_inst_pkt, now());

        // PushPromise is blocked wathing for encoder instructions.
        assert!(check_push_events(&mut client));
    }

    // If PushPromise is blocked, stream data can still be received.
    #[test]
    fn push_promise_blocked_but_stream_is_not_blocked() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        // Send response headers
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_HEADER_ONLY_1,
            false,
        );

        let encoder_inst_pkt =
            send_push_promise_using_encoder(&mut client, &mut server, request_stream_id, 0);

        // PushPromise is blocked wathing for encoder instructions.
        assert!(!check_push_events(&mut client));

        // Stream data can be still read
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_DATA_FRAME_1_ONLY_1,
            false,
        );

        assert!(check_data_readable(&mut client));

        // Let client receive the encoder instructions.
        let _out = client.process(encoder_inst_pkt, now());

        // PushPromise is blocked wathing for encoder instructions.
        assert!(check_push_events(&mut client));

        // Stream data can be still read
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_DATA_FRAME_2_ONLY_1,
            false,
        );

        assert!(check_data_readable(&mut client));
    }

    // The response Headers are not block if they do not refer the dynamic table.
    #[test]
    fn push_promise_does_not_block_headers() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let encoder_inst_pkt =
            send_push_promise_using_encoder(&mut client, &mut server, request_stream_id, 0);

        // PushPromise is blocked wathing for encoder instructions.
        assert!(!check_push_events(&mut client));

        // Send response headers
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_HEADER_ONLY_1,
            false,
        );

        assert!(check_header_ready(&mut client));

        // Let client receive the encoder instructions.
        let _out = client.process(encoder_inst_pkt, now());

        // PushPromise is blocked wathing for encoder instructions.
        assert!(check_push_events(&mut client));
    }

    // The response Headers are blocked if they refer a dynamic table entry.
    #[test]
    fn push_promise_block_headers() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        // Insert an elemet into a dynamic table.
        // insert "content-length: 1234
        server
            .encoder
            .borrow_mut()
            .send_and_insert(&mut server.conn, b"content-length", b"1234")
            .unwrap();
        let encoder_inst_pkt1 = server.conn.process(None, now()).dgram();
        let _out = client.process(encoder_inst_pkt1, now());

        // Send a PushPromise that is blocked until encoder_inst_pkt2 is process by the client.
        let encoder_inst_pkt2 =
            send_push_promise_using_encoder(&mut client, &mut server, request_stream_id, 0);

        // PushPromise is blocked wathing for encoder instructions.
        assert!(!check_push_events(&mut client));

        let response_headers = vec![
            Header::new(":status", "200"),
            Header::new("content-length", "1234"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &response_headers,
            request_stream_id,
        );
        let header_hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };
        let mut d = Encoder::default();
        header_hframe.encode(&mut d);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // The response headers are blocked.
        assert!(!check_header_ready(&mut client));

        // Let client receive the encoder instructions.
        let _out = client.process(encoder_inst_pkt2, now());

        // The response headers are blocked.
        assert!(check_header_ready_and_push_promise(&mut client));
    }

    // In this test there are 2 push promises that are blocked and the response header is
    // blocked as well. After a packet is received only the first push promises is unblocked.
    #[test]
    fn two_push_promises_and_header_block() {
        let mut client = default_http3_client_param(200);
        let mut server = TestServer::new_with_settings(&[
            HSetting::new(HSettingType::MaxTableCapacity, 200),
            HSetting::new(HSettingType::BlockedStreams, 100),
            HSetting::new(HSettingType::MaxHeaderListSize, 10000),
        ]);
        connect_only_transport_with(&mut client, &mut server);
        server.create_control_stream();
        server.create_qpack_streams();
        setup_server_side_encoder_param(&mut client, &mut server, 200);

        let request_stream_id = make_request_and_exchange_pkts(&mut client, &mut server, true);

        // Send a PushPromise that is blocked until encoder_inst_pkt2 is process by the client.
        let encoder_inst_pkt1 = send_push_promise_using_encoder_with_custom_headers(
            &mut client,
            &mut server,
            request_stream_id,
            0,
            Header::new("myn1", "myv1"),
        );

        // PushPromise is blocked wathing for encoder instructions.
        assert!(!check_push_events(&mut client));

        let encoder_inst_pkt2 = send_push_promise_using_encoder_with_custom_headers(
            &mut client,
            &mut server,
            request_stream_id,
            1,
            Header::new("myn2", "myv2"),
        );

        // PushPromise is blocked wathing for encoder instructions.
        assert!(!check_push_events(&mut client));

        let response_headers = vec![
            Header::new(":status", "200"),
            Header::new("content-length", "1234"),
            Header::new("myn3", "myv3"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &response_headers,
            request_stream_id,
        );
        let header_hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };
        let mut d = Encoder::default();
        header_hframe.encode(&mut d);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // The response headers are blocked.
        assert!(!check_header_ready(&mut client));

        // Let client receive the encoder instructions.
        let _out = client.process(encoder_inst_pkt1, now());

        assert!(check_push_events(&mut client));

        // Let client receive the encoder instructions.
        let _out = client.process(encoder_inst_pkt2, now());

        assert!(check_header_ready_and_push_promise(&mut client));
    }

    // The PushPromise blocked on header decoding will be canceled if the stream is closed.
    #[test]
    fn blocked_push_promises_canceled() {
        const STREAM_CANCELED_ID_0: &[u8] = &[0x40];

        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        mem::drop(
            send_push_promise_using_encoder(&mut client, &mut server, request_stream_id, 0)
                .unwrap(),
        );

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_1,
            true,
        );

        // Read response that will make stream change to closed state.
        assert!(check_header_ready(&mut client));
        let mut buf = [0_u8; 100];
        let _ = client
            .read_data(now(), request_stream_id, &mut buf)
            .unwrap();

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));
        // Check that encoder got stream_canceled instruction.
        let mut inst = [0_u8; 100];
        let (amount, fin) = server
            .conn
            .stream_recv(CLIENT_SIDE_DECODER_STREAM_ID, &mut inst)
            .unwrap();
        assert!(!fin);
        assert_eq!(amount, STREAM_CANCELED_ID_0.len());
        assert_eq!(&inst[..amount], STREAM_CANCELED_ID_0);
    }

    #[test]
    fn data_readable_in_decoder_blocked_state() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let headers = vec![
            Header::new(":status", "200"),
            Header::new("my-header", "my-header"),
            Header::new("content-length", "0"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        // Delay encoder instruction so that the stream will be blocked.
        let encoder_insts = server.conn.process(None, now());

        // Send response headers.
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // Headers are blocked waiting fro the encoder instructions.
        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!client.events().any(header_ready_event));

        // Now send data frame. This will trigger DataRead event.
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        let d_frame = HFrame::Data { len: 0 };
        d_frame.encode(&mut d);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            true,
        );

        // Now read headers.
        mem::drop(client.process(encoder_insts.dgram(), now()));
    }

    #[test]
    fn qpack_stream_reset() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        setup_server_side_encoder(&mut client, &mut server);
        // Cancel request.
        mem::drop(client.cancel_fetch(request_stream_id, Error::HttpRequestCancelled.code()));
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 0);
        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));
        mem::drop(server.encoder_receiver.receive(&mut server.conn));
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 1);
    }

    fn send_headers_using_encoder(
        client: &mut Http3Client,
        server: &mut TestServer,
        request_stream_id: StreamId,
        headers: &[Header],
        data: &[u8],
    ) -> Option<Datagram> {
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        let out = server.conn.process(None, now());

        // Send response
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        let d_frame = HFrame::Data {
            len: u64::try_from(data.len()).unwrap(),
        };
        d_frame.encode(&mut d);
        d.encode(data);
        server_send_response_and_exchange_packet(client, server, request_stream_id, &d, true);

        out.dgram()
    }

    #[test]
    fn qpack_stream_reset_recv() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        setup_server_side_encoder(&mut client, &mut server);

        // Cancel request.
        server
            .conn
            .stream_reset_send(request_stream_id, Error::HttpRequestCancelled.code())
            .unwrap();
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 0);
        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        mem::drop(server.conn.process(out.dgram(), now()));
        mem::drop(server.encoder_receiver.receive(&mut server.conn));
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 1);
    }

    #[test]
    fn qpack_stream_reset_during_header_qpack_blocked() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        mem::drop(
            send_headers_using_encoder(
                &mut client,
                &mut server,
                request_stream_id,
                &[
                    Header::new(":status", "200"),
                    Header::new("my-header", "my-header"),
                    Header::new("content-length", "3"),
                ],
                &[0x61, 0x62, 0x63],
            )
            .unwrap(),
        );

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!client.events().any(header_ready_event));

        // Cancel request.
        client
            .cancel_fetch(request_stream_id, Error::HttpRequestCancelled.code())
            .unwrap();

        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 0);
        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));
        mem::drop(server.encoder_receiver.receive(&mut server.conn).unwrap());
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 1);
    }

    #[test]
    fn qpack_no_stream_cancelled_after_fin() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let encoder_instruct = send_headers_using_encoder(
            &mut client,
            &mut server,
            request_stream_id,
            &[
                Header::new(":status", "200"),
                Header::new("my-header", "my-header"),
                Header::new("content-length", "3"),
            ],
            &[],
        );

        // Exchange encoder instructions
        mem::drop(client.process(encoder_instruct, now()));

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(client.events().any(header_ready_event));
        // After this the recv_stream is in ClosePending state

        // Cancel request.
        client
            .cancel_fetch(request_stream_id, Error::HttpRequestCancelled.code())
            .unwrap();

        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 0);
        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));
        mem::drop(server.encoder_receiver.receive(&mut server.conn).unwrap());
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 0);
    }

    #[test]
    fn qpack_stream_reset_push_promise_header_decoder_block() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let headers = vec![
            Header::new(":status", "200"),
            Header::new("content-length", "3"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        // Send the encoder instructions.
        let out = server.conn.process(None, now());
        mem::drop(client.process(out.dgram(), now()));

        // Send PushPromise that will be blocked waiting for decoder instructions.
        mem::drop(
            send_push_promise_using_encoder(&mut client, &mut server, request_stream_id, 0)
                .unwrap(),
        );

        // Send response
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        let d_frame = HFrame::Data { len: 0 };
        d_frame.encode(&mut d);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            true,
        );

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(client.events().any(header_ready_event));

        // Cancel request.
        client
            .cancel_fetch(request_stream_id, Error::HttpRequestCancelled.code())
            .unwrap();

        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));
        mem::drop(server.encoder_receiver.receive(&mut server.conn).unwrap());
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 1);
    }

    #[test]
    fn qpack_stream_reset_dynamic_table_zero() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        // Cancel request.
        client
            .cancel_fetch(request_stream_id, Error::HttpRequestCancelled.code())
            .unwrap();
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 0);
        let out = client.process(None, now());
        mem::drop(server.conn.process(out.dgram(), now()));
        mem::drop(server.encoder_receiver.receive(&mut server.conn).unwrap());
        assert_eq!(server.encoder.borrow_mut().stats().stream_cancelled_recv, 0);
    }

    #[test]
    fn multiple_streams_in_decoder_blocked_state() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let headers = vec![
            Header::new(":status", "200"),
            Header::new("my-header", "my-header"),
            Header::new("content-length", "0"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        // Delay encoder instruction so that the stream will be blocked.
        let encoder_insts = server.conn.process(None, now());

        // Send response headers.
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            true,
        );

        // Headers are blocked waiting for the encoder instructions.
        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!client.events().any(header_ready_event));

        // Make another request.
        let request2 = make_request_and_exchange_pkts(&mut client, &mut server, true);
        // Send response headers.
        server_send_response_and_exchange_packet(&mut client, &mut server, request2, &d, true);

        // Headers on the second request are blocked as well are blocked
        // waiting for the encoder instructions.
        assert!(!client.events().any(header_ready_event));

        // Now make the encoder instructions available.
        mem::drop(client.process(encoder_insts.dgram(), now()));

        // Header blocks for both streams should be ready.
        let mut count_responses = 0;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::HeaderReady { stream_id, .. } = e {
                assert!((stream_id == request_stream_id) || (stream_id == request2));
                count_responses += 1;
            }
        }
        assert_eq!(count_responses, 2);
    }

    #[test]
    fn reserved_frames() {
        for f in H3_RESERVED_FRAME_TYPES {
            let mut enc = Encoder::default();
            enc.encode_varint(*f);
            test_wrong_frame_on_control_stream(&enc);
            test_wrong_frame_on_push_stream(&enc);
            test_wrong_frame_on_request_stream(&enc);
        }
    }

    #[test]
    fn send_reserved_settings() {
        for s in H3_RESERVED_SETTINGS {
            let (mut client, mut server) = connect_only_transport();
            let control_stream = server.conn.stream_create(StreamType::UniDi).unwrap();
            // Send the control stream type(0x0).
            let _ = server
                .conn
                .stream_send(control_stream, CONTROL_STREAM_TYPE)
                .unwrap();
            // Create a settings frame of length 2.
            let mut enc = Encoder::default();
            enc.encode_varint(H3_FRAME_TYPE_SETTINGS);
            enc.encode_varint(2_u64);
            // The settings frame contains a reserved settings type and some value (0x1).
            enc.encode_varint(*s);
            enc.encode_varint(1_u64);
            let sent = server.conn.stream_send(control_stream, &enc);
            assert_eq!(sent, Ok(4));
            let out = server.conn.process(None, now());
            client.process(out.dgram(), now());
            assert_closed(&client, &Error::HttpSettings);
        }
    }

    #[test]
    fn response_w_1xx() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let mut d = Encoder::default();
        let headers1xx: &[Header] = &[Header::new(":status", "103")];
        server.encode_headers(request_stream_id, headers1xx, &mut d);

        let headers200: &[Header] = &[
            Header::new(":status", "200"),
            Header::new("my-header", "my-header"),
            Header::new("content-length", "3"),
        ];
        server.encode_headers(request_stream_id, headers200, &mut d);

        // Send 1xx and 200 headers response.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // Sending response data.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_DATA_FRAME_ONLY_2,
            true,
        );

        let mut events = client.events().filter_map(|e| {
            if let Http3ClientEvent::HeaderReady {
                stream_id,
                interim,
                headers,
                ..
            } = e
            {
                Some((stream_id, interim, headers))
            } else {
                None
            }
        });
        let (stream_id_1xx_rec, interim1xx_rec, headers1xx_rec) = events.next().unwrap();
        assert_eq!(
            (stream_id_1xx_rec, interim1xx_rec, headers1xx_rec.as_ref()),
            (request_stream_id, true, headers1xx)
        );

        let (stream_id_200_rec, interim200_rec, headers200_rec) = events.next().unwrap();
        assert_eq!(
            (stream_id_200_rec, interim200_rec, headers200_rec.as_ref()),
            (request_stream_id, false, headers200)
        );
        assert!(events.next().is_none());
    }

    #[test]
    fn response_wo_status() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let mut d = Encoder::default();
        let headers = vec![
            Header::new("my-header", "my-header"),
            Header::new("content-length", "3"),
        ];
        server.encode_headers(request_stream_id, &headers, &mut d);

        // Send response
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // Stream has been reset because of the malformed headers.
        let e = client.events().next().unwrap();
        assert_eq!(
            e,
            Http3ClientEvent::Reset {
                stream_id: request_stream_id,
                error: Error::InvalidHeader.code(),
                local: true,
            }
        );

        let out = client.process(None, now()).dgram();
        mem::drop(server.conn.process(out, now()));

        // Check that server has received a reset.
        let stop_sending_event = |e| {
            matches!(e, ConnectionEvent::SendStreamStopSending {
            stream_id,
            app_error
        } if stream_id == request_stream_id && app_error == Error::InvalidHeader.code())
        };
        assert!(server.conn.events().any(stop_sending_event));

        // Stream should now be closed and gone
        let mut buf = [0_u8; 100];
        assert_eq!(
            client.read_data(now(), StreamId::new(0), &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Client: receive a push stream
    #[test]
    fn push_single_with_1xx() {
        const FIRST_PUSH_ID: u64 = 0;
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send a push promise.
        send_push_promise(&mut server.conn, request_stream_id, FIRST_PUSH_ID);
        // Create a push stream
        let push_stream_id = server.conn.stream_create(StreamType::UniDi).unwrap();

        let mut d = Encoder::default();
        let headers1xx: &[Header] = &[Header::new(":status", "100")];
        server.encode_headers(push_stream_id, headers1xx, &mut d);

        let headers200: &[Header] = &[
            Header::new(":status", "200"),
            Header::new("my-header", "my-header"),
            Header::new("content-length", "3"),
        ];
        server.encode_headers(push_stream_id, headers200, &mut d);

        // create a push stream.
        send_data_on_push(
            &mut server.conn,
            push_stream_id,
            u8::try_from(FIRST_PUSH_ID).unwrap(),
            &d,
            true,
        );

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );

        let mut events = client.events().filter_map(|e| {
            if let Http3ClientEvent::PushHeaderReady {
                push_id,
                interim,
                headers,
                ..
            } = e
            {
                Some((push_id, interim, headers))
            } else {
                None
            }
        });

        let (push_id_1xx_rec, interim1xx_rec, headers1xx_rec) = events.next().unwrap();
        assert_eq!(
            (push_id_1xx_rec, interim1xx_rec, headers1xx_rec.as_ref()),
            (FIRST_PUSH_ID, true, headers1xx)
        );

        let (push_id_200_rec, interim200_rec, headers200_rec) = events.next().unwrap();
        assert_eq!(
            (push_id_200_rec, interim200_rec, headers200_rec.as_ref()),
            (FIRST_PUSH_ID, false, headers200)
        );
        assert!(events.next().is_none());
    }

    // Client: receive a push stream
    #[test]
    fn push_single_wo_status() {
        const FIRST_PUSH_ID: u64 = 0;
        // Connect and send a request
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send a push promise.
        send_push_promise(&mut server.conn, request_stream_id, FIRST_PUSH_ID);
        // Create a push stream
        let push_stream_id = server.conn.stream_create(StreamType::UniDi).unwrap();

        let mut d = Encoder::default();
        let headers = vec![
            Header::new("my-header", "my-header"),
            Header::new("content-length", "3"),
        ];
        server.encode_headers(request_stream_id, &headers, &mut d);

        send_data_on_push(
            &mut server.conn,
            push_stream_id,
            u8::try_from(FIRST_PUSH_ID).unwrap(),
            &d,
            false,
        );

        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            HTTP_RESPONSE_2,
            true,
        );

        // Stream has been reset because of thei malformed headers.
        let push_reset_event = |e| {
            matches!(e, Http3ClientEvent::PushReset {
            push_id,
            error,
        } if push_id == FIRST_PUSH_ID && error == Error::InvalidHeader.code())
        };

        assert!(client.events().any(push_reset_event));

        let out = client.process(None, now()).dgram();
        mem::drop(server.conn.process(out, now()));

        // Check that server has received a reset.
        let stop_sending_event = |e| {
            matches!(e, ConnectionEvent::SendStreamStopSending {
            stream_id,
            app_error
        } if stream_id == push_stream_id && app_error == Error::InvalidHeader.code())
        };
        assert!(server.conn.events().any(stop_sending_event));
    }

    fn handshake_client_error(client: &mut Http3Client, server: &mut TestServer, error: &Error) {
        let out = handshake_only(client, server);
        client.process(out.dgram(), now());
        assert_closed(client, error);
    }

    /// Client fails to create a control stream, since server does not allow it.
    #[test]
    fn client_control_stream_create_failed() {
        let mut client = default_http3_client();
        let mut server = TestServer::new();
        server.set_max_uni_stream(0);
        handshake_client_error(&mut client, &mut server, &Error::StreamLimitError);
    }

    /// 2 streams isn't enough for control and QPACK streams.
    #[test]
    fn client_qpack_stream_create_failed() {
        let mut client = default_http3_client();
        let mut server = TestServer::new();
        server.set_max_uni_stream(2);
        handshake_client_error(&mut client, &mut server, &Error::StreamLimitError);
    }

    fn do_malformed_response_test(headers: &[Header]) {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let mut d = Encoder::default();
        server.encode_headers(request_stream_id, headers, &mut d);

        // Send response
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // Stream has been reset because of the malformed headers.
        let e = client.events().next().unwrap();
        assert_eq!(
            e,
            Http3ClientEvent::Reset {
                stream_id: request_stream_id,
                error: Error::InvalidHeader.code(),
                local: true,
            }
        );
    }

    #[test]
    fn malformed_response_pseudo_header_after_regular_header() {
        do_malformed_response_test(&[
            Header::new("content-type", "text/plain"),
            Header::new(":status", "100"),
        ]);
    }

    #[test]
    fn malformed_response_undefined_pseudo_header() {
        do_malformed_response_test(&[Header::new(":status", "200"), Header::new(":cheese", "200")]);
    }

    #[test]
    fn malformed_response_duplicate_pseudo_header() {
        do_malformed_response_test(&[
            Header::new(":status", "200"),
            Header::new(":status", "100"),
            Header::new("content-type", "text/plain"),
        ]);
    }

    #[test]
    fn malformed_response_uppercase_header() {
        do_malformed_response_test(&[
            Header::new(":status", "200"),
            Header::new("content-Type", "text/plain"),
        ]);
    }

    #[test]
    fn malformed_response_excluded_header() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let mut d = Encoder::default();
        server.encode_headers(
            request_stream_id,
            &[
                Header::new(":status", "200"),
                Header::new("content-type", "text/plain"),
                Header::new("connection", "close"),
            ],
            &mut d,
        );

        // Send response
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // Stream has been reset because of the malformed headers.
        let e = client.events().next().unwrap();
        assert_eq!(
            e,
            Http3ClientEvent::HeaderReady {
                stream_id: request_stream_id,
                headers: vec![
                    Header::new(":status", "200"),
                    Header::new("content-type", "text/plain")
                ],
                interim: false,
                fin: false,
            }
        );
    }

    #[test]
    fn malformed_response_excluded_byte_in_header() {
        do_malformed_response_test(&[
            Header::new(":status", "200"),
            Header::new("content:type", "text/plain"),
        ]);
    }

    #[test]
    fn malformed_response_request_header_in_response() {
        do_malformed_response_test(&[
            Header::new(":status", "200"),
            Header::new(":method", "GET"),
            Header::new("content-type", "text/plain"),
        ]);
    }

    fn maybe_authenticate(conn: &mut Http3Client) {
        let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
        if conn.events().any(authentication_needed) {
            conn.authenticated(AuthenticationStatus::Ok, now());
        }
    }

    const MAX_TABLE_SIZE: u64 = 65536;
    const MAX_BLOCKED_STREAMS: u16 = 5;

    fn get_resumption_token(server: &mut Http3Server) -> ResumptionToken {
        let mut client = default_http3_client_param(MAX_TABLE_SIZE);

        let mut datagram = None;
        let is_done = |c: &Http3Client| matches!(c.state(), Http3State::Connected);
        while !is_done(&mut client) {
            maybe_authenticate(&mut client);
            datagram = client.process(datagram, now()).dgram();
            datagram = server.process(datagram, now()).dgram();
        }

        // exchange qpack settings, server will send a token as well.
        datagram = client.process(datagram, now()).dgram();
        datagram = server.process(datagram, now()).dgram();
        mem::drop(client.process(datagram, now()).dgram());

        client
            .events()
            .find_map(|e| {
                if let Http3ClientEvent::ResumptionToken(token) = e {
                    Some(token)
                } else {
                    None
                }
            })
            .unwrap()
    }

    // Test that decoder stream type is always sent before any other instruction also
    // in case when 0RTT is used.
    // A client will send a request that uses the dynamic table. This will trigger a header-ack
    // from a server. We will use stats to check that a header-ack has been received.
    #[test]
    fn zerortt_request_use_dynamic_table() {
        let mut server = Http3Server::new(
            now(),
            DEFAULT_KEYS,
            DEFAULT_ALPN_H3,
            anti_replay(),
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            Http3Parameters::default()
                .max_table_size_encoder(MAX_TABLE_SIZE)
                .max_table_size_decoder(MAX_TABLE_SIZE)
                .max_blocked_streams(MAX_BLOCKED_STREAMS),
            None,
        )
        .unwrap();

        let token = get_resumption_token(&mut server);
        // Make a new connection.
        let mut client = default_http3_client_param(MAX_TABLE_SIZE);
        assert_eq!(client.state(), Http3State::Initializing);
        client
            .enable_resumption(now(), &token)
            .expect("Set resumption token.");

        assert_eq!(client.state(), Http3State::ZeroRtt);
        let zerortt_event = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::ZeroRtt));
        assert!(client.events().any(zerortt_event));

        // Make a request that uses the dynamic table.
        let _ = make_request(&mut client, true, &[Header::new("myheaders", "myvalue")]);
        // Assert that the request has used dynamic table. That will trigger a header_ack.
        assert_eq!(client.qpack_encoder_stats().dynamic_table_references, 1);

        // Exchange packets until header-ack is received.
        // These many packet exchange is needed, to get a header-ack.
        // TODO this may be optimize at Http3Server.
        let out = client.process(None, now()).dgram();
        let out = server.process(out, now()).dgram();
        let out = client.process(out, now()).dgram();
        let out = server.process(out, now()).dgram();
        let out = client.process(out, now()).dgram();
        let out = server.process(out, now()).dgram();
        let out = client.process(out, now()).dgram();
        let out = server.process(out, now()).dgram();
        mem::drop(client.process(out, now()));

        // The header ack for the first request has been received.
        assert_eq!(client.qpack_encoder_stats().header_acks_recv, 1);
    }

    fn manipulate_conrol_stream(client: &mut Http3Client, stream_id: StreamId) {
        assert_eq!(
            client
                .cancel_fetch(stream_id, Error::HttpNoError.code())
                .unwrap_err(),
            Error::InvalidStreamId
        );
        assert_eq!(
            client.stream_close_send(stream_id).unwrap_err(),
            Error::InvalidStreamId
        );
        let mut buf = [0; 2];
        assert_eq!(
            client.send_data(stream_id, &buf).unwrap_err(),
            Error::InvalidStreamId
        );
        assert_eq!(
            client.read_data(now(), stream_id, &mut buf).unwrap_err(),
            Error::InvalidStreamId
        );
    }

    #[test]
    fn manipulate_conrol_streams() {
        let (mut client, server, request_stream_id) = connect_and_send_request(false);
        manipulate_conrol_stream(&mut client, CLIENT_SIDE_CONTROL_STREAM_ID);
        manipulate_conrol_stream(&mut client, CLIENT_SIDE_ENCODER_STREAM_ID);
        manipulate_conrol_stream(&mut client, CLIENT_SIDE_DECODER_STREAM_ID);
        manipulate_conrol_stream(&mut client, server.control_stream_id.unwrap());
        manipulate_conrol_stream(&mut client, server.encoder_stream_id.unwrap());
        manipulate_conrol_stream(&mut client, server.decoder_stream_id.unwrap());
        client
            .cancel_fetch(request_stream_id, Error::HttpNoError.code())
            .unwrap();
    }

    // Client: receive a push stream
    #[test]
    fn incomple_push_stream() {
        let (mut client, mut server) = connect();

        // Create a push stream
        let push_stream_id = server.conn.stream_create(StreamType::UniDi).unwrap();
        let _ = server
            .conn
            .stream_send(push_stream_id, PUSH_STREAM_TYPE)
            .unwrap();
        let _ = server.conn.stream_send(push_stream_id, &[0]).unwrap();
        server.conn.stream_close_send(push_stream_id).unwrap();
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, &Error::HttpGeneralProtocol);
    }

    #[test]
    fn priority_update_during_full_buffer() {
        // set a lower MAX_DATA on the server side to restrict the data the client can send
        let (mut client, mut server) =
            connect_with_connection_parameters(ConnectionParameters::default().max_data(1200));

        let request_stream_id = make_request_and_exchange_pkts(&mut client, &mut server, false);
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(client.events().any(data_writable));
        // Send a lot of data to reach the flow control limit
        client.send_data(request_stream_id, &[0; 2000]).unwrap();

        // now queue a priority_update packet for that stream
        assert!(client
            .priority_update(request_stream_id, Priority::new(6, false))
            .unwrap());

        let md_before = server.conn.stats().frame_tx.max_data;

        // sending the http request and most most of the request data
        let out = client.process(None, now()).dgram();
        let out = server.conn.process(out, now()).dgram();

        // the server responses with an ack, but the max_data didn't change
        assert_eq!(md_before, server.conn.stats().frame_tx.max_data);

        let out = client.process(out, now()).dgram();
        let out = server.conn.process(out, now()).dgram();

        // the server increased the max_data during the second read if that isn't the case
        // in the future and therefore this asserts fails, the request data on stream 0 could be read
        // to cause a max_update frame
        assert_eq!(md_before + 1, server.conn.stats().frame_tx.max_data);

        // make sure that the server didn't receive a priority_update on client control stream (stream_id 2) yet
        let mut buf = [0; 32];
        assert_eq!(
            server.conn.stream_recv(StreamId::new(2), &mut buf),
            Ok((0, false))
        );

        // the client now sends the priority update
        let out = client.process(out, now()).dgram();
        server.conn.process_input(out.unwrap(), now());

        // check that the priority_update arrived at the client control stream
        let num_read = server.conn.stream_recv(StreamId::new(2), &mut buf).unwrap();
        assert_eq!(b"\x80\x0f\x07\x00\x04\x00\x75\x3d\x36", &buf[0..num_read.0]);
    }

    #[test]
    fn error_request_stream() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let headers = vec![
            Header::new(":status", "200"),
            Header::new(":method", "GET"), // <- invalid
            Header::new("my-header", "my-header"),
            Header::new("content-length", "3"),
        ];
        let encoded_headers = server.encoder.borrow_mut().encode_header_block(
            &mut server.conn,
            &headers,
            request_stream_id,
        );
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };

        // Send the encoder instructions, but delay them so that the stream is blocked on decoding headers.
        let encoder_inst_pkt = server.conn.process(None, now());

        // Send response
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        let d_frame = HFrame::Data { len: 3 };
        d_frame.encode(&mut d);
        d.encode(b"abc");
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            true,
        );

        // Let client receive the encoder instructions.
        client.process_input(encoder_inst_pkt.dgram().unwrap(), now());

        let reset_event = |e| matches!(e, Http3ClientEvent::Reset { stream_id, .. } if stream_id == request_stream_id);
        assert!(client.events().any(reset_event));
    }

    #[test]
    fn response_w_101() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        setup_server_side_encoder(&mut client, &mut server);

        let mut d = Encoder::default();
        let headers1xx = &[Header::new(":status", "101")];
        server.encode_headers(request_stream_id, headers1xx, &mut d);

        // Send 101 response.
        server_send_response_and_exchange_packet(
            &mut client,
            &mut server,
            request_stream_id,
            &d,
            false,
        );

        // Stream has been reset because of the 101 response.
        let e = client.events().next().unwrap();
        assert_eq!(
            e,
            Http3ClientEvent::Reset {
                stream_id: request_stream_id,
                error: Error::InvalidHeader.code(),
                local: true,
            }
        );
    }
}
