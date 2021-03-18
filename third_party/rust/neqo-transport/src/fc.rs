// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracks possibly-redundant flow control signals from other code and converts
// into flow control frames needing to be sent to the remote.

use crate::frame::{
    write_varint_frame, FRAME_TYPE_DATA_BLOCKED, FRAME_TYPE_MAX_DATA, FRAME_TYPE_MAX_STREAM_DATA,
    FRAME_TYPE_STREAM_DATA_BLOCKED,
};
use crate::packet::PacketBuilder;
use crate::recovery::RecoveryToken;
use crate::stats::FrameStats;
use crate::stream_id::StreamId;
use crate::Res;

use std::convert::TryFrom;
use std::fmt::Debug;

#[derive(Debug)]
pub struct SenderFlowControl<T>
where
    T: Debug + Sized,
{
    /// The thing that we're counting for.
    subject: T,
    /// The limit.
    limit: u64,
    /// How much of that limit we've used.
    used: u64,
    /// The point at which blocking occurred.  This is updated each time
    /// the sender decides that it is blocked.  It only ever changes
    /// when blocking occurs.  This ensures that blocking at any given limit
    /// is only reported once.
    /// Note: All values are one greater than the corresponding `limit` to
    /// allow distinguishing between blocking at a limit of 0 and no blocking.
    blocked_at: u64,
    /// Whether a blocked frame should be sent.
    blocked_frame: bool,
}

impl<T> SenderFlowControl<T>
where
    T: Debug + Sized,
{
    /// Make a new instance with the initial value and subject.
    pub fn new(subject: T, initial: u64) -> Self {
        Self {
            subject,
            limit: initial,
            used: 0,
            blocked_at: 0,
            blocked_frame: false,
        }
    }

    /// Update the maximum.  Returns `true` if the change was an increase.
    pub fn update(&mut self, limit: u64) -> bool {
        debug_assert!(limit < u64::MAX);
        if limit > self.limit {
            self.limit = limit;
            self.blocked_frame = false;
            true
        } else {
            false
        }
    }

    /// Consume flow control.
    pub fn consume(&mut self, count: usize) {
        let amt = u64::try_from(count).unwrap();
        debug_assert!(self.used + amt <= self.limit);
        self.used += amt;
    }

    /// Get available flow control.
    pub fn available(&self) -> usize {
        usize::try_from(self.limit - self.used).unwrap_or(usize::MAX)
    }

    /// How much data has been written.
    pub fn used(&self) -> u64 {
        self.used
    }

    /// Mark flow control as blocked.
    /// This only does something if the current limit exceeds the last reported blocking limit.
    pub fn blocked(&mut self) {
        if self.limit >= self.blocked_at {
            self.blocked_at = self.limit + 1;
            self.blocked_frame = true;
        }
    }

    /// Return whether a blocking frame needs to be sent.
    /// This is `Some` with the active limit if `blocked` has been called,
    /// if a blocking frame has not been sent (or it has been lost), and
    /// if the blocking condition remains.
    fn blocked_needed(&self) -> Option<u64> {
        if self.blocked_frame && self.limit < self.blocked_at {
            Some(self.blocked_at - 1)
        } else {
            None
        }
    }

    /// Clear the need to send a blocked frame.
    fn blocked_sent(&mut self) {
        self.blocked_frame = false;
    }

    /// Mark a blocked frame as having been lost.
    /// Only send again if value of `self.blocked_at` hasn't increased since sending.
    /// That would imply that the limit has since increased.
    pub fn lost(&mut self, limit: u64) {
        if self.blocked_at == limit + 1 {
            self.blocked_frame = true;
        }
    }
}

impl SenderFlowControl<()> {
    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> Res<()> {
        if let Some(limit) = self.blocked_needed() {
            if write_varint_frame(builder, &[FRAME_TYPE_DATA_BLOCKED, limit])? {
                stats.data_blocked += 1;
                tokens.push(RecoveryToken::DataBlocked(limit));
                self.blocked_sent();
            }
        }
        Ok(())
    }
}

impl SenderFlowControl<StreamId> {
    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> Res<()> {
        if let Some(limit) = self.blocked_needed() {
            if write_varint_frame(
                builder,
                &[FRAME_TYPE_STREAM_DATA_BLOCKED, self.subject.as_u64(), limit],
            )? {
                stats.stream_data_blocked += 1;
                tokens.push(RecoveryToken::StreamDataBlocked {
                    stream_id: self.subject,
                    limit,
                });
                self.blocked_sent();
            }
        }
        Ok(())
    }
}

