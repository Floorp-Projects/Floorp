// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(unused_assignments)]

use std::{
    mem,
    time::{Duration, Instant},
};

use neqo_common::{event::Provider, qtrace, Datagram};
use neqo_crypto::{AuthenticationStatus, ResumptionToken};
use neqo_http3::{
    Header, Http3Client, Http3ClientEvent, Http3OrWebTransportStream, Http3Parameters, Http3Server,
    Http3ServerEvent, Http3State, Priority,
};
use neqo_transport::{CloseReason, ConnectionParameters, Error, Output, StreamType};
use test_fixture::*;

const RESPONSE_DATA: &[u8] = &[0x61, 0x62, 0x63];

fn receive_request(server: &mut Http3Server) -> Option<Http3OrWebTransportStream> {
    while let Some(event) = server.next_event() {
        if let Http3ServerEvent::Headers {
            stream,
            headers,
            fin,
        } = event
        {
            assert_eq!(
                &headers,
                &[
                    Header::new(":method", "GET"),
                    Header::new(":scheme", "https"),
                    Header::new(":authority", "something.com"),
                    Header::new(":path", "/")
                ]
            );
            assert!(fin);

            return Some(stream);
        }
    }
    None
}

fn set_response(request: &mut Http3OrWebTransportStream) {
    request
        .send_headers(&[
            Header::new(":status", "200"),
            Header::new("content-length", "3"),
        ])
        .unwrap();
    request.send_data(RESPONSE_DATA).unwrap();
    request.stream_close_send().unwrap();
}

fn process_server_events(server: &mut Http3Server) {
    let mut request = receive_request(server).unwrap();
    set_response(&mut request);
}

fn process_client_events(conn: &mut Http3Client) {
    let mut response_header_found = false;
    let mut response_data_found = false;
    while let Some(event) = conn.next_event() {
        match event {
            Http3ClientEvent::HeaderReady { headers, fin, .. } => {
                assert_eq!(
                    &headers,
                    &[
                        Header::new(":status", "200"),
                        Header::new("content-length", "3"),
                    ]
                );
                assert!(!fin);
                response_header_found = true;
            }
            Http3ClientEvent::DataReadable { stream_id } => {
                let mut buf = [0u8; 100];
                let (amount, fin) = conn.read_data(now(), stream_id, &mut buf).unwrap();
                assert!(fin);
                assert_eq!(amount, RESPONSE_DATA.len());
                assert_eq!(&buf[..RESPONSE_DATA.len()], RESPONSE_DATA);
                response_data_found = true;
            }
            _ => {}
        }
    }
    assert!(response_header_found);
    assert!(response_data_found);
}

fn connect_peers(hconn_c: &mut Http3Client, hconn_s: &mut Http3Server) -> Option<Datagram> {
    assert_eq!(hconn_c.state(), Http3State::Initializing);
    let out = hconn_c.process(None, now()); // Initial
    let out = hconn_s.process(out.as_dgram_ref(), now()); // Initial + Handshake
    let out = hconn_c.process(out.as_dgram_ref(), now()); // ACK
    mem::drop(hconn_s.process(out.as_dgram_ref(), now())); // consume ACK
    let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
    assert!(hconn_c.events().any(authentication_needed));
    hconn_c.authenticated(AuthenticationStatus::Ok, now());
    let out = hconn_c.process(None, now()); // Handshake
    assert_eq!(hconn_c.state(), Http3State::Connected);
    let out = hconn_s.process(out.as_dgram_ref(), now()); // Handshake
    let out = hconn_c.process(out.as_dgram_ref(), now());
    let out = hconn_s.process(out.as_dgram_ref(), now());
    // assert!(hconn_s.settings_received);
    let out = hconn_c.process(out.as_dgram_ref(), now());
    // assert!(hconn_c.settings_received);

    out.dgram()
}

