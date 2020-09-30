// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::{Connection, Output};
use super::{
    assert_full_cwnd, connect, connect_force_idle, connect_rtt_idle, cwnd_packets, default_client,
    default_server, fill_cwnd, AT_LEAST_PTO, POST_HANDSHAKE_CWND,
};
use crate::cc::{CWND_MIN, MAX_DATAGRAM_SIZE, PACING_BURST_SIZE};
use crate::frame::{Frame, StreamType};
use crate::packet::PacketNumber;
use crate::recovery::ACK_ONLY_SIZE_LIMIT;
use crate::stats::MAX_PTO_COUNTS;
use crate::tparams::{self, TransportParameter};
use crate::tracking::{PNSpace, MAX_UNACKED_PKTS};

use neqo_common::{qdebug, qtrace, Datagram};
use std::convert::TryFrom;
use std::time::{Duration, Instant};
use test_fixture::{self, now};

fn induce_persistent_congestion(
    client: &mut Connection,
    server: &mut Connection,
    mut now: Instant,
) -> Instant {
    // Note: wait some arbitrary time that should be longer than pto
    // timer. This is rather brittle.
    now += AT_LEAST_PTO;

    let mut pto_counts = [0; MAX_PTO_COUNTS];
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    qtrace!([client], "first PTO");
    let (c_tx_dgrams, next_now) = fill_cwnd(client, 0, now);
    now = next_now;
    assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

    pto_counts[0] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    qtrace!([client], "second PTO");
    now += AT_LEAST_PTO * 2;
    let (c_tx_dgrams, next_now) = fill_cwnd(client, 0, now);
    now = next_now;
    assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

    pto_counts[0] = 0;
    pto_counts[1] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    qtrace!([client], "third PTO");
    now += AT_LEAST_PTO * 4;
    let (c_tx_dgrams, next_now) = fill_cwnd(client, 0, now);
    now = next_now;
    assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

    pto_counts[1] = 0;
    pto_counts[2] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // Generate ACK
    let (s_tx_dgram, _) = ack_bytes(server, 0, c_tx_dgrams, now);

    // An ACK for the third PTO causes persistent congestion.
    for dgram in s_tx_dgram {
        client.process_input(dgram, now);
    }

    assert_eq!(client.loss_recovery.cwnd(), CWND_MIN);
    now
}

// Receive multiple packets and generate an ack-only packet.
fn ack_bytes<D>(
    dest: &mut Connection,
    stream: u64,
    in_dgrams: D,
    now: Instant,
) -> (Vec<Datagram>, Vec<Frame>)
where
    D: IntoIterator<Item = Datagram>,
    D::IntoIter: ExactSizeIterator,
{
    let mut srv_buf = [0; 4_096];
    let mut recvd_frames = Vec::new();

    let in_dgrams = in_dgrams.into_iter();
    qdebug!([dest], "ack_bytes {} datagrams", in_dgrams.len());
    for dgram in in_dgrams {
        recvd_frames.extend(dest.test_process_input(dgram, now));
    }

    loop {
        let (bytes_read, _fin) = dest.stream_recv(stream, &mut srv_buf).unwrap();
        qtrace!([dest], "ack_bytes read {} bytes", bytes_read);
        if bytes_read == 0 {
            break;
        }
    }

    let mut tx_dgrams = Vec::new();
    while let Output::Datagram(dg) = dest.process_output(now) {
        tx_dgrams.push(dg);
    }

    assert!((tx_dgrams.len() == 1) || (tx_dgrams.len() == 2));

    (
        tx_dgrams,
        recvd_frames.into_iter().map(|(f, _e)| f).collect(),
    )
}

