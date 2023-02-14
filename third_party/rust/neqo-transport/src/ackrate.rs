// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Management of the peer's ack rate.
#![deny(clippy::pedantic)]

use crate::connection::params::ACK_RATIO_SCALE;
use crate::frame::FRAME_TYPE_ACK_FREQUENCY;
use crate::packet::PacketBuilder;
use crate::recovery::RecoveryToken;
use crate::stats::FrameStats;

use neqo_common::qtrace;
use std::cmp::max;
use std::convert::TryFrom;
use std::time::Duration;

#[derive(Debug, Clone)]
pub struct AckRate {
    /// The maximum number of packets that can be received without sending an ACK.
    packets: usize,
    /// The maximum delay before sending an ACK.
    delay: Duration,
}

impl AckRate {
    pub fn new(minimum: Duration, ratio: u8, cwnd: usize, mtu: usize, rtt: Duration) -> Self {
        const PACKET_RATIO: usize = ACK_RATIO_SCALE as usize;
        // At worst, ask for an ACK for every other packet.
        const MIN_PACKETS: usize = 2;
        // At worst, require an ACK every 256 packets.
        const MAX_PACKETS: usize = 256;
        const RTT_RATIO: u32 = ACK_RATIO_SCALE as u32;
        const MAX_DELAY: Duration = Duration::from_millis(50);

        let packets = cwnd * PACKET_RATIO / mtu / usize::from(ratio);
        let packets = packets.clamp(MIN_PACKETS, MAX_PACKETS) - 1;
        let delay = rtt * RTT_RATIO / u32::from(ratio);
        let delay = delay.clamp(minimum, MAX_DELAY);
        qtrace!("AckRate inputs: {}/{}/{}, {:?}", cwnd, mtu, ratio, rtt);
        Self { packets, delay }
    }

    pub fn write_frame(&self, builder: &mut PacketBuilder, seqno: u64) -> bool {
        builder.write_varint_frame(&[
            FRAME_TYPE_ACK_FREQUENCY,
            seqno,
            u64::try_from(self.packets + 1).unwrap(),
            u64::try_from(self.delay.as_micros()).unwrap(),
            0,
        ])
    }

    /// Determine whether to send an update frame.
    pub fn needs_update(&self, target: &Self) -> bool {
        if self.packets != target.packets {
            return true;
        }
        // Allow more flexibility for delays, as those can change
        // by small amounts fairly easily.
        let delta = target.delay / 4;
        target.delay + delta < self.delay || target.delay > self.delay + delta
    }
}

#[derive(Debug, Clone)]
pub struct FlexibleAckRate {
    current: AckRate,
    target: AckRate,
    next_frame_seqno: u64,
    frame_outstanding: bool,
    min_ack_delay: Duration,
    ratio: u8,
}

impl FlexibleAckRate {
    fn new(
        max_ack_delay: Duration,
        min_ack_delay: Duration,
        ratio: u8,
        cwnd: usize,
        mtu: usize,
        rtt: Duration,
    ) -> Self {
        qtrace!(
            "FlexibleAckRate: {:?} {:?} {}",
            max_ack_delay,
            min_ack_delay,
            ratio
        );
        let ratio = max(ACK_RATIO_SCALE, ratio); // clamp it
        Self {
            current: AckRate {
                packets: 1,
                delay: max_ack_delay,
            },
            target: AckRate::new(min_ack_delay, ratio, cwnd, mtu, rtt),
            next_frame_seqno: 0,
            frame_outstanding: false,
            min_ack_delay,
            ratio,
        }
    }

    fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        if !self.frame_outstanding
            && self.current.needs_update(&self.target)
            && self.target.write_frame(builder, self.next_frame_seqno)
        {
            qtrace!("FlexibleAckRate: write frame {:?}", self.target);
            self.frame_outstanding = true;
            self.next_frame_seqno += 1;
            tokens.push(RecoveryToken::AckFrequency(self.target.clone()));
            stats.ack_frequency += 1;
        }
    }

    fn frame_acked(&mut self, acked: &AckRate) {
        self.frame_outstanding = false;
        self.current = acked.clone();
    }

    fn frame_lost(&mut self, _lost: &AckRate) {
        self.frame_outstanding = false;
    }

    fn update(&mut self, cwnd: usize, mtu: usize, rtt: Duration) {
        self.target = AckRate::new(self.min_ack_delay, self.ratio, cwnd, mtu, rtt);
        qtrace!("FlexibleAckRate: {:?} -> {:?}", self.current, self.target);
    }

    fn peer_ack_delay(&self) -> Duration {
        max(self.current.delay, self.target.delay)
    }
}

#[derive(Debug, Clone)]
pub enum PeerAckDelay {
    Fixed(Duration),
    Flexible(FlexibleAckRate),
}

impl PeerAckDelay {
    pub fn fixed(max_ack_delay: Duration) -> Self {
        Self::Fixed(max_ack_delay)
    }

    pub fn flexible(
        max_ack_delay: Duration,
        min_ack_delay: Duration,
        ratio: u8,
        cwnd: usize,
        mtu: usize,
        rtt: Duration,
    ) -> Self {
        Self::Flexible(FlexibleAckRate::new(
            max_ack_delay,
            min_ack_delay,
            ratio,
            cwnd,
            mtu,
            rtt,
        ))
    }

    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        if let Self::Flexible(rate) = self {
            rate.write_frames(builder, tokens, stats);
        }
    }

    pub fn frame_acked(&mut self, r: &AckRate) {
        if let Self::Flexible(rate) = self {
            rate.frame_acked(r);
        }
    }

    pub fn frame_lost(&mut self, r: &AckRate) {
        if let Self::Flexible(rate) = self {
            rate.frame_lost(r);
        }
    }

    pub fn max(&self) -> Duration {
        match self {
            Self::Flexible(rate) => rate.peer_ack_delay(),
            Self::Fixed(delay) => *delay,
        }
    }

    pub fn update(&mut self, cwnd: usize, mtu: usize, rtt: Duration) {
        if let Self::Flexible(rate) = self {
            rate.update(cwnd, mtu, rtt);
        }
    }
}

impl Default for PeerAckDelay {
    fn default() -> Self {
        Self::fixed(Duration::from_millis(25))
    }
}
