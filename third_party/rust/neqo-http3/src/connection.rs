// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::control_stream_local::ControlStreamLocal;
use crate::control_stream_remote::ControlStreamRemote;
use crate::features::extended_connect::{
    webtransport_session::WebTransportSession,
    webtransport_streams::{WebTransportRecvStream, WebTransportSendStream},
    ExtendedConnectEvents, ExtendedConnectFeature, ExtendedConnectType,
};
use crate::frames::HFrame;
use crate::push_controller::PushController;
use crate::qpack_decoder_receiver::DecoderRecvStream;
use crate::qpack_encoder_receiver::EncoderRecvStream;
use crate::recv_message::{RecvMessage, RecvMessageInfo};
use crate::request_target::{AsRequestTarget, RequestTarget};
use crate::send_message::SendMessage;
use crate::settings::{HSettingType, HSettings, HttpZeroRttChecker};
use crate::stream_type_reader::NewStreamHeadReader;
use crate::{
    client_events::Http3ClientEvents, CloseType, Http3Parameters, Http3StreamType,
    HttpRecvStreamEvents, NewStreamType, Priority, PriorityHandler, ReceiveOutput, RecvStream,
    RecvStreamEvents, SendStream, SendStreamEvents,
};
use neqo_common::{qdebug, qerror, qinfo, qtrace, qwarn, Header, MessageType, Role};
use neqo_qpack::decoder::QPackDecoder;
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::{
    AppError, Connection, ConnectionError, State, StreamId, StreamType, ZeroRttState,
};
use std::cell::RefCell;
use std::collections::{BTreeSet, HashMap};
use std::fmt::Debug;
use std::mem;
use std::rc::Rc;

use crate::{Error, Res};

pub struct RequestDescription<'b, 't, T>
where
    T: AsRequestTarget<'t> + ?Sized + Debug,
{
    pub method: &'b str,
    pub connect_type: Option<ExtendedConnectType>,
    pub target: &'t T,
    pub headers: &'b [Header],
    pub priority: Priority,
}

#[derive(Debug)]
enum Http3RemoteSettingsState {
    NotReceived,
    Received(HSettings),
    ZeroRtt(HSettings),
}

#[derive(Debug, PartialEq, PartialOrd, Ord, Eq, Clone)]
pub enum Http3State {
    Initializing,
    ZeroRtt,
    Connected,
    GoingAway(StreamId),
    Closing(ConnectionError),
    Closed(ConnectionError),
}

impl Http3State {
    #[must_use]
    pub fn active(&self) -> bool {
        matches!(
            self,
            Http3State::Connected | Http3State::GoingAway(_) | Http3State::ZeroRtt
        )
    }
}

#[derive(Debug)]
pub(crate) struct Http3Connection {
    role: Role,
    pub state: Http3State,
    local_params: Http3Parameters,
    control_stream_local: ControlStreamLocal,
    pub qpack_encoder: Rc<RefCell<QPackEncoder>>,
    pub qpack_decoder: Rc<RefCell<QPackDecoder>>,
    settings_state: Http3RemoteSettingsState,
    streams_with_pending_data: BTreeSet<StreamId>,
    pub send_streams: HashMap<StreamId, Box<dyn SendStream>>,
    pub recv_streams: HashMap<StreamId, Box<dyn RecvStream>>,
    webtransport: ExtendedConnectFeature,
}

impl ::std::fmt::Display for Http3Connection {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 connection")
    }
}

impl Http3Connection {
    /// Create a new connection.
    pub fn new(conn_params: Http3Parameters, role: Role) -> Self {
        Self {
            state: Http3State::Initializing,
            control_stream_local: ControlStreamLocal::new(),
            qpack_encoder: Rc::new(RefCell::new(QPackEncoder::new(
                conn_params.get_qpack_settings(),
                true,
            ))),
            qpack_decoder: Rc::new(RefCell::new(QPackDecoder::new(
                conn_params.get_qpack_settings(),
            ))),
            webtransport: ExtendedConnectFeature::new(
                ExtendedConnectType::WebTransport,
                conn_params.get_webtransport(),
            ),
            local_params: conn_params,
            settings_state: Http3RemoteSettingsState::NotReceived,
            streams_with_pending_data: BTreeSet::new(),
            send_streams: HashMap::new(),
            recv_streams: HashMap::new(),
            role,
        }
    }

    pub fn set_features_listener(&mut self, feature_listener: Http3ClientEvents) {
        self.webtransport.set_listener(feature_listener);
    }

    fn initialize_http3_connection(&mut self, conn: &mut Connection) -> Res<()> {
        qinfo!([self], "Initialize the http3 connection.");
        self.control_stream_local.create(conn)?;

        self.send_settings();
        self.create_qpack_streams(conn)?;
        Ok(())
    }

    fn send_settings(&mut self) {
        qdebug!([self], "Send settings.");
        self.control_stream_local.queue_frame(&HFrame::Settings {
            settings: HSettings::from(&self.local_params),
        });
        self.control_stream_local.queue_frame(&HFrame::Grease);
    }

    /// Save settings for adding to the session ticket.
    pub(crate) fn save_settings(&self) -> Vec<u8> {
        HttpZeroRttChecker::save(&self.local_params)
    }

    fn create_qpack_streams(&mut self, conn: &mut Connection) -> Res<()> {
        qdebug!([self], "create_qpack_streams.");
        self.qpack_encoder
            .borrow_mut()
            .add_send_stream(conn.stream_create(StreamType::UniDi)?);
        self.qpack_decoder
            .borrow_mut()
            .add_send_stream(conn.stream_create(StreamType::UniDi)?);
        Ok(())
    }

