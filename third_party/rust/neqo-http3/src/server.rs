// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::Http3State;
use crate::connection_server::Http3ServerHandler;
use crate::server_connection_events::Http3ServerConnEvent;
use crate::server_events::{ClientRequestStream, Http3ServerEvent, Http3ServerEvents};
use crate::Res;
use neqo_common::{qtrace, Datagram};
use neqo_crypto::AntiReplay;
use neqo_transport::server::{ActiveConnectionRef, Server};
use neqo_transport::{ConnectionIdManager, Output};
use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;
use std::time::Instant;

type HandlerRef = Rc<RefCell<Http3ServerHandler>>;

pub struct Http3Server {
    server: Server,
    max_table_size: u32,
    max_blocked_streams: u16,
    http3_handlers: HashMap<ActiveConnectionRef, HandlerRef>,
    events: Http3ServerEvents,
}

impl ::std::fmt::Display for Http3Server {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 server ")
    }
}

impl Http3Server {
    pub fn new(
        now: Instant,
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: AntiReplay,
        cid_manager: Rc<RefCell<dyn ConnectionIdManager>>,
        max_table_size: u32,
        max_blocked_streams: u16,
    ) -> Res<Self> {
        Ok(Self {
            server: Server::new(now, certs, protocols, anti_replay, cid_manager)?,
            max_table_size,
            max_blocked_streams,
            http3_handlers: HashMap::new(),
            events: Http3ServerEvents::default(),
        })
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

    pub fn process_http3(&mut self, now: Instant) {
        qtrace!([self], "Process http3 internal.");
        let mut active_conns = self.server.active_connections();

        // We need to find connections that needs to be process on http3 level.
        let mut http3_active: Vec<ActiveConnectionRef> = self
            .http3_handlers
            .iter()
            .filter(|(conn, handler)| {
                handler.borrow_mut().should_be_processed() && !active_conns.contains(&conn)
            })
            .map(|(conn, _)| conn)
            .cloned()
            .collect();
        // For http_active connection we need to put them in neqo-transport's server
        // waiting queue.
        http3_active
            .iter()
            .for_each(|conn| self.server.add_to_waiting(conn.clone()));
        active_conns.append(&mut http3_active);
        active_conns.dedup();
        let max_table_size = self.max_table_size;
        let max_blocked_streams = self.max_blocked_streams;
        for mut conn in active_conns {
            let handler = self.http3_handlers.entry(conn.clone()).or_insert_with(|| {
                Rc::new(RefCell::new(Http3ServerHandler::new(
                    max_table_size,
                    max_blocked_streams,
                )))
            });

            handler
                .borrow_mut()
                .process_http3(&mut conn.borrow_mut(), now);
            let mut remove = false;
            while let Some(e) = handler.borrow_mut().next_event() {
                match e {
                    Http3ServerConnEvent::Headers {
                        stream_id,
                        headers,
                        fin,
                    } => self.events.headers(
                        ClientRequestStream::new(conn.clone(), handler.clone(), stream_id),
                        headers,
                        fin,
                    ),
                    Http3ServerConnEvent::Data {
                        stream_id,
                        data,
                        fin,
                    } => self.events.data(
                        ClientRequestStream::new(conn.clone(), handler.clone(), stream_id),
                        data,
                        fin,
                    ),
                    Http3ServerConnEvent::StateChange(state) => {
                        self.events
                            .connection_state_change(conn.clone(), state.clone());
                        if let Http3State::Closed { .. } = state {
                            remove = true;
                        }
                    }
                    _ => {}
                }
            }
            if remove {
                self.http3_handlers.remove(&conn.clone());
            }
        }
    }

    /// Get all current events. Best used just in debug/testing code, use
    /// next_event() instead.
    pub fn events(&mut self) -> impl Iterator<Item = Http3ServerEvent> {
        self.events.events()
    }

    /// Return true if there are outstanding events.
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::Error;
    use neqo_common::matches;
    use neqo_crypto::AuthenticationStatus;
    use neqo_qpack::encoder::QPackEncoder;
    use neqo_transport::{
        CloseError, Connection, ConnectionEvent, FixedConnectionIdManager, State, StreamType,
    };
    use test_fixture::*;

    /// Create a http3 server with default configuration.
    pub fn default_http3_server() -> Http3Server {
        fixture_init();
        Http3Server::new(
            now(),
            DEFAULT_KEYS,
            DEFAULT_ALPN,
            anti_replay(),
            Rc::new(RefCell::new(FixedConnectionIdManager::new(5))),
            100,
            100,
        )
        .expect("create a default server")
    }

    fn assert_closed(hconn: &mut Http3Server, expected: Error) {
        let err = CloseError::Application(expected.code());
        let closed = |e| {
            matches!(e,
            Http3ServerEvent::StateChange{ state: Http3State::Closing(e), .. }
            | Http3ServerEvent::StateChange{ state: Http3State::Closed(e), .. }
              if e == err)
        };
        assert!(hconn.events().any(closed));
    }

    fn assert_connected(hconn: &mut Http3Server) {
        let connected =
            |e| matches!(e, Http3ServerEvent::StateChange{ state: Http3State::Connected, ..} );
        assert!(hconn.events().any(connected));
    }

    fn assert_not_closed(hconn: &mut Http3Server) {
        let closed = |e| {
            matches!(e,
            Http3ServerEvent::StateChange{ state: Http3State::Closing(..), .. })
        };
        assert!(!hconn.events().any(closed));
    }

    // Start a client/server and check setting frame.
    #[allow(clippy::cognitive_complexity)]
    fn connect_and_receive_settings() -> (Http3Server, Connection) {
        // Create a server and connect it to a client.
        // We will have a http3 server on one side and a neqo_transport
        // connection on the other side so that we can check what the http3
        // side sends and also to simulate an incorrectly behaving http3
        // client.

        fixture_init();
        let mut hconn = default_http3_server();
        let mut neqo_trans_conn = default_client();

        let out = neqo_trans_conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        let out = neqo_trans_conn.process(out.dgram(), now());
        let _ = hconn.process(out.dgram(), now());
        let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
        assert!(neqo_trans_conn.events().any(authentication_needed));
        neqo_trans_conn.authenticated(AuthenticationStatus::Ok, now());
        let out = neqo_trans_conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        assert_connected(&mut hconn);
        neqo_trans_conn.process(out.dgram(), now());

        let mut connected = false;
        while let Some(e) = neqo_trans_conn.next_event() {
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => {
                    assert!((stream_id == 3) || (stream_id == 7) || (stream_id == 11));
                    assert_eq!(stream_type, StreamType::UniDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    if stream_id == 2 || stream_id == 3 {
                        // the control stream
                        let mut buf = [0u8; 100];
                        let (amount, fin) =
                            neqo_trans_conn.stream_recv(stream_id, &mut buf).unwrap();
                        assert_eq!(fin, false);
                        const CONTROL_STREAM_DATA: &[u8] =
                            &[0x0, 0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64];
                        assert_eq!(amount, CONTROL_STREAM_DATA.len());
                        assert_eq!(&buf[..9], CONTROL_STREAM_DATA);
                    } else if stream_id == 6 || stream_id == 7 {
                        let mut buf = [0u8; 100];
                        let (amount, fin) =
                            neqo_trans_conn.stream_recv(stream_id, &mut buf).unwrap();
                        assert_eq!(fin, false);
                        assert_eq!(amount, 1);
                        assert_eq!(buf[..1], [0x2]);
                    } else if stream_id == 10 || stream_id == 11 {
                        let mut buf = [0u8; 100];
                        let (amount, fin) =
                            neqo_trans_conn.stream_recv(stream_id, &mut buf).unwrap();
                        assert_eq!(fin, false);
                        assert_eq!(amount, 1);
                        assert_eq!(buf[..1], [0x3]);
                    } else {
                        panic!("unexpected event");
                    }
                }
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    assert!((stream_id == 2) || (stream_id == 6) || (stream_id == 10));
                }
                ConnectionEvent::StateChange(State::Connected) => connected = true,
                ConnectionEvent::StateChange(_) => (),
                _ => panic!("unexpected event"),
            }
        }
        assert!(connected);
        (hconn, neqo_trans_conn)
    }

