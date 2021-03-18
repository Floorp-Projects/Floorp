// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

use neqo_common::{event::Provider, hex_with_len, qdebug, qtrace, Datagram, Decoder, Encoder};
use neqo_crypto::{
    aead::Aead,
    constants::{TLS_AES_128_GCM_SHA256, TLS_VERSION_1_3},
    hkdf,
    hp::HpKey,
    AllowZeroRtt, AuthenticationStatus, ResumptionToken, ZeroRttCheckResult, ZeroRttChecker,
};
use neqo_transport::{
    server::{ActiveConnectionRef, Server, ValidateAddress},
    stream_id::StreamIndex,
    Connection, ConnectionError, ConnectionEvent, ConnectionParameters, Error, Output, QuicVersion,
    State, StreamType,
};
use test_fixture::{
    self, addr, assertions, default_client, now, split_datagram, CountingConnectionIdGenerator,
};

use std::cell::RefCell;
use std::convert::TryFrom;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::ops::Range;
use std::rc::Rc;
use std::time::Duration;

// Different than the one in the fixture, which is a single connection.
fn default_server() -> Server {
    Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Box::new(AllowZeroRtt {}),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default(),
    )
    .expect("should create a server")
}

// Check that there is at least one connection.  Returns a ref to the first confirmed connection.
fn connected_server(server: &mut Server) -> ActiveConnectionRef {
    let server_connections = server.active_connections();
    // Find confirmed connections.  There should only be one.
    let mut confirmed = server_connections
        .iter()
        .filter(|c: &&ActiveConnectionRef| *c.borrow().state() == State::Confirmed);
    let c = confirmed.next().expect("one confirmed");
    assert!(confirmed.next().is_none(), "only one confirmed");
    c.clone()
}

/// Connect.  This returns a reference to the server connection.
fn connect(client: &mut Connection, server: &mut Server) -> ActiveConnectionRef {
    server.set_validation(ValidateAddress::Never);

    assert_eq!(*client.state(), State::Init);
    let dgram = client.process(None, now()).dgram(); // ClientHello
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // ServerHello...
    assert!(dgram.is_some());

    // Ingest the server Certificate.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // This should just be an ACK.
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_none()); // So the server should have nothing to say.

    // Now mark the server as authenticated.
    client.authenticated(AuthenticationStatus::Ok, now());
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // ACK + HANDSHAKE_DONE + NST

    // Have the client process the HANDSHAKE_DONE.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_none());
    assert_eq!(*client.state(), State::Confirmed);

    connected_server(server)
}

/// Take a pair of connections in any state and complete the handshake.
/// The `datagram` argument is a packet that was received from the server.
/// See `connect` for what this returns.
fn complete_connection(
    client: &mut Connection,
    server: &mut Server,
    mut datagram: Option<Datagram>,
) -> ActiveConnectionRef {
    let is_done = |c: &Connection| {
        matches!(
            c.state(),
            State::Confirmed | State::Closing { .. } | State::Closed(..)
        )
    };
    while !is_done(client) {
        let _ = test_fixture::maybe_authenticate(client);
        let out = client.process(datagram, now());
        let out = server.process(out.dgram(), now());
        datagram = out.dgram();
    }

    assert_eq!(*client.state(), State::Confirmed);
    connected_server(server)
}

#[test]
fn single_client() {
    let mut server = default_server();
    let mut client = default_client();
    connect(&mut client, &mut server);
}

#[test]
fn duplicate_initial() {
    let mut server = default_server();
    let mut client = default_client();

    assert_eq!(*client.state(), State::Init);
    let initial = client.process(None, now()).dgram();
    assert!(initial.is_some());

    // The server should ignore a packets with the same remote address and
    // destination connection ID as an existing connection attempt.
    let server_initial = server.process(initial.clone(), now()).dgram();
    assert!(server_initial.is_some());
    let dgram = server.process(initial, now()).dgram();
    assert!(dgram.is_none());

    assert_eq!(server.active_connections().len(), 1);
    complete_connection(&mut client, &mut server, server_initial);
}

#[test]
fn duplicate_initial_new_path() {
    let mut server = default_server();
    let mut client = default_client();

    assert_eq!(*client.state(), State::Init);
    let initial = client.process(None, now()).dgram().unwrap();
    let other = Datagram::new(
        SocketAddr::new(initial.source().ip(), initial.source().port() ^ 23),
        initial.destination(),
        &initial[..],
    );

    // The server should respond to both as these came from different addresses.
    let dgram = server.process(Some(other), now()).dgram();
    assert!(dgram.is_some());

    let server_initial = server.process(Some(initial), now()).dgram();
    assert!(server_initial.is_some());

    assert_eq!(server.active_connections().len(), 2);
    complete_connection(&mut client, &mut server, server_initial);
}

