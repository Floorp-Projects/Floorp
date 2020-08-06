// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control

use std::cmp::{max, min};
use std::fmt::{self, Display};
use std::time::{Duration, Instant};

use crate::pace::Pacer;
use crate::path::PATH_MTU_V6;
use crate::qlog::{self, CongestionState, QlogMetric};
use crate::tracking::SentPacket;
use neqo_common::{const_max, const_min, qdebug, qinfo, qlog::NeqoQlog, qtrace};

pub const MAX_DATAGRAM_SIZE: usize = PATH_MTU_V6;
pub const INITIAL_CWND_PKTS: usize = 10;
const INITIAL_WINDOW: usize = const_min(
    INITIAL_CWND_PKTS * MAX_DATAGRAM_SIZE,
    const_max(2 * MAX_DATAGRAM_SIZE, 14720),
);
pub const MIN_CONG_WINDOW: usize = MAX_DATAGRAM_SIZE * 2;
/// The number of packets we allow to burst from the pacer.
pub(crate) const PACING_BURST_SIZE: usize = 2;
const PERSISTENT_CONG_THRESH: u32 = 3;

#[derive(Debug)]
pub struct CongestionControl {
    congestion_window: usize, // = kInitialWindow
    bytes_in_flight: usize,
    acked_bytes: usize,
    congestion_recovery_start_time: Option<Instant>,
    ssthresh: usize,
    pacer: Option<Pacer>,
    in_recovery: bool,

    qlog: NeqoQlog,
    qlog_curr_cong_state: CongestionState,
}

impl Default for CongestionControl {
    fn default() -> Self {
        Self {
            congestion_window: INITIAL_WINDOW,
            bytes_in_flight: 0,
            acked_bytes: 0,
            congestion_recovery_start_time: None,
            ssthresh: std::usize::MAX,
            pacer: None,
            in_recovery: false,
            qlog: NeqoQlog::disabled(),
            qlog_curr_cong_state: CongestionState::SlowStart,
        }
    }
}

impl Display for CongestionControl {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "CongCtrl {}/{} ssthresh {}",
            self.bytes_in_flight, self.congestion_window, self.ssthresh,
        )?;
        if let Some(p) = &self.pacer {
            write!(f, " {}", p)?;
        }
        Ok(())
    }
}

