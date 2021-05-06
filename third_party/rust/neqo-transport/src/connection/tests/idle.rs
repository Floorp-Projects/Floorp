// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::{IdleTimeout, Output, State, LOCAL_IDLE_TIMEOUT};
use super::{
    connect, connect_force_idle, connect_with_rtt, default_client, default_server,
    maybe_authenticate, send_something, AT_LEAST_PTO,
};
use crate::packet::PacketBuilder;
use crate::stats::FrameStats;
use crate::tparams::{self, TransportParameter};
use crate::tracking::PacketNumberSpace;
use crate::StreamType;

use neqo_common::Encoder;
use std::time::Duration;
use test_fixture::{self, now, split_datagram};

#[test]
fn idle_timeout() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    let res = client.process(None, now);
    assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

    // Still connected after 29 seconds. Idle timer not reset
    let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT - Duration::from_secs(1));
    assert!(matches!(client.state(), State::Confirmed));

    let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT);

    // Not connected after LOCAL_IDLE_TIMEOUT seconds.
    assert!(matches!(client.state(), State::Closed(_)));
}

#[test]
fn asymmetric_idle_timeout() {
    const LOWER_TIMEOUT_MS: u64 = 1000;
    const LOWER_TIMEOUT: Duration = Duration::from_millis(LOWER_TIMEOUT_MS);
    // Sanity check the constant.
    assert!(LOWER_TIMEOUT < LOCAL_IDLE_TIMEOUT);

    let mut client = default_client();
    let mut server = default_server();

    // Overwrite the default at the server.
    server
        .tps
        .borrow_mut()
        .local
        .set_integer(tparams::IDLE_TIMEOUT, LOWER_TIMEOUT_MS);
    server.idle_timeout = IdleTimeout::new(LOWER_TIMEOUT);

    // Now connect and force idleness manually.
    // We do that by following what `force_idle` does and have each endpoint
    // send two packets, which are delivered out of order.  See `force_idle`.
    connect(&mut client, &mut server);
    let c1 = send_something(&mut client, now());
    let c2 = send_something(&mut client, now());
    server.process_input(c2, now());
    server.process_input(c1, now());
    let s1 = send_something(&mut server, now());
    let s2 = send_something(&mut server, now());
    client.process_input(s2, now());
    let ack = client.process(Some(s1), now()).dgram();
    assert!(ack.is_some());
    // Now both should have received ACK frames so should be idle.
    assert_eq!(server.process(ack, now()), Output::Callback(LOWER_TIMEOUT));
    assert_eq!(client.process(None, now()), Output::Callback(LOWER_TIMEOUT));
}

#[test]
fn tiny_idle_timeout() {
    const RTT: Duration = Duration::from_millis(500);
    const LOWER_TIMEOUT_MS: u64 = 100;
    const LOWER_TIMEOUT: Duration = Duration::from_millis(LOWER_TIMEOUT_MS);
    // We won't respect a value that is lower than 3*PTO, sanity check.
    assert!(LOWER_TIMEOUT < 3 * RTT);

    let mut client = default_client();
    let mut server = default_server();

    // Overwrite the default at the server.
    server
        .set_local_tparam(
            tparams::IDLE_TIMEOUT,
            TransportParameter::Integer(LOWER_TIMEOUT_MS),
        )
        .unwrap();
    server.idle_timeout = IdleTimeout::new(LOWER_TIMEOUT);

    // Now connect with an RTT and force idleness manually.
    let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT);
    let c1 = send_something(&mut client, now);
    let c2 = send_something(&mut client, now);
    now += RTT / 2;
    server.process_input(c2, now);
    server.process_input(c1, now);
    let s1 = send_something(&mut server, now);
    let s2 = send_something(&mut server, now);
    now += RTT / 2;
    client.process_input(s2, now);
    let ack = client.process(Some(s1), now).dgram();
    assert!(ack.is_some());

    // The client should be idle now, but with a different timer.
    if let Output::Callback(t) = client.process(None, now) {
        assert!(t > LOWER_TIMEOUT);
    } else {
        panic!("Client not idle");
    }

    // The server should go idle after the ACK, but again with a larger timeout.
    now += RTT / 2;
    if let Output::Callback(t) = client.process(ack, now) {
        assert!(t > LOWER_TIMEOUT);
    } else {
        panic!("Client not idle");
    }
}

#[test]
fn idle_send_packet1() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    let res = client.process(None, now);
    assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 5);

    let out = client.process(None, now + Duration::from_secs(10));
    let out = server.process(out.dgram(), now + Duration::from_secs(10));

    // Still connected after 39 seconds because idle timer reset by outgoing
    // packet
    let _ = client.process(
        out.dgram(),
        now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(9),
    );
    assert!(matches!(client.state(), State::Confirmed));

    // Not connected after 40 seconds.
    let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(10));

    assert!(matches!(client.state(), State::Closed(_)));
}