    /// Inform a `HttpConnection` that a stream has data to send and that `send` should be called for the stream.
    pub fn stream_has_pending_data(&mut self, stream_id: StreamId) {
        self.streams_with_pending_data.insert(stream_id);
    }

    /// Return true if there is a stream that needs to send data.
    pub fn has_data_to_send(&self) -> bool {
        !self.streams_with_pending_data.is_empty()
    }

    fn send_non_control_streams(&mut self, conn: &mut Connection) -> Res<()> {
        let to_send = mem::take(&mut self.streams_with_pending_data);
        for stream_id in to_send {
            let done = if let Some(s) = &mut self.send_streams.get_mut(&stream_id) {
                s.send(conn)?;
                if s.has_data_to_send() {
                    self.streams_with_pending_data.insert(stream_id);
                }
                s.done()
            } else {
                false
            };
            if done {
                self.remove_send_stream(stream_id, conn);
            }
        }
        Ok(())
    }

    /// Call `send` for all streams that need to send data.
    pub fn process_sending(&mut self, conn: &mut Connection) -> Res<()> {
        // check if control stream has data to send.
        self.control_stream_local
            .send(conn, &mut self.recv_streams)?;

        self.send_non_control_streams(conn)?;

        self.qpack_decoder.borrow_mut().send(conn)?;
        match self.qpack_encoder.borrow_mut().send_encoder_updates(conn) {
            Ok(())
            | Err(neqo_qpack::Error::EncoderStreamBlocked)
            | Err(neqo_qpack::Error::DynamicTableFull) => {}
            Err(e) => return Err(Error::QpackError(e)),
        }
        Ok(())
    }

    /// We have a resumption token which remembers previous settings. Update the setting.
    pub fn set_0rtt_settings(&mut self, conn: &mut Connection, settings: HSettings) -> Res<()> {
        self.initialize_http3_connection(conn)?;
        self.set_qpack_settings(&settings)?;
        self.settings_state = Http3RemoteSettingsState::ZeroRtt(settings);
        self.state = Http3State::ZeroRtt;
        Ok(())
    }

    /// Returns the settings for a connection. This is used for creating a resumption token.
    pub fn get_settings(&self) -> Option<HSettings> {
        if let Http3RemoteSettingsState::Received(settings) = &self.settings_state {
            Some(settings.clone())
        } else {
            None
        }
    }

    pub fn add_new_stream(&mut self, stream_id: StreamId) {
        qtrace!([self], "A new stream: {}.", stream_id);
        self.recv_streams.insert(
            stream_id,
            Box::new(NewStreamHeadReader::new(stream_id, self.role)),
        );
    }

    #[allow(clippy::option_if_let_else)] // False positive as borrow scope isn't lexical here.
    fn stream_receive(&mut self, conn: &mut Connection, stream_id: StreamId) -> Res<ReceiveOutput> {
        qtrace!([self], "Readable stream {}.", stream_id);

        if let Some(recv_stream) = self.recv_streams.get_mut(&stream_id) {
            let res = recv_stream.receive(conn);
            return self
                .handle_stream_manipulation_output(res, stream_id, conn)
                .map(|(output, _)| output);
        }
        Ok(ReceiveOutput::NoOutput)
    }

    fn handle_unblocked_streams(
        &mut self,
        unblocked_streams: Vec<StreamId>,
        conn: &mut Connection,
    ) -> Res<()> {
        for stream_id in unblocked_streams {
            qdebug!([self], "Stream {} is unblocked", stream_id);
            if let Some(r) = self.recv_streams.get_mut(&stream_id) {
                let res = r
                    .http_stream()
                    .ok_or(Error::HttpInternal(10))?
                    .header_unblocked(conn);
                let res = self.handle_stream_manipulation_output(res, stream_id, conn)?;
                debug_assert!(matches!(res, (ReceiveOutput::NoOutput, _)));
            }
        }
        Ok(())
    }

    /// This function handles reading from all streams, i.e. control, qpack, request/response
    /// stream and unidi stream that are still do not have a type.
    /// The function cannot handle:
    /// 1) a Push stream (if an unknown unidi stream is decoded to be a push stream)
    /// 2) frames `MaxPushId` or `Goaway` must be handled by `Http3Client`/`Server`.
    /// The function returns `ReceiveOutput`.
    pub fn handle_stream_readable(
        &mut self,
        conn: &mut Connection,
        stream_id: StreamId,
    ) -> Res<ReceiveOutput> {
        let mut output = self.stream_receive(conn, stream_id)?;

        if let ReceiveOutput::NewStream(stream_type) = output {
            output = self.handle_new_stream(conn, stream_type, stream_id)?;
        }

        #[allow(clippy::match_same_arms)] // clippy is being stupid here
        match output {
            ReceiveOutput::UnblockedStreams(unblocked_streams) => {
                self.handle_unblocked_streams(unblocked_streams, conn)?;
                Ok(ReceiveOutput::NoOutput)
            }
            ReceiveOutput::ControlFrames(mut control_frames) => {
                let mut rest = Vec::new();
                for cf in control_frames.drain(..) {
                    if let Some(not_handled) = self.handle_control_frame(cf)? {
                        rest.push(not_handled);
                    }
                }
                Ok(ReceiveOutput::ControlFrames(rest))
            }
            ReceiveOutput::NewStream(NewStreamType::Push(_))
            | ReceiveOutput::NewStream(NewStreamType::Http)
            | ReceiveOutput::NewStream(NewStreamType::WebTransportStream(_)) => Ok(output),
            ReceiveOutput::NewStream(_) => {
                unreachable!("NewStream should have been handled already")
            }
            _ => Ok(output),
        }
    }

