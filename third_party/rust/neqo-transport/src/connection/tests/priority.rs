// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{
    super::{Connection, Error, Output},
    connect, default_client, default_server, fill_cwnd, maybe_authenticate,
};
use crate::{
    addr_valid::{AddressValidation, ValidateAddress},
    send_stream::{RetransmissionPriority, TransmissionPriority},
    ConnectionEvent, StreamId, StreamType,
};

use neqo_common::event::Provider;
use std::{cell::RefCell, mem, rc::Rc};
use test_fixture::{self, now};

const BLOCK_SIZE: usize = 4_096;

fn fill_stream(c: &mut Connection, id: StreamId) {
    loop {
        if c.stream_send(id, &[0x42; BLOCK_SIZE]).unwrap() < BLOCK_SIZE {
            return;
        }
    }
}

/// A receive stream cannot be prioritized (yet).
#[test]
fn receive_stream() {
    const MESSAGE: &[u8] = b"hello";
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(MESSAGE.len(), client.stream_send(id, MESSAGE).unwrap());
    let dgram = client.process_output(now()).dgram();

    server.process_input(dgram.unwrap(), now());
    assert_eq!(
        server
            .stream_priority(
                id,
                TransmissionPriority::default(),
                RetransmissionPriority::default()
            )
            .unwrap_err(),
        Error::InvalidStreamId,
        "Priority doesn't apply to inbound unidirectional streams"
    );

    // But the stream does exist and can be read.
    let mut buf = [0; 10];
    let (len, end) = server.stream_recv(id, &mut buf).unwrap();
    assert_eq!(MESSAGE, &buf[..len]);
    assert!(!end);
}

/// Higher priority streams get sent ahead of lower ones, even when
/// the higher priority stream is written to later.
#[test]
fn relative() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // id_normal is created first, but it is lower priority.
    let id_normal = client.stream_create(StreamType::UniDi).unwrap();
    fill_stream(&mut client, id_normal);
    let high = client.stream_create(StreamType::UniDi).unwrap();
    fill_stream(&mut client, high);
    client
        .stream_priority(
            high,
            TransmissionPriority::High,
            RetransmissionPriority::default(),
        )
        .unwrap();

    let dgram = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());

    // The "id_normal" stream will get a `NewStream` event, but no data.
    for e in server.events() {
        if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
            assert_ne!(stream_id, id_normal);
        }
    }
}

/// Check that changing priority has effect on the next packet that is sent.
#[test]
fn reprioritize() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // id_normal is created first, but it is lower priority.
    let id_normal = client.stream_create(StreamType::UniDi).unwrap();
    fill_stream(&mut client, id_normal);
    let id_high = client.stream_create(StreamType::UniDi).unwrap();
    fill_stream(&mut client, id_high);
    client
        .stream_priority(
            id_high,
            TransmissionPriority::High,
            RetransmissionPriority::default(),
        )
        .unwrap();

    let dgram = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());

    // The "id_normal" stream will get a `NewStream` event, but no data.
    for e in server.events() {
        if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
            assert_ne!(stream_id, id_normal);
        }
    }

    // When the high priority stream drops in priority, the streams are equal
    // priority and so their stream ID determines what is sent.
    client
        .stream_priority(
            id_high,
            TransmissionPriority::Normal,
            RetransmissionPriority::default(),
        )
        .unwrap();
    let dgram = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());

    for e in server.events() {
        if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
            assert_ne!(stream_id, id_high);
        }
    }
}

