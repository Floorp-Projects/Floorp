// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::client_events::{Http3ClientEvent, Http3ClientEvents};
use crate::connection::{HandleReadableOutput, Http3Connection, Http3State, Http3Transaction};
use crate::hframe::HFrame;
use crate::hsettings_frame::HSettings;
use crate::transaction_client::TransactionClient;
use crate::Header;
use neqo_common::{hex, matches, qdebug, qinfo, qtrace, Datagram, Decoder, Encoder};
use neqo_crypto::{agent::CertificateInfo, AuthenticationStatus, SecretAgentInfo};
use neqo_transport::{
    AppError, Connection, ConnectionEvent, ConnectionIdManager, Output, Role, StreamType,
};
use std::cell::RefCell;
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::Instant;

use crate::{Error, Res};

pub struct Http3Client {
    conn: Connection,
    base_handler: Http3Connection<TransactionClient>,
    events: Http3ClientEvents,
}

impl ::std::fmt::Display for Http3Client {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 client")
    }
}

impl Http3Client {
    pub fn new(
        server_name: &str,
        protocols: &[impl AsRef<str>],
        cid_manager: Rc<RefCell<dyn ConnectionIdManager>>,
        local_addr: SocketAddr,
        remote_addr: SocketAddr,
        max_table_size: u64,
        max_blocked_streams: u16,
    ) -> Res<Self> {
        Ok(Self::new_with_conn(
            Connection::new_client(server_name, protocols, cid_manager, local_addr, remote_addr)?,
            max_table_size,
            max_blocked_streams,
        ))
    }

    pub fn new_with_conn(c: Connection, max_table_size: u64, max_blocked_streams: u16) -> Self {
        Self {
            conn: c,
            base_handler: Http3Connection::new(max_table_size, max_blocked_streams),
            events: Http3ClientEvents::default(),
        }
    }

    pub fn role(&self) -> Role {
        self.conn.role()
    }

    pub fn state(&self) -> Http3State {
        self.base_handler.state()
    }

    pub fn tls_info(&self) -> Option<&SecretAgentInfo> {
        self.conn.tls_info()
    }

    /// Get the peer's certificate.
    pub fn peer_certificate(&self) -> Option<CertificateInfo> {
        self.conn.peer_certificate()
    }

    pub fn authenticated(&mut self, status: AuthenticationStatus, now: Instant) {
        self.conn.authenticated(status, now);
    }

    pub fn resumption_token(&self) -> Option<Vec<u8>> {
        if let Some(token) = self.conn.resumption_token() {
            if let Some(settings) = self.base_handler.get_settings() {
                let mut enc = Encoder::default();
                settings.encode_frame_contents(&mut enc);
                enc.encode(&token[..]);
                Some(enc.into())
            } else {
                None
            }
        } else {
            None
        }
    }

    pub fn set_resumption_token(&mut self, now: Instant, token: &[u8]) -> Res<()> {
        let mut dec = Decoder::from(token);
        let settings_slice = match dec.decode_vvec() {
            Some(v) => v,
            _ => return Err(Error::InvalidResumptionToken),
        };
        qtrace!([self], "  settings {}", hex(&settings_slice));
        let mut dec_settings = Decoder::from(settings_slice);
        let mut settings = HSettings::default();
        settings.decode_frame_contents(&mut dec_settings)?;
        let tok = dec.decode_remainder();
        qtrace!([self], "  Transport token {}", hex(&tok));
        self.conn.set_resumption_token(now, tok)?;
        self.base_handler
            .set_resumption_settings(&mut self.conn, settings)
    }

    pub fn close(&mut self, now: Instant, error: AppError, msg: &str) {
        qinfo!([self], "Close the connection error={} msg={}.", error, msg);
        if !matches!(self.base_handler.state, Http3State::Closing(_)| Http3State::Closed(_)) {
            self.conn.close(now, error, msg);
            self.base_handler.close(error);
            self.events
                .connection_state_change(self.base_handler.state());
        }
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
        let id = self.conn.stream_create(StreamType::BiDi)?;
        self.base_handler.add_transaction(
            id,
            TransactionClient::new(id, method, scheme, host, path, headers, self.events.clone()),
        );
        Ok(id)
    }

    pub fn stream_reset(&mut self, stream_id: u64, error: AppError) -> Res<()> {
        qinfo!([self], "reset_stream {} error={}.", stream_id, error);
        self.base_handler
            .stream_reset(&mut self.conn, stream_id, error)?;
        self.events.remove_events_for_stream_id(stream_id);
        Ok(())
    }