#[test]
fn different_initials_same_path() {
    let mut server = default_server();
    let mut client1 = default_client();
    let mut client2 = default_client();

    let client_initial1 = client1.process(None, now()).dgram();
    assert!(client_initial1.is_some());
    let client_initial2 = client2.process(None, now()).dgram();
    assert!(client_initial2.is_some());

    // The server should respond to both as these came from different addresses.
    let server_initial1 = server.process(client_initial1, now()).dgram();
    assert!(server_initial1.is_some());

    let server_initial2 = server.process(client_initial2, now()).dgram();
    assert!(server_initial2.is_some());

    assert_eq!(server.active_connections().len(), 2);
    complete_connection(&mut client1, &mut server, server_initial1);
    complete_connection(&mut client2, &mut server, server_initial2);
}

#[test]
fn same_initial_after_connected() {
    let mut server = default_server();
    let mut client = default_client();

    let client_initial = client.process(None, now()).dgram();
    assert!(client_initial.is_some());

    let server_initial = server.process(client_initial.clone(), now()).dgram();
    assert!(server_initial.is_some());
    complete_connection(&mut client, &mut server, server_initial);
    // This removes the connection from the active set until something happens to it.
    assert_eq!(server.active_connections().len(), 0);

    // Now make a new connection using the exact same initial as before.
    // The server should respond to an attempt to connect with the same Initial.
    let dgram = server.process(client_initial, now()).dgram();
    assert!(dgram.is_some());
    // The server should make a new connection object.
    assert_eq!(server.active_connections().len(), 1);
}

#[test]
fn drop_non_initial() {
    const CID: &[u8] = &[55; 8]; // not a real connection ID
    let mut server = default_server();

    // This is big enough to look like an Initial, but it uses the Retry type.
    let mut header = neqo_common::Encoder::with_capacity(1200);
    header
        .encode_byte(0xfa)
        .encode_uint(4, QuicVersion::default().as_u32())
        .encode_vec(1, CID)
        .encode_vec(1, CID);
    let mut bogus_data: Vec<u8> = header.into();
    bogus_data.resize(1200, 66);

    let bogus = Datagram::new(test_fixture::addr(), test_fixture::addr(), bogus_data);
    assert!(server.process(Some(bogus), now()).dgram().is_none());
}

#[test]
fn drop_short_initial() {
    const CID: &[u8] = &[55; 8]; // not a real connection ID
    let mut server = default_server();

    // This too small to be an Initial, but it is otherwise plausible.
    let mut header = neqo_common::Encoder::with_capacity(1199);
    header
        .encode_byte(0xca)
        .encode_uint(4, QuicVersion::default().as_u32())
        .encode_vec(1, CID)
        .encode_vec(1, CID);
    let mut bogus_data: Vec<u8> = header.into();
    bogus_data.resize(1199, 66);

    let bogus = Datagram::new(test_fixture::addr(), test_fixture::addr(), bogus_data);
    assert!(server.process(Some(bogus), now()).dgram().is_none());
}

