// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

mod common;

use common::{
    apply_header_protection, client_initial_aead_and_hp, connect, connected_server,
    decode_initial_header, default_server, get_ticket, remove_header_protection,
};

use neqo_common::{qtrace, Datagram, Decoder, Encoder};
use neqo_crypto::{generate_ech_keys, AllowZeroRtt, ZeroRttCheckResult, ZeroRttChecker};
use neqo_transport::{
    server::{ActiveConnectionRef, Server, ValidateAddress},
    Connection, ConnectionError, ConnectionParameters, Error, Output, QuicVersion, State,
    StreamType,
};
use test_fixture::{
    self, assertions, default_client, now, split_datagram, CountingConnectionIdGenerator,
};

use std::cell::RefCell;
use std::convert::TryFrom;
use std::mem;
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::Duration;

/// Take a pair of connections in any state and complete the handshake.
/// The `datagram` argument is a packet that was received from the server.
/// See `connect` for what this returns.
/// # Panics
/// Only when the connection fails.
pub fn complete_connection(
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
    mem::drop(server.process(Some(c2), now));
    assert!(server.active_connections().is_empty());

    // Now handshake and let another 0-RTT packet in.
    let shs = server.process(Some(c1), now).dgram();
    mem::drop(server.process(Some(c3), now));
    // The server will have received two STREAM frames now if it processed both packets.
    let active = server.active_connections();
    assert_eq!(active.len(), 1);
    assert_eq!(active[0].borrow().stats().frame_rx.stream, 2);

    // Complete the handshake.  As the client was pacing 0-RTT packets, extend the time
    // a little so that the pacer doesn't prevent the Finished from being sent.
    now += now - start_time;
    let cfin = client.process(shs, now).dgram();
    mem::drop(server.process(cfin, now));

    // The server will drop this last 0-RTT packet.
    mem::drop(server.process(Some(c4), now));
    let active = server.active_connections();
    assert_eq!(active.len(), 1);
    assert_eq!(active[0].borrow().stats().frame_rx.stream, 2);
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
            .max_streams(StreamType::BiDi, MAX_STREAMS)
            .max_streams(StreamType::UniDi, MAX_STREAMS),
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
    can_create_streams(&mut client, StreamType::UniDi, local_limit_unidi);
    let local_limit_bidi = ConnectionParameters::default().get_max_streams(StreamType::BiDi);
    can_create_streams(&mut client, StreamType::BiDi, local_limit_bidi);
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
    const MAX_STREAMS_BIDI: u64 = 40;
    const MAX_STREAMS_UNIDI: u64 = 30;
    let mut server = Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Box::new(RejectZeroRtt {}),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default()
            .max_streams(StreamType::BiDi, MAX_STREAMS_BIDI)
            .max_streams(StreamType::UniDi, MAX_STREAMS_UNIDI),
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
    can_create_streams(&mut client, StreamType::UniDi, MAX_STREAMS_UNIDI);
    can_create_streams(&mut client, StreamType::BiDi, MAX_STREAMS_BIDI);
}

#[test]
fn ech() {
    // Check that ECH can be used.
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server.enable_ech(0x4a, "public.example", &sk, &pk).unwrap();

    let mut client = default_client();
    client.client_enable_ech(server.ech_config()).unwrap();
    let server_instance = connect(&mut client, &mut server);

    assert!(client.tls_info().unwrap().ech_accepted());
    assert!(server_instance.borrow().tls_info().unwrap().ech_accepted());
    assert!(client.tls_preinfo().unwrap().ech_accepted().unwrap());
    assert!(server_instance
        .borrow()
        .tls_preinfo()
        .unwrap()
        .ech_accepted()
        .unwrap());
}
