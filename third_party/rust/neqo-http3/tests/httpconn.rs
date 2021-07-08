// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(unused_assignments)]

use neqo_common::{event::Provider, Datagram};
use neqo_crypto::AuthenticationStatus;
use neqo_http3::{
    Header, Http3Client, Http3ClientEvent, Http3Server, Http3ServerEvent, Http3State,
};
use std::mem;
use test_fixture::*;

const RESPONSE_DATA: &[u8] = &[0x61, 0x62, 0x63];

fn process_server_events(server: &mut Http3Server) {
    let mut request_found = false;
    while let Some(event) = server.next_event() {
        if let Http3ServerEvent::Headers {
            mut request,
            headers,
            fin,
        } = event
        {
            assert_eq!(
                headers,
                vec![
                    Header::new(":method", "GET"),
                    Header::new(":scheme", "https"),
                    Header::new(":authority", "something.com"),
                    Header::new(":path", "/")
                ]
            );
            assert!(fin);
            request
                .set_response(
                    &[
                        Header::new(":status", "200"),
                        Header::new("content-length", "3"),
                    ],
                    RESPONSE_DATA,
                )
                .unwrap();
            request_found = true;
        }
    }
    assert!(request_found);
}

fn process_client_events(conn: &mut Http3Client) {
    let mut response_header_found = false;
    let mut response_data_found = false;
    while let Some(event) = conn.next_event() {
        match event {
            Http3ClientEvent::HeaderReady { headers, fin, .. } => {
                assert_eq!(
                    headers,
                    vec![
                        Header::new(":status", "200"),
                        Header::new("content-length", "3"),
                    ]
                );
                assert!(!fin);
                response_header_found = true;
            }
            Http3ClientEvent::DataReadable { stream_id } => {
                let mut buf = [0u8; 100];
                let (amount, fin) = conn.read_response_data(now(), stream_id, &mut buf).unwrap();
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

fn connect() -> (Http3Client, Http3Server, Option<Datagram>) {
    let mut hconn_c = default_http3_client();
    let mut hconn_s = default_http3_server();

    assert_eq!(hconn_c.state(), Http3State::Initializing);
    let out = hconn_c.process(None, now()); // Initial
    let out = hconn_s.process(out.dgram(), now()); // Initial + Handshake
    let out = hconn_c.process(out.dgram(), now()); // ACK
    mem::drop(hconn_s.process(out.dgram(), now())); //consume ACK
    let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
    assert!(hconn_c.events().any(authentication_needed));
    hconn_c.authenticated(AuthenticationStatus::Ok, now());
    let out = hconn_c.process(None, now()); // Handshake
    assert_eq!(hconn_c.state(), Http3State::Connected);
    let out = hconn_s.process(out.dgram(), now()); // Handshake
    let out = hconn_c.process(out.dgram(), now());
    let out = hconn_s.process(out.dgram(), now());
    // assert!(hconn_s.settings_received);
    let out = hconn_c.process(out.dgram(), now());
    // assert!(hconn_c.settings_received);

    (hconn_c, hconn_s, out.dgram())
}

#[test]
fn test_connect() {
    let (_hconn_c, _hconn_s, _d) = connect();
}

#[test]
fn test_fetch() {
    let (mut hconn_c, mut hconn_s, dgram) = connect();

    eprintln!("-----client");
    let req = hconn_c
        .fetch(now(), "GET", "https", "something.com", "/", &[])
        .unwrap();
    assert_eq!(req, 0);
    hconn_c.stream_close_send(req).unwrap();
    let out = hconn_c.process(dgram, now());
    eprintln!("-----server");
    let out = hconn_s.process(out.dgram(), now());
    mem::drop(hconn_c.process(out.dgram(), now()));
    process_server_events(&mut hconn_s);
    let out = hconn_s.process(None, now());

    eprintln!("-----client");
    mem::drop(hconn_c.process(out.dgram(), now()));
    let out = hconn_s.process(None, now());
    mem::drop(hconn_c.process(out.dgram(), now()));
    process_client_events(&mut hconn_c);
}
