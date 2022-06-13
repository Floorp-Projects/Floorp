// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::connection::Http3State;
use crate::connection_server::Http3ServerHandler;
use crate::server_connection_events::Http3ServerConnEvent;
use crate::server_events::{
    Http3OrWebTransportStream, Http3ServerEvent, Http3ServerEvents, WebTransportRequest,
};
use crate::settings::HttpZeroRttChecker;
use crate::{Http3Parameters, Http3StreamInfo, Res};
use neqo_common::{qtrace, Datagram};
use neqo_crypto::{AntiReplay, Cipher, PrivateKey, PublicKey, ZeroRttChecker};
use neqo_transport::server::{ActiveConnectionRef, Server, ValidateAddress};
use neqo_transport::{ConnectionIdGenerator, Output};
use std::cell::RefCell;
use std::cell::RefMut;
use std::collections::HashMap;
use std::path::PathBuf;
use std::rc::Rc;
use std::time::Instant;

type HandlerRef = Rc<RefCell<Http3ServerHandler>>;

const MAX_EVENT_DATA_SIZE: usize = 1024;

pub struct Http3Server {
    server: Server,
    http3_parameters: Http3Parameters,
    http3_handlers: HashMap<ActiveConnectionRef, HandlerRef>,
    events: Http3ServerEvents,
}

impl ::std::fmt::Display for Http3Server {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 server ")
    }
}

impl Http3Server {
    /// # Errors
    /// Making a `neqo_transport::Server` may produce an error. This can only be a crypto error if
    /// the socket can't be created or configured.
    pub fn new(
        now: Instant,
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: AntiReplay,
        cid_manager: Rc<RefCell<dyn ConnectionIdGenerator>>,
        http3_parameters: Http3Parameters,
        zero_rtt_checker: Option<Box<dyn ZeroRttChecker>>,
    ) -> Res<Self> {
        Ok(Self {
            server: Server::new(
                now,
                certs,
                protocols,
                anti_replay,
                zero_rtt_checker
                    .unwrap_or_else(|| Box::new(HttpZeroRttChecker::new(http3_parameters.clone()))),
                cid_manager,
                http3_parameters.get_connection_parameters().clone(),
            )?,
            http3_parameters,
            http3_handlers: HashMap::new(),
            events: Http3ServerEvents::default(),
        })
    }

    pub fn set_qlog_dir(&mut self, dir: Option<PathBuf>) {
        self.server.set_qlog_dir(dir);
    }

    pub fn set_validation(&mut self, v: ValidateAddress) {
        self.server.set_validation(v);
    }

    pub fn set_ciphers(&mut self, ciphers: impl AsRef<[Cipher]>) {
        self.server.set_ciphers(ciphers);
    }

    /// Enable encrypted client hello (ECH).
    ///
    /// # Errors
    /// Only when NSS can't serialize a configuration.
    pub fn enable_ech(
        &mut self,
        config: u8,
        public_name: &str,
        sk: &PrivateKey,
        pk: &PublicKey,
    ) -> Res<()> {
        self.server.enable_ech(config, public_name, sk, pk)?;
        Ok(())
    }

