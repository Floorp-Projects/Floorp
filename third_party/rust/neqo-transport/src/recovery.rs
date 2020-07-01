// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of sent packets and detecting their loss.

#![deny(clippy::pedantic)]

use std::cell::RefCell;
use std::cmp::{max, min};
use std::collections::BTreeMap;
use std::rc::Rc;
use std::time::{Duration, Instant};

use smallvec::{smallvec, SmallVec};

use neqo_common::{qdebug, qinfo, qlog::NeqoQlog, qtrace, qwarn};

use crate::cc::CongestionControl;
use crate::crypto::CryptoRecoveryToken;
use crate::flow_mgr::FlowControlRecoveryToken;
use crate::qlog::{self, QlogMetric};
use crate::send_stream::StreamRecoveryToken;
use crate::tracking::{AckToken, PNSpace, SentPacket};
use crate::LOCAL_IDLE_TIMEOUT;

pub const GRANULARITY: Duration = Duration::from_millis(20);
pub const MAX_ACK_DELAY: Duration = Duration::from_millis(25);
// Defined in -recovery 6.2 as 500ms but using lower value until we have RTT
// caching. See https://github.com/mozilla/neqo/issues/79
const INITIAL_RTT: Duration = Duration::from_millis(100);
const PACKET_THRESHOLD: u64 = 3;
/// `ACK_ONLY_SIZE_LIMIT` is the minimum size of the congestion window.
/// If the congestion window is this small, we will only send ACK frames.
pub(crate) const ACK_ONLY_SIZE_LIMIT: usize = 256;
/// The number of packets we send on a PTO.
/// And the number to declare lost when the PTO timer is hit.
pub const PTO_PACKET_COUNT: usize = 2;

#[derive(Debug, Clone)]
#[allow(clippy::module_name_repetitions)]
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
    fn update_rtt(
        &mut self,
        qlog: &Rc<RefCell<Option<NeqoQlog>>>,
        latest_rtt: Duration,
        ack_delay: Duration,
    ) {
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

                self.rttvar = (self.rttvar * 3 + rttvar_sample) / 4;
                self.smoothed_rtt = Some((smoothed_rtt * 7 + self.latest_rtt) / 8);
            }
        }
        qlog::metrics_updated(
            &mut qlog.borrow_mut(),
            &[
                QlogMetric::LatestRtt(self.latest_rtt),
                QlogMetric::MinRtt(self.min_rtt),
                QlogMetric::SmoothedRtt(self.smoothed_rtt.unwrap()),
            ],
        );
    }

    pub fn rtt(&self) -> Duration {
        self.smoothed_rtt.unwrap_or(self.latest_rtt)
    }

    fn pto(&self, pn_space: PNSpace) -> Duration {
        self.rtt()
            + max(4 * self.rttvar, GRANULARITY)
            + if pn_space == PNSpace::ApplicationData {
                self.max_ack_delay
            } else {
                Duration::from_millis(0)
            }
    }
}

/// `SendProfile` tells a sender how to send packets.
#[derive(Debug)]
pub struct SendProfile {
    limit: usize,
    pto: Option<PNSpace>,
    paced: bool,
}

impl SendProfile {
    pub fn new_limited(limit: usize) -> Self {
        // When the limit is too low, we only send ACK frames.
        // Set the limit to `ACK_ONLY_SIZE_LIMIT - 1` to ensure that
        // ACK-only packets are still limited in size.
        Self {
            limit: max(ACK_ONLY_SIZE_LIMIT - 1, limit),
            pto: None,
            paced: false,
        }
    }

    pub fn new_paced() -> Self {
        // When pacing, we still allow ACK frames to be sent.
        Self {
            limit: ACK_ONLY_SIZE_LIMIT - 1,
            pto: None,
            paced: true,
        }
    }

    pub fn new_pto(pn_space: PNSpace, mtu: usize) -> Self {
        debug_assert!(mtu > ACK_ONLY_SIZE_LIMIT);
        Self {
            limit: mtu,
            pto: Some(pn_space),
            paced: false,
        }
    }

    pub fn pto(&self) -> bool {
        self.pto.is_some()
    }

