// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control
#![deny(clippy::pedantic)]

use crate::cc::new_reno::NewReno;
use crate::cc::{ClassicCongestionControl, CongestionControl, CWND_INITIAL, MAX_DATAGRAM_SIZE};
use crate::packet::PacketType;
use crate::tracking::SentPacket;
use std::time::Duration;
use test_fixture::now;

const PTO: Duration = Duration::from_millis(100);
const RTT: Duration = Duration::from_millis(98);

fn cwnd_is_default(cc: &ClassicCongestionControl<NewReno>) {
    assert_eq!(cc.cwnd(), CWND_INITIAL);
    assert_eq!(cc.ssthresh(), usize::MAX);
}

fn cwnd_is_halved(cc: &ClassicCongestionControl<NewReno>) {
    assert_eq!(cc.cwnd(), CWND_INITIAL / 2);
    assert_eq!(cc.ssthresh(), CWND_INITIAL / 2);
}

#[test]
fn issue_876() {
    let mut cc = ClassicCongestionControl::new(NewReno::default());
    let time_now = now();
    let time_before = time_now.checked_sub(Duration::from_millis(100)).unwrap();
    let time_after = time_now + Duration::from_millis(150);

    let sent_packets = &[
        SentPacket::new(
            PacketType::Short,
            1,                     // pn
            time_before,           // time sent
            true,                  // ack eliciting
            Vec::new(),            // tokens
            MAX_DATAGRAM_SIZE - 1, // size
        ),
        SentPacket::new(
            PacketType::Short,
            2,                     // pn
            time_before,           // time sent
            true,                  // ack eliciting
            Vec::new(),            // tokens
            MAX_DATAGRAM_SIZE - 2, // size
        ),
        SentPacket::new(
            PacketType::Short,
            3,                 // pn
            time_before,       // time sent
            true,              // ack eliciting
            Vec::new(),        // tokens
            MAX_DATAGRAM_SIZE, // size
        ),
        SentPacket::new(
            PacketType::Short,
            4,                 // pn
            time_before,       // time sent
            true,              // ack eliciting
            Vec::new(),        // tokens
            MAX_DATAGRAM_SIZE, // size
        ),
        SentPacket::new(
            PacketType::Short,
            5,                 // pn
            time_before,       // time sent
            true,              // ack eliciting
            Vec::new(),        // tokens
            MAX_DATAGRAM_SIZE, // size
        ),
        SentPacket::new(
            PacketType::Short,
            6,                 // pn
            time_before,       // time sent
            true,              // ack eliciting
            Vec::new(),        // tokens
            MAX_DATAGRAM_SIZE, // size
        ),
        SentPacket::new(
            PacketType::Short,
            7,                     // pn
            time_after,            // time sent
            true,                  // ack eliciting
            Vec::new(),            // tokens
            MAX_DATAGRAM_SIZE - 3, // size
        ),
    ];

    // Send some more packets so that the cc is not app-limited.
    for p in &sent_packets[..6] {
        cc.on_packet_sent(p);
    }
    assert_eq!(cc.acked_bytes(), 0);
    cwnd_is_default(&cc);
    assert_eq!(cc.bytes_in_flight(), 6 * MAX_DATAGRAM_SIZE - 3);

    cc.on_packets_lost(Some(time_now), None, PTO, &sent_packets[0..1]);

    // We are now in recovery
    assert!(cc.recovery_packet());
    assert_eq!(cc.acked_bytes(), 0);
    cwnd_is_halved(&cc);
    assert_eq!(cc.bytes_in_flight(), 5 * MAX_DATAGRAM_SIZE - 2);

    // Send a packet after recovery starts
    cc.on_packet_sent(&sent_packets[6]);
    assert!(!cc.recovery_packet());
    cwnd_is_halved(&cc);
    assert_eq!(cc.acked_bytes(), 0);
    assert_eq!(cc.bytes_in_flight(), 6 * MAX_DATAGRAM_SIZE - 5);

    // and ack it. cwnd increases slightly
    cc.on_packets_acked(&sent_packets[6..], RTT, time_now);
    assert_eq!(cc.acked_bytes(), sent_packets[6].size);
    cwnd_is_halved(&cc);
    assert_eq!(cc.bytes_in_flight(), 5 * MAX_DATAGRAM_SIZE - 2);

    // Packet from before is lost. Should not hurt cwnd.
    cc.on_packets_lost(Some(time_now), None, PTO, &sent_packets[1..2]);
    assert!(!cc.recovery_packet());
    assert_eq!(cc.acked_bytes(), sent_packets[6].size);
    cwnd_is_halved(&cc);
    assert_eq!(cc.bytes_in_flight(), 4 * MAX_DATAGRAM_SIZE);
}

