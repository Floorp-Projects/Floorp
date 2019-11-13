// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::client_events::{Http3ClientEvent, Http3ClientEvents};
use crate::connection::{Http3ClientHandler, Http3Connection, Http3State, Http3Transaction};

use crate::transaction_client::TransactionClient;
use crate::Header;
use neqo_common::{qinfo, qtrace, Datagram};
use neqo_crypto::{agent::CertificateInfo, AuthenticationStatus, SecretAgentInfo};
use neqo_transport::{AppError, Connection, ConnectionIdManager, Output, Role, StreamType};
use std::cell::RefCell;
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::Instant;

use crate::{Error, Res};

pub struct Http3Client {
    base_handler: Http3Connection<Http3ClientEvents, TransactionClient, Http3ClientHandler>,
}

impl ::std::fmt::Display for Http3Client {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 connection {:?}", self.role())
    }
}

impl Http3Client {
    pub fn new(
        server_name: &str,
        protocols: &[impl AsRef<str>],
        cid_manager: Rc<RefCell<dyn ConnectionIdManager>>,
        local_addr: SocketAddr,
        remote_addr: SocketAddr,
        max_table_size: u32,
        max_blocked_streams: u16,
    ) -> Res<Http3Client> {
        Ok(Http3Client {
            base_handler: Http3Connection::new_client(
                server_name,
                protocols,
                cid_manager,
                local_addr,
                remote_addr,
                max_table_size,
                max_blocked_streams,
            )?,
        })
    }

    pub fn new_with_conn(
        c: Connection,
        max_table_size: u32,
        max_blocked_streams: u16,
    ) -> Res<Http3Client> {
        Ok(Http3Client {
            base_handler: Http3Connection::new_client_with_conn(
                c,
                max_table_size,
                max_blocked_streams,
            )?,
        })
    }

    pub fn role(&self) -> Role {
        self.base_handler.role()
    }

    pub fn state(&self) -> Http3State {
        self.base_handler.state()
    }

    pub fn tls_info(&self) -> Option<&SecretAgentInfo> {
        self.base_handler.conn.tls_info()
    }

    /// Get the peer's certificate.
    pub fn peer_certificate(&self) -> Option<CertificateInfo> {
        self.base_handler.conn.peer_certificate()
    }

    pub fn authenticated(&mut self, status: AuthenticationStatus, now: Instant) {
        self.base_handler.conn.authenticated(status, now);
    }

    pub fn close(&mut self, now: Instant, error: AppError, msg: &str) {
        qinfo!([self], "Close the connection error={} msg={}.", error, msg);
        self.base_handler.close(now, error, msg);
    }

    pub fn fetch(
        &mut self,
        method: &str,
        scheme: &str,
        host: &str,
        path: &str,
        headers: &[Header],
    ) -> Res<u64> {
        qinfo!(
            [self],
            "Fetch method={}, scheme={}, host={}, path={}",
            method,
            scheme,
            host,
            path
        );
        let id = self.base_handler.conn().stream_create(StreamType::BiDi)?;
        self.base_handler.add_transaction(
            id,
            TransactionClient::new(
                id,
                method,
                scheme,
                host,
                path,
                headers,
                self.base_handler.events.clone(),
            ),
        );
        Ok(id)
    }

    pub fn stream_reset(&mut self, stream_id: u64, error: AppError) -> Res<()> {
        qinfo!([self], "reset_stream {} error={}.", stream_id, error);
        self.base_handler.stream_reset(stream_id, error)
    }

    pub fn stream_close_send(&mut self, stream_id: u64) -> Res<()> {
        qinfo!([self], "Close senidng side stream={}.", stream_id);
        self.base_handler.stream_close_send(stream_id)
    }

    pub fn send_request_body(&mut self, stream_id: u64, buf: &[u8]) -> Res<usize> {
        qinfo!(
            [self],
            "send_request_body from stream {} sending {} bytes.",
            stream_id,
            buf.len()
        );
        self.base_handler
            .transactions
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?
            .send_request_body(&mut self.base_handler.conn, buf)
    }

    pub fn read_response_headers(&mut self, stream_id: u64) -> Res<(Vec<Header>, bool)> {
        qinfo!([self], "read_response_headers from stream {}.", stream_id);
        let transaction = self
            .base_handler
            .transactions
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?;
        match transaction.read_response_headers() {
            Ok((headers, fin)) => {
                if transaction.done() {
                    self.base_handler.transactions.remove(&stream_id);
                }
                Ok((headers, fin))
            }
            Err(e) => Err(e),
        }
    }

    pub fn read_response_data(
        &mut self,
        now: Instant,
        stream_id: u64,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        qinfo!([self], "read_data from stream {}.", stream_id);
        let transaction = self
            .base_handler
            .transactions
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?;

        match transaction.read_response_data(&mut self.base_handler.conn, buf) {
            Ok((amount, fin)) => {
                if fin {
                    self.base_handler.transactions.remove(&stream_id);
                } else if amount > 0 {
                    // Directly call receive instead of adding to
                    // streams_are_readable here. This allows the app to
                    // pick up subsequent already-received data frames in
                    // the stream even if no new packets arrive to cause
                    // process_http3() to run.
                    transaction.receive(
                        &mut self.base_handler.conn,
                        &mut self.base_handler.qpack_decoder,
                    )?;
                }
                Ok((amount, fin))
            }
            Err(e) => {
                if e == Error::HttpFrameError {
                    self.close(now, e.code(), "");
                }
                Err(e)
            }
        }
    }

    /// Get all current events. Best used just in debug/testing code, use
    /// next_event() instead.
    pub fn events(&mut self) -> impl Iterator<Item = Http3ClientEvent> {
        self.base_handler.events.events()
    }

    /// Return true if there are outstanding events.
    pub fn has_events(&self) -> bool {
        self.base_handler.events.has_events()
    }

