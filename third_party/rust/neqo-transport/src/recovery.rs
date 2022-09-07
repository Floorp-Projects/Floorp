// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of sent packets and detecting their loss.

#![deny(clippy::pedantic)]

use std::cmp::{max, min};
use std::collections::BTreeMap;
use std::convert::TryFrom;
use std::mem;
use std::ops::RangeInclusive;
use std::time::{Duration, Instant};

use smallvec::{smallvec, SmallVec};

use neqo_common::{qdebug, qinfo, qlog::NeqoQlog, qtrace, qwarn};

use crate::ackrate::AckRate;
use crate::cid::ConnectionIdEntry;
use crate::crypto::CryptoRecoveryToken;
use crate::packet::PacketNumber;
use crate::path::{Path, PathRef};
use crate::qlog::{self, QlogMetric};
use crate::quic_datagrams::DatagramTracking;
use crate::rtt::RttEstimate;
use crate::send_stream::SendStreamRecoveryToken;
use crate::stats::{Stats, StatsCell};
use crate::stream_id::{StreamId, StreamType};
use crate::tracking::{AckToken, PacketNumberSpace, PacketNumberSpaceSet, SentPacket};

pub(crate) const PACKET_THRESHOLD: u64 = 3;
/// `ACK_ONLY_SIZE_LIMIT` is the minimum size of the congestion window.
/// If the congestion window is this small, we will only send ACK frames.
pub(crate) const ACK_ONLY_SIZE_LIMIT: usize = 256;
/// The number of packets we send on a PTO.
/// And the number to declare lost when the PTO timer is hit.
pub const PTO_PACKET_COUNT: usize = 2;
/// The preferred limit on the number of packets that are tracked.
/// If we exceed this number, we start sending `PING` frames sooner to
/// force the peer to acknowledge some of them.
pub(crate) const MAX_OUTSTANDING_UNACK: usize = 200;
/// Disable PING until this many packets are outstanding.
pub(crate) const MIN_OUTSTANDING_UNACK: usize = 16;
/// The scale we use for the fast PTO feature.
pub const FAST_PTO_SCALE: u8 = 100;

#[derive(Debug, Clone)]
#[allow(clippy::module_name_repetitions)]
pub enum StreamRecoveryToken {
    Stream(SendStreamRecoveryToken),
    ResetStream {
        stream_id: StreamId,
    },
    StopSending {
        stream_id: StreamId,
    },

    MaxData(u64),
    DataBlocked(u64),

    MaxStreamData {
        stream_id: StreamId,
        max_data: u64,
    },
    StreamDataBlocked {
        stream_id: StreamId,
        limit: u64,
    },

    MaxStreams {
        stream_type: StreamType,
        max_streams: u64,
    },
    StreamsBlocked {
        stream_type: StreamType,
        limit: u64,
    },
}

#[derive(Debug, Clone)]
#[allow(clippy::module_name_repetitions)]
pub enum RecoveryToken {
    Stream(StreamRecoveryToken),
    Ack(AckToken),
    Crypto(CryptoRecoveryToken),
    HandshakeDone,
    KeepAlive, // Special PING.
    NewToken(usize),
    NewConnectionId(ConnectionIdEntry<[u8; 16]>),
    RetireConnectionId(u64),
    AckFrequency(AckRate),
    Datagram(DatagramTracking),
}

/// `SendProfile` tells a sender how to send packets.
#[derive(Debug)]
pub struct SendProfile {
    /// The limit on the size of the packet.
    limit: usize,
    /// Whether this is a PTO, and what space the PTO is for.
    pto: Option<PacketNumberSpace>,
    /// What spaces should be probed.
    probe: PacketNumberSpaceSet,
    /// Whether pacing is active.
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
            probe: PacketNumberSpaceSet::default(),
            paced: false,
        }
    }

    pub fn new_paced() -> Self {
        // When pacing, we still allow ACK frames to be sent.
        Self {
            limit: ACK_ONLY_SIZE_LIMIT - 1,
            pto: None,
            probe: PacketNumberSpaceSet::default(),
            paced: true,
        }
    }

    pub fn new_pto(pn_space: PacketNumberSpace, mtu: usize, probe: PacketNumberSpaceSet) -> Self {
        debug_assert!(mtu > ACK_ONLY_SIZE_LIMIT);
        debug_assert!(probe[pn_space]);
        Self {
            limit: mtu,
            pto: Some(pn_space),
            probe,
            paced: false,
        }
    }

    /// Whether probing this space is helpful.  This isn't necessarily the space
    /// that caused the timer to pop, but it is helpful to send a PING in a space
    /// that has the PTO timer armed.
    pub fn should_probe(&self, space: PacketNumberSpace) -> bool {
        self.probe[space]
    }

    /// Determine whether an ACK-only packet should be sent for the given packet
    /// number space.
    /// Send only ACKs either: when the space available is too small, or when a PTO
    /// exists for a later packet number space (which should get the most space).
    pub fn ack_only(&self, space: PacketNumberSpace) -> bool {
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
    space: PacketNumberSpace,
    largest_acked: Option<PacketNumber>,
    largest_acked_sent_time: Option<Instant>,
    /// The time used to calculate the PTO timer for this space.
    /// This is the time that the last ACK-eliciting packet in this space
    /// was sent.  This might be the time that a probe was sent.
    last_ack_eliciting: Option<Instant>,
    /// The number of outstanding packets in this space that are in flight.
    /// This might be less than the number of ACK-eliciting packets,
    /// because PTO packets don't count.
    in_flight_outstanding: usize,
    sent_packets: BTreeMap<u64, SentPacket>,
    /// The time that the first out-of-order packet was sent.
    /// This is `None` if there were no out-of-order packets detected.
    /// When set to `Some(T)`, time-based loss detection should be enabled.
    first_ooo_time: Option<Instant>,
}

impl LossRecoverySpace {
    pub fn new(space: PacketNumberSpace) -> Self {
        Self {
            space,
            largest_acked: None,
            largest_acked_sent_time: None,
            last_ack_eliciting: None,
            in_flight_outstanding: 0,
            sent_packets: BTreeMap::default(),
            first_ooo_time: None,
        }
    }

