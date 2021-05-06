// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of received packets and generating acks thereof.

#![deny(clippy::pedantic)]

use std::cmp::min;
use std::collections::VecDeque;
use std::convert::TryFrom;
use std::ops::{Index, IndexMut};
use std::time::{Duration, Instant};

use neqo_common::{qdebug, qinfo, qtrace, qwarn};
use neqo_crypto::{Epoch, TLS_EPOCH_HANDSHAKE, TLS_EPOCH_INITIAL};

use crate::packet::{PacketBuilder, PacketNumber, PacketType};
use crate::recovery::RecoveryToken;
use crate::stats::FrameStats;
use crate::{Error, Res};

use smallvec::{smallvec, SmallVec};

// TODO(mt) look at enabling EnumMap for this: https://stackoverflow.com/a/44905797/1375574
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd, Ord, Eq)]
pub enum PacketNumberSpace {
    Initial,
    Handshake,
    ApplicationData,
}

#[allow(clippy::use_self)] // https://github.com/rust-lang/rust-clippy/issues/3410
impl PacketNumberSpace {
    pub fn iter() -> impl Iterator<Item = &'static PacketNumberSpace> {
        const SPACES: &[PacketNumberSpace] = &[
            PacketNumberSpace::Initial,
            PacketNumberSpace::Handshake,
            PacketNumberSpace::ApplicationData,
        ];
        SPACES.iter()
    }
}

impl From<Epoch> for PacketNumberSpace {
    fn from(epoch: Epoch) -> Self {
        match epoch {
            TLS_EPOCH_INITIAL => Self::Initial,
            TLS_EPOCH_HANDSHAKE => Self::Handshake,
            _ => Self::ApplicationData,
        }
    }
}

impl From<PacketType> for PacketNumberSpace {
    fn from(pt: PacketType) -> Self {
        match pt {
            PacketType::Initial => Self::Initial,
            PacketType::Handshake => Self::Handshake,
            PacketType::ZeroRtt | PacketType::Short => Self::ApplicationData,
            _ => panic!("Attempted to get space from wrong packet type"),
        }
    }
}

#[derive(Clone, Copy, Default)]
pub struct PacketNumberSpaceSet {
    initial: bool,
    handshake: bool,
    application_data: bool,
}

impl PacketNumberSpaceSet {
    pub fn all() -> Self {
        Self {
            initial: true,
            handshake: true,
            application_data: true,
        }
    }
}

impl Index<PacketNumberSpace> for PacketNumberSpaceSet {
    type Output = bool;

    fn index(&self, space: PacketNumberSpace) -> &Self::Output {
        match space {
            PacketNumberSpace::Initial => &self.initial,
            PacketNumberSpace::Handshake => &self.handshake,
            PacketNumberSpace::ApplicationData => &self.application_data,
        }
    }
}

impl IndexMut<PacketNumberSpace> for PacketNumberSpaceSet {
    fn index_mut(&mut self, space: PacketNumberSpace) -> &mut Self::Output {
        match space {
            PacketNumberSpace::Initial => &mut self.initial,
            PacketNumberSpace::Handshake => &mut self.handshake,
            PacketNumberSpace::ApplicationData => &mut self.application_data,
        }
    }
}

impl<T: AsRef<[PacketNumberSpace]>> From<T> for PacketNumberSpaceSet {
    fn from(spaces: T) -> Self {
        let mut v = Self::default();
        for sp in spaces.as_ref() {
            v[*sp] = true;
        }
        v
    }
}

impl std::fmt::Debug for PacketNumberSpaceSet {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let mut first = true;
        f.write_str("(")?;
        for sp in PacketNumberSpace::iter() {
            if self[*sp] {
                if !first {
                    f.write_str("+")?;
                    first = false;
                }
                std::fmt::Display::fmt(sp, f)?;
            }
        }
        f.write_str(")")
    }
}

#[derive(Debug, Clone)]
pub struct SentPacket {
    pub pt: PacketType,
    pub pn: PacketNumber,
    ack_eliciting: bool,
    pub time_sent: Instant,
    primary_path: bool,
    pub tokens: Vec<RecoveryToken>,

