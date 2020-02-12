// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of sent packets and detecting their loss.

use std::cmp::{max, min};
use std::collections::BTreeMap;
use std::fmt::{self, Display};
use std::ops::{Index, IndexMut};
use std::time::{Duration, Instant};

use smallvec::SmallVec;

use neqo_common::{const_max, const_min, qdebug, qinfo};

use crate::crypto::CryptoRecoveryToken;
use crate::flow_mgr::FlowControlRecoveryToken;
use crate::send_stream::StreamRecoveryToken;
use crate::tracking::{AckToken, PNSpace};

const GRANULARITY: Duration = Duration::from_millis(20);
// Defined in -recovery 6.2 as 500ms but using lower value until we have RTT
// caching. See https://github.com/mozilla/neqo/issues/79
const INITIAL_RTT: Duration = Duration::from_millis(100);

const PACKET_THRESHOLD: u64 = 3;
pub const MAX_DATAGRAM_SIZE: usize = 1232; // For ipv6, smaller than ipv4 (1252)
pub const INITIAL_CWND_PKTS: usize = 10;
const INITIAL_WINDOW: usize = const_min(
    INITIAL_CWND_PKTS * MAX_DATAGRAM_SIZE,
    const_max(2 * MAX_DATAGRAM_SIZE, 14720),
);
pub const MIN_CONG_WINDOW: usize = MAX_DATAGRAM_SIZE * 2;
const PERSISTENT_CONG_THRESH: u32 = 3;

#[derive(Debug, Clone)]
pub enum RecoveryToken {
    Ack(AckToken),
    Stream(StreamRecoveryToken),
    Crypto(CryptoRecoveryToken),
    Flow(FlowControlRecoveryToken),
}

#[derive(Debug, Clone)]
pub struct SentPacket {
    ack_eliciting: bool,
    time_sent: Instant,
    pub tokens: Vec<RecoveryToken>,

    time_declared_lost: Option<Instant>,

    in_flight: bool,
    size: usize,
}

impl SentPacket {
    pub fn new(
        time_sent: Instant,
        ack_eliciting: bool,
        tokens: Vec<RecoveryToken>,
        size: usize,
        in_flight: bool,
    ) -> SentPacket {
        SentPacket {
            time_sent,
            ack_eliciting,
            tokens,
            time_declared_lost: None,
            size,
            in_flight,
        }
    }
}

#[derive(Debug, Default)]
struct RttVals {
    latest_rtt: Duration,
    smoothed_rtt: Option<Duration>,
    rttvar: Duration,
    min_rtt: Duration,
    max_ack_delay: Duration,
}

impl RttVals {
    fn update_rtt(&mut self, latest_rtt: Duration, ack_delay: Duration) {
        self.latest_rtt = latest_rtt;
        // min_rtt ignores ack delay.
        self.min_rtt = min(self.min_rtt, self.latest_rtt);
        // Limit ack_delay by max_ack_delay
        let ack_delay = min(ack_delay, self.max_ack_delay);
        // Adjust for ack delay if it's plausible.
        if self.latest_rtt - self.min_rtt >= ack_delay {
            self.latest_rtt -= ack_delay;
        }
        // Based on {{?RFC6298}}.
        match self.smoothed_rtt {
            None => {
                self.smoothed_rtt = Some(self.latest_rtt);
                self.rttvar = self.latest_rtt / 2;
            }
            Some(smoothed_rtt) => {
                let rttvar_sample = if smoothed_rtt > self.latest_rtt {
                    smoothed_rtt - self.latest_rtt
                } else {
                    self.latest_rtt - smoothed_rtt
                };

                self.rttvar = (self.rttvar * 3 / 4) + (rttvar_sample / 4);
                self.smoothed_rtt = Some((smoothed_rtt * 7 / 8) + (self.latest_rtt / 8));
            }
        }
    }

    fn rtt(&self) -> Duration {
        self.smoothed_rtt.unwrap_or(self.latest_rtt)
    }

    fn pto(&self) -> Duration {
        self.rtt() + max(4 * self.rttvar, GRANULARITY) + self.max_ack_delay
    }
}

