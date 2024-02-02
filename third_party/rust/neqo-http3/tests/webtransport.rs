// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{cell::RefCell, rc::Rc};

use neqo_common::{event::Provider, Header};
use neqo_crypto::AuthenticationStatus;
use neqo_http3::{
    Http3Client, Http3ClientEvent, Http3OrWebTransportStream, Http3Parameters, Http3Server,
    Http3ServerEvent, Http3State, WebTransportEvent, WebTransportRequest, WebTransportServerEvent,
    WebTransportSessionAcceptAction,
};
use neqo_transport::{StreamId, StreamType};
use test_fixture::{
    addr, anti_replay, fixture_init, now, CountingConnectionIdGenerator, DEFAULT_ALPN_H3,
    DEFAULT_KEYS, DEFAULT_SERVER_NAME,
};

fn connect() -> (Http3Client, Http3Server) {
    fixture_init();
    let mut client = Http3Client::new(
        DEFAULT_SERVER_NAME,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        addr(),
        addr(),
        Http3Parameters::default().webtransport(true),
        now(),
    )
    .expect("create a default client");
    let mut server = Http3Server::new(
        now(),
        DEFAULT_KEYS,
        DEFAULT_ALPN_H3,
        anti_replay(),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        Http3Parameters::default().webtransport(true),
        None,
    )
    .expect("create a server");
    assert_eq!(client.state(), Http3State::Initializing);
    let out = client.process(None, now());
    assert_eq!(client.state(), Http3State::Initializing);

    let out = server.process(out.as_dgram_ref(), now());
    let out = client.process(out.as_dgram_ref(), now());
    let out = server.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_none());

    let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
    assert!(client.events().any(authentication_needed));
    client.authenticated(AuthenticationStatus::Ok, now());

    let mut out = client.process(out.as_dgram_ref(), now()).dgram();
    let connected = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::Connected));
    assert!(client.events().any(connected));

    assert_eq!(client.state(), Http3State::Connected);

    // Exchange H3 setttings
    loop {
        out = server.process(out.as_ref(), now()).dgram();
        let dgram_present = out.is_some();
        out = client.process(out.as_ref(), now()).dgram();
        if out.is_none() && !dgram_present {
            break;
        }
    }
    (client, server)
}

fn exchange_packets(client: &mut Http3Client, server: &mut Http3Server) {
    let mut out = None;
    loop {
        out = client.process(out.as_ref(), now()).dgram();
        out = server.process(out.as_ref(), now()).dgram();
        if out.is_none() {
            break;
        }
    }
}

fn create_wt_session(client: &mut Http3Client, server: &mut Http3Server) -> WebTransportRequest {
    let wt_session_id = client
        .webtransport_create_session(now(), &("https", "something.com", "/"), &[])
        .unwrap();
    exchange_packets(client, server);

    let mut wt_server_session = None;
    while let Some(event) = server.next_event() {
        match event {
            Http3ServerEvent::WebTransport(WebTransportServerEvent::NewSession {
                mut session,
                headers,
            }) => {
                assert!(
                    headers
                        .iter()
                        .any(|h| h.name() == ":method" && h.value() == "CONNECT")
                        && headers
                            .iter()
                            .any(|h| h.name() == ":protocol" && h.value() == "webtransport")
                );
                session
                    .response(&WebTransportSessionAcceptAction::Accept)
                    .unwrap();
                wt_server_session = Some(session);
            }
            Http3ServerEvent::Data { .. } => {
                panic!("There should not be any data events!");
            }
            _ => {}
        }
    }

    exchange_packets(client, server);

    let wt_session_negotiated_event = |e| {
        matches!(
            e,
            Http3ClientEvent::WebTransport(WebTransportEvent::Session{
                stream_id,
                status,
                headers,
            }) if (
                stream_id == wt_session_id &&
                status == 200 &&
                headers.contains(&Header::new(":status", "200"))
            )
        )
    };
    assert!(client.events().any(wt_session_negotiated_event));

    let wt_server_session = wt_server_session.unwrap();
    assert_eq!(wt_session_id, wt_server_session.stream_id());
    wt_server_session
}

fn send_data_client(
    client: &mut Http3Client,
    server: &mut Http3Server,
    wt_stream_id: StreamId,
    data: &[u8],
) {
    assert_eq!(client.send_data(wt_stream_id, data).unwrap(), data.len());
    exchange_packets(client, server);
}

fn send_data_server(
    client: &mut Http3Client,
    server: &mut Http3Server,
    wt_stream: &mut Http3OrWebTransportStream,
    data: &[u8],
) {
    assert_eq!(wt_stream.send_data(data).unwrap(), data.len());
    exchange_packets(client, server);
}

