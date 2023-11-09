// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Congestion control
#![deny(clippy::pedantic)]

use std::cmp::{max, min};
use std::fmt::{self, Debug, Display};
use std::time::{Duration, Instant};

use super::CongestionControl;

use crate::cc::MAX_DATAGRAM_SIZE;
use crate::qlog::{self, QlogMetric};
use crate::sender::PACING_BURST_SIZE;
use crate::tracking::SentPacket;
use ::qlog::events::quic::CongestionStateUpdated;
use ::qlog::events::EventData;
use neqo_common::{const_max, const_min, qdebug, qinfo, qlog::NeqoQlog, qtrace};

pub const CWND_INITIAL_PKTS: usize = 10;
pub const CWND_INITIAL: usize = const_min(
    CWND_INITIAL_PKTS * MAX_DATAGRAM_SIZE,
    const_max(2 * MAX_DATAGRAM_SIZE, 14720),
);
pub const CWND_MIN: usize = MAX_DATAGRAM_SIZE * 2;
const PERSISTENT_CONG_THRESH: u32 = 3;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum State {
    /// In either slow start or congestion avoidance, not recovery.
    SlowStart,
    /// In congestion avoidance.
    CongestionAvoidance,
    /// In a recovery period, but no packets have been sent yet.  This is a
    /// transient state because we want to exempt the first packet sent after
    /// entering recovery from the congestion window.
    RecoveryStart,
    /// In a recovery period, with the first packet sent at this time.
    Recovery,
    /// Start of persistent congestion, which is transient, like `RecoveryStart`.
    PersistentCongestion,
}

impl State {
    pub fn in_recovery(self) -> bool {
        matches!(self, Self::RecoveryStart | Self::Recovery)
    }

    pub fn in_slow_start(self) -> bool {
        self == Self::SlowStart
    }

    /// These states are transient, we tell qlog on entry, but not on exit.
    pub fn transient(self) -> bool {
        matches!(self, Self::RecoveryStart | Self::PersistentCongestion)
    }

    /// Update a transient state to the true state.
    pub fn update(&mut self) {
        *self = match self {
            Self::PersistentCongestion => Self::SlowStart,
            Self::RecoveryStart => Self::Recovery,
            _ => unreachable!(),
        };
    }

    pub fn to_qlog(self) -> &'static str {
        match self {
            Self::SlowStart | Self::PersistentCongestion => "slow_start",
            Self::CongestionAvoidance => "congestion_avoidance",
            Self::Recovery | Self::RecoveryStart => "recovery",
        }
    }
}

pub trait WindowAdjustment: Display + Debug {
    /// This is called when an ack is received.
    /// The function calculates the amount of acked bytes congestion controller needs
    /// to collect before increasing its cwnd by `MAX_DATAGRAM_SIZE`.
    fn bytes_for_cwnd_increase(
        &mut self,
        curr_cwnd: usize,
        new_acked_bytes: usize,
        min_rtt: Duration,
        now: Instant,
    ) -> usize;
    /// This function is called when a congestion event has beed detected and it
    /// returns new (decreased) values of `curr_cwnd` and `acked_bytes`.
    /// This value can be very small; the calling code is responsible for ensuring that the
    /// congestion window doesn't drop below the minimum of `CWND_MIN`.
    fn reduce_cwnd(&mut self, curr_cwnd: usize, acked_bytes: usize) -> (usize, usize);
    /// Cubic needs this signal to reset its epoch.
    fn on_app_limited(&mut self);
    #[cfg(test)]
    fn last_max_cwnd(&self) -> f64;
    #[cfg(test)]
    fn set_last_max_cwnd(&mut self, last_max_cwnd: f64);
}

#[derive(Debug)]
pub struct ClassicCongestionControl<T> {
    cc_algorithm: T,
    state: State,
    congestion_window: usize, // = kInitialWindow
    bytes_in_flight: usize,
    acked_bytes: usize,
    ssthresh: usize,
    recovery_start: Option<Instant>,

    qlog: NeqoQlog,
}