    time_declared_lost: Option<Instant>,
    /// After a PTO, this is true when the packet has been released.
    pto: bool,

    pub size: usize,
}

impl SentPacket {
    pub fn new(
        pt: PacketType,
        pn: PacketNumber,
        time_sent: Instant,
        ack_eliciting: bool,
        tokens: Vec<RecoveryToken>,
        size: usize,
    ) -> Self {
        Self {
            pt,
            pn,
            time_sent,
            ack_eliciting,
            primary_path: true,
            tokens,
            time_declared_lost: None,
            pto: false,
            size,
        }
    }

    /// Returns `true` if the packet will elicit an ACK.
    pub fn ack_eliciting(&self) -> bool {
        self.ack_eliciting
    }

    /// Returns `true` if the packet was sent on the primary path.
    pub fn on_primary_path(&self) -> bool {
        self.primary_path
    }

    /// Clears the flag that had this packet on the primary path.
    /// Used when migrating to clear out state.
    pub fn clear_primary_path(&mut self) {
        self.primary_path = false;
    }

    /// Whether the packet has been declared lost.
    pub fn lost(&self) -> bool {
        self.time_declared_lost.is_some()
    }

    /// Whether accounting for the loss or acknowledgement in the
    /// congestion controller is pending.
    /// Returns `true` if the packet counts as being "in flight",
    /// and has not previously been declared lost.
    /// Note that this should count packets that contain only ACK and PADDING,
    /// but we don't send PADDING, so we don't track that.
    pub fn cc_outstanding(&self) -> bool {
        self.ack_eliciting() && self.on_primary_path() && !self.lost()
    }

    /// Whether the packet should be tracked as in-flight.
    pub fn cc_in_flight(&self) -> bool {
        self.ack_eliciting() && self.on_primary_path()
    }

    /// Declare the packet as lost.  Returns `true` if this is the first time.
    pub fn declare_lost(&mut self, now: Instant) -> bool {
        if self.lost() {
            false
        } else {
            self.time_declared_lost = Some(now);
            true
        }
    }

    /// Ask whether this tracked packet has been declared lost for long enough
    /// that it can be expired and no longer tracked.
    pub fn expired(&self, now: Instant, expiration_period: Duration) -> bool {
        self.time_declared_lost
            .map_or(false, |loss_time| (loss_time + expiration_period) <= now)
    }

    /// Whether the packet contents were cleared out after a PTO.
    pub fn pto_fired(&self) -> bool {
        self.pto
    }

    /// On PTO, we need to get the recovery tokens so that we can ensure that
    /// the frames we sent can be sent again in the PTO packet(s).  Do that just once.
    pub fn pto(&mut self) -> bool {
        if self.pto || self.lost() {
            false
        } else {
            self.pto = true;
            true
        }
    }
}

impl std::fmt::Display for PacketNumberSpace {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str(match self {
            Self::Initial => "in",
            Self::Handshake => "hs",
            Self::ApplicationData => "ap",
        })
    }
}

/// `InsertionResult` tracks whether something was inserted for `PacketRange::add()`.
pub enum InsertionResult {
    Largest,
    Smallest,
    NotInserted,
}

#[derive(Clone, Debug, Default)]
pub struct PacketRange {
    largest: PacketNumber,
    smallest: PacketNumber,
    ack_needed: bool,
}

impl PacketRange {
    /// Make a single packet range.
    pub fn new(pn: PacketNumber) -> Self {
        Self {
            largest: pn,
            smallest: pn,
            ack_needed: true,
        }
    }

    /// Get the number of acknowleged packets in the range.
    pub fn len(&self) -> u64 {
        self.largest - self.smallest + 1
    }

    /// Returns whether this needs to be sent.
    pub fn ack_needed(&self) -> bool {
        self.ack_needed
    }

    /// Return whether the given number is in the range.
    pub fn contains(&self, pn: PacketNumber) -> bool {
        (pn >= self.smallest) && (pn <= self.largest)
    }