/// Verify that the server can read 0-RTT properly.  A more robust server would buffer
/// 0-RTT before the handshake begins and let 0-RTT arrive for a short periiod after
/// the handshake completes, but ours is for testing so it only allows 0-RTT while
/// the handshake is running.
#[test]
fn zero_rtt() {
    let mut server = default_server();
    let token = get_ticket(&mut server);

    // Discharge the old connection so that we don't have to worry about it.
    let mut now = now();
    let t = server.process(None, now).callback();
    now += t;
    assert_eq!(server.process(None, now), Output::None);
    assert_eq!(server.active_connections().len(), 1);

    let start_time = now;
    let mut client = default_client();
    client.enable_resumption(now, &token).unwrap();

    let mut client_send = || {
        let client_stream = client.stream_create(StreamType::UniDi).unwrap();
        client.stream_send(client_stream, &[1, 2, 3]).unwrap();
        match client.process(None, now) {
            Output::Datagram(d) => d,
            Output::Callback(t) => {
                // Pacing...
                now += t;
                client.process(None, now).dgram().unwrap()
            }
            Output::None => panic!(),
        }
    };

    // Now generate a bunch of 0-RTT packets...
    let c1 = client_send();
    assertions::assert_coalesced_0rtt(&c1);
    let c2 = client_send();
    let c3 = client_send();
    let c4 = client_send();

    // 0-RTT packets that arrive before the handshake get dropped.
    let _ = server.process(Some(c2), now);
    assert!(server.active_connections().is_empty());

    // Now handshake and let another 0-RTT packet in.
    let shs = server.process(Some(c1), now).dgram();
    let _ = server.process(Some(c3), now);
    // The server will have received two STREAM frames now if it processed both packets.
    let active = server.active_connections();
    assert_eq!(active.len(), 1);
    assert_eq!(active[0].borrow().stats().frame_rx.stream, 2);

    // Complete the handshake.  As the client was pacing 0-RTT packets, extend the time
    // a little so that the pacer doesn't prevent the Finished from being sent.
    now += now - start_time;
    let cfin = client.process(shs, now).dgram();
    let _ = server.process(cfin, now);

    // The server will drop this last 0-RTT packet.
    let _ = server.process(Some(c4), now);
    let active = server.active_connections();
    assert_eq!(active.len(), 1);
    assert_eq!(active[0].borrow().stats().frame_rx.stream, 2);
}

#[test]
fn retry_basic() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now()).dgram(); // Initial w/token
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Initial, HS
    assert!(dgram.is_some());
    let _ = client.process(dgram, now()).dgram(); // Ingest, drop any ACK.
    client.authenticated(AuthenticationStatus::Ok, now());
    let dgram = client.process(None, now()).dgram(); // Send Finished
    assert!(dgram.is_some());
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram(); // (done)
    assert!(dgram.is_some()); // Note that this packet will be dropped...
    connected_server(&mut server);
}

#[test]
fn retry_expired() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();
    let mut now = now();

    let dgram = client.process(None, now).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now).dgram(); // Initial w/token
    assert!(dgram.is_some());

    now += Duration::from_secs(60); // Too long for Retry.
    let dgram = server.process(dgram, now).dgram(); // Initial, HS
    assert!(dgram.is_none());
}

fn get_ticket(server: &mut Server) -> ResumptionToken {
    let mut client = default_client();
    let mut server_conn = connect(&mut client, server);

    server_conn.borrow_mut().send_ticket(now(), &[]).unwrap();
    let dgram = server.process(None, now()).dgram();
    client.process_input(dgram.unwrap(), now()); // Consume ticket, ignore output.

    let ticket = client
        .events()
        .find_map(|e| {
            if let ConnectionEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        })
        .unwrap();

    // Have the client close the connection and then let the server clean up.
    client.close(now(), 0, "got a ticket");
    let dgram = client.process_output(now()).dgram();
    let _ = server.process(dgram, now());
    // Calling active_connections clears the set of active connections.
    assert_eq!(server.active_connections().len(), 1);
    ticket
}

// Attempt a retry with 0-RTT, and have 0-RTT packets sent with the second ClientHello.
#[test]
fn retry_0rtt() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::Always);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

    let client_stream = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream, &[1, 2, 3]).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial w/0-RTT
    assert!(dgram.is_some());
    assertions::assert_coalesced_0rtt(dgram.as_ref().unwrap());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());
    assertions::assert_retry(dgram.as_ref().unwrap());

    // After retry, there should be a token and still coalesced 0-RTT.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some());
    assertions::assert_coalesced_0rtt(dgram.as_ref().unwrap());

    let dgram = server.process(dgram, now()).dgram(); // Initial, HS
    assert!(dgram.is_some());
    let dgram = client.process(dgram, now()).dgram();
    // Note: the client doesn't need to authenticate the server here
    // as there is no certificate; authentication is based on the ticket.
    assert!(dgram.is_some());
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram(); // (done)
    assert!(dgram.is_some());
    connected_server(&mut server);
    assert!(client.tls_info().unwrap().resumed());
}

#[test]
fn new_token_0rtt() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::NoToken);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

    let client_stream = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream, &[1, 2, 3]).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial w/0-RTT
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), true);
    assertions::assert_coalesced_0rtt(dgram.as_ref().unwrap());
    let dgram = server.process(dgram, now()).dgram(); // Initial
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), false);

    let dgram = client.process(dgram, now()).dgram();
    // Note: the client doesn't need to authenticate the server here
    // as there is no certificate; authentication is based on the ticket.
    assert!(dgram.is_some());
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram(); // (done)
    assert!(dgram.is_some());
    connected_server(&mut server);
    assert!(client.tls_info().unwrap().resumed());
}

