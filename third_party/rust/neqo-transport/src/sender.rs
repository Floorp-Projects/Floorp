// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control

#![allow(clippy::module_name_repetitions)]

use std::{
    fmt::{self, Debug, Display},
    time::{Duration, Instant},
};

use neqo_common::qlog::NeqoQlog;

use crate::{
    cc::{ClassicCongestionControl, CongestionControl, CongestionControlAlgorithm, Cubic, NewReno},
    pace::Pacer,
    rtt::RttEstimate,
    tracking::SentPacket,
};

/// The number of packets we allow to burst from the pacer.
pub const PACING_BURST_SIZE: usize = 2;

#[derive(Debug)]
pub struct PacketSender {
    cc: Box<dyn CongestionControl>,
    pacer: Pacer,
}

impl Display for PacketSender {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} {}", self.cc, self.pacer)
    }
}

impl PacketSender {
    #[must_use]
    pub fn new(
        alg: CongestionControlAlgorithm,
        pacing_enabled: bool,
        mtu: usize,
        now: Instant,
    ) -> Self {
        Self {
            cc: match alg {
                CongestionControlAlgorithm::NewReno => {
                    Box::new(ClassicCongestionControl::new(NewReno::default()))
                }
                CongestionControlAlgorithm::Cubic => {
                    Box::new(ClassicCongestionControl::new(Cubic::default()))
                }
            },
            pacer: Pacer::new(pacing_enabled, now, mtu * PACING_BURST_SIZE, mtu),
        }
    }

    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.cc.set_qlog(qlog);
    }

    #[must_use]
    pub fn cwnd(&self) -> usize {
        self.cc.cwnd()
    }

    #[must_use]
    pub fn cwnd_avail(&self) -> usize {
        self.cc.cwnd_avail()
    }

    pub fn on_packets_acked(
        &mut self,
        acked_pkts: &[SentPacket],
        rtt_est: &RttEstimate,
        now: Instant,
    ) {
        self.cc.on_packets_acked(acked_pkts, rtt_est, now);
    }

    /// Called when packets are lost.  Returns true if the congestion window was reduced.
    pub fn on_packets_lost(
        &mut self,
        first_rtt_sample_time: Option<Instant>,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) -> bool {
        self.cc.on_packets_lost(
            first_rtt_sample_time,
            prev_largest_acked_sent,
            pto,
            lost_packets,
        )
    }

    pub fn discard(&mut self, pkt: &SentPacket) {
        self.cc.discard(pkt);
    }

    /// When we migrate, the congestion controller for the previously active path drops
    /// all bytes in flight.
    pub fn discard_in_flight(&mut self) {
        self.cc.discard_in_flight();
    }

    pub fn on_packet_sent(&mut self, pkt: &SentPacket, rtt: Duration) {
        self.pacer
            .spend(pkt.time_sent, rtt, self.cc.cwnd(), pkt.size);
        self.cc.on_packet_sent(pkt);
    }

    #[must_use]
    pub fn next_paced(&self, rtt: Duration) -> Option<Instant> {
        // Only pace if there are bytes in flight.
        if self.cc.bytes_in_flight() > 0 {
            Some(self.pacer.next(rtt, self.cc.cwnd()))
        } else {
            None
        }
    }

    #[must_use]
    pub fn recovery_packet(&self) -> bool {
        self.cc.recovery_packet()
    }
}