    #[must_use]
    pub fn ech_config(&self) -> &[u8] {
        self.server.ech_config()
    }

    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        qtrace!([self], "Process.");
        let out = self.server.process(dgram, now);
        self.process_http3(now);
        // If we do not that a dgram already try again after process_http3.
        match out {
            Output::Datagram(d) => {
                qtrace!([self], "Send packet: {:?}", d);
                Output::Datagram(d)
            }
            _ => self.server.process(None, now),
        }
    }

    /// Process HTTP3 layer.
    fn process_http3(&mut self, now: Instant) {
        qtrace!([self], "Process http3 internal.");
        let mut active_conns = self.server.active_connections();

        // We need to find connections that needs to be process on http3 level.
        let mut http3_active: Vec<ActiveConnectionRef> = self
            .http3_handlers
            .iter()
            .filter_map(|(conn, handler)| {
                if handler.borrow_mut().should_be_processed() && !active_conns.contains(conn) {
                    Some(conn)
                } else {
                    None
                }
            })
            .cloned()
            .collect();
        // For http_active connection we need to put them in neqo-transport's server
        // waiting queue.
        active_conns.append(&mut http3_active);
        active_conns.dedup();
        active_conns
            .iter()
            .for_each(|conn| self.server.add_to_waiting(conn.clone()));
        for mut conn in active_conns {
            self.process_events(&mut conn, now);
        }
    }

    fn process_events(&mut self, conn: &mut ActiveConnectionRef, now: Instant) {
        let mut remove = false;
        let http3_parameters = &self.http3_parameters;
        {
            let handler = self.http3_handlers.entry(conn.clone()).or_insert_with(|| {
                Rc::new(RefCell::new(Http3ServerHandler::new(
                    http3_parameters.clone(),
                )))
            });
            handler
                .borrow_mut()
                .process_http3(&mut conn.borrow_mut(), now);
            let mut handler_borrowed = handler.borrow_mut();
            while let Some(e) = handler_borrowed.next_event() {
                match e {
                    Http3ServerConnEvent::Headers {
                        stream_info,
                        headers,
                        fin,
                    } => self.events.headers(
                        Http3OrWebTransportStream::new(conn.clone(), handler.clone(), stream_info),
                        headers,
                        fin,
                    ),
                    Http3ServerConnEvent::DataReadable { stream_info } => {
                        prepare_data(
                            stream_info,
                            &mut handler_borrowed,
                            conn,
                            handler,
                            now,
                            &mut self.events,
                        );
                    }
                    Http3ServerConnEvent::DataWritable { stream_info } => self
                        .events
                        .data_writable(conn.clone(), handler.clone(), stream_info),
                    Http3ServerConnEvent::StreamReset { stream_info, error } => {
                        self.events
                            .stream_reset(conn.clone(), handler.clone(), stream_info, error);
                    }
                    Http3ServerConnEvent::StreamStopSending { stream_info, error } => {
                        self.events.stream_stop_sending(
                            conn.clone(),
                            handler.clone(),
                            stream_info,
                            error,
                        );
                    }
                    Http3ServerConnEvent::StateChange(state) => {
                        self.events
                            .connection_state_change(conn.clone(), state.clone());
                        if let Http3State::Closed { .. } = state {
                            remove = true;
                        }
                    }
                    Http3ServerConnEvent::PriorityUpdate {
                        stream_id,
                        priority,
                    } => {
                        self.events.priority_update(stream_id, priority);
                    }
                    Http3ServerConnEvent::ExtendedConnect { stream_id, headers } => {
                        self.events.webtransport_new_session(
                            WebTransportRequest::new(conn.clone(), handler.clone(), stream_id),
                            headers,
                        );
                    }
                    Http3ServerConnEvent::ExtendedConnectClosed {
                        stream_id, reason, ..
                    } => self.events.webtransport_session_closed(
                        WebTransportRequest::new(conn.clone(), handler.clone(), stream_id),
                        reason,
                    ),
                    Http3ServerConnEvent::ExtendedConnectNewStream(stream_info) => self
                        .events
                        .webtransport_new_stream(Http3OrWebTransportStream::new(
                            conn.clone(),
                            handler.clone(),
                            stream_info,
                        )),
                }
            }
        }
        if remove {
            self.http3_handlers.remove(&conn.clone());
        }
    }

    /// Get all current events. Best used just in debug/testing code, use
    /// `next_event` instead.
    pub fn events(&mut self) -> impl Iterator<Item = Http3ServerEvent> {
        self.events.events()
    }

    /// Return true if there are outstanding events.
    #[must_use]
    pub fn has_events(&self) -> bool {
        self.events.has_events()
    }

    /// Get events that indicate state changes on the connection. This method
    /// correctly handles cases where handling one event can obsolete
    /// previously-queued events, or cause new events to be generated.
    pub fn next_event(&mut self) -> Option<Http3ServerEvent> {
        self.events.next_event()
    }
}
fn prepare_data(
    stream_info: Http3StreamInfo,
    handler_borrowed: &mut RefMut<Http3ServerHandler>,
    conn: &mut ActiveConnectionRef,
    handler: &HandlerRef,
    now: Instant,
    events: &mut Http3ServerEvents,
) {
    loop {
        let mut data = vec![0; MAX_EVENT_DATA_SIZE];
        let res = handler_borrowed.read_data(
            &mut conn.borrow_mut(),
            now,
            stream_info.stream_id(),
            &mut data,
        );
        if let Ok((amount, fin)) = res {
            if amount > 0 || fin {
                if amount < MAX_EVENT_DATA_SIZE {
                    data.resize(amount, 0);
                }

                events.data(conn.clone(), handler.clone(), stream_info, data, fin);
            }
            if amount < MAX_EVENT_DATA_SIZE || fin {
                break;
            }
        } else {
            // Any error will closed the handler, just ignore this event, the next event must
            // be a state change event.
            break;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{Http3Server, Http3ServerEvent, Http3State, Rc, RefCell};
    use crate::{Error, HFrame, Header, Http3Parameters, Priority};
    use neqo_common::event::Provider;
    use neqo_common::Encoder;
    use neqo_crypto::{AuthenticationStatus, ZeroRttCheckResult, ZeroRttChecker};
    use neqo_qpack::{encoder::QPackEncoder, QpackSettings};
    use neqo_transport::{
        Connection, ConnectionError, ConnectionEvent, State, StreamId, StreamType, ZeroRttState,
    };
    use std::collections::HashMap;
    use std::mem;
    use std::ops::{Deref, DerefMut};
    use test_fixture::{
        anti_replay, default_client, fixture_init, now, CountingConnectionIdGenerator,
        DEFAULT_ALPN, DEFAULT_KEYS,
    };

    const DEFAULT_SETTINGS: QpackSettings = QpackSettings {
        max_table_size_encoder: 100,
        max_table_size_decoder: 100,
        max_blocked_streams: 100,
    };

    fn http3params(qpack_settings: QpackSettings) -> Http3Parameters {
        Http3Parameters::default()
            .max_table_size_encoder(qpack_settings.max_table_size_encoder)
            .max_table_size_decoder(qpack_settings.max_table_size_decoder)
            .max_blocked_streams(qpack_settings.max_blocked_streams)
    }

    pub fn create_server(conn_params: Http3Parameters) -> Http3Server {
        fixture_init();
        Http3Server::new(
            now(),
            DEFAULT_KEYS,
            DEFAULT_ALPN,
            anti_replay(),
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            conn_params,
            None,
        )
        .expect("create a server")
    }

    /// Create a http3 server with default configuration.
    pub fn default_server() -> Http3Server {
        create_server(http3params(DEFAULT_SETTINGS))
    }

    fn assert_closed(hconn: &mut Http3Server, expected: &Error) {
        let err = ConnectionError::Application(expected.code());
        let closed = |e| matches!(e, Http3ServerEvent::StateChange{ state: Http3State::Closing(e) | Http3State::Closed(e), .. } if e == err);
        assert!(hconn.events().any(closed));
    }

    fn assert_connected(hconn: &mut Http3Server) {
        let connected = |e| {
            matches!(
                e,
                Http3ServerEvent::StateChange {
                    state: Http3State::Connected,
                    ..
                }
            )
        };
        assert!(hconn.events().any(connected));
    }

    fn assert_not_closed(hconn: &mut Http3Server) {
        let closed = |e| {
            matches!(
                e,
                Http3ServerEvent::StateChange {
                    state: Http3State::Closing(..),
                    ..
                }
            )
        };
        assert!(!hconn.events().any(closed));
    }

    const CLIENT_SIDE_CONTROL_STREAM_ID: StreamId = StreamId::new(2);
    const CLIENT_SIDE_ENCODER_STREAM_ID: StreamId = StreamId::new(6);
    const CLIENT_SIDE_DECODER_STREAM_ID: StreamId = StreamId::new(10);
    const SERVER_SIDE_CONTROL_STREAM_ID: StreamId = StreamId::new(3);
    const SERVER_SIDE_ENCODER_STREAM_ID: StreamId = StreamId::new(7);
    const SERVER_SIDE_DECODER_STREAM_ID: StreamId = StreamId::new(11);

    fn connect_transport(server: &mut Http3Server, client: &mut Connection, resume: bool) {
        let c1 = client.process(None, now()).dgram();
        let s1 = server.process(c1, now()).dgram();
        let c2 = client.process(s1, now()).dgram();
        let needs_auth = client
            .events()
            .any(|e| e == ConnectionEvent::AuthenticationNeeded);
        let c2 = if needs_auth {
            assert!(!resume);
            // c2 should just be an ACK, so absorb that.
            let s_ack = server.process(c2, now()).dgram();
            assert!(s_ack.is_none());

            client.authenticated(AuthenticationStatus::Ok, now());
            client.process(None, now()).dgram()
        } else {
            assert!(resume);
            c2
        };
        assert!(client.state().connected());
        let s2 = server.process(c2, now()).dgram();
        assert_connected(server);
        let c3 = client.process(s2, now()).dgram();
        assert!(c3.is_none());
    }

    // Start a client/server and check setting frame.
    fn connect_and_receive_settings_with_server(server: &mut Http3Server) -> Connection {
        const CONTROL_STREAM_DATA: &[u8] = &[0x0, 0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64];

        let mut client = default_client();
        connect_transport(server, &mut client, false);

        let mut connected = false;
        while let Some(e) = client.next_event() {
            match e {
                ConnectionEvent::NewStream { stream_id } => {
                    assert!(
                        (stream_id == SERVER_SIDE_CONTROL_STREAM_ID)
                            || (stream_id == SERVER_SIDE_ENCODER_STREAM_ID)
                            || (stream_id == SERVER_SIDE_DECODER_STREAM_ID)
                    );
                    assert_eq!(stream_id.stream_type(), StreamType::UniDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    if stream_id == CLIENT_SIDE_CONTROL_STREAM_ID
                        || stream_id == SERVER_SIDE_CONTROL_STREAM_ID
                    {
                        // the control stream
                        let mut buf = [0_u8; 100];
                        let (amount, fin) = client.stream_recv(stream_id, &mut buf).unwrap();
                        assert!(!fin);
                        assert_eq!(amount, CONTROL_STREAM_DATA.len());
                        assert_eq!(&buf[..9], CONTROL_STREAM_DATA);
                    } else if stream_id == CLIENT_SIDE_ENCODER_STREAM_ID
                        || stream_id == SERVER_SIDE_ENCODER_STREAM_ID
                    {
                        let mut buf = [0_u8; 100];
                        let (amount, fin) = client.stream_recv(stream_id, &mut buf).unwrap();
                        assert!(!fin);
                        assert_eq!(amount, 1);
                        assert_eq!(buf[..1], [0x2]);
                    } else if stream_id == CLIENT_SIDE_DECODER_STREAM_ID
                        || stream_id == SERVER_SIDE_DECODER_STREAM_ID
                    {
                        let mut buf = [0_u8; 100];
                        let (amount, fin) = client.stream_recv(stream_id, &mut buf).unwrap();
                        assert!(!fin);
                        assert_eq!(amount, 1);
                        assert_eq!(buf[..1], [0x3]);
                    } else {
                        panic!("unexpected event");
                    }
                }
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    assert!(
                        (stream_id == CLIENT_SIDE_CONTROL_STREAM_ID)
                            || (stream_id == CLIENT_SIDE_ENCODER_STREAM_ID)
                            || (stream_id == CLIENT_SIDE_DECODER_STREAM_ID)
                    );
                }
                ConnectionEvent::StateChange(State::Connected) => connected = true,
                ConnectionEvent::StateChange(_) | ConnectionEvent::SendStreamCreatable { .. } => (),
                _ => panic!("unexpected event"),
            }
        }
        assert!(connected);
        client
    }

    fn connect_and_receive_settings() -> (Http3Server, Connection) {
        // Create a server and connect it to a client.
        // We will have a http3 server on one side and a neqo_transport
        // connection on the other side so that we can check what the http3
        // side sends and also to simulate an incorrectly behaving http3
        // client.

        let mut server = default_server();
        let client = connect_and_receive_settings_with_server(&mut server);
        (server, client)
    }

    // Test http3 connection inintialization.
    // The server will open the control and qpack streams and send SETTINGS frame.
    #[test]
    fn test_server_connect() {
        mem::drop(connect_and_receive_settings());
    }

    struct PeerConnection {
        conn: Connection,
        control_stream_id: StreamId,
    }

    impl PeerConnection {
        /// A shortcut for sending on the control stream.
        fn control_send(&mut self, data: &[u8]) {
            let res = self.conn.stream_send(self.control_stream_id, data);
            assert_eq!(res, Ok(data.len()));
        }
    }

    impl Deref for PeerConnection {
        type Target = Connection;
        fn deref(&self) -> &Self::Target {
            &self.conn
        }
    }

    impl DerefMut for PeerConnection {
        fn deref_mut(&mut self) -> &mut Self::Target {
            &mut self.conn
        }
    }

    // Connect transport, send and receive settings.
    fn connect_to(server: &mut Http3Server) -> PeerConnection {
        let mut neqo_trans_conn = connect_and_receive_settings_with_server(server);
        let control_stream = neqo_trans_conn.stream_create(StreamType::UniDi).unwrap();
        let mut sent = neqo_trans_conn.stream_send(
            control_stream,
            &[0x0, 0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64],
        );
        assert_eq!(sent, Ok(9));
        let mut encoder = QPackEncoder::new(
            &QpackSettings {
                max_table_size_encoder: 100,
                max_table_size_decoder: 0,
                max_blocked_streams: 0,
            },
            true,
        );
        encoder.add_send_stream(neqo_trans_conn.stream_create(StreamType::UniDi).unwrap());
        encoder.send_encoder_updates(&mut neqo_trans_conn).unwrap();
        let decoder_stream = neqo_trans_conn.stream_create(StreamType::UniDi).unwrap();
        sent = neqo_trans_conn.stream_send(decoder_stream, &[0x3]);
        assert_eq!(sent, Ok(1));
        let out1 = neqo_trans_conn.process(None, now());
        let out2 = server.process(out1.dgram(), now());
        mem::drop(neqo_trans_conn.process(out2.dgram(), now()));

        // assert no error occured.
        assert_not_closed(server);

        PeerConnection {
            conn: neqo_trans_conn,
            control_stream_id: control_stream,
        }
    }

    fn connect() -> (Http3Server, PeerConnection) {
        let mut server = default_server();
        let client = connect_to(&mut server);
        (server, client)
    }

    // Server: Test receiving a new control stream and a SETTINGS frame.
    #[test]
    fn test_server_receive_control_frame() {
        mem::drop(connect());
    }

    // Server: Test that the connection will be closed if control stream
    // has been closed.
    #[test]
    fn test_server_close_control_stream() {
        let (mut hconn, mut peer_conn) = connect();
        let control = peer_conn.control_stream_id;
        peer_conn.stream_close_send(control).unwrap();
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpClosedCriticalStream);
    }

    // Server: test missing SETTINGS frame
    // (the first frame sent is a MAX_PUSH_ID frame).
    #[test]
    fn test_server_missing_settings() {
        let (mut hconn, mut neqo_trans_conn) = connect_and_receive_settings();
        // Create client control stream.
        let control_stream = neqo_trans_conn.stream_create(StreamType::UniDi).unwrap();
        // Send a MAX_PUSH_ID frame instead.
        let sent = neqo_trans_conn.stream_send(control_stream, &[0x0, 0xd, 0x1, 0xf]);
        assert_eq!(sent, Ok(4));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpMissingSettings);
    }

    // Server: receiving SETTINGS frame twice causes connection close
    // with error HTTP_UNEXPECTED_FRAME.
    #[test]
    fn test_server_receive_settings_twice() {
        let (mut hconn, mut peer_conn) = connect();
        // send the second SETTINGS frame.
        peer_conn.control_send(&[0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64]);
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpFrameUnexpected);
    }

    fn priority_update_check_id(stream_id: StreamId, valid: bool) {
        let (mut hconn, mut peer_conn) = connect();
        // send a priority update
        let frame = HFrame::PriorityUpdateRequest {
            element_id: stream_id.as_u64(),
            priority: Priority::default(),
        };
        let mut e = Encoder::default();
        frame.encode(&mut e);
        peer_conn.control_send(e.as_ref());
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        // check if the given connection got closed on invalid stream ids
        if valid {
            assert_not_closed(&mut hconn);
        } else {
            assert_closed(&mut hconn, &Error::HttpId);
        }
    }

    #[test]
    fn test_priority_update_valid_id_0() {
        // Client-Initiated, Bidirectional
        priority_update_check_id(StreamId::new(0), true);
    }
    #[test]
    fn test_priority_update_invalid_id_1() {
        // Server-Initiated, Bidirectional
        priority_update_check_id(StreamId::new(1), false);
    }
    #[test]
    fn test_priority_update_invalid_id_2() {
        // Client-Initiated, Unidirectional
        priority_update_check_id(StreamId::new(2), false);
    }
    #[test]
    fn test_priority_update_invalid_id_3() {
        // Server-Initiated, Unidirectional
        priority_update_check_id(StreamId::new(3), false);
    }

    #[test]
    fn test_priority_update_invalid_large_id() {
        // Server-Initiated, Unidirectional (divisible by 4)
        priority_update_check_id(StreamId::new(1_000_000_000), false);
    }

    fn test_wrong_frame_on_control_stream(v: &[u8]) {
        let (mut hconn, mut peer_conn) = connect();

        // receive a frame that is not allowed on the control stream.
        peer_conn.control_send(v);

        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpFrameUnexpected);
    }

    // send DATA frame on a control stream
    #[test]
    fn test_server_data_frame_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x0, 0x2, 0x1, 0x2]);
    }

    // send HEADERS frame on a cortrol stream
    #[test]
    fn test_server_headers_frame_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x1, 0x2, 0x1, 0x2]);
    }

    // send PUSH_PROMISE frame on a cortrol stream
    #[test]
    fn test_server_push_promise_frame_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0x5, 0x2, 0x1, 0x2]);
    }

    // Server: receive unknown stream type
    // also test getting stream id that does not fit into a single byte.
    #[test]
    fn test_server_received_unknown_stream() {
        let (mut hconn, mut peer_conn) = connect();

        // create a stream with unknown type.
        let new_stream_id = peer_conn.stream_create(StreamType::UniDi).unwrap();
        let _ = peer_conn
            .stream_send(new_stream_id, &[0x41, 0x19, 0x4, 0x4, 0x6, 0x0, 0x8, 0x0])
            .unwrap();
        let out = peer_conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        mem::drop(peer_conn.process(out.dgram(), now()));
        let out = hconn.process(None, now());
        mem::drop(peer_conn.process(out.dgram(), now()));

        // check for stop-sending with Error::HttpStreamCreation.
        let mut stop_sending_event_found = false;
        while let Some(e) = peer_conn.next_event() {
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
        assert_not_closed(&mut hconn);
    }

    // Server: receiving a push stream on a server should cause WrongStreamDirection
    #[test]
    fn test_server_received_push_stream() {
        let (mut hconn, mut peer_conn) = connect();

        // create a push stream.
        let push_stream_id = peer_conn.stream_create(StreamType::UniDi).unwrap();
        let _ = peer_conn.stream_send(push_stream_id, &[0x1]).unwrap();
        let out = peer_conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        mem::drop(peer_conn.conn.process(out.dgram(), now()));
        assert_closed(&mut hconn, &Error::HttpStreamCreation);
    }

    //// Test reading of a slowly streamed frame. bytes are received one by one
    #[test]
    fn test_server_frame_reading() {
        let (mut hconn, mut peer_conn) = connect_and_receive_settings();

        // create a control stream.
        let control_stream = peer_conn.stream_create(StreamType::UniDi).unwrap();

        // send the stream type
        let mut sent = peer_conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // start sending SETTINGS frame
        sent = peer_conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x6]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x8]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        assert_not_closed(&mut hconn);

        // Now test PushPromise
        sent = peer_conn.stream_send(control_stream, &[0x5]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x5]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x61]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x62]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x63]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = peer_conn.stream_send(control_stream, &[0x64]);
        assert_eq!(sent, Ok(1));
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // PUSH_PROMISE on a control stream will cause an error
        assert_closed(&mut hconn, &Error::HttpFrameUnexpected);
    }

    // Test reading of a slowly streamed frame. bytes are received one by one
    fn test_incomplete_frame(res: &[u8]) {
        let (mut hconn, mut peer_conn) = connect_and_receive_settings();

        // send an incomplete reequest.
        let stream_id = peer_conn.stream_create(StreamType::BiDi).unwrap();
        peer_conn.stream_send(stream_id, res).unwrap();
        peer_conn.stream_close_send(stream_id).unwrap();

        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        assert_closed(&mut hconn, &Error::HttpFrame);
    }

    const REQUEST_WITH_BODY: &[u8] = &[
        // headers
        0x01, 0x10, 0x00, 0x00, 0xd1, 0xd7, 0x50, 0x89, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x2e,
        0x43, 0xd3, 0xc1, // the first data frame.
        0x0, 0x3, 0x61, 0x62, 0x63, // the second data frame.
        0x0, 0x3, 0x64, 0x65, 0x66,
    ];
    const REQUEST_BODY: &[u8] = &[0x61, 0x62, 0x63, 0x64, 0x65, 0x66];

    const RESPONSE_BODY: &[u8] = &[0x67, 0x68, 0x69];

    fn check_request_header(header: &[Header]) {
        let expected_request_header = &[
            Header::new(":method", "GET"),
            Header::new(":scheme", "https"),
            Header::new(":authority", "something.com"),
            Header::new(":path", "/"),
        ];
        assert_eq!(header, expected_request_header);
    }

    // Incomplete DATA frame
    #[test]
    fn test_server_incomplete_data_frame() {
        test_incomplete_frame(&REQUEST_WITH_BODY[..22]);
    }

    // Incomplete HEADERS frame
    #[test]
    fn test_server_incomplete_headers_frame() {
        test_incomplete_frame(&REQUEST_WITH_BODY[..10]);
    }

    #[test]
    fn test_server_incomplete_unknown_frame() {
        test_incomplete_frame(&[0x21]);
    }

    #[test]
    fn test_server_request_with_body() {
        let (mut hconn, mut peer_conn) = connect();

        let stream_id = peer_conn.stream_create(StreamType::BiDi).unwrap();
        peer_conn.stream_send(stream_id, REQUEST_WITH_BODY).unwrap();
        peer_conn.stream_close_send(stream_id).unwrap();

        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Check connection event. There should be 1 Header and 2 data events.
        let mut headers_frames = 0;
        let mut data_received = 0;
        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers { headers, fin, .. } => {
                    check_request_header(&headers);
                    assert!(!fin);
                    headers_frames += 1;
                }
                Http3ServerEvent::Data {
                    mut stream,
                    data,
                    fin,
                } => {
                    assert_eq!(data, REQUEST_BODY);
                    assert!(fin);
                    stream
                        .send_headers(&[
                            Header::new(":status", "200"),
                            Header::new("content-length", "3"),
                        ])
                        .unwrap();
                    stream.send_data(RESPONSE_BODY).unwrap();
                    data_received += 1;
                }
                Http3ServerEvent::DataWritable { .. }
                | Http3ServerEvent::StreamReset { .. }
                | Http3ServerEvent::StreamStopSending { .. }
                | Http3ServerEvent::StateChange { .. }
                | Http3ServerEvent::PriorityUpdate { .. }
                | Http3ServerEvent::WebTransport(_) => {}
            }
        }
        assert_eq!(headers_frames, 1);
        assert_eq!(data_received, 1);
    }

    #[test]
    fn test_server_request_with_body_send_stop_sending() {
        let (mut hconn, mut peer_conn) = connect();

        let stream_id = peer_conn.stream_create(StreamType::BiDi).unwrap();
        // Send only request headers for now.
        peer_conn
            .stream_send(stream_id, &REQUEST_WITH_BODY[..20])
            .unwrap();

        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Check connection event. There should be 1 Header and no data events.
        let mut headers_frames = 0;
        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers {
                    mut stream,
                    headers,
                    fin,
                } => {
                    check_request_header(&headers);
                    assert!(!fin);
                    headers_frames += 1;
                    stream
                        .stream_stop_sending(Error::HttpNoError.code())
                        .unwrap();
                    stream
                        .send_headers(&[
                            Header::new(":status", "200"),
                            Header::new("content-length", "3"),
                        ])
                        .unwrap();
                    stream.send_data(RESPONSE_BODY).unwrap();
                }
                Http3ServerEvent::Data { .. } => {
                    panic!("We should not have a Data event");
                }
                Http3ServerEvent::DataWritable { .. }
                | Http3ServerEvent::StreamReset { .. }
                | Http3ServerEvent::StreamStopSending { .. }
                | Http3ServerEvent::StateChange { .. }
                | Http3ServerEvent::PriorityUpdate { .. }
                | Http3ServerEvent::WebTransport(_) => {}
            }
        }
        let out = hconn.process(None, now());

        // Send data.
        peer_conn
            .stream_send(stream_id, &REQUEST_WITH_BODY[20..])
            .unwrap();
        peer_conn.stream_close_send(stream_id).unwrap();

        let out = peer_conn.process(out.dgram(), now());
        hconn.process(out.dgram(), now());

        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers { .. } => {
                    panic!("We should not have a Header event");
                }
                Http3ServerEvent::Data { .. } => {
                    panic!("We should not have a Data event");
                }
                Http3ServerEvent::DataWritable { .. }
                | Http3ServerEvent::StreamReset { .. }
                | Http3ServerEvent::StreamStopSending { .. }
                | Http3ServerEvent::StateChange { .. }
                | Http3ServerEvent::PriorityUpdate { .. }
                | Http3ServerEvent::WebTransport(_) => {}
            }
        }
        assert_eq!(headers_frames, 1);
    }

    #[test]
    fn test_server_request_with_body_server_reset() {
        let (mut hconn, mut peer_conn) = connect();

        let request_stream_id = peer_conn.stream_create(StreamType::BiDi).unwrap();
        // Send only request headers for now.
        peer_conn
            .stream_send(request_stream_id, &REQUEST_WITH_BODY[..20])
            .unwrap();

        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Check connection event. There should be 1 Header and no data events.
        // The server will reset the stream.
        let mut headers_frames = 0;
        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers {
                    mut stream,
                    headers,
                    fin,
                } => {
                    check_request_header(&headers);
                    assert!(!fin);
                    headers_frames += 1;
                    stream
                        .cancel_fetch(Error::HttpRequestRejected.code())
                        .unwrap();
                }
                Http3ServerEvent::Data { .. } => {
                    panic!("We should not have a Data event");
                }
                Http3ServerEvent::DataWritable { .. }
                | Http3ServerEvent::StreamReset { .. }
                | Http3ServerEvent::StreamStopSending { .. }
                | Http3ServerEvent::StateChange { .. }
                | Http3ServerEvent::PriorityUpdate { .. }
                | Http3ServerEvent::WebTransport(_) => {}
            }
        }
        let out = hconn.process(None, now());

        let out = peer_conn.process(out.dgram(), now());
        hconn.process(out.dgram(), now());

        // Check that STOP_SENDING and REET has been received.
        let mut reset = 0;
        let mut stop_sending = 0;
        while let Some(event) = peer_conn.next_event() {
            match event {
                ConnectionEvent::RecvStreamReset { stream_id, .. } => {
                    assert_eq!(request_stream_id, stream_id);
                    reset += 1;
                }
                ConnectionEvent::SendStreamStopSending { stream_id, .. } => {
                    assert_eq!(request_stream_id, stream_id);
                    stop_sending += 1;
                }
                _ => {}
            }
        }
        assert_eq!(headers_frames, 1);
        assert_eq!(reset, 1);
        assert_eq!(stop_sending, 1);
    }

    // Server: Test that the connection will be closed if the local control stream
    // has been reset.
    #[test]
    fn test_server_reset_control_stream() {
        let (mut hconn, mut peer_conn) = connect();
        peer_conn
            .stream_reset_send(CLIENT_SIDE_CONTROL_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpClosedCriticalStream);
    }

    // Server: Test that the connection will be closed if the client side encoder stream
    // has been reset.
    #[test]
    fn test_server_reset_client_side_encoder_stream() {
        let (mut hconn, mut peer_conn) = connect();
        peer_conn
            .stream_reset_send(CLIENT_SIDE_ENCODER_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpClosedCriticalStream);
    }

    // Server: Test that the connection will be closed if the client side decoder stream
    // has been reset.
    #[test]
    fn test_server_reset_client_side_decoder_stream() {
        let (mut hconn, mut peer_conn) = connect();
        peer_conn
            .stream_reset_send(CLIENT_SIDE_DECODER_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpClosedCriticalStream);
    }

    // Server: Test that the connection will be closed if the local control stream
    // has received a stop_sending.
    #[test]
    fn test_client_stop_sending_control_stream() {
        let (mut hconn, mut peer_conn) = connect();

        peer_conn
            .stream_stop_sending(SERVER_SIDE_CONTROL_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpClosedCriticalStream);
    }

    // Server: Test that the connection will be closed if the server side encoder stream
    // has received a stop_sending.
    #[test]
    fn test_server_stop_sending_encoder_stream() {
        let (mut hconn, mut peer_conn) = connect();
        peer_conn
            .stream_stop_sending(SERVER_SIDE_ENCODER_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpClosedCriticalStream);
    }

    // Server: Test that the connection will be closed if the server side decoder stream
    // has received a stop_sending.
    #[test]
    fn test_server_stop_sending_decoder_stream() {
        let (mut hconn, mut peer_conn) = connect();
        peer_conn
            .stream_stop_sending(SERVER_SIDE_DECODER_STREAM_ID, Error::HttpNoError.code())
            .unwrap();
        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, &Error::HttpClosedCriticalStream);
    }

    /// Perform a handshake, then another with the token from the first.
    /// The second should always resume, but it might not always accept early data.
    fn zero_rtt_with_settings(conn_params: Http3Parameters, zero_rtt: ZeroRttState) {
        let (_, mut client) = connect();
        let token = client.events().find_map(|e| {
            if let ConnectionEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        });
        assert!(token.is_some());

        let mut server = create_server(conn_params);
        let mut client = default_client();
        client.enable_resumption(now(), token.unwrap()).unwrap();

        connect_transport(&mut server, &mut client, true);
        assert!(client.tls_info().unwrap().resumed());
        assert_eq!(client.zero_rtt_state(), zero_rtt);
    }

    #[test]
    fn zero_rtt() {
        zero_rtt_with_settings(http3params(DEFAULT_SETTINGS), ZeroRttState::AcceptedClient);
    }

    /// A larger QPACK decoder table size isn't an impediment to 0-RTT.
    #[test]
    fn zero_rtt_larger_decoder_table() {
        zero_rtt_with_settings(
            http3params(QpackSettings {
                max_table_size_decoder: DEFAULT_SETTINGS.max_table_size_decoder + 1,
                ..DEFAULT_SETTINGS
            }),
            ZeroRttState::AcceptedClient,
        );
    }

    /// A smaller QPACK decoder table size prevents 0-RTT.
    #[test]
    fn zero_rtt_smaller_decoder_table() {
        zero_rtt_with_settings(
            http3params(QpackSettings {
                max_table_size_decoder: DEFAULT_SETTINGS.max_table_size_decoder - 1,
                ..DEFAULT_SETTINGS
            }),
            ZeroRttState::Rejected,
        );
    }

    /// More blocked streams does not prevent 0-RTT.
    #[test]
    fn zero_rtt_more_blocked_streams() {
        zero_rtt_with_settings(
            http3params(QpackSettings {
                max_blocked_streams: DEFAULT_SETTINGS.max_blocked_streams + 1,
                ..DEFAULT_SETTINGS
            }),
            ZeroRttState::AcceptedClient,
        );
    }

    /// A lower number of blocked streams also prevents 0-RTT.
    #[test]
    fn zero_rtt_fewer_blocked_streams() {
        zero_rtt_with_settings(
            http3params(QpackSettings {
                max_blocked_streams: DEFAULT_SETTINGS.max_blocked_streams - 1,
                ..DEFAULT_SETTINGS
            }),
            ZeroRttState::Rejected,
        );
    }

    /// The size of the encoder table is local and therefore doesn't prevent 0-RTT.
    #[test]
    fn zero_rtt_smaller_encoder_table() {
        zero_rtt_with_settings(
            http3params(QpackSettings {
                max_table_size_encoder: DEFAULT_SETTINGS.max_table_size_encoder - 1,
                ..DEFAULT_SETTINGS
            }),
            ZeroRttState::AcceptedClient,
        );
    }

    #[test]
    fn client_request_hash() {
        let (mut hconn, mut peer_conn) = connect();

        let request_stream_id_1 = peer_conn.stream_create(StreamType::BiDi).unwrap();
        // Send only request headers for now.
        peer_conn
            .stream_send(request_stream_id_1, REQUEST_WITH_BODY)
            .unwrap();

        let request_stream_id_2 = peer_conn.stream_create(StreamType::BiDi).unwrap();
        // Send only request headers for now.
        peer_conn
            .stream_send(request_stream_id_2, REQUEST_WITH_BODY)
            .unwrap();

        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        let mut requests = HashMap::new();
        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers { stream, .. } => {
                    assert!(requests.get(&stream).is_none());
                    requests.insert(stream, 0);
                }
                Http3ServerEvent::Data { stream, .. } => {
                    assert!(requests.get(&stream).is_some());
                }
                Http3ServerEvent::DataWritable { .. }
                | Http3ServerEvent::StreamReset { .. }
                | Http3ServerEvent::StreamStopSending { .. }
                | Http3ServerEvent::StateChange { .. }
                | Http3ServerEvent::PriorityUpdate { .. }
                | Http3ServerEvent::WebTransport(_) => {}
            }
        }
        assert_eq!(requests.len(), 2);
    }

    #[derive(Debug, Default)]
    pub struct RejectZeroRtt {}
    impl ZeroRttChecker for RejectZeroRtt {
        fn check(&self, _token: &[u8]) -> ZeroRttCheckResult {
            ZeroRttCheckResult::Reject
        }
    }

    #[test]
    fn reject_zero_server() {
        fixture_init();
        let mut server = Http3Server::new(
            now(),
            DEFAULT_KEYS,
            DEFAULT_ALPN,
            anti_replay(),
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            http3params(DEFAULT_SETTINGS),
            Some(Box::new(RejectZeroRtt::default())),
        )
        .expect("create a server");
        let mut client = connect_to(&mut server);
        let token = client.events().find_map(|e| {
            if let ConnectionEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        });
        assert!(token.is_some());

        let mut client = default_client();
        client.enable_resumption(now(), token.unwrap()).unwrap();

        connect_transport(&mut server, &mut client, true);
        assert!(client.tls_info().unwrap().resumed());
        assert_eq!(client.zero_rtt_state(), ZeroRttState::Rejected);
    }
}
