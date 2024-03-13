// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::time::Duration;

use test_fixture::{datagram, now};

use super::{
    super::{Connection, Output, State},
    connect, connect_force_idle, default_client, default_server, send_something,
};
use crate::{
    tparams::{self, TransportParameter},
    AppError, ConnectionError, Error, ERROR_APPLICATION_CLOSE,
};

fn assert_draining(c: &Connection, expected: &Error) {
    assert!(c.state().closed());
    if let State::Draining {
        error: ConnectionError::Transport(error),
        ..
    } = c.state()
    {
        assert_eq!(error, expected);
    } else {
        panic!();
    }
}

#[test]
fn connection_close() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let now = now();

    client.close(now, 42, "");

    let out = client.process(None, now);

    server.process_input(&out.dgram().unwrap(), now);
    assert_draining(&server, &Error::PeerApplicationError(42));
}

#[test]
fn connection_close_with_long_reason_string() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let now = now();
    // Create a long string and use it as the close reason.
    let long_reason = String::from_utf8([0x61; 2048].to_vec()).unwrap();
    client.close(now, 42, long_reason);

    let out = client.process(None, now);

    server.process_input(&out.dgram().unwrap(), now);
    assert_draining(&server, &Error::PeerApplicationError(42));
}

// During the handshake, an application close should be sanitized.
#[test]
fn early_application_close() {
    let mut client = default_client();
    let mut server = default_server();

    // One flight each.
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    let dgram = server.process(dgram.as_ref(), now()).dgram();
    assert!(dgram.is_some());

    server.close(now(), 77, String::new());
    assert!(server.state().closed());
    let dgram = server.process(None, now()).dgram();
    assert!(dgram.is_some());

    client.process_input(&dgram.unwrap(), now());
    assert_draining(&client, &Error::PeerError(ERROR_APPLICATION_CLOSE));
}

#[test]
fn bad_tls_version() {
    let mut client = default_client();
    // Do a bad, bad thing.
    client
        .crypto
        .tls
        .set_option(neqo_crypto::Opt::Tls13CompatMode, true)
        .unwrap();
    let mut server = default_server();

    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    let dgram = server.process(dgram.as_ref(), now()).dgram();
    assert_eq!(
        *server.state(),
        State::Closed(ConnectionError::Transport(Error::ProtocolViolation))
    );
    assert!(dgram.is_some());
    client.process_input(&dgram.unwrap(), now());
    assert_draining(&client, &Error::PeerError(Error::ProtocolViolation.code()));
}

/// Test the interaction between the loss recovery timer
/// and the closing timer.
#[test]
fn closing_timers_interation() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let mut now = now();

    // We're going to induce time-based loss recovery so that timer is set.
    let _p1 = send_something(&mut client, now);
    let p2 = send_something(&mut client, now);
    let ack = server.process(Some(&p2), now).dgram();
    assert!(ack.is_some()); // This is an ACK.

    // After processing the ACK, we should be on the loss recovery timer.
    let cb = client.process(ack.as_ref(), now).callback();
    assert_ne!(cb, Duration::from_secs(0));
    now += cb;

    // Rather than let the timer pop, close the connection.
    client.close(now, 0, "");
    let client_close = client.process(None, now).dgram();
    assert!(client_close.is_some());
    // This should now report the end of the closing period, not a
    // zero-duration wait driven by the (now defunct) loss recovery timer.
    let client_close_timer = client.process(None, now).callback();
    assert_ne!(client_close_timer, Duration::from_secs(0));
}

#[test]
fn closing_and_draining() {
    const APP_ERROR: AppError = 7;
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // Save a packet from the client for later.
    let p1 = send_something(&mut client, now());

    // Close the connection.
    client.close(now(), APP_ERROR, "");
    let client_close = client.process(None, now()).dgram();
    assert!(client_close.is_some());
    let client_close_timer = client.process(None, now()).callback();
    assert_ne!(client_close_timer, Duration::from_secs(0));

    // The client will spit out the same packet in response to anything it receives.
    let p3 = send_something(&mut server, now());
    let client_close2 = client.process(Some(&p3), now()).dgram();
    assert_eq!(
        client_close.as_ref().unwrap().len(),
        client_close2.as_ref().unwrap().len()
    );

    // After this time, the client should transition to closed.
    let end = client.process(None, now() + client_close_timer);
    assert_eq!(end, Output::None);
    assert_eq!(
        *client.state(),
        State::Closed(ConnectionError::Application(APP_ERROR))
    );

    // When the server receives the close, it too should generate CONNECTION_CLOSE.
    let server_close = server.process(client_close.as_ref(), now()).dgram();
    assert!(server.state().closed());
    assert!(server_close.is_some());
    // .. but it ignores any further close packets.
    let server_close_timer = server.process(client_close2.as_ref(), now()).callback();
    assert_ne!(server_close_timer, Duration::from_secs(0));
    // Even a legitimate packet without a close in it.
    let server_close_timer2 = server.process(Some(&p1), now()).callback();
    assert_eq!(server_close_timer, server_close_timer2);

    let end = server.process(None, now() + server_close_timer);
    assert_eq!(end, Output::None);
    assert_eq!(
        *server.state(),
        State::Closed(ConnectionError::Transport(Error::PeerApplicationError(
            APP_ERROR
        )))
    );
}

/// Test that a client can handle a stateless reset correctly.
#[test]
fn stateless_reset_client() {
    let mut client = default_client();
    let mut server = default_server();
    server
        .set_local_tparam(
            tparams::STATELESS_RESET_TOKEN,
            TransportParameter::Bytes(vec![77; 16]),
        )
        .unwrap();
    connect_force_idle(&mut client, &mut server);

    client.process_input(&datagram(vec![77; 21]), now());
    assert_draining(&client, &Error::StatelessReset);
}
