// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control

use std::cmp::max;
use std::fmt::{self, Display};
use std::time::{Duration, Instant};

use crate::tracking::SentPacket;
use neqo_common::{const_max, const_min, qdebug, qinfo, qtrace};

pub const MAX_DATAGRAM_SIZE: usize = 1232; // For ipv6, smaller than ipv4 (1252)
pub const INITIAL_CWND_PKTS: usize = 10;
const INITIAL_WINDOW: usize = const_min(
    INITIAL_CWND_PKTS * MAX_DATAGRAM_SIZE,
    const_max(2 * MAX_DATAGRAM_SIZE, 14720),
);
pub const MIN_CONG_WINDOW: usize = MAX_DATAGRAM_SIZE * 2;
const PERSISTENT_CONG_THRESH: u32 = 3;

#[derive(Debug)]
pub struct CongestionControl {
    congestion_window: usize, // = kInitialWindow
    bytes_in_flight: usize,
    congestion_recovery_start_time: Option<Instant>,
    ssthresh: usize,
}

impl Default for CongestionControl {
    fn default() -> Self {
        Self {
            congestion_window: INITIAL_WINDOW,
            bytes_in_flight: 0,
            congestion_recovery_start_time: None,
            ssthresh: std::usize::MAX,
        }
    }
}

impl Display for CongestionControl {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "CongCtrl {}/{} ssthresh {}",
            self.bytes_in_flight, self.congestion_window, self.ssthresh
        )
    }
}

impl CongestionControl {
    #[cfg(test)]
    #[must_use]
    pub fn cwnd(&self) -> usize {
        self.congestion_window
    }

    #[cfg(test)]
    #[must_use]
    pub fn ssthresh(&self) -> usize {
        self.ssthresh
    }

    #[must_use]
    pub fn cwnd_avail(&self) -> usize {
        // BIF can be higher than cwnd due to PTO packets, which are sent even
        // if avail is 0, but still count towards BIF.
        self.congestion_window.saturating_sub(self.bytes_in_flight)
    }

    // Multi-packet version of OnPacketAckedCC
    pub fn on_packets_acked(&mut self, acked_pkts: &[SentPacket]) {
        for pkt in acked_pkts
            .iter()
            .filter(|pkt| pkt.in_flight)
            .filter(|pkt| pkt.time_declared_lost.is_none())
        {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;

            if self.in_congestion_recovery(pkt.time_sent) {
                // Do not increase congestion window in recovery period.
                continue;
            }
            if self.app_limited() {
                // Do not increase congestion_window if application limited.
                continue;
            }

            if self.congestion_window < self.ssthresh {
                self.congestion_window += pkt.size;
                qinfo!([self], "slow start");
            } else {
                self.congestion_window += (MAX_DATAGRAM_SIZE * pkt.size) / self.congestion_window;
                qinfo!([self], "congestion avoidance");
            }
        }
    }

    pub fn on_packets_lost(
        &mut self,
        now: Instant,
        largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) {
        if lost_packets.is_empty() {
            return;
        }

        for pkt in lost_packets.iter().filter(|pkt| pkt.in_flight) {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;
        }

        qdebug!([self], "Pkts lost {}", lost_packets.len());

        let last_lost_pkt = lost_packets.last().unwrap();
        self.on_congestion_event(now, last_lost_pkt.time_sent);

        let in_persistent_congestion = {
            let congestion_period = pto * PERSISTENT_CONG_THRESH;

            match largest_acked_sent {
                Some(las) => las < last_lost_pkt.time_sent - congestion_period,
                None => {
                    // Nothing has ever been acked. Could still be PC.
                    let first_lost_pkt_sent = lost_packets.first().unwrap().time_sent;
                    last_lost_pkt.time_sent - first_lost_pkt_sent > congestion_period
                }
            }
        };
        if in_persistent_congestion {
            qinfo!([self], "persistent congestion");
            self.congestion_window = MIN_CONG_WINDOW;
        }
    }

    pub fn discard(&mut self, pkt: &SentPacket) {
        if pkt.in_flight {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;
            qtrace!([self], "Ignore pkt with size {}", pkt.size);
        }
    }

    pub fn on_packet_sent(&mut self, pkt: &SentPacket) {
        if !pkt.in_flight {
            return;
        }

        self.bytes_in_flight += pkt.size;
        qdebug!(
            [self],
            "Pkt Sent len {}, bif {}, cwnd {}",
            pkt.size,
            self.bytes_in_flight,
            self.congestion_window
        );
        debug_assert!(self.bytes_in_flight <= self.congestion_window);
    }

    #[must_use]
    pub fn in_congestion_recovery(&self, sent_time: Instant) -> bool {
        self.congestion_recovery_start_time
            .map(|start| sent_time <= start)
            .unwrap_or(false)
    }

    fn on_congestion_event(&mut self, now: Instant, sent_time: Instant) {
        // Start a new congestion event if packet was sent after the
        // start of the previous congestion recovery period.
        if !self.in_congestion_recovery(sent_time) {
            self.congestion_recovery_start_time = Some(now);
            self.congestion_window /= 2; // kLossReductionFactor = 0.5
            self.congestion_window = max(self.congestion_window, MIN_CONG_WINDOW);
            self.ssthresh = self.congestion_window;
            qinfo!(
                [self],
                "Cong event -> recovery; cwnd {}, ssthresh {}",
                self.congestion_window,
                self.ssthresh
            );
        } else {
            qdebug!([self], "Cong event but already in recovery");
        }
    }

    fn app_limited(&self) -> bool {
        //TODO(agrover): how do we get this info??
        false
    }
}
