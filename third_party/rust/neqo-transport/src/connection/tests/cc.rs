// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::Output;
use super::{
    ack_bytes, assert_full_cwnd, connect_rtt_idle, cwnd, cwnd_avail, cwnd_packets, default_client,
    default_server, fill_cwnd, induce_persistent_congestion, send_something, DEFAULT_RTT,
    FORCE_IDLE_CLIENT_1RTT_PACKETS, POST_HANDSHAKE_CWND,
};
use crate::cc::MAX_DATAGRAM_SIZE;
use crate::packet::PacketNumber;
use crate::recovery::{ACK_ONLY_SIZE_LIMIT, PACKET_THRESHOLD};
use crate::sender::PACING_BURST_SIZE;
use crate::stream_id::StreamType;
use crate::tracking::DEFAULT_ACK_PACKET_TOLERANCE;

use neqo_common::{qdebug, qinfo, Datagram};
use std::convert::TryFrom;
use std::mem;
use std::time::Duration;

#[test]
/// Verify initial CWND is honored.
fn cc_slow_start() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Try to send a lot of data
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    let (c_tx_dgrams, _) = fill_cwnd(&mut client, stream_id, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
    assert!(cwnd_avail(&client) < ACK_ONLY_SIZE_LIMIT);
}

#[test]
/// Verify that CC moves to cong avoidance when a packet is marked lost.
fn cc_slow_start_to_cong_avoidance_recovery_period() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Create stream 0
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    assert_eq!(stream_id, 0);

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, stream_id, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
    // Predict the packet number of the last packet sent.
    // We have already sent packets in `connect_rtt_idle`,
    // so include a fudge factor.
    let flight1_largest =
        PacketNumber::try_from(c_tx_dgrams.len() + FORCE_IDLE_CLIENT_1RTT_PACKETS).unwrap();

    // Server: Receive and generate ack
    now += DEFAULT_RTT / 2;
    let s_ack = ack_bytes(&mut server, stream_id, c_tx_dgrams, now);
    assert_eq!(
        server.stats().frame_tx.largest_acknowledged,
        flight1_largest
    );

    // Client: Process ack
    now += DEFAULT_RTT / 2;
    client.process_input(s_ack, now);
    assert_eq!(
        client.stats().frame_rx.largest_acknowledged,
        flight1_largest
    );

    // Client: send more
    let (mut c_tx_dgrams, mut now) = fill_cwnd(&mut client, stream_id, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND * 2);
    let flight2_largest = flight1_largest + u64::try_from(c_tx_dgrams.len()).unwrap();

    // Server: Receive and generate ack again, but drop first packet
    now += DEFAULT_RTT / 2;
    c_tx_dgrams.remove(0);
    let s_ack = ack_bytes(&mut server, stream_id, c_tx_dgrams, now);
    assert_eq!(
        server.stats().frame_tx.largest_acknowledged,
        flight2_largest
    );

    // Client: Process ack
    now += DEFAULT_RTT / 2;
    client.process_input(s_ack, now);
    assert_eq!(
        client.stats().frame_rx.largest_acknowledged,
        flight2_largest
    );
}

#[test]
/// Verify that CC stays in recovery period when packet sent before start of
/// recovery period is acked.
fn cc_cong_avoidance_recovery_period_unchanged() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Create stream 0
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    assert_eq!(stream_id, 0);

    // Buffer up lot of data and generate packets
    let (mut c_tx_dgrams, now) = fill_cwnd(&mut client, stream_id, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Drop 0th packet. When acked, this should put client into CARP.
    c_tx_dgrams.remove(0);

    let c_tx_dgrams2 = c_tx_dgrams.split_off(5);

    // Server: Receive and generate ack
    let s_ack = ack_bytes(&mut server, stream_id, c_tx_dgrams, now);
    client.process_input(s_ack, now);

    let cwnd1 = cwnd(&client);

    // Generate ACK for more received packets
    let s_ack = ack_bytes(&mut server, stream_id, c_tx_dgrams2, now);

    // ACK more packets but they were sent before end of recovery period
    client.process_input(s_ack, now);

    // cwnd should not have changed since ACKed packets were sent before
    // recovery period expired
    let cwnd2 = cwnd(&client);
    assert_eq!(cwnd1, cwnd2);
}