    /// This is called when a RESET frame has been received.
    pub fn handle_stream_reset(
        &mut self,
        stream_id: StreamId,
        app_error: AppError,
        conn: &mut Connection,
    ) -> Res<()> {
        qinfo!(
            [self],
            "Handle a stream reset stream_id={} app_err={}",
            stream_id,
            app_error
        );

        self.close_recv(stream_id, CloseType::ResetRemote(app_error), conn)
    }

    pub fn handle_stream_stop_sending(
        &mut self,
        stream_id: StreamId,
        app_error: AppError,
        conn: &mut Connection,
    ) -> Res<()> {
        qinfo!(
            [self],
            "Handle stream_stop_sending stream_id={} app_err={}",
            stream_id,
            app_error
        );

        if self.send_stream_is_critical(stream_id) {
            return Err(Error::HttpClosedCriticalStream);
        }

        self.close_send(stream_id, CloseType::ResetRemote(app_error), conn);
        Ok(())
    }

    /// This is called when `neqo_transport::Connection` state has been change to take proper actions in
    /// the HTTP3 layer.
    pub fn handle_state_change(&mut self, conn: &mut Connection, state: &State) -> Res<bool> {
        qdebug!([self], "Handle state change {:?}", state);
        match state {
            State::Handshaking => {
                if self.role == Role::Server
                    && conn.zero_rtt_state() == &ZeroRttState::AcceptedServer
                {
                    self.state = Http3State::ZeroRtt;
                    self.initialize_http3_connection(conn)?;
                    Ok(true)
                } else {
                    Ok(false)
                }
            }
            State::Connected => {
                debug_assert!(matches!(
                    self.state,
                    Http3State::Initializing | Http3State::ZeroRtt
                ));
                if self.state == Http3State::Initializing {
                    self.initialize_http3_connection(conn)?;
                }
                self.state = Http3State::Connected;
                Ok(true)
            }
            State::Closing { error, .. } | State::Draining { error, .. } => {
                if matches!(self.state, Http3State::Closing(_) | Http3State::Closed(_)) {
                    Ok(false)
                } else {
                    self.state = Http3State::Closing(error.clone());
                    Ok(true)
                }
            }
            State::Closed(error) => {
                if matches!(self.state, Http3State::Closed(_)) {
                    Ok(false)
                } else {
                    self.state = Http3State::Closed(error.clone());
                    Ok(true)
                }
            }
            _ => Ok(false),
        }
    }

    /// This is called when 0RTT has been reseted to clear `send_streams`, `recv_streams` and settings.
    pub fn handle_zero_rtt_rejected(&mut self) -> Res<()> {
        if self.state == Http3State::ZeroRtt {
            self.state = Http3State::Initializing;
            self.control_stream_local = ControlStreamLocal::new();
            self.qpack_encoder = Rc::new(RefCell::new(QPackEncoder::new(
                self.local_params.get_qpack_settings(),
                true,
            )));
            self.qpack_decoder = Rc::new(RefCell::new(QPackDecoder::new(
                self.local_params.get_qpack_settings(),
            )));
            self.settings_state = Http3RemoteSettingsState::NotReceived;
            self.streams_with_pending_data.clear();
            // TODO: investigate whether this code can automatically retry failed transactions.
            self.send_streams.clear();
            self.recv_streams.clear();
            Ok(())
        } else {
            debug_assert!(false, "Zero rtt rejected in the wrong state.");
            Err(Error::HttpInternal(3))
        }
    }

    fn check_stream_exists(&self, stream_type: Http3StreamType) -> Res<()> {
        if self
            .recv_streams
            .values()
            .any(|c| c.stream_type() == stream_type)
        {
            Err(Error::HttpStreamCreation)
        } else {
            Ok(())
        }
    }

    /// If the new stream is a control stream, this function creates a proper handler
    /// and perform a read.
    /// if the new stream is a push stream, the function returns `ReceiveOutput::PushStream`
    /// and the caller will handle it.
    /// If the stream is of a unknown type the stream will be closed.
    fn handle_new_stream(
        &mut self,
        conn: &mut Connection,
        stream_type: NewStreamType,
        stream_id: StreamId,
    ) -> Res<ReceiveOutput> {
        match stream_type {
            NewStreamType::Control => {
                self.check_stream_exists(Http3StreamType::Control)?;
                self.recv_streams
                    .insert(stream_id, Box::new(ControlStreamRemote::new(stream_id)));
            }

            NewStreamType::Push(push_id) => {
                qinfo!(
                    [self],
                    "A new push stream {} push_id:{}.",
                    stream_id,
                    push_id
                );
            }
            NewStreamType::Decoder => {
                qinfo!([self], "A new remote qpack encoder stream {}", stream_id);
                self.check_stream_exists(Http3StreamType::Decoder)?;
                self.recv_streams.insert(
                    stream_id,
                    Box::new(DecoderRecvStream::new(
                        stream_id,
                        Rc::clone(&self.qpack_decoder),
                    )),
                );
            }
            NewStreamType::Encoder => {
                qinfo!([self], "A new remote qpack decoder stream {}", stream_id);
                self.check_stream_exists(Http3StreamType::Encoder)?;
                self.recv_streams.insert(
                    stream_id,
                    Box::new(EncoderRecvStream::new(
                        stream_id,
                        Rc::clone(&self.qpack_encoder),
                    )),
                );
            }
            NewStreamType::Http => {
                qinfo!([self], "A new http stream {}.", stream_id);
            }
            NewStreamType::WebTransportStream(session_id) => {
                let session_exists = self
                    .send_streams
                    .get(&StreamId::from(session_id))
                    .map_or(false, |s| {
                        s.stream_type() == Http3StreamType::ExtendedConnect
                    });
                if !session_exists {
                    conn.stream_stop_sending(stream_id, Error::HttpStreamCreation.code())?;
                    return Ok(ReceiveOutput::NoOutput);
                }
            }
            NewStreamType::Unknown => {
                conn.stream_stop_sending(stream_id, Error::HttpStreamCreation.code())?;
            }
        };

        match stream_type {
            NewStreamType::Control | NewStreamType::Decoder | NewStreamType::Encoder => {
                self.stream_receive(conn, stream_id)
            }
            NewStreamType::Push(_) | NewStreamType::Http | NewStreamType::WebTransportStream(_) => {
                Ok(ReceiveOutput::NewStream(stream_type))
            }
            NewStreamType::Unknown => Ok(ReceiveOutput::NoOutput),
        }
    }