#[derive(Debug)]
pub struct ReceiverFlowControl<T>
where
    T: Debug + Sized,
{
    /// The thing that we're counting for.
    subject: T,
    /// The maximum amount of items that can be active (e.g., the size of the receive buffer).
    max_active: u64,
    // Last max data sent.
    max_data: u64,
    // Retired bytes.
    retired: u64,
    frame_pending: bool,
}

impl<T> ReceiverFlowControl<T>
where
    T: Debug + Sized,
{
    /// Make a new instance with the initial value and subject.
    pub fn new(subject: T, max_bytes: u64) -> Self {
        Self {
            subject,
            max_active: max_bytes,
            max_data: max_bytes,
            retired: 0,
            frame_pending: false,
        }
    }

    /// Check if received data exceeds the allowed flow control limit.
    pub fn check_allowed(&self, new_end: u64) -> bool {
        new_end <= self.max_data
    }

    /// Some data has been read, retired them and maybe send flow control
    /// update.
    pub fn retired(&mut self, retired: u64) {
        if retired <= self.retired {
            return;
        }

        self.retired = retired;
        if self.retired + self.max_active / 2 > self.max_data {
            self.frame_pending = true;
        }
    }

    /// This function is called when STREAM_DATA_BLOCKED frame is received.
    /// The flow control willl try to send an update if possible.
    pub fn send_flowc_update(&mut self) {
        if self.retired + self.max_active > self.max_data {
            self.frame_pending = true;
        }
    }

    pub fn max_data_needed(&self) -> Option<u64> {
        if self.frame_pending {
            Some(self.retired + self.max_active)
        } else {
            None
        }
    }

    pub fn lost(&mut self, maximum_data: u64) {
        if maximum_data == self.max_data {
            self.frame_pending = true;
        }
    }

    fn max_data_sent(&mut self, new_max: u64) {
        self.max_data = new_max;
        self.frame_pending = false;
    }

    pub fn set_max_active(&mut self, max: u64) {
        // If max_active has been increased, send an update immediately.
        self.frame_pending |= self.max_active < max;
        self.max_active = max;
    }
}

impl ReceiverFlowControl<()> {
    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> Res<()> {
        if let Some(max_data) = self.max_data_needed() {
            if write_varint_frame(builder, &[FRAME_TYPE_MAX_DATA, max_data])? {
                stats.max_data += 1;
                tokens.push(RecoveryToken::MaxData(max_data));
                self.max_data_sent(max_data);
            }
        }
        Ok(())
    }
}