    /// Maybe add a packet number to the range.  Returns true if it was added
    /// at the small end (which indicates that this might need merging with a
    /// preceding range).
    pub fn add(&mut self, pn: PacketNumber) -> InsertionResult {
        assert!(!self.contains(pn));
        // Only insert if this is adjacent the current range.
        if (self.largest + 1) == pn {
            qtrace!([self], "Adding largest {}", pn);
            self.largest += 1;
            self.ack_needed = true;
            InsertionResult::Largest
        } else if self.smallest == (pn + 1) {
            qtrace!([self], "Adding smallest {}", pn);
            self.smallest -= 1;
            self.ack_needed = true;
            InsertionResult::Smallest
        } else {
            InsertionResult::NotInserted
        }
    }

    /// Maybe merge a higher-numbered range into this.
    fn merge_larger(&mut self, other: &Self) {
        qinfo!([self], "Merging {}", other);
        // This only works if they are immediately adjacent.
        assert_eq!(self.largest + 1, other.smallest);

        self.largest = other.largest;
        self.ack_needed = self.ack_needed || other.ack_needed;
    }

    /// When a packet containing the range `other` is acknowledged,
    /// clear the `ack_needed` attribute on this.
    /// Requires that other is equal to this, or a larger range.
    pub fn acknowledged(&mut self, other: &Self) {
        if (other.smallest <= self.smallest) && (other.largest >= self.largest) {
            self.ack_needed = false;
        }
    }
}

impl ::std::fmt::Display for PacketRange {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}->{}", self.largest, self.smallest)
    }
}

/// The ACK delay we use.
pub const ACK_DELAY: Duration = Duration::from_millis(20); // 20ms
pub const MAX_UNACKED_PKTS: usize = 1;
const MAX_TRACKED_RANGES: usize = 32;
const MAX_ACKS_PER_FRAME: usize = 32;

/// A structure that tracks what was included in an ACK.
#[derive(Debug, Clone)]
pub struct AckToken {
    space: PacketNumberSpace,
    ranges: Vec<PacketRange>,
}

/// A structure that tracks what packets have been received,
/// and what needs acknowledgement for a packet number space.
#[derive(Debug)]
pub struct RecvdPackets {
    space: PacketNumberSpace,
    ranges: VecDeque<PacketRange>,
    /// The packet number of the lowest number packet that we are tracking.
    min_tracked: PacketNumber,
    /// The time we got the largest acknowledged.
    largest_pn_time: Option<Instant>,
    // The time that we should be sending an ACK.
    ack_time: Option<Instant>,
    pkts_since_last_ack: usize,
}

impl RecvdPackets {
    /// Make a new `RecvdPackets` for the indicated packet number space.
    pub fn new(space: PacketNumberSpace) -> Self {
        Self {
            space,
            ranges: VecDeque::new(),
            min_tracked: 0,
            largest_pn_time: None,
            ack_time: None,
            pkts_since_last_ack: 0,
        }
    }

    /// Get the time at which the next ACK should be sent.
    pub fn ack_time(&self) -> Option<Instant> {
        self.ack_time
    }

    /// Returns true if an ACK frame should be sent now.
    fn ack_now(&self, now: Instant) -> bool {
        match self.ack_time {
            Some(t) => t <= now,
            None => false,
        }
    }

    // A simple addition of a packet number to the tracked set.
    // This doesn't do a binary search on the assumption that
    // new packets will generally be added to the start of the list.
    fn add(&mut self, pn: PacketNumber) {
        for i in 0..self.ranges.len() {
            match self.ranges[i].add(pn) {
                InsertionResult::Largest => return,
                InsertionResult::Smallest => {
                    // If this was the smallest, it might have filled a gap.
                    let nxt = i + 1;
                    if (nxt < self.ranges.len()) && (pn - 1 == self.ranges[nxt].largest) {
                        let larger = self.ranges.remove(i).unwrap();
                        self.ranges[i].merge_larger(&larger);
                    }
                    return;
                }
                InsertionResult::NotInserted => {
                    if self.ranges[i].largest < pn {
                        self.ranges.insert(i, PacketRange::new(pn));
                        return;
                    }
                }
            }
        }
        self.ranges.push_back(PacketRange::new(pn));
    }