fn receive_data_client(
    client: &mut Http3Client,
    expected_stream_id: StreamId,
    new_stream: bool,
    expected_data: &[u8],
    expected_fin: bool,
) {
    let mut new_stream_received = false;
    let mut data_received = false;
    while let Some(event) = client.next_event() {
        match event {
            Http3ClientEvent::WebTransport(WebTransportEvent::NewStream { stream_id, .. }) => {
                assert_eq!(stream_id, expected_stream_id);
                new_stream_received = true;
            }
            Http3ClientEvent::DataReadable { stream_id } => {
                assert_eq!(stream_id, expected_stream_id);
                let mut buf = [0; 100];
                let (amount, fin) = client.read_data(now(), stream_id, &mut buf).unwrap();
                assert_eq!(fin, expected_fin);
                assert_eq!(amount, expected_data.len());
                assert_eq!(&buf[..amount], expected_data);
                data_received = true;
            }
            _ => {}
        }
    }
    assert!(data_received);
    assert_eq!(new_stream, new_stream_received);
}

fn receive_data_server(
    client: &mut Http3Client,
    server: &mut Http3Server,
    stream_id: StreamId,
    new_stream: bool,
    expected_data: &[u8],
    expected_fin: bool,
) -> Http3OrWebTransportStream {
    exchange_packets(client, server);
    let mut new_stream_received = false;
    let mut data_received = false;
    let mut wt_stream = None;
    let mut stream_closed = false;
    let mut recv_data = Vec::new();
    while let Some(event) = server.next_event() {
        match event {
            Http3ServerEvent::WebTransport(WebTransportServerEvent::NewStream(request)) => {
                assert_eq!(stream_id, request.stream_id());
                new_stream_received = true;
            }
            Http3ServerEvent::Data {
                mut data,
                fin,
                stream,
            } => {
                recv_data.append(&mut data);
                stream_closed = fin;
                data_received = true;
                wt_stream = Some(stream);
            }
            _ => {}
        }
    }
    assert_eq!(&recv_data[..], expected_data);
    assert!(data_received);
    assert_eq!(new_stream, new_stream_received);
    assert_eq!(stream_closed, expected_fin);
    wt_stream.unwrap()
}

#[test]
fn wt_client_stream_uni() {
    const BUF_CLIENT: &[u8] = &[0; 10];

    let (mut client, mut server) = connect();
    let wt_session = create_wt_session(&mut client, &mut server);
    let wt_stream = client
        .webtransport_create_stream(wt_session.stream_id(), StreamType::UniDi)
        .unwrap();
    send_data_client(&mut client, &mut server, wt_stream, BUF_CLIENT);
    exchange_packets(&mut client, &mut server);
    receive_data_server(&mut client, &mut server, wt_stream, true, BUF_CLIENT, false);
}

#[test]
fn wt_client_stream_bidi() {
    const BUF_CLIENT: &[u8] = &[0; 10];
    const BUF_SERVER: &[u8] = &[1; 20];

    let (mut client, mut server) = connect();
    let wt_session = create_wt_session(&mut client, &mut server);
    let wt_client_stream = client
        .webtransport_create_stream(wt_session.stream_id(), StreamType::BiDi)
        .unwrap();
    send_data_client(&mut client, &mut server, wt_client_stream, BUF_CLIENT);
    let mut wt_server_stream = receive_data_server(
        &mut client,
        &mut server,
        wt_client_stream,
        true,
        BUF_CLIENT,
        false,
    );
    send_data_server(&mut client, &mut server, &mut wt_server_stream, BUF_SERVER);
    receive_data_client(&mut client, wt_client_stream, false, BUF_SERVER, false);
}

#[test]
fn wt_server_stream_uni() {
    const BUF_SERVER: &[u8] = &[2; 30];

    let (mut client, mut server) = connect();
    let mut wt_session = create_wt_session(&mut client, &mut server);
    let mut wt_server_stream = wt_session.create_stream(StreamType::UniDi).unwrap();
    send_data_server(&mut client, &mut server, &mut wt_server_stream, BUF_SERVER);
    receive_data_client(
        &mut client,
        wt_server_stream.stream_id(),
        true,
        BUF_SERVER,
        false,
    );
}

#[test]
fn wt_server_stream_bidi() {
    const BUF_CLIENT: &[u8] = &[0; 10];
    const BUF_SERVER: &[u8] = &[1; 20];

    let (mut client, mut server) = connect();
    let mut wt_session = create_wt_session(&mut client, &mut server);
    let mut wt_server_stream = wt_session.create_stream(StreamType::BiDi).unwrap();
    send_data_server(&mut client, &mut server, &mut wt_server_stream, BUF_SERVER);
    receive_data_client(
        &mut client,
        wt_server_stream.stream_id(),
        true,
        BUF_SERVER,
        false,
    );
    send_data_client(
        &mut client,
        &mut server,
        wt_server_stream.stream_id(),
        BUF_CLIENT,
    );
    assert_eq!(
        receive_data_server(
            &mut client,
            &mut server,
            wt_server_stream.stream_id(),
            false,
            BUF_CLIENT,
            false
        )
        .stream_id(),
        wt_server_stream.stream_id()
    );
}