impl ReceiverFlowControl<StreamId> {
    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> Res<()> {
        if let Some(max_data) = self.max_data_needed() {
            if write_varint_frame(
                builder,
                &[FRAME_TYPE_MAX_STREAM_DATA, self.subject.as_u64(), max_data],
            )? {
                stats.max_stream_data += 1;
                tokens.push(RecoveryToken::MaxStreamData {
                    stream_id: self.subject,
                    max_data,
                });
                self.max_data_sent(max_data);
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::{ReceiverFlowControl, SenderFlowControl};

    #[test]
    fn blocked_at_zero() {
        let mut fc = SenderFlowControl::new((), 0);
        fc.blocked();
        assert_eq!(fc.blocked_needed(), Some(0));
    }

    #[test]
    fn blocked() {
        let mut fc = SenderFlowControl::new((), 10);
        fc.blocked();
        assert_eq!(fc.blocked_needed(), Some(10));
    }

    #[test]
    fn update_consume() {
        let mut fc = SenderFlowControl::new((), 10);
        fc.consume(10);
        assert_eq!(fc.available(), 0);
        fc.update(5); // An update lower than the current limit does nothing.
        assert_eq!(fc.available(), 0);
        fc.update(15);
        assert_eq!(fc.available(), 5);
        fc.consume(3);
        assert_eq!(fc.available(), 2);
    }

    #[test]
    fn update_clears_blocked() {
        let mut fc = SenderFlowControl::new((), 10);
        fc.blocked();
        assert_eq!(fc.blocked_needed(), Some(10));
        fc.update(5); // An update lower than the current limit does nothing.
        assert_eq!(fc.blocked_needed(), Some(10));
        fc.update(11);
        assert_eq!(fc.blocked_needed(), None);
    }

    #[test]
    fn lost_blocked_resent() {
        let mut fc = SenderFlowControl::new((), 10);
        fc.blocked();
        fc.blocked_sent();
        assert_eq!(fc.blocked_needed(), None);
        fc.lost(10);
        assert_eq!(fc.blocked_needed(), Some(10));
    }

    #[test]
    fn lost_after_increase() {
        let mut fc = SenderFlowControl::new((), 10);
        fc.blocked();
        fc.blocked_sent();
        assert_eq!(fc.blocked_needed(), None);
        fc.update(11);
        fc.lost(10);
        assert_eq!(fc.blocked_needed(), None);
    }

    #[test]
    fn lost_after_higher_blocked() {
        let mut fc = SenderFlowControl::new((), 10);
        fc.blocked();
        fc.blocked_sent();
        fc.update(11);
        fc.blocked();
        assert_eq!(fc.blocked_needed(), Some(11));
        fc.blocked_sent();
        fc.lost(10);
        assert_eq!(fc.blocked_needed(), None);
    }

    #[test]
    fn do_no_need_max_data_frame_at_start() {
        let fc = ReceiverFlowControl::new((), 0);
        assert_eq!(fc.max_data_needed(), None);
    }

    #[test]
    fn max_data_after_bytes_retired() {
        let mut fc = ReceiverFlowControl::new((), 100);
        fc.retired(49);
        assert_eq!(fc.max_data_needed(), None);
        fc.retired(51);
        assert_eq!(fc.max_data_needed(), Some(151));
    }

    #[test]
    fn need_max_data_frame_after_loss() {
        let mut fc = ReceiverFlowControl::new((), 100);
        fc.retired(100);
        assert_eq!(fc.max_data_needed(), Some(200));
        fc.max_data_sent(200);
        assert_eq!(fc.max_data_needed(), None);
        fc.lost(200);
        assert_eq!(fc.max_data_needed(), Some(200));
    }

    #[test]
    fn no_max_data_frame_after_old_loss() {
        let mut fc = ReceiverFlowControl::new((), 100);
        fc.retired(51);
        assert_eq!(fc.max_data_needed(), Some(151));
        fc.max_data_sent(151);
        assert_eq!(fc.max_data_needed(), None);
        fc.retired(102);
        assert_eq!(fc.max_data_needed(), Some(202));
        fc.max_data_sent(202);
        assert_eq!(fc.max_data_needed(), None);
        fc.lost(151);
        assert_eq!(fc.max_data_needed(), None);
    }

    #[test]
    fn force_send_max_data() {
        let mut fc = ReceiverFlowControl::new((), 100);
        fc.retired(10);
        assert_eq!(fc.max_data_needed(), None);
    }

    #[test]
    fn multiple_retries_after_frame_pending_is_set() {
        let mut fc = ReceiverFlowControl::new((), 100);
        fc.retired(51);
        assert_eq!(fc.max_data_needed(), Some(151));
        fc.retired(61);
        assert_eq!(fc.max_data_needed(), Some(161));
        fc.retired(88);
        assert_eq!(fc.max_data_needed(), Some(188));
        fc.retired(90);
        assert_eq!(fc.max_data_needed(), Some(190));
        fc.max_data_sent(190);
        assert_eq!(fc.max_data_needed(), None);
        fc.retired(141);
        assert_eq!(fc.max_data_needed(), Some(241));
        fc.max_data_sent(241);
        assert_eq!(fc.max_data_needed(), None);
    }

    #[test]
    fn new_retired_before_loss() {
        let mut fc = ReceiverFlowControl::new((), 100);
        fc.retired(51);
        assert_eq!(fc.max_data_needed(), Some(151));
        fc.max_data_sent(151);
        assert_eq!(fc.max_data_needed(), None);
        fc.retired(62);
        assert_eq!(fc.max_data_needed(), None);
        fc.lost(151);
        assert_eq!(fc.max_data_needed(), Some(162));
    }

    #[test]
    fn changing_max_active() {
        let mut fc = ReceiverFlowControl::new((), 100);
        fc.set_max_active(50);
        // There is no MAX_STREAM_DATA frame needed.
        assert_eq!(fc.max_data_needed(), None);
        // We can still retire more than 50.
        fc.retired(60);
        // There is no MAX_STREAM_DATA fame needed yet.
        assert_eq!(fc.max_data_needed(), None);
        fc.retired(76);
        assert_eq!(fc.max_data_needed(), Some(126));

        // Increase max_active.
        fc.set_max_active(60);
        assert_eq!(fc.max_data_needed(), Some(136));

        // We can retire more than 60.
        fc.retired(136);
        assert_eq!(fc.max_data_needed(), Some(196));
    }
}
