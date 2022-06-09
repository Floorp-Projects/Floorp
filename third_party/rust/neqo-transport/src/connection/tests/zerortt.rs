// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::Connection;
use super::{
    connect, default_client, default_server, exchange_ticket, new_server, resumed_server,
    CountingConnectionIdGenerator,
};
use crate::events::ConnectionEvent;
use crate::{ConnectionParameters, Error, StreamType, Version};

use neqo_common::event::Provider;
use neqo_crypto::{AllowZeroRtt, AntiReplay};
use std::cell::RefCell;
use std::rc::Rc;
use test_fixture::{self, assertions, now};

#[test]
fn zero_rtt_negotiate() {
    // Note that the two servers in this test will get different anti-replay filters.
    // That's OK because we aren't testing anti-replay.
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let token = exchange_ticket(&mut client, &mut server, now());
    let mut client = default_client();
    client
        .enable_resumption(now(), token)
        .expect("should set token");
    let mut server = resumed_server(&client);
    connect(&mut client, &mut server);
    assert!(client.tls_info().unwrap().early_data_accepted());
    assert!(server.tls_info().unwrap().early_data_accepted());
}

#[test]
fn zero_rtt_send_recv() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let token = exchange_ticket(&mut client, &mut server, now());
    let mut client = default_client();
    client
        .enable_resumption(now(), token)
        .expect("should set token");
    let mut server = resumed_server(&client);

    // Send ClientHello.
    let client_hs = client.process(None, now());
    assert!(client_hs.as_dgram_ref().is_some());

    // Now send a 0-RTT packet.
    let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream_id, &[1, 2, 3]).unwrap();
    let client_0rtt = client.process(None, now());
    assert!(client_0rtt.as_dgram_ref().is_some());
    // 0-RTT packets on their own shouldn't be padded to 1200.
    assert!(client_0rtt.as_dgram_ref().unwrap().len() < 1200);

    let server_hs = server.process(client_hs.dgram(), now());
    assert!(server_hs.as_dgram_ref().is_some()); // ServerHello, etc...

    let all_frames = server.stats().frame_tx.all;
    let ack_frames = server.stats().frame_tx.ack;
    let server_process_0rtt = server.process(client_0rtt.dgram(), now());
    assert!(server_process_0rtt.as_dgram_ref().is_some());
    assert_eq!(server.stats().frame_tx.all, all_frames + 1);
    assert_eq!(server.stats().frame_tx.ack, ack_frames + 1);

    let server_stream_id = server
        .events()
        .find_map(|evt| match evt {
            ConnectionEvent::NewStream { stream_id, .. } => Some(stream_id),
            _ => None,
        })
        .expect("should have received a new stream event");
    assert_eq!(client_stream_id, server_stream_id.as_u64());
}

#[test]
fn zero_rtt_send_coalesce() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let token = exchange_ticket(&mut client, &mut server, now());
    let mut client = default_client();
    client
        .enable_resumption(now(), token)
        .expect("should set token");
    let mut server = resumed_server(&client);

    // Write 0-RTT before generating any packets.
    // This should result in a datagram that coalesces Initial and 0-RTT.
    let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream_id, &[1, 2, 3]).unwrap();
    let client_0rtt = client.process(None, now());
    assert!(client_0rtt.as_dgram_ref().is_some());

    assertions::assert_coalesced_0rtt(&client_0rtt.as_dgram_ref().unwrap()[..]);

    let server_hs = server.process(client_0rtt.dgram(), now());
    assert!(server_hs.as_dgram_ref().is_some()); // Should produce ServerHello etc...

    let server_stream_id = server
        .events()
        .find_map(|evt| match evt {
            ConnectionEvent::NewStream { stream_id } => Some(stream_id),
            _ => None,
        })
        .expect("should have received a new stream event");
    assert_eq!(client_stream_id, server_stream_id.as_u64());
}

#[test]
fn zero_rtt_before_resumption_token() {
    let mut client = default_client();
    assert!(client.stream_create(StreamType::BiDi).is_err());
}