#[test]
/// Verify initial CWND is honored.
fn cc_slow_start() {
    let mut client = default_client();
    let mut server = default_server();

    server
        .set_local_tparam(
            tparams::INITIAL_MAX_DATA,
            TransportParameter::Integer(65536),
        )
        .unwrap();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    // Try to send a lot of data
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
    let (c_tx_dgrams, _) = fill_cwnd(&mut client, 2, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
    assert!(client.loss_recovery.cwnd_avail() < ACK_ONLY_SIZE_LIMIT);
}

#[test]
/// Verify that CC moves to cong avoidance when a packet is marked lost.
fn cc_slow_start_to_cong_avoidance_recovery_period() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    // Create stream 0
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now());
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
    // Predict the packet number of the last packet sent.
    // We have already sent one packet in `connect_force_idle` (an ACK),
    // so this will be equal to the number of packets in this flight.
    let flight1_largest = PacketNumber::try_from(c_tx_dgrams.len()).unwrap();

    // Server: Receive and generate ack
    let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

    // Client: Process ack
    for dgram in s_tx_dgram {
        let recvd_frames = client.test_process_input(dgram, now);

        // Verify that server-sent frame was what we thought.
        if let (
            Frame::Ack {
                largest_acknowledged,
                ..
            },
            PNSpace::ApplicationData,
        ) = recvd_frames[0]
        {
            assert_eq!(largest_acknowledged, flight1_largest);
        } else {
            panic!("Expected an application ACK");
        }
    }

    // Client: send more
    let (mut c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now);
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND * 2);
    let flight2_largest = flight1_largest + u64::try_from(c_tx_dgrams.len()).unwrap();

    // Server: Receive and generate ack again, but drop first packet
    c_tx_dgrams.remove(0);
    let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

    // Client: Process ack
    for dgram in s_tx_dgram {
        let recvd_frames = client.test_process_input(dgram, now);

        // Verify that server-sent frame was what we thought.
        if let (
            Frame::Ack {
                largest_acknowledged,
                ..
            },
            PNSpace::ApplicationData,
        ) = recvd_frames[0]
        {
            assert_eq!(largest_acknowledged, flight2_largest);
        } else {
            panic!("Expected an application ACK");
        }
    }

    // If we just triggered cong avoidance, these should be equal
    assert_eq!(client.loss_recovery.cwnd(), client.loss_recovery.ssthresh());
}

#[test]
/// Verify that CC stays in recovery period when packet sent before start of
/// recovery period is acked.
fn cc_cong_avoidance_recovery_period_unchanged() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    // Create stream 0
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

    // Buffer up lot of data and generate packets
    let (mut c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now());
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Drop 0th packet. When acked, this should put client into CARP.
    c_tx_dgrams.remove(0);

    let c_tx_dgrams2 = c_tx_dgrams.split_off(5);

    // Server: Receive and generate ack
    let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);
    for dgram in s_tx_dgram {
        client.test_process_input(dgram, now);
    }

    // If we just triggered cong avoidance, these should be equal
    let cwnd1 = client.loss_recovery.cwnd();
    assert_eq!(cwnd1, client.loss_recovery.ssthresh());

    // Generate ACK for more received packets
    let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams2, now);

    // ACK more packets but they were sent before end of recovery period
    for dgram in s_tx_dgram {
        client.test_process_input(dgram, now);
    }

    // cwnd should not have changed since ACKed packets were sent before
    // recovery period expired
    let cwnd2 = client.loss_recovery.cwnd();
    assert_eq!(cwnd1, cwnd2);
}

#[test]
/// Verify that CC moves out of recovery period when packet sent after start
/// of recovery period is acked.
fn cc_cong_avoidance_recovery_period_to_cong_avoidance() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // Create stream 0
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

    // Buffer up lot of data and generate packets
    let (mut c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());

    // Drop 0th packet. When acked, this should put client into CARP.
    c_tx_dgrams.remove(0);

    // Server: Receive and generate ack
    let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

    // Client: Process ack
    for dgram in s_tx_dgram {
        client.test_process_input(dgram, now);
    }

    // Should be in CARP now.

    now += Duration::from_millis(10); // Time passes. CARP -> CA

    // Now make sure that we increase congestion window according to the
    // accurate byte counting version of congestion avoidance.
    // Check over several increases to be sure.
    let mut expected_cwnd = client.loss_recovery.cwnd();
    for i in 0..5 {
        println!("{}", i);
        // Client: Send more data
        let (mut c_tx_dgrams, next_now) = fill_cwnd(&mut client, 0, now);
        now = next_now;

        let c_tx_size: usize = c_tx_dgrams.iter().map(|d| d.len()).sum();
        println!(
            "client sending {} bytes into cwnd of {}",
            c_tx_size,
            client.loss_recovery.cwnd()
        );
        assert_eq!(c_tx_size, expected_cwnd);

        // Until we process all the packets, the congestion window remains the same.
        // Note that we need the client to process ACK frames in stages, so split the
        // datagrams into two, ensuring that we allow for an ACK for each batch.
        let most = c_tx_dgrams.len() - MAX_UNACKED_PKTS - 1;
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams.drain(..most), now);
        for dgram in s_tx_dgram {
            assert_eq!(client.loss_recovery.cwnd(), expected_cwnd);
            client.process_input(dgram, now);
        }
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);
        for dgram in s_tx_dgram {
            assert_eq!(client.loss_recovery.cwnd(), expected_cwnd);
            client.process_input(dgram, now);
        }
        expected_cwnd += MAX_DATAGRAM_SIZE;
        assert_eq!(client.loss_recovery.cwnd(), expected_cwnd);
    }
}