/// Retransmission can be prioritized differently (usually higher).
#[test]
fn repairing_loss() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);
    let mut now = now();

    // Send a few packets at low priority, lose one.
    let id_low = client.stream_create(StreamType::UniDi).unwrap();
    fill_stream(&mut client, id_low);
    client
        .stream_priority(
            id_low,
            TransmissionPriority::Low,
            RetransmissionPriority::Higher,
        )
        .unwrap();

    let _lost = client.process_output(now).dgram();
    for _ in 0..5 {
        match client.process_output(now) {
            Output::Datagram(d) => server.process_input(d, now),
            Output::Callback(delay) => now += delay,
            Output::None => unreachable!(),
        }
    }

    // Generate an ACK.  The first packet is now considered lost.
    let ack = server.process_output(now).dgram();
    _ = server.events().count(); // Drain events.

    let id_normal = client.stream_create(StreamType::UniDi).unwrap();
    fill_stream(&mut client, id_normal);

    let dgram = client.process(ack, now).dgram();
    assert_eq!(client.stats().lost, 1); // Client should have noticed the loss.
    server.process_input(dgram.unwrap(), now);

    // Only the low priority stream has data as the retransmission of the data from
    // the lost packet is now more important than new data from the high priority stream.
    for e in server.events() {
        println!("Event: {e:?}");
        if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
            assert_eq!(stream_id, id_low);
        }
    }

    // However, only the retransmission is prioritized.
    // Though this might contain some retransmitted data, as other frames might push
    // the retransmitted data into a second packet, it will also contain data from the
    // normal priority stream.
    let dgram = client.process_output(now).dgram();
    server.process_input(dgram.unwrap(), now);
    assert!(server.events().any(
        |e| matches!(e, ConnectionEvent::RecvStreamReadable { stream_id } if stream_id == id_normal),
    ));
}

#[test]
fn critical() {
    let mut client = default_client();
    let mut server = default_server();
    let now = now();

    // Rather than connect, send stream data in 0.5-RTT.
    // That allows this to test that critical streams pre-empt most frame types.
    let dgram = client.process_output(now).dgram();
    let dgram = server.process(dgram, now).dgram();
    client.process_input(dgram.unwrap(), now);
    maybe_authenticate(&mut client);

    let id = server.stream_create(StreamType::UniDi).unwrap();
    server
        .stream_priority(
            id,
            TransmissionPriority::Critical,
            RetransmissionPriority::default(),
        )
        .unwrap();

    // Can't use fill_cwnd here because the server is blocked on the amplification
    // limit, so it can't fill the congestion window.
    while server.stream_create(StreamType::UniDi).is_ok() {}

    fill_stream(&mut server, id);
    let stats_before = server.stats().frame_tx;
    let dgram = server.process_output(now).dgram();
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto);
    assert_eq!(stats_after.streams_blocked, 0);
    assert_eq!(stats_after.new_connection_id, 0);
    assert_eq!(stats_after.new_token, 0);
    assert_eq!(stats_after.handshake_done, 0);

    // Complete the handshake.
    let dgram = client.process(dgram, now).dgram();
    server.process_input(dgram.unwrap(), now);

    // Critical beats everything but HANDSHAKE_DONE.
    let stats_before = server.stats().frame_tx;
    mem::drop(fill_cwnd(&mut server, id, now));
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto);
    assert_eq!(stats_after.streams_blocked, 0);
    assert_eq!(stats_after.new_connection_id, 0);
    assert_eq!(stats_after.new_token, 0);
    assert_eq!(stats_after.handshake_done, 1);
}

#[test]
fn important() {
    let mut client = default_client();
    let mut server = default_server();
    let now = now();

    // Rather than connect, send stream data in 0.5-RTT.
    // That allows this to test that important streams pre-empt most frame types.
    let dgram = client.process_output(now).dgram();
    let dgram = server.process(dgram, now).dgram();
    client.process_input(dgram.unwrap(), now);
    maybe_authenticate(&mut client);

    let id = server.stream_create(StreamType::UniDi).unwrap();
    server
        .stream_priority(
            id,
            TransmissionPriority::Important,
            RetransmissionPriority::default(),
        )
        .unwrap();
    fill_stream(&mut server, id);

    // Important beats everything but flow control.
    // Make enough streams to get a STREAMS_BLOCKED frame out.
    while server.stream_create(StreamType::UniDi).is_ok() {}

    let stats_before = server.stats().frame_tx;
    let dgram = server.process_output(now).dgram();
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto);
    assert_eq!(stats_after.streams_blocked, 1);
    assert_eq!(stats_after.new_connection_id, 0);
    assert_eq!(stats_after.new_token, 0);
    assert_eq!(stats_after.handshake_done, 0);
    assert_eq!(stats_after.stream, stats_before.stream + 1);

    // Complete the handshake.
    let dgram = client.process(dgram, now).dgram();
    server.process_input(dgram.unwrap(), now);

    // Important beats everything but flow control.
    let stats_before = server.stats().frame_tx;
    mem::drop(fill_cwnd(&mut server, id, now));
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto);
    assert_eq!(stats_after.streams_blocked, 1);
    assert_eq!(stats_after.new_connection_id, 0);
    assert_eq!(stats_after.new_token, 0);
    assert_eq!(stats_after.handshake_done, 1);
    assert!(stats_after.stream > stats_before.stream);
}