#[test]
fn retry_different_ip() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now()).dgram(); // Initial w/token
    assert!(dgram.is_some());

    // Change the source IP on the address from the client.
    let dgram = dgram.unwrap();
    let other_v4 = IpAddr::V4(Ipv4Addr::new(127, 0, 0, 2));
    let other_addr = SocketAddr::new(other_v4, 443);
    let from_other = Datagram::new(other_addr, dgram.destination(), &dgram[..]);
    let dgram = server.process(Some(from_other), now()).dgram();
    assert!(dgram.is_none());
}

#[test]
fn new_token_different_ip() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::NoToken);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), true);

    // Now rewrite the source address.
    let d = dgram.unwrap();
    let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::new(127, 0, 0, 2)), d.source().port());
    let dgram = Some(Datagram::new(src, d.destination(), &d[..]));
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());
    assertions::assert_retry(dgram.as_ref().unwrap());
}

#[test]
fn new_token_different_port() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::NoToken);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), true);

    // Now rewrite the source port, which should not change that the token is OK.
    let d = dgram.unwrap();
    let src = SocketAddr::new(d.source().ip(), d.source().port() + 1);
    let dgram = Some(Datagram::new(src, d.destination(), &d[..]));
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), false);
}

#[test]
fn new_token_expired() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::NoToken);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), true);

    // Now move into the future.
    // We can't go too far or we'll overflow our field.  Not when checking,
    // but when trying to generate another Retry.  A month is fine.
    let the_future = now() + Duration::from_secs(60 * 60 * 24 * 30);
    let d = dgram.unwrap();
    let src = SocketAddr::new(d.source().ip(), d.source().port() + 1);
    let dgram = Some(Datagram::new(src, d.destination(), &d[..]));
    let dgram = server.process(dgram, the_future).dgram(); // Retry
    assert!(dgram.is_some());
    assertions::assert_retry(dgram.as_ref().unwrap());
}

#[test]
fn retry_after_initial() {
    let mut server = default_server();
    let mut retry_server = default_server();
    retry_server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let cinit = client.process(None, now()).dgram(); // Initial
    assert!(cinit.is_some());
    let server_flight = server.process(cinit.clone(), now()).dgram(); // Initial
    assert!(server_flight.is_some());

    // We need to have the client just process the Initial.
    let (server_initial, _other) = split_datagram(server_flight.as_ref().unwrap());
    let dgram = client.process(Some(server_initial), now()).dgram();
    assert!(dgram.is_some());
    assert!(*client.state() != State::Connected);

    let retry = retry_server.process(cinit, now()).dgram(); // Retry!
    assert!(retry.is_some());
    assertions::assert_retry(&retry.as_ref().unwrap());

    // The client should ignore the retry.
    let junk = client.process(retry, now()).dgram();
    assert!(junk.is_none());

    // Either way, the client should still be able to process the server flight and connect.
    let dgram = client.process(server_flight, now()).dgram();
    assert!(dgram.is_some()); // Drop this one.
    assert!(test_fixture::maybe_authenticate(&mut client));
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());

    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram(); // (done)
    assert!(dgram.is_some());
    connected_server(&mut server);
}

#[test]
fn retry_bad_integrity() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    let retry = &dgram.as_ref().unwrap();
    assertions::assert_retry(retry);

    let mut tweaked = retry.to_vec();
    tweaked[retry.len() - 1] ^= 0x45; // damage the auth tag
    let tweaked_packet = Datagram::new(retry.source(), retry.destination(), tweaked);

    // The client should ignore this packet.
    let dgram = client.process(Some(tweaked_packet), now()).dgram();
    assert!(dgram.is_none());
}

#[test]
fn retry_bad_token() {
    let mut client = default_client();
    let mut retry_server = default_server();
    retry_server.set_validation(ValidateAddress::Always);
    let mut server = default_server();

    // Send a retry to one server, then replay it to the other.
    let client_initial1 = client.process(None, now()).dgram();
    assert!(client_initial1.is_some());
    let retry = retry_server.process(client_initial1, now()).dgram();
    assert!(retry.is_some());
    let client_initial2 = client.process(retry, now()).dgram();
    assert!(client_initial2.is_some());

    let dgram = server.process(client_initial2, now()).dgram();
    assert!(dgram.is_none());
}