fn connect_peers_with_network_propagation_delay(
    hconn_c: &mut Http3Client,
    hconn_s: &mut Http3Server,
    net_delay: u64,
) -> (Option<Datagram>, Instant) {
    let net_delay = Duration::from_millis(net_delay);
    assert_eq!(hconn_c.state(), Http3State::Initializing);
    let mut now = now();
    let out = hconn_c.process(None, now); // Initial
    now += net_delay;
    let out = hconn_s.process(out.as_dgram_ref(), now); // Initial + Handshake
    now += net_delay;
    let out = hconn_c.process(out.as_dgram_ref(), now); // ACK
    now += net_delay;
    let out = hconn_s.process(out.as_dgram_ref(), now); // consume ACK
    assert!(out.dgram().is_none());
    let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
    assert!(hconn_c.events().any(authentication_needed));
    now += net_delay;
    hconn_c.authenticated(AuthenticationStatus::Ok, now);
    let out = hconn_c.process(None, now); // Handshake
    assert_eq!(hconn_c.state(), Http3State::Connected);
    now += net_delay;
    let out = hconn_s.process(out.as_dgram_ref(), now); // HANDSHAKE_DONE
    now += net_delay;
    let out = hconn_c.process(out.as_dgram_ref(), now); // Consume HANDSHAKE_DONE, send control streams.
    now += net_delay;
    let out = hconn_s.process(out.as_dgram_ref(), now); // consume and send control streams.
    now += net_delay;
    let out = hconn_c.process(out.as_dgram_ref(), now); // consume control streams.
    (out.dgram(), now)
}

fn connect() -> (Http3Client, Http3Server, Option<Datagram>) {
    let mut hconn_c = default_http3_client();
    let mut hconn_s = default_http3_server();

    let out = connect_peers(&mut hconn_c, &mut hconn_s);
    (hconn_c, hconn_s, out)
}

fn exchange_packets(client: &mut Http3Client, server: &mut Http3Server, out_ex: Option<Datagram>) {
    let mut out = out_ex;
    loop {
        out = client.process(out.as_ref(), now()).dgram();
        out = server.process(out.as_ref(), now()).dgram();
        if out.is_none() {
            break;
        }
    }
}

#[test]
fn test_connect() {
    let (_hconn_c, _hconn_s, _d) = connect();
}

#[test]
fn test_fetch() {
    let (mut hconn_c, mut hconn_s, dgram) = connect();

    qtrace!("-----client");
    let req = hconn_c
        .fetch(
            now(),
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::default(),
        )
        .unwrap();
    assert_eq!(req, 0);
    hconn_c.stream_close_send(req).unwrap();
    let out = hconn_c.process(dgram.as_ref(), now());
    qtrace!("-----server");
    let out = hconn_s.process(out.as_dgram_ref(), now());
    mem::drop(hconn_c.process(out.as_dgram_ref(), now()));
    process_server_events(&mut hconn_s);
    let out = hconn_s.process(None, now());

    qtrace!("-----client");
    mem::drop(hconn_c.process(out.as_dgram_ref(), now()));
    let out = hconn_s.process(None, now());
    mem::drop(hconn_c.process(out.as_dgram_ref(), now()));
    process_client_events(&mut hconn_c);
}

#[test]
fn test_103_response() {
    let (mut hconn_c, mut hconn_s, dgram) = connect();

    let req = hconn_c
        .fetch(
            now(),
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::default(),
        )
        .unwrap();
    assert_eq!(req, 0);
    hconn_c.stream_close_send(req).unwrap();
    let out = hconn_c.process(dgram.as_ref(), now());

    let out = hconn_s.process(out.as_dgram_ref(), now());
    mem::drop(hconn_c.process(out.as_dgram_ref(), now()));
    let mut request = receive_request(&mut hconn_s).unwrap();

    let info_headers = [
        Header::new(":status", "103"),
        Header::new("link", "</style.css>; rel=preload; as=style"),
    ];
    // Send 103
    request.send_headers(&info_headers).unwrap();
    let out = hconn_s.process(None, now());

    mem::drop(hconn_c.process(out.as_dgram_ref(), now()));

    let info_headers_event = |e| {
        matches!(e, Http3ClientEvent::HeaderReady { headers,
                    interim,
                    fin, .. } if !fin && interim && headers.as_ref() == info_headers)
    };
    assert!(hconn_c.events().any(info_headers_event));

    set_response(&mut request);
    let out = hconn_s.process(None, now());
    mem::drop(hconn_c.process(out.as_dgram_ref(), now()));
    process_client_events(&mut hconn_c);
}