#[test]
// https://github.com/mozilla/neqo/pull/1465
fn issue_1465() {
    let mut cc = ClassicCongestionControl::new(NewReno::default());
    let mut pn = 0;
    let mut now = now();
    let mut next_packet = |now| {
        let p = SentPacket::new(
            PacketType::Short,
            pn,                // pn
            now,               // time_sent
            true,              // ack eliciting
            Vec::new(),        // tokens
            MAX_DATAGRAM_SIZE, // size
        );
        pn += 1;
        p
    };
    let mut send_next = |cc: &mut ClassicCongestionControl<NewReno>, now| {
        let p = next_packet(now);
        cc.on_packet_sent(&p);
        p
    };

    let p1 = send_next(&mut cc, now);
    let p2 = send_next(&mut cc, now);
    let p3 = send_next(&mut cc, now);

    assert_eq!(cc.acked_bytes(), 0);
    cwnd_is_default(&cc);
    assert_eq!(cc.bytes_in_flight(), 3 * MAX_DATAGRAM_SIZE);

    // advance one rtt to detect lost packet there this simplifies the timers, because on_packet_loss
    // would only be called after RTO, but that is not relevant to the problem
    now += RTT;
    cc.on_packets_lost(Some(now), None, PTO, &[p1]);

    // We are now in recovery
    assert!(cc.recovery_packet());
    assert_eq!(cc.acked_bytes(), 0);
    cwnd_is_halved(&cc);
    assert_eq!(cc.bytes_in_flight(), 2 * MAX_DATAGRAM_SIZE);

    // Don't reduce the cwnd again on second packet loss
    cc.on_packets_lost(Some(now), None, PTO, &[p3]);
    assert_eq!(cc.acked_bytes(), 0);
    cwnd_is_halved(&cc); // still the same as after first packet loss
    assert_eq!(cc.bytes_in_flight(), MAX_DATAGRAM_SIZE);

    // the acked packets before on_packet_sent were the cause of
    // https://github.com/mozilla/neqo/pull/1465
    cc.on_packets_acked(&[p2], RTT, now);

    assert_eq!(cc.bytes_in_flight(), 0);

    // send out recovery packet and get it acked to get out of recovery state
    let p4 = send_next(&mut cc, now);
    cc.on_packet_sent(&p4);
    now += RTT;
    cc.on_packets_acked(&[p4], RTT, now);

    // do the same as in the first rtt but now the bug appears
    let p5 = send_next(&mut cc, now);
    let p6 = send_next(&mut cc, now);
    now += RTT;

    let cur_cwnd = cc.cwnd();
    cc.on_packets_lost(Some(now), None, PTO, &[p5]);

    // go back into recovery
    assert!(cc.recovery_packet());
    assert_eq!(cc.cwnd(), cur_cwnd / 2);
    assert_eq!(cc.acked_bytes(), 0);
    assert_eq!(cc.bytes_in_flight(), 2 * MAX_DATAGRAM_SIZE);

    // this shouldn't introduce further cwnd reduction, but it did before https://github.com/mozilla/neqo/pull/1465
    cc.on_packets_lost(Some(now), None, PTO, &[p6]);
    assert_eq!(cc.cwnd(), cur_cwnd / 2);
}