    /// Get events that indicate state changes on the connection. This method
    /// correctly handles cases where handling one event can obsolete
    /// previously-queued events, or cause new events to be generated.
    pub fn next_event(&mut self) -> Option<Http3ClientEvent> {
        self.base_handler.events.next_event()
    }

    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        qtrace!([self], "Process.");
        self.base_handler.process(dgram, now)
    }

    pub fn process_input(&mut self, dgram: Datagram, now: Instant) {
        qtrace!([self], "Process input.");
        self.base_handler.process_input(dgram, now);
    }

    pub fn process_timer(&mut self, now: Instant) {
        qtrace!([self], "Process timer.");
        self.base_handler.process_timer(now);
    }

    pub fn conn(&mut self) -> &mut Connection {
        &mut self.base_handler.conn
    }

    pub fn process_http3(&mut self, now: Instant) {
        qtrace!([self], "Process http3 internal.");

        self.base_handler.process_http3(now);
    }

    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!([self], "Process output.");
        self.base_handler.process_output(now)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::hframe::HFrame;
    use neqo_common::{matches, Encoder};
    use neqo_qpack::encoder::QPackEncoder;
    use neqo_transport::{CloseError, ConnectionEvent, FixedConnectionIdManager, State};
    use test_fixture::*;

    fn assert_closed(hconn: &Http3Client, expected: Error) {
        match hconn.state() {
            Http3State::Closing(err) | Http3State::Closed(err) => {
                assert_eq!(err, CloseError::Application(expected.code()))
            }
            _ => panic!("Wrong state {:?}", hconn.state()),
        };
    }

    /// Create a http3 client with default configuration.
    pub fn default_http3_client() -> Http3Client {
        Http3Client::new(
            DEFAULT_SERVER_NAME,
            DEFAULT_ALPN,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(3))),
            loopback(),
            loopback(),
            100,
            100,
        )
        .expect("create a default client")
    }

    // Start a client/server and check setting frame.
    #[allow(clippy::cognitive_complexity)]
    fn connect_and_receive_settings() -> (Http3Client, Connection) {
        // Create a client and connect it to a server.
        // We will have a http3 client on one side and a neqo_transport
        // connection on the other side so that we can check what the http3
        // side sends and also to simulate an incorrectly behaving http3
        // server.

        fixture_init();
        let mut hconn = default_http3_client();
        let mut neqo_trans_conn = default_server();

        assert_eq!(hconn.state(), Http3State::Initializing);
        let out = hconn.process(None, now());
        assert_eq!(hconn.state(), Http3State::Initializing);
        assert_eq!(*neqo_trans_conn.state(), State::WaitInitial);
        let out = neqo_trans_conn.process(out.dgram(), now());
        assert_eq!(*neqo_trans_conn.state(), State::Handshaking);
        let out = hconn.process(out.dgram(), now());
        let out = neqo_trans_conn.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());

        let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
        assert!(hconn.events().any(authentication_needed));
        hconn.authenticated(AuthenticationStatus::Ok, now());

        let out = hconn.process(out.dgram(), now());
        let connected = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::Connected));
        assert!(hconn.events().any(connected));

        assert_eq!(hconn.state(), Http3State::Connected);
        neqo_trans_conn.process(out.dgram(), now());

        let mut connected = false;
        while let Some(e) = neqo_trans_conn.next_event() {
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => {
                    assert!((stream_id == 2) || (stream_id == 6) || (stream_id == 10));
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
    // The client will open the control and qpack streams and send SETTINGS frame.
    #[test]
    fn test_client_connect() {
        let _ = connect_and_receive_settings();
    }

    struct PeerConnection {
        conn: Connection,
        control_stream_id: u64,
        encoder: QPackEncoder,
    }

    // Connect transport, send and receive settings.
    fn connect() -> (Http3Client, PeerConnection) {
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
                encoder,
            },
        )
    }

    // Client: Test receiving a new control stream and a SETTINGS frame.
    #[test]
    fn test_client_receive_control_frame() {
        let _ = connect();
    }

    // Client: Test that the connection will be closed if control stream
    // has been closed.
    #[test]
    fn test_client_close_control_stream() {
        let (mut hconn, mut peer_conn) = connect();
        peer_conn
            .conn
            .stream_close_send(peer_conn.control_stream_id)
            .unwrap();
        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&hconn, Error::HttpClosedCriticalStream);
    }

    // Client: test missing SETTINGS frame
    // (the first frame sent is a garbage frame).
    #[test]
    fn test_client_missing_settings() {
        let (mut hconn, mut neqo_trans_conn) = connect_and_receive_settings();
        // Create server control stream.
        let control_stream = neqo_trans_conn.stream_create(StreamType::UniDi).unwrap();
        // Send a HEADERS frame instead (which contains garbage).
        let sent = neqo_trans_conn.stream_send(control_stream, &[0x0, 0x1, 0x3, 0x0, 0x1, 0x2]);
        assert_eq!(sent, Ok(6));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());
        assert_closed(&hconn, Error::HttpMissingSettings);
    }

    // Client: receiving SETTINGS frame twice causes connection close
    // with error HTTP_UNEXPECTED_FRAME.
    #[test]
    fn test_client_receive_settings_twice() {
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

    // Client: receive unkonwn stream type
    // This function also tests getting stream id that does not fit into a single byte.
    #[test]
    fn test_client_received_unknown_stream() {
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
        assert_eq!(hconn.state(), Http3State::Connected);
    }

    // Client: receive a push stream
    #[test]
    fn test_client_received_push_stream() {
        let (mut hconn, mut peer_conn) = connect();

        // create a push stream.
        let push_stream_id = peer_conn.conn.stream_create(StreamType::UniDi).unwrap();
        let _ = peer_conn.conn.stream_send(push_stream_id, &[0x1]);
        let out = peer_conn.conn.process(None, now());
        let out = hconn.process(out.dgram(), now());
        peer_conn.conn.process(out.dgram(), now());

        assert_closed(&hconn, Error::HttpIdError);
    }

    // Test wrong frame on req/rec stream
    fn test_wrong_frame_on_request_stream(v: &[u8]) {
        let (mut hconn, mut peer_conn) = connect();

        assert_eq!(
            hconn.fetch("GET", "https", "something.com", "/", &[]),
            Ok(0)
        );

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // find the new request/response stream and send frame v on it.
        while let Some(e) = peer_conn.conn.next_event() {
            if let ConnectionEvent::NewStream {
                stream_id,
                stream_type,
            } = e
            {
                assert_eq!(stream_type, StreamType::BiDi);
                let _ = peer_conn.conn.stream_send(stream_id, v);
            }
        }
        // Generate packet with the above bad h3 input
        let out = peer_conn.conn.process(None, now());
        // Process bad input and generate stop sending frame
        let out = hconn.process(out.dgram(), now());
        // Process stop sending frame and generate an event and a reset frame
        let _ = peer_conn.conn.process(out.dgram(), now());

        assert_closed(&hconn, Error::HttpFrameUnexpected);
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

    // Test reading of a slowly streamed frame. bytes are received one by one
    #[test]
    fn test_frame_reading() {
        let (mut hconn, mut neqo_trans_conn) = connect_and_receive_settings();

        // create a control stream.
        let control_stream = neqo_trans_conn.stream_create(StreamType::UniDi).unwrap();

        // send the stream type
        let mut sent = neqo_trans_conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // start sending SETTINGS frame
        sent = neqo_trans_conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = neqo_trans_conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = neqo_trans_conn.stream_send(control_stream, &[0x6]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = neqo_trans_conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = neqo_trans_conn.stream_send(control_stream, &[0x8]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = neqo_trans_conn.stream_send(control_stream, &[0x0]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        assert_eq!(hconn.state(), Http3State::Connected);

        // Now test PushPromise
        sent = neqo_trans_conn.stream_send(control_stream, &[0x5]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = neqo_trans_conn.stream_send(control_stream, &[0x5]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        sent = neqo_trans_conn.stream_send(control_stream, &[0x4]);
        assert_eq!(sent, Ok(1));
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        // PUSH_PROMISE on a control stream will cause an error
        assert_closed(&hconn, Error::HttpFrameUnexpected);
    }

    // We usually send the same request header. This function check if the request has been
    // receive properly by a peer.
    fn check_header_frame(peer_conn: &mut Connection, stream_id: u64, expected_fin: bool) {
        let mut buf = [0u8; 18];
        let (amount, fin) = peer_conn.stream_recv(stream_id, &mut buf).unwrap();
        const EXPECTED_REQUEST_HEADER_FRAME: &[u8] = &[
            0x01, 0x10, 0x00, 0x00, 0xd1, 0xd7, 0x50, 0x89, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53,
            0x2e, 0x43, 0xd3, 0xc1,
        ];
        assert_eq!(fin, expected_fin);
        assert_eq!(amount, EXPECTED_REQUEST_HEADER_FRAME.len());
        assert_eq!(&buf[..], EXPECTED_REQUEST_HEADER_FRAME);
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    fn fetch_basic() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);
        let _ = hconn.stream_close_send(request_stream_id);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // find the new request/response stream and send frame v on it.
        let events = peer_conn.conn.events().collect::<Vec<_>>();
        assert_eq!(events.len(), 6); // NewStream, RecvStreamReadable, SendStreamWritable x 4
        for e in events {
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(stream_type, StreamType::BiDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_header_frame(&mut peer_conn.conn, stream_id, true);

                    // send response - 200  Content-Length: 6
                    // with content: 'abcdef'.
                    // The content will be send in 2 DATA frames.
                    let _ = peer_conn.conn.stream_send(
                        stream_id,
                        &[
                            // headers
                            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
                            // the first data frame
                            0x0, 0x3, 0x61, 0x62, 0x63,
                            // the second data frame
                            // the first data frame
                            0x0, 0x3, 0x64, 0x65, 0x66,
                        ],
                    );
                    peer_conn.conn.stream_close_send(stream_id).unwrap();
                }
                _ => {}
            }
        }
        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        let http_events = hconn.events().collect::<Vec<_>>();
        assert_eq!(http_events.len(), 2);
        for e in http_events {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let h = hconn.read_response_headers(stream_id);
                    assert_eq!(
                        h,
                        Ok((
                            vec![
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3"))
                            ],
                            false
                        ))
                    );
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = hconn
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, false);
                    assert_eq!(amount, 3);
                    assert_eq!(buf[..3], [0x61, 0x62, 0x63]);
                }
                _ => {}
            }
        }

        hconn.process_http3(now());
        let http_events = hconn.events().collect::<Vec<_>>();
        assert_eq!(http_events.len(), 1);
        for e in http_events {
            match e {
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = hconn
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, true);
                    assert_eq!(amount, 3);
                    assert_eq!(buf[..3], [0x64, 0x65, 0x66]);
                }
                _ => panic!("unexpected event"),
            }
        }

        // after this stream will be removed from hcoon. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    // Helper function
    fn read_response(
        mut hconn: Http3Client,
        mut neqo_trans_conn: Connection,
        request_stream_id: u64,
    ) {
        let out = neqo_trans_conn.process(None, now());
        hconn.process(out.dgram(), now());

        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let h = hconn.read_response_headers(stream_id);
                    assert_eq!(
                        h,
                        Ok((
                            vec![
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3"))
                            ],
                            false
                        ))
                    );
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = hconn
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, true);
                    const EXPECTED_RESPONSE_BODY: &[u8] = &[0x61, 0x62, 0x63];
                    assert_eq!(amount, EXPECTED_RESPONSE_BODY.len());
                    assert_eq!(&buf[..3], EXPECTED_RESPONSE_BODY);
                }
                _ => {}
            }
        }

        // after this stream will be removed from hconn. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    // Send a request with the request body.
    #[test]
    fn fetch_with_data() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, &[0x64, 0x65, 0x66]);
        assert_eq!(sent, Ok(3));
        let _ = hconn.stream_close_send(request_stream_id);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // find the new request/response stream and send response on it.
        while let Some(e) = peer_conn.conn.next_event() {
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(stream_type, StreamType::BiDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_header_frame(&mut peer_conn.conn, stream_id, false);

                    // Read request body.
                    let mut buf = [0u8; 100];
                    let (amount, fin) = peer_conn.conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert_eq!(fin, true);
                    const EXPECTED_REQUEST_BODY: &[u8] = &[0x0, 0x3, 0x64, 0x65, 0x66];
                    assert_eq!(amount, EXPECTED_REQUEST_BODY.len());
                    assert_eq!(&buf[..5], EXPECTED_REQUEST_BODY);

                    // send response - 200  Content-Length: 3
                    // with content: 'abc'.
                    let _ = peer_conn.conn.stream_send(
                        stream_id,
                        &[
                            // headers
                            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
                            // a data frame
                            0x0, 0x3, 0x61, 0x62, 0x63,
                        ],
                    );
                    peer_conn.conn.stream_close_send(stream_id).unwrap();
                }
                _ => {}
            }
        }

        read_response(hconn, peer_conn.conn, request_stream_id);
    }

    // send a request with request body containing request_body. We expect to receive expected_data_frame_header.
    fn fetch_with_data_length_xbytes(request_body: &[u8], expected_data_frame_header: &[u8]) {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, request_body);
        assert_eq!(sent, Ok(request_body.len()));

        // Close stream.
        let _ = hconn.stream_close_send(request_stream_id);

        // We need to loop a bit until all data has been sent.
        let mut out = hconn.process(None, now());
        for _i in 0..20 {
            out = peer_conn.conn.process(out.dgram(), now());
            out = hconn.process(out.dgram(), now());
        }

        // find the new request/response stream, check received frames and send a response.
        while let Some(e) = peer_conn.conn.next_event() {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                if stream_id == request_stream_id {
                    // Read only the HEADER frame
                    check_header_frame(&mut peer_conn.conn, stream_id, false);

                    // Read the DATA frame.
                    let mut buf = [1u8; 0xffff];
                    let (amount, fin) = peer_conn.conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert_eq!(fin, true);
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
                    let _ = peer_conn.conn.stream_send(
                        stream_id,
                        &[
                            // headers
                            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
                            // a data frame
                            0x0, 0x3, 0x61, 0x62, 0x63,
                        ],
                    );
                    peer_conn.conn.stream_close_send(stream_id).unwrap();
                }
            }
        }

        read_response(hconn, peer_conn.conn, request_stream_id);
    }

    // send a request with 63 bytes. The DATA frame length field will still have 1 byte.
    #[test]
    fn fetch_with_data_length_63bytes() {
        fetch_with_data_length_xbytes(&[0u8; 63], &[0x0, 0x3f]);
    }

    // send a request with 64 bytes. The DATA frame length field will need 2 byte.
    #[test]
    fn fetch_with_data_length_64bytes() {
        fetch_with_data_length_xbytes(&[0u8; 64], &[0x0, 0x40, 0x40]);
    }

    // send a request with 16383 bytes. The DATA frame length field will still have 2 byte.
    #[test]
    fn fetch_with_data_length_16383bytes() {
        fetch_with_data_length_xbytes(&[0u8; 16383], &[0x0, 0x7f, 0xff]);
    }

    // send a request with 16384 bytes. The DATA frame length field will need 4 byte.
    #[test]
    fn fetch_with_data_length_16384bytes() {
        fetch_with_data_length_xbytes(&[0u8; 16384], &[0x0, 0x80, 0x0, 0x40, 0x0]);
    }

    // Send 2 data frames so that the second one cannot fit into the send_buf and it is only
    // partialy sent. We check that the sent data is correct.
    fn fetch_with_two_data_frames(
        first_frame: &[u8],
        expected_first_data_frame_header: &[u8],
        expected_second_data_frame_header: &[u8],
        expected_second_data_frame: &[u8],
    ) {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));

        // Send the first frame.
        let sent = hconn.send_request_body(request_stream_id, first_frame);
        assert_eq!(sent, Ok(first_frame.len()));

        // The second frame cannot fit.
        let sent = hconn.send_request_body(request_stream_id, &[0u8; 0xffff]);
        assert_eq!(sent, Ok(expected_second_data_frame.len()));

        // Close stream.
        let _ = hconn.stream_close_send(request_stream_id);

        let mut out = hconn.process(None, now());
        // We need to loop a bit until all data has been sent.
        for _i in 0..55 {
            out = peer_conn.conn.process(out.dgram(), now());
            out = hconn.process(out.dgram(), now());
        }

        // find the new request/response stream, check received frames and send a response.
        while let Some(e) = peer_conn.conn.next_event() {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                if stream_id == request_stream_id {
                    // Read only the HEADER frame
                    check_header_frame(&mut peer_conn.conn, stream_id, false);

                    // Read DATA frames.
                    let mut buf = [1u8; 0xffff];
                    let (amount, fin) = peer_conn.conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert_eq!(fin, true);
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
                    let start = end;
                    let end = end + expected_second_data_frame_header.len();
                    assert_eq!(&buf[start..end], expected_second_data_frame_header);

                    // Check the second frame data.
                    let start = end;
                    let end = end + expected_second_data_frame.len();
                    assert_eq!(&buf[start..end], expected_second_data_frame);

                    // send response - 200  Content-Length: 3
                    // with content: 'abc'.
                    let _ = peer_conn.conn.stream_send(
                        stream_id,
                        &[
                            // headers
                            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
                            // a data frame
                            0x0, 0x3, 0x61, 0x62, 0x63,
                        ],
                    );
                    peer_conn.conn.stream_close_send(stream_id).unwrap();
                }
            }
        }

        read_response(hconn, peer_conn.conn, request_stream_id);
    }

    // Send 2 frames. For the second one we can only send 63 bytes.
    // After the first frame there is exactly 63+2 bytes left in the send buffer.
    #[test]
    fn fetch_two_data_frame_second_63bytes() {
        fetch_with_two_data_frames(
            &[0u8; 65447],
            &[0x0, 0x80, 0x0, 0xff, 0x0a7],
            &[0x0, 0x3f],
            &[0u8; 63],
        );
    }

    // Send 2 frames. For the second one we can only send 63 bytes.
    // After the first frame there is exactly 63+3 bytes left in the send buffer,
    // but we can only send 63 bytes.
    #[test]
    fn fetch_two_data_frame_second_63bytes_place_for_66() {
        fetch_with_two_data_frames(
            &[0u8; 65446],
            &[0x0, 0x80, 0x0, 0xff, 0x0a6],
            &[0x0, 0x3f],
            &[0u8; 63],
        );
    }

    // Send 2 frames. For the second one we can only send 64 bytes.
    // After the first frame there is exactly 64+3 bytes left in the send buffer,
    // but we can only send 64 bytes.
    #[test]
    fn fetch_two_data_frame_second_64bytes_place_for_67() {
        fetch_with_two_data_frames(
            &[0u8; 65445],
            &[0x0, 0x80, 0x0, 0xff, 0x0a5],
            &[0x0, 0x40, 0x40],
            &[0u8; 64],
        );
    }

    // Send 2 frames. For the second one we can only send 16383 bytes.
    // After the first frame there is exactly 16383+3 bytes left in the send buffer.
    #[test]
    fn fetch_two_data_frame_second_16383bytes() {
        fetch_with_two_data_frames(
            &[0u8; 49126],
            &[0x0, 0x80, 0x0, 0xbf, 0x0e6],
            &[0x0, 0x7f, 0xff],
            &[0u8; 16383],
        );
    }

    // Send 2 frames. For the second one we can only send 16383 bytes.
    // After the first frame there is exactly 16383+4 bytes left in the send buffer, but we can only send 16383 bytes.
    #[test]
    fn fetch_two_data_frame_second_16383bytes_place_for_16387() {
        fetch_with_two_data_frames(
            &[0u8; 49125],
            &[0x0, 0x80, 0x0, 0xbf, 0x0e5],
            &[0x0, 0x7f, 0xff],
            &[0u8; 16383],
        );
    }

    // Send 2 frames. For the second one we can only send 16383 bytes.
    // After the first frame there is exactly 16383+5 bytes left in the send buffer, but we can only send 16383 bytes.
    #[test]
    fn fetch_two_data_frame_second_16383bytes_place_for_16388() {
        fetch_with_two_data_frames(
            &[0u8; 49124],
            &[0x0, 0x80, 0x0, 0xbf, 0x0e4],
            &[0x0, 0x7f, 0xff],
            &[0u8; 16383],
        );
    }

    // Send 2 frames. For the second one we can send 16384 bytes.
    // After the first frame there is exactly 16384+5 bytes left in the send buffer, but we can send 16384 bytes.
    #[test]
    fn fetch_two_data_frame_second_16384bytes_place_for_16389() {
        fetch_with_two_data_frames(
            &[0u8; 49123],
            &[0x0, 0x80, 0x0, 0xbf, 0x0e3],
            &[0x0, 0x80, 0x0, 0x40, 0x0],
            &[0u8; 16384],
        );
    }

    fn read_request(mut neqo_trans_conn: &mut Connection, request_stream_id: u64) {
        // find the new request/response stream and check request data.
        while let Some(e) = neqo_trans_conn.next_event() {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                if stream_id == request_stream_id {
                    // Read only header frame
                    check_header_frame(&mut neqo_trans_conn, stream_id, false);

                    // Read DATA frames.
                    let mut buf = [1u8; 0xffff];
                    let (_, fin) = neqo_trans_conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert_eq!(fin, false);
                }
            }
        }
    }

    // Test receiving STOP_SENDING with the EarlyResponse error code.
    #[test]
    fn test_stop_sending_early_response() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, &[0u8; 10000]);
        assert_eq!(sent, Ok(10000));

        let out = hconn.process(None, now());
        let _ = peer_conn.conn.process(out.dgram(), now());

        read_request(&mut peer_conn.conn, request_stream_id);

        // Stop sending with early_response.
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpEarlyResponse.code())
        );

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        let _ = peer_conn.conn.stream_send(
            request_stream_id,
            &[
                // headers
                0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // a data frame
                0x0, 0x3, 0x61, 0x62, 0x63,
            ],
        );
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        let mut response_headers = false;
        let mut response_body = false;
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::StopSending { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpEarlyResponse.code());
                    // assert that we cannot send any more request data.
                    assert_eq!(
                        Err(Error::AlreadyClosed),
                        hconn.send_request_body(request_stream_id, &[0u8; 10])
                    );
                }
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let h = hconn.read_response_headers(stream_id);
                    assert_eq!(
                        h,
                        Ok((
                            vec![
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3"))
                            ],
                            false
                        ))
                    );
                    response_headers = true;
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = hconn
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, true);
                    assert_eq!(amount, 3);
                    assert_eq!(buf[..3], [0x61, 0x62, 0x63]);
                    response_body = true;
                }
                _ => {}
            }
        }
        assert!(response_headers);
        assert!(response_body);

        // after this stream will be removed from hconn. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    // Server sends stop sending and reset.
    #[test]
    fn test_stop_sending_other_error_with_reset() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, &[0u8; 10000]);
        assert_eq!(sent, Ok(10000));

        let out = hconn.process(None, now());
        let _ = peer_conn.conn.process(out.dgram(), now());

        read_request(&mut peer_conn.conn, request_stream_id);

        // Stop sending with RequestRejected.
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestRejected.code())
        );
        // also reset with RequestRejested.
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_reset_send(request_stream_id, Error::HttpRequestRejected.code())
        );

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestRejected.code());
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        // after this stream will be removed from hconn. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    // Server sends stop sending with RequestRejected, but it does not send reset.
    // We will reset the stream anyway.
    #[test]
    fn test_stop_sending_other_error_wo_reset() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, &[0u8; 10000]);
        assert_eq!(sent, Ok(10000));

        let out = hconn.process(None, now());
        let _ = peer_conn.conn.process(out.dgram(), now());

        read_request(&mut peer_conn.conn, request_stream_id);

        // Stop sending with RequestRejected.
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestRejected.code())
        );

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestRejected.code());
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        // after this stream will be removed from hconn. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    // Server sends stop sending and reset. We have some events for that stream already
    // in hconn.events. The events will be removed.
    #[test]
    fn test_stop_sending_and_reset_other_error_with_events() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, &[0u8; 10000]);
        assert_eq!(sent, Ok(10000));

        let out = hconn.process(None, now());
        let _ = peer_conn.conn.process(out.dgram(), now());

        read_request(&mut peer_conn.conn, request_stream_id);

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        let _ = peer_conn.conn.stream_send(
            request_stream_id,
            &[
                // headers
                0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // a data frame
                0x0, 0x3, 0x61, 0x62, 0x63,
            ],
        );

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());
        // At this moment we have some new events, i.e. a HeadersReady event

        // Send a stop sending and reset.
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestCancelled.code())
        );
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_reset_send(request_stream_id, Error::HttpRequestCancelled.code())
        );

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        // after this stream will be removed from hconn. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    // Server sends stop sending with code that is not EarlyResponse.
    // We have some events for that stream already in the hconn.events.
    // The events will be removed.
    #[test]
    fn test_stop_sending_other_error_with_events() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, &[0u8; 10000]);
        assert_eq!(sent, Ok(10000));

        let out = hconn.process(None, now());
        let _ = peer_conn.conn.process(out.dgram(), now());

        read_request(&mut peer_conn.conn, request_stream_id);

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        let _ = peer_conn.conn.stream_send(
            request_stream_id,
            &[
                // headers
                0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // a data frame
                0x0, 0x3, 0x61, 0x62, 0x63,
            ],
        );

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());
        // At this moment we have some new event, i.e. a HeadersReady event

        // Send a stop sending.
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpRequestCancelled.code())
        );

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        // after this stream will be removed from hconn. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    // Server sends a reset. We will close sending side as well.
    #[test]
    fn test_reset_wo_stop_sending() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(hconn.events().any(data_writable));
        let sent = hconn.send_request_body(request_stream_id, &[0u8; 10000]);
        assert_eq!(sent, Ok(10000));

        let out = hconn.process(None, now());
        let _ = peer_conn.conn.process(out.dgram(), now());

        read_request(&mut peer_conn.conn, request_stream_id);

        // Send a reset.
        assert_eq!(
            Ok(()),
            peer_conn
                .conn
                .stream_reset_send(request_stream_id, Error::HttpRequestCancelled.code())
        );

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
                }
                Http3ClientEvent::HeaderReady { .. } | Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not get any headers or data");
                }
                _ => {}
            }
        }

        // after this stream will be removed from hconn. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = hconn.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        hconn.close(now(), 0, "");
    }

    fn test_incomplet_frame(res: &[u8], error: Error) {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();

        // send an incomplete response - 200  Content-Length: 3
        // with content: 'abc'.
        let _ = peer_conn.conn.stream_send(request_stream_id, res);
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        while let Some(e) = hconn.next_event() {
            if let Http3ClientEvent::DataReadable { stream_id } = e {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let res = hconn.read_response_data(now(), stream_id, &mut buf);
                assert!(res.is_err());
                assert_eq!(res.unwrap_err(), Error::HttpFrameError);
            }
        }
        assert_closed(&hconn, error);
    }

    // Incomplete DATA frame
    #[test]
    fn test_incomplet_data_frame() {
        test_incomplet_frame(
            &[
                // headers
                0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
                // the data frame is incomplete.
                0x0, 0x3, 0x61, 0x62,
            ],
            Error::HttpFrameError,
        );
    }

    // Incomplete HEADERS frame
    #[test]
    fn test_incomplet_headers_frame() {
        test_incomplet_frame(
            &[
                // headers
                0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01,
            ],
            Error::HttpFrameError,
        );
    }

    #[test]
    fn test_incomplet_unknown_frame() {
        test_incomplet_frame(&[0x21], Error::HttpFrameError);
    }

    // test goaway
    #[test]
    fn test_goaway() {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id_1 = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id_1, 0);
        let request_stream_id_2 = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id_2, 4);
        let request_stream_id_3 = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id_3, 8);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        let _ = peer_conn
            .conn
            .stream_send(peer_conn.control_stream_id, &[0x7, 0x1, 0x8]);

        // find the new request/response stream and send frame v on it.
        while let Some(e) = peer_conn.conn.next_event() {
            match e {
                ConnectionEvent::NewStream { .. } => {}
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    let mut buf = [0u8; 100];
                    let _ = peer_conn.conn.stream_recv(stream_id, &mut buf).unwrap();
                    if stream_id == request_stream_id_1 || stream_id == request_stream_id_2 {
                        // send response - 200  Content-Length: 6
                        // with content: 'abcdef'.
                        // The content will be send in 2 DATA frames.
                        let _ = peer_conn.conn.stream_send(
                            stream_id,
                            &[
                                // headers
                                0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
                                // the first data frame
                                0x0, 0x3, 0x61, 0x62, 0x63,
                                // the second data frame
                                // the first data frame
                                0x0, 0x3, 0x64, 0x65, 0x66,
                            ],
                        );

                        peer_conn.conn.stream_close_send(stream_id).unwrap();
                    }
                }
                _ => {}
            }
        }
        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        let mut stream_reset = false;
        let mut http_events = hconn.events().collect::<Vec<_>>();
        while !http_events.is_empty() {
            for e in http_events {
                match e {
                    Http3ClientEvent::HeaderReady { stream_id } => {
                        let h = hconn.read_response_headers(stream_id);
                        assert_eq!(
                            h,
                            Ok((
                                vec![
                                    (String::from(":status"), String::from("200")),
                                    (String::from("content-length"), String::from("3"))
                                ],
                                false
                            ))
                        );
                    }
                    Http3ClientEvent::DataReadable { stream_id } => {
                        assert!(
                            stream_id == request_stream_id_1 || stream_id == request_stream_id_2
                        );
                        let mut buf = [0u8; 100];
                        let (amount, _) = hconn
                            .read_response_data(now(), stream_id, &mut buf)
                            .unwrap();
                        assert_eq!(amount, 3);
                    }
                    Http3ClientEvent::Reset { stream_id, error } => {
                        assert!(stream_id == request_stream_id_3);
                        assert_eq!(error, Error::HttpRequestRejected.code());
                        stream_reset = true;
                    }
                    _ => {}
                }
            }
            hconn.process_http3(now());
            http_events = hconn.events().collect::<Vec<_>>();
        }

        assert!(stream_reset);
        assert_eq!(hconn.state(), Http3State::GoingAway);
        hconn.close(now(), 0, "");
    }

    fn connect_and_send_request() -> (Http3Client, PeerConnection, u64) {
        let (mut hconn, mut peer_conn) = connect();
        let request_stream_id = hconn
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        assert_eq!(request_stream_id, 0);
        let _ = hconn.stream_close_send(request_stream_id);

        let out = hconn.process(None, now());
        peer_conn.conn.process(out.dgram(), now());

        // find the new request/response stream and send frame v on it.
        while let Some(e) = peer_conn.conn.next_event() {
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(stream_type, StreamType::BiDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    check_header_frame(&mut peer_conn.conn, stream_id, true);
                }
                _ => {}
            }
        }
        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        (hconn, peer_conn, request_stream_id)
    }

    // Close stream before headers.
    #[test]
    fn test_stream_fin_wo_headers() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();
        // send fin before sending any data.
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv HeaderReady wo headers with fin.
        let e = hconn.events().next().unwrap();
        if let Http3ClientEvent::HeaderReady { stream_id } = e {
            assert_eq!(stream_id, request_stream_id);
            let h = hconn.read_response_headers(stream_id);
            assert_eq!(h, Ok((vec![], true)));
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Close stream imemediately after headers.
    #[test]
    fn test_stream_fin_after_headers() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();
        let data = &[
            // headers
            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
        ];
        let _ = peer_conn.conn.stream_send(request_stream_id, data);
        // ok NOW send fin
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv HeaderReady with headers and fin.
        let e = hconn.events().next().unwrap();
        if let Http3ClientEvent::HeaderReady { stream_id } = e {
            assert_eq!(stream_id, request_stream_id);
            let h = hconn.read_response_headers(stream_id);
            assert_eq!(
                h,
                Ok((
                    vec![
                        (String::from(":status"), String::from("200")),
                        (String::from("content-length"), String::from("3"))
                    ],
                    true
                ))
            );
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers, read headers and than close stream.
    // We should get HeaderReady and a DataReadable
    #[test]
    fn test_stream_fin_after_headers_are_read_wo_data_frame() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();
        // Send some good data wo fin
        let data = &[
            // headers
            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
        ];
        let _ = peer_conn.conn.stream_send(request_stream_id, data);

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv headers wo fin
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let h = hconn.read_response_headers(stream_id);
                    assert_eq!(
                        h,
                        Ok((
                            vec![
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3"))
                            ],
                            false
                        ))
                    );
                }
                Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not receive a DataGeadable event!");
                }
                _ => {}
            };
        }

        // ok NOW send fin
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv DataReadable wo data with fin
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { .. } => {
                    panic!("We should not get another HeaderReady!");
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let res = hconn.read_response_data(now(), stream_id, &mut buf);
                    let (len, fin) = res.expect("should read");
                    assert_eq!(0, len);
                    assert_eq!(fin, true);
                }
                _ => {}
            };
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers anf an empy data frame and a close stream.
    // We should only recv HeadersReady event
    #[test]
    fn test_stream_fin_after_headers_and_a_empty_data_frame() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();
        // Send some good data wo fin
        let data = &[
            // headers
            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // data
            0x00, 0x00,
        ];
        let _ = peer_conn.conn.stream_send(request_stream_id, data);
        // ok NOW send fin
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv HeaderReady with fin.
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let h = hconn.read_response_headers(stream_id);
                    assert_eq!(
                        h,
                        Ok((
                            vec![
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3"))
                            ],
                            true
                        ))
                    );
                }
                Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not receive a DataGeadable event!");
                }
                _ => {}
            };
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers and an empty data frame. Read headers and then close the stream.
    // We should get a HeaderReady without fin and a DataReadable wo data and with fin.
    #[test]
    fn test_stream_fin_after_headers_an_empty_data_frame_are_read() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();
        // Send some good data wo fin
        let data = &[
            // headers
            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // the data frame
            0x0, 0x0,
        ];
        let _ = peer_conn.conn.stream_send(request_stream_id, data);

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv headers wo fin
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let h = hconn.read_response_headers(stream_id);
                    assert_eq!(
                        h,
                        Ok((
                            vec![
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3"))
                            ],
                            false
                        ))
                    );
                }
                Http3ClientEvent::DataReadable { .. } => {
                    panic!("We should not receive a DataGeadable event!");
                }
                _ => {}
            };
        }

        // ok NOW send fin
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv no data, but do get fin
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { .. } => {
                    panic!("We should not get another HeaderReady!");
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let res = hconn.read_response_data(now(), stream_id, &mut buf);
                    let (len, fin) = res.expect("should read");
                    assert_eq!(0, len);
                    assert_eq!(fin, true);
                }
                _ => {}
            };
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_stream_fin_after_a_data_frame() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();
        // Send some good data wo fin
        let data = &[
            // headers
            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // the data frame is complete
            0x0, 0x3, 0x61, 0x62, 0x63,
        ];
        let _ = peer_conn.conn.stream_send(request_stream_id, data);

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Recv some good data wo fin
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let h = hconn.read_response_headers(stream_id);
                    assert_eq!(
                        h,
                        Ok((
                            vec![
                                (String::from(":status"), String::from("200")),
                                (String::from("content-length"), String::from("3"))
                            ],
                            false
                        ))
                    );
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let res = hconn.read_response_data(now(), stream_id, &mut buf);
                    let (len, fin) = res.expect("should have data");
                    assert_eq!(&buf[..len], &[0x61, 0x62, 0x63]);
                    assert_eq!(fin, false);
                }
                _ => {}
            };
        }

        // ok NOW send fin
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();
        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // fin wo data should generate DataReadable
        let e = hconn.events().next().unwrap();
        if let Http3ClientEvent::DataReadable { stream_id } = e {
            assert_eq!(stream_id, request_stream_id);
            let mut buf = [0u8; 100];
            let res = hconn.read_response_data(now(), stream_id, &mut buf);
            let (len, fin) = res.expect("should read");
            assert_eq!(0, len);
            assert_eq!(fin, true);
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_multiple_data_frames() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();

        // Send two data frames with fin
        let data = &[
            // headers
            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // 2 complete data frames
            0x0, 0x3, 0x61, 0x62, 0x63, 0x0, 0x3, 0x64, 0x65, 0x66,
        ];
        let _ = peer_conn.conn.stream_send(request_stream_id, data);
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());

        // Read first frame
        match hconn.events().nth(1).unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let (len, fin) = hconn
                    .read_response_data(now(), stream_id, &mut buf)
                    .unwrap();
                assert_eq!(&buf[..len], &[0x61, 0x62, 0x63]);
                assert_eq!(fin, false);
            }
            x => {
                eprintln!("event {:?}", x);
                panic!()
            }
        }

        // Second frame isn't read in first read_response_data(), but it generates
        // another DataReadable event so that another read_response_data() will happen to
        // pick it up.
        match hconn.events().next().unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let (len, fin) = hconn
                    .read_response_data(now(), stream_id, &mut buf)
                    .unwrap();
                assert_eq!(&buf[..len], &[0x64, 0x65, 0x66]);
                assert_eq!(fin, true);
            }
            x => {
                eprintln!("event {:?}", x);
                panic!()
            }
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_receive_grease_before_response() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();

        // Construct an unknown frame.
        const UNKNOWN_FRAME_LEN: usize = 832;
        let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
        enc.encode_varint(1028u64); // Arbitrary type.
        enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
        let mut buf: Vec<_> = enc.into();
        buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
        let _ = peer_conn.conn.stream_send(request_stream_id, &buf).unwrap();

        // Send a headers and a data frame with fin
        let data = &[
            // headers
            0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // 1 complete data frames
            0x0, 0x3, 0x61, 0x62, 0x63,
        ];
        let _ = peer_conn.conn.stream_send(request_stream_id, data);
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        let out = peer_conn.conn.process(None, now());
        hconn.process(out.dgram(), now());
        hconn.process(None, now());

        // Read first frame
        match hconn.events().nth(1).unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let (len, fin) = hconn
                    .read_response_data(now(), stream_id, &mut buf)
                    .unwrap();
                assert_eq!(&buf[..len], &[0x61, 0x62, 0x63]);
                assert_eq!(fin, true);
            }
            x => {
                eprintln!("event {:?}", x);
                panic!()
            }
        }
        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            hconn.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_read_frames_header_blocked() {
        let (mut hconn, mut peer_conn, request_stream_id) = connect_and_send_request();

        peer_conn.encoder.set_max_capacity(100).unwrap();
        peer_conn.encoder.set_max_blocked_streams(100).unwrap();

        let headers = vec![
            (String::from(":status"), String::from("200")),
            (String::from("my-header"), String::from("my-header")),
            (String::from("content-length"), String::from("3")),
        ];
        let encoded_headers = peer_conn
            .encoder
            .encode_header_block(&headers, request_stream_id);
        let hframe = HFrame::Headers {
            len: encoded_headers.len() as u64,
        };
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        d.encode(&encoded_headers);
        let d_frame = HFrame::Data { len: 3 };
        d_frame.encode(&mut d);
        d.encode(&[0x61, 0x62, 0x63]);
        let _ = peer_conn.conn.stream_send(request_stream_id, &d[..]);
        peer_conn.conn.stream_close_send(request_stream_id).unwrap();

        // Send response before sending encoder instructions.
        let out = peer_conn.conn.process(None, now());
        let _out = hconn.process(out.dgram(), now());

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!hconn.events().any(header_ready_event));

        // Send encoder instructions to unblock the stream.
        peer_conn.encoder.send(&mut peer_conn.conn).unwrap();

        let out = peer_conn.conn.process(None, now());
        let _out = hconn.process(out.dgram(), now());
        let _out = hconn.process(None, now());

        let mut recv_header = false;
        let mut recv_data = false;
        // Now the stream is unblocked and both headers and data will be consumed.
        while let Some(e) = hconn.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    recv_header = true;
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    recv_data = true;
                    assert_eq!(stream_id, request_stream_id);
                }
                x => {
                    eprintln!("event {:?}", x);
                    panic!()
                }
            }
        }
        assert!(recv_header && recv_data);
    }
}