    /// This is called when an application closes the connection.
    pub fn close(&mut self, error: AppError) {
        qinfo!([self], "Close connection error {:?}.", error);
        self.state = Http3State::Closing(ConnectionError::Application(error));
        if (!self.send_streams.is_empty() || !self.recv_streams.is_empty()) && (error == 0) {
            qwarn!("close(0) called when streams still active");
        }
        self.send_streams.clear();
        self.recv_streams.clear();
    }

    /// This function will not handle the output of the function completely, but only
    /// handle the indication that a stream is closed. There are 2 cases:
    ///  - an error occurred or
    ///  - the stream is done, i.e. the second value in `output` tuple is true if
    ///    the stream is done and can be removed from the `recv_streams`
    /// How it is handling `output`:
    ///  - if the stream is done, it removes the stream from `recv_streams`
    ///  - if the stream is not done and there is no error, return `output` and the caller will
    ///    handle it.
    ///  - in case of an error:
    ///    - if it is only a stream error and the stream is not critical, send `STOP_SENDING`
    ///      frame, remove the stream from `recv_streams` and inform the listener that the stream
    ///      has been reset.
    ///    - otherwise this is a connection error. In this case, propagate the error to the caller
    ///      that will handle it properly.
    fn handle_stream_manipulation_output<U>(
        &mut self,
        output: Res<(U, bool)>,
        stream_id: StreamId,
        conn: &mut Connection,
    ) -> Res<(U, bool)>
    where
        U: Default,
    {
        match &output {
            Ok((_, true)) => {
                self.remove_recv_stream(stream_id, conn);
            }
            Ok((_, false)) => {}
            Err(e) => {
                if e.stream_reset_error() && !self.recv_stream_is_critical(stream_id) {
                    mem::drop(conn.stream_stop_sending(stream_id, e.code()));
                    self.close_recv(stream_id, CloseType::LocalError(e.code()), conn)?;
                    return Ok((U::default(), false));
                }
            }
        }
        output
    }

    fn create_fetch_headers<'b, 't, T>(request: &RequestDescription<'b, 't, T>) -> Res<Vec<Header>>
    where
        T: AsRequestTarget<'t> + ?Sized + Debug,
    {
        let target = request
            .target
            .as_request_target()
            .map_err(|_| Error::InvalidRequestTarget)?;

        // Transform pseudo-header fields
        let mut final_headers = vec![
            Header::new(":method", request.method),
            Header::new(":scheme", target.scheme()),
            Header::new(":authority", target.authority()),
            Header::new(":path", target.path()),
        ];
        if let Some(conn_type) = request.connect_type {
            final_headers.push(Header::new(":protocol", conn_type.string()));
        }

        if let Some(priority_header) = request.priority.header() {
            final_headers.push(priority_header);
        }
        final_headers.extend_from_slice(request.headers);
        Ok(final_headers)
    }

    pub fn fetch<'b, 't, T>(
        &mut self,
        conn: &mut Connection,
        send_events: Box<dyn SendStreamEvents>,
        recv_events: Box<dyn HttpRecvStreamEvents>,
        push_handler: Option<Rc<RefCell<PushController>>>,
        request: &RequestDescription<'b, 't, T>,
    ) -> Res<StreamId>
    where
        T: AsRequestTarget<'t> + ?Sized + Debug,
    {
        qinfo!(
            [self],
            "Fetch method={} target: {:?}",
            request.method,
            request.target,
        );
        let id = self.create_bidi_transport_stream(conn)?;
        self.fetch_with_stream(id, conn, send_events, recv_events, push_handler, request)?;
        Ok(id)
    }

    fn create_bidi_transport_stream(&self, conn: &mut Connection) -> Res<StreamId> {
        // Requests cannot be created when a connection is in states: Initializing, GoingAway, Closing and Closed.
        match self.state() {
            Http3State::GoingAway(..) | Http3State::Closing(..) | Http3State::Closed(..) => {
                return Err(Error::AlreadyClosed)
            }
            Http3State::Initializing => return Err(Error::Unavailable),
            _ => {}
        }

        let id = conn
            .stream_create(StreamType::BiDi)
            .map_err(|e| Error::map_stream_create_errors(&e))?;
        conn.stream_keep_alive(id, true)?;
        Ok(id)
    }