    /// Determine whether an ACK-only packet should be sent for the given packet
    /// number space.
    /// Send only ACKs either: when the space available is too small, or when a PTO
    /// exists for a later packet number space (which could use extra space for data).
    pub fn ack_only(&self, space: PNSpace) -> bool {
        self.limit < ACK_ONLY_SIZE_LIMIT || self.pto.map_or(false, |sp| space < sp)
    }

    pub fn paced(&self) -> bool {
        self.paced
    }

    pub fn limit(&self) -> usize {
        self.limit
    }
}

#[derive(Debug)]
pub(crate) struct LossRecoverySpace {
    space: PNSpace,
    largest_acked: Option<u64>,
    largest_acked_sent_time: Option<Instant>,
    /// The time used to calculate the PTO timer for this space.
    /// This is the time that the last ACK-eliciting packet in this space
    /// was sent.  This might be the time that a probe was sent.
    pto_base_time: Option<Instant>,
    /// The number of outstanding packets in this space that are in flight.
    /// This might be less than the number of ACK-eliciting packets,
    /// because PTO packets don't count.
    in_flight_outstanding: u64,
    sent_packets: BTreeMap<u64, SentPacket>,
    /// The time that the first out-of-order packet was sent.
    /// This is `None` if there were no out-of-order packets detected.
    /// When set to `Some(T)`, time-based loss detection should be enabled.
    first_ooo_time: Option<Instant>,
}

impl LossRecoverySpace {
    pub fn new(space: PNSpace) -> Self {
        Self {
            space,
            largest_acked: None,
            largest_acked_sent_time: None,
            pto_base_time: None,
            in_flight_outstanding: 0,
            sent_packets: BTreeMap::default(),
            first_ooo_time: None,
        }
    }

    #[must_use]
    pub fn space(&self) -> PNSpace {
        self.space
    }

    /// Find the time we sent the first packet that is lower than the
    /// largest acknowledged and that isn't yet declared lost.
    /// Use the value we prepared earlier in `detect_lost_packets`.
    #[must_use]
    pub fn loss_recovery_timer_start(&self) -> Option<Instant> {
        self.first_ooo_time
    }