// This is really a client test, but we need a server with Retry to test it.
// In this test, the client sends Initial on PTO.  The Retry should cause
// all loss recovery timers to be reset, but we had a bug where the PTO timer
// was not properly reset.  This tests that the client generates a new Initial
// in response to receiving a Retry, even after it sends the Initial on PTO.
#[test]
fn retry_after_pto() {
    let mut client = default_client();
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut now = now();

    let ci = client.process(None, now).dgram();
    assert!(ci.is_some()); // sit on this for a bit.RefCell

    // Let PTO fire on the client and then let it exhaust its PTO packets.
    now += Duration::from_secs(1);
    let pto1 = client.process(None, now).dgram();
    assert!(pto1.unwrap().len() >= 1200);
    let pto2 = client.process(None, now).dgram();
    assert!(pto2.unwrap().len() >= 1200);
    let cb = client.process(None, now).callback();
    assert_ne!(cb, Duration::new(0, 0));

    let retry = server.process(ci, now).dgram();
    assertions::assert_retry(&retry.as_ref().unwrap());

    let ci2 = client.process(retry, now).dgram();
    assert!(ci2.unwrap().len() >= 1200);
}

#[test]
fn vn_after_retry() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now()).dgram(); // Initial w/token
    assert!(dgram.is_some());

    let mut encoder = Encoder::default();
    encoder.encode_byte(0x80);
    encoder.encode(&[0; 4]); // Zero version == VN.
    encoder.encode_vec(1, &client.odcid().unwrap()[..]);
    encoder.encode_vec(1, &[]);
    encoder.encode_uint(4, 0x5a5a_6a6a_u64);
    let vn = Datagram::new(addr(), addr(), encoder);

    assert_ne!(
        client.process(Some(vn), now()).callback(),
        Duration::from_secs(0)
    );
}

// Generate an AEAD and header protection object for a client Initial.
fn client_initial_aead_and_hp(dcid: &[u8]) -> (Aead, HpKey) {
    const INITIAL_SALT: &[u8] = &[
        0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34, 0xb3, 0x4d, 0x17, 0x9a, 0xe6, 0xa4, 0xc8, 0x0c,
        0xad, 0xcc, 0xbb, 0x7f, 0x0a,
    ];
    let initial_secret = hkdf::extract(
        TLS_VERSION_1_3,
        TLS_AES_128_GCM_SHA256,
        Some(
            hkdf::import_key(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, INITIAL_SALT)
                .as_ref()
                .unwrap(),
        ),
        hkdf::import_key(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, dcid)
            .as_ref()
            .unwrap(),
    )
    .unwrap();

    let secret = hkdf::expand_label(
        TLS_VERSION_1_3,
        TLS_AES_128_GCM_SHA256,
        &initial_secret,
        &[],
        "client in",
    )
    .unwrap();
    (
        Aead::new(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, &secret, "quic ").unwrap(),
        HpKey::extract(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, &secret, "quic hp").unwrap(),
    )
}

// Decode the header of a client Initial packet, returning three values:
// * the entire header short of the packet number,
// * just the DCID,
// * just the SCID, and
// * the protected payload including the packet number.
// Any token is thrown away.
fn decode_initial_header(dgram: &Datagram) -> (&[u8], &[u8], &[u8], &[u8]) {
    let mut dec = Decoder::new(&dgram[..]);
    let type_and_ver = dec.decode(5).unwrap().to_vec();
    assert_eq!(type_and_ver[0] & 0xf0, 0xc0);
    let dest_cid = dec.decode_vec(1).unwrap();
    let src_cid = dec.decode_vec(1).unwrap();
    dec.skip_vvec(); // Ignore any the token.

    // Need to read of the length separately so that we can find the packet number.
    let payload_len = usize::try_from(dec.decode_varint().unwrap()).unwrap();
    let pn_offset = dgram.len() - dec.remaining();
    (
        &dgram[..pn_offset],
        dest_cid,
        src_cid,
        dec.decode(payload_len).unwrap(),
    )
}