impl<T: WindowAdjustment> Display for ClassicCongestionControl<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{} CongCtrl {}/{} ssthresh {}",
            self.cc_algorithm, self.bytes_in_flight, self.congestion_window, self.ssthresh,
        )?;
        Ok(())
    }
}

impl<T: WindowAdjustment> CongestionControl for ClassicCongestionControl<T> {
    fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.qlog = qlog;
    }

    #[must_use]
    fn cwnd(&self) -> usize {
        self.congestion_window
    }

    #[must_use]
    fn bytes_in_flight(&self) -> usize {
        self.bytes_in_flight
    }

    #[must_use]
    fn cwnd_avail(&self) -> usize {
        // BIF can be higher than cwnd due to PTO packets, which are sent even
        // if avail is 0, but still count towards BIF.
        self.congestion_window.saturating_sub(self.bytes_in_flight)
    }

    // Multi-packet version of OnPacketAckedCC
    fn on_packets_acked(&mut self, acked_pkts: &[SentPacket], min_rtt: Duration, now: Instant) {
        // Check whether we are app limited before acked packets are removed
        // from bytes_in_flight.
        let is_app_limited = self.app_limited();
        qtrace!(
            [self],
            "limited={}, bytes_in_flight={}, cwnd={}, state={:?} pacing_burst_size={}",
            is_app_limited,
            self.bytes_in_flight,
            self.congestion_window,
            self.state,
            MAX_DATAGRAM_SIZE * PACING_BURST_SIZE,
        );

        let mut new_acked = 0;
        for pkt in acked_pkts.iter().filter(|pkt| pkt.cc_outstanding()) {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;

            if !self.after_recovery_start(pkt) {
                // Do not increase congestion window for packets sent before
                // recovery last started.
                continue;
            }

            if self.state.in_recovery() {
                self.set_state(State::CongestionAvoidance);
                qlog::metrics_updated(&mut self.qlog, &[QlogMetric::InRecovery(false)]);
            }

            new_acked += pkt.size;
        }

        if is_app_limited {
            self.cc_algorithm.on_app_limited();
            return;
        }

        qtrace!([self], "ACK received, acked_bytes = {}", self.acked_bytes);

        // Slow start, up to the slow start threshold.
        if self.congestion_window < self.ssthresh {
            self.acked_bytes += new_acked;
            let increase = min(self.ssthresh - self.congestion_window, self.acked_bytes);
            self.congestion_window += increase;
            self.acked_bytes -= increase;
            qinfo!([self], "slow start += {}", increase);
            if self.congestion_window == self.ssthresh {
                // This doesn't look like it is necessary, but it can happen
                // after persistent congestion.
                self.set_state(State::CongestionAvoidance);
            }
        }
        // Congestion avoidance, above the slow start threshold.
        if self.congestion_window >= self.ssthresh {
            // The following function return the amount acked bytes a controller needs
            // to collect to be allowed to increase its cwnd by MAX_DATAGRAM_SIZE.
            let bytes_for_increase = self.cc_algorithm.bytes_for_cwnd_increase(
                self.congestion_window,
                new_acked,
                min_rtt,
                now,
            );
            debug_assert!(bytes_for_increase > 0);
            // If enough credit has been accumulated already, apply them gradually.
            // If we have sudden increase in allowed rate we actually increase cwnd gently.
            if self.acked_bytes >= bytes_for_increase {
                self.acked_bytes = 0;
                self.congestion_window += MAX_DATAGRAM_SIZE;
            }
            self.acked_bytes += new_acked;
            if self.acked_bytes >= bytes_for_increase {
                self.acked_bytes -= bytes_for_increase;
                self.congestion_window += MAX_DATAGRAM_SIZE; // or is this the current MTU?
            }
            // The number of bytes we require can go down over time with Cubic.
            // That might result in an excessive rate of increase, so limit the number of unused
            // acknowledged bytes after increasing the congestion window twice.
            self.acked_bytes = min(bytes_for_increase, self.acked_bytes);
        }
        qlog::metrics_updated(
            &mut self.qlog,
            &[
                QlogMetric::CongestionWindow(self.congestion_window),
                QlogMetric::BytesInFlight(self.bytes_in_flight),
            ],
        );
    }

    /// Update congestion controller state based on lost packets.
    fn on_packets_lost(
        &mut self,
        first_rtt_sample_time: Option<Instant>,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) -> bool {
        if lost_packets.is_empty() {
            return false;
        }

        for pkt in lost_packets.iter().filter(|pkt| pkt.cc_in_flight()) {
            assert!(self.bytes_in_flight >= pkt.size);
            self.bytes_in_flight -= pkt.size;
        }
        qlog::metrics_updated(
            &mut self.qlog,
            &[QlogMetric::BytesInFlight(self.bytes_in_flight)],
        );

        qdebug!([self], "Pkts lost {}", lost_packets.len());

        let congestion = self.on_congestion_event(lost_packets.last().unwrap());
        let persistent_congestion = self.detect_persistent_congestion(
            first_rtt_sample_time,
            prev_largest_acked_sent,
            pto,
            lost_packets,
        );
        congestion || persistent_congestion
    }

    fn discard(&mut self, pkt: &SentPacket) {
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

    fn discard_in_flight(&mut self) {
        self.bytes_in_flight = 0;
        qlog::metrics_updated(
            &mut self.qlog,
            &[QlogMetric::BytesInFlight(self.bytes_in_flight)],
        );
    }

    fn on_packet_sent(&mut self, pkt: &SentPacket) {
        // Record the recovery time and exit any transient state.
        if self.state.transient() {
            self.recovery_start = Some(pkt.time_sent);
            self.state.update();
        }

        if !pkt.cc_in_flight() {
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

    /// Whether a packet can be sent immediately as a result of entering recovery.
    fn recovery_packet(&self) -> bool {
        self.state == State::RecoveryStart
    }
}

impl<T: WindowAdjustment> ClassicCongestionControl<T> {
    pub fn new(cc_algorithm: T) -> Self {
        Self {
            cc_algorithm,
            state: State::SlowStart,
            congestion_window: CWND_INITIAL,
            bytes_in_flight: 0,
            acked_bytes: 0,
            ssthresh: usize::MAX,
            recovery_start: None,
            qlog: NeqoQlog::disabled(),
        }
    }

    #[cfg(test)]
    #[must_use]
    pub fn ssthresh(&self) -> usize {
        self.ssthresh
    }

    #[cfg(test)]
    pub fn set_ssthresh(&mut self, v: usize) {
        self.ssthresh = v;
    }

    #[cfg(test)]
    pub fn last_max_cwnd(&self) -> f64 {
        self.cc_algorithm.last_max_cwnd()
    }

    #[cfg(test)]
    pub fn set_last_max_cwnd(&mut self, last_max_cwnd: f64) {
        self.cc_algorithm.set_last_max_cwnd(last_max_cwnd);
    }

    #[cfg(test)]
    pub fn acked_bytes(&self) -> usize {
        self.acked_bytes
    }

    fn set_state(&mut self, state: State) {
        if self.state != state {
            qdebug!([self], "state -> {:?}", state);
            let old_state = self.state;
            self.qlog.add_event_data(|| {
                // No need to tell qlog about exit from transient states.
                if old_state.transient() {
                    None
                } else {
                    let ev_data = EventData::CongestionStateUpdated(CongestionStateUpdated {
                        old: Some(old_state.to_qlog().to_owned()),
                        new: state.to_qlog().to_owned(),
                        trigger: None,
                    });
                    Some(ev_data)
                }
            });
            self.state = state;
        }
    }

    fn detect_persistent_congestion(
        &mut self,
        first_rtt_sample_time: Option<Instant>,
        prev_largest_acked_sent: Option<Instant>,
        pto: Duration,
        lost_packets: &[SentPacket],
    ) -> bool {
        if first_rtt_sample_time.is_none() {
            return false;
        }

        let pc_period = pto * PERSISTENT_CONG_THRESH;

        let mut last_pn = 1 << 62; // Impossibly large, but not enough to overflow.
        let mut start = None;

        // Look for the first lost packet after the previous largest acknowledged.
        // Ignore packets that weren't ack-eliciting for the start of this range.
        // Also, make sure to ignore any packets sent before we got an RTT estimate
        // as we might not have sent PTO packets soon enough after those.
        let cutoff = max(first_rtt_sample_time, prev_largest_acked_sent);
        for p in lost_packets
            .iter()
            .skip_while(|p| Some(p.time_sent) < cutoff)
        {
            if p.pn != last_pn + 1 {
                // Not a contiguous range of lost packets, start over.
                start = None;
            }
            last_pn = p.pn;
            if !p.cc_in_flight() {
                // Not interesting, keep looking.
                continue;
            }
            if let Some(t) = start {
                if p.time_sent.checked_duration_since(t).unwrap() > pc_period {
                    qinfo!([self], "persistent congestion");
                    self.congestion_window = CWND_MIN;
                    self.acked_bytes = 0;
                    self.set_state(State::PersistentCongestion);
                    qlog::metrics_updated(
                        &mut self.qlog,
                        &[QlogMetric::CongestionWindow(self.congestion_window)],
                    );
                    return true;
                }
            } else {
                start = Some(p.time_sent);
            }
        }
        false
    }

    #[must_use]
    fn after_recovery_start(&mut self, packet: &SentPacket) -> bool {
        // At the start of the recovery period, the state is transient and
        // all packets will have been sent before recovery. When sending out
        // the first packet we transition to the non-transient `Recovery`
        // state and update the variable `self.recovery_start`. Before the
        // first recovery, all packets were sent after the recovery event,
        // allowing to reduce the cwnd on congestion events.
        !self.state.transient() && self.recovery_start.map_or(true, |t| packet.time_sent >= t)
    }

    /// Handle a congestion event.
    /// Returns true if this was a true congestion event.
    fn on_congestion_event(&mut self, last_packet: &SentPacket) -> bool {
        // Start a new congestion event if lost packet was sent after the start
        // of the previous congestion recovery period.
        if !self.after_recovery_start(last_packet) {
            return false;
        }

        let (cwnd, acked_bytes) = self
            .cc_algorithm
            .reduce_cwnd(self.congestion_window, self.acked_bytes);
        self.congestion_window = max(cwnd, CWND_MIN);
        self.acked_bytes = acked_bytes;
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
        self.set_state(State::RecoveryStart);
        true
    }

    #[allow(clippy::unused_self)]
    fn app_limited(&self) -> bool {
        if self.bytes_in_flight >= self.congestion_window {
            false
        } else if self.state.in_slow_start() {
            // Allow for potential doubling of the congestion window during slow start.
            // That is, the application might not have been able to send enough to respond
            // to increases to the congestion window.
            self.bytes_in_flight < self.congestion_window / 2
        } else {
            // We're not limited if the in-flight data is within a single burst of the
            // congestion window.
            (self.bytes_in_flight + MAX_DATAGRAM_SIZE * PACING_BURST_SIZE) < self.congestion_window
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{
        ClassicCongestionControl, WindowAdjustment, CWND_INITIAL, CWND_MIN, PERSISTENT_CONG_THRESH,
    };
    use crate::cc::cubic::{Cubic, CUBIC_BETA_USIZE_DIVIDEND, CUBIC_BETA_USIZE_DIVISOR};
    use crate::cc::new_reno::NewReno;
    use crate::cc::{
        CongestionControl, CongestionControlAlgorithm, CWND_INITIAL_PKTS, MAX_DATAGRAM_SIZE,
    };
    use crate::packet::{PacketNumber, PacketType};
    use crate::tracking::SentPacket;
    use std::convert::TryFrom;
    use std::time::{Duration, Instant};
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

    fn cwnd_is_default(cc: &ClassicCongestionControl<NewReno>) {
        assert_eq!(cc.cwnd(), CWND_INITIAL);
        assert_eq!(cc.ssthresh(), usize::MAX);
    }

    fn cwnd_is_halved(cc: &ClassicCongestionControl<NewReno>) {
        assert_eq!(cc.cwnd(), CWND_INITIAL / 2);
        assert_eq!(cc.ssthresh(), CWND_INITIAL / 2);
    }

    fn lost(pn: PacketNumber, ack_eliciting: bool, t: Duration) -> SentPacket {
        SentPacket::new(
            PacketType::Short,
            pn,
            now() + t,
            ack_eliciting,
            Vec::new(),
            100,
        )
    }

    fn congestion_control(cc: CongestionControlAlgorithm) -> Box<dyn CongestionControl> {
        match cc {
            CongestionControlAlgorithm::NewReno => {
                Box::new(ClassicCongestionControl::new(NewReno::default()))
            }
            CongestionControlAlgorithm::Cubic => {
                Box::new(ClassicCongestionControl::new(Cubic::default()))
            }
        }
    }

    fn persistent_congestion_by_algorithm(
        cc_alg: CongestionControlAlgorithm,
        reduced_cwnd: usize,
        lost_packets: &[SentPacket],
        persistent_expected: bool,
    ) {
        let mut cc = congestion_control(cc_alg);
        for p in lost_packets {
            cc.on_packet_sent(p);
        }

        cc.on_packets_lost(Some(now()), None, PTO, lost_packets);

        let persistent = if cc.cwnd() == reduced_cwnd {
            false
        } else if cc.cwnd() == CWND_MIN {
            true
        } else {
            panic!("unexpected cwnd");
        };
        assert_eq!(persistent, persistent_expected);
    }

    fn persistent_congestion(lost_packets: &[SentPacket], persistent_expected: bool) {
        persistent_congestion_by_algorithm(
            CongestionControlAlgorithm::NewReno,
            CWND_INITIAL / 2,
            lost_packets,
            persistent_expected,
        );
        persistent_congestion_by_algorithm(
            CongestionControlAlgorithm::Cubic,
            CWND_INITIAL * CUBIC_BETA_USIZE_DIVIDEND / CUBIC_BETA_USIZE_DIVISOR,
            lost_packets,
            persistent_expected,
        );
    }

    /// A span of exactly the PC threshold only reduces the window on loss.
    #[test]
    fn persistent_congestion_none() {
        persistent_congestion(&[lost(1, true, ZERO), lost(2, true, SUB_PC)], false);
    }

    /// A span of just more than the PC threshold causes persistent congestion.
    #[test]
    fn persistent_congestion_simple() {
        persistent_congestion(&[lost(1, true, ZERO), lost(2, true, PC)], true);
    }

    /// Both packets need to be ack-eliciting.
    #[test]
    fn persistent_congestion_non_ack_eliciting() {
        persistent_congestion(&[lost(1, false, ZERO), lost(2, true, PC)], false);
        persistent_congestion(&[lost(1, true, ZERO), lost(2, false, PC)], false);
    }

    /// Packets in the middle, of any type, are OK.
    #[test]
    fn persistent_congestion_middle() {
        persistent_congestion(
            &[lost(1, true, ZERO), lost(2, false, RTT), lost(3, true, PC)],
            true,
        );
        persistent_congestion(
            &[lost(1, true, ZERO), lost(2, true, RTT), lost(3, true, PC)],
            true,
        );
    }

    /// Leading non-ack-eliciting packets are skipped.
    #[test]
    fn persistent_congestion_leading_non_ack_eliciting() {
        persistent_congestion(
            &[lost(1, false, ZERO), lost(2, true, RTT), lost(3, true, PC)],
            false,
        );
        persistent_congestion(
            &[
                lost(1, false, ZERO),
                lost(2, true, RTT),
                lost(3, true, RTT + PC),
            ],
            true,
        );
    }

    /// Trailing non-ack-eliciting packets aren't relevant.
    #[test]
    fn persistent_congestion_trailing_non_ack_eliciting() {
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, PC),
                lost(3, false, PC + EPSILON),
            ],
            true,
        );
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, SUB_PC),
                lost(3, false, PC),
            ],
            false,
        );
    }

    /// Gaps in the middle, of any type, restart the count.
    #[test]
    fn persistent_congestion_gap_reset() {
        persistent_congestion(&[lost(1, true, ZERO), lost(3, true, PC)], false);
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, RTT),
                lost(4, true, GAP),
                lost(5, true, GAP + PTO * PERSISTENT_CONG_THRESH),
            ],
            false,
        );
    }

    /// A span either side of a gap will cause persistent congestion.
    #[test]
    fn persistent_congestion_gap_or() {
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, PC),
                lost(4, true, GAP),
                lost(5, true, GAP + PTO),
            ],
            true,
        );
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, PTO),
                lost(4, true, GAP),
                lost(5, true, GAP + PC),
            ],
            true,
        );
    }

    /// A gap only restarts after an ack-eliciting packet.
    #[test]
    fn persistent_congestion_gap_non_ack_eliciting() {
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, PTO),
                lost(4, false, GAP),
                lost(5, true, GAP + PC),
            ],
            false,
        );
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, PTO),
                lost(4, false, GAP),
                lost(5, true, GAP + RTT),
                lost(6, true, GAP + RTT + SUB_PC),
            ],
            false,
        );
        persistent_congestion(
            &[
                lost(1, true, ZERO),
                lost(2, true, PTO),
                lost(4, false, GAP),
                lost(5, true, GAP + RTT),
                lost(6, true, GAP + RTT + PC),
            ],
            true,
        );
    }

    /// Get a time, in multiples of `PTO`, relative to `now()`.
    fn by_pto(t: u32) -> Instant {
        now() + (PTO * t)
    }

    /// Make packets that will be made lost.
    /// `times` is the time of sending, in multiples of `PTO`, relative to `now()`.
    fn make_lost(times: &[u32]) -> Vec<SentPacket> {
        times
            .iter()
            .enumerate()
            .map(|(i, &t)| {
                SentPacket::new(
                    PacketType::Short,
                    u64::try_from(i).unwrap(),
                    by_pto(t),
                    true,
                    Vec::new(),
                    1000,
                )
            })
            .collect::<Vec<_>>()
    }

    /// Call `detect_persistent_congestion` using times relative to now and the fixed PTO time.
    /// `last_ack` and `rtt_time` are times in multiples of `PTO`, relative to `now()`,
    /// for the time of the largest acknowledged and the first RTT sample, respectively.
    fn persistent_congestion_by_pto<T: WindowAdjustment>(
        mut cc: ClassicCongestionControl<T>,
        last_ack: u32,
        rtt_time: u32,
        lost: &[SentPacket],
    ) -> bool {
        assert_eq!(cc.cwnd(), CWND_INITIAL);

        let last_ack = Some(by_pto(last_ack));
        let rtt_time = Some(by_pto(rtt_time));

        // Persistent congestion is never declared if the RTT time is `None`.
        cc.detect_persistent_congestion(None, None, PTO, lost);
        assert_eq!(cc.cwnd(), CWND_INITIAL);
        cc.detect_persistent_congestion(None, last_ack, PTO, lost);
        assert_eq!(cc.cwnd(), CWND_INITIAL);

        cc.detect_persistent_congestion(rtt_time, last_ack, PTO, lost);
        cc.cwnd() == CWND_MIN
    }

    /// No persistent congestion can be had if there are no lost packets.
    #[test]
    fn persistent_congestion_no_lost() {
        let lost = make_lost(&[]);
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            0,
            0,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            0,
            0,
            &lost
        ));
    }

    /// No persistent congestion can be had if there is only one lost packet.
    #[test]
    fn persistent_congestion_one_lost() {
        let lost = make_lost(&[1]);
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            0,
            0,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            0,
            0,
            &lost
        ));
    }

    /// Persistent congestion can't happen based on old packets.
    #[test]
    fn persistent_congestion_past() {
        // Packets sent prior to either the last acknowledged or the first RTT
        // sample are not considered.  So 0 is ignored.
        let lost = make_lost(&[0, PERSISTENT_CONG_THRESH + 1, PERSISTENT_CONG_THRESH + 2]);
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            1,
            1,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            0,
            1,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            1,
            0,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            1,
            1,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            0,
            1,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            1,
            0,
            &lost
        ));
    }

    /// Persistent congestion doesn't start unless the packet is ack-eliciting.
    #[test]
    fn persistent_congestion_ack_eliciting() {
        let mut lost = make_lost(&[1, PERSISTENT_CONG_THRESH + 2]);
        lost[0] = SentPacket::new(
            lost[0].pt,
            lost[0].pn,
            lost[0].time_sent,
            false,
            Vec::new(),
            lost[0].size,
        );
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            0,
            0,
            &lost
        ));
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            0,
            0,
            &lost
        ));
    }

    /// Detect persistent congestion.  Note that the first lost packet needs to have a time
    /// greater than the previously acknowledged packet AND the first RTT sample.  And the
    /// difference in times needs to be greater than the persistent congestion threshold.
    #[test]
    fn persistent_congestion_min() {
        let lost = make_lost(&[1, PERSISTENT_CONG_THRESH + 2]);
        assert!(persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            0,
            0,
            &lost
        ));
        assert!(persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            0,
            0,
            &lost
        ));
    }

    /// Make sure that not having a previous largest acknowledged also results
    /// in detecting persistent congestion.  (This is not expected to happen, but
    /// the code permits it).
    #[test]
    fn persistent_congestion_no_prev_ack_newreno() {
        let lost = make_lost(&[1, PERSISTENT_CONG_THRESH + 2]);
        let mut cc = ClassicCongestionControl::new(NewReno::default());
        cc.detect_persistent_congestion(Some(by_pto(0)), None, PTO, &lost);
        assert_eq!(cc.cwnd(), CWND_MIN);
    }

    #[test]
    fn persistent_congestion_no_prev_ack_cubic() {
        let lost = make_lost(&[1, PERSISTENT_CONG_THRESH + 2]);
        let mut cc = ClassicCongestionControl::new(Cubic::default());
        cc.detect_persistent_congestion(Some(by_pto(0)), None, PTO, &lost);
        assert_eq!(cc.cwnd(), CWND_MIN);
    }

    /// The code asserts on ordering errors.
    #[test]
    #[should_panic]
    fn persistent_congestion_unsorted_newreno() {
        let lost = make_lost(&[PERSISTENT_CONG_THRESH + 2, 1]);
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(NewReno::default()),
            0,
            0,
            &lost
        ));
    }

    /// The code asserts on ordering errors.
    #[test]
    #[should_panic]
    fn persistent_congestion_unsorted_cubic() {
        let lost = make_lost(&[PERSISTENT_CONG_THRESH + 2, 1]);
        assert!(!persistent_congestion_by_pto(
            ClassicCongestionControl::new(Cubic::default()),
            0,
            0,
            &lost
        ));
    }

    #[test]
    fn app_limited_slow_start() {
        const LESS_THAN_CWND_PKTS: usize = 4;
        let mut cc = ClassicCongestionControl::new(NewReno::default());

        for i in 0..CWND_INITIAL_PKTS {
            let sent = SentPacket::new(
                PacketType::Short,
                u64::try_from(i).unwrap(), // pn
                now(),                     // time sent
                true,                      // ack eliciting
                Vec::new(),                // tokens
                MAX_DATAGRAM_SIZE,         // size
            );
            cc.on_packet_sent(&sent);
        }
        assert_eq!(cc.bytes_in_flight(), CWND_INITIAL);

        for i in 0..LESS_THAN_CWND_PKTS {
            let acked = SentPacket::new(
                PacketType::Short,
                u64::try_from(i).unwrap(), // pn
                now(),                     // time sent
                true,                      // ack eliciting
                Vec::new(),                // tokens
                MAX_DATAGRAM_SIZE,         // size
            );
            cc.on_packets_acked(&[acked], RTT, now());

            assert_eq!(
                cc.bytes_in_flight(),
                (CWND_INITIAL_PKTS - i - 1) * MAX_DATAGRAM_SIZE
            );
            assert_eq!(cc.cwnd(), (CWND_INITIAL_PKTS + i + 1) * MAX_DATAGRAM_SIZE);
        }

        // Now we are app limited
        for i in 4..CWND_INITIAL_PKTS {
            let p = [SentPacket::new(
                PacketType::Short,
                u64::try_from(i).unwrap(), // pn
                now(),                     // time sent
                true,                      // ack eliciting
                Vec::new(),                // tokens
                MAX_DATAGRAM_SIZE,         // size
            )];
            cc.on_packets_acked(&p, RTT, now());

            assert_eq!(
                cc.bytes_in_flight(),
                (CWND_INITIAL_PKTS - i - 1) * MAX_DATAGRAM_SIZE
            );
            assert_eq!(cc.cwnd(), (CWND_INITIAL_PKTS + 4) * MAX_DATAGRAM_SIZE);
        }
    }

    #[test]
    fn app_limited_congestion_avoidance() {
        const CWND_PKTS_CA: usize = CWND_INITIAL_PKTS / 2;

        let mut cc = ClassicCongestionControl::new(NewReno::default());

        // Change state to congestion avoidance by introducing loss.

        let p_lost = SentPacket::new(
            PacketType::Short,
            1,                 // pn
            now(),             // time sent
            true,              // ack eliciting
            Vec::new(),        // tokens
            MAX_DATAGRAM_SIZE, // size
        );
        cc.on_packet_sent(&p_lost);
        cwnd_is_default(&cc);
        cc.on_packets_lost(Some(now()), None, PTO, &[p_lost]);
        cwnd_is_halved(&cc);
        let p_not_lost = SentPacket::new(
            PacketType::Short,
            1,                 // pn
            now(),             // time sent
            true,              // ack eliciting
            Vec::new(),        // tokens
            MAX_DATAGRAM_SIZE, // size
        );
        cc.on_packet_sent(&p_not_lost);
        cc.on_packets_acked(&[p_not_lost], RTT, now());
        cwnd_is_halved(&cc);
        // cc is app limited therefore cwnd in not increased.
        assert_eq!(cc.acked_bytes, 0);

        // Now we are in the congestion avoidance state.
        let mut pkts = Vec::new();
        for i in 0..CWND_PKTS_CA {
            let p = SentPacket::new(
                PacketType::Short,
                u64::try_from(i + 3).unwrap(), // pn
                now(),                         // time sent
                true,                          // ack eliciting
                Vec::new(),                    // tokens
                MAX_DATAGRAM_SIZE,             // size
            );
            cc.on_packet_sent(&p);
            pkts.push(p);
        }
        assert_eq!(cc.bytes_in_flight(), CWND_INITIAL / 2);

        for i in 0..CWND_PKTS_CA - 2 {
            cc.on_packets_acked(&pkts[i..=i], RTT, now());

            assert_eq!(
                cc.bytes_in_flight(),
                (CWND_PKTS_CA - i - 1) * MAX_DATAGRAM_SIZE
            );
            assert_eq!(cc.cwnd(), CWND_PKTS_CA * MAX_DATAGRAM_SIZE);
            assert_eq!(cc.acked_bytes, MAX_DATAGRAM_SIZE * (i + 1));
        }

        // Now we are app limited
        for i in CWND_PKTS_CA - 2..CWND_PKTS_CA {
            cc.on_packets_acked(&pkts[i..=i], RTT, now());

            assert_eq!(
                cc.bytes_in_flight(),
                (CWND_PKTS_CA - i - 1) * MAX_DATAGRAM_SIZE
            );
            assert_eq!(cc.cwnd(), CWND_PKTS_CA * MAX_DATAGRAM_SIZE);
            assert_eq!(cc.acked_bytes, MAX_DATAGRAM_SIZE * 3);
        }
    }
}
