// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of sent packets and detecting their loss.

use std::cmp::{max, min};
use std::collections::BTreeMap;
use std::ops::{Index, IndexMut};
use std::time::{Duration, Instant};

use smallvec::SmallVec;

use neqo_common::{qdebug, qinfo, qtrace};

use crate::cc::CongestionControl;
use crate::crypto::CryptoRecoveryToken;
use crate::flow_mgr::FlowControlRecoveryToken;
use crate::send_stream::StreamRecoveryToken;
use crate::tracking::{AckToken, PNSpace, SentPacket};
use crate::LOCAL_IDLE_TIMEOUT;

const GRANULARITY: Duration = Duration::from_millis(20);
// Defined in -recovery 6.2 as 500ms but using lower value until we have RTT
// caching. See https://github.com/mozilla/neqo/issues/79
const INITIAL_RTT: Duration = Duration::from_millis(100);
const PACKET_THRESHOLD: u64 = 3;

#[derive(Debug, Clone)]
pub enum RecoveryToken {
    Ack(AckToken),
    Stream(StreamRecoveryToken),
    Crypto(CryptoRecoveryToken),
    Flow(FlowControlRecoveryToken),
    HandshakeDone,
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

    fn pto(&self, pn_space: PNSpace) -> Duration {
        self.rtt()
            + max(4 * self.rttvar, GRANULARITY)
            + if pn_space != PNSpace::ApplicationData {
                Duration::from_millis(0)
            } else {
                self.max_ack_delay
            }
    }
}

#[derive(Debug)]
pub(crate) struct LossRecoveryState {
    mode: LossRecoveryMode,
    callback_time: Option<Instant>,
}

