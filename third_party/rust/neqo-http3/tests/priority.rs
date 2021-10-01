// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_common::event::Provider;

use neqo_crypto::AuthenticationStatus;
use neqo_http3::{
    Header, Http3Client, Http3ClientEvent, Http3Parameters, Http3Server, Http3ServerEvent,
    Http3State, Priority,
};
use neqo_qpack::QpackSettings;
use neqo_transport::ConnectionParameters;
use std::cell::RefCell;

use std::rc::Rc;

use std::time::Instant;
use test_fixture::*;

const DEFAULT_SETTINGS: QpackSettings = QpackSettings {
    max_table_size_encoder: 65536,
    max_table_size_decoder: 65536,
    max_blocked_streams: 100,
};

pub fn default_http3_client() -> Http3Client {
    fixture_init();
    Http3Client::new(
        DEFAULT_SERVER_NAME,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        addr(),
        addr(),
        ConnectionParameters::default(),
        &Http3Parameters {
            qpack_settings: DEFAULT_SETTINGS,
            max_concurrent_push_streams: 5,
        },
        now(),
    )
    .expect("create a default client")
}

pub fn default_http3_server() -> Http3Server {
    Http3Server::new(
        now(),
        DEFAULT_KEYS,
        DEFAULT_ALPN_H3,
        anti_replay(),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        DEFAULT_SETTINGS,
        None,
    )
    .expect("create a server")
}

fn exchange_packets(client: &mut Http3Client, server: &mut Http3Server) {
    let mut out = None;
    let mut client_data;
    loop {
        out = client.process(out, now()).dgram();
        client_data = out.is_none();
        out = server.process(out, now()).dgram();
        if out.is_none() && client_data {
            break;
        }
    }
}

// Perform only Quic transport handshake.
fn connect_with(client: &mut Http3Client, server: &mut Http3Server) {
    assert_eq!(client.state(), Http3State::Initializing);
    let out = client.process(None, now());
    assert_eq!(client.state(), Http3State::Initializing);

    let out = server.process(out.dgram(), now());
    let out = client.process(out.dgram(), now());
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
    assert!(client.events().any(authentication_needed));
    client.authenticated(AuthenticationStatus::Ok, now());

    let out = client.process(out.dgram(), now());
    let connected = |e| matches!(e, Http3ClientEvent::StateChange(Http3State::Connected));
    assert!(client.events().any(connected));

    assert_eq!(client.state(), Http3State::Connected);
    // Exchange H3 setttings
    let out = server.process(out.dgram(), now());
    let out = client.process(out.dgram(), now());
    let out = server.process(out.dgram(), now());
    let out = client.process(out.dgram(), now());
    let _ = server.process(out.dgram(), now());
}

fn connect() -> (Http3Client, Http3Server) {
    let mut client = default_http3_client();
    let mut server = default_http3_server();
    connect_with(&mut client, &mut server);
    (client, server)
}

#[test]
fn priority_update() {
    let (mut client, mut server) = connect();
    let stream_id = client
        .fetch(
            Instant::now(),
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::new(4, true),
        )
        .unwrap();
    exchange_packets(&mut client, &mut server);

    // get event of the above request, skipping events of the connection setup
    let header_event = loop {
        let event = server.next_event().unwrap();
        if matches!(event, Http3ServerEvent::Headers { .. }) {
            break event;
        }
    };

    match header_event {
        Http3ServerEvent::Headers {
            request: _,
            headers,
            fin,
        } => {
            let expected_headers = vec![
                Header::new(":method", "GET"),
                Header::new(":scheme", "https"),
                Header::new(":authority", "something.com"),
                Header::new(":path", "/"),
                Header::new("priority", "u=4,i"),
            ];
            assert_eq!(headers, expected_headers);
            assert!(!fin);
        }
        other => panic!("unexpected server event: {:?}", other),
    }

    let update_priority = Priority::new(3, false);
    client.priority_update(stream_id, update_priority).unwrap();
    exchange_packets(&mut client, &mut server);

    let priority_event = loop {
        let event = server.next_event().unwrap();
        if matches!(event, Http3ServerEvent::PriorityUpdate { .. }) {
            break event;
        }
    };

    match priority_event {
        Http3ServerEvent::PriorityUpdate {
            stream_id: update_id,
            priority,
        } => {
            assert_eq!(update_id, stream_id);
            assert_eq!(priority, update_priority);
        }
        other => panic!("unexpected server event: {:?}", other),
    }
}

#[test]
fn priority_update_dont_send_for_cancelled_stream() {
    let (mut client, mut server) = connect();
    let stream_id = client
        .fetch(
            Instant::now(),
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::new(5, false),
        )
        .unwrap();
    exchange_packets(&mut client, &mut server);

    let update_priority = Priority::new(6, false);
    client.priority_update(stream_id, update_priority).unwrap();
    client.stream_reset(stream_id, 11).unwrap();
    exchange_packets(&mut client, &mut server);

    while let Some(event) = server.next_event() {
        if matches!(event, Http3ServerEvent::PriorityUpdate { .. }) {
            panic!("Priority update sent on cancelled stream");
        }
    }
}