#[test]
/// Verify transition to persistent congestion state if conditions are met.
fn cc_slow_start_to_persistent_congestion_no_acks() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    // Create stream 0
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Server: Receive and generate ack
    now += Duration::from_millis(100);
    let (_s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

    // ACK lost.

    induce_persistent_congestion(&mut client, &mut server, now);
}

#[test]
/// Verify transition to persistent congestion state if conditions are met.
fn cc_slow_start_to_persistent_congestion_some_acks() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    // Create stream 0
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Server: Receive and generate ack
    now += Duration::from_millis(100);
    let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

    now += Duration::from_millis(100);
    for dgram in s_tx_dgram {
        client.process_input(dgram, now);
    }

    // send bytes that will be lost
    let (_, next_now) = fill_cwnd(&mut client, 0, now);
    now = next_now + Duration::from_millis(100);

    induce_persistent_congestion(&mut client, &mut server, now);
}

#[test]
/// Verify persistent congestion moves to slow start after recovery period
/// ends.
fn cc_persistent_congestion_to_slow_start() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    // Create stream 0
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

    // Buffer up lot of data and generate packets
    let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // Server: Receive and generate ack
    now += Duration::from_millis(10);
    let _ = ack_bytes(&mut server, 0, c_tx_dgrams, now);

    // ACK lost.

    now = induce_persistent_congestion(&mut client, &mut server, now);

    // New part of test starts here

    now += Duration::from_millis(10);

    // Send packets from after start of CARP
    let (c_tx_dgrams, next_now) = fill_cwnd(&mut client, 0, now);
    assert_eq!(c_tx_dgrams.len(), 2);

    // Server: Receive and generate ack
    now = next_now + Duration::from_millis(100);
    let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

    // No longer in CARP. (pkts acked from after start of CARP)
    // Should be in slow start now.
    for dgram in s_tx_dgram {
        client.test_process_input(dgram, now);
    }

    // ACKing 2 packets should let client send 4.
    let (c_tx_dgrams, _) = fill_cwnd(&mut client, 0, now);
    assert_eq!(c_tx_dgrams.len(), 4);
}

#[test]
fn ack_are_not_cc() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    // Create a stream
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

    // Buffer up lot of data and generate packets, so that cc window is filled.
    let (c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now());
    assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

    // The server hasn't received any of these packets yet, the server
    // won't ACK, but if it sends an ack-eliciting packet instead.
    qdebug!([server], "Sending ack-eliciting");
    assert_eq!(server.stream_create(StreamType::BiDi).unwrap(), 1);
    server.stream_send(1, b"dropped").unwrap();
    let dropped_packet = server.process(None, now).dgram();
    assert!(dropped_packet.is_some()); // Now drop this one.

    // Now the server sends a packet that will force an ACK,
    // because the client will detect a gap.
    server.stream_send(1, b"sent").unwrap();
    let ack_eliciting_packet = server.process(None, now).dgram();
    assert!(ack_eliciting_packet.is_some());

    // The client can ack the server packet even if cc windows is full.
    qdebug!([client], "Process ack-eliciting");
    let ack_pkt = client.process(ack_eliciting_packet, now).dgram();
    assert!(ack_pkt.is_some());
    qdebug!([server], "Handle ACK");
    let frames = server.test_process_input(ack_pkt.unwrap(), now);
    assert_eq!(frames.len(), 1);
    assert!(matches!(
        frames[0],
        (Frame::Ack { .. }, PNSpace::ApplicationData)
    ));
}

#[test]
fn pace() {
    const RTT: Duration = Duration::from_millis(1000);
    const DATA: &[u8] = &[0xcc; 4_096];
    let mut client = default_client();
    let mut server = default_server();
    let mut now = connect_rtt_idle(&mut client, &mut server, RTT);

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
    for _ in 0..PACING_BURST_SIZE {
        let dgram = client.process_output(now).dgram();
        assert!(dgram.is_some());
        count += 1;
    }
    let gap = client.process_output(now).callback();
    assert_ne!(gap, Duration::new(0, 0));
    for _ in PACING_BURST_SIZE..cwnd_packets(POST_HANDSHAKE_CWND) {
        assert_eq!(client.process_output(now).callback(), gap);
        now += gap;
        let dgram = client.process_output(now).dgram();
        assert!(dgram.is_some());
        count += 1;
    }
    assert_eq!(count, cwnd_packets(POST_HANDSHAKE_CWND));
    let fin = client.process_output(now).callback();
    assert_ne!(fin, Duration::new(0, 0));
    assert_ne!(fin, gap);
}
