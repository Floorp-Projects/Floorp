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
use neqo_common::{qdebug, qerror, qinfo, qtrace, qwarn, Decoder, Header, MessageType, Role};
use neqo_qpack::decoder::QPackDecoder;
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::{
    streams::SendOrder, AppError, Connection, ConnectionError, DatagramTracking, State, StreamId,
    StreamType, ZeroRttState,
};
use std::cell::RefCell;
use std::collections::{BTreeSet, HashMap};
use std::fmt::Debug;
use std::mem;
use std::rc::Rc;

use crate::{Error, Res};

pub(crate) struct RequestDescription<'b, 't, T>
where
    T: AsRequestTarget<'t> + ?Sized + Debug,
{
    pub method: &'b str,
    pub connect_type: Option<ExtendedConnectType>,
    pub target: &'t T,
    pub headers: &'b [Header],
    pub priority: Priority,
}

pub enum WebTransportSessionAcceptAction {
    Accept,
    Reject(Vec<Header>),
}

impl ::std::fmt::Display for WebTransportSessionAcceptAction {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        match self {
            WebTransportSessionAcceptAction::Accept => f.write_str("Accept"),
            WebTransportSessionAcceptAction::Reject(_) => f.write_str("Reject"),
        }
    }
}

#[derive(Debug)]
enum Http3RemoteSettingsState {
    NotReceived,
    Received(HSettings),
    ZeroRtt(HSettings),
}

/// States:
/// - `Initializing`: this is the state during the QUIC handshake,
/// - `ZeroRtt`: 0-RTT has been enabled and is active
/// - Connected
/// - GoingAway(StreamId): The connection has received a `GOAWAY` frame
/// - Closing(ConnectionError): The connection is closed. The closing has been initiated by this
///   end of the connection, e.g., the `CONNECTION_CLOSE` frame has been sent. In this state, the
///   connection waits a certain amount of time to retransmit the `CONNECTION_CLOSE` frame if
///   needed.
/// - Closed(ConnectionError): This is the final close state: closing has been initialized by the
///   peer and an ack for the `CONNECTION_CLOSE` frame has been sent or the closing has been
///   initiated by this end of the connection and the ack for the `CONNECTION_CLOSE` has been
///   received or the waiting time has passed.
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