/// Test [`neqo_http3::SendMessage::send_data`] to set
/// [`neqo_transport::SendStream::set_writable_event_low_watermark`].
#[allow(clippy::cast_possible_truncation)]
#[test]
fn test_data_writable_events_low_watermark() -> Result<(), Box<dyn std::error::Error>> {
    const STREAM_LIMIT: u64 = 5000;
    const DATA_FRAME_HEADER_SIZE: usize = 3;

    // Create a client and a server.
    let mut hconn_c = http3_client_with_params(Http3Parameters::default().connection_parameters(
        ConnectionParameters::default().max_stream_data(StreamType::BiDi, false, STREAM_LIMIT),
    ));
    let mut hconn_s = default_http3_server();
    mem::drop(connect_peers(&mut hconn_c, &mut hconn_s));

    // Client sends GET to server.
    let stream_id = hconn_c.fetch(
        now(),
        "GET",
        &("https", "something.com", "/"),
        &[],
        Priority::default(),
    )?;
    hconn_c.stream_close_send(stream_id)?;
    exchange_packets(&mut hconn_c, &mut hconn_s, None);

    // Server receives GET and responds with headers.
    let mut request = receive_request(&mut hconn_s).unwrap();
    request.send_headers(&[Header::new(":status", "200")])?;

    // Sending these headers clears the server's send stream buffer and thus
    // emits a DataWritable event.
    exchange_packets(&mut hconn_c, &mut hconn_s, None);
    let data_writable = |e| {
        matches!(
            e,
            Http3ServerEvent::DataWritable {
                stream
            } if stream.stream_id() == stream_id
        )
    };
    assert!(hconn_s.events().any(data_writable));

    // Have server fill entire send buffer minus 1 byte.
    let all_but_one = request.available()? - DATA_FRAME_HEADER_SIZE - 1;
    let buf = vec![1; all_but_one];
    let sent = request.send_data(&buf)?;
    assert_eq!(sent, all_but_one);
    assert_eq!(request.available()?, 1);

    // Sending the buffered data clears the send stream buffer and thus emits a
    // DataWritable event.
    exchange_packets(&mut hconn_c, &mut hconn_s, None);
    assert!(hconn_s.events().any(data_writable));

    // Sending more fails, given that each data frame needs to be preceeded by a
    // header, i.e. needs more than 1 byte of send space to send 1 byte payload.
    assert_eq!(request.available()?, 1);
    assert_eq!(request.send_data(&buf)?, 0);

    // Have the client read all the pending data.
    let mut recv_buf = vec![0_u8; all_but_one];
    let (recvd, _) = hconn_c.read_data(now(), stream_id, &mut recv_buf)?;
    assert_eq!(sent, recvd);
    exchange_packets(&mut hconn_c, &mut hconn_s, None);

    // Expect the server's available send space to be back to the stream limit.
    assert_eq!(request.available()?, STREAM_LIMIT as usize);

    // Expect the server to emit a DataWritable event, even though it always had
    // at least 1 byte available to send, i.e. it never exhausted the entire
    // available send space.
    assert!(hconn_s.events().any(data_writable));

    Ok(())
}

#[test]
fn test_data_writable_events() {
    const STREAM_LIMIT: u64 = 5000;
    const DATA_AMOUNT: usize = 10000;

    let mut hconn_c = http3_client_with_params(Http3Parameters::default().connection_parameters(
        ConnectionParameters::default().max_stream_data(StreamType::BiDi, false, STREAM_LIMIT),
    ));
    let mut hconn_s = default_http3_server();

    mem::drop(connect_peers(&mut hconn_c, &mut hconn_s));

    // Create a request.
    let req = hconn_c
        .fetch(
            now(),
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::default(),
        )
        .unwrap();
    hconn_c.stream_close_send(req).unwrap();
    exchange_packets(&mut hconn_c, &mut hconn_s, None);

    let mut request = receive_request(&mut hconn_s).unwrap();

    request
        .send_headers(&[
            Header::new(":status", "200"),
            Header::new("content-length", DATA_AMOUNT.to_string()),
        ])
        .unwrap();

    // Send a lot of data
    let buf = &[1; DATA_AMOUNT];
    let mut sent = request.send_data(buf).unwrap();
    assert!(sent < DATA_AMOUNT);

    // Exchange packets and read the data on the client side.
    exchange_packets(&mut hconn_c, &mut hconn_s, None);
    let stream_id = request.stream_id();
    let mut recv_buf = [0_u8; DATA_AMOUNT];
    let (mut recvd, _) = hconn_c.read_data(now(), stream_id, &mut recv_buf).unwrap();
    assert_eq!(sent, recvd);
    exchange_packets(&mut hconn_c, &mut hconn_s, None);

    let data_writable = |e| {
        matches!(
            e,
            Http3ServerEvent::DataWritable {
                stream
            } if stream.stream_id() == stream_id
        )
    };
    // Make sure we have a DataWritable event.
    assert!(hconn_s.events().any(data_writable));
    // Data can be sent again.
    let s = request.send_data(&buf[sent..]).unwrap();
    assert!(s > 0);
    sent += s;

    // Exchange packets and read the data on the client side.
    exchange_packets(&mut hconn_c, &mut hconn_s, None);
    let (r, _) = hconn_c
        .read_data(now(), stream_id, &mut recv_buf[recvd..])
        .unwrap();
    recvd += r;
    exchange_packets(&mut hconn_c, &mut hconn_s, None);
    assert_eq!(sent, recvd);

    // One more DataWritable event.
    assert!(hconn_s.events().any(data_writable));
    // Send more data.
    let s = request.send_data(&buf[sent..]).unwrap();
    assert!(s > 0);
    sent += s;
    assert_eq!(sent, DATA_AMOUNT);

    exchange_packets(&mut hconn_c, &mut hconn_s, None);
    let (r, _) = hconn_c
        .read_data(now(), stream_id, &mut recv_buf[recvd..])
        .unwrap();
    recvd += r;

    // Make sure all data is received by the client.
    assert_eq!(recvd, DATA_AMOUNT);
    assert_eq!(&recv_buf, buf);
}

