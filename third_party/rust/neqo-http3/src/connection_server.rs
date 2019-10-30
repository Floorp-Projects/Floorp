// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::{Http3Connection, Http3ServerHandler, Http3State};
use crate::server_events::Http3ServerEvents;
use crate::transaction_server::{RequestHandler, TransactionServer};
use neqo_common::{qdebug, qtrace, Datagram};
use neqo_crypto::AntiReplay;
use neqo_transport::{AppError, Connection, ConnectionIdManager, Output, Role};
use std::cell::RefCell;
use std::rc::Rc;
use std::time::Instant;

use crate::{Error, Res};

pub struct Http3Server {
    base_handler: Http3Connection<Http3ServerEvents, TransactionServer, Http3ServerHandler>,
    handler: Option<RequestHandler>,
}

impl ::std::fmt::Display for Http3Server {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 connection {:?}", self.role())
    }
}

impl Http3Server {
    pub fn new(
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: &AntiReplay,
        cid_manager: Rc<RefCell<dyn ConnectionIdManager>>,
        max_table_size: u32,
        max_blocked_streams: u16,
        handler: Option<RequestHandler>,
    ) -> Res<Http3Server> {
        Ok(Http3Server {
            base_handler: Http3Connection::new_server(
                certs,
                protocols,
                anti_replay,
                cid_manager,
                max_table_size,
                max_blocked_streams,
            )?,
            handler,
        })
    }

    fn role(&self) -> Role {
        self.base_handler.role()
    }

    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        qtrace!([self] "Process.");
        self.base_handler.process(dgram, now)
    }

    pub fn process_input(&mut self, dgram: Datagram, now: Instant) {
        qtrace!([self] "Process input.");
        self.base_handler.process_input(dgram, now);
    }

    pub fn process_timer(&mut self, now: Instant) {
        qtrace!([self] "Process timer.");
        self.base_handler.process_timer(now);
    }

    pub fn conn(&mut self) -> &mut Connection {
        &mut self.base_handler.conn
    }

    pub fn process_http3(&mut self, now: Instant) {
        qtrace!([self] "Process http3 internal.");
        self.base_handler.process_http3(now);
        self.handle_done_reading_request();
    }

    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!([self] "Process output.");
        self.base_handler.process_output(now)
    }

    pub fn close(&mut self, now: Instant, error: AppError, msg: &str) {
        qdebug!([self] "Closed.");
        self.base_handler.close(now, error, msg);
    }

    pub fn state(&self) -> Http3State {
        self.base_handler.state.clone()
    }

    // TODO: this is a work arround until a new server API is implementted.
    fn handle_done_reading_request(&mut self) {
        let mut to_remove: Vec<u64> = Vec::new();
        for (stream_id, t) in self.base_handler.transactions.iter_mut() {
            if t.done_reading_request() {
                if let Some(ref mut cb) = self.handler {
                    let (headers, data, close_error) = (cb)(t.get_request_headers(), false);
                    qdebug!(
                        "Sending response: {:?} {:?} {:?}",
                        headers,
                        data,
                        close_error
                    );
                    match close_error {
                        Some(e) => {
                            let _ = self
                                .base_handler
                                .conn
                                .stream_stop_sending(*stream_id, e.code());
                            if e != Error::HttpEarlyResponse {
                                to_remove.push(*stream_id);
                                let _ = self
                                    .base_handler
                                    .conn
                                    .stream_reset_send(*stream_id, e.code());
                            } else {
                                t.set_response(
                                    &headers,
                                    data,
                                    &mut self.base_handler.qpack_encoder,
                                );
                            }
                        }
                        None => {
                            t.set_response(&headers, data, &mut self.base_handler.qpack_encoder)
                        }
                    };
                }
            }
        }
        for stream_id in to_remove.iter() {
            self.base_handler.transactions.remove(&stream_id);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use neqo_common::matches;
    use neqo_crypto::AuthenticationStatus;
    use neqo_qpack::encoder::QPackEncoder;
    use neqo_transport::{
        CloseError, ConnectionEvent, FixedConnectionIdManager, State, StreamType,
    };
    use test_fixture::*;

    /// Create a http3 server with default configuration.
    pub fn default_http3_server() -> Http3Server {
        fixture_init();
        Http3Server::new(
            DEFAULT_KEYS,
            DEFAULT_ALPN,
            &anti_replay(),
            Rc::new(RefCell::new(FixedConnectionIdManager::new(5))),
            100,
            100,
            None,
        )
        .expect("create a default server")
    }

    fn assert_closed(hconn: &Http3Server, expected: Error) {
        match hconn.state() {
            Http3State::Closing(err) | Http3State::Closed(err) => {
                assert_eq!(err, CloseError::Application(expected.code()))
            }
            _ => panic!("Wrong state {:?}", hconn.state()),
        };
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

        assert_eq!(hconn.state(), Http3State::Initializing);
        let out = neqo_trans_conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        let out = neqo_trans_conn.process(out.dgram(), now());
        let _ = hconn.process(out.dgram(), now());
        let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
        assert!(neqo_trans_conn.events().any(authentication_needed));
        neqo_trans_conn.authenticated(AuthenticationStatus::Ok, now());
        let out = neqo_trans_conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        assert_eq!(hconn.state(), Http3State::Connected);
        neqo_trans_conn.process(out.dgram(), now());

        let events = neqo_trans_conn.events();
        let mut connected = false;
        for e in events {
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
        assert_eq!(hconn.state(), Http3State::Connected);
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
        assert_closed(&hconn, Error::HttpClosedCriticalStream);
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
        assert_closed(&hconn, Error::HttpMissingSettings);
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
        assert_closed(&hconn, Error::HttpFrameUnexpected);
    }

    fn test_wrong_frame_on_control_stream(v: &[u8]) {
        let (mut hconn, mut peer_conn) = connect();

        // receive a frame that is not allowed on the control stream.
        let _ = peer_conn.conn.stream_send(peer_conn.control_stream_id, v);

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        assert_closed(&hconn, Error::HttpFrameUnexpected);
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

    // send DUPLICATE_PUSH frame on a cortrol stream
    #[test]
    fn test_duplicate_push_frame_on_control_stream() {
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

        // check for stop-sending with Error::HttpStreamCreationError.
        let events = peer_conn.conn.events();
        let mut stop_sending_event_found = false;
        for e in events {
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
        assert_eq!(hconn.state(), Http3State::Connected);
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
        assert_closed(&hconn, Error::HttpStreamCreationError);
    }
}