    // Test http3 connection inintialization.
    // The server will open the control and qpack streams and send SETTINGS frame.
    #[test]
    fn test_server_connect() {
        let _ = connect_and_receive_settings();
    }

    struct PeerConnection {
        conn: Connection,
        control_stream_id: u64,
    }

    // Connect transport, send and receive settings.
    fn connect() -> (Http3Server, PeerConnection) {
        let (mut hconn, mut neqo_trans_conn) = connect_and_receive_settings();
        let control_stream = neqo_trans_conn.stream_create(StreamType::UniDi).unwrap();
        let mut sent = neqo_trans_conn.stream_send(
            control_stream,
            &[0x0, 0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64],
        );
        assert_eq!(sent, Ok(9));
        let mut encoder = QPackEncoder::new(true);
        encoder.add_send_stream(neqo_trans_conn.stream_create(StreamType::UniDi).unwrap());
        encoder.send(&mut neqo_trans_conn).unwrap();
        let decoder_stream = neqo_trans_conn.stream_create(StreamType::UniDi).unwrap();
        sent = neqo_trans_conn.stream_send(decoder_stream, &[0x3]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // assert no error occured.
        assert_not_closed(&mut hconn);
        (
            hconn,
            PeerConnection {
                conn: neqo_trans_conn,
                control_stream_id: control_stream,
            },
        )
    }

    // Server: Test receiving a new control stream and a SETTINGS frame.
    #[test]
    fn test_server_receive_control_frame() {
        let _ = connect();
    }

    // Server: Test that the connection will be closed if control stream
    // has been closed.
    #[test]
    fn test_server_close_control_stream() {
        let (mut hconn, mut peer_conn) = connect();
        peer_conn
            .conn
            .stream_close_send(peer_conn.control_stream_id)
            .unwrap();
        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, Error::HttpClosedCriticalStream);
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
        assert_closed(&mut hconn, Error::HttpMissingSettings);
    }