#[test]
/// Ensure that a single packet is sent after entering recovery, even
/// when that exceeds the available congestion window.
fn single_packet_on_recovery() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Drop a few packets, up to the reordering threshold.
    for _ in 0..PACKET_THRESHOLD {
        let _dropped = send_something(&mut client, now);
    }
    let delivered = send_something(&mut client, now);

    // Now fill the congestion window.
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    assert_eq!(stream_id, 0);
    let (_, now) = fill_cwnd(&mut client, stream_id, now);
    assert!(cwnd_avail(&client) < ACK_ONLY_SIZE_LIMIT);

    // Acknowledge just one packet and cause one packet to be declared lost.
    // The length is the amount of credit the client should have.
    let ack = server.process(Some(delivered), now).dgram();
    assert!(ack.is_some());

    // The client should see the loss and enter recovery.
    // As there are many outstanding packets, there should be no available cwnd.
    client.process_input(ack.unwrap(), now);
    assert_eq!(cwnd_avail(&client), 0);

    // The client should send one packet, ignoring the cwnd.
    let dgram = client.process_output(now).dgram();
    assert!(dgram.is_some());
}

#[test]
/// Verify that CC moves out of recovery period when packet sent after start
/// of recovery period is acked.
fn cc_cong_avoidance_recovery_period_to_cong_avoidance() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Create stream 0
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    assert_eq!(stream_id, 0);

    // Buffer up lot of data and generate packets
    let (mut c_tx_dgrams, mut now) = fill_cwnd(&mut client, stream_id, now);

    // Drop 0th packet. When acked, this should put client into CARP.
    c_tx_dgrams.remove(0);

    // Server: Receive and generate ack
    now += DEFAULT_RTT / 2;
    let s_ack = ack_bytes(&mut server, stream_id, c_tx_dgrams, now);

    // Client: Process ack
    now += DEFAULT_RTT / 2;
    client.process_input(s_ack, now);

    // Should be in CARP now.
    now += DEFAULT_RTT / 2;
    qinfo!("moving to congestion avoidance {}", cwnd(&client));

    // Now make sure that we increase congestion window according to the
    // accurate byte counting version of congestion avoidance.
    // Check over several increases to be sure.
    let mut expected_cwnd = cwnd(&client);
    // Fill cwnd.
    let (mut c_tx_dgrams, next_now) = fill_cwnd(&mut client, stream_id, now);
    now = next_now;
    for i in 0..5 {
        qinfo!("iteration {}", i);

        let c_tx_size: usize = c_tx_dgrams.iter().map(|d| d.len()).sum();
        qinfo!(
            "client sending {} bytes into cwnd of {}",
            c_tx_size,
            cwnd(&client)
        );
        assert_eq!(c_tx_size, expected_cwnd);

        // As acks arrive we will continue filling cwnd and save all packets
        // from this cycle will be stored in next_c_tx_dgrams.
        let mut next_c_tx_dgrams: Vec<Datagram> = Vec::new();

        // Until we process all the packets, the congestion window remains the same.
        // Note that we need the client to process ACK frames in stages, so split the
        // datagrams into two, ensuring that we allow for an ACK for each batch.
        let most = c_tx_dgrams.len() - usize::try_from(DEFAULT_ACK_PACKET_TOLERANCE).unwrap() - 1;
        let s_ack = ack_bytes(&mut server, stream_id, c_tx_dgrams.drain(..most), now);
        assert_eq!(cwnd(&client), expected_cwnd);
        client.process_input(s_ack, now);
        // make sure to fill cwnd again.
        let (mut new_pkts, next_now) = fill_cwnd(&mut client, stream_id, now);
        now = next_now;
        next_c_tx_dgrams.append(&mut new_pkts);

        let s_ack = ack_bytes(&mut server, stream_id, c_tx_dgrams, now);
        assert_eq!(cwnd(&client), expected_cwnd);
        client.process_input(s_ack, now);
        // make sure to fill cwnd again.
        let (mut new_pkts, next_now) = fill_cwnd(&mut client, stream_id, now);
        now = next_now;
        next_c_tx_dgrams.append(&mut new_pkts);

        expected_cwnd += MAX_DATAGRAM_SIZE;
        assert_eq!(cwnd(&client), expected_cwnd);
        c_tx_dgrams = next_c_tx_dgrams;
    }
}

#[test]
/// Verify transition to persistent congestion state if conditions are met.
fn cc_slow_start_to_persistent_congestion_no_acks() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    let stream = client.stream_create(StreamType::BiDi).unwrap();

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, stream, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Server: Receive and generate ack
    now += DEFAULT_RTT / 2;
    mem::drop(ack_bytes(&mut server, stream, c_tx_dgrams, now));

    // ACK lost.
    induce_persistent_congestion(&mut client, &mut server, stream, now);
}

#[test]
/// Verify transition to persistent congestion state if conditions are met.
fn cc_slow_start_to_persistent_congestion_some_acks() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Create stream 0
    let stream = client.stream_create(StreamType::BiDi).unwrap();

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, stream, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Server: Receive and generate ack
    now += Duration::from_millis(100);
    let s_ack = ack_bytes(&mut server, stream, c_tx_dgrams, now);

    now += Duration::from_millis(100);
    client.process_input(s_ack, now);

    // send bytes that will be lost
    let (_, next_now) = fill_cwnd(&mut client, stream, now);
    now = next_now + Duration::from_millis(100);

    induce_persistent_congestion(&mut client, &mut server, stream, now);
}

