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