// Remove header protection, returning the unmasked header and the packet number.
fn remove_header_protection(hp: &HpKey, header: &[u8], payload: &[u8]) -> (Vec<u8>, u64) {
    // Make a copy of the header that can be modified.
    let mut fixed_header = header.to_vec();
    let pn_offset = header.len();
    // Save 4 extra in case the packet number is that long.
    fixed_header.extend_from_slice(&payload[..4]);

    // Sample for masking and apply the mask.
    let mask = hp.mask(&payload[4..20]).unwrap();
    fixed_header[0] ^= mask[0] & 0xf;
    let pn_len = 1 + usize::from(fixed_header[0] & 0x3);
    for i in 0..pn_len {
        fixed_header[pn_offset + i] ^= mask[1 + i];
    }
    // Trim down to size.
    fixed_header.truncate(pn_offset + pn_len);
    // The packet number should be 1.
    let pn = Decoder::new(&fixed_header[pn_offset..])
        .decode_uint(pn_len)
        .unwrap();

    (fixed_header, pn)
}

fn apply_header_protection(hp: &HpKey, packet: &mut [u8], pn_bytes: Range<usize>) {
    let sample_start = pn_bytes.start + 4;
    let sample_end = sample_start + 16;
    let mask = hp.mask(&packet[sample_start..sample_end]).unwrap();
    qtrace!(
        "sample={} mask={}",
        hex_with_len(&packet[sample_start..sample_end]),
        hex_with_len(&mask)
    );
    packet[0] ^= mask[0] & 0xf;
    for i in 0..(pn_bytes.end - pn_bytes.start) {
        packet[pn_bytes.start + i] ^= mask[1 + i];
    }
}

// This tests a simulated on-path attacker that intercepts the first
// client Initial packet and spoofs a retry.
// The tricky part is in rewriting the second client Initial so that
// the server doesn't reject the Initial for having a bad token.
// The client is the only one that can detect this, and that is because
// the original connection ID is not in transport parameters.
//
// Note that this depends on having the server produce a CID that is
// at least 8 bytes long.  Otherwise, the second Initial won't have a
// long enough connection ID.
#[test]
#[allow(clippy::shadow_unrelated)]
fn mitm_retry() {
    let mut client = default_client();
    let mut retry_server = default_server();
    retry_server.set_validation(ValidateAddress::Always);
    let mut server = default_server();

    // Trigger initial and a second client Initial.
    let client_initial1 = client.process(None, now()).dgram();
    assert!(client_initial1.is_some());
    let retry = retry_server.process(client_initial1, now()).dgram();
    assert!(retry.is_some());
    let client_initial2 = client.process(retry, now()).dgram();
    assert!(client_initial2.is_some());

    // Now to start the epic process of decrypting the packet,
    // rewriting the header to remove the token, and then re-encrypting.
    let client_initial2 = client_initial2.unwrap();
    let (protected_header, d_cid, s_cid, payload) = decode_initial_header(&client_initial2);

    // Now we have enough information to make keys.
    let (aead, hp) = client_initial_aead_and_hp(&d_cid);
    let (header, pn) = remove_header_protection(&hp, protected_header, payload);
    let pn_len = header.len() - protected_header.len();

    // Decrypt.
    assert_eq!(pn, 1);
    let mut plaintext_buf = vec![0; client_initial2.len()];
    let plaintext = aead
        .decrypt(pn, &header, &payload[pn_len..], &mut plaintext_buf)
        .unwrap();

    // Now re-encode without the token.
    let mut enc = Encoder::with_capacity(header.len());
    enc.encode(&header[..5])
        .encode_vec(1, d_cid)
        .encode_vec(1, s_cid)
        .encode_vvec(&[])
        .encode_varint(u64::try_from(payload.len()).unwrap());
    let pn_offset = enc.len();
    let notoken_header = enc.encode_uint(pn_len, pn).to_vec();
    qtrace!("notoken_header={}", hex_with_len(&notoken_header));

    // Encrypt.
    let mut notoken_packet = Encoder::with_capacity(1200)
        .encode(&notoken_header)
        .to_vec();
    notoken_packet.resize_with(1200, u8::default);
    aead.encrypt(
        pn,
        &notoken_header,
        plaintext,
        &mut notoken_packet[notoken_header.len()..],
    )
    .unwrap();
    // Unlike with decryption, don't truncate.
    // All 1200 bytes are needed to reach the minimum datagram size.

    apply_header_protection(&hp, &mut notoken_packet, pn_offset..(pn_offset + pn_len));
    qtrace!("packet={}", hex_with_len(&notoken_packet));

    let new_datagram = Datagram::new(
        client_initial2.source(),
        client_initial2.destination(),
        notoken_packet,
    );
    qdebug!("passing modified Initial to the main server");
    let dgram = server.process(Some(new_datagram), now()).dgram();
    assert!(dgram.is_some());

    let dgram = client.process(dgram, now()).dgram(); // Generate an ACK.
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_none());
    assert!(test_fixture::maybe_authenticate(&mut client));
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // Client sending CLOSE_CONNECTIONs
    assert!(matches!(
        *client.state(),
        State::Closing {
            error: ConnectionError::Transport(Error::ProtocolViolation),
            ..
        }
    ));
}

