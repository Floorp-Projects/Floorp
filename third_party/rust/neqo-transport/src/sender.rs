// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control
#![deny(clippy::pedantic)]
#![allow(clippy::module_name_repetitions)]

use crate::cc::{
    ClassicCongestionControl, CongestionControl, CongestionControlAlgorithm, NewReno,
    MAX_DATAGRAM_SIZE,
};
use crate::pace::Pacer;
use crate::tracking::SentPacket;
use neqo_common::qlog::NeqoQlog;

use std::fmt::{self, Debug, Display};
use std::time::{Duration, Instant};

/// The number of packets we allow to burst from the pacer.
pub const PACING_BURST_SIZE: usize = 2;

#[derive(Debug)]
pub struct PacketSender {
    cc: Box<dyn CongestionControl>,
    pacer: Option<Pacer>,
}

impl Display for PacketSender {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.cc)?;
        if let Some(p) = &self.pacer {
            write!(f, " {}", p)?;
        }
        Ok(())
    }
}

impl PacketSender {
    #[must_use]
    pub fn new(alg: CongestionControlAlgorithm) -> Self {
        Self {
            cc: match alg {
                CongestionControlAlgorithm::NewReno => {
                    Box::new(ClassicCongestionControl::new(NewReno::default()))
                }
            },
            pacer: None,
        }
    }

    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.cc.set_qlog(qlog);
    }

    #[cfg(test)]
    #[must_use]
    pub fn cwnd(&self) -> usize {
        self.cc.cwnd()
    }

    #[must_use]
    pub fn cwnd_avail(&self) -> usize {
        self.cc.cwnd_avail()
    }

    // Multi-packet version of OnPacketAckedCC
    pub fn on_packets_acked(&mut self, acked_pkts: &[SentPacket]) {
        self.cc.on_packets_acked(acked_pkts);
    }

    pub fn on_packets_lost(
        &mut self,
        first_rtt_sample_time: Option<Instant>,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) {
        self.cc.on_packets_lost(
            first_rtt_sample_time,
            prev_largest_acked_sent,
            pto,
            lost_packets,
        );
    }

    pub fn discard(&mut self, pkt: &SentPacket) {
        self.cc.discard(pkt);
    }

    pub fn on_packet_sent(&mut self, pkt: &SentPacket, rtt: Duration) {
        self.pacer
            .as_mut()
            .unwrap()
            .spend(pkt.time_sent, rtt, self.cc.cwnd(), pkt.size);
        self.cc.on_packet_sent(pkt);
    }

    pub fn start_pacer(&mut self, now: Instant) {
        // Start the pacer with a small burst size.
        self.pacer = Some(Pacer::new(
            now,
            MAX_DATAGRAM_SIZE * PACING_BURST_SIZE,
            MAX_DATAGRAM_SIZE,
        ));
    }

    #[must_use]
    pub fn next_paced(&self, rtt: Duration) -> Option<Instant> {
        // Only pace if there are bytes in flight.
        if self.cc.bytes_in_flight() > 0 {
            Some(self.pacer.as_ref().unwrap().next(rtt, self.cc.cwnd()))
        } else {
            None
        }
    }

    #[must_use]
    pub fn recovery_packet(&self) -> bool {
        self.cc.recovery_packet()
    }
}