    pub fn in_flight_outstanding(&self) -> bool {
        self.in_flight_outstanding > 0
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

    pub fn pto_base_time(&self) -> Option<Instant> {
        if self.in_flight_outstanding() {
            debug_assert!(self.pto_base_time.is_some());
            self.pto_base_time
        } else {
            None
        }
    }

    pub fn on_packet_sent(&mut self, sent_packet: SentPacket) {
        if sent_packet.ack_eliciting() {
            self.pto_base_time = Some(sent_packet.time_sent);
            if sent_packet.cc_in_flight() {
                self.in_flight_outstanding += 1;
            }
        }
        self.sent_packets.insert(sent_packet.pn, sent_packet);
    }

    pub fn remove_packet(&mut self, pn: u64) -> Option<SentPacket> {
        if let Some(sent) = self.sent_packets.remove(&pn) {
            if sent.cc_in_flight() {
                debug_assert!(self.in_flight_outstanding > 0);
                self.in_flight_outstanding -= 1;
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
                    eliciting |= sent.ack_eliciting();
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
        self.in_flight_outstanding = 0;
        std::mem::take(&mut self.sent_packets)
            .into_iter()
            .map(|(_, v)| v)
    }

    pub fn detect_lost_packets(
        &mut self,
        now: Instant,
        loss_delay: Duration,
        lost_packets: &mut Vec<SentPacket>,
    ) {
        // Packets sent before this time are deemed lost.
        let lost_deadline = now - loss_delay;
        qtrace!(
            "detect lost {}: now={:?} delay={:?} deadline={:?}",
            self.space,
            now,
            loss_delay,
            lost_deadline
        );
        self.first_ooo_time = None;

        let largest_acked = self.largest_acked;

        // Lost for retrans/CC purposes
        let mut lost_pns = SmallVec::<[_; 8]>::new();

        // Lost for we-can-actually-forget-about-it purposes
        let mut really_lost_pns = SmallVec::<[_; 8]>::new();

        for (pn, packet) in self
            .sent_packets
            .iter_mut()
            // BTreeMap iterates in order of ascending PN
            .take_while(|(&k, _)| Some(k) < largest_acked)
        {
            if packet.time_sent <= lost_deadline {
                qdebug!(
                    "lost={}, time sent {:?} is before lost_deadline {:?}",
                    pn,
                    packet.time_sent,
                    lost_deadline
                );
            } else if largest_acked >= Some(*pn + PACKET_THRESHOLD) {
                qdebug!(
                    "lost={}, is >= {} from largest acked {:?}",
                    pn,
                    PACKET_THRESHOLD,
                    largest_acked
                );
            } else {
                self.first_ooo_time = Some(packet.time_sent);
                // No more packets can be declared lost after this one.
                break;
            };

            if packet.declare_lost(now) {
                lost_pns.push(*pn);
            } else if packet.expired(now, loss_delay * 2) {
                really_lost_pns.push(*pn);
            }
        }

        for pn in really_lost_pns {
            self.remove_packet(pn).expect("lost packet missing");
        }

        lost_packets.extend(lost_pns.iter().map(|pn| self.sent_packets[pn].clone()));
    }
}

#[derive(Debug)]
pub(crate) struct LossRecoverySpaces {
    /// When we have all of the loss recovery spaces, this will use a separate
    /// allocation, but this is reduced once the handshake is done.
    spaces: SmallVec<[LossRecoverySpace; 1]>,
}

impl LossRecoverySpaces {
    pub fn new() -> Self {
        Self {
            spaces: smallvec![
                LossRecoverySpace::new(PNSpace::ApplicationData),
                LossRecoverySpace::new(PNSpace::Handshake),
                LossRecoverySpace::new(PNSpace::Initial),
            ],
        }
    }

    fn idx(space: PNSpace) -> usize {
        match space {
            PNSpace::ApplicationData => 0,
            PNSpace::Handshake => 1,
            PNSpace::Initial => 2,
        }
    }

    /// Drop a packet number space and return all the packets that were
    /// outstanding, so that those can be marked as lost.
    /// # Panics
    /// If the space has already been removed.
    pub fn drop_space(&mut self, space: PNSpace) -> Vec<SentPacket> {
        let sp = match space {
            PNSpace::Initial => self.spaces.pop(),
            PNSpace::Handshake => {
                let sp = self.spaces.pop();
                self.spaces.shrink_to_fit();
                sp
            }
            _ => panic!("discarding application space"),
        };
        let mut sp = sp.unwrap();
        assert_eq!(sp.space(), space, "dropping spaces out of order");
        sp.remove_ignored().collect()
    }

    pub fn get(&self, space: PNSpace) -> Option<&LossRecoverySpace> {
        self.spaces.get(Self::idx(space))
    }

    pub fn get_mut(&mut self, space: PNSpace) -> Option<&mut LossRecoverySpace> {
        self.spaces.get_mut(Self::idx(space))
    }
}

impl LossRecoverySpaces {
    fn iter(&self) -> impl Iterator<Item = &LossRecoverySpace> {
        self.spaces.iter()
    }
    fn iter_mut(&mut self) -> impl Iterator<Item = &mut LossRecoverySpace> {
        self.spaces.iter_mut()
    }
}

#[derive(Debug)]
struct PtoState {
    space: PNSpace,
    count: usize,
    packets: usize,
}

impl PtoState {
    pub fn new(space: PNSpace) -> Self {
        Self {
            space,
            count: 1,
            packets: PTO_PACKET_COUNT,
        }
    }

    pub fn pto(&mut self, space: PNSpace) {
        self.space = space;
        self.count += 1;
        self.packets = PTO_PACKET_COUNT;
    }

    pub fn count(&self) -> usize {
        self.count
    }

    /// Generate a sending profile, indicating what space it should be from.
    /// This takes a packet from the supply or returns an ack-only profile if it can't.
    pub fn send_profile(&mut self, mtu: usize) -> SendProfile {
        if self.packets > 0 {
            self.packets -= 1;
            SendProfile::new_pto(self.space, mtu)
        } else {
            SendProfile::new_limited(0)
        }
    }
}

#[derive(Debug)]
pub(crate) struct LossRecovery {
    pto_state: Option<PtoState>,
    rtt_vals: RttVals,
    cc: CongestionControl,

    spaces: LossRecoverySpaces,

    qlog: Rc<RefCell<Option<NeqoQlog>>>,
}

impl LossRecovery {
    pub fn new() -> Self {
        Self {
            rtt_vals: RttVals {
                min_rtt: Duration::from_secs(u64::max_value()),
                max_ack_delay: MAX_ACK_DELAY,
                latest_rtt: INITIAL_RTT,
                ..RttVals::default()
            },
            pto_state: None,
            cc: CongestionControl::default(),
            spaces: LossRecoverySpaces::new(),
            qlog: Rc::new(RefCell::new(None)),
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

    pub fn rtt(&self) -> Duration {
        self.rtt_vals.rtt()
    }

    pub fn set_initial_rtt(&mut self, value: Duration) {
        debug_assert!(self.rtt_vals.smoothed_rtt.is_none());
        self.rtt_vals.latest_rtt = value
    }

    pub fn cwnd_avail(&self) -> usize {
        self.cc.cwnd_avail()
    }

    pub fn largest_acknowledged_pn(&self, pn_space: PNSpace) -> Option<u64> {
        self.spaces.get(pn_space).and_then(|sp| sp.largest_acked)
    }

    pub fn pto(&self) -> Duration {
        self.rtt_vals.pto(PNSpace::ApplicationData)
    }

    pub fn set_qlog(&mut self, qlog: Rc<RefCell<Option<NeqoQlog>>>) {
        self.qlog = qlog.clone();
        self.cc.set_qlog(qlog)
    }

    pub fn drop_0rtt(&mut self) -> Vec<SentPacket> {
        // The largest acknowledged or loss_time should still be unset.
        // The client should not have received any ACK frames when it drops 0-RTT.
        assert!(self
            .spaces
            .get(PNSpace::ApplicationData)
            .unwrap()
            .largest_acked
            .is_none());
        self.spaces
            .get_mut(PNSpace::ApplicationData)
            .unwrap()
            .remove_ignored()
            .inspect(|p| self.cc.discard(&p))
            .collect()
    }

    pub fn on_packet_sent(&mut self, sent_packet: SentPacket) {
        let pn_space = PNSpace::from(sent_packet.pt);
        qdebug!([self], "packet {}-{} sent", pn_space, sent_packet.pn);
        let rtt = self.rtt();
        if let Some(space) = self.spaces.get_mut(pn_space) {
            self.cc.on_packet_sent(&sent_packet, rtt);
            space.on_packet_sent(sent_packet);
        } else {
            qinfo!(
                [self],
                "ignoring {}-{} from dropped space",
                pn_space,
                sent_packet.pn
            );
        }
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
            "ACK for {} - largest_acked={}.",
            pn_space,
            largest_acked
        );

        let space = self
            .spaces
            .get_mut(pn_space)
            .expect("ACK on discarded space");
        let (acked_packets, any_ack_eliciting) = space.remove_acked(acked_ranges);
        if acked_packets.is_empty() {
            // No new information.
            return (Vec::new(), Vec::new());
        }

        // Track largest PN acked per space
        let prev_largest_acked_sent_time = space.largest_acked_sent_time;
        if Some(largest_acked) > space.largest_acked {
            space.largest_acked = Some(largest_acked);

            // If the largest acknowledged is newly acked and any newly acked
            // packet was ack-eliciting, update the RTT. (-recovery 5.1)
            let largest_acked_pkt = acked_packets.last().expect("must be there");
            space.largest_acked_sent_time = Some(largest_acked_pkt.time_sent);
            if any_ack_eliciting {
                let latest_rtt = now - largest_acked_pkt.time_sent;
                self.rtt_vals.update_rtt(&self.qlog, latest_rtt, ack_delay);
            }
        }
        self.cc.on_packets_acked(&acked_packets);

        let loss_delay = self.loss_delay();
        let mut lost_packets = Vec::new();
        self.spaces.get_mut(pn_space).unwrap().detect_lost_packets(
            now,
            loss_delay,
            &mut lost_packets,
        );
        // TODO Process ECN information if present.
        self.cc.on_packets_lost(
            now,
            prev_largest_acked_sent_time,
            self.rtt_vals.pto(pn_space),
            &lost_packets,
        );

        self.pto_state = None;

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
        self.pto_state = None;
        let cc = &mut self.cc;
        self.spaces
            .iter_mut()
            .flat_map(LossRecoverySpace::remove_ignored)
            .inspect(|p| cc.discard(&p))
            .collect()
    }

    /// Discard state for a given packet number space.
    pub fn discard(&mut self, space: PNSpace) {
        qdebug!([self], "Reset loss recovery state for {}", space);
        // We just made progress, so discard PTO count.
        self.pto_state = None;
        for p in self.spaces.drop_space(space) {
            self.cc.discard(&p);
        }
    }

    /// Calculate when the next timeout is likely to be.  This is the earlier of the loss timer
    /// and the PTO timer; either or both might be disabled, so this can return `None`.
    pub fn next_timeout(&mut self) -> Option<Instant> {
        let loss_time = self.earliest_loss_time();
        let pto_time = self.earliest_pto();
        qtrace!(
            [self],
            "next_timeout loss={:?} pto={:?}",
            loss_time,
            pto_time
        );
        match (loss_time, pto_time) {
            (Some(loss_time), Some(pto_time)) => Some(min(loss_time, pto_time)),
            (Some(loss_time), None) => Some(loss_time),
            (None, Some(pto_time)) => Some(pto_time),
            _ => None,
        }
    }

    /// Find when the earliest sent packet should be considered lost.
    fn earliest_loss_time(&self) -> Option<Instant> {
        self.spaces
            .iter()
            .filter_map(LossRecoverySpace::loss_recovery_timer_start)
            .min()
            .map(|val| val + self.loss_delay())
    }

    /// Get the Base PTO value, which is derived only from the `RTT` and `RTTvar` values.
    /// This is for those cases where you need a value for the time you might sensibly
    /// wait for a packet to propagate.  Using `3*raw_pto()` is common.
    pub fn raw_pto(&self) -> Duration {
        self.rtt_vals.pto(PNSpace::ApplicationData)
    }

    // Calculate PTO time for the given space.
    fn pto_time(&self, pn_space: PNSpace) -> Option<Instant> {
        if let Some(space) = self.spaces.get(pn_space) {
            space.pto_base_time().map(|t| {
                t + self
                    .rtt_vals
                    .pto(pn_space)
                    .checked_mul(1 << self.pto_state.as_ref().map_or(0, |p| p.count))
                    .unwrap_or(LOCAL_IDLE_TIMEOUT * 2)
            })
        } else {
            None
        }
    }

    /// Find the earliest PTO time for all active packet number spaces.
    /// Ignore Application if either Initial or Handshake have an active PTO.
    fn earliest_pto(&self) -> Option<Instant> {
        self.pto_time(PNSpace::Initial)
            .iter()
            .chain(self.pto_time(PNSpace::Handshake).iter())
            .min()
            .cloned()
            .or_else(|| self.pto_time(PNSpace::ApplicationData))
    }

    /// This checks whether the PTO timer has fired and fires it if needed.
    /// When it has, mark a few packets as "lost" for the purposes of having frames
    /// regenerated in subsequent packets.  The packets aren't truly lost, so
    /// we have to clone the `SentPacket` instance.
    fn maybe_fire_pto(&mut self, now: Instant, lost: &mut Vec<SentPacket>) {
        let mut pto_space = None;
        for pn_space in PNSpace::iter() {
            // Skip early packet number spaces where the PTO timer hasn't fired.
            // Once the timer for one space has fired, include higher spaces. Declaring more
            // data as "lost" makes it more likely that PTO packets will include useful data.
            if pto_space.is_none() && self.pto_time(*pn_space).map_or(true, |t| t > now) {
                continue;
            }
            qdebug!([self], "PTO timer fired for {}", pn_space);
            if let Some(space) = self.spaces.get_mut(*pn_space) {
                pto_space = pto_space.or(Some(*pn_space));
                lost.extend(space.pto_packets(PTO_PACKET_COUNT).cloned());
            } else {
                qwarn!([self], "PTO timer for dropped space {}", pn_space);
            }
        }

        // This has to happen outside the loop. Increasing the PTO count here causes the
        // pto_time to increase which might cause PTO for later packet number spaces to not fire.
        if let Some(pn_space) = pto_space {
            if let Some(st) = &mut self.pto_state {
                st.pto(pn_space);
            } else {
                self.pto_state = Some(PtoState::new(pn_space));
            }
            qlog::metrics_updated(
                &mut self.qlog.borrow_mut(),
                &[QlogMetric::PtoCount(
                    self.pto_state.as_ref().unwrap().count(),
                )],
            );
        }
    }

    pub fn timeout(&mut self, now: Instant) -> Vec<SentPacket> {
        qtrace!([self], "timeout {:?}", now);

        let loss_delay = self.loss_delay();
        let mut lost_packets = Vec::new();
        for space in self.spaces.iter_mut() {
            let first = lost_packets.len(); // The first packet lost in this space.
            space.detect_lost_packets(now, loss_delay, &mut lost_packets);
            self.cc.on_packets_lost(
                now,
                space.largest_acked_sent_time,
                self.rtt_vals.pto(space.space()),
                &lost_packets[first..],
            )
        }

        self.maybe_fire_pto(now, &mut lost_packets);
        lost_packets
    }

    /// Start the packet pacer.
    pub fn start_pacer(&mut self, now: Instant) {
        self.cc.start_pacer(now);
    }

    /// Get the next time that a paced packet might be sent.
    pub fn next_paced(&self) -> Option<Instant> {
        self.cc.next_paced(self.rtt())
    }

    /// Check how packets should be sent, based on whether there is a PTO,
    /// what the current congestion window is, and what the pacer says.
    pub fn send_profile(&mut self, now: Instant, mtu: usize) -> SendProfile {
        qdebug!([self], "get send profile {:?}", now);
        if let Some(pto) = self.pto_state.as_mut() {
            pto.send_profile(mtu)
        } else {
            let cwnd = self.cwnd_avail();
            if cwnd > mtu {
                // More than an MTU available; we might need to pace.
                if self.next_paced().map_or(false, |t| t > now) {
                    SendProfile::new_paced()
                } else {
                    SendProfile::new_limited(mtu)
                }
            } else {
                SendProfile::new_limited(cwnd)
            }
        }
    }
}

impl ::std::fmt::Display for LossRecovery {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "LossRecovery")
    }
}

#[cfg(test)]
mod tests {
    use super::{LossRecovery, LossRecoverySpace, PNSpace, SentPacket};
    use crate::packet::PacketType;
    use std::convert::TryInto;
    use std::rc::Rc;
    use std::time::{Duration, Instant};
    use test_fixture::now;

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
        let est = |sp| {
            lr.spaces
                .get(sp)
                .and_then(LossRecoverySpace::loss_recovery_timer_start)
        };
        println!(
            "loss times: {:?} {:?} {:?}",
            est(PNSpace::Initial),
            est(PNSpace::Handshake),
            est(PNSpace::ApplicationData),
        );
        assert_eq!(est(PNSpace::Initial), initial, "Initial earliest sent time");
        assert_eq!(
            est(PNSpace::Handshake),
            handshake,
            "Handshake earliest sent time"
        );
        assert_eq!(
            est(PNSpace::ApplicationData),
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
        now() + (PACING * pn.try_into().unwrap())
    }

    fn pace(lr: &mut LossRecovery, count: u64) {
        for pn in 0..count {
            lr.on_packet_sent(SentPacket::new(
                PacketType::Short,
                pn,
                pn_time(pn),
                true,
                Rc::default(),
                ON_SENT_SIZE,
                true,
            ));
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
        lr.start_pacer(now());
        pace(&mut lr, 1);
        let rtt = ms!(100);
        ack(&mut lr, 0, rtt);
        assert_rtts(&lr, rtt, rtt, rtt / 2, rtt);
        assert_no_sent_times(&lr);
    }

    /// An initial RTT for using with `setup_lr`.
    const INITIAL_RTT: Duration = ms!(80);
    const INITIAL_RTTVAR: Duration = ms!(40);

    /// Send `n` packets (using PACING), then acknowledge the first.
    fn setup_lr(n: u64) -> LossRecovery {
        let mut lr = LossRecovery::new();
        lr.start_pacer(now());
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
        lr.start_pacer(now());
        // Create a single packet gap, and have pn 0 time out.
        // This can't use the default pacing, which is too tight.
        // So send two packets with 1/4 RTT between them.  Acknowledge pn 1 after 1 RTT.
        // pn 0 should then be marked lost because it is then outstanding for 5RTT/4
        // the loss time for packets is 9RTT/8.
        lr.on_packet_sent(SentPacket::new(
            PacketType::Short,
            0,
            pn_time(0),
            true,
            Rc::default(),
            ON_SENT_SIZE,
            true,
        ));
        lr.on_packet_sent(SentPacket::new(
            PacketType::Short,
            1,
            pn_time(0) + INITIAL_RTT / 4,
            true,
            Rc::default(),
            ON_SENT_SIZE,
            true,
        ));
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

        // We want to declare PN 2 as acknowledged before we declare PN 1 as lost.
        // For this to work, we need PACING above to be less than 1/8 of an RTT.
        let pn1_sent_time = pn_time(1);
        let pn1_loss_time = pn1_sent_time + (INITIAL_RTT * 9 / 8);
        let pn2_ack_time = pn_time(2) + INITIAL_RTT;
        assert!(pn1_loss_time > pn2_ack_time);

        let (_, lost) = lr.on_ack_received(
            PNSpace::ApplicationData,
            2,
            vec![(2, 2)],
            ACK_DELAY,
            pn2_ack_time,
        );
        assert!(lost.is_empty());
        // Run the timeout function here to force time-based loss recovery to be enabled.
        let lost = lr.timeout(pn2_ack_time);
        assert!(lost.is_empty());
        assert_sent_times(&lr, None, None, Some(pn1_sent_time));

        // After time elapses, pn 1 is marked lost.
        let callback_time = lr.next_timeout();
        assert_eq!(callback_time, Some(pn1_loss_time));
        let packets = lr.timeout(pn1_loss_time);
        assert_eq!(packets.len(), 1);
        // Checking for expiration with zero delay lets us check the loss time.
        assert!(packets[0].expired(pn1_loss_time, Duration::from_secs(0)));
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

    #[test]
    #[should_panic(expected = "discarding application space")]
    fn drop_app() {
        let mut lr = LossRecovery::new();
        lr.discard(PNSpace::ApplicationData);
    }

    #[test]
    #[should_panic(expected = "dropping spaces out of order")]
    fn drop_out_of_order() {
        let mut lr = LossRecovery::new();
        lr.discard(PNSpace::Handshake);
    }

    #[test]
    #[should_panic(expected = "ACK on discarded space")]
    fn ack_after_drop() {
        let mut lr = LossRecovery::new();
        lr.start_pacer(now());
        lr.discard(PNSpace::Initial);
        lr.on_ack_received(
            PNSpace::Initial,
            0,
            vec![],
            Duration::from_millis(0),
            pn_time(0),
        );
    }

    #[test]
    fn drop_spaces() {
        let mut lr = LossRecovery::new();
        lr.start_pacer(now());
        lr.on_packet_sent(SentPacket::new(
            PacketType::Initial,
            0,
            pn_time(0),
            true,
            Rc::default(),
            ON_SENT_SIZE,
            true,
        ));
        lr.on_packet_sent(SentPacket::new(
            PacketType::Handshake,
            0,
            pn_time(1),
            true,
            Rc::default(),
            ON_SENT_SIZE,
            true,
        ));
        lr.on_packet_sent(SentPacket::new(
            PacketType::Short,
            0,
            pn_time(2),
            true,
            Rc::default(),
            ON_SENT_SIZE,
            true,
        ));

        // Now put all spaces on the LR timer so we can see them.
        for sp in &[
            PacketType::Initial,
            PacketType::Handshake,
            PacketType::Short,
        ] {
            let sent_pkt =
                SentPacket::new(*sp, 1, pn_time(3), true, Rc::default(), ON_SENT_SIZE, true);
            let pn_space = PNSpace::from(sent_pkt.pt);
            lr.on_packet_sent(sent_pkt);
            lr.on_ack_received(
                pn_space,
                1,
                vec![(1, 1)],
                Duration::from_secs(0),
                pn_time(3),
            );
            let mut lost = Vec::new();
            lr.spaces.get_mut(pn_space).unwrap().detect_lost_packets(
                pn_time(3),
                INITIAL_RTT,
                &mut lost,
            );
            assert!(lost.is_empty());
        }

        lr.discard(PNSpace::Initial);
        assert_sent_times(&lr, None, Some(pn_time(1)), Some(pn_time(2)));

        lr.discard(PNSpace::Handshake);
        assert_sent_times(&lr, None, None, Some(pn_time(2)));

        // There are cases where we send a packet that is not subsequently tracked.
        // So check that this works.
        lr.on_packet_sent(SentPacket::new(
            PacketType::Initial,
            0,
            pn_time(3),
            true,
            Rc::default(),
            ON_SENT_SIZE,
            true,
        ));
        assert_sent_times(&lr, None, None, Some(pn_time(2)));
    }
}