    // Server: receiving SETTINGS frame twice causes connection close
    // with error HTTP_UNEXPECTED_FRAME.
    #[test]
    fn test_server_receive_settings_twice() {
        let (mut hconn, mut peer_conn) = connect();
        // send the second SETTINGS frame.
        let sent = peer_conn.conn.stream_send(
            peer_conn.control_stream_id,
            &[0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64],
        );
        assert_eq!(sent, Ok(8));
        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, Error::HttpFrameUnexpected);
    }

    fn test_wrong_frame_on_control_stream(v: &[u8]) {
        let (mut hconn, mut peer_conn) = connect();

        // receive a frame that is not allowed on the control stream.
        let _ = peer_conn.conn.stream_send(peer_conn.control_stream_id, v);

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&mut hconn, Error::HttpFrameUnexpected);
    }

    // send DATA frame on a cortrol stream
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

    // send DUPLICATE_PUSH frame on a cortrol stream
    #[test]
    fn test_server_duplicate_push_frame_on_control_stream() {
        test_wrong_frame_on_control_stream(&[0xe, 0x2, 0x1, 0x2]);
    }

    // Server: receive unkonwn stream type
    // also test getting stream id that does not fit into a single byte.
    #[test]
    fn test_server_received_unknown_stream() {
        let (mut hconn, mut peer_conn) = connect();

        // create a stream with unknown type.
        let new_stream_id = peer_conn.conn.stream_create(StreamType::UniDi).unwrap();
        let _ = peer_conn
            .conn
            .stream_send(new_stream_id, &[0x41, 0x19, 0x4, 0x4, 0x6, 0x0, 0x8, 0x0]);
        let out = peer_conn.conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        peer_conn.conn.process(out.dgram(), now());
        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // check for stop-sending with Error::HttpStreamCreationError.
        let mut stop_sending_event_found = false;
        while let Some(e) = peer_conn.conn.next_event() {
            if let ConnectionEvent::SendStreamStopSending {
                stream_id,
                app_error,
            } = e
            {
                stop_sending_event_found = true;
                assert_eq!(stream_id, new_stream_id);
                assert_eq!(app_error, Error::HttpStreamCreationError.code());
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
        let push_stream_id = peer_conn.conn.stream_create(StreamType::UniDi).unwrap();
        let _ = peer_conn.conn.stream_send(push_stream_id, &[0x1]);
        let out = peer_conn.conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        peer_conn.conn.process(out.dgram(), now());
        assert_closed(&mut hconn, Error::HttpStreamCreationError);
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
        assert_closed(&mut hconn, Error::HttpFrameUnexpected);
    }

    // Test reading of a slowly streamed frame. bytes are received one by one
    fn test_incomplet_frame(res: &[u8]) {
        let (mut hconn, mut peer_conn) = connect_and_receive_settings();

        // send an incomplete reequest.
        let stream_id = peer_conn.stream_create(StreamType::BiDi).unwrap();
        peer_conn.stream_send(stream_id, res).unwrap();
        peer_conn.stream_close_send(stream_id).unwrap();

        let out = peer_conn.process(None, now());
        hconn.process(out.dgram(), now());

        assert_closed(&mut hconn, Error::HttpFrameError);
    }

    const REQUEST_WITH_BODY: &[u8] = &[
        // headers
        0x01, 0x10, 0x00, 0x00, 0xd1, 0xd7, 0x50, 0x89, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x2e,
        0x43, 0xd3, 0xc1, // the first data frame.
        0x0, 0x3, 0x61, 0x62, 0x63, // the second data frame.
        0x0, 0x3, 0x64, 0x65, 0x66,
    ];

    // Incomplete DATA frame
    #[test]
    fn test_server_incomplet_data_frame() {
        test_incomplet_frame(&REQUEST_WITH_BODY[..22]);
    }

    // Incomplete HEADERS frame
    #[test]
    fn test_server_incomplet_headers_frame() {
        test_incomplet_frame(&REQUEST_WITH_BODY[..10]);
    }

    #[test]
    fn test_server_incomplet_unknown_frame() {
        test_incomplet_frame(&[0x21]);
    }

    #[test]
    fn test_server_request_with_body() {
        let (mut hconn, mut peer_conn) = connect();

        let stream_id = peer_conn.conn.stream_create(StreamType::BiDi).unwrap();
        peer_conn
            .conn
            .stream_send(stream_id, REQUEST_WITH_BODY)
            .unwrap();
        peer_conn.conn.stream_close_send(stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Check connection event. There should be 1 Header and 2 data events.
        let mut headers_frames = 0;
        let mut data_frames = 0;
        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers { headers, fin, .. } => {
                    assert_eq!(
                        headers,
                        vec![
                            (String::from(":method"), String::from("GET")),
                            (String::from(":scheme"), String::from("https")),
                            (String::from(":authority"), String::from("something.com")),
                            (String::from(":path"), String::from("/"))
                        ]
                    );
                    assert_eq!(fin, false);
                    headers_frames += 1;
                }
                Http3ServerEvent::Data {
                    mut request,
                    data,
                    fin,
                } => {
                    if data_frames == 0 {
                        assert_eq!(data, &REQUEST_WITH_BODY[20..23]);
                    } else {
                        assert_eq!(data, &REQUEST_WITH_BODY[25..]);
                        assert_eq!(fin, true);
                        request
                            .set_response(
                                &[
                                    (String::from(":status"), String::from("200")),
                                    (String::from("content-length"), String::from("3")),
                                ],
                                vec![0x67, 0x68, 0x69],
                            )
                            .unwrap();
                    }
                    data_frames += 1;
                }
                _ => {}
            }
        }
        assert_eq!(headers_frames, 1);
        assert_eq!(data_frames, 2);
    }

    #[test]
    fn test_server_request_with_body_send_stop_sending() {
        let (mut hconn, mut peer_conn) = connect();

        let stream_id = peer_conn.conn.stream_create(StreamType::BiDi).unwrap();
        // Send only request headers for now.
        peer_conn
            .conn
            .stream_send(stream_id, &REQUEST_WITH_BODY[..20])
            .unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Check connection event. There should be 1 Header and no data events.
        let mut headers_frames = 0;
        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers {
                    mut request,
                    headers,
                    fin,
                } => {
                    assert_eq!(
                        headers,
                        vec![
                            (String::from(":method"), String::from("GET")),
                            (String::from(":scheme"), String::from("https")),
                            (String::from(":authority"), String::from("something.com")),
                            (String::from(":path"), String::from("/"))
                        ]
                    );
                    assert_eq!(fin, false);
                    headers_frames += 1;
                    request
                        .stream_stop_sending(Error::HttpEarlyResponse.code())
                        .unwrap();
                    request
                        .set_response(
                            &[
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3")),
                            ],
                            vec![0x67, 0x68, 0x69],
                        )
                        .unwrap();
                }
                Http3ServerEvent::Data { .. } => {
                    panic!("We should not have a Data event");
                }
                _ => {}
            }
        }
        let out = hconn.process(None, now());

        // Send data.
        peer_conn
            .conn
            .stream_send(stream_id, &REQUEST_WITH_BODY[20..])
            .unwrap();
        peer_conn.conn.stream_close_send(stream_id).unwrap();

        let out = peer_conn.conn.process(out.dgram(), now());
        hconn.process(out.dgram(), now());

        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers { .. } => {
                    panic!("We should not have a Data event");
                }
                Http3ServerEvent::Data { .. } => {
                    panic!("We should not have a Data event");
                }
                _ => {}
            }
        }
        assert_eq!(headers_frames, 1);
    }

    #[test]
    fn test_server_request_with_body_server_reset() {
        let (mut hconn, mut peer_conn) = connect();

        let request_stream_id = peer_conn.conn.stream_create(StreamType::BiDi).unwrap();
        // Send only request headers for now.
        peer_conn
            .conn
            .stream_send(request_stream_id, &REQUEST_WITH_BODY[..20])
            .unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Check connection event. There should be 1 Header and no data events.
        // The server will reset the stream.
        let mut headers_frames = 0;
        while let Some(event) = hconn.next_event() {
            match event {
                Http3ServerEvent::Headers {
                    mut request,
                    headers,
                    fin,
                } => {
                    assert_eq!(
                        headers,
                        vec![
                            (String::from(":method"), String::from("GET")),
                            (String::from(":scheme"), String::from("https")),
                            (String::from(":authority"), String::from("something.com")),
                            (String::from(":path"), String::from("/"))
                        ]
                    );
                    assert_eq!(fin, false);
                    headers_frames += 1;
                    request
                        .stream_reset(Error::HttpRequestRejected.code())
                        .unwrap();
                }
                Http3ServerEvent::Data { .. } => {
                    panic!("We should not have a Data event");
                }
                _ => {}
            }
        }
        let out = hconn.process(None, now());

        let out = peer_conn.conn.process(out.dgram(), now());
        hconn.process(out.dgram(), now());

        // Check that STOP_SENDING and REET has been received.
        let mut reset = 0;
        let mut stop_sending = 0;
        while let Some(event) = peer_conn.conn.next_event() {
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
}