    pub fn stream_close_send(&mut self, stream_id: u64) -> Res<()> {
        qinfo!([self], "Close sending side stream={}.", stream_id);
        self.base_handler
            .stream_close_send(&mut self.conn, stream_id)
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
            .send_request_body(&mut self.conn, buf)
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
                    qinfo!([self], "read_response_headers transaction done");
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

        match transaction.read_response_data(&mut self.conn, buf) {
            Ok((amount, fin)) => {
                if fin {
                    self.base_handler.transactions.remove(&stream_id);
                } else if amount > 0 {
                    // Directly call receive instead of adding to
                    // streams_are_readable here. This allows the app to
                    // pick up subsequent already-received data frames in
                    // the stream even if no new packets arrive to cause
                    // process_http3() to run.
                    transaction.receive(&mut self.conn, &mut self.base_handler.qpack_decoder)?;
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
        self.events.events()
    }

    /// Return true if there are outstanding events.
    pub fn has_events(&self) -> bool {
        self.events.has_events()
    }

    /// Get events that indicate state changes on the connection. This method
    /// correctly handles cases where handling one event can obsolete
    /// previously-queued events, or cause new events to be generated.
    pub fn next_event(&mut self) -> Option<Http3ClientEvent> {
        self.events.next_event()
    }

    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        qtrace!([self], "Process.");
        if let Some(d) = dgram {
            self.process_input(d, now);
        }
        self.process_http3(now);
        self.process_output(now)
    }

    pub fn process_input(&mut self, dgram: Datagram, now: Instant) {
        qtrace!([self], "Process input.");
        self.conn.process_input(dgram, now);
    }

    pub fn process_timer(&mut self, now: Instant) {
        qtrace!([self], "Process timer.");
        self.conn.process_timer(now);
    }

    pub fn conn(&mut self) -> &mut Connection {
        &mut self.conn
    }

    pub fn process_http3(&mut self, now: Instant) {
        qtrace!([self], "Process http3 internal.");
        match self.base_handler.state() {
            Http3State::ZeroRtt | Http3State::Connected | Http3State::GoingAway => {
                let res = self.check_connection_events();
                if self.check_result(now, res) {
                    return;
                }
                let res = self.base_handler.process_sending(&mut self.conn);
                self.check_result(now, res);
            }
            Http3State::Closed { .. } => {}
            _ => {
                let res = self.check_connection_events();
                let _ = self.check_result(now, res);
            }
        }
    }

    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!([self], "Process output.");
        self.conn.process_output(now)
    }

    // This function takes the provided result and check for an error.
    // An error results in closing the connection.
    fn check_result<ERR>(&mut self, now: Instant, res: Res<ERR>) -> bool {
        match &res {
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
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => match stream_type {
                    StreamType::BiDi => return Err(Error::HttpStreamCreationError),
                    StreamType::UniDi => {
                        if self
                            .base_handler
                            .handle_new_unidi_stream(&mut self.conn, stream_id)?
                        {
                            return Err(Error::HttpIdError);
                        }
                    }
                },
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    if let Some(t) = self.base_handler.transactions.get_mut(&stream_id) {
                        if t.is_state_sending_data() {
                            self.events.data_writable(stream_id);
                        }
                    }
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    self.handle_stream_readable(stream_id)?
                }
                ConnectionEvent::RecvStreamReset {
                    stream_id,
                    app_error,
                } => {
                    if self.base_handler.handle_stream_reset(
                        &mut self.conn,
                        stream_id,
                        app_error,
                    )? {
                        // Post the reset event.
                        self.events.reset(stream_id, app_error);
                    }
                }
                ConnectionEvent::SendStreamStopSending {
                    stream_id,
                    app_error,
                } => self.handle_stream_stop_sending(stream_id, app_error)?,
                ConnectionEvent::SendStreamComplete { .. } => {}
                ConnectionEvent::SendStreamCreatable { stream_type } => {
                    self.events.new_requests_creatable(stream_type)
                }
                ConnectionEvent::AuthenticationNeeded => self.events.authentication_needed(),
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
                }
            }
        }
        Ok(())
    }

    fn handle_stream_readable(&mut self, stream_id: u64) -> Res<()> {
        match self
            .base_handler
            .handle_stream_readable(&mut self.conn, stream_id)?
        {
            HandleReadableOutput::PushStream => Err(Error::HttpIdError),
            HandleReadableOutput::ControlFrames(control_frames) => {
                for f in control_frames.into_iter() {
                    match f {
                        HFrame::MaxPushId { .. } => Err(Error::HttpFrameUnexpected),
                        HFrame::Goaway { stream_id } => self.handle_goaway(stream_id),
                        _ => {
                            unreachable!(
                                "we should only put MaxPushId and Goaway into control_frames."
                            );
                        }
                    }?;
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }

    fn handle_stream_stop_sending(&mut self, stop_stream_id: u64, app_err: AppError) -> Res<()> {
        qinfo!(
            [self],
            "Handle stream_stop_sending stream_id={} app_err={}",
            stop_stream_id,
            app_err
        );

        if let Some(t) = self.base_handler.transactions.get_mut(&stop_stream_id) {
            // If error is Error::EarlyResponse we will post StopSending event,
            // otherwise post reset.
            if app_err == Error::HttpEarlyResponse.code() && !t.is_sending_closed() {
                self.events.stop_sending(stop_stream_id, app_err);
            }
            // close sending side.
            t.stop_sending();
            // if error is not Error::EarlyResponse we will close receiving part as well.
            if app_err != Error::HttpEarlyResponse.code() {
                self.events.reset(stop_stream_id, app_err);
                // The server may close its sending side as well, but just to be sure
                // we will do it ourselves.
                let _ = self.conn.stream_stop_sending(stop_stream_id, app_err);
                t.reset_receiving_side();
            }
            if t.done() {
                self.base_handler.transactions.remove(&stop_stream_id);
            }
        }
        Ok(())
    }

    fn handle_goaway(&mut self, goaway_stream_id: u64) -> Res<()> {
        qinfo!([self], "handle_goaway");
        // Issue reset events for streams >= goaway stream id
        for id in self
            .base_handler
            .transactions
            .iter()
            .filter(|(id, _)| **id >= goaway_stream_id)
            .map(|(id, _)| *id)
        {
            self.events.reset(id, Error::HttpRequestRejected.code())
        }
        self.events.goaway_received();

        // Actually remove (i.e. don't retain) these streams
        self.base_handler
            .transactions
            .retain(|id, _| *id < goaway_stream_id);

        if self.base_handler.state == Http3State::Connected {
            self.base_handler.state = Http3State::GoingAway;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::hframe::HFrame;
    use crate::hsettings_frame::{HSetting, HSettingType};
    use neqo_common::{matches, Encoder};
    use neqo_crypto::AntiReplay;
    use neqo_qpack::encoder::QPackEncoder;
    use neqo_transport::{CloseError, ConnectionEvent, FixedConnectionIdManager, State};
    use test_fixture::*;

    fn assert_closed(client: &Http3Client, expected: Error) {
        match client.state() {
            Http3State::Closing(err) | Http3State::Closed(err) => {
                assert_eq!(err, CloseError::Application(expected.code()))
            }
            _ => panic!("Wrong state {:?}", client.state()),
        };
    }

    /// Create a http3 client with default configuration.
    pub fn default_http3_client() -> Http3Client {
        fixture_init();
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

    // default_http3_client use following setting:
    //  - max_table_capacity = 100
    //  - max_blocked_streams = 100
    // The following is what the server will see on the control stream:
    //  - 0x0 - control stream type
    //  - 0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64 - a setting frame with MaxTableCapacity
    //    and BlockedStreams both equal to 100.
    const CONTROL_STREAM_DATA: &[u8] = &[0x0, 0x4, 0x6, 0x1, 0x40, 0x64, 0x7, 0x40, 0x64];

    const CONTROL_STREAM_TYPE: &[u8] = &[0x0];

    // Encoder stream data
    const ENCODER_STREAM_DATA: &[u8] = &[0x2];

    // Encoder stream data with a change capacity instruction(0x3f, 0x45 = change capacity to 100)
    // This data will be send when 0-RTT is used and we already have a max_table_capacity from
    // resumed settings.
    const ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION: &[u8] = &[0x2, 0x3f, 0x45];

    // Decoder stream data
    const DECODER_STREAM_DATA: &[u8] = &[0x3];

    const PUSH_STREAM_DATA: &[u8] = &[0x1];

    struct TestServer {
        settings: HFrame,
        conn: Connection,
        control_stream_id: Option<u64>,
        encoder: QPackEncoder,
    }

    fn make_server(server_settings: &[HSetting]) -> TestServer {
        fixture_init();
        TestServer {
            settings: HFrame::Settings {
                settings: HSettings::new(server_settings),
            },
            conn: default_server(),
            control_stream_id: None,
            encoder: QPackEncoder::new(true),
        }
    }

    fn make_default_server() -> TestServer {
        make_server(&[
            HSetting::new(HSettingType::MaxTableCapacity, 100),
            HSetting::new(HSettingType::BlockedStreams, 100),
            HSetting::new(HSettingType::MaxHeaderListSize, 10000),
        ])
    }

    // Perform only Quic transport handshake.
    fn connect_only_transport_with(client: &mut Http3Client, server: &mut TestServer) {
        assert_eq!(client.state(), Http3State::Initializing);
        let out = client.process(None, now());
        assert_eq!(client.state(), Http3State::Initializing);

        assert_eq!(*server.conn.state(), State::WaitInitial);
        let out = server.conn.process(out.dgram(), now());
        assert_eq!(*server.conn.state(), State::Handshaking);

        let out = client.process(out.dgram(), now());
        let out = server.conn.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());

        let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
        assert!(client.events().any(authentication_needed));
        client.authenticated(AuthenticationStatus::Ok, now());

        let out = client.process(out.dgram(), now());
        let connected = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::Connected));
        assert!(client.events().any(connected));

        assert_eq!(client.state(), Http3State::Connected);
        let _ = server.conn.process(out.dgram(), now());
        assert!(server.conn.state().connected());
    }

    // Perform only Quic transport handshake.
    fn connect_only_transport() -> (Http3Client, TestServer) {
        let mut client = default_http3_client();
        let mut server = make_default_server();
        connect_only_transport_with(&mut client, &mut server);
        (client, server)
    }

    // Perform Quic transport handshake and exchange Http3 settings.
    fn connect_with(client: &mut Http3Client, server: &mut TestServer) {
        connect_only_transport_with(client, server);

        // send and receive client settings
        let out = client.process(None, now());
        server.conn.process(out.dgram(), now());
        check_control_qpack_streams(&mut server.conn);

        // send and receive server settings

        // Creat control stream
        server.control_stream_id = Some(server.conn.stream_create(StreamType::UniDi).unwrap());
        let mut enc = Encoder::default();
        server.settings.encode(&mut enc);
        // Send stream type on the control stream.
        let mut sent = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), CONTROL_STREAM_TYPE);
        assert_eq!(sent.unwrap(), 1);
        // Encode a settings frame and send it.
        let mut enc = Encoder::default();
        server.settings.encode(&mut enc);
        sent = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &enc[..]);
        assert_eq!(sent.unwrap(), enc[..].len());
        // Create a QPACK encoder stream
        server
            .encoder
            .add_send_stream(server.conn.stream_create(StreamType::UniDi).unwrap());
        server.encoder.send(&mut server.conn).unwrap();

        // Create decoder stream
        let decoder_stream = server.conn.stream_create(StreamType::UniDi).unwrap();
        sent = server.conn.stream_send(decoder_stream, DECODER_STREAM_DATA);
        assert_eq!(sent, Ok(1));
        // Actually send all above data
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // assert no error occured.
        assert_eq!(client.state(), Http3State::Connected);
    }

    // Perform Quic transport handshake and exchange Http3 settings.
    fn connect() -> (Http3Client, TestServer) {
        let mut client = default_http3_client();
        let mut server = make_default_server();
        connect_with(&mut client, &mut server);
        (client, server)
    }

    fn read_and_check_stream_data(
        server: &mut Connection,
        stream_id: u64,
        expected_data: &[u8],
        expected_fin: bool,
    ) {
        let mut buf = [0u8; 100];
        let (amount, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(fin, expected_fin);
        assert_eq!(amount, expected_data.len());
        assert_eq!(&buf[..amount], expected_data);
    }

    // Check that server has received correct settings and qpack streams.
    fn check_control_qpack_streams(server: &mut Connection) {
        let mut connected = false;
        let mut control_stream = false;
        let mut qpack_decoder_stream = false;
        let mut qpack_encoder_stream = false;
        while let Some(e) = server.next_event() {
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => {
                    assert!(matches!(stream_id, 2 | 6 | 10));
                    assert_eq!(stream_type, StreamType::UniDi);
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    if stream_id == 2 {
                        // the control stream
                        read_and_check_stream_data(server, stream_id, CONTROL_STREAM_DATA, false);
                        control_stream = true;
                    } else if stream_id == 6 {
                        // the qpack encoder stream
                        read_and_check_stream_data(server, stream_id, ENCODER_STREAM_DATA, false);
                        qpack_encoder_stream = true;
                    } else if stream_id == 10 {
                        // the qpack decoder stream
                        read_and_check_stream_data(server, stream_id, DECODER_STREAM_DATA, false);
                        qpack_decoder_stream = true;
                    } else {
                        panic!("unexpected event");
                    }
                }
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    assert!(matches!(stream_id, 2 | 6 | 10));
                }
                ConnectionEvent::StateChange(State::Connected) => connected = true,
                ConnectionEvent::StateChange(_) => {}
                _ => panic!("unexpected event"),
            }
        }
        assert!(connected);
        assert!(control_stream);
        assert!(qpack_encoder_stream);
        assert!(qpack_decoder_stream);
    }

    // Fetch request fetch("GET", "https", "something.com", "/", &[]).
    fn make_request(client: &mut Http3Client, close_sending_side: bool) -> u64 {
        let request_stream_id = client
            .fetch("GET", "https", "something.com", "/", &[])
            .unwrap();
        if close_sending_side {
            let _ = client.stream_close_send(request_stream_id);
        }
        request_stream_id
    }

    // For fetch request fetch("GET", "https", "something.com", "/", &[])
    // the following request header frame will be sent:
    const EXPECTED_REQUEST_HEADER_FRAME: &[u8] = &[
        0x01, 0x10, 0x00, 0x00, 0xd1, 0xd7, 0x50, 0x89, 0x41, 0xe9, 0x2a, 0x67, 0x35, 0x53, 0x2e,
        0x43, 0xd3, 0xc1,
    ];

    const HTTP_HEADER_FRAME_0: &[u8] = &[0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x30];

    // The response header from HTTP_HEADER_FRAME (0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x30) are
    // decoded into:
    fn check_response_header_0(header: Vec<Header>) {
        let expected_response_header_1 = vec![
            (String::from(":status"), String::from("200")),
            (String::from("content-length"), String::from("0")),
        ];
        assert_eq!(header, expected_response_header_1);
    }

    const HTTP_RESPONSE_1: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x37, // the first data frame
        0x0, 0x3, 0x61, 0x62, 0x63, // the second data frame
        0x0, 0x4, 0x64, 0x65, 0x66, 0x67,
    ];

    // The response header from HTTP_RESPONSE_1 (0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x36) are
    // decoded into:
    fn check_response_header_1(header: Vec<Header>) {
        let expected_response_header_1 = vec![
            (String::from(":status"), String::from("200")),
            (String::from("content-length"), String::from("7")),
        ];
        assert_eq!(header, expected_response_header_1);
    }

    // 2 data frames payload from HTTP_RESPONSE_1 are:
    const EXPECTED_RESPONSE_DATA_1_FRAME_1: &[u8] = &[0x61, 0x62, 0x63];
    const EXPECTED_RESPONSE_DATA_1_FRAME_2: &[u8] = &[0x64, 0x65, 0x66, 0x67];

    const HTTP_RESPONSE_2: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33, // the data frame
        0x0, 0x3, 0x61, 0x62, 0x63,
    ];

    const HTTP_RESPONSE_HEADER_ONLY_2: &[u8] = &[
        // headers
        0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x33,
    ];

    // The response header from HTTP_RESPONSE_2 (0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x36) are
    // decoded into:
    fn check_response_header_2(header: Vec<Header>) {
        let expected_response_header_2 = vec![
            (String::from(":status"), String::from("200")),
            (String::from("content-length"), String::from("3")),
        ];
        assert_eq!(header, expected_response_header_2);
    }

    // The data frame payload from HTTP_RESPONSE_2 is:
    const EXPECTED_RESPONSE_DATA_2_FRAME_1: &[u8] = &[0x61, 0x62, 0x63];

    fn connect_and_send_request(close_sending_side: bool) -> (Http3Client, TestServer, u64) {
        let (mut client, mut server) = connect();
        let request_stream_id = make_request(&mut client, close_sending_side);
        assert_eq!(request_stream_id, 0);

        let out = client.process(None, now());
        server.conn.process(out.dgram(), now());

        // find the new request/response stream and send frame v on it.
        while let Some(e) = server.conn.next_event() {
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
                    read_and_check_stream_data(
                        &mut server.conn,
                        stream_id,
                        EXPECTED_REQUEST_HEADER_FRAME,
                        close_sending_side,
                    );
                }
                _ => {}
            }
        }
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        (client, server, request_stream_id)
    }

    // Client: Test receiving a new control stream and a SETTINGS frame.
    #[test]
    fn test_client_connect_and_exchange_qpack_and_control_streams() {
        let _ = connect();
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
        assert_closed(&client, Error::HttpClosedCriticalStream);
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
        assert_closed(&client, Error::HttpMissingSettings);
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
        assert_closed(&client, Error::HttpFrameUnexpected);
    }

    fn test_wrong_frame_on_control_stream(v: &[u8]) {
        let (mut client, mut server) = connect();

        // send a frame that is not allowed on the control stream.
        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), v);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        assert_closed(&client, Error::HttpFrameUnexpected);
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

    // Client: receive unknown stream type
    // This function also tests getting stream id that does not fit into a single byte.
    #[test]
    fn test_client_received_unknown_stream() {
        let (mut client, mut server) = connect();

        // create a stream with unknown type.
        let new_stream_id = server.conn.stream_create(StreamType::UniDi).unwrap();
        let _ = server
            .conn
            .stream_send(new_stream_id, &[0x41, 0x19, 0x4, 0x4, 0x6, 0x0, 0x8, 0x0]);
        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        server.conn.process(out.dgram(), now());

        // check for stop-sending with Error::HttpStreamCreationError.
        let mut stop_sending_event_found = false;
        while let Some(e) = server.conn.next_event() {
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
        assert_eq!(client.state(), Http3State::Connected);
    }

    // Client: receive a push stream
    #[test]
    fn test_client_received_push_stream() {
        let (mut client, mut server) = connect();

        // create a push stream.
        let push_stream_id = server.conn.stream_create(StreamType::UniDi).unwrap();
        let _ = server.conn.stream_send(push_stream_id, PUSH_STREAM_DATA);
        let out = server.conn.process(None, now());
        let out = client.process(out.dgram(), now());
        server.conn.process(out.dgram(), now());

        assert_closed(&client, Error::HttpIdError);
    }

    // Test wrong frame on req/rec stream
    fn test_wrong_frame_on_request_stream(v: &[u8]) {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        let _ = server.conn.stream_send(request_stream_id, v);

        // Generate packet with the above bad h3 input
        let out = server.conn.process(None, now());
        // Process bad input and close the connection.
        let _ = client.process(out.dgram(), now());

        assert_closed(&client, Error::HttpFrameUnexpected);
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
        assert_closed(&client, Error::HttpFrameUnexpected);
    }

    #[test]
    fn fetch_basic() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // send response - 200  Content-Length: 7
        // with content: 'abcdefg'.
        // The content will be send in 2 DATA frames.
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_1);
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let http_events = client.events().collect::<Vec<_>>();
        assert_eq!(http_events.len(), 2);
        for e in http_events {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let (h, fin) = client.read_response_headers(stream_id).unwrap();
                    check_response_header_1(h);
                    assert_eq!(fin, false);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = client
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, false);
                    assert_eq!(amount, EXPECTED_RESPONSE_DATA_1_FRAME_1.len());
                    assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_1_FRAME_1);
                }
                _ => {}
            }
        }

        client.process_http3(now());
        let http_events = client.events().collect::<Vec<_>>();
        assert_eq!(http_events.len(), 1);
        for e in http_events {
            match e {
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = client
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, true);
                    assert_eq!(amount, EXPECTED_RESPONSE_DATA_1_FRAME_2.len());
                    assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_1_FRAME_2);
                }
                _ => panic!("unexpected event"),
            }
        }

        // after this stream will be removed from hcoon. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    // Helper function: read response when a server sends HTTP_RESPONSE_2.
    fn read_response(client: &mut Http3Client, server: &mut Connection, request_stream_id: u64) {
        let out = server.process(None, now());
        client.process(out.dgram(), now());

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let (h, fin) = client.read_response_headers(stream_id).unwrap();
                    check_response_header_2(h);
                    assert_eq!(fin, false);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = client
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, true);
                    assert_eq!(amount, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                    assert_eq!(&buf[..amount], EXPECTED_RESPONSE_DATA_2_FRAME_1);
                }
                _ => {}
            }
        }

        // after this stream will be removed from client. We will check this by trying to read
        // from the stream and that should fail.
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
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
        let sent = client
            .send_request_body(request_stream_id, REQUEST_BODY)
            .unwrap();
        assert_eq!(sent, REQUEST_BODY.len());
        let _ = client.stream_close_send(request_stream_id);

        let out = client.process(None, now());
        server.conn.process(out.dgram(), now());

        // find the new request/response stream and send response on it.
        while let Some(e) = server.conn.next_event() {
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

                    // Read request body.
                    let mut buf = [0u8; 100];
                    let (amount, fin) = server.conn.stream_recv(stream_id, &mut buf).unwrap();
                    assert_eq!(fin, true);
                    assert_eq!(amount, EXPECTED_REQUEST_BODY_FRAME.len());
                    assert_eq!(&buf[..amount], EXPECTED_REQUEST_BODY_FRAME);

                    // send response - 200  Content-Length: 3
                    // with content: 'abc'.
                    let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_2);
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
        let sent = client.send_request_body(request_stream_id, request_body);
        assert_eq!(sent, Ok(request_body.len()));

        // Close stream.
        let _ = client.stream_close_send(request_stream_id);

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
                    let mut buf = [1u8; 0xffff];
                    let (amount, fin) = server.conn.stream_recv(stream_id, &mut buf).unwrap();
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
                    let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_2);
                    server.conn.stream_close_send(stream_id).unwrap();
                }
            }
        }

        read_response(&mut client, &mut server.conn, request_stream_id);
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
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Get DataWritable for the request stream so that we can write the request body.
        let data_writable = |e| matches!(e, Http3ClientEvent::DataWritable { .. });
        assert!(client.events().any(data_writable));

        // Send the first frame.
        let sent = client.send_request_body(request_stream_id, first_frame);
        assert_eq!(sent, Ok(first_frame.len()));

        // The second frame cannot fit.
        let sent = client.send_request_body(request_stream_id, &[0u8; 0xffff]);
        assert_eq!(sent, Ok(expected_second_data_frame.len()));

        // Close stream.
        let _ = client.stream_close_send(request_stream_id);

        let mut out = client.process(None, now());
        // We need to loop a bit until all data has been sent.
        for _i in 0..55 {
            out = server.conn.process(out.dgram(), now());
            out = client.process(out.dgram(), now());
        }

        //  check received frames and send a response.
        while let Some(e) = server.conn.next_event() {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                if stream_id == request_stream_id {
                    // Read DATA frames.
                    let mut buf = [1u8; 0xffff];
                    let (amount, fin) = server.conn.stream_recv(stream_id, &mut buf).unwrap();
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
                    let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_2);
                    server.conn.stream_close_send(stream_id).unwrap();
                }
            }
        }

        read_response(&mut client, &mut server.conn, request_stream_id);
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

    // Test receiving STOP_SENDING with the EarlyResponse error code.
    #[test]
    fn test_stop_sending_early_response() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Stop sending with early_response.
        assert_eq!(
            Ok(()),
            server
                .conn
                .stream_stop_sending(request_stream_id, Error::HttpEarlyResponse.code())
        );

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_2);
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut stop_sending = false;
        let mut response_headers = false;
        let mut response_body = false;
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpEarlyResponse.code());
                    // assert that we cannot send any more request data.
                    assert_eq!(
                        Err(Error::AlreadyClosed),
                        client.send_request_body(request_stream_id, &[0u8; 10])
                    );
                    stop_sending = true;
                }
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let (h, fin) = client.read_response_headers(stream_id).unwrap();
                    check_response_header_2(h);
                    assert_eq!(fin, false);
                    response_headers = true;
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let (amount, fin) = client
                        .read_response_data(now(), stream_id, &mut buf)
                        .unwrap();
                    assert_eq!(fin, true);
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
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
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
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestRejected.code());
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
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    // Server sends stop sending with RequestRejected, but it does not send reset.
    // We will reset the stream anyway.
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

        let mut reset = false;

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestRejected.code());
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
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

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
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_2);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
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
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
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
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    // Server sends stop sending with code that is not EarlyResponse.
    // We have some events for that stream already in the client.events.
    // The events will be removed.
    #[test]
    fn test_stop_sending_other_error_with_events() {
        // Connect exchange headers and send a request. Also check if the correct header frame has been sent.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // send response - 200  Content-Length: 3
        // with content: 'abc'.
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_2);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
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

        let mut reset = false;

        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::StopSending { .. } => {
                    panic!("We should not get StopSending.");
                }
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
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
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

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
                Http3ClientEvent::Reset { stream_id, error } => {
                    assert_eq!(stream_id, request_stream_id);
                    assert_eq!(error, Error::HttpRequestCancelled.code());
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
        let mut buf = [0u8; 100];
        let res = client.read_response_data(now(), request_stream_id, &mut buf);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        client.close(now(), 0, "");
    }

    fn test_incomplet_frame(buf: &[u8], error: Error) {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        let _ = server.conn.stream_send(request_stream_id, buf);
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::DataReadable { stream_id } = e {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let res = client.read_response_data(now(), stream_id, &mut buf);
                assert!(res.is_err());
                assert_eq!(res.unwrap_err(), Error::HttpFrameError);
            }
        }
        assert_closed(&client, error);
    }

    // Incomplete DATA frame
    #[test]
    fn test_incomplet_data_frame() {
        test_incomplet_frame(&HTTP_RESPONSE_2[..12], Error::HttpFrameError);
    }

    // Incomplete HEADERS frame
    #[test]
    fn test_incomplet_headers_frame() {
        test_incomplet_frame(&HTTP_RESPONSE_2[..7], Error::HttpFrameError);
    }

    #[test]
    fn test_incomplet_unknown_frame() {
        test_incomplet_frame(&[0x21], Error::HttpFrameError);
    }

    // test goaway
    #[test]
    fn test_goaway() {
        let (mut client, mut server) = connect();
        let request_stream_id_1 = make_request(&mut client, false);
        assert_eq!(request_stream_id_1, 0);
        let request_stream_id_2 = make_request(&mut client, false);
        assert_eq!(request_stream_id_2, 4);
        let request_stream_id_3 = make_request(&mut client, false);
        assert_eq!(request_stream_id_3, 8);

        let out = client.process(None, now());
        server.conn.process(out.dgram(), now());

        let _ = server
            .conn
            .stream_send(server.control_stream_id.unwrap(), &[0x7, 0x1, 0x8]);

        // find the new request/response stream and send frame v on it.
        while let Some(e) = server.conn.next_event() {
            match e {
                ConnectionEvent::NewStream { .. } => {}
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    let mut buf = [0u8; 100];
                    let _ = server.conn.stream_recv(stream_id, &mut buf).unwrap();
                    if (stream_id == request_stream_id_1) || (stream_id == request_stream_id_2) {
                        // send response - 200  Content-Length: 7
                        // with content: 'abcdefg'.
                        // The content will be send in 2 DATA frames.
                        let _ = server.conn.stream_send(stream_id, HTTP_RESPONSE_1);
                        server.conn.stream_close_send(stream_id).unwrap();
                    }
                }
                _ => {}
            }
        }
        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let mut stream_reset = false;
        let mut http_events = client.events().collect::<Vec<_>>();
        while !http_events.is_empty() {
            for e in http_events {
                match e {
                    Http3ClientEvent::HeaderReady { stream_id } => {
                        let (h, fin) = client.read_response_headers(stream_id).unwrap();
                        check_response_header_1(h);
                        assert_eq!(fin, false);
                    }
                    Http3ClientEvent::DataReadable { stream_id } => {
                        assert!(
                            (stream_id == request_stream_id_1)
                                || (stream_id == request_stream_id_2)
                        );
                        let mut buf = [0u8; 100];
                        let (amount, _) = client
                            .read_response_data(now(), stream_id, &mut buf)
                            .unwrap();
                        assert!(
                            (amount == EXPECTED_RESPONSE_DATA_1_FRAME_1.len())
                                || (amount == EXPECTED_RESPONSE_DATA_1_FRAME_2.len())
                        );
                    }
                    Http3ClientEvent::Reset { stream_id, error } => {
                        assert_eq!(stream_id, request_stream_id_3);
                        assert_eq!(error, Error::HttpRequestRejected.code());
                        stream_reset = true;
                    }
                    _ => {}
                }
            }
            client.process_http3(now());
            http_events = client.events().collect::<Vec<_>>();
        }

        assert!(stream_reset);
        assert_eq!(client.state(), Http3State::GoingAway);
        client.close(now(), 0, "");
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
        if let Http3ClientEvent::HeaderReady { stream_id } = e {
            assert_eq!(stream_id, request_stream_id);
            let h = client.read_response_headers(stream_id);
            assert_eq!(h, Ok((vec![], true)));
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            client.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Close stream imemediately after headers.
    #[test]
    fn test_stream_fin_after_headers() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_RESPONSE_HEADER_ONLY_2);
        // ok NOW send fin
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv HeaderReady with headers and fin.
        let e = client.events().next().unwrap();
        if let Http3ClientEvent::HeaderReady { stream_id } = e {
            assert_eq!(stream_id, request_stream_id);
            let (h, fin) = client.read_response_headers(stream_id).unwrap();
            check_response_header_2(h);
            assert_eq!(fin, true);
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            client.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers, read headers and than close stream.
    // We should get HeaderReady and a DataReadable
    #[test]
    fn test_stream_fin_after_headers_are_read_wo_data_frame() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        // Send some good data wo fin
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_RESPONSE_HEADER_ONLY_2);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv headers wo fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let (h, fin) = client.read_response_headers(stream_id).unwrap();
                    check_response_header_2(h);
                    assert_eq!(fin, false);
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
                    let mut buf = [0u8; 100];
                    let res = client.read_response_data(now(), stream_id, &mut buf);
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
            client.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    // Send headers and an empty data frame, then close the stream.
    // We should only recv HeadersReady event.
    #[test]
    fn test_stream_fin_after_headers_and_a_empty_data_frame() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send headers.
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_RESPONSE_HEADER_ONLY_2);
        // Send an empty data frame.
        let _ = server.conn.stream_send(request_stream_id, &[0x00, 0x00]);
        // ok NOW send fin
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv HeaderReady with fin.
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let (h, fin) = client.read_response_headers(stream_id).unwrap();
                    check_response_header_2(h);
                    assert_eq!(fin, true);
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
            client.read_response_data(now(), 0, &mut buf),
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
            .stream_send(request_stream_id, HTTP_RESPONSE_HEADER_ONLY_2);
        // Send an empty data frame.
        let _ = server.conn.stream_send(request_stream_id, &[0x00, 0x00]);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv headers wo fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let (h, fin) = client.read_response_headers(stream_id).unwrap();
                    check_response_header_2(h);
                    assert_eq!(fin, false);
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
                    let mut buf = [0u8; 100];
                    let res = client.read_response_data(now(), stream_id, &mut buf);
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
            client.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_stream_fin_after_a_data_frame() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);
        // Send some good data wo fin
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_2);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Recv some good data wo fin
        while let Some(e) = client.next_event() {
            match e {
                Http3ClientEvent::HeaderReady { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let (h, fin) = client.read_response_headers(stream_id).unwrap();
                    check_response_header_2(h);
                    assert_eq!(fin, false);
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    assert_eq!(stream_id, request_stream_id);
                    let mut buf = [0u8; 100];
                    let res = client.read_response_data(now(), stream_id, &mut buf);
                    let (len, fin) = res.expect("should have data");
                    assert_eq!(len, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                    assert_eq!(&buf[..len], EXPECTED_RESPONSE_DATA_2_FRAME_1);
                    assert_eq!(fin, false);
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
            let mut buf = [0u8; 100];
            let res = client.read_response_data(now(), stream_id, &mut buf);
            let (len, fin) = res.expect("should read");
            assert_eq!(0, len);
            assert_eq!(fin, true);
        } else {
            panic!("wrong event type");
        }

        // Stream should now be closed and gone
        let mut buf = [0u8; 100];
        assert_eq!(
            client.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_multiple_data_frames() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send two data frames with fin
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_1);
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Read first frame
        match client.events().nth(1).unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let (len, fin) = client
                    .read_response_data(now(), stream_id, &mut buf)
                    .unwrap();
                assert_eq!(len, EXPECTED_RESPONSE_DATA_1_FRAME_1.len());
                assert_eq!(&buf[..len], EXPECTED_RESPONSE_DATA_1_FRAME_1);
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
        match client.events().next().unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let (len, fin) = client
                    .read_response_data(now(), stream_id, &mut buf)
                    .unwrap();
                assert_eq!(len, EXPECTED_RESPONSE_DATA_1_FRAME_2.len());
                assert_eq!(&buf[..len], EXPECTED_RESPONSE_DATA_1_FRAME_2);
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
            client.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_receive_grease_before_response() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Construct an unknown frame.
        const UNKNOWN_FRAME_LEN: usize = 832;
        let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
        enc.encode_varint(1028u64); // Arbitrary type.
        enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
        let mut buf: Vec<_> = enc.into();
        buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
        let _ = server.conn.stream_send(request_stream_id, &buf).unwrap();

        // Send a headers and a data frame with fin
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_2);
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        client.process(None, now());

        // Read first frame
        match client.events().nth(1).unwrap() {
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, request_stream_id);
                let mut buf = [0u8; 100];
                let (len, fin) = client
                    .read_response_data(now(), stream_id, &mut buf)
                    .unwrap();
                assert_eq!(len, EXPECTED_RESPONSE_DATA_2_FRAME_1.len());
                assert_eq!(&buf[..len], EXPECTED_RESPONSE_DATA_2_FRAME_1);
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
            client.read_response_data(now(), 0, &mut buf),
            Err(Error::InvalidStreamId)
        );
    }

    #[test]
    fn test_read_frames_header_blocked() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        server.encoder.set_max_capacity(100).unwrap();
        server.encoder.set_max_blocked_streams(100).unwrap();

        let headers = vec![
            (String::from(":status"), String::from("200")),
            (String::from("my-header"), String::from("my-header")),
            (String::from("content-length"), String::from("3")),
        ];
        let encoded_headers = server
            .encoder
            .encode_header_block(&headers, request_stream_id);
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        let d_frame = HFrame::Data { len: 3 };
        d_frame.encode(&mut d);
        d.encode(&[0x61, 0x62, 0x63]);
        let _ = server.conn.stream_send(request_stream_id, &d[..]);
        server.conn.stream_close_send(request_stream_id).unwrap();

        // Send response before sending encoder instructions.
        let out = server.conn.process(None, now());
        let _out = client.process(out.dgram(), now());

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!client.events().any(header_ready_event));

        // Send encoder instructions to unblock the stream.
        server.encoder.send(&mut server.conn).unwrap();

        let out = server.conn.process(None, now());
        let _out = client.process(out.dgram(), now());
        let _out = client.process(None, now());

        let mut recv_header = false;
        let mut recv_data = false;
        // Now the stream is unblocked and both headers and data will be consumed.
        while let Some(e) = client.next_event() {
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

    #[test]
    fn test_read_frames_header_blocked_with_fin_after_headers() {
        let (mut hconn, mut server, request_stream_id) = connect_and_send_request(true);

        server.encoder.set_max_capacity(100).unwrap();
        server.encoder.set_max_blocked_streams(100).unwrap();

        let headers = vec![
            (String::from(":status"), String::from("200")),
            (String::from("my-header"), String::from("my-header")),
            (String::from("content-length"), String::from("0")),
        ];
        let encoded_headers = server
            .encoder
            .encode_header_block(&headers, request_stream_id);
        let hframe = HFrame::Headers {
            header_block: encoded_headers.to_vec(),
        };
        let mut d = Encoder::default();
        hframe.encode(&mut d);

        let _ = server.conn.stream_send(request_stream_id, &d[..]);
        server.conn.stream_close_send(request_stream_id).unwrap();

        // Send response before sending encoder instructions.
        let out = server.conn.process(None, now());
        let _out = hconn.process(out.dgram(), now());

        let header_ready_event = |e| matches!(e, Http3ClientEvent::HeaderReady { .. });
        assert!(!hconn.events().any(header_ready_event));

        // Send encoder instructions to unblock the stream.
        server.encoder.send(&mut server.conn).unwrap();

        let out = server.conn.process(None, now());
        let _out = hconn.process(out.dgram(), now());
        let _out = hconn.process(None, now());

        let mut recv_header = false;
        // Now the stream is unblocked. After headers we will receive a fin.
        while let Some(e) = hconn.next_event() {
            if let Http3ClientEvent::HeaderReady { stream_id } = e {
                assert_eq!(stream_id, request_stream_id);
                let (h, fin) = hconn.read_response_headers(stream_id).unwrap();
                assert_eq!(h, headers);
                assert_eq!(fin, true);
                recv_header = true;
            } else {
                panic!("event {:?}", e);
            }
        }
        assert!(recv_header);
    }

    fn check_control_qpack_request_streams_resumption(
        server: &mut Connection,
        expect_encoder_stream_data: &[u8],
        expect_request: bool,
    ) {
        let mut control_stream = false;
        let mut qpack_decoder_stream = false;
        let mut qpack_encoder_stream = false;
        let mut request = false;
        while let Some(e) = server.next_event() {
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => {
                    assert!(matches!(stream_id, 2 | 6 | 10 | 0));
                    if stream_id == 0 {
                        assert_eq!(stream_type, StreamType::BiDi);
                    } else {
                        assert_eq!(stream_type, StreamType::UniDi);
                    }
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    if stream_id == 2 {
                        // the control stream
                        read_and_check_stream_data(server, stream_id, CONTROL_STREAM_DATA, false);
                        control_stream = true;
                    } else if stream_id == 6 {
                        let mut buf = [0u8; 100];
                        let (amount, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
                        assert_eq!(fin, false);
                        assert_eq!(amount, expect_encoder_stream_data.len());
                        assert_eq!(&buf[..amount], expect_encoder_stream_data);
                        qpack_encoder_stream = true;
                    } else if stream_id == 10 {
                        let mut buf = [0u8; 100];
                        let (amount, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
                        assert_eq!(fin, false);
                        assert_eq!(amount, DECODER_STREAM_DATA.len());
                        assert_eq!(&buf[..amount], DECODER_STREAM_DATA);
                        qpack_decoder_stream = true;
                    } else if stream_id == 0 {
                        assert!(expect_request);
                        read_and_check_stream_data(
                            server,
                            stream_id,
                            EXPECTED_REQUEST_HEADER_FRAME,
                            true,
                        );
                        request = true;
                    } else {
                        panic!("unexpected event");
                    }
                }
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    assert!(matches!(stream_id, 2 | 6 | 10));
                }
                ConnectionEvent::StateChange(_) => (),
                _ => panic!("unexpected event"),
            }
        }
        assert!(control_stream);
        assert!(qpack_encoder_stream);
        assert!(qpack_decoder_stream);
        assert_eq!(request, expect_request);
    }

    fn exchange_token(client: &mut Http3Client, server: &mut Connection) -> Vec<u8> {
        server.send_ticket(now(), &[]).expect("can send ticket");
        let out = server.process_output(now());
        assert!(out.as_dgram_ref().is_some());
        client.process_input(out.dgram().unwrap(), now());
        assert_eq!(client.state(), Http3State::Connected);
        client.resumption_token().expect("should have token")
    }

    fn start_with_0rtt() -> (Http3Client, TestServer) {
        let (mut client, mut server) = connect();
        let token = exchange_token(&mut client, &mut server.conn);

        let mut client = default_http3_client();

        let server = make_default_server();

        assert_eq!(client.state(), Http3State::Initializing);
        client
            .set_resumption_token(now(), &token)
            .expect("Set resumption token.");

        assert_eq!(client.state(), Http3State::ZeroRtt);

        (client, server)
    }

    #[test]
    fn zero_rtt_negotiated() {
        let (mut client, mut server) = start_with_0rtt();

        let out = client.process(None, now());

        assert_eq!(client.state(), Http3State::ZeroRtt);
        assert_eq!(*server.conn.state(), State::WaitInitial);
        let out = server.conn.process(out.dgram(), now());

        // Check that control and qpack streams are received and a
        // SETTINGS frame has been received.
        // Also qpack encoder stream will send "change capacity" instruction because it has
        // the peer settings already.
        check_control_qpack_request_streams_resumption(
            &mut server.conn,
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
            false,
        );

        assert_eq!(*server.conn.state(), State::Handshaking);
        let out = client.process(out.dgram(), now());
        assert_eq!(client.state(), Http3State::Connected);

        let _out = server.conn.process(out.dgram(), now());
        assert!(server.conn.state().connected());

        assert!(client.tls_info().unwrap().resumed());
        assert!(server.conn.tls_info().unwrap().resumed());
    }

    #[test]
    fn zero_rtt_send_request() {
        let (mut client, mut server) = start_with_0rtt();

        let request_stream_id = make_request(&mut client, true);
        assert_eq!(request_stream_id, 0);

        let out = client.process(None, now());

        assert_eq!(client.state(), Http3State::ZeroRtt);
        assert_eq!(*server.conn.state(), State::WaitInitial);
        let out = server.conn.process(out.dgram(), now());

        // Check that control and qpack streams are received and a
        // SETTINGS frame has been received.
        // Also qpack encoder stream will send "change capacity" instruction because it has
        // the peer settings already.
        check_control_qpack_request_streams_resumption(
            &mut server.conn,
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
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
            .fetch("GET", "https", "something.com", "/", &[])
            .is_err());
    }

    #[test]
    fn zero_rtt_send_reject() {
        let (mut client, mut server) = connect();
        let token = exchange_token(&mut client, &mut server.conn);

        let mut client = default_http3_client();

        // Using a freshly initialized anti-replay context
        // should result in the server rejecting 0-RTT.
        let ar = AntiReplay::new(now(), test_fixture::ANTI_REPLAY_WINDOW, 1, 3)
            .expect("setup anti-replay");
        let mut server = Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN,
            &ar,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(10))),
        )
        .unwrap();

        assert_eq!(client.state(), Http3State::Initializing);
        client
            .set_resumption_token(now(), &token)
            .expect("Set resumption token.");

        // Send ClientHello.
        let client_hs = client.process(None, now());
        assert!(client_hs.as_dgram_ref().is_some());

        // Create a request
        let request_stream_id = make_request(&mut client, false);
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
        let _ = server.process(client_out.dgram(), now());
        check_control_qpack_streams(&mut server);

        // Check that we can send a request and that the stream_id starts again from 0.
        let request_stream_id = make_request(&mut client, false);
        assert_eq!(request_stream_id, 0);
    }

    // Connect to a server, get token and reconnect using 0-rtt. Seerver sends new Settings.
    fn zero_rtt_change_settings(
        original_settings: &[HSetting],
        resumption_settings: &[HSetting],
        expected_client_state: Http3State,
        expected_encoder_stream_data: &[u8],
    ) {
        let mut client = default_http3_client();
        let mut server = make_server(original_settings);
        // Connect and get a token
        connect_with(&mut client, &mut server);
        let token = exchange_token(&mut client, &mut server.conn);

        let mut client = default_http3_client();
        let mut server = make_server(resumption_settings);
        assert_eq!(client.state(), Http3State::Initializing);
        client
            .set_resumption_token(now(), &token)
            .expect("Set resumption token.");
        assert_eq!(client.state(), Http3State::ZeroRtt);
        let out = client.process(None, now());

        assert_eq!(client.state(), Http3State::ZeroRtt);
        assert_eq!(*server.conn.state(), State::WaitInitial);
        let out = server.conn.process(out.dgram(), now());

        // Check that control and qpack streams anda SETTINGS frame are received.
        // Also qpack encoder stream will send "change capacity" instruction because it has
        // the peer settings already.
        check_control_qpack_request_streams_resumption(
            &mut server.conn,
            expected_encoder_stream_data,
            false,
        );

        assert_eq!(*server.conn.state(), State::Handshaking);
        let out = client.process(out.dgram(), now());
        assert_eq!(client.state(), Http3State::Connected);

        let _out = server.conn.process(out.dgram(), now());
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

        assert_eq!(client.state(), expected_client_state);
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
            Http3State::Connected,
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
            Http3State::Closing(CloseError::Application(265)),
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
            Http3State::Closing(CloseError::Application(265)),
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
            Http3State::Connected,
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
            Http3State::Closing(CloseError::Application(514)),
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
            Http3State::Closing(CloseError::Application(265)),
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
            Http3State::Connected,
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
            Http3State::Closing(CloseError::Application(265)),
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
            Http3State::Connected,
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
            Http3State::Closing(CloseError::Application(265)),
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
            Http3State::Connected,
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
            Http3State::Connected,
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
            Http3State::Closing(CloseError::Application(265)),
            ENCODER_STREAM_DATA_WITH_CAP_INSTRUCTION,
        );
    }

    #[test]
    fn test_trailers_with_fin_after_headers() {
        // Make a new connection.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send HEADER frame.
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_HEADER_FRAME_0);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Check response headers.
        let mut response_headers = false;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::HeaderReady { stream_id } = e {
                assert_eq!(stream_id, request_stream_id);
                let (h, fin) = client.read_response_headers(stream_id).unwrap();
                check_response_header_0(h);
                assert_eq!(fin, false);
                response_headers = true;
            }
        }
        assert!(response_headers);

        // Send trailers
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_HEADER_FRAME_0);
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        let events: Vec<Http3ClientEvent> = client.events().collect();

        // We already had HeaderReady
        let header_ready: fn(&Http3ClientEvent) -> _ =
            |e| matches!(*e, Http3ClientEvent::HeaderReady { .. });
        assert!(!events.iter().any(header_ready));

        // Check that we have a DataReady event. Reading from the stream will return fin=true.
        let data_readable: fn(&Http3ClientEvent) -> _ =
            |e| matches!(*e, Http3ClientEvent::DataReadable { .. });
        assert!(events.iter().any(data_readable));
        let mut buf = [0u8; 100];
        let (len, fin) = client
            .read_response_data(now(), request_stream_id, &mut buf)
            .unwrap();
        assert_eq!(0, len);
        assert_eq!(fin, true)
    }

    #[test]
    fn test_trailers_with_later_fin_after_headers() {
        // Make a new connection.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send HEADER frame.
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_HEADER_FRAME_0);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Check response headers.
        let mut response_headers = false;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::HeaderReady { stream_id } = e {
                assert_eq!(stream_id, request_stream_id);
                let (h, fin) = client.read_response_headers(stream_id).unwrap();
                check_response_header_0(h);
                assert_eq!(fin, false);
                response_headers = true;
            }
        }
        assert!(response_headers);

        // Send trailers
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_HEADER_FRAME_0);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

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
        let data_readable: fn(&Http3ClientEvent) -> _ =
            |e| matches!(*e, Http3ClientEvent::DataReadable { .. });
        assert!(events.iter().any(data_readable));
        let mut buf = [0u8; 100];
        let (len, fin) = client
            .read_response_data(now(), request_stream_id, &mut buf)
            .unwrap();
        assert_eq!(0, len);
        assert_eq!(fin, true);
    }

    #[test]
    fn test_data_after_trailers_after_headers() {
        // Make a new connection.
        let (mut client, mut server, request_stream_id) = connect_and_send_request(true);

        // Send HEADER frame.
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_HEADER_FRAME_0);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Check response headers.
        let mut response_headers = false;
        while let Some(e) = client.next_event() {
            if let Http3ClientEvent::HeaderReady { stream_id } = e {
                assert_eq!(stream_id, request_stream_id);
                let (h, fin) = client.read_response_headers(stream_id).unwrap();
                check_response_header_0(h);
                assert_eq!(fin, false);
                response_headers = true;
            }
        }
        assert!(response_headers);

        // Send trailers
        let _ = server
            .conn
            .stream_send(request_stream_id, HTTP_HEADER_FRAME_0);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        // Check that we do not have a DataReady event.
        let data_readable = |e| matches!(e, Http3ClientEvent::DataReadable { .. });
        assert!(!client.events().any(data_readable));

        // Send Data frame.
        let _ = server.conn.stream_send(
            request_stream_id,
            &[
                // data frame
                0x0, 0x3, 0x61, 0x62, 0x63,
            ],
        );

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());
        assert_closed(&client, Error::HttpFrameUnexpected);
    }

    #[test]
    fn transport_stream_readable_event_after_all_data() {
        let (mut client, mut server, request_stream_id) = connect_and_send_request(false);

        // Send headers.
        let _ = server.conn.stream_send(request_stream_id, HTTP_RESPONSE_2);

        let out = server.conn.process(None, now());
        client.process(out.dgram(), now());

        // Send an empty data frame.
        let _ = server.conn.stream_send(request_stream_id, &[0x00, 0x00]);
        // ok NOW send fin
        server.conn.stream_close_send(request_stream_id).unwrap();

        let out = server.conn.process(None, now());
        client.process_input(out.dgram().unwrap(), now());

        let (h, fin) = client.read_response_headers(request_stream_id).unwrap();
        check_response_header_2(h);
        assert_eq!(fin, false);
        let mut buf = [0u8; 100];
        assert_eq!(
            client.read_response_data(now(), 0, &mut buf),
            Ok((3, false))
        );

        client.process(None, now());
    }
}