    #[must_use]
    pub fn space(&self) -> PacketNumberSpace {
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
            debug_assert!(self.last_ack_eliciting.is_some());
            self.last_ack_eliciting
        } else if self.space == PacketNumberSpace::ApplicationData {
            None
        } else {
            // Nasty special case to prevent handshake deadlocks.
            // A client needs to keep the PTO timer armed to prevent a stall
            // of the handshake.  Technically, this has to stop once we receive
            // an ACK of Handshake or 1-RTT, or when we receive HANDSHAKE_DONE,
            // but a few extra probes won't hurt.
            // It only means that we fail anti-amplification tests.
            // A server shouldn't arm its PTO timer this way. The server sends
            // ack-eliciting, in-flight packets immediately so this only
            // happens when the server has nothing outstanding.  If we had
            // client authentication, this might cause some extra probes,
            // but they would be harmless anyway.
            self.last_ack_eliciting
        }
    }

    pub fn on_packet_sent(&mut self, sent_packet: SentPacket) {
        if sent_packet.ack_eliciting() {
            self.last_ack_eliciting = Some(sent_packet.time_sent);
            self.in_flight_outstanding += 1;
        } else if self.space != PacketNumberSpace::ApplicationData
            && self.last_ack_eliciting.is_none()
        {
            // For Initial and Handshake spaces, make sure that we have a PTO baseline
            // always. See `LossRecoverySpace::pto_base_time()` for details.
            self.last_ack_eliciting = Some(sent_packet.time_sent);
        }
        self.sent_packets.insert(sent_packet.pn, sent_packet);
    }

    /// If we are only sending ACK frames, send a PING frame after 2 PTOs so that
    /// the peer sends an ACK frame.  If we have received lots of packets and no ACK,
    /// send a PING frame after 1 PTO.  Note that this can't be within a PTO, or
    /// we would risk setting up a feedback loop; having this many packets
    /// outstanding can be normal and we don't want to PING too often.
    pub fn should_probe(&self, pto: Duration, now: Instant) -> bool {
        let n_pto = if self.sent_packets.len() >= MAX_OUTSTANDING_UNACK {
            1
        } else if self.sent_packets.len() >= MIN_OUTSTANDING_UNACK {
            2
        } else {
            return false;
        };
        self.last_ack_eliciting
            .map_or(false, |t| now > t + (pto * n_pto))
    }

    fn remove_packet(&mut self, p: &SentPacket) {
        if p.ack_eliciting() {
            debug_assert!(self.in_flight_outstanding > 0);
            self.in_flight_outstanding -= 1;
            if self.in_flight_outstanding == 0 {
                qtrace!("remove_packet outstanding == 0 for space {}", self.space);
            }
        }
    }

    /// Remove all acknowledged packets.
    /// Returns all the acknowledged packets, with the largest packet number first.
    /// ...and a boolean indicating if any of those packets were ack-eliciting.
    /// This operates more efficiently because it assumes that the input is sorted
    /// in the order that an ACK frame is (from the top).
    fn remove_acked<R>(&mut self, acked_ranges: R, stats: &mut Stats) -> (Vec<SentPacket>, bool)
    where
        R: IntoIterator<Item = RangeInclusive<u64>>,
        R::IntoIter: ExactSizeIterator,
    {
        let acked_ranges = acked_ranges.into_iter();
        let mut keep = Vec::with_capacity(acked_ranges.len());

        let mut acked = Vec::new();
        let mut eliciting = false;
        for range in acked_ranges {
            let first_keep = *range.end() + 1;
            if let Some((&first, _)) = self.sent_packets.range(range).next() {
                let mut tail = self.sent_packets.split_off(&first);
                if let Some((&next, _)) = tail.range(first_keep..).next() {
                    keep.push(tail.split_off(&next));
                }
                for (_, p) in tail.into_iter().rev() {
                    self.remove_packet(&p);
                    eliciting |= p.ack_eliciting();
                    if p.lost() {
                        stats.late_ack += 1;
                    }
                    if p.pto_fired() {
                        stats.pto_ack += 1;
                    }
                    acked.push(p);
                }
            }
        }

        for mut k in keep.into_iter().rev() {
            self.sent_packets.append(&mut k);
        }

        (acked, eliciting)
    }

    /// Remove all tracked packets from the space.
    /// This is called by a client when 0-RTT packets are dropped, when a Retry is received
    /// and when keys are dropped.
    fn remove_ignored(&mut self) -> impl Iterator<Item = SentPacket> {
        self.in_flight_outstanding = 0;
        mem::take(&mut self.sent_packets)
            .into_iter()
            .map(|(_, v)| v)
    }

    /// Remove the primary path marking on any packets this is tracking.
    fn migrate(&mut self) {
        for pkt in self.sent_packets.values_mut() {
            pkt.clear_primary_path();
        }
    }

    /// Remove old packets that we've been tracking in case they get acknowledged.
    /// We try to keep these around until a probe is sent for them, so it is
    /// important that `cd` is set to at least the current PTO time; otherwise we
    /// might remove all in-flight packets and stop sending probes.
    #[allow(clippy::option_if_let_else)] // Hard enough to read as-is.
    fn remove_old_lost(&mut self, now: Instant, cd: Duration) {
        let mut it = self.sent_packets.iter();
        // If the first item is not expired, do nothing.
        if it.next().map_or(false, |(_, p)| p.expired(now, cd)) {
            // Find the index of the first unexpired packet.
            let to_remove = if let Some(first_keep) =
                it.find_map(|(i, p)| if p.expired(now, cd) { None } else { Some(*i) })
            {
                // Some packets haven't expired, so keep those.
                let keep = self.sent_packets.split_off(&first_keep);
                mem::replace(&mut self.sent_packets, keep)
            } else {
                // All packets are expired.
                mem::take(&mut self.sent_packets)
            };
            for (_, p) in to_remove {
                self.remove_packet(&p);
            }
        }
    }

    /// Detect lost packets.
    /// `loss_delay` is the time we will wait before declaring something lost.
    /// `cleanup_delay` is the time we will wait before cleaning up a lost packet.
    pub fn detect_lost_packets(
        &mut self,
        now: Instant,
        loss_delay: Duration,
        cleanup_delay: Duration,
        lost_packets: &mut Vec<SentPacket>,
    ) {
        // Housekeeping.
        self.remove_old_lost(now, cleanup_delay);

        qtrace!(
            "detect lost {}: now={:?} delay={:?}",
            self.space,
            now,
            loss_delay,
        );
        self.first_ooo_time = None;

        let largest_acked = self.largest_acked;

        // Lost for retrans/CC purposes
        let mut lost_pns = SmallVec::<[_; 8]>::new();

        for (pn, packet) in self
            .sent_packets
            .iter_mut()
            // BTreeMap iterates in order of ascending PN
            .take_while(|(&k, _)| Some(k) < largest_acked)
        {
            // Packets sent before now - loss_delay are deemed lost.
            if packet.time_sent + loss_delay <= now {
                qtrace!(
                    "lost={}, time sent {:?} is before lost_delay {:?}",
                    pn,
                    packet.time_sent,
                    loss_delay
                );
            } else if largest_acked >= Some(*pn + PACKET_THRESHOLD) {
                qtrace!(
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
            }
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
    fn idx(space: PacketNumberSpace) -> usize {
        match space {
            PacketNumberSpace::ApplicationData => 0,
            PacketNumberSpace::Handshake => 1,
            PacketNumberSpace::Initial => 2,
        }
    }

    /// Drop a packet number space and return all the packets that were
    /// outstanding, so that those can be marked as lost.
    /// # Panics
    /// If the space has already been removed.
    pub fn drop_space(&mut self, space: PacketNumberSpace) -> impl IntoIterator<Item = SentPacket> {
        let sp = match space {
            PacketNumberSpace::Initial => self.spaces.pop(),
            PacketNumberSpace::Handshake => {
                let sp = self.spaces.pop();
                self.spaces.shrink_to_fit();
                sp
            }
            PacketNumberSpace::ApplicationData => panic!("discarding application space"),
        };
        let mut sp = sp.unwrap();
        assert_eq!(sp.space(), space, "dropping spaces out of order");
        sp.remove_ignored()
    }

    pub fn get(&self, space: PacketNumberSpace) -> Option<&LossRecoverySpace> {
        self.spaces.get(Self::idx(space))
    }

    pub fn get_mut(&mut self, space: PacketNumberSpace) -> Option<&mut LossRecoverySpace> {
        self.spaces.get_mut(Self::idx(space))
    }

    fn iter(&self) -> impl Iterator<Item = &LossRecoverySpace> {
        self.spaces.iter()
    }

    fn iter_mut(&mut self) -> impl Iterator<Item = &mut LossRecoverySpace> {
        self.spaces.iter_mut()
    }
}

impl Default for LossRecoverySpaces {
    fn default() -> Self {
        Self {
            spaces: smallvec![
                LossRecoverySpace::new(PacketNumberSpace::ApplicationData),
                LossRecoverySpace::new(PacketNumberSpace::Handshake),
                LossRecoverySpace::new(PacketNumberSpace::Initial),
            ],
        }
    }
}

#[derive(Debug)]
struct PtoState {
    /// The packet number space that caused the PTO to fire.
    space: PacketNumberSpace,
    /// The number of probes that we have sent.
    count: usize,
    packets: usize,
    /// The complete set of packet number spaces that can have probes sent.
    probe: PacketNumberSpaceSet,
}

impl PtoState {
    pub fn new(space: PacketNumberSpace, probe: PacketNumberSpaceSet) -> Self {
        debug_assert!(probe[space]);
        Self {
            space,
            count: 1,
            packets: PTO_PACKET_COUNT,
            probe,
        }
    }

    pub fn pto(&mut self, space: PacketNumberSpace, probe: PacketNumberSpaceSet) {
        debug_assert!(probe[space]);
        self.space = space;
        self.count += 1;
        self.packets = PTO_PACKET_COUNT;
        self.probe = probe;
    }

    pub fn count(&self) -> usize {
        self.count
    }

    pub fn count_pto(&self, stats: &mut Stats) {
        stats.add_pto_count(self.count);
    }

    /// Generate a sending profile, indicating what space it should be from.
    /// This takes a packet from the supply if one remains, or returns `None`.
    pub fn send_profile(&mut self, mtu: usize) -> Option<SendProfile> {
        if self.packets > 0 {
            // This is a PTO, so ignore the limit.
            self.packets -= 1;
            Some(SendProfile::new_pto(self.space, mtu, self.probe))
        } else {
            None
        }
    }
}

#[derive(Debug)]
pub(crate) struct LossRecovery {
    /// When the handshake was confirmed, if it has been.
    confirmed_time: Option<Instant>,
    pto_state: Option<PtoState>,
    spaces: LossRecoverySpaces,
    qlog: NeqoQlog,
    stats: StatsCell,
    /// The factor by which the PTO period is reduced.
    /// This enables faster probing at a cost in additional lost packets.
    fast_pto: u8,
}

impl LossRecovery {
    pub fn new(stats: StatsCell, fast_pto: u8) -> Self {
        Self {
            confirmed_time: None,
            pto_state: None,
            spaces: LossRecoverySpaces::default(),
            qlog: NeqoQlog::default(),
            stats,
            fast_pto,
        }
    }

    pub fn largest_acknowledged_pn(&self, pn_space: PacketNumberSpace) -> Option<PacketNumber> {
        self.spaces.get(pn_space).and_then(|sp| sp.largest_acked)
    }

    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.qlog = qlog;
    }

    pub fn drop_0rtt(&mut self, primary_path: &PathRef, now: Instant) -> Vec<SentPacket> {
        // The largest acknowledged or loss_time should still be unset.
        // The client should not have received any ACK frames when it drops 0-RTT.
        assert!(self
            .spaces
            .get(PacketNumberSpace::ApplicationData)
            .unwrap()
            .largest_acked
            .is_none());
        let mut dropped = self
            .spaces
            .get_mut(PacketNumberSpace::ApplicationData)
            .unwrap()
            .remove_ignored()
            .collect::<Vec<_>>();
        let mut path = primary_path.borrow_mut();
        for p in &mut dropped {
            path.discard_packet(p, now);
        }
        dropped
    }

    pub fn on_packet_sent(&mut self, path: &PathRef, mut sent_packet: SentPacket) {
        let pn_space = PacketNumberSpace::from(sent_packet.pt);
        qdebug!([self], "packet {}-{} sent", pn_space, sent_packet.pn);
        if let Some(space) = self.spaces.get_mut(pn_space) {
            path.borrow_mut().packet_sent(&mut sent_packet);
            space.on_packet_sent(sent_packet);
        } else {
            qwarn!(
                [self],
                "ignoring {}-{} from dropped space",
                pn_space,
                sent_packet.pn
            );
        }
    }

    pub fn should_probe(&self, pto: Duration, now: Instant) -> bool {
        self.spaces
            .get(PacketNumberSpace::ApplicationData)
            .unwrap()
            .should_probe(pto, now)
    }

    /// Record an RTT sample.
    fn rtt_sample(
        &mut self,
        rtt: &mut RttEstimate,
        send_time: Instant,
        now: Instant,
        ack_delay: Duration,
    ) {
        let confirmed = self.confirmed_time.map_or(false, |t| t < send_time);
        if let Some(sample) = now.checked_duration_since(send_time) {
            rtt.update(&mut self.qlog, sample, ack_delay, confirmed, now);
        }
    }

    /// Returns (acked packets, lost packets)
    pub fn on_ack_received<R>(
        &mut self,
        primary_path: &PathRef,
        pn_space: PacketNumberSpace,
        largest_acked: u64,
        acked_ranges: R,
        ack_delay: Duration,
        now: Instant,
    ) -> (Vec<SentPacket>, Vec<SentPacket>)
    where
        R: IntoIterator<Item = RangeInclusive<u64>>,
        R::IntoIter: ExactSizeIterator,
    {
        qdebug!(
            [self],
            "ACK for {} - largest_acked={}.",
            pn_space,
            largest_acked
        );

        let space = self.spaces.get_mut(pn_space);
        let space = if let Some(sp) = space {
            sp
        } else {
            qinfo!("ACK on discarded space");
            return (Vec::new(), Vec::new());
        };

        let (acked_packets, any_ack_eliciting) =
            space.remove_acked(acked_ranges, &mut *self.stats.borrow_mut());
        if acked_packets.is_empty() {
            // No new information.
            return (Vec::new(), Vec::new());
        }

        // Track largest PN acked per space
        let prev_largest_acked = space.largest_acked_sent_time;
        if Some(largest_acked) > space.largest_acked {
            space.largest_acked = Some(largest_acked);

            // If the largest acknowledged is newly acked and any newly acked
            // packet was ack-eliciting, update the RTT. (-recovery 5.1)
            let largest_acked_pkt = acked_packets.first().expect("must be there");
            space.largest_acked_sent_time = Some(largest_acked_pkt.time_sent);
            if any_ack_eliciting && largest_acked_pkt.on_primary_path() {
                self.rtt_sample(
                    primary_path.borrow_mut().rtt_mut(),
                    largest_acked_pkt.time_sent,
                    now,
                    ack_delay,
                );
            }
        }

        // Perform loss detection.
        // PTO is used to remove lost packets from in-flight accounting.
        // We need to ensure that we have sent any PTO probes before they are removed
        // as we rely on the count of in-flight packets to determine whether to send
        // another probe.  Removing them too soon would result in not sending on PTO.
        let loss_delay = primary_path.borrow().rtt().loss_delay();
        let cleanup_delay = self.pto_period(primary_path.borrow().rtt(), pn_space);
        let mut lost = Vec::new();
        self.spaces.get_mut(pn_space).unwrap().detect_lost_packets(
            now,
            loss_delay,
            cleanup_delay,
            &mut lost,
        );
        self.stats.borrow_mut().lost += lost.len();

        // Tell the congestion controller about any lost packets.
        // The PTO for congestion control is the raw number, without exponential
        // backoff, so that we can determine persistent congestion.
        primary_path
            .borrow_mut()
            .on_packets_lost(prev_largest_acked, pn_space, &lost);

        // This must happen after on_packets_lost. If in recovery, this could
        // take us out, and then lost packets will start a new recovery period
        // when it shouldn't.
        primary_path
            .borrow_mut()
            .on_packets_acked(&acked_packets, now);

        self.pto_state = None;

        (acked_packets, lost)
    }

    /// When receiving a retry, get all the sent packets so that they can be flushed.
    /// We also need to pretend that they never happened for the purposes of congestion control.
    pub fn retry(&mut self, primary_path: &PathRef, now: Instant) -> Vec<SentPacket> {
        self.pto_state = None;
        let mut dropped = self
            .spaces
            .iter_mut()
            .flat_map(LossRecoverySpace::remove_ignored)
            .collect::<Vec<_>>();
        let mut path = primary_path.borrow_mut();
        for p in &mut dropped {
            path.discard_packet(p, now);
        }
        dropped
    }

    fn confirmed(&mut self, rtt: &RttEstimate, now: Instant) {
        debug_assert!(self.confirmed_time.is_none());
        self.confirmed_time = Some(now);
        // Up until now, the ApplicationData space has been ignored for PTO.
        // So maybe fire a PTO.
        if let Some(pto) = self.pto_time(rtt, PacketNumberSpace::ApplicationData) {
            if pto < now {
                let probes = PacketNumberSpaceSet::from(&[PacketNumberSpace::ApplicationData]);
                self.fire_pto(PacketNumberSpace::ApplicationData, probes);
            }
        }
    }

    /// This function is called when the connection migrates.
    /// It marks all packets that are outstanding as having being sent on a non-primary path.
    /// This way failure to deliver on the old path doesn't count against the congestion
    /// control state on the new path and the RTT measurements don't apply either.
    pub fn migrate(&mut self) {
        for space in self.spaces.iter_mut() {
            space.migrate();
        }
    }

    /// Discard state for a given packet number space.
    pub fn discard(&mut self, primary_path: &PathRef, space: PacketNumberSpace, now: Instant) {
        qdebug!([self], "Reset loss recovery state for {}", space);
        let mut path = primary_path.borrow_mut();
        for p in self.spaces.drop_space(space) {
            path.discard_packet(&p, now);
        }

        // We just made progress, so discard PTO count.
        // The spec says that clients should not do this until confirming that
        // the server has completed address validation, but ignore that.
        self.pto_state = None;

        if space == PacketNumberSpace::Handshake {
            self.confirmed(path.rtt(), now);
        }
    }

    /// Calculate when the next timeout is likely to be.  This is the earlier of the loss timer
    /// and the PTO timer; either or both might be disabled, so this can return `None`.
    pub fn next_timeout(&mut self, rtt: &RttEstimate) -> Option<Instant> {
        let loss_time = self.earliest_loss_time(rtt);
        let pto_time = self.earliest_pto(rtt);
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
    fn earliest_loss_time(&self, rtt: &RttEstimate) -> Option<Instant> {
        self.spaces
            .iter()
            .filter_map(LossRecoverySpace::loss_recovery_timer_start)
            .min()
            .map(|val| val + rtt.loss_delay())
    }

    /// Simple wrapper for the PTO calculation that avoids borrow check rules.
    fn pto_period_inner(
        rtt: &RttEstimate,
        pto_state: Option<&PtoState>,
        pn_space: PacketNumberSpace,
        fast_pto: u8,
    ) -> Duration {
        // This is a complicated (but safe) way of calculating:
        //   base_pto * F * 2^pto_count
        // where F = fast_pto / FAST_PTO_SCALE (== 1 by default)
        let pto_count = pto_state.map_or(0, |p| u32::try_from(p.count).unwrap_or(0));
        rtt.pto(pn_space)
            .checked_mul(
                u32::from(fast_pto)
                    .checked_shl(pto_count)
                    .unwrap_or(u32::MAX),
            )
            .map_or(Duration::from_secs(3600), |p| p / u32::from(FAST_PTO_SCALE))
    }

    /// Get the current PTO period for the given packet number space.
    /// Unlike calling `RttEstimate::pto` directly, this includes exponential backoff.
    fn pto_period(&self, rtt: &RttEstimate, pn_space: PacketNumberSpace) -> Duration {
        Self::pto_period_inner(rtt, self.pto_state.as_ref(), pn_space, self.fast_pto)
    }

    // Calculate PTO time for the given space.
    fn pto_time(&self, rtt: &RttEstimate, pn_space: PacketNumberSpace) -> Option<Instant> {
        if self.confirmed_time.is_none() && pn_space == PacketNumberSpace::ApplicationData {
            None
        } else {
            self.spaces.get(pn_space).and_then(|space| {
                space
                    .pto_base_time()
                    .map(|t| t + self.pto_period(rtt, pn_space))
            })
        }
    }

    /// Find the earliest PTO time for all active packet number spaces.
    /// Ignore Application if either Initial or Handshake have an active PTO.
    fn earliest_pto(&self, rtt: &RttEstimate) -> Option<Instant> {
        if self.confirmed_time.is_some() {
            self.pto_time(rtt, PacketNumberSpace::ApplicationData)
        } else {
            self.pto_time(rtt, PacketNumberSpace::Initial)
                .iter()
                .chain(self.pto_time(rtt, PacketNumberSpace::Handshake).iter())
                .min()
                .copied()
        }
    }

    fn fire_pto(&mut self, pn_space: PacketNumberSpace, allow_probes: PacketNumberSpaceSet) {
        if let Some(st) = &mut self.pto_state {
            st.pto(pn_space, allow_probes);
        } else {
            self.pto_state = Some(PtoState::new(pn_space, allow_probes));
        }

        self.pto_state
            .as_mut()
            .unwrap()
            .count_pto(&mut *self.stats.borrow_mut());

        qlog::metrics_updated(
            &mut self.qlog,
            &[QlogMetric::PtoCount(
                self.pto_state.as_ref().unwrap().count(),
            )],
        );
    }

    /// This checks whether the PTO timer has fired and fires it if needed.
    /// When it has, mark a few packets as "lost" for the purposes of having frames
    /// regenerated in subsequent packets.  The packets aren't truly lost, so
    /// we have to clone the `SentPacket` instance.
    fn maybe_fire_pto(&mut self, rtt: &RttEstimate, now: Instant, lost: &mut Vec<SentPacket>) {
        let mut pto_space = None;
        // The spaces in which we will allow probing.
        let mut allow_probes = PacketNumberSpaceSet::default();
        for pn_space in PacketNumberSpace::iter() {
            if let Some(t) = self.pto_time(rtt, *pn_space) {
                allow_probes[*pn_space] = true;
                if t <= now {
                    qdebug!([self], "PTO timer fired for {}", pn_space);
                    let space = self.spaces.get_mut(*pn_space).unwrap();
                    lost.extend(space.pto_packets(PTO_PACKET_COUNT).cloned());

                    pto_space = pto_space.or(Some(*pn_space));
                }
            }
        }

        // This has to happen outside the loop. Increasing the PTO count here causes the
        // pto_time to increase which might cause PTO for later packet number spaces to not fire.
        if let Some(pn_space) = pto_space {
            qtrace!([self], "PTO {}, probing {:?}", pn_space, allow_probes);
            self.fire_pto(pn_space, allow_probes);
        }
    }

    pub fn timeout(&mut self, primary_path: &PathRef, now: Instant) -> Vec<SentPacket> {
        qtrace!([self], "timeout {:?}", now);

        let loss_delay = primary_path.borrow().rtt().loss_delay();

        let mut lost_packets = Vec::new();
        for space in self.spaces.iter_mut() {
            let first = lost_packets.len(); // The first packet lost in this space.
            let pto = Self::pto_period_inner(
                primary_path.borrow().rtt(),
                self.pto_state.as_ref(),
                space.space(),
                self.fast_pto,
            );
            space.detect_lost_packets(now, loss_delay, pto, &mut lost_packets);

            primary_path.borrow_mut().on_packets_lost(
                space.largest_acked_sent_time,
                space.space(),
                &lost_packets[first..],
            );
        }
        self.stats.borrow_mut().lost += lost_packets.len();

        self.maybe_fire_pto(primary_path.borrow().rtt(), now, &mut lost_packets);
        lost_packets
    }

    /// Check how packets should be sent, based on whether there is a PTO,
    /// what the current congestion window is, and what the pacer says.
    #[allow(clippy::option_if_let_else)]
    pub fn send_profile(&mut self, path: &Path, now: Instant) -> SendProfile {
        qdebug!([self], "get send profile {:?}", now);
        let sender = path.sender();
        let mtu = path.mtu();
        if let Some(profile) = self
            .pto_state
            .as_mut()
            .and_then(|pto| pto.send_profile(mtu))
        {
            profile
        } else {
            let limit = min(sender.cwnd_avail(), path.amplification_limit());
            if limit > mtu {
                // More than an MTU available; we might need to pace.
                if sender
                    .next_paced(path.rtt().estimate())
                    .map_or(false, |t| t > now)
                {
                    SendProfile::new_paced()
                } else {
                    SendProfile::new_limited(mtu)
                }
            } else if sender.recovery_packet() {
                // After entering recovery, allow a packet to be sent immediately.
                // This uses the PTO machinery, probing in all spaces. This will
                // result in a PING being sent in every active space.
                SendProfile::new_pto(PacketNumberSpace::Initial, mtu, PacketNumberSpaceSet::all())
            } else {
                SendProfile::new_limited(limit)
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
    use super::{
        LossRecovery, LossRecoverySpace, PacketNumberSpace, SendProfile, SentPacket, FAST_PTO_SCALE,
    };
    use crate::cc::CongestionControlAlgorithm;
    use crate::cid::{ConnectionId, ConnectionIdEntry};
    use crate::packet::PacketType;
    use crate::path::{Path, PathRef};
    use crate::rtt::RttEstimate;
    use crate::stats::{Stats, StatsCell};
    use neqo_common::qlog::NeqoQlog;
    use std::cell::RefCell;
    use std::convert::TryInto;
    use std::ops::{Deref, DerefMut, RangeInclusive};
    use std::rc::Rc;
    use std::time::{Duration, Instant};
    use test_fixture::{addr, now};

    // Shorthand for a time in milliseconds.
    const fn ms(t: u64) -> Duration {
        Duration::from_millis(t)
    }

    const ON_SENT_SIZE: usize = 100;
    /// An initial RTT for using with `setup_lr`.
    const TEST_RTT: Duration = ms(80);
    const TEST_RTTVAR: Duration = ms(40);

    struct Fixture {
        lr: LossRecovery,
        path: PathRef,
    }

    // This shadows functions on the base object so that the path and RTT estimator
    // is used consistently in the tests.  It also simplifies the function signatures.
    impl Fixture {
        pub fn on_ack_received(
            &mut self,
            pn_space: PacketNumberSpace,
            largest_acked: u64,
            acked_ranges: Vec<RangeInclusive<u64>>,
            ack_delay: Duration,
            now: Instant,
        ) -> (Vec<SentPacket>, Vec<SentPacket>) {
            self.lr.on_ack_received(
                &self.path,
                pn_space,
                largest_acked,
                acked_ranges,
                ack_delay,
                now,
            )
        }

        pub fn on_packet_sent(&mut self, sent_packet: SentPacket) {
            self.lr.on_packet_sent(&self.path, sent_packet);
        }

        pub fn timeout(&mut self, now: Instant) -> Vec<SentPacket> {
            self.lr.timeout(&self.path, now)
        }

        pub fn next_timeout(&mut self) -> Option<Instant> {
            self.lr.next_timeout(self.path.borrow().rtt())
        }

        pub fn discard(&mut self, space: PacketNumberSpace, now: Instant) {
            self.lr.discard(&self.path, space, now);
        }

        pub fn pto_time(&self, space: PacketNumberSpace) -> Option<Instant> {
            self.lr.pto_time(self.path.borrow().rtt(), space)
        }

        pub fn send_profile(&mut self, now: Instant) -> SendProfile {
            self.lr.send_profile(&self.path.borrow(), now)
        }
    }

    impl Default for Fixture {
        fn default() -> Self {
            const CC: CongestionControlAlgorithm = CongestionControlAlgorithm::NewReno;
            let mut path = Path::temporary(addr(), addr(), CC, NeqoQlog::default(), now());
            path.make_permanent(
                None,
                ConnectionIdEntry::new(0, ConnectionId::from(&[1, 2, 3]), [0; 16]),
            );
            path.set_primary(true);
            Self {
                lr: LossRecovery::new(StatsCell::default(), FAST_PTO_SCALE),
                path: Rc::new(RefCell::new(path)),
            }
        }
    }

    // Most uses of the fixture only care about the loss recovery piece,
    // but the internal functions need the other bits.
    impl Deref for Fixture {
        type Target = LossRecovery;
        #[must_use]
        fn deref(&self) -> &Self::Target {
            &self.lr
        }
    }

    impl DerefMut for Fixture {
        fn deref_mut(&mut self) -> &mut Self::Target {
            &mut self.lr
        }
    }

    fn assert_rtts(
        lr: &Fixture,
        latest_rtt: Duration,
        smoothed_rtt: Duration,
        rttvar: Duration,
        min_rtt: Duration,
    ) {
        let p = lr.path.borrow();
        let rtt = p.rtt();
        println!(
            "rtts: {:?} {:?} {:?} {:?}",
            rtt.latest(),
            rtt.estimate(),
            rtt.rttvar(),
            rtt.minimum(),
        );
        assert_eq!(rtt.latest(), latest_rtt, "latest RTT");
        assert_eq!(rtt.estimate(), smoothed_rtt, "smoothed RTT");
        assert_eq!(rtt.rttvar(), rttvar, "RTT variance");
        assert_eq!(rtt.minimum(), min_rtt, "min RTT");
    }

    fn assert_sent_times(
        lr: &Fixture,
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
            est(PacketNumberSpace::Initial),
            est(PacketNumberSpace::Handshake),
            est(PacketNumberSpace::ApplicationData),
        );
        assert_eq!(
            est(PacketNumberSpace::Initial),
            initial,
            "Initial earliest sent time"
        );
        assert_eq!(
            est(PacketNumberSpace::Handshake),
            handshake,
            "Handshake earliest sent time"
        );
        assert_eq!(
            est(PacketNumberSpace::ApplicationData),
            app_data,
            "AppData earliest sent time"
        );
    }

    fn assert_no_sent_times(lr: &Fixture) {
        assert_sent_times(lr, None, None, None);
    }

    // In most of the tests below, packets are sent at a fixed cadence, with PACING between each.
    const PACING: Duration = ms(7);
    fn pn_time(pn: u64) -> Instant {
        now() + (PACING * pn.try_into().unwrap())
    }

    fn pace(lr: &mut Fixture, count: u64) {
        for pn in 0..count {
            lr.on_packet_sent(SentPacket::new(
                PacketType::Short,
                pn,
                pn_time(pn),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ));
        }
    }

    const ACK_DELAY: Duration = ms(24);
    /// Acknowledge PN with the identified delay.
    fn ack(lr: &mut Fixture, pn: u64, delay: Duration) {
        lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            pn,
            vec![pn..=pn],
            ACK_DELAY,
            pn_time(pn) + delay,
        );
    }

    fn add_sent(lrs: &mut LossRecoverySpace, packet_numbers: &[u64]) {
        for &pn in packet_numbers {
            lrs.on_packet_sent(SentPacket::new(
                PacketType::Short,
                pn,
                pn_time(pn),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ));
        }
    }

    fn match_acked(acked: &[SentPacket], expected: &[u64]) {
        assert!(acked.iter().map(|p| &p.pn).eq(expected));
    }

    #[test]
    fn remove_acked() {
        let mut lrs = LossRecoverySpace::new(PacketNumberSpace::ApplicationData);
        let mut stats = Stats::default();
        add_sent(&mut lrs, &[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        let (acked, _) = lrs.remove_acked(vec![], &mut stats);
        assert!(acked.is_empty());
        let (acked, _) = lrs.remove_acked(vec![7..=8, 2..=4], &mut stats);
        match_acked(&acked, &[8, 7, 4, 3, 2]);
        let (acked, _) = lrs.remove_acked(vec![8..=11], &mut stats);
        match_acked(&acked, &[10, 9]);
        let (acked, _) = lrs.remove_acked(vec![0..=2], &mut stats);
        match_acked(&acked, &[1]);
        let (acked, _) = lrs.remove_acked(vec![5..=6], &mut stats);
        match_acked(&acked, &[6, 5]);
    }

    #[test]
    fn initial_rtt() {
        let mut lr = Fixture::default();
        pace(&mut lr, 1);
        let rtt = ms(100);
        ack(&mut lr, 0, rtt);
        assert_rtts(&lr, rtt, rtt, rtt / 2, rtt);
        assert_no_sent_times(&lr);
    }

    /// Send `n` packets (using PACING), then acknowledge the first.
    fn setup_lr(n: u64) -> Fixture {
        let mut lr = Fixture::default();
        pace(&mut lr, n);
        ack(&mut lr, 0, TEST_RTT);
        assert_rtts(&lr, TEST_RTT, TEST_RTT, TEST_RTTVAR, TEST_RTT);
        assert_no_sent_times(&lr);
        lr
    }

    // The ack delay is removed from any RTT estimate.
    #[test]
    fn ack_delay_adjusted() {
        let mut lr = setup_lr(2);
        ack(&mut lr, 1, TEST_RTT + ACK_DELAY);
        // RTT stays the same, but the RTTVAR is adjusted downwards.
        assert_rtts(&lr, TEST_RTT, TEST_RTT, TEST_RTTVAR * 3 / 4, TEST_RTT);
        assert_no_sent_times(&lr);
    }

    // The ack delay is ignored when it would cause a sample to be less than min_rtt.
    #[test]
    fn ack_delay_ignored() {
        let mut lr = setup_lr(2);
        let extra = ms(8);
        assert!(extra < ACK_DELAY);
        ack(&mut lr, 1, TEST_RTT + extra);
        let expected_rtt = TEST_RTT + (extra / 8);
        let expected_rttvar = (TEST_RTTVAR * 3 + extra) / 4;
        assert_rtts(
            &lr,
            TEST_RTT + extra,
            expected_rtt,
            expected_rttvar,
            TEST_RTT,
        );
        assert_no_sent_times(&lr);
    }

    // A lower observed RTT is used as min_rtt (and ack delay is ignored).
    #[test]
    fn reduce_min_rtt() {
        let mut lr = setup_lr(2);
        let delta = ms(4);
        let reduced_rtt = TEST_RTT - delta;
        ack(&mut lr, 1, reduced_rtt);
        let expected_rtt = TEST_RTT - (delta / 8);
        let expected_rttvar = (TEST_RTTVAR * 3 + delta) / 4;
        assert_rtts(&lr, reduced_rtt, expected_rtt, expected_rttvar, reduced_rtt);
        assert_no_sent_times(&lr);
    }

    // Acknowledging something again has no effect.
    #[test]
    fn no_new_acks() {
        let mut lr = setup_lr(1);
        let check = |lr: &Fixture| {
            assert_rtts(lr, TEST_RTT, TEST_RTT, TEST_RTTVAR, TEST_RTT);
            assert_no_sent_times(lr);
        };
        check(&lr);

        ack(&mut lr, 0, ms(1339)); // much delayed ACK
        check(&lr);

        ack(&mut lr, 0, ms(3)); // time travel!
        check(&lr);
    }

    // Test time loss detection as part of handling a regular ACK.
    #[test]
    fn time_loss_detection_gap() {
        let mut lr = Fixture::default();
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
            Vec::new(),
            ON_SENT_SIZE,
        ));
        lr.on_packet_sent(SentPacket::new(
            PacketType::Short,
            1,
            pn_time(0) + TEST_RTT / 4,
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));
        let (_, lost) = lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            1,
            vec![1..=1],
            ACK_DELAY,
            pn_time(0) + (TEST_RTT * 5 / 4),
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
        let pn1_loss_time = pn1_sent_time + (TEST_RTT * 9 / 8);
        let pn2_ack_time = pn_time(2) + TEST_RTT;
        assert!(pn1_loss_time > pn2_ack_time);

        let (_, lost) = lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            2,
            vec![2..=2],
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
        assert!(packets[0].expired(pn1_loss_time, Duration::new(0, 0)));
        assert_no_sent_times(&lr);
    }

    #[test]
    fn big_gap_loss() {
        let mut lr = setup_lr(5); // This sends packets 0-4 and acknowledges pn 0.

        // Acknowledge just 2-4, which will cause pn 1 to be marked as lost.
        assert_eq!(super::PACKET_THRESHOLD, 3);
        let (_, lost) = lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            4,
            vec![2..=4],
            ACK_DELAY,
            pn_time(4),
        );
        assert_eq!(lost.len(), 1);
    }

    #[test]
    #[should_panic(expected = "discarding application space")]
    fn drop_app() {
        let mut lr = Fixture::default();
        lr.discard(PacketNumberSpace::ApplicationData, now());
    }

    #[test]
    #[should_panic(expected = "dropping spaces out of order")]
    fn drop_out_of_order() {
        let mut lr = Fixture::default();
        lr.discard(PacketNumberSpace::Handshake, now());
    }

    #[test]
    fn ack_after_drop() {
        let mut lr = Fixture::default();
        lr.discard(PacketNumberSpace::Initial, now());
        let (acked, lost) = lr.on_ack_received(
            PacketNumberSpace::Initial,
            0,
            vec![],
            Duration::from_millis(0),
            pn_time(0),
        );
        assert!(acked.is_empty());
        assert!(lost.is_empty());
    }

    #[test]
    fn drop_spaces() {
        let mut lr = Fixture::default();
        lr.on_packet_sent(SentPacket::new(
            PacketType::Initial,
            0,
            pn_time(0),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));
        lr.on_packet_sent(SentPacket::new(
            PacketType::Handshake,
            0,
            pn_time(1),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));
        lr.on_packet_sent(SentPacket::new(
            PacketType::Short,
            0,
            pn_time(2),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));

        // Now put all spaces on the LR timer so we can see them.
        for sp in &[
            PacketType::Initial,
            PacketType::Handshake,
            PacketType::Short,
        ] {
            let sent_pkt = SentPacket::new(*sp, 1, pn_time(3), true, Vec::new(), ON_SENT_SIZE);
            let pn_space = PacketNumberSpace::from(sent_pkt.pt);
            lr.on_packet_sent(sent_pkt);
            lr.on_ack_received(pn_space, 1, vec![1..=1], Duration::from_secs(0), pn_time(3));
            let mut lost = Vec::new();
            lr.spaces.get_mut(pn_space).unwrap().detect_lost_packets(
                pn_time(3),
                TEST_RTT,
                TEST_RTT * 3, // unused
                &mut lost,
            );
            assert!(lost.is_empty());
        }

        lr.discard(PacketNumberSpace::Initial, pn_time(3));
        assert_sent_times(&lr, None, Some(pn_time(1)), Some(pn_time(2)));

        lr.discard(PacketNumberSpace::Handshake, pn_time(3));
        assert_sent_times(&lr, None, None, Some(pn_time(2)));

        // There are cases where we send a packet that is not subsequently tracked.
        // So check that this works.
        lr.on_packet_sent(SentPacket::new(
            PacketType::Initial,
            0,
            pn_time(3),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));
        assert_sent_times(&lr, None, None, Some(pn_time(2)));
    }

    #[test]
    fn rearm_pto_after_confirmed() {
        let mut lr = Fixture::default();
        lr.on_packet_sent(SentPacket::new(
            PacketType::Initial,
            0,
            now(),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));
        // Set the RTT to the initial value so that discarding doesn't
        // alter the estimate.
        let rtt = lr.path.borrow().rtt().estimate();
        lr.on_ack_received(
            PacketNumberSpace::Initial,
            0,
            vec![0..=0],
            Duration::new(0, 0),
            now() + rtt,
        );

        lr.on_packet_sent(SentPacket::new(
            PacketType::Handshake,
            0,
            now(),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));
        lr.on_packet_sent(SentPacket::new(
            PacketType::Short,
            0,
            now(),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));

        assert_eq!(lr.pto_time(PacketNumberSpace::ApplicationData), None);
        lr.discard(PacketNumberSpace::Initial, pn_time(1));
        assert_eq!(lr.pto_time(PacketNumberSpace::ApplicationData), None);

        // Expiring state after the PTO on the ApplicationData space has
        // expired should result in setting a PTO state.
        let default_pto = RttEstimate::default().pto(PacketNumberSpace::ApplicationData);
        let expected_pto = pn_time(2) + default_pto;
        lr.discard(PacketNumberSpace::Handshake, expected_pto);
        let profile = lr.send_profile(now());
        assert!(profile.pto.is_some());
        assert!(!profile.should_probe(PacketNumberSpace::Initial));
        assert!(!profile.should_probe(PacketNumberSpace::Handshake));
        assert!(profile.should_probe(PacketNumberSpace::ApplicationData));
    }

    #[test]
    fn no_pto_if_amplification_limited() {
        let mut lr = Fixture::default();
        // Eat up the amplification limit by telling the path that we've sent a giant packet.
        {
            const SPARE: usize = 10;
            let mut path = lr.path.borrow_mut();
            let limit = path.amplification_limit();
            path.add_sent(limit - SPARE);
            assert_eq!(path.amplification_limit(), SPARE);
        }

        lr.on_packet_sent(SentPacket::new(
            PacketType::Initial,
            1,
            now(),
            true,
            Vec::new(),
            ON_SENT_SIZE,
        ));

        let handshake_pto = RttEstimate::default().pto(PacketNumberSpace::Handshake);
        let expected_pto = now() + handshake_pto;
        assert_eq!(lr.pto_time(PacketNumberSpace::Initial), Some(expected_pto));
        let profile = lr.send_profile(now());
        assert!(profile.ack_only(PacketNumberSpace::Initial));
        assert!(profile.pto.is_none());
        assert!(!profile.should_probe(PacketNumberSpace::Initial));
        assert!(!profile.should_probe(PacketNumberSpace::Handshake));
        assert!(!profile.should_probe(PacketNumberSpace::ApplicationData));
    }
}