#[test]
/// Verify persistent congestion moves to slow start after recovery period
/// ends.
fn cc_persistent_congestion_to_slow_start() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Create stream 0
    let stream = client.stream_create(StreamType::BiDi).unwrap();

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, stream, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Server: Receive and generate ack
    now += Duration::from_millis(10);
    mem::drop(ack_bytes(&mut server, stream, c_tx_dgrams, now));

    // ACK lost.

    now = induce_persistent_congestion(&mut client, &mut server, stream, now);

    // New part of test starts here

    now += Duration::from_millis(10);

    // Send packets from after start of CARP
    let (c_tx_dgrams, next_now) = fill_cwnd(&mut client, stream, now);
    assert_eq!(c_tx_dgrams.len(), 2);

    // Server: Receive and generate ack
    now = next_now + Duration::from_millis(100);
    let s_ack = ack_bytes(&mut server, stream, c_tx_dgrams, now);

    // No longer in CARP. (pkts acked from after start of CARP)
    // Should be in slow start now.
    client.process_input(s_ack, now);

    // ACKing 2 packets should let client send 4.
    let (c_tx_dgrams, _) = fill_cwnd(&mut client, stream, now);
    assert_eq!(c_tx_dgrams.len(), 4);
}

#[test]
fn ack_are_not_cc() {
    let mut client = default_client();
    let mut server = default_server();
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Create a stream
    let stream = client.stream_create(StreamType::BiDi).unwrap();
    assert_eq!(stream, 0);

    // Buffer up lot of data and generate packets, so that cc window is filled.
    let (c_tx_dgrams, now) = fill_cwnd(&mut client, stream, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // The server hasn't received any of these packets yet, the server
    // won't ACK, but if it sends an ack-eliciting packet instead.
    qdebug!([server], "Sending ack-eliciting");
    let other_stream = server.stream_create(StreamType::BiDi).unwrap();
    assert_eq!(other_stream, 1);
    server.stream_send(other_stream, b"dropped").unwrap();
    let dropped_packet = server.process(None, now).dgram();
    assert!(dropped_packet.is_some()); // Now drop this one.

    // Now the server sends a packet that will force an ACK,
    // because the client will detect a gap.
    server.stream_send(other_stream, b"sent").unwrap();
    let ack_eliciting_packet = server.process(None, now).dgram();
    assert!(ack_eliciting_packet.is_some());

    // The client can ack the server packet even if cc windows is full.
    qdebug!([client], "Process ack-eliciting");
    let ack_pkt = client.process(ack_eliciting_packet, now).dgram();
    assert!(ack_pkt.is_some());
    qdebug!([server], "Handle ACK");
    let prev_ack_count = server.stats().frame_rx.ack;
    server.process_input(ack_pkt.unwrap(), now);
    assert_eq!(server.stats().frame_rx.ack, prev_ack_count + 1);
}

#[test]
fn pace() {
    const DATA: &[u8] = &[0xcc; 4_096];
    let mut client = default_client();
    let mut server = default_server();
    let mut now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Now fill up the pipe and watch it trickle out.
    let stream = client.stream_create(StreamType::BiDi).unwrap();
    loop {
        let written = client.stream_send(stream, DATA).unwrap();
        if written < DATA.len() {
            break;
        }
    }
    let mut count = 0;
    // We should get a burst at first.
    // The first packet is not subject to pacing as there are no bytes in flight.
    // After that we allow the burst to continue up to a number of packets (2).
    for _ in 0..=PACING_BURST_SIZE {
        let dgram = client.process_output(now).dgram();
        assert!(dgram.is_some());
        count += 1;
    }
    let gap = client.process_output(now).callback();
    assert_ne!(gap, Duration::new(0, 0));
    for _ in (1 + PACING_BURST_SIZE)..cwnd_packets(POST_HANDSHAKE_CWND) {
        match client.process_output(now) {
            Output::Callback(t) => assert_eq!(t, gap),
            Output::Datagram(_) => {
                // The last packet might not be paced.
                count += 1;
                break;
            }
            Output::None => panic!(),
        }
        now += gap;
        let dgram = client.process_output(now).dgram();
        assert!(dgram.is_some());
        count += 1;
    }
    let dgram = client.process_output(now).dgram();
    assert!(dgram.is_none());
    assert_eq!(count, cwnd_packets(POST_HANDSHAKE_CWND));
    let fin = client.process_output(now).callback();
    assert_ne!(fin, Duration::new(0, 0));
    assert_ne!(fin, gap);
}