    fn trim_ranges(&mut self) {
        // Limit the number of ranges that are tracked to MAX_TRACKED_RANGES.
        if self.ranges.len() > MAX_TRACKED_RANGES {
            let oldest = self.ranges.pop_back().unwrap();
            if oldest.ack_needed {
                qwarn!([self], "Dropping unacknowledged ACK range: {}", oldest);
            // TODO(mt) Record some statistics about this so we can tune MAX_TRACKED_RANGES.
            } else {
                qdebug!([self], "Drop ACK range: {}", oldest);
            }
            self.min_tracked = oldest.largest + 1;
        }
    }

    /// Add the packet to the tracked set.
    /// Return true if the packet was the largest received so far.
    pub fn set_received(&mut self, now: Instant, pn: PacketNumber, ack_eliciting: bool) -> bool {
        let next_in_order_pn = self.ranges.front().map_or(0, |pr| pr.largest + 1);
        qdebug!(
            [self],
            "received {}, next in order pn: {}",
            pn,
            next_in_order_pn
        );

        self.add(pn);
        self.trim_ranges();

        // The new addition was the largest, so update the time we use for calculating ACK delay.
        let largest = if pn >= next_in_order_pn {
            self.largest_pn_time = Some(now);
            true
        } else {
            false
        };

        if ack_eliciting {
            self.pkts_since_last_ack += 1;

            // Send ACK right away if out-of-order
            // On the first in-order ack-eliciting packet since sending an ACK,
            // set a delay.
            // Count packets until we exceed MAX_UNACKED_PKTS, then remove the
            // delay.
            if pn != next_in_order_pn {
                self.ack_time = Some(now);
            } else if self.space == PacketNumberSpace::ApplicationData {
                match &mut self.pkts_since_last_ack {
                    0 => unreachable!(),
                    1 => self.ack_time = Some(now + ACK_DELAY),
                    x if *x > MAX_UNACKED_PKTS => self.ack_time = Some(now),
                    _ => debug_assert!(self.ack_time.is_some()),
                }
            } else {
                self.ack_time = Some(now);
            }
            qdebug!([self], "Set ACK timer to {:?}", self.ack_time);
        }
        largest
    }

    /// Check if the packet is a duplicate.
    pub fn is_duplicate(&self, pn: PacketNumber) -> bool {
        if pn < self.min_tracked {
            return true;
        }
        // TODO(mt) consider a binary search or early exit.
        for range in &self.ranges {
            if range.contains(pn) {
                return true;
            }
        }
        false
    }

    /// Mark the given range as having been acknowledged.
    pub fn acknowledged(&mut self, acked: &[PacketRange]) {
        let mut range_iter = self.ranges.iter_mut();
        let mut cur = range_iter.next().expect("should have at least one range");
        for ack in acked {
            while cur.smallest > ack.largest {
                cur = match range_iter.next() {
                    Some(c) => c,
                    None => return,
                };
            }
            cur.acknowledged(&ack);
        }
    }