#[test]
fn zero_rtt_send_reject() {
    const MESSAGE: &[u8] = &[1, 2, 3];

    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let token = exchange_ticket(&mut client, &mut server, now());
    let mut client = default_client();
    client
        .enable_resumption(now(), token)
        .expect("should set token");
    let mut server = Connection::new_server(
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default().versions(client.version(), Version::all()),
    )
    .unwrap();
    // Using a freshly initialized anti-replay context
    // should result in the server rejecting 0-RTT.
    let ar =
        AntiReplay::new(now(), test_fixture::ANTI_REPLAY_WINDOW, 1, 3).expect("setup anti-replay");
    server
        .server_enable_0rtt(&ar, AllowZeroRtt {})
        .expect("enable 0-RTT");

    // Send ClientHello.
    let client_hs = client.process(None, now());
    assert!(client_hs.as_dgram_ref().is_some());

    // Write some data on the client.
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(stream_id, MESSAGE).unwrap();
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
    let client_fin = client.process(server_hs.dgram(), now());
    let recvd_0rtt_reject = |e| e == ConnectionEvent::ZeroRttRejected;
    assert!(client.events().any(recvd_0rtt_reject));

    // Server consume client_fin
    let server_ack = server.process(client_fin.dgram(), now());
    assert!(server_ack.as_dgram_ref().is_some());
    let client_out = client.process(server_ack.dgram(), now());
    assert!(client_out.as_dgram_ref().is_none());

    // ...and the client stream should be gone.
    let res = client.stream_send(stream_id, MESSAGE);
    assert!(res.is_err());
    assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

    // Open a new stream and send data. StreamId should start with 0.
    let stream_id_after_reject = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(stream_id, stream_id_after_reject);
    client.stream_send(stream_id_after_reject, MESSAGE).unwrap();
    let client_after_reject = client.process(None, now()).dgram();
    assert!(client_after_reject.is_some());

    // The server should receive new stream
    server.process_input(client_after_reject.unwrap(), now());
    assert!(server.events().any(recvd_stream_evt));
}

#[test]
fn zero_rtt_update_flow_control() {
    const LOW: u64 = 3;
    const HIGH: u64 = 10;
    #[allow(clippy::cast_possible_truncation)]
    const MESSAGE: &[u8] = &[0; HIGH as usize];

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default()
            .max_stream_data(StreamType::UniDi, true, LOW)
            .max_stream_data(StreamType::BiDi, true, LOW),
    );
    connect(&mut client, &mut server);

    let token = exchange_ticket(&mut client, &mut server, now());
    let mut client = default_client();
    client
        .enable_resumption(now(), token)
        .expect("should set token");
    let mut server = new_server(
        ConnectionParameters::default()
            .max_stream_data(StreamType::UniDi, true, HIGH)
            .max_stream_data(StreamType::BiDi, true, HIGH)
            .versions(client.version, Version::all()),
    );

    // Stream limits should be low for 0-RTT.
    let client_hs = client.process(None, now()).dgram();
    let uni_stream = client.stream_create(StreamType::UniDi).unwrap();
    assert!(!client.stream_send_atomic(uni_stream, MESSAGE).unwrap());
    let bidi_stream = client.stream_create(StreamType::BiDi).unwrap();
    assert!(!client.stream_send_atomic(bidi_stream, MESSAGE).unwrap());

    // Now get the server transport parameters.
    let server_hs = server.process(client_hs, now()).dgram();
    client.process_input(server_hs.unwrap(), now());

    // The streams should report a writeable event.
    let mut uni_stream_event = false;
    let mut bidi_stream_event = false;
    for e in client.events() {
        if let ConnectionEvent::SendStreamWritable { stream_id } = e {
            if stream_id.is_uni() {
                uni_stream_event = true;
            } else {
                bidi_stream_event = true;
            }
        }
    }
    assert!(uni_stream_event);
    assert!(bidi_stream_event);
    // But no MAX_STREAM_DATA frame was received.
    assert_eq!(client.stats().frame_rx.max_stream_data, 0);

    // And the new limit applies.
    assert!(client.stream_send_atomic(uni_stream, MESSAGE).unwrap());
    assert!(client.stream_send_atomic(bidi_stream, MESSAGE).unwrap());
}