impl CongestionControl {
    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.qlog = qlog;
    }

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

    #[cfg(test)]
    #[must_use]
    pub fn bif(&self) -> usize {
        self.bytes_in_flight
    }

    #[must_use]
    pub fn cwnd_avail(&self) -> usize {
        // BIF can be higher than cwnd due to PTO packets, which are sent even
        // if avail is 0, but still count towards BIF.
        self.congestion_window.saturating_sub(self.bytes_in_flight)
    }

    // Multi-packet version of OnPacketAckedCC
    pub fn on_packets_acked(&mut self, acked_pkts: &[SentPacket]) {
        for pkt in acked_pkts.iter().filter(|pkt| pkt.cc_outstanding()) {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;

            if !self.after_recovery_start(pkt.time_sent) {
                // Do not increase congestion window for packets sent before
                // recovery start.
                continue;
            }

            if self.in_recovery {
                self.in_recovery = false;
                qlog::metrics_updated(&mut self.qlog, &[QlogMetric::InRecovery(false)]);
            }

            if self.app_limited() {
                // Do not increase congestion_window if application limited.
                qlog::congestion_state_updated(
                    &mut self.qlog,
                    &mut self.qlog_curr_cong_state,
                    CongestionState::ApplicationLimited,
                );
                continue;
            }

            self.acked_bytes += pkt.size;
        }
        qtrace!([self], "ACK received, acked_bytes = {}", self.acked_bytes);

        // Slow start, up to the slow start threshold.
        if self.congestion_window < self.ssthresh {
            let increase = min(self.ssthresh - self.congestion_window, self.acked_bytes);
            self.congestion_window += increase;
            self.acked_bytes -= increase;
            qinfo!([self], "slow start += {}", increase);
            qlog::congestion_state_updated(
                &mut self.qlog,
                &mut self.qlog_curr_cong_state,
                CongestionState::SlowStart,
            );
        }
        // Congestion avoidance, above the slow start threshold.
        if self.acked_bytes >= self.congestion_window {
            self.acked_bytes -= self.congestion_window;
            self.congestion_window += MAX_DATAGRAM_SIZE;
            qinfo!([self], "congestion avoidance += {}", MAX_DATAGRAM_SIZE);
            qlog::congestion_state_updated(
                &mut self.qlog,
                &mut self.qlog_curr_cong_state,
                CongestionState::CongestionAvoidance,
            );
        }
        qlog::metrics_updated(
            &mut self.qlog,
            &[
                QlogMetric::CongestionWindow(self.congestion_window),
                QlogMetric::BytesInFlight(self.bytes_in_flight),
            ],
        );
    }

    pub fn on_packets_lost(
        &mut self,
        now: Instant,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) {
        if lost_packets.is_empty() {
            return;
        }

        for pkt in lost_packets.iter().filter(|pkt| pkt.ack_eliciting()) {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;
        }
        qlog::metrics_updated(
            &mut self.qlog,
            &[QlogMetric::BytesInFlight(self.bytes_in_flight)],
        );

        qdebug!([self], "Pkts lost {}", lost_packets.len());

        let last_lost_pkt = lost_packets.last().unwrap();
        self.on_congestion_event(now, last_lost_pkt.time_sent);

        let congestion_period = pto * PERSISTENT_CONG_THRESH;

        // Simpler to ignore any acked pkts in between first and last lost pkts
        if let Some(first) = lost_packets
            .iter()
            .find(|p| Some(p.time_sent) > prev_largest_acked_sent)
        {
            if last_lost_pkt.time_sent.duration_since(first.time_sent) > congestion_period {
                self.congestion_window = MIN_CONG_WINDOW;
                self.acked_bytes = 0;
                qlog::metrics_updated(
                    &mut self.qlog,
                    &[QlogMetric::CongestionWindow(self.congestion_window)],
                );
                qinfo!([self], "persistent congestion");
            }
        }
    }

    pub fn discard(&mut self, pkt: &SentPacket) {
        if pkt.cc_outstanding() {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;
            qlog::metrics_updated(
                &mut self.qlog,
                &[QlogMetric::BytesInFlight(self.bytes_in_flight)],
            );
            qtrace!([self], "Ignore pkt with size {}", pkt.size);
        }
    }

    pub fn on_packet_sent(&mut self, pkt: &SentPacket, rtt: Duration) {
        self.pacer
            .as_mut()
            .unwrap()
            .spend(pkt.time_sent, rtt, self.congestion_window, pkt.size);

        if !pkt.ack_eliciting() {
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
        qlog::metrics_updated(
            &mut self.qlog,
            &[QlogMetric::BytesInFlight(self.bytes_in_flight)],
        );
    }

    #[must_use]
    fn after_recovery_start(&mut self, sent_time: Instant) -> bool {
        match self.congestion_recovery_start_time {
            Some(crst) => sent_time > crst,
            None => true,
        }
    }

    fn on_congestion_event(&mut self, now: Instant, sent_time: Instant) {
        // Start a new congestion event if lost packet was sent after the start
        // of the previous congestion recovery period.
        if self.after_recovery_start(sent_time) {
            self.congestion_recovery_start_time = Some(now);
            self.congestion_window /= 2; // kLossReductionFactor = 0.5
            self.acked_bytes /= 2;
            self.congestion_window = max(self.congestion_window, MIN_CONG_WINDOW);
            self.ssthresh = self.congestion_window;
            qinfo!(
                [self],
                "Cong event -> recovery; cwnd {}, ssthresh {}",
                self.congestion_window,
                self.ssthresh
            );
            qlog::metrics_updated(
                &mut self.qlog,
                &[
                    QlogMetric::CongestionWindow(self.congestion_window),
                    QlogMetric::SsThresh(self.ssthresh),
                    QlogMetric::InRecovery(true),
                ],
            );
            self.in_recovery = true;
            qlog::congestion_state_updated(
                &mut self.qlog,
                &mut self.qlog_curr_cong_state,
                CongestionState::Recovery,
            );
        } else {
            qdebug!([self], "Cong event but already in recovery");
        }
    }

    fn app_limited(&self) -> bool {
        //TODO(agrover): how do we get this info??
        false
    }

    pub fn start_pacer(&mut self, now: Instant) {
        // Start the pacer with a small burst size.
        self.pacer = Some(Pacer::new(
            now,
            MAX_DATAGRAM_SIZE * PACING_BURST_SIZE,
            MAX_DATAGRAM_SIZE,
        ));
    }

    pub fn next_paced(&self, rtt: Duration) -> Option<Instant> {
        // Only pace if there are bytes in flight.
        if self.bytes_in_flight > 0 {
            Some(
                self.pacer
                    .as_ref()
                    .unwrap()
                    .next(rtt, self.congestion_window),
            )
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::packet::PacketType;
    use std::rc::Rc;
    use test_fixture::now;

    #[test]
    fn issue_876() {
        const PTO: Duration = Duration::from_millis(100);
        const RTT: Duration = Duration::from_millis(98);
        let mut cc = CongestionControl::default();
        let time_now = now();
        let time_before = time_now - Duration::from_millis(100);
        let time_after1 = time_now + Duration::from_millis(100);
        let time_after2 = time_now + Duration::from_millis(150);
        let time_after3 = time_now + Duration::from_millis(175);

        cc.start_pacer(time_now);

        let sent_packets = vec![
            SentPacket::new(
                PacketType::Short,
                1,             // pn
                time_before,   // time sent
                true,          // ack eliciting
                Rc::default(), // tokens
                103,           // size
            ),
            SentPacket::new(
                PacketType::Short,
                2,             // pn
                time_before,   // time sent
                true,          // ack eliciting
                Rc::default(), // tokens
                105,           // size
            ),
            SentPacket::new(
                PacketType::Short,
                3,             // pn
                time_after2,   // time sent
                true,          // ack eliciting
                Rc::default(), // tokens
                107,           // size
            ),
        ];

        cc.on_packet_sent(&sent_packets[0], RTT);
        assert_eq!(cc.acked_bytes, 0);
        assert_eq!(cc.cwnd(), INITIAL_WINDOW);
        assert_eq!(cc.ssthresh(), std::usize::MAX);
        assert_eq!(cc.bif(), 103);

        cc.on_packet_sent(&sent_packets[1], RTT);
        assert_eq!(cc.acked_bytes, 0);
        assert_eq!(cc.cwnd(), INITIAL_WINDOW);
        assert_eq!(cc.ssthresh(), std::usize::MAX);
        assert_eq!(cc.bif(), 208);

        cc.on_packets_lost(time_after1, None, PTO, &sent_packets[0..1]);

        // We are now in recovery
        assert_eq!(cc.acked_bytes, 0);
        assert_eq!(cc.cwnd(), INITIAL_WINDOW / 2);
        assert_eq!(cc.ssthresh(), INITIAL_WINDOW / 2);
        assert_eq!(cc.bif(), 105);

        // Send a packet after recovery starts
        cc.on_packet_sent(&sent_packets[2], RTT);
        assert_eq!(cc.acked_bytes, 0);
        assert_eq!(cc.cwnd(), INITIAL_WINDOW / 2);
        assert_eq!(cc.ssthresh(), INITIAL_WINDOW / 2);
        assert_eq!(cc.bif(), 212);

        // and ack it. cwnd increases slightly
        cc.on_packets_acked(&sent_packets[2..3]);
        assert_eq!(cc.acked_bytes, sent_packets[2].size);
        assert_eq!(cc.cwnd(), INITIAL_WINDOW / 2);
        assert_eq!(cc.ssthresh(), INITIAL_WINDOW / 2);
        assert_eq!(cc.bif(), 105);

        // Packet from before is lost. Should not hurt cwnd.
        cc.on_packets_lost(time_after3, None, PTO, &sent_packets[1..2]);
        assert_eq!(cc.acked_bytes, sent_packets[2].size);
        assert_eq!(cc.cwnd(), INITIAL_WINDOW / 2);
        assert_eq!(cc.ssthresh(), INITIAL_WINDOW / 2);
        assert_eq!(cc.bif(), 0);
    }
}