/**
# HTTP/3 core implementation

This is the core implementation of HTTP/3 protocol. It implements most of the features of the
protocol. `Http3Client` and `Http3ServerHandler` implement only client and server side behavior.

The API consists of:
- functions that correspond to the `Http3Client` and `Http3ServerHandler` API:
  - `new`
  - `close`
  - `fetch` -  only used by the client-side implementation
  - `read_data`
  - `stream_reset_send`
  - `stream_stop_sending`
  - `cancel_fetch`
  - `stream_close_send`
- functions that correspond to [`WebTransport`](https://w3c.github.io/webtransport/) functions:
  - `webtransport_create_session` -  only used by the client-side implementation
  - `webtransport_session_accept` -  only used by the server-side implementation
  - `webtransport_close_session`
  - `webtransport_create_stream_local` -  this function is called when an application wants to open
     a new `WebTransport` stream. For example `Http3Client::webtransport_create_stream` will call
     this function.
  - `webtransport_create_stream_remote` -  this is called when a `WebTransport` stream has been
     opened by the peer and this function sets up the appropriate handler for the stream.
- functions that are called by `process_http3`
  - `process_sending` - some send-streams are buffered streams(see the Streams section) and this
     function is called to trigger sending of the buffer data.
- functions that are called to  handle `ConnectionEvent`s:
  - `add_new_stream`
  - `handle_stream_readable`
  - `handle_stream_reset`
  - `handle_stream_stop_sending`
  - `handle_state_change`
  - `handle_zero_rtt_rejected`
- Additional functions:
  - `set_features_listener`
  - `stream_has_pending_data`
  - `has_data_to_send`
  - `add_streams`
  - `add_recv_stream`
  - `queue_control_frame`
  - `queue_update_priority`
  - `set_0rtt_settings`
  - `get_settings`
  - `state`
  - `webtransport_enabled`

## Streams

Each `Http3Connection` holds a list of stream handlers. Each send and receive-handler is registered in
`send_streams` and `recv_streams`. Unidirectional streams are registered only on one of the lists
and bidirectional streams are registered in both lists and the 2 handlers are independent, e.g. one
can be closed and removed ane second may still be active.

The only streams that are not registered are the local control stream, local QPACK decoder stream,
and local QPACK encoder stream. These streams are send-streams and sending data on this stream is
handled a bit differently. This is done in the `process_sending` function, i.e. the control data is
sent first and QPACK data is sent after regular stream data is sent because this stream may have
new data only after regular streams are handled (TODO we may improve this a bit to send QPACK
commands before headers.)

There are the following types of streams:
- `Control`: there is only a receiver stream of this type and the handler is `ControlStreamRemote`.
- `Decoder`: there is only a receiver stream of this type and the handler is `DecoderRecvStream`.
- `Encoder`: there is only a receiver stream of this type and the handler is `EncoderRecvStream`.
- `NewStream`: there is only a receiver stream of this type and the handler is
               `NewStreamHeadReader`.
- `Http`: `SendMessage` and `RecvMessage` handlers are responsible for this type of streams.
- `Push`: `RecvMessage` is responsible for this type of streams.
- `ExtendedConnect`: `WebTransportSession` is responsible sender and receiver handler.
- `WebTransport(StreamId)`: `WebTransportSendStream` and `WebTransportRecvStream` are responsible
                            sender and receiver handler.
- `Unknown`: These are all other stream types that are not unknown to the current implementation
             and should be handled properly by the spec, e.g., in our implementation the streams are
             reset.

The streams are registered in `send_streams` and `recv_streams` in following ways depending if they
are local or remote:
- local streams:
  - all local stream will be registered with the appropriate handler.
- remote streams:
  - all new incoming streams are registered with `NewStreamHeadReader`. This is triggered by
    `ConnectionEvent::NewStream` and `add_new_stream` is called.
  - reading from a `NewStreamHeadReader` stream, via the `receive` function, will decode a stream
    type. `NewStreamHeadReader::receive` will return `ReceiveOutput::NewStream(_)` when a stream
    type has been decoded.  After this point the stream:
    - will be regegistered with the appropriate handler,
    - will be canceled if is an unknown stream type or
    - the connection will fail if it is unallowed stream type (receiveing HTTP request on the
      client-side).
    The output is handled in `handle_new_stream`, for control,  qpack streams and partially
    `WebTransport` streams, otherwise the output is handled by `Http3Client` and `Http3ServerHandler`.


### Receiving data

Reading from a stream is triggered by `ConnectionEvent::RecvStreamReadable` events for the stream.
The receive handler is retrieved from `recv_streams` and its `RecvStream::receive` function is
called.

Receiving data on `Http` streams is also triggered by the `read_data` function.
`ConnectionEvent::RecvStreamReadable` events will trigger reading `HEADERS` frame and frame headers
for `DATA` frames which will produce `Http3ClientEvent` or `Http3ServerEvent` events. The content of
`DATA` frames is read by the application using the `read_data` function. The `read_data` function
may read frame headers for consecutive `DATA` frames.

On a `WebTransport(_)` stream data will be read only by the `read_data` function. The
`RecvStream::receive` function only produces an `Http3ClientEvent` or `Http3ServerEvent` event.

The `receive` and `read_data` functions may detect that the stream is done, e.g. FIN received. In
this case, the stream will be removed from the `recv_stream` register, see `remove_recv_stream`.

### Sending data

All sender stream handlers have buffers. Data is first written into a buffer before being supplied
to the QUIC layer. All data except the `DATA` frame and `WebTransport(_)`’s payload are written
into the buffer. This includes stream type byte, e.g. `WEBTRANSPORT_STREAM` as well. In the case of
`Http` and `WebTransport(_)` applications can write directly to the QUIC layer using the
`send_data` function to avoid copying data. Sending data via the `send_data` function is only
possible if there is no buffered data.

If a stream has buffered data it will be registered in the `streams_with_pending_data` queue and
actual sending will be performed in the `process_sending` function call. (This is done in this way,
i.e. data is buffered first and then sent, for 2 reasons: in this way, sending will happen in a
single function,  therefore error handling and clean up is easier and the QUIC layer may not be
able to accept all data and being able to buffer data is required in any case.)

The `send` and `send_data` functions may detect that the stream is closed and all outstanding data
has been transferred to the QUIC layer. In this case, the stream will be removed from the
`send_stream` register.

### `ControlStreamRemote`

The `ControlStreamRemote` handler uses `FrameReader` to read and decode frames received on the
control frame. The `receive` returns `ReceiveOutput::ControlFrames(_)` with a list of control
frames read (the list may be empty). The control frames are handled by `Http3Connection` and/or by
`Http3Client` and `Http3ServerHandler`.

### `DecoderRecvStream` and `EncoderRecvStream`

The `receive` functions of these handlers call corresponding `receive` functions of `QPackDecoder`
and `QPackDecoder`.

`DecoderRecvStream` returns `ReceiveOutput::UnblockedStreams(_)` that may contain a list of stream
ids that are unblocked by receiving qpack decoder commands. `Http3Connection` will handle this
output by calling `receive` for the listed stream ids.

`EncoderRecvStream` only returns `ReceiveOutput::NoOutput`.

Both handlers may return an error that will close the connection.

### `NewStreamHeadReader`

A new incoming receiver stream registers a `NewStreamHeadReader` handler. This handler reads the
first bytes of a stream to detect a stream type. The `receive` function returns
`ReceiveOutput::NoOutput` if a stream type is still not known by reading the available stream data
or `ReceiveOutput::NewStream(_)`. The handling of the output is explained above.

### `SendMessage` and `RecvMessage`

`RecvMessage::receive` only returns `ReceiveOutput::NoOutput`. It also have an event listener of
type `HttpRecvStreamEvents`. The listener is called when headers are ready, or data is ready, etc.

For example for `Http`   stream the listener will produce  `HeaderReady` and `DataReadable` events.

### `WebTransportSession`

A `WebTransport` session is connected to a control stream that is in essence an HTTP transaction.
Therefore, `WebTransportSession` will internally use a `SendMessage` and `RecvMessage` handler to
handle parsing and sending of HTTP part of the control stream. When HTTP headers are exchenged,
`WebTransportSession` will take over handling of stream data. `WebTransportSession` sets
`WebTransportSessionListener` as the `RecvMessage` event listener.

`WebTransportSendStream` and `WebTransportRecvStream` are associated with a `WebTransportSession`
and they will be canceled if the session is closed. To be avle to do this `WebTransportSession`
holds a list of its active streams and clean up is done in `remove_extended_connect`.

###  `WebTransportSendStream` and `WebTransportRecvStream`

`WebTransport` streams are associated with a session. `WebTransportSendStream` and
`WebTransportRecvStream` hold a reference to the session and are registered in the session upon
 creation by `Http3Connection`. The `WebTransportSendStream` and `WebTransportRecvStream`
 handlers will be unregistered from the session if they are closed, reset, or canceled.

The call to function `receive` may produce `Http3ClientEvent::DataReadable`. Actual reading of
data is done in the `read_data` function.
*/
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

    /// This function is called when a not default feature needs to be negotiated. This is currently
    /// only used for the `WebTransport` feature. The negotiation is done via the `SETTINGS` frame
    /// and when the peer's `SETTINGS` frame has been received the listener will be called.
    pub fn set_features_listener(&mut self, feature_listener: Http3ClientEvents) {
        self.webtransport.set_listener(feature_listener);
    }

    /// This function creates and initializes, i.e. send stream type, the control and qpack
    /// streams.
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

    /// This function calls the `send` function for all streams that have data to send. If a stream
    /// has data to send it will be added to the `streams_with_pending_data` list.
    ///
    /// Control and QPACK streams are handled differently and are never added to the list.
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

    /// Call `send` for all streams that need to send data. See explanation for the main structure
    /// for more details.
    pub fn process_sending(&mut self, conn: &mut Connection) -> Res<()> {
        // check if control stream has data to send.
        self.control_stream_local
            .send(conn, &mut self.recv_streams)?;

        self.send_non_control_streams(conn)?;

        self.qpack_decoder.borrow_mut().send(conn)?;
        match self.qpack_encoder.borrow_mut().send_encoder_updates(conn) {
            Ok(())
            | Err(neqo_qpack::Error::EncoderStreamBlocked | neqo_qpack::Error::DynamicTableFull) => {
            }
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

    /// This is called when a `ConnectionEvent::NewStream` event is received. This register the
    /// stream with a `NewStreamHeadReader` handler.
    pub fn add_new_stream(&mut self, stream_id: StreamId) {
        qtrace!([self], "A new stream: {}.", stream_id);
        self.recv_streams.insert(
            stream_id,
            Box::new(NewStreamHeadReader::new(stream_id, self.role)),
        );
    }

    /// The function calls `receive` for a stream. It also deals with the outcome of a read by
    /// calling `handle_stream_manipulation_output`.
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
    /// 1) a `Push(_)`, `Htttp` or `WebTransportStream(_)` stream
    /// 2) frames `MaxPushId`, `PriorityUpdateRequest`, `PriorityUpdateRequestPush` or `Goaway`
    ///    must be handled by `Http3Client`/`Server`.
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
            ReceiveOutput::NewStream(
                NewStreamType::Push(_) | NewStreamType::Http | NewStreamType::WebTransportStream(_),
            ) => Ok(output),
            ReceiveOutput::NewStream(_) => {
                unreachable!("NewStream should have been handled already")
            }
            ReceiveOutput::NoOutput => Ok(output),
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
                    && conn.zero_rtt_state() == ZeroRttState::AcceptedServer
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

    /// This is called when 0RTT has been reset to clear `send_streams`, `recv_streams` and settings.
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

    pub fn handle_datagram(&mut self, datagram: &[u8]) {
        let mut decoder = Decoder::new(datagram);
        let session = decoder
            .decode_varint()
            .and_then(|id| self.recv_streams.get_mut(&StreamId::from(id * 4)))
            .and_then(|stream| stream.webtransport());
        if let Some(s) = session {
            s.borrow_mut().datagram(decoder.decode_remainder().to_vec());
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

    /// If the new stream is a control or QPACK stream, this function creates a proper handler
    /// and perform a read.
    /// if the new stream is a `Push(_)`, `Http` or `WebTransportStream(_)` stream, the function
    /// returns `ReceiveOutput::NewStream(_)` and the caller will handle it.
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
                // set incoming WebTransport streams to be fair (share bandwidth)
                conn.stream_fairness(stream_id, true).ok();
                qinfo!(
                    [self],
                    "A new WebTransport stream {} for session {}.",
                    stream_id,
                    session_id
                );
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
        // it can send request body immediately in most cases and does not need to do a complete process loop.
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

    /// Set the stream `SendOrder`.
    /// # Errors
    /// Returns `InvalidStreamId` if the stream id doesn't exist
    pub fn stream_set_sendorder(
        conn: &mut Connection,
        stream_id: StreamId,
        sendorder: Option<SendOrder>,
    ) -> Res<()> {
        conn.stream_sendorder(stream_id, sendorder)
            .map_err(|_| Error::InvalidStreamId)
    }

    /// Set the stream Fairness.   Fair streams will share bandwidth with other
    /// streams of the same sendOrder group (or the unordered group).  Unfair streams
    /// will give bandwidth preferentially to the lowest streamId with data to send.
    /// # Errors
    /// Returns `InvalidStreamId` if the stream id doesn't exist
    pub fn stream_set_fairness(
        conn: &mut Connection,
        stream_id: StreamId,
        fairness: bool,
    ) -> Res<()> {
        conn.stream_fairness(stream_id, fairness)
            .map_err(|_| Error::InvalidStreamId)
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
        accept_res: &WebTransportSessionAcceptAction,
    ) -> Res<()> {
        qtrace!(
            "Respond to WebTransport session with accept={}.",
            accept_res
        );
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

        match (send_stream, recv_stream, accept_res) {
            (None, None, _) => Err(Error::InvalidStreamId),
            (None, Some(_), _) | (Some(_), None, _) => {
                // TODO this needs a better error
                self.cancel_fetch(stream_id, Error::HttpRequestRejected.code(), conn)?;
                Err(Error::InvalidStreamId)
            }
            (Some(s), Some(_r), WebTransportSessionAcceptAction::Reject(headers)) => {
                if s.http_stream()
                    .ok_or(Error::InvalidStreamId)?
                    .send_headers(headers, conn)
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
            (Some(s), Some(_r), WebTransportSessionAcceptAction::Accept) => {
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
        // Set outgoing WebTransport streams to be fair (share bandwidth)
        // This really can't fail, panics if it does
        conn.stream_fairness(stream_id, true).unwrap();

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

    pub fn webtransport_send_datagram(
        &mut self,
        session_id: StreamId,
        conn: &mut Connection,
        buf: &[u8],
        id: impl Into<DatagramTracking>,
    ) -> Res<()> {
        self.recv_streams
            .get_mut(&session_id)
            .ok_or(Error::InvalidStreamId)?
            .webtransport()
            .ok_or(Error::InvalidStreamId)?
            .borrow_mut()
            .send_datagram(conn, buf, id)
    }

    /// If the control stream has received frames `MaxPushId`, `Goaway`, `PriorityUpdateRequest` or
    /// `PriorityUpdateRequestPush` which handling is specific to the client and server, we must
    /// give them to the specific client/server handler.
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
                        HSettingType::MaxHeaderListSize
                        | HSettingType::EnableWebTransport
                        | HSettingType::EnableH3Datagram => (),
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
        let (recv, send) = wt.borrow_mut().take_sub_streams();

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