    /// Generate an ACK frame for this packet number space.
    ///
    /// Unlike other frame generators this doesn't modify the underlying instance
    /// to track what has been sent. This only clears the delayed ACK timer.
    ///
    /// When sending ACKs, we want to always send the most recent ranges,
    /// even if they have been sent in other packets.
    ///
    /// We don't send ranges that have been acknowledged, but they still need
    /// to be tracked so that duplicates can be detected.
    fn write_frame(
        &mut self,
        now: Instant,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        // The worst possible ACK frame, assuming only one range.
        // Note that this assumes one byte for the type and count of extra ranges.
        const LONGEST_ACK_HEADER: usize = 1 + 8 + 8 + 1 + 8;

        // Check that we aren't delaying ACKs.
        if !self.ack_now(now) {
            return;
        }

        // Drop extra ACK ranges to fit the available space.  Do this based on
        // a worst-case estimate of frame size for simplicity.
        //
        // When congestion limited, ACK-only packets are 255 bytes at most
        // (`recovery::ACK_ONLY_SIZE_LIMIT - 1`).  This results in limiting the
        // ranges to 13 here.
        let max_ranges = if let Some(avail) = builder.remaining().checked_sub(LONGEST_ACK_HEADER) {
            // Apply a hard maximum to keep plenty of space for other stuff.
            min(1 + (avail / 16), MAX_ACKS_PER_FRAME)
        } else {
            return;
        };

        let ranges = self
            .ranges
            .iter()
            .filter(|r| r.ack_needed())
            .take(max_ranges)
            .cloned()
            .collect::<Vec<_>>();

        builder.encode_varint(crate::frame::FRAME_TYPE_ACK);
        let mut iter = ranges.iter();
        let first = match iter.next() {
            Some(v) => v,
            None => return, // Nothing to send.
        };
        builder.encode_varint(first.largest);
        stats.largest_acknowledged = first.largest;
        stats.ack += 1;

        let elapsed = now.duration_since(self.largest_pn_time.unwrap());
        // We use the default exponent, so delay is in multiples of 8 microseconds.
        let ack_delay = u64::try_from(elapsed.as_micros() / 8).unwrap_or(u64::MAX);
        let ack_delay = min((1 << 62) - 1, ack_delay);
        builder.encode_varint(ack_delay);
        builder.encode_varint(u64::try_from(ranges.len() - 1).unwrap()); // extra ranges
        builder.encode_varint(first.len() - 1); // first range

        let mut last = first.smallest;
        for r in iter {
            // the difference must be at least 2 because 0-length gaps,
            // (difference 1) are illegal.
            builder.encode_varint(last - r.largest - 2); // Gap
            builder.encode_varint(r.len() - 1); // Range
            last = r.smallest;
        }

        // We've sent an ACK, reset the timer.
        self.ack_time = None;
        self.pkts_since_last_ack = 0;

        tokens.push(RecoveryToken::Ack(AckToken {
            space: self.space,
            ranges,
        }));
    }
}

impl ::std::fmt::Display for RecvdPackets {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Recvd-{}", self.space)
    }
}

#[derive(Debug)]
pub struct AckTracker {
    /// This stores information about received packets in *reverse* order
    /// by spaces.  Why reverse?  Because we ultimately only want to keep
    /// `ApplicationData` and this allows us to drop other spaces easily.
    spaces: SmallVec<[RecvdPackets; 1]>,
}

impl AckTracker {
    pub fn drop_space(&mut self, space: PacketNumberSpace) {
        let sp = match space {
            PacketNumberSpace::Initial => self.spaces.pop(),
            PacketNumberSpace::Handshake => {
                let sp = self.spaces.pop();
                self.spaces.shrink_to_fit();
                sp
            }
            PacketNumberSpace::ApplicationData => panic!("discarding application space"),
        };
        assert_eq!(sp.unwrap().space, space, "dropping spaces out of order");
    }

    pub fn get_mut(&mut self, space: PacketNumberSpace) -> Option<&mut RecvdPackets> {
        self.spaces.get_mut(match space {
            PacketNumberSpace::ApplicationData => 0,
            PacketNumberSpace::Handshake => 1,
            PacketNumberSpace::Initial => 2,
        })
    }

    /// Determine the earliest time that an ACK might be needed.
    pub fn ack_time(&self, now: Instant) -> Option<Instant> {
        if self.spaces.len() == 1 {
            self.spaces[0].ack_time()
        } else {
            // Ignore any time that is in the past relative to `now`.
            // That is something of a hack, but there are cases where we can't send ACK
            // frames for all spaces, which can mean that one space is stuck in the past.
            // That isn't a problem because we guarantee that earlier spaces will always
            // be able to send ACK frames.
            self.spaces
                .iter()
                .filter_map(|recvd| recvd.ack_time().filter(|t| *t > now))
                .min()
        }
    }

    pub fn acked(&mut self, token: &AckToken) {
        if let Some(space) = self.get_mut(token.space) {
            space.acknowledged(&token.ranges);
        }
    }