#[test]
fn high_normal() {
    let mut client = default_client();
    let mut server = default_server();
    let now = now();

    // Rather than connect, send stream data in 0.5-RTT.
    // That allows this to test that important streams pre-empt most frame types.
    let dgram = client.process_output(now).dgram();
    let dgram = server.process(dgram, now).dgram();
    client.process_input(dgram.unwrap(), now);
    maybe_authenticate(&mut client);

    let id = server.stream_create(StreamType::UniDi).unwrap();
    server
        .stream_priority(
            id,
            TransmissionPriority::High,
            RetransmissionPriority::default(),
        )
        .unwrap();
    fill_stream(&mut server, id);

    // Important beats everything but flow control.
    // Make enough streams to get a STREAMS_BLOCKED frame out.
    while server.stream_create(StreamType::UniDi).is_ok() {}

    let stats_before = server.stats().frame_tx;
    let dgram = server.process_output(now).dgram();
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto);
    assert_eq!(stats_after.streams_blocked, 1);
    assert_eq!(stats_after.new_connection_id, 0);
    assert_eq!(stats_after.new_token, 0);
    assert_eq!(stats_after.handshake_done, 0);
    assert_eq!(stats_after.stream, stats_before.stream + 1);

    // Complete the handshake.
    let dgram = client.process(dgram, now).dgram();
    server.process_input(dgram.unwrap(), now);

    // High or Normal doesn't beat NEW_CONNECTION_ID,
    // but they beat CRYPTO/NEW_TOKEN.
    let stats_before = server.stats().frame_tx;
    server.send_ticket(now, &[]).unwrap();
    mem::drop(fill_cwnd(&mut server, id, now));
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto);
    assert_eq!(stats_after.streams_blocked, 1);
    assert_ne!(stats_after.new_connection_id, 0); // Note: > 0
    assert_eq!(stats_after.new_token, 0);
    assert_eq!(stats_after.handshake_done, 1);
    assert!(stats_after.stream > stats_before.stream);
}

#[test]
fn low() {
    let mut client = default_client();
    let mut server = default_server();
    let now = now();
    // Use address validation; note that we need to hold a strong reference
    // as the server will only hold a weak reference.
    let validation = Rc::new(RefCell::new(
        AddressValidation::new(now, ValidateAddress::Never).unwrap(),
    ));
    server.set_validation(Rc::clone(&validation));
    connect(&mut client, &mut server);

    let id = server.stream_create(StreamType::UniDi).unwrap();
    server
        .stream_priority(
            id,
            TransmissionPriority::Low,
            RetransmissionPriority::default(),
        )
        .unwrap();
    fill_stream(&mut server, id);

    // Send a session ticket and make it big enough to require a whole packet.
    // The resulting CRYPTO frame beats out the stream data.
    let stats_before = server.stats().frame_tx;
    server.send_ticket(now, &[0; 2048]).unwrap();
    mem::drop(server.process_output(now));
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto + 1);
    assert_eq!(stats_after.stream, stats_before.stream);

    // The above can't test if NEW_TOKEN wins because once that fits in a packet,
    // it is very hard to ensure that the STREAM frame won't also fit.
    // However, we can ensure that the next packet doesn't consist of just STREAM.
    let stats_before = server.stats().frame_tx;
    mem::drop(server.process_output(now));
    let stats_after = server.stats().frame_tx;
    assert_eq!(stats_after.crypto, stats_before.crypto + 1);
    assert_eq!(stats_after.new_token, 1);
    assert_eq!(stats_after.stream, stats_before.stream + 1);
}
