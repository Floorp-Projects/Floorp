// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(unused_assignments)]

use neqo_common::{matches, Datagram};
use neqo_crypto::AuthenticationStatus;
use neqo_http3::transaction_server::Response;
use neqo_http3::{Header, Http3Client, Http3ClientEvent, Http3Server, Http3State};
use test_fixture::*;

fn new_stream_callback(request_headers: &[Header], error: bool) -> Response {
    println!("Error: {}", error);

    assert_eq!(
        request_headers,
        &[
            (String::from(":method"), String::from("GET")),
            (String::from(":scheme"), String::from("https")),
            (String::from(":authority"), String::from("something.com")),
            (String::from(":path"), String::from("/"))
        ]
    );

    (
        vec![
            (String::from(":status"), String::from("200")),
            (String::from("content-length"), String::from("3")),
        ],
        b"123".to_vec(),
        None,
    )
}

fn connect() -> (Http3Client, Http3Server, Option<Datagram>) {
    let mut hconn_c = default_http3_client();
    let mut hconn_s = default_http3_server(Some(Box::new(new_stream_callback)));

    assert_eq!(hconn_c.state(), Http3State::Initializing);
    assert_eq!(hconn_s.state(), Http3State::Initializing);
    let out = hconn_c.process(None, now()); // Initial
    let out = hconn_s.process(out.dgram(), now()); // Initial + Handshake
    let out = hconn_c.process(out.dgram(), now()); // ACK
    let _ = hconn_s.process(out.dgram(), now()); //consume ACK
    let authentication_needed = |e| matches!(e, Http3ClientEvent::AuthenticationNeeded);
    assert!(hconn_c.events().any(authentication_needed));
    hconn_c.authenticated(AuthenticationStatus::Ok, now());
    let out = hconn_c.process(None, now()); // Handshake
    assert_eq!(hconn_c.state(), Http3State::Connected);
    let out = hconn_s.process(out.dgram(), now()); // Handshake
    assert_eq!(hconn_s.state(), Http3State::Connected);
    let out = hconn_c.process(out.dgram(), now());
    let out = hconn_s.process(out.dgram(), now());
    // assert_eq!(hconn_s.settings_received, true);
    let out = hconn_c.process(out.dgram(), now());
    // assert_eq!(hconn_c.settings_received, true);

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
        .fetch("GET", "https", "something.com", "/", &[])
        .unwrap();
    assert_eq!(req, 0);
    let out = hconn_c.process(dgram, now());
    eprintln!("-----server");
    let out = hconn_s.process(out.dgram(), now());

    eprintln!("-----client");
    let _ = hconn_c.process(out.dgram(), now());
    // TODO: some kind of client API needed to read result of fetch
    // TODO: assert result is as expected e.g. (200 "abc")
    // assert!(false);
}
