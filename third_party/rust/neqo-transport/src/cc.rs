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

    fn detect_persistent_congestion(
        &mut self,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) {
        let pc_period = pto * PERSISTENT_CONG_THRESH;

        let mut last_pn = 1 << 62; // Impossibly large, but not enough to overflow.
        let mut start = None;
        for p in lost_packets
            .iter()
            .skip_while(|p| Some(p.time_sent) <= prev_largest_acked_sent)
        {
            if p.pn != last_pn + 1 {
                // Not a contiguous range of lost packets, start over.
                start = None;
            }
            last_pn = p.pn;
            if !p.ack_eliciting() {
                // Not interesting, keep looking.
                continue;
            }
            if let Some(t) = start {
                if p.time_sent.duration_since(t) > pc_period {
                    // In persistent congestion.  Stop.
                    self.congestion_window = MIN_CONG_WINDOW;
                    self.acked_bytes = 0;
                    qlog::metrics_updated(
                        &mut self.qlog,
                        &[QlogMetric::CongestionWindow(self.congestion_window)],
                    );
                    qinfo!([self], "persistent congestion");
                    return;
                }
            } else {
                start = Some(p.time_sent);
            }
        }
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

        self.detect_persistent_congestion(prev_largest_acked_sent, pto, lost_packets);
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
    use super::{CongestionControl, INITIAL_WINDOW, MIN_CONG_WINDOW, PERSISTENT_CONG_THRESH};
    use crate::packet::{PacketNumber, PacketType};
    use crate::tracking::SentPacket;
    use std::rc::Rc;
    use std::time::Duration;
    use test_fixture::now;

    const PTO: Duration = Duration::from_millis(100);
    const RTT: Duration = Duration::from_millis(98);
    const ZERO: Duration = Duration::from_secs(0);
    const EPSILON: Duration = Duration::from_nanos(1);
    const GAP: Duration = Duration::from_secs(1);
    /// The largest time between packets without causing persistent congestion.
    const SUB_PC: Duration = Duration::from_millis(100 * PERSISTENT_CONG_THRESH as u64);
    /// The minimum time between packets to cause persistent congestion.
    /// Uses an odd expression because `Duration` arithmetic isn't `const`.
    const PC: Duration = Duration::from_nanos(100_000_000 * (PERSISTENT_CONG_THRESH as u64) + 1);

    #[test]
    fn issue_876() {
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

    fn lost(pn: PacketNumber, ack_eliciting: bool, t: Duration) -> SentPacket {
        SentPacket::new(
            PacketType::Short,
            pn,
            now() + t,
            ack_eliciting,
            Rc::default(),
            100,
        )
    }

    fn persistent_congestion(lost_packets: &[SentPacket]) -> bool {
        let mut cc = CongestionControl::default();
        cc.start_pacer(now());
        for p in lost_packets {
            cc.on_packet_sent(p, RTT);
        }

        cc.on_packets_lost(now(), None, PTO, lost_packets);
        if cc.cwnd() == INITIAL_WINDOW / 2 {
            false
        } else if cc.cwnd() == MIN_CONG_WINDOW {
            true
        } else {
            panic!("unexpected cwnd");
        }
    }

    /// A span of exactly the PC threshold only reduces the window on loss.
    #[test]
    fn persistent_congestion_none() {
        assert!(!persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, SUB_PC),
        ]));
    }

    /// A span of just more than the PC threshold causes persistent congestion.
    #[test]
    fn persistent_congestion_simple() {
        assert!(persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, PC),
        ]));
    }

    /// Both packets need to be ack-eliciting.
    #[test]
    fn persistent_congestion_non_ack_eliciting() {
        assert!(!persistent_congestion(&[
            lost(1, false, ZERO),
            lost(2, true, PC),
        ]));
        assert!(!persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, false, PC),
        ]));
    }

    /// Packets in the middle, of any type, are OK.
    #[test]
    fn persistent_congestion_middle() {
        assert!(persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, false, RTT),
            lost(3, true, PC),
        ]));
        assert!(persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, RTT),
            lost(3, true, PC),
        ]));
    }

    /// Leading non-ack-eliciting packets are skipped.
    #[test]
    fn persistent_congestion_leading_non_ack_eliciting() {
        assert!(!persistent_congestion(&[
            lost(1, false, ZERO),
            lost(2, true, RTT),
            lost(3, true, PC),
        ]));
        assert!(persistent_congestion(&[
            lost(1, false, ZERO),
            lost(2, true, RTT),
            lost(3, true, RTT + PC),
        ]));
    }

    /// Trailing non-ack-eliciting packets aren't relevant.
    #[test]
    fn persistent_congestion_trailing_non_ack_eliciting() {
        assert!(persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, PC),
            lost(3, false, PC + EPSILON),
        ]));
        assert!(!persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, SUB_PC),
            lost(3, false, PC),
        ]));
    }

    /// Gaps in the middle, of any type, restart the count.
    #[test]
    fn persistent_congestion_gap_reset() {
        assert!(!persistent_congestion(&[
            lost(1, true, ZERO),
            lost(3, true, PC),
        ]));
        assert!(!persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, RTT),
            lost(4, true, GAP),
            lost(5, true, GAP + PTO * PERSISTENT_CONG_THRESH),
        ]));
    }

    /// A span either side of a gap will cause persistent congestion.
    #[test]
    fn persistent_congestion_gap_or() {
        assert!(persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, PC),
            lost(4, true, GAP),
            lost(5, true, GAP + PTO),
        ]));
        assert!(persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, PTO),
            lost(4, true, GAP),
            lost(5, true, GAP + PC),
        ]));
    }

    /// A gap only restarts after an ack-eliciting packet.
    #[test]
    fn persistent_congestion_gap_non_ack_eliciting() {
        assert!(!persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, PTO),
            lost(4, false, GAP),
            lost(5, true, GAP + PC),
        ]));
        assert!(persistent_congestion(&[
            lost(1, true, ZERO),
            lost(2, true, PTO),
            lost(4, false, GAP),
            lost(5, true, GAP + RTT),
            lost(6, true, GAP + RTT + PC),
        ]));
    }
}