#[derive(Debug)]
pub(crate) struct LossRecoveryState {
    mode: LossRecoveryMode,
    callback_time: Option<Instant>,
}

impl LossRecoveryState {
    fn new(mode: LossRecoveryMode, callback_time: Option<Instant>) -> LossRecoveryState {
        LossRecoveryState {
            mode,
            callback_time,
        }
    }

    pub fn callback_time(&self) -> Option<Instant> {
        self.callback_time
    }

    pub fn mode(&self) -> LossRecoveryMode {
        self.mode
    }
}

impl Default for LossRecoveryState {
    fn default() -> LossRecoveryState {
        LossRecoveryState {
            mode: LossRecoveryMode::None,
            callback_time: None,
        }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub(crate) enum LossRecoveryMode {
    None,
    LostPackets,
    PTO,
}

#[derive(Debug, Default)]
pub(crate) struct LossRecoverySpace {
    tx_pn: u64,
    largest_acked: Option<u64>,
    largest_acked_sent_time: Option<Instant>,
    sent_packets: BTreeMap<u64, SentPacket>,
}

impl LossRecoverySpace {
    pub fn earliest_sent_time(&self) -> Option<Instant> {
        // Lowest PN must have been sent earliest
        let earliest = self.sent_packets.values().next().map(|sp| sp.time_sent);
        debug_assert_eq!(
            earliest,
            self.sent_packets
                .values()
                .min_by_key(|sp| sp.time_sent)
                .map(|sp| sp.time_sent)
        );
        earliest
    }

    // Remove all the acked packets. Returns them in ascending order -- largest
    // (i.e. highest PN) acked packet is last.
    fn remove_acked(&mut self, acked_ranges: Vec<(u64, u64)>) -> (Vec<SentPacket>, bool) {
        let mut acked_packets = BTreeMap::new();
        let mut eliciting = false;
        for (end, start) in acked_ranges {
            // ^^ Notabug: see Frame::decode_ack_frame()
            for pn in start..=end {
                if let Some(sent) = self.sent_packets.remove(&pn) {
                    qdebug!("acked={}", pn);
                    eliciting |= sent.ack_eliciting;
                    acked_packets.insert(pn, sent);
                }
            }
        }
        (
            acked_packets.into_iter().map(|(_k, v)| v).collect(),
            eliciting,
        )
    }

    /// Remove all tracked packets from the space.
    /// This is called by a client when 0-RTT packets are dropped and when a Retry is received.
    fn remove_ignored(&mut self) -> impl Iterator<Item = SentPacket> {
        // The largest acknowledged or loss_time should still be unset.
        // The client should not have received any ACK frames when it drops 0-RTT.
        assert!(self.largest_acked.is_none());
        std::mem::replace(&mut self.sent_packets, BTreeMap::default())
            .into_iter()
            .map(|(_, v)| v)
    }
}

#[derive(Debug, Default)]
pub(crate) struct LossRecoverySpaces([LossRecoverySpace; 3]);

impl Index<PNSpace> for LossRecoverySpaces {
    type Output = LossRecoverySpace;

    fn index(&self, index: PNSpace) -> &Self::Output {
        &self.0[index as usize]
    }
}

impl IndexMut<PNSpace> for LossRecoverySpaces {
    fn index_mut(&mut self, index: PNSpace) -> &mut Self::Output {
        &mut self.0[index as usize]
    }
}

impl LossRecoverySpaces {
    fn iter(&self) -> impl Iterator<Item = &LossRecoverySpace> {
        self.0.iter()
    }
    fn iter_mut(&mut self) -> impl Iterator<Item = &mut LossRecoverySpace> {
        self.0.iter_mut()
    }
}

#[derive(Debug)]
struct CongestionControl {
    congestion_window: usize, // = kInitialWindow
    bytes_in_flight: usize,
    congestion_recovery_start_time: Option<Instant>,
    ssthresh: usize,
}

impl Default for CongestionControl {
    fn default() -> Self {
        CongestionControl {
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
    pub fn cwnd(&self) -> usize {
        self.congestion_window
    }

    #[cfg(test)]
    pub fn ssthresh(&self) -> usize {
        self.ssthresh
    }

    fn cwnd_avail(&self) -> usize {
        // BIF can be higher than cwnd due to PTO packets, which are sent even
        // if avail is 0, but still count towards BIF.
        self.congestion_window.saturating_sub(self.bytes_in_flight)
    }

    // Multi-packet version of OnPacketAckedCC
    fn on_packets_acked(&mut self, acked_pkts: &[SentPacket]) {
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

    fn on_packets_lost(
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

    fn on_packet_sent(&mut self, pkt: &SentPacket) {
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

    fn in_congestion_recovery(&self, sent_time: Instant) -> bool {
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

#[derive(Debug, Default)]
pub(crate) struct LossRecovery {
    pto_count: u32,
    time_of_last_sent_ack_eliciting_packet: Option<Instant>,
    rtt_vals: RttVals,

    cc: CongestionControl,

    enable_timed_loss_detection: bool,
    spaces: LossRecoverySpaces,
}

impl LossRecovery {
    pub fn new() -> LossRecovery {
        LossRecovery {
            rtt_vals: RttVals {
                min_rtt: Duration::from_secs(u64::max_value()),
                max_ack_delay: Duration::from_millis(25),
                latest_rtt: INITIAL_RTT,
                ..RttVals::default()
            },

            ..LossRecovery::default()
        }
    }

    #[cfg(test)]
    pub fn cwnd(&self) -> usize {
        self.cc.cwnd()
    }

    #[cfg(test)]
    pub fn ssthresh(&self) -> usize {
        self.cc.ssthresh()
    }

    pub fn cwnd_avail(&self) -> usize {
        self.cc.cwnd_avail()
    }

    pub fn next_pn(&mut self, pn_space: PNSpace) -> u64 {
        self.spaces[pn_space].tx_pn
    }

    pub fn inc_pn(&mut self, pn_space: PNSpace) {
        self.spaces[pn_space].tx_pn += 1;
    }

    pub fn increment_pto_count(&mut self) {
        self.pto_count += 1;
    }

    pub fn largest_acknowledged_pn(&self, pn_space: PNSpace) -> Option<u64> {
        self.spaces[pn_space].largest_acked
    }

    pub fn pto(&self) -> Duration {
        self.rtt_vals.pto()
    }

    pub fn drop_0rtt(&mut self) -> impl Iterator<Item = SentPacket> {
        self.spaces[PNSpace::ApplicationData].remove_ignored()
    }

    pub fn on_packet_sent(
        &mut self,
        pn_space: PNSpace,
        packet_number: u64,
        sent_packet: SentPacket,
    ) {
        qdebug!([self], "packet {:?}-{} sent.", pn_space, packet_number);
        if sent_packet.ack_eliciting {
            self.time_of_last_sent_ack_eliciting_packet = Some(sent_packet.time_sent);
        }
        self.cc.on_packet_sent(&sent_packet);

        self.spaces[pn_space]
            .sent_packets
            .insert(packet_number, sent_packet);
    }

    /// Returns (acked packets, lost packets)
    pub fn on_ack_received(
        &mut self,
        pn_space: PNSpace,
        largest_acked: u64,
        acked_ranges: Vec<(u64, u64)>,
        ack_delay: Duration,
        now: Instant,
    ) -> (Vec<SentPacket>, Vec<SentPacket>) {
        qdebug!(
            [self],
            "ack received for {:?} - largest_acked={}.",
            pn_space,
            largest_acked
        );

        let (acked_packets, any_ack_eliciting) = self.spaces[pn_space].remove_acked(acked_ranges);
        if acked_packets.is_empty() {
            // No new information.
            return (Vec::new(), Vec::new());
        }

        // Track largest PN acked per space
        let space = &mut self.spaces[pn_space];
        let prev_largest_acked_sent_time = space.largest_acked_sent_time;
        if Some(largest_acked) > space.largest_acked {
            space.largest_acked = Some(largest_acked);

            // If the largest acknowledged is newly acked and any newly acked
            // packet was ack-eliciting, update the RTT. (-recovery 5.1)
            let largest_acked_pkt = acked_packets.last().expect("must be there");
            space.largest_acked_sent_time = Some(largest_acked_pkt.time_sent);
            if any_ack_eliciting {
                let latest_rtt = now - largest_acked_pkt.time_sent;
                self.rtt_vals.update_rtt(latest_rtt, ack_delay);
            }
        }

        // TODO Process ECN information if present.

        let lost_packets = self.detect_lost_packets(pn_space, now);

        self.pto_count = 0;

        self.cc.on_packets_acked(&acked_packets);

        self.cc.on_packets_lost(
            now,
            prev_largest_acked_sent_time,
            self.rtt_vals.pto(),
            &lost_packets,
        );

        (acked_packets, lost_packets)
    }

    fn loss_delay(&self) -> Duration {
        // kTimeThreshold = 9/8
        // loss_delay = kTimeThreshold * max(latest_rtt, smoothed_rtt)
        // loss_delay = max(loss_delay, kGranularity)
        let rtt = match self.rtt_vals.smoothed_rtt {
            None => self.rtt_vals.latest_rtt,
            Some(smoothed_rtt) => max(self.rtt_vals.latest_rtt, smoothed_rtt),
        };
        max(rtt * 9 / 8, GRANULARITY)
    }

    /// When receiving a retry, get all the sent packets so that they can be flushed.
    /// We also need to pretend that they never happened for the purposes of congestion control.
    pub fn retry(&mut self) -> Vec<SentPacket> {
        self.spaces
            .iter_mut()
            .flat_map(|spc| spc.remove_ignored())
            .collect()
    }

    /// Detect packets whose contents may need to be retransmitted.
    pub fn detect_lost_packets(&mut self, pn_space: PNSpace, now: Instant) -> Vec<SentPacket> {
        self.enable_timed_loss_detection = false;
        let loss_delay = self.loss_delay();

        // Packets sent before this time are deemed lost.
        let lost_deadline = now - loss_delay;
        qdebug!(
            [self],
            "detect lost packets = now {:?} loss delay {:?} lost_deadline {:?}",
            now,
            loss_delay,
            lost_deadline
        );

        let packet_space = &mut self.spaces[pn_space];
        let largest_acked = packet_space.largest_acked;

        // Lost for retrans/CC purposes
        let mut lost_pns = SmallVec::<[_; 8]>::new();

        // Lost for we-can-actually-forget-about-it purposes
        let mut really_lost_pns = SmallVec::<[_; 8]>::new();

        let current_rtt = self.rtt_vals.rtt();
        for (pn, packet) in packet_space
            .sent_packets
            .iter_mut()
            // BTreeMap iterates in order of ascending PN
            .take_while(|(&k, _)| Some(k) < largest_acked)
        {
            let lost = if packet.time_sent <= lost_deadline {
                qdebug!(
                    "lost={}, time sent {:?} is before lost_deadline {:?}",
                    pn,
                    packet.time_sent,
                    lost_deadline
                );
                true
            } else if largest_acked >= Some(*pn + PACKET_THRESHOLD) {
                qdebug!(
                    "lost={}, is >= {} from largest acked {:?}",
                    pn,
                    PACKET_THRESHOLD,
                    largest_acked
                );
                true
            } else {
                false
            };

            if lost && packet.time_declared_lost.is_none() {
                // Track declared-lost packets for a little while, maybe they
                // will still show up?
                packet.time_declared_lost = Some(now);

                lost_pns.push(*pn);
            } else if packet
                .time_declared_lost
                .map(|tdl| tdl + (current_rtt * 2) < now)
                .unwrap_or(false)
            {
                really_lost_pns.push(*pn);
            } else {
                // OOO but not quite lost yet. Set the timed loss detect timer
                self.enable_timed_loss_detection = true;
            }
        }

        for pn in really_lost_pns {
            packet_space
                .sent_packets
                .remove(&pn)
                .expect("PN must be in sent_packets");
        }

        let mut lost_packets = Vec::with_capacity(lost_pns.len());
        for pn in lost_pns {
            let lost_packet = packet_space
                .sent_packets
                .get(&pn)
                .expect("PN must be in sent_packets")
                .clone();
            lost_packets.push(lost_packet);
        }

        lost_packets
    }

    pub fn get_timer(&mut self) -> LossRecoveryState {
        qdebug!([self], "get_loss_detection_timer.");

        let has_ack_eliciting_out = self
            .spaces
            .iter()
            .flat_map(|spc| spc.sent_packets.values())
            .any(|sp| sp.ack_eliciting);

        qdebug!([self], "has_ack_eliciting_out={}", has_ack_eliciting_out,);

        if !has_ack_eliciting_out {
            return LossRecoveryState::new(LossRecoveryMode::None, None);
        }

        qinfo!(
            [self],
            "sent packets {} {} {}",
            self.spaces[PNSpace::Initial].sent_packets.len(),
            self.spaces[PNSpace::Handshake].sent_packets.len(),
            self.spaces[PNSpace::ApplicationData].sent_packets.len()
        );

        // QUIC only has one timer, but it does double duty because it falls
        // back to other uses if first use is not needed: first the loss
        // detection timer, and then the probe timeout (PTO).

        let (mode, maybe_timer) = if let Some((_, earliest_time)) = self.get_earliest_loss_time() {
            (LossRecoveryMode::LostPackets, Some(earliest_time))
        } else {
            // Calculate PTO duration
            let timeout = self.rtt_vals.pto() * 2_u32.pow(self.pto_count);
            (
                LossRecoveryMode::PTO,
                self.time_of_last_sent_ack_eliciting_packet
                    .map(|i| i + timeout),
            )
        };

        qdebug!(
            [self],
            "loss_detection_timer mode={:?} timer={:?}",
            mode,
            maybe_timer
        );
        LossRecoveryState::new(mode, maybe_timer)
    }

    /// Find when the earliest sent packet should be considered lost.
    pub fn get_earliest_loss_time(&self) -> Option<(PNSpace, Instant)> {
        if !self.enable_timed_loss_detection {
            return None;
        }

        PNSpace::iter()
            .map(|spc| (*spc, self.spaces[*spc].earliest_sent_time()))
            .filter_map(|(spc, time)| {
                // None is ordered less than Some(_). Bad. Filter them out.
                if let Some(time) = time {
                    Some((spc, time))
                } else {
                    None
                }
            })
            .min_by_key(|&(_, time)| time)
            .map(|(spc, val)| (spc, val + self.loss_delay()))
    }
}

impl ::std::fmt::Display for LossRecovery {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "LossRecovery")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::convert::TryInto;
    use std::time::{Duration, Instant};

    const ON_SENT_SIZE: usize = 100;

    fn assert_rtts(
        lr: &LossRecovery,
        latest_rtt: Duration,
        smoothed_rtt: Duration,
        rttvar: Duration,
        min_rtt: Duration,
    ) {
        println!(
            "rtts: {:?} {:?} {:?} {:?}",
            lr.rtt_vals.latest_rtt,
            lr.rtt_vals.smoothed_rtt,
            lr.rtt_vals.rttvar,
            lr.rtt_vals.min_rtt,
        );
        assert_eq!(lr.rtt_vals.latest_rtt, latest_rtt, "latest RTT");
        assert_eq!(lr.rtt_vals.smoothed_rtt, Some(smoothed_rtt), "smoothed RTT");
        assert_eq!(lr.rtt_vals.rttvar, rttvar, "RTT variance");
        assert_eq!(lr.rtt_vals.min_rtt, min_rtt, "min RTT");
    }

    fn assert_sent_times(
        lr: &LossRecovery,
        initial: Option<Instant>,
        handshake: Option<Instant>,
        app_data: Option<Instant>,
    ) {
        if !lr.enable_timed_loss_detection {
            return;
        }

        println!(
            "loss times: {:?} {:?} {:?}",
            lr.spaces[PNSpace::Initial].earliest_sent_time(),
            lr.spaces[PNSpace::Handshake].earliest_sent_time(),
            lr.spaces[PNSpace::ApplicationData].earliest_sent_time(),
        );
        assert_eq!(
            lr.spaces[PNSpace::Initial].earliest_sent_time(),
            initial,
            "Initial earliest sent time"
        );
        assert_eq!(
            lr.spaces[PNSpace::Handshake].earliest_sent_time(),
            handshake,
            "Handshake earliest sent time"
        );
        assert_eq!(
            lr.spaces[PNSpace::ApplicationData].earliest_sent_time(),
            app_data,
            "AppData earliest sent time"
        );
    }

    fn assert_no_sent_times(lr: &LossRecovery) {
        assert_sent_times(lr, None, None, None);
    }

    // Time in milliseconds.
    macro_rules! ms {
        ($t:expr) => {
            Duration::from_millis($t)
        };
    }

    // In most of the tests below, packets are sent at a fixed cadence, with PACING between each.
    const PACING: Duration = ms!(7);
    fn pn_time(pn: u64) -> Instant {
        ::test_fixture::now() + (PACING * pn.try_into().unwrap())
    }

    fn pace(lr: &mut LossRecovery, count: u64) {
        for pn in 0..count {
            lr.on_packet_sent(
                PNSpace::ApplicationData,
                pn,
                SentPacket::new(pn_time(pn), true, Vec::new(), ON_SENT_SIZE, true),
            );
        }
    }

    const ACK_DELAY: Duration = ms!(24);
    /// Acknowledge PN with the identified delay.
    fn ack(lr: &mut LossRecovery, pn: u64, delay: Duration) {
        lr.on_ack_received(
            PNSpace::ApplicationData,
            pn,
            vec![(pn, pn)],
            ACK_DELAY,
            pn_time(pn) + delay,
        );
    }

    #[test]
    fn initial_rtt() {
        let mut lr = LossRecovery::new();
        pace(&mut lr, 1);
        let rtt = ms!(100);
        ack(&mut lr, 0, rtt);
        assert_rtts(&lr, rtt, rtt, rtt / 2, rtt);
        assert_no_sent_times(&lr);
    }

    /// An INITIAL_RTT for using with setup_lr().
    const INITIAL_RTT: Duration = ms!(80);
    const INITIAL_RTTVAR: Duration = ms!(40);

    /// Send `n` packets (using PACING), then acknowledge the first.
    fn setup_lr(n: u64) -> LossRecovery {
        let mut lr = LossRecovery::new();
        pace(&mut lr, n);
        ack(&mut lr, 0, INITIAL_RTT);
        assert_rtts(&lr, INITIAL_RTT, INITIAL_RTT, INITIAL_RTTVAR, INITIAL_RTT);
        assert_no_sent_times(&lr);
        lr
    }

    // The ack delay is removed from any RTT estimate.
    #[test]
    fn ack_delay_adjusted() {
        let mut lr = setup_lr(2);
        ack(&mut lr, 1, INITIAL_RTT + ACK_DELAY);
        // RTT stays the same, but the RTTVAR is adjusted downwards.
        assert_rtts(
            &lr,
            INITIAL_RTT,
            INITIAL_RTT,
            INITIAL_RTTVAR * 3 / 4,
            INITIAL_RTT,
        );
        assert_no_sent_times(&lr);
    }

    // The ack delay is ignored when it would cause a sample to be less than min_rtt.
    #[test]
    fn ack_delay_ignored() {
        let mut lr = setup_lr(2);
        let extra = ms!(8);
        assert!(extra < ACK_DELAY);
        ack(&mut lr, 1, INITIAL_RTT + extra);
        let expected_rtt = INITIAL_RTT + (extra / 8);
        let expected_rttvar = (INITIAL_RTTVAR * 3 + extra) / 4;
        assert_rtts(
            &lr,
            INITIAL_RTT + extra,
            expected_rtt,
            expected_rttvar,
            INITIAL_RTT,
        );
        assert_no_sent_times(&lr);
    }

    // A lower observed RTT is used as min_rtt (and ack delay is ignored).
    #[test]
    fn reduce_min_rtt() {
        let mut lr = setup_lr(2);
        let delta = ms!(4);
        let reduced_rtt = INITIAL_RTT - delta;
        ack(&mut lr, 1, reduced_rtt);
        let expected_rtt = INITIAL_RTT - (delta / 8);
        let expected_rttvar = (INITIAL_RTTVAR * 3 + delta) / 4;
        assert_rtts(&lr, reduced_rtt, expected_rtt, expected_rttvar, reduced_rtt);
        assert_no_sent_times(&lr);
    }

    // Acknowledging something again has no effect.
    #[test]
    fn no_new_acks() {
        let mut lr = setup_lr(1);
        let check = |lr: &LossRecovery| {
            assert_rtts(&lr, INITIAL_RTT, INITIAL_RTT, INITIAL_RTTVAR, INITIAL_RTT);
            assert_no_sent_times(&lr);
        };
        check(&lr);

        ack(&mut lr, 0, ms!(1339)); // much delayed ACK
        check(&lr);

        ack(&mut lr, 0, ms!(3)); // time travel!
        check(&lr);
    }

    // Test time loss detection as part of handling a regular ACK.
    #[test]
    fn time_loss_detection_gap() {
        let mut lr = LossRecovery::new();
        // Create a single packet gap, and have pn 0 time out.
        // This can't use the default pacing, which is too tight.
        // So send two packets with 1/4 RTT between them.  Acknowledge pn 1 after 1 RTT.
        // pn 0 should then be marked lost because it is then outstanding for 5RTT/4
        // the loss time for packets is 9RTT/8.
        lr.on_packet_sent(
            PNSpace::ApplicationData,
            0,
            SentPacket::new(pn_time(0), true, Vec::new(), ON_SENT_SIZE, true),
        );
        lr.on_packet_sent(
            PNSpace::ApplicationData,
            1,
            SentPacket::new(
                pn_time(0) + INITIAL_RTT / 4,
                true,
                Vec::new(),
                ON_SENT_SIZE,
                true,
            ),
        );
        let (_, lost) = lr.on_ack_received(
            PNSpace::ApplicationData,
            1,
            vec![(1, 1)],
            ACK_DELAY,
            pn_time(0) + (INITIAL_RTT * 5 / 4),
        );
        assert_eq!(lost.len(), 1);
        assert_no_sent_times(&lr);
    }

    // Test time loss detection as part of an explicit timeout.
    #[test]
    fn time_loss_detection_timeout() {
        let mut lr = setup_lr(3);
        // Create a small gap so that pn 1 can be regarded as lost.
        // Make sure to provide this before the loss timer for pn 1 expires.
        // This relies on having `PACING < INITIAL_RTT/8`.  We want to keep the RTT constant,
        // but we also want to ensure that acknowledging pn 2 doesn't cause pn 1 to be lost.
        // Because pn 1 was sent at `pn_time(2) - PACING` and
        // `pn_time(2) - PACING + (INITIAL_RTT * 9 /8)` needs to be in the future.
        assert!(PACING < (INITIAL_RTT / 8));
        let (_, lost) = lr.on_ack_received(
            PNSpace::ApplicationData,
            2,
            vec![(2, 2)],
            ACK_DELAY,
            pn_time(2) + INITIAL_RTT,
        );
        assert!(lost.is_empty());
        let pn1_sent_time = pn_time(1);
        assert_sent_times(&lr, None, None, Some(pn1_sent_time));

        // After time elapses, pn 1 is marked lost.
        let lr_state = lr.get_timer();
        let pn1_lost_time = pn1_sent_time + (INITIAL_RTT * 9 / 8);
        assert_eq!(lr_state.callback_time, Some(pn1_lost_time));
        match lr_state.mode {
            LossRecoveryMode::LostPackets => {
                let packets = lr.detect_lost_packets(PNSpace::ApplicationData, pn1_lost_time);

                assert_eq!(packets.len(), 1)
            }
            _ => panic!("unexpected mode"),
        }
        assert_no_sent_times(&lr);
    }

    #[test]
    fn big_gap_loss() {
        let mut lr = setup_lr(5); // This sends packets 0-4 and acknowledges pn 0.
                                  // Acknowledge just 2-4, which will cause pn 1 to be marked as lost.
        assert_eq!(super::PACKET_THRESHOLD, 3);
        let (_, lost) = lr.on_ack_received(
            PNSpace::ApplicationData,
            4,
            vec![(4, 2)],
            ACK_DELAY,
            pn_time(4),
        );
        assert_eq!(lost.len(), 1);
    }
}