#[test]
fn idle_send_packet2() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    let res = client.process(None, now);
    assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 5);

    let _out = client.process(None, now + Duration::from_secs(10));

    assert_eq!(client.stream_send(stream_id, b"there").unwrap(), 5);
    let _out = client.process(None, now + Duration::from_secs(20));

    // Still connected after 39 seconds.
    let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(9));
    assert!(matches!(client.state(), State::Confirmed));

    // Not connected after 40 seconds because timer not reset by second
    // outgoing packet
    let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(10));
    assert!(matches!(client.state(), State::Closed(_)));
}

#[test]
fn idle_recv_packet() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    let res = client.process(None, now);
    assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);
    assert_eq!(client.stream_send(0, b"hello").unwrap(), 5);

    // Respond with another packet
    let out = client.process(None, now + Duration::from_secs(10));
    server.process_input(out.dgram().unwrap(), now + Duration::from_secs(10));
    assert_eq!(server.stream_send(0, b"world").unwrap(), 5);
    let out = server.process_output(now + Duration::from_secs(10));
    assert_ne!(out.as_dgram_ref(), None);

    let _ = client.process(out.dgram(), now + Duration::from_secs(20));
    assert!(matches!(client.state(), State::Confirmed));

    // Still connected after 49 seconds because idle timer reset by received
    // packet
    let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(19));
    assert!(matches!(client.state(), State::Confirmed));

    // Not connected after 50 seconds.
    let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(20));

    assert!(matches!(client.state(), State::Closed(_)));
}

/// Caching packets should not cause the connection to become idle.
/// This requires a few tricks to keep the connection from going
/// idle while preventing any progress on the handshake.
#[test]
fn idle_caching() {
    let mut client = default_client();
    let mut server = default_server();
    let start = now();
    let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);

    // Perform the first round trip, but drop the Initial from the server.
    // The client then caches the Handshake packet.
    let dgram = client.process_output(start).dgram();
    let dgram = server.process(dgram, start).dgram();
    let (_, handshake) = split_datagram(&dgram.unwrap());
    client.process_input(handshake.unwrap(), start);

    // Perform an exchange and keep the connection alive.
    // Only allow a packet containing a PING to pass.
    let middle = start + AT_LEAST_PTO;
    let _ = client.process_output(middle);
    let dgram = client.process_output(middle).dgram();

    // Get the server to send its first probe and throw that away.
    let _ = server.process_output(middle).dgram();
    // Now let the server process the client PING.  This causes the server
    // to send CRYPTO frames again, so manually extract and discard those.
    let ping_before_s = server.stats().frame_rx.ping;
    server.process_input(dgram.unwrap(), middle);
    assert_eq!(server.stats().frame_rx.ping, ping_before_s + 1);
    let mut tokens = Vec::new();
    server
        .crypto
        .streams
        .write_frame(
            PacketNumberSpace::Initial,
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        )
        .unwrap();
    assert_eq!(tokens.len(), 1);
    tokens.clear();
    server
        .crypto
        .streams
        .write_frame(
            PacketNumberSpace::Initial,
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        )
        .unwrap();
    assert!(tokens.is_empty());
    let dgram = server.process_output(middle).dgram();

    // Now only allow the Initial packet from the server through;
    // it shouldn't contain a CRYPTO frame.
    let (initial, _) = split_datagram(&dgram.unwrap());
    let ping_before_c = client.stats().frame_rx.ping;
    let ack_before = client.stats().frame_rx.ack;
    client.process_input(initial, middle);
    assert_eq!(client.stats().frame_rx.ping, ping_before_c + 1);
    assert_eq!(client.stats().frame_rx.ack, ack_before + 1);

    let end = start + LOCAL_IDLE_TIMEOUT + (AT_LEAST_PTO / 2);
    // Now let the server Initial through, with the CRYPTO frame.
    let dgram = server.process_output(end).dgram();
    let (initial, _) = split_datagram(&dgram.unwrap());
    neqo_common::qwarn!("client ingests initial, finally");
    let _ = client.process(Some(initial), end);
    maybe_authenticate(&mut client);
    let dgram = client.process_output(end).dgram();
    let dgram = server.process(dgram, end).dgram();
    client.process_input(dgram.unwrap(), end);
    assert_eq!(*client.state(), State::Confirmed);
    assert_eq!(*server.state(), State::Confirmed);
}