    fn fetch_with_stream<'b, 't, T>(
        &mut self,
        stream_id: StreamId,
        conn: &mut Connection,
        send_events: Box<dyn SendStreamEvents>,
        recv_events: Box<dyn HttpRecvStreamEvents>,
        push_handler: Option<Rc<RefCell<PushController>>>,
        request: &RequestDescription<'b, 't, T>,
    ) -> Res<()>
    where
        T: AsRequestTarget<'t> + ?Sized + Debug,
    {
        let final_headers = Http3Connection::create_fetch_headers(request)?;

        let stream_type = if request.connect_type.is_some() {
            Http3StreamType::ExtendedConnect
        } else {
            Http3StreamType::Http
        };

        let mut send_message = SendMessage::new(
            MessageType::Request,
            stream_type,
            stream_id,
            self.qpack_encoder.clone(),
            send_events,
        );

        send_message
            .http_stream()
            .unwrap()
            .send_headers(&final_headers, conn)?;

        self.add_streams(
            stream_id,
            Box::new(send_message),
            Box::new(RecvMessage::new(
                &RecvMessageInfo {
                    message_type: MessageType::Response,
                    stream_type,
                    stream_id,
                    header_frame_type_read: false,
                },
                Rc::clone(&self.qpack_decoder),
                recv_events,
                push_handler,
                PriorityHandler::new(false, request.priority),
            )),
        );

        // Call immediately send so that at least headers get sent. This will make Firefox faster, since
        // it can send request body immediatly in most cases and does not need to do a complete process loop.
        self.send_streams
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?
            .send(conn)?;
        Ok(())
    }

    /// Stream data are read directly into a buffer supplied as a parameter of this function to avoid copying
    /// data.
    /// # Errors
    /// It returns an error if a stream does not exist or an error happens while reading a stream, e.g.
    /// early close, protocol error, etc.
    pub fn read_data(
        &mut self,
        conn: &mut Connection,
        stream_id: StreamId,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        qinfo!([self], "read_data from stream {}.", stream_id);
        let res = self
            .recv_streams
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?
            .read_data(conn, buf);
        self.handle_stream_manipulation_output(res, stream_id, conn)
    }

    /// This is called when an application resets a stream.
    /// The application reset will close both sides.
    pub fn stream_reset_send(
        &mut self,
        conn: &mut Connection,
        stream_id: StreamId,
        error: AppError,
    ) -> Res<()> {
        qinfo!(
            [self],
            "Reset sending side of stream {} error={}.",
            stream_id,
            error
        );

        if self.send_stream_is_critical(stream_id) {
            return Err(Error::InvalidStreamId);
        }

        self.close_send(stream_id, CloseType::ResetApp(error), conn);
        conn.stream_reset_send(stream_id, error)?;
        Ok(())
    }

    pub fn stream_stop_sending(
        &mut self,
        conn: &mut Connection,
        stream_id: StreamId,
        error: AppError,
    ) -> Res<()> {
        qinfo!(
            [self],
            "Send stop sending for stream {} error={}.",
            stream_id,
            error
        );
        if self.recv_stream_is_critical(stream_id) {
            return Err(Error::InvalidStreamId);
        }

        self.close_recv(stream_id, CloseType::ResetApp(error), conn)?;

        // Stream may be already be closed and we may get an error here, but we do not care.
        conn.stream_stop_sending(stream_id, error)?;
        Ok(())
    }

    pub fn cancel_fetch(
        &mut self,
        stream_id: StreamId,
        error: AppError,
        conn: &mut Connection,
    ) -> Res<()> {
        qinfo!([self], "cancel_fetch {} error={}.", stream_id, error);
        let send_stream = self.send_streams.get(&stream_id);
        let recv_stream = self.recv_streams.get(&stream_id);
        match (send_stream, recv_stream) {
            (None, None) => return Err(Error::InvalidStreamId),
            (Some(s), None) => {
                if !matches!(
                    s.stream_type(),
                    Http3StreamType::Http | Http3StreamType::ExtendedConnect
                ) {
                    return Err(Error::InvalidStreamId);
                }
                // Stream may be already be closed and we may get an error here, but we do not care.
                mem::drop(self.stream_reset_send(conn, stream_id, error));
            }
            (None, Some(s)) => {
                if !matches!(
                    s.stream_type(),
                    Http3StreamType::Http
                        | Http3StreamType::Push
                        | Http3StreamType::ExtendedConnect
                ) {
                    return Err(Error::InvalidStreamId);
                }

                // Stream may be already be closed and we may get an error here, but we do not care.
                mem::drop(self.stream_stop_sending(conn, stream_id, error));
            }
            (Some(s), Some(r)) => {
                debug_assert_eq!(s.stream_type(), r.stream_type());
                if !matches!(
                    s.stream_type(),
                    Http3StreamType::Http | Http3StreamType::ExtendedConnect
                ) {
                    return Err(Error::InvalidStreamId);
                }
                // Stream may be already be closed and we may get an error here, but we do not care.
                mem::drop(self.stream_reset_send(conn, stream_id, error));
                // Stream may be already be closed and we may get an error here, but we do not care.
                mem::drop(self.stream_stop_sending(conn, stream_id, error));
            }
        }
        Ok(())
    }