impl LossRecoveryState {
    fn new(mode: LossRecoveryMode, callback_time: Option<Instant>) -> Self {
        Self {
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

    pub fn check_pto(&mut self) -> Option<(PNSpace, bool)> {
        if let LossRecoveryMode::PtoExpired {
            dgram_available,
            min_pn_space,
        } = &mut self.mode
        {
            if *dgram_available > 0 {
                *dgram_available -= 1;
                Some((*min_pn_space, true))
            } else {
                Some((*min_pn_space, false))
            }
        } else {
            None
        }
    }
}

impl Default for LossRecoveryState {
    fn default() -> Self {
        Self {
            mode: LossRecoveryMode::None,
            callback_time: None,
        }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub(crate) enum LossRecoveryMode {
    None,
    LostPacketsTimer, // lost packet timer is armed.
    PtoTimer,         // pto timer is armed
    PtoExpired {
        dgram_available: usize,
        min_pn_space: PNSpace,
    }, // pto expired, in this state we should send pto packets.
}

#[derive(Debug, Default)]
pub(crate) struct LossRecoverySpace {
    largest_acked: Option<u64>,
    largest_acked_sent_time: Option<Instant>,
    time_of_last_sent_ack_eliciting_packet: Option<Instant>,
    ack_eliciting_outstanding: u64,
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

    pub fn get_largest_acked(&self) -> Option<u64> {
        self.largest_acked
    }

    pub fn ack_eliciting_outstanding(&self) -> bool {
        self.ack_eliciting_outstanding > 0
    }

    pub fn pto_packets(&mut self, count: usize) -> impl Iterator<Item = &SentPacket> {
        self.sent_packets
            .iter_mut()
            .filter_map(|(pn, sent)| {
                if sent.pto() {
                    qtrace!("PTO: marking packet {} lost ", pn);
                    Some(&*sent)
                } else {
                    None
                }
            })
            .take(count)
    }

    pub fn time_of_last_sent_ack_eliciting_packet(&self) -> Option<Instant> {
        if self.ack_eliciting_outstanding() {
            debug_assert!(self.time_of_last_sent_ack_eliciting_packet.is_some());
            self.time_of_last_sent_ack_eliciting_packet
        } else {
            None
        }
    }

    pub fn on_packet_sent(&mut self, packet_number: u64, sent_packet: SentPacket) {
        if sent_packet.ack_eliciting {
            self.time_of_last_sent_ack_eliciting_packet = Some(sent_packet.time_sent);
            self.ack_eliciting_outstanding += 1;
        }
        self.sent_packets.insert(packet_number, sent_packet);
    }

    pub fn remove_packet(&mut self, pn: u64) -> Option<SentPacket> {
        if let Some(sent) = self.sent_packets.remove(&pn) {
            if sent.ack_eliciting {
                debug_assert!(self.ack_eliciting_outstanding > 0);
                self.ack_eliciting_outstanding -= 1;
            }
            Some(sent)
        } else {
            None
        }
    }

    // Remove all the acked packets. Returns them in ascending order -- largest
    // (i.e. highest PN) acked packet is last.
    fn remove_acked(&mut self, acked_ranges: Vec<(u64, u64)>) -> (Vec<SentPacket>, bool) {
        let mut acked_packets = BTreeMap::new();
        let mut eliciting = false;
        for (end, start) in acked_ranges {
            // ^^ Notabug: see Frame::decode_ack_frame()
            for pn in start..=end {
                if let Some(sent) = self.remove_packet(pn) {
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
    /// This is called by a client when 0-RTT packets are dropped, when a Retry is received
    /// and when keys are dropped.
    fn remove_ignored(&mut self) -> impl Iterator<Item = SentPacket> {
        self.ack_eliciting_outstanding = 0;
        std::mem::take(&mut self.sent_packets)
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

#[derive(Debug, Default)]
pub(crate) struct LossRecovery {
    pto_count: u32,
    rtt_vals: RttVals,
    cc: CongestionControl,

    enable_timed_loss_detection: bool,
    spaces: LossRecoverySpaces,
    loss_recovery_state: LossRecoveryState,
}

impl LossRecovery {
    pub fn new() -> Self {
        Self {
            rtt_vals: RttVals {
                min_rtt: Duration::from_secs(u64::max_value()),
                max_ack_delay: Duration::from_millis(25),
                latest_rtt: INITIAL_RTT,
                ..RttVals::default()
            },

            ..Self::default()
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

    pub fn largest_acknowledged_pn(&self, pn_space: PNSpace) -> Option<u64> {
        self.spaces[pn_space].largest_acked
    }

    pub fn pto(&self) -> Duration {
        self.rtt_vals.pto(PNSpace::ApplicationData)
    }

    pub fn drop_0rtt(&mut self) -> Vec<SentPacket> {
        // The largest acknowledged or loss_time should still be unset.
        // The client should not have received any ACK frames when it drops 0-RTT.
        assert!(self.spaces[PNSpace::ApplicationData]
            .get_largest_acked()
            .is_none());
        self.spaces[PNSpace::ApplicationData]
            .remove_ignored()
            .inspect(|p| self.cc.discard(&p))
            .collect()
    }

    pub fn on_packet_sent(
        &mut self,
        pn_space: PNSpace,
        packet_number: u64,
        sent_packet: SentPacket,
    ) {
        qdebug!([self], "packet {:?}-{} sent.", pn_space, packet_number);
        self.cc.on_packet_sent(&sent_packet);
        self.spaces[pn_space].on_packet_sent(packet_number, sent_packet);
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
            self.rtt_vals.pto(pn_space),
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

    // Calculate PTO duration
    fn pto_timeout(&self, pn_space: PNSpace) -> Duration {
        self.rtt_vals
            .pto(pn_space)
            .checked_mul(1 << self.pto_count)
            .unwrap_or(LOCAL_IDLE_TIMEOUT * 2)
    }

    /// When receiving a retry, get all the sent packets so that they can be flushed.
    /// We also need to pretend that they never happened for the purposes of congestion control.
    pub fn retry(&mut self) -> Vec<SentPacket> {
        let cc = &mut self.cc;
        self.spaces
            .iter_mut()
            .flat_map(|spc| spc.remove_ignored())
            .inspect(|p| cc.discard(&p))
            .collect()
    }

    /// Discard state for a given packet number space.
    pub fn discard(&mut self, space: PNSpace) {
        qdebug!([self], "Reset loss recovery state for {}", space);
        // We just made progress, so discard PTO count.
        self.pto_count = 0;
        for p in self.spaces[space].remove_ignored() {
            self.cc.discard(&p);
        }
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
                .remove_packet(pn)
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

    pub fn callback_time(&mut self) -> Option<Instant> {
        self.loss_recovery_state.callback_time()
    }

    #[cfg(test)]
    pub fn state_mode(&self) -> LossRecoveryMode {
        self.loss_recovery_state.mode()
    }

    pub fn calculate_timer(&mut self) -> Option<Instant> {
        qtrace!([self], "get_loss_detection_timer.");

        let has_ack_eliciting_out = self.spaces.iter().any(|sp| sp.ack_eliciting_outstanding());

        qdebug!([self], "has_ack_eliciting_out={}", has_ack_eliciting_out,);

        if !has_ack_eliciting_out {
            self.loss_recovery_state = LossRecoveryState::new(LossRecoveryMode::None, None);
            return None;
        }

        qinfo!(
            [self],
            "sent packets init:({} ack_eliciting:{}), hs:({} ack_eliciting:{}), app:({} ack_eliciting:{})",
            self.spaces[PNSpace::Initial].sent_packets.len(),
            self.spaces[PNSpace::Initial].ack_eliciting_outstanding(),
            self.spaces[PNSpace::Handshake].sent_packets.len(),
            self.spaces[PNSpace::Handshake].ack_eliciting_outstanding(),
            self.spaces[PNSpace::ApplicationData].sent_packets.len(),
            self.spaces[PNSpace::ApplicationData].ack_eliciting_outstanding()
        );

        // QUIC only has one timer, but it does double duty because it falls
        // back to other uses if first use is not needed: first the loss
        // detection timer, and then the probe timeout (PTO).

        self.loss_recovery_state = if let Some((_, earliest_time)) = self.get_earliest_loss_time() {
            LossRecoveryState::new(LossRecoveryMode::LostPacketsTimer, Some(earliest_time))
        } else {
            LossRecoveryState::new(LossRecoveryMode::PtoTimer, self.get_min_pto())
        };

        qdebug!(
            [self],
            "loss_detection_timer mode={:?} timer={:?}",
            self.loss_recovery_state.mode(),
            self.loss_recovery_state.callback_time()
        );
        self.loss_recovery_state.callback_time()
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

    fn pto_time(&self, pn_space: PNSpace) -> Option<Instant> {
        if let Some(time) = self.spaces[pn_space].time_of_last_sent_ack_eliciting_packet() {
            Some(time + self.pto_timeout(pn_space))
        } else {
            None
        }
    }

    /// Find when the last ack eliciting packet was sent.
    pub fn get_min_pto(&self) -> Option<Instant> {
        // TODO ignore PNSpace::Application until handshake is done -> a server side problem.
        PNSpace::iter()
            .filter_map(|spc| self.pto_time(*spc))
            .min_by_key(|&time| time)
    }

    /// This checks whether the PTO timer has fired.
    /// When it has, mark a few packets as "lost" for the purposes of having frames
    /// regenerated in subsequent packets.  The packets aren't truly lost, so
    /// we have to clone the `SentPacket` instance.
    fn check_pto_timer(&mut self, now: Instant) -> Option<Vec<SentPacket>> {
        const PTO_COUNT: usize = 2;

        let mut pto_space = None;
        let mut lost = Vec::new();
        for space in PNSpace::iter() {
            // Skip early packet number spaces where the PTO timer hasn't fired.
            // Once the timer for one space has fired, include higher spaces. Declaring more
            // data as "lost" makes it more likely that PTO packets will include useful data.
            if pto_space.is_none() && self.pto_time(*space).map(|t| t > now).unwrap_or(true) {
                continue;
            }
            qdebug!([self], "PTO timer fired for {}", space);
            pto_space = pto_space.or(Some(*space));
            lost.extend(self.spaces[*space].pto_packets(PTO_COUNT).cloned());
        }

        if let Some(space) = pto_space {
            self.loss_recovery_state = LossRecoveryState::new(
                LossRecoveryMode::PtoExpired {
                    dgram_available: PTO_COUNT,
                    min_pn_space: space,
                },
                Some(now),
            );
            self.pto_count += 1;
        }
        Some(lost)
    }

    pub fn check_loss_detection_timeout(&mut self, now: Instant) -> Option<Vec<SentPacket>> {
        qdebug!([self], "check_loss_timeouts");

        if self.loss_recovery_state.mode() == LossRecoveryMode::None {
            // LR not the active timer
            return None;
        }

        if self.callback_time() > Some(now) {
            // LR timer, but hasn't expired.
            return None;
        }

        // Timer expired and LR was active timer.
        match self.loss_recovery_state.mode() {
            LossRecoveryMode::None => unreachable!(),
            LossRecoveryMode::LostPacketsTimer => {
                // Time threshold loss detection
                let (pn_space, _) = self
                    .get_earliest_loss_time()
                    .expect("must be sent packets if in LostPackets mode");
                Some(self.detect_lost_packets(pn_space, now))
            }
            LossRecoveryMode::PtoTimer => {
                qinfo!([self], "PTO timer fired, send 1 or 2 packets");
                self.check_pto_timer(now)
            }
            _ => None, // We are already in PtoExpired state
        }
    }

    /// Check the PTO state and - if there is a PTO - return the packet number space and
    /// whether more packets can be sent.
    pub fn check_pto(&mut self) -> Option<(PNSpace, bool)> {
        self.loss_recovery_state.check_pto()
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
        let callback_time = lr.calculate_timer();
        let pn1_lost_time = pn1_sent_time + (INITIAL_RTT * 9 / 8);
        assert_eq!(callback_time, Some(pn1_lost_time));
        match lr.state_mode() {
            LossRecoveryMode::LostPacketsTimer => {
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