    pub(crate) fn write_frame(
        &mut self,
        pn_space: PacketNumberSpace,
        now: Instant,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> Res<()> {
        if let Some(space) = self.get_mut(pn_space) {
            space.write_frame(now, builder, tokens, stats);
            if builder.len() > builder.limit() {
                return Err(Error::InternalError(24));
            }
        }
        Ok(())
    }
}

impl Default for AckTracker {
    fn default() -> Self {
        Self {
            spaces: smallvec![
                RecvdPackets::new(PacketNumberSpace::ApplicationData),
                RecvdPackets::new(PacketNumberSpace::Handshake),
                RecvdPackets::new(PacketNumberSpace::Initial),
            ],
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{
        AckTracker, Duration, Instant, PacketNumberSpace, PacketNumberSpaceSet, RecoveryToken,
        RecvdPackets, ACK_DELAY, MAX_TRACKED_RANGES, MAX_UNACKED_PKTS,
    };
    use crate::frame::Frame;
    use crate::packet::PacketBuilder;
    use crate::stats::FrameStats;
    use lazy_static::lazy_static;
    use neqo_common::Encoder;
    use std::collections::HashSet;
    use std::convert::TryFrom;

    lazy_static! {
        static ref NOW: Instant = Instant::now();
    }

    fn test_ack_range(pns: &[u64], nranges: usize) {
        let mut rp = RecvdPackets::new(PacketNumberSpace::Initial); // Any space will do.
        let mut packets = HashSet::new();

        for pn in pns {
            rp.set_received(*NOW, *pn, true);
            packets.insert(*pn);
        }

        assert_eq!(rp.ranges.len(), nranges);

        // Check that all these packets will be detected as duplicates.
        for pn in pns {
            assert!(rp.is_duplicate(*pn));
        }

        // Check that the ranges decrease monotonically and don't overlap.
        let mut iter = rp.ranges.iter();
        let mut last = iter.next().expect("should have at least one");
        for n in iter {
            assert!(n.largest + 1 < last.smallest);
            last = n;
        }

        // Check that the ranges include the right values.
        let mut in_ranges = HashSet::new();
        for range in &rp.ranges {
            for included in range.smallest..=range.largest {
                in_ranges.insert(included);
            }
        }
        assert_eq!(packets, in_ranges);
    }

    #[test]
    fn pn0() {
        test_ack_range(&[0], 1);
    }

    #[test]
    fn pn1() {
        test_ack_range(&[1], 1);
    }

    #[test]
    fn two_ranges() {
        test_ack_range(&[0, 1, 2, 5, 6, 7], 2);
    }

    #[test]
    fn fill_in_range() {
        test_ack_range(&[0, 1, 2, 5, 6, 7, 3, 4], 1);
    }

    #[test]
    fn too_many_ranges() {
        let mut rp = RecvdPackets::new(PacketNumberSpace::Initial); // Any space will do.

        // This will add one too many disjoint ranges.
        for i in 0..=MAX_TRACKED_RANGES {
            rp.set_received(*NOW, (i * 2) as u64, true);
        }

        assert_eq!(rp.ranges.len(), MAX_TRACKED_RANGES);
        assert_eq!(rp.ranges.back().unwrap().largest, 2);

        // Even though the range was dropped, we still consider it a duplicate.
        assert!(rp.is_duplicate(0));
        assert!(!rp.is_duplicate(1));
        assert!(rp.is_duplicate(2));
    }

    #[test]
    fn ack_delay() {
        // Only application data packets are delayed.
        let mut rp = RecvdPackets::new(PacketNumberSpace::ApplicationData);
        assert!(rp.ack_time().is_none());
        assert!(!rp.ack_now(*NOW));

        // Some packets won't cause an ACK to be needed.
        let max_unacked = u64::try_from(MAX_UNACKED_PKTS).unwrap();
        for num in 0..max_unacked {
            rp.set_received(*NOW, num, true);
            assert_eq!(Some(*NOW + ACK_DELAY), rp.ack_time());
            assert!(!rp.ack_now(*NOW));
            assert!(rp.ack_now(*NOW + ACK_DELAY));
        }

        // Exceeding MAX_UNACKED_PKTS will move the ACK time to now.
        rp.set_received(*NOW, max_unacked, true);
        assert_eq!(Some(*NOW), rp.ack_time());
        assert!(rp.ack_now(*NOW));
    }

    #[test]
    fn no_ack_delay() {
        for space in &[PacketNumberSpace::Initial, PacketNumberSpace::Handshake] {
            let mut rp = RecvdPackets::new(*space);
            assert!(rp.ack_time().is_none());
            assert!(!rp.ack_now(*NOW));

            // Any packet will be acknowledged straight away.
            rp.set_received(*NOW, 0, true);
            assert_eq!(Some(*NOW), rp.ack_time());
            assert!(rp.ack_now(*NOW));
        }
    }

    #[test]
    fn ooo_no_ack_delay() {
        for space in &[
            PacketNumberSpace::Initial,
            PacketNumberSpace::Handshake,
            PacketNumberSpace::ApplicationData,
        ] {
            let mut rp = RecvdPackets::new(*space);
            assert!(rp.ack_time().is_none());
            assert!(!rp.ack_now(*NOW));

            // Any OoO packet will be acknowledged straight away.
            rp.set_received(*NOW, 3, true);
            assert_eq!(Some(*NOW), rp.ack_time());
            assert!(rp.ack_now(*NOW));
        }
    }

    #[test]
    fn aggregate_ack_time() {
        let mut tracker = AckTracker::default();
        // This packet won't trigger an ACK.
        tracker
            .get_mut(PacketNumberSpace::Handshake)
            .unwrap()
            .set_received(*NOW, 0, false);
        assert_eq!(None, tracker.ack_time(*NOW));

        // This should be delayed.
        tracker
            .get_mut(PacketNumberSpace::ApplicationData)
            .unwrap()
            .set_received(*NOW, 0, true);
        assert_eq!(Some(*NOW + ACK_DELAY), tracker.ack_time(*NOW));

        // This should move the time forward.
        let later = *NOW + ACK_DELAY.checked_div(2).unwrap();
        tracker
            .get_mut(PacketNumberSpace::Initial)
            .unwrap()
            .set_received(later, 0, true);
        assert_eq!(Some(later), tracker.ack_time(*NOW));
    }

    #[test]
    #[should_panic(expected = "discarding application space")]
    fn drop_app() {
        let mut tracker = AckTracker::default();
        tracker.drop_space(PacketNumberSpace::ApplicationData);
    }

    #[test]
    #[should_panic(expected = "dropping spaces out of order")]
    fn drop_out_of_order() {
        let mut tracker = AckTracker::default();
        tracker.drop_space(PacketNumberSpace::Handshake);
    }

    #[test]
    fn drop_spaces() {
        let mut tracker = AckTracker::default();
        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        tracker
            .get_mut(PacketNumberSpace::Initial)
            .unwrap()
            .set_received(*NOW, 0, true);
        // The reference time for `ack_time` has to be in the past or we filter out the timer.
        assert!(tracker.ack_time(*NOW - Duration::from_millis(1)).is_some());

        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        tracker
            .write_frame(
                PacketNumberSpace::Initial,
                *NOW,
                &mut builder,
                &mut tokens,
                &mut stats,
            )
            .unwrap();
        assert_eq!(stats.ack, 1);

        // Mark another packet as received so we have cause to send another ACK in that space.
        tracker
            .get_mut(PacketNumberSpace::Initial)
            .unwrap()
            .set_received(*NOW, 1, true);
        assert!(tracker.ack_time(*NOW - Duration::from_millis(1)).is_some());

        // Now drop that space.
        tracker.drop_space(PacketNumberSpace::Initial);

        assert!(tracker.get_mut(PacketNumberSpace::Initial).is_none());
        assert!(tracker.ack_time(*NOW - Duration::from_millis(1)).is_none());
        tracker
            .write_frame(
                PacketNumberSpace::Initial,
                *NOW,
                &mut builder,
                &mut tokens,
                &mut stats,
            )
            .unwrap();
        assert_eq!(stats.ack, 1);
        if let RecoveryToken::Ack(tok) = &tokens[0] {
            tracker.acked(tok); // Should be a noop.
        } else {
            panic!("not an ACK token");
        }
    }

    #[test]
    fn no_room_for_ack() {
        let mut tracker = AckTracker::default();
        tracker
            .get_mut(PacketNumberSpace::Initial)
            .unwrap()
            .set_received(*NOW, 0, true);
        assert!(tracker.ack_time(*NOW - Duration::from_millis(1)).is_some());

        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        builder.set_limit(10);

        let mut stats = FrameStats::default();
        tracker
            .write_frame(
                PacketNumberSpace::Initial,
                *NOW,
                &mut builder,
                &mut Vec::new(),
                &mut stats,
            )
            .unwrap();
        assert_eq!(stats.ack, 0);
        assert_eq!(builder.len(), 1); // Only the short packet header has been added.
    }

    #[test]
    fn no_room_for_extra_range() {
        let mut tracker = AckTracker::default();
        tracker
            .get_mut(PacketNumberSpace::Initial)
            .unwrap()
            .set_received(*NOW, 0, true);
        tracker
            .get_mut(PacketNumberSpace::Initial)
            .unwrap()
            .set_received(*NOW, 2, true);
        assert!(tracker.ack_time(*NOW - Duration::from_millis(1)).is_some());

        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        builder.set_limit(32);

        let mut stats = FrameStats::default();
        tracker
            .write_frame(
                PacketNumberSpace::Initial,
                *NOW,
                &mut builder,
                &mut Vec::new(),
                &mut stats,
            )
            .unwrap();
        assert_eq!(stats.ack, 1);

        let mut dec = builder.as_decoder();
        let _ = dec.decode_byte().unwrap(); // Skip the short header.
        let frame = Frame::decode(&mut dec).unwrap();
        if let Frame::Ack { ack_ranges, .. } = frame {
            assert_eq!(ack_ranges.len(), 0);
        } else {
            panic!("not an ACK!");
        }
    }

    #[test]
    fn ack_time_elapsed() {
        let mut tracker = AckTracker::default();

        // While we have multiple PN spaces, we ignore ACK timers from the past.
        // Send out of order to cause the delayed ack timer to be set to `*NOW`.
        tracker
            .get_mut(PacketNumberSpace::ApplicationData)
            .unwrap()
            .set_received(*NOW, 3, true);
        assert!(tracker.ack_time(*NOW + Duration::from_millis(1)).is_none());

        // When we are reduced to one space, that filter is off.
        tracker.drop_space(PacketNumberSpace::Initial);
        tracker.drop_space(PacketNumberSpace::Handshake);
        assert_eq!(
            tracker.ack_time(*NOW + Duration::from_millis(1)),
            Some(*NOW)
        );
    }

    #[test]
    fn pnspaceset_default() {
        let set = PacketNumberSpaceSet::default();
        assert!(!set[PacketNumberSpace::Initial]);
        assert!(!set[PacketNumberSpace::Handshake]);
        assert!(!set[PacketNumberSpace::ApplicationData]);
    }

    #[test]
    fn pnspaceset_from() {
        let set = PacketNumberSpaceSet::from(&[PacketNumberSpace::Initial]);
        assert!(set[PacketNumberSpace::Initial]);
        assert!(!set[PacketNumberSpace::Handshake]);
        assert!(!set[PacketNumberSpace::ApplicationData]);

        let set =
            PacketNumberSpaceSet::from(&[PacketNumberSpace::Handshake, PacketNumberSpace::Initial]);
        assert!(set[PacketNumberSpace::Initial]);
        assert!(set[PacketNumberSpace::Handshake]);
        assert!(!set[PacketNumberSpace::ApplicationData]);

        let set = PacketNumberSpaceSet::from(&[
            PacketNumberSpace::ApplicationData,
            PacketNumberSpace::ApplicationData,
        ]);
        assert!(!set[PacketNumberSpace::Initial]);
        assert!(!set[PacketNumberSpace::Handshake]);
        assert!(set[PacketNumberSpace::ApplicationData]);
    }

    #[test]
    fn pnspaceset_copy() {
        let set = PacketNumberSpaceSet::from(&[
            PacketNumberSpace::Handshake,
            PacketNumberSpace::ApplicationData,
        ]);
        let copy = set;
        assert!(!copy[PacketNumberSpace::Initial]);
        assert!(copy[PacketNumberSpace::Handshake]);
        assert!(copy[PacketNumberSpace::ApplicationData]);
    }
}