    /// This is called when an application wants to close the sending side of a stream.
    pub fn stream_close_send(&mut self, conn: &mut Connection, stream_id: StreamId) -> Res<()> {
        qinfo!([self], "Close the sending side for stream {}.", stream_id);
        debug_assert!(self.state.active());
        let send_stream = self
            .send_streams
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?;
        // The following function may return InvalidStreamId from the transport layer if the stream has been closed
        // already. It is ok to ignore it here.
        mem::drop(send_stream.close(conn));
        if send_stream.done() {
            self.remove_send_stream(stream_id, conn);
        } else if send_stream.has_data_to_send() {
            self.streams_with_pending_data.insert(stream_id);
        }
        Ok(())
    }

    pub fn webtransport_create_session<'x, 't: 'x, T>(
        &mut self,
        conn: &mut Connection,
        events: Box<dyn ExtendedConnectEvents>,
        target: &'t T,
        headers: &'t [Header],
    ) -> Res<StreamId>
    where
        T: AsRequestTarget<'x> + ?Sized + Debug,
    {
        qinfo!([self], "Create WebTransport");
        if !self.webtransport_enabled() {
            return Err(Error::Unavailable);
        }

        let id = self.create_bidi_transport_stream(conn)?;

        let extended_conn = Rc::new(RefCell::new(WebTransportSession::new(
            id,
            events,
            self.role,
            Rc::clone(&self.qpack_encoder),
            Rc::clone(&self.qpack_decoder),
        )));
        self.add_streams(
            id,
            Box::new(extended_conn.clone()),
            Box::new(extended_conn.clone()),
        );

        let final_headers = Http3Connection::create_fetch_headers(&RequestDescription {
            method: "CONNECT",
            target,
            headers,
            connect_type: Some(ExtendedConnectType::WebTransport),
            priority: Priority::default(),
        })?;
        extended_conn
            .borrow_mut()
            .send_request(&final_headers, conn)?;
        self.streams_with_pending_data.insert(id);
        Ok(id)
    }

    pub(crate) fn webtransport_session_accept(
        &mut self,
        conn: &mut Connection,
        stream_id: StreamId,
        events: Box<dyn ExtendedConnectEvents>,
        accept: bool,
    ) -> Res<()> {
        qtrace!("Respond to WebTransport session with accept={}.", accept);
        if !self.webtransport_enabled() {
            return Err(Error::Unavailable);
        }
        let mut recv_stream = self.recv_streams.get_mut(&stream_id);
        if let Some(r) = &mut recv_stream {
            if !r
                .http_stream()
                .ok_or(Error::InvalidStreamId)?
                .extended_connect_wait_for_response()
            {
                return Err(Error::InvalidStreamId);
            }
        }

        let send_stream = self.send_streams.get_mut(&stream_id);

        match (send_stream, recv_stream, accept) {
            (None, None, _) => Err(Error::InvalidStreamId),
            (None, Some(_), _) | (Some(_), None, _) => {
                // TODO this needs a better error
                self.cancel_fetch(stream_id, Error::HttpRequestRejected.code(), conn)?;
                Err(Error::InvalidStreamId)
            }
            (Some(s), Some(_r), false) => {
                if s.http_stream()
                    .ok_or(Error::InvalidStreamId)?
                    .send_headers(&[Header::new(":status", "404")], conn)
                    .is_ok()
                {
                    mem::drop(self.stream_close_send(conn, stream_id));
                    // TODO issue 1294: add a timer to clean up the recv_stream if the peer does not do that in a short time.
                    self.streams_with_pending_data.insert(stream_id);
                } else {
                    self.cancel_fetch(stream_id, Error::HttpRequestRejected.code(), conn)?;
                }
                Ok(())
            }
            (Some(s), Some(_r), true) => {
                if s.http_stream()
                    .ok_or(Error::InvalidStreamId)?
                    .send_headers(&[Header::new(":status", "200")], conn)
                    .is_ok()
                {
                    let extended_conn =
                        Rc::new(RefCell::new(WebTransportSession::new_with_http_streams(
                            stream_id,
                            events,
                            self.role,
                            self.recv_streams.remove(&stream_id).unwrap(),
                            self.send_streams.remove(&stream_id).unwrap(),
                        )));
                    self.add_streams(
                        stream_id,
                        Box::new(extended_conn.clone()),
                        Box::new(extended_conn),
                    );
                    self.streams_with_pending_data.insert(stream_id);
                } else {
                    self.cancel_fetch(stream_id, Error::HttpRequestRejected.code(), conn)?;
                    return Err(Error::InvalidStreamId);
                }
                Ok(())
            }
        }
    }

    pub(crate) fn webtransport_close_session(
        &mut self,
        conn: &mut Connection,
        session_id: StreamId,
        error: u32,
        message: &str,
    ) -> Res<()> {
        qtrace!("Clos WebTransport session {:?}", session_id);
        let send_stream = self
            .send_streams
            .get_mut(&session_id)
            .ok_or(Error::InvalidStreamId)?;
        if send_stream.stream_type() != Http3StreamType::ExtendedConnect {
            return Err(Error::InvalidStreamId);
        }

        send_stream.close_with_message(conn, error, message)?;
        if send_stream.done() {
            self.remove_send_stream(session_id, conn);
        } else if send_stream.has_data_to_send() {
            self.streams_with_pending_data.insert(session_id);
        }
        Ok(())
    }

    pub fn webtransport_create_stream_local(
        &mut self,
        conn: &mut Connection,
        session_id: StreamId,
        stream_type: StreamType,
        send_events: Box<dyn SendStreamEvents>,
        recv_events: Box<dyn RecvStreamEvents>,
    ) -> Res<StreamId> {
        qtrace!(
            "Create new WebTransport stream session={} type={:?}",
            session_id,
            stream_type
        );

        let wt = self
            .recv_streams
            .get(&session_id)
            .ok_or(Error::InvalidStreamId)?
            .webtransport()
            .ok_or(Error::InvalidStreamId)?;
        if !wt.borrow().is_active() {
            return Err(Error::InvalidStreamId);
        }

        let stream_id = conn
            .stream_create(stream_type)
            .map_err(|e| Error::map_stream_create_errors(&e))?;

        self.webtransport_create_stream_internal(
            wt,
            stream_id,
            session_id,
            send_events,
            recv_events,
            true,
        );
        Ok(stream_id)
    }

    pub fn webtransport_create_stream_remote(
        &mut self,
        session_id: StreamId,
        stream_id: StreamId,
        send_events: Box<dyn SendStreamEvents>,
        recv_events: Box<dyn RecvStreamEvents>,
    ) -> Res<()> {
        qtrace!(
            "Create new WebTransport stream session={} stream_id={}",
            session_id,
            stream_id
        );

        let wt = self
            .recv_streams
            .get(&session_id)
            .ok_or(Error::InvalidStreamId)?
            .webtransport()
            .ok_or(Error::InvalidStreamId)?;

        self.webtransport_create_stream_internal(
            wt,
            stream_id,
            session_id,
            send_events,
            recv_events,
            false,
        );
        Ok(())
    }

    fn webtransport_create_stream_internal(
        &mut self,
        webtransport_session: Rc<RefCell<WebTransportSession>>,
        stream_id: StreamId,
        session_id: StreamId,
        send_events: Box<dyn SendStreamEvents>,
        recv_events: Box<dyn RecvStreamEvents>,
        local: bool,
    ) {
        // TODO conn.stream_keep_alive(stream_id, true)?;
        webtransport_session.borrow_mut().add_stream(stream_id);
        if stream_id.stream_type() == StreamType::UniDi {
            if local {
                self.send_streams.insert(
                    stream_id,
                    Box::new(WebTransportSendStream::new(
                        stream_id,
                        session_id,
                        send_events,
                        webtransport_session,
                        true,
                    )),
                );
            } else {
                self.recv_streams.insert(
                    stream_id,
                    Box::new(WebTransportRecvStream::new(
                        stream_id,
                        session_id,
                        recv_events,
                        webtransport_session,
                    )),
                );
            }
        } else {
            self.add_streams(
                stream_id,
                Box::new(WebTransportSendStream::new(
                    stream_id,
                    session_id,
                    send_events,
                    webtransport_session.clone(),
                    local,
                )),
                Box::new(WebTransportRecvStream::new(
                    stream_id,
                    session_id,
                    recv_events,
                    webtransport_session,
                )),
            );
        }
    }

    // If the control stream has received frames MaxPushId or Goaway which handling is specific to
    // the client and server, we must give them to the specific client/server handler.
    fn handle_control_frame(&mut self, f: HFrame) -> Res<Option<HFrame>> {
        qinfo!([self], "Handle a control frame {:?}", f);
        if !matches!(f, HFrame::Settings { .. })
            && !matches!(
                self.settings_state,
                Http3RemoteSettingsState::Received { .. }
            )
        {
            return Err(Error::HttpMissingSettings);
        }
        match f {
            HFrame::Settings { settings } => {
                self.handle_settings(settings)?;
                Ok(None)
            }
            HFrame::Goaway { .. }
            | HFrame::MaxPushId { .. }
            | HFrame::CancelPush { .. }
            | HFrame::PriorityUpdateRequest { .. }
            | HFrame::PriorityUpdatePush { .. } => Ok(Some(f)),
            _ => Err(Error::HttpFrameUnexpected),
        }
    }

    fn set_qpack_settings(&mut self, settings: &HSettings) -> Res<()> {
        let mut qpe = self.qpack_encoder.borrow_mut();
        qpe.set_max_capacity(settings.get(HSettingType::MaxTableCapacity))?;
        qpe.set_max_blocked_streams(settings.get(HSettingType::BlockedStreams))?;
        Ok(())
    }

    fn handle_settings(&mut self, new_settings: HSettings) -> Res<()> {
        qinfo!([self], "Handle SETTINGS frame.");
        match &self.settings_state {
            Http3RemoteSettingsState::NotReceived => {
                self.set_qpack_settings(&new_settings)?;
                self.webtransport.handle_settings(&new_settings);
                self.settings_state = Http3RemoteSettingsState::Received(new_settings);
                Ok(())
            }
            Http3RemoteSettingsState::ZeroRtt(settings) => {
                self.webtransport.handle_settings(&new_settings);
                let mut qpack_changed = false;
                for st in &[
                    HSettingType::MaxHeaderListSize,
                    HSettingType::MaxTableCapacity,
                    HSettingType::BlockedStreams,
                ] {
                    let zero_rtt_value = settings.get(*st);
                    let new_value = new_settings.get(*st);
                    if zero_rtt_value == new_value {
                        continue;
                    }
                    if zero_rtt_value > new_value {
                        qerror!(
                            [self],
                            "The new({}) and the old value({}) of setting {:?} do not match",
                            new_value,
                            zero_rtt_value,
                            st
                        );
                        return Err(Error::HttpSettings);
                    }

                    match st {
                        HSettingType::MaxTableCapacity => {
                            if zero_rtt_value != 0 {
                                return Err(Error::QpackError(neqo_qpack::Error::DecoderStream));
                            }
                            qpack_changed = true;
                        }
                        HSettingType::BlockedStreams => qpack_changed = true,
                        HSettingType::MaxHeaderListSize | HSettingType::EnableWebTransport => (),
                    }
                }
                if qpack_changed {
                    qdebug!([self], "Settings after zero rtt differ.");
                    self.set_qpack_settings(&(new_settings))?;
                }
                self.settings_state = Http3RemoteSettingsState::Received(new_settings);
                Ok(())
            }
            Http3RemoteSettingsState::Received { .. } => Err(Error::HttpFrameUnexpected),
        }
    }

    /// Return the current state on `Http3Connection`.
    pub fn state(&self) -> Http3State {
        self.state.clone()
    }

    /// Adds a new send and receive stream.
    pub fn add_streams(
        &mut self,
        stream_id: StreamId,
        send_stream: Box<dyn SendStream>,
        recv_stream: Box<dyn RecvStream>,
    ) {
        if send_stream.has_data_to_send() {
            self.streams_with_pending_data.insert(stream_id);
        }
        self.send_streams.insert(stream_id, send_stream);
        self.recv_streams.insert(stream_id, recv_stream);
    }

    /// Add a new recv stream. This is used for push streams.
    pub fn add_recv_stream(&mut self, stream_id: StreamId, recv_stream: Box<dyn RecvStream>) {
        self.recv_streams.insert(stream_id, recv_stream);
    }

    pub fn queue_control_frame(&mut self, frame: &HFrame) {
        self.control_stream_local.queue_frame(frame);
    }

    pub fn queue_update_priority(&mut self, stream_id: StreamId, priority: Priority) -> Res<bool> {
        let stream = self
            .recv_streams
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?
            .http_stream()
            .ok_or(Error::InvalidStreamId)?;

        if stream.maybe_update_priority(priority) {
            self.control_stream_local.queue_update_priority(stream_id);
            Ok(true)
        } else {
            Ok(false)
        }
    }

    fn recv_stream_is_critical(&self, stream_id: StreamId) -> bool {
        if let Some(r) = self.recv_streams.get(&stream_id) {
            matches!(
                r.stream_type(),
                Http3StreamType::Control | Http3StreamType::Encoder | Http3StreamType::Decoder
            )
        } else {
            false
        }
    }

    fn send_stream_is_critical(&self, stream_id: StreamId) -> bool {
        self.qpack_encoder
            .borrow()
            .local_stream_id()
            .iter()
            .chain(self.qpack_decoder.borrow().local_stream_id().iter())
            .chain(self.control_stream_local.stream_id().iter())
            .any(|id| stream_id == *id)
    }

    fn close_send(&mut self, stream_id: StreamId, close_type: CloseType, conn: &mut Connection) {
        if let Some(mut s) = self.remove_send_stream(stream_id, conn) {
            s.handle_stop_sending(close_type);
        }
    }

    fn close_recv(
        &mut self,
        stream_id: StreamId,
        close_type: CloseType,
        conn: &mut Connection,
    ) -> Res<()> {
        if let Some(mut s) = self.remove_recv_stream(stream_id, conn) {
            s.reset(close_type)?;
        }
        Ok(())
    }

    fn remove_extended_connect(
        &mut self,
        wt: &Rc<RefCell<WebTransportSession>>,
        conn: &mut Connection,
    ) {
        let out = wt.borrow_mut().take_sub_streams();
        if out.is_none() {
            return;
        }
        let (recv, send) = out.unwrap();

        for id in recv {
            qtrace!("Remove the extended connect sub receiver stream {}", id);
            // Use CloseType::ResetRemote so that an event will be sent. CloseType::LocalError would have
            // the same effect.
            if let Some(mut s) = self.recv_streams.remove(&id) {
                mem::drop(s.reset(CloseType::ResetRemote(Error::HttpRequestCancelled.code())));
            }
            mem::drop(conn.stream_stop_sending(id, Error::HttpRequestCancelled.code()));
        }
        for id in send {
            qtrace!("Remove the extended connect sub send stream {}", id);
            if let Some(mut s) = self.send_streams.remove(&id) {
                s.handle_stop_sending(CloseType::ResetRemote(Error::HttpRequestCancelled.code()));
            }
            mem::drop(conn.stream_reset_send(id, Error::HttpRequestCancelled.code()));
        }
    }

    fn remove_recv_stream(
        &mut self,
        stream_id: StreamId,
        conn: &mut Connection,
    ) -> Option<Box<dyn RecvStream>> {
        let stream = self.recv_streams.remove(&stream_id);
        if let Some(ref s) = stream {
            if s.stream_type() == Http3StreamType::ExtendedConnect {
                self.send_streams.remove(&stream_id).unwrap();
                if let Some(wt) = s.webtransport() {
                    self.remove_extended_connect(&wt, conn);
                }
            }
        }
        stream
    }

    fn remove_send_stream(
        &mut self,
        stream_id: StreamId,
        conn: &mut Connection,
    ) -> Option<Box<dyn SendStream>> {
        let stream = self.send_streams.remove(&stream_id);
        if let Some(ref s) = stream {
            if s.stream_type() == Http3StreamType::ExtendedConnect {
                if let Some(wt) = self.recv_streams.remove(&stream_id).unwrap().webtransport() {
                    self.remove_extended_connect(&wt, conn);
                }
            }
        }
        stream
    }

    pub fn webtransport_enabled(&self) -> bool {
        self.webtransport.enabled()
    }
}