#[test]
fn bad_client_initial() {
    let mut client = default_client();
    let mut server = default_server();

    let dgram = client.process(None, now()).dgram().expect("a datagram");
    let (header, d_cid, s_cid, payload) = decode_initial_header(&dgram);
    let (aead, hp) = client_initial_aead_and_hp(d_cid);
    let (fixed_header, pn) = remove_header_protection(&hp, header, payload);
    let payload = &payload[(fixed_header.len() - header.len())..];

    let mut plaintext_buf = vec![0; dgram.len()];
    let plaintext = aead
        .decrypt(pn, &fixed_header, payload, &mut plaintext_buf)
        .unwrap();

    let mut payload_enc = Encoder::from(plaintext);
    payload_enc.encode(&[0x08, 0x02, 0x00, 0x00]); // Add a stream frame.

    // Make a new header with a 1 byte packet number length.
    let mut header_enc = Encoder::new();
    header_enc
        .encode_byte(0xc0) // Initial with 1 byte packet number.
        .encode_uint(4, QuicVersion::default().as_u32())
        .encode_vec(1, d_cid)
        .encode_vec(1, s_cid)
        .encode_vvec(&[])
        .encode_varint(u64::try_from(payload_enc.len() + aead.expansion() + 1).unwrap())
        .encode_byte(u8::try_from(pn).unwrap());

    let mut ciphertext = header_enc.to_vec();
    ciphertext.resize(header_enc.len() + payload_enc.len() + aead.expansion(), 0);
    let v = aead
        .encrypt(
            pn,
            &header_enc,
            &payload_enc,
            &mut ciphertext[header_enc.len()..],
        )
        .unwrap();
    assert_eq!(header_enc.len() + v.len(), ciphertext.len());
    // Pad with zero to get up to 1200.
    ciphertext.resize(1200, 0);

    apply_header_protection(
        &hp,
        &mut ciphertext,
        (header_enc.len() - 1)..header_enc.len(),
    );
    let bad_dgram = Datagram::new(dgram.source(), dgram.destination(), ciphertext);

    // The server should reject this.
    let response = server.process(Some(bad_dgram), now());
    let close_dgram = response.dgram().unwrap();
    // The resulting datagram might contain multiple packets, but each is small.
    let (initial_close, rest) = split_datagram(&close_dgram);
    // Allow for large connection IDs and a 32 byte CONNECTION_CLOSE.
    assert!(initial_close.len() <= 100);
    let (handshake_close, short_close) = split_datagram(&rest.unwrap());
    // The Handshake packet containing the close is the same size as the Initial,
    // plus 1 byte for the Token field in the Initial.
    assert_eq!(initial_close.len(), handshake_close.len() + 1);
    assert!(short_close.unwrap().len() <= 73);

    // The client should accept this new and stop trying to connect.
    // It will generate a CONNECTION_CLOSE first though.
    let response = client.process(Some(close_dgram), now()).dgram();
    assert!(response.is_some());
    // The client will now wait out its closing period.
    let delay = client.process(None, now()).callback();
    assert_ne!(delay, Duration::from_secs(0));
    assert!(matches!(
        *client.state(),
        State::Draining { error: ConnectionError::Transport(Error::PeerError(code)), .. } if code == Error::ProtocolViolation.code()
    ));

    for server in server.active_connections() {
        assert_eq!(
            *server.borrow().state(),
            State::Closed(ConnectionError::Transport(Error::ProtocolViolation))
        );
    }

    // After sending the CONNECTION_CLOSE, the server goes idle.
    let res = server.process(None, now());
    assert_eq!(res, Output::None);
}