fn get_token(client: &mut Http3Client) -> ResumptionToken {
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

#[test]
fn zerortt() {
    let (mut hconn_c, _, dgram) = connect();
    let token = get_token(&mut hconn_c);

    // Create a new connection with a resumption token.
    let mut hconn_c = default_http3_client();
    hconn_c
        .enable_resumption(now(), &token)
        .expect("Set resumption token.");
    let mut hconn_s = default_http3_server();

    // Create a request.
    let req = hconn_c
        .fetch(
            now(),
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::default(),
        )
        .unwrap();
    hconn_c.stream_close_send(req).unwrap();

    let out = hconn_c.process(dgram.as_ref(), now());
    let out = hconn_s.process(out.as_dgram_ref(), now());

    let mut request_stream = None;
    let mut zerortt_state_change = false;
    while let Some(event) = hconn_s.next_event() {
        match event {
            Http3ServerEvent::Headers {
                stream,
                headers,
                fin,
            } => {
                assert_eq!(
                    &headers,
                    &[
                        Header::new(":method", "GET"),
                        Header::new(":scheme", "https"),
                        Header::new(":authority", "something.com"),
                        Header::new(":path", "/")
                    ]
                );
                assert!(fin);

                request_stream = Some(stream);
            }
            Http3ServerEvent::StateChange { state, .. } => {
                assert_eq!(state, Http3State::ZeroRtt);
                zerortt_state_change = true;
            }
            _ => {}
        }
    }
    assert!(zerortt_state_change);
    let mut request_stream = request_stream.unwrap();

    // Send a response
    set_response(&mut request_stream);

    // Receive the response
    exchange_packets(&mut hconn_c, &mut hconn_s, out.dgram());
    process_client_events(&mut hconn_c);
}

#[test]
/// When a client has an outstanding fetch, it will send keepalives.
/// Test that it will successfully run until the connection times out.
fn fetch_noresponse_will_idletimeout() {
    let mut hconn_c = default_http3_client();
    let mut hconn_s = default_http3_server();

    let (dgram, mut now) =
        connect_peers_with_network_propagation_delay(&mut hconn_c, &mut hconn_s, 10);

    qtrace!("-----client");
    let req = hconn_c
        .fetch(
            now,
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::default(),
        )
        .unwrap();
    assert_eq!(req, 0);
    hconn_c.stream_close_send(req).unwrap();
    let _out = hconn_c.process(dgram.as_ref(), now);
    qtrace!("-----server");

    let mut done = false;
    while !done {
        while let Some(event) = hconn_c.next_event() {
            if let Http3ClientEvent::StateChange(
                Http3State::Closing(error_code) | Http3State::Closed(error_code),
            ) = event
            {
                assert_eq!(error_code, CloseReason::Transport(Error::IdleTimeout));
                done = true;
            }
        }

        if let Output::Callback(t) = hconn_c.process_output(now) {
            now += t;
        }
    }
}