#[test]
fn version_negotiation() {
    let mut server = default_server();
    let mut client = default_client();

    // Any packet will do, but let's make something that looks real.
    let dgram = client.process(None, now()).dgram().expect("a datagram");
    let mut input = dgram.to_vec();
    input[1] ^= 0x12;
    let damaged = Datagram::new(dgram.source(), dgram.destination(), input.clone());
    let vn = server.process(Some(damaged), now()).dgram();

    let mut dec = Decoder::from(&input[5..]); // Skip past version.
    let d_cid = dec.decode_vec(1).expect("client DCID").to_vec();
    let s_cid = dec.decode_vec(1).expect("client SCID").to_vec();

    // We should have received a VN packet.
    let vn = vn.expect("a vn packet");
    let mut dec = Decoder::from(&vn[1..]); // Skip first byte.

    assert_eq!(dec.decode_uint(4).expect("VN"), 0);
    assert_eq!(dec.decode_vec(1).expect("VN DCID"), &s_cid[..]);
    assert_eq!(dec.decode_vec(1).expect("VN SCID"), &d_cid[..]);
    let mut found = false;
    while dec.remaining() > 0 {
        let v = dec.decode_uint(4).expect("supported version");
        found |= v == u64::from(QuicVersion::default().as_u32());
    }
    assert!(found, "valid version not found");

    // Client ignores VN packet that contain negotiated version.
    let res = client.process(Some(vn), now());
    assert!(res.callback() > Duration::new(0, 120));
    assert_eq!(client.state(), &State::WaitInitial);
}

#[test]
fn closed() {
    // Let a server connection idle and it should be removed.
    let mut server = default_server();
    let mut client = default_client();
    connect(&mut client, &mut server);

    // The server will have sent a few things, so it will be on PTO.
    let res = server.process(None, now());
    assert!(res.callback() > Duration::new(0, 0));
    // The client will be on the delayed ACK timer.
    let res = client.process(None, now());
    assert!(res.callback() > Duration::new(0, 0));

    qtrace!("60s later");
    let res = server.process(None, now() + Duration::from_secs(60));
    assert_eq!(res, Output::None);
}

fn can_create_streams(c: &mut Connection, t: StreamType, n: u64) {
    for _ in 0..n {
        c.stream_create(t).unwrap();
    }
    assert_eq!(c.stream_create(t), Err(Error::StreamLimitError));
}

#[test]
fn max_streams() {
    const MAX_STREAMS: u64 = 40;
    let mut server = Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Box::new(AllowZeroRtt {}),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default()
            .max_streams(StreamType::BiDi, StreamIndex::new(MAX_STREAMS))
            .max_streams(StreamType::UniDi, StreamIndex::new(MAX_STREAMS)),
    )
    .expect("should create a server");

    let mut client = default_client();
    connect(&mut client, &mut server);

    // Make sure that we can create MAX_STREAMS uni- and bidirectional streams.
    can_create_streams(&mut client, StreamType::UniDi, MAX_STREAMS);
    can_create_streams(&mut client, StreamType::BiDi, MAX_STREAMS);
}

#[test]
fn max_streams_default() {
    let mut server = Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Box::new(AllowZeroRtt {}),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default(),
    )
    .expect("should create a server");

    let mut client = default_client();
    connect(&mut client, &mut server);

    // Make sure that we can create streams up to the local limit.
    let local_limit_unidi = ConnectionParameters::default().get_max_streams(StreamType::UniDi);
    can_create_streams(&mut client, StreamType::UniDi, local_limit_unidi.as_u64());
    let local_limit_bidi = ConnectionParameters::default().get_max_streams(StreamType::BiDi);
    can_create_streams(&mut client, StreamType::BiDi, local_limit_bidi.as_u64());
}

#[derive(Debug)]
struct RejectZeroRtt {}
impl ZeroRttChecker for RejectZeroRtt {
    fn check(&self, _token: &[u8]) -> ZeroRttCheckResult {
        ZeroRttCheckResult::Reject
    }
}

#[test]
fn max_streams_after_0rtt_rejection() {
    const MAX_STREAMS: u64 = 40;
    let mut server = Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Box::new(RejectZeroRtt {}),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default()
            .max_streams(StreamType::BiDi, StreamIndex::new(MAX_STREAMS))
            .max_streams(StreamType::UniDi, StreamIndex::new(MAX_STREAMS)),
    )
    .expect("should create a server");
    let token = get_ticket(&mut server);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();
    let _ = client.stream_create(StreamType::BiDi).unwrap();
    let dgram = client.process_output(now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // We're far enough along to complete the test now.

    // Make sure that we can create MAX_STREAMS uni- and bidirectional streams.
    can_create_streams(&mut client, StreamType::UniDi, MAX_STREAMS);
    can_create_streams(&mut client, StreamType::BiDi, MAX_STREAMS);
}
