// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Buffering data to send until it is acked.

use std::cell::RefCell;
use std::cmp::{max, min, Ordering};
use std::collections::{BTreeMap, VecDeque};
use std::convert::TryFrom;
use std::mem;
use std::ops::Add;
use std::rc::Rc;

use indexmap::IndexMap;
use smallvec::SmallVec;

use neqo_common::{qdebug, qerror, qinfo, qtrace, Encoder, Role};

use crate::events::ConnectionEvents;
use crate::fc::SenderFlowControl;
use crate::frame::{Frame, FRAME_TYPE_RESET_STREAM};
use crate::packet::PacketBuilder;
use crate::recovery::{RecoveryToken, StreamRecoveryToken};
use crate::stats::FrameStats;
use crate::stream_id::StreamId;
use crate::tparams::{self, TransportParameters};
use crate::{AppError, Error, Res};

pub const SEND_BUFFER_SIZE: usize = 0x10_0000; // 1 MiB

/// The priority that is assigned to sending data for the stream.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransmissionPriority {
    /// This stream is more important than the functioning of the connection.
    /// Don't use this priority unless the stream really is that important.
    /// A stream at this priority can starve out other connection functions,
    /// including flow control, which could be very bad.
    Critical,
    /// The stream is very important.  Stream data will be written ahead of
    /// some of the less critical connection functions, like path validation,
    /// connection ID management, and session tickets.
    Important,
    /// High priority streams are important, but not enough to disrupt
    /// connection operation.  They go ahead of session tickets though.
    High,
    /// The default priority.
    Normal,
    /// Low priority streams get sent last.
    Low,
}

impl Default for TransmissionPriority {
    fn default() -> Self {
        Self::Normal
    }
}

impl PartialOrd for TransmissionPriority {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for TransmissionPriority {
    fn cmp(&self, other: &Self) -> Ordering {
        if self == other {
            return Ordering::Equal;
        }
        match (self, other) {
            (Self::Critical, _) => Ordering::Greater,
            (_, Self::Critical) => Ordering::Less,
            (Self::Important, _) => Ordering::Greater,
            (_, Self::Important) => Ordering::Less,
            (Self::High, _) => Ordering::Greater,
            (_, Self::High) => Ordering::Less,
            (Self::Normal, _) => Ordering::Greater,
            (_, Self::Normal) => Ordering::Less,
            _ => unreachable!(),
        }
    }
}

impl Add<RetransmissionPriority> for TransmissionPriority {
    type Output = Self;
    fn add(self, rhs: RetransmissionPriority) -> Self::Output {
        match rhs {
            RetransmissionPriority::Fixed(fixed) => fixed,
            RetransmissionPriority::Same => self,
            RetransmissionPriority::Higher => match self {
                Self::Critical => Self::Critical,
                Self::Important | Self::High => Self::Important,
                Self::Normal => Self::High,
                Self::Low => Self::Normal,
            },
            RetransmissionPriority::MuchHigher => match self {
                Self::Critical | Self::Important => Self::Critical,
                Self::High | Self::Normal => Self::Important,
                Self::Low => Self::High,
            },
        }
    }
}

/// If data is lost, this determines the priority that applies to retransmissions
/// of that data.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RetransmissionPriority {
    /// Prioritize retransmission at a fixed priority.
    /// With this, it is possible to prioritize retransmissions lower than transmissions.
    /// Doing that can create a deadlock with flow control which might cause the connection
    /// to stall unless new data stops arriving fast enough that retransmissions can complete.
    Fixed(TransmissionPriority),
    /// Don't increase priority for retransmission.  This is probably not a good idea
    /// as it could mean starving flow control.
    Same,
    /// Increase the priority of retransmissions (the default).
    /// Retransmissions of `Critical` or `Important` aren't elevated at all.
    Higher,
    /// Increase the priority of retransmissions a lot.
    /// This is useful for streams that are particularly exposed to head-of-line blocking.
    MuchHigher,
}

impl Default for RetransmissionPriority {
    fn default() -> Self {
        Self::Higher
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
enum RangeState {
    Sent,
    Acked,
}

/// Track ranges in the stream as sent or acked. Acked implies sent. Not in a
/// range implies needing-to-be-sent, either initially or as a retransmission.
#[derive(Debug, Default, PartialEq)]
struct RangeTracker {
    // offset, (len, RangeState). Use u64 for len because ranges can exceed 32bits.
    used: BTreeMap<u64, (u64, RangeState)>,
}

impl RangeTracker {
    fn highest_offset(&self) -> u64 {
        self.used
            .range(..)
            .next_back()
            .map_or(0, |(k, (v, _))| *k + *v)
    }

    fn acked_from_zero(&self) -> u64 {
        self.used
            .get(&0)
            .filter(|(_, state)| *state == RangeState::Acked)
            .map_or(0, |(v, _)| *v)
    }

    /// Find the first unmarked range. If all are contiguous, this will return
    /// (highest_offset(), None).
    fn first_unmarked_range(&self) -> (u64, Option<u64>) {
        let mut prev_end = 0;

        for (cur_off, (cur_len, _)) in &self.used {
            if prev_end == *cur_off {
                prev_end = cur_off + cur_len;
            } else {
                return (prev_end, Some(cur_off - prev_end));
            }
        }
        (prev_end, None)
    }

    /// Turn one range into a list of subranges that align with existing
    /// ranges.
    /// Check impermissible overlaps in subregions: Sent cannot overwrite Acked.
    //
    // e.g. given N is new and ABC are existing:
    //             NNNNNNNNNNNNNNNN
    //               AAAAA   BBBCCCCC  ...then we want 5 chunks:
    //             1122222333444555
    //
    // but also if we have this:
    //             NNNNNNNNNNNNNNNN
    //           AAAAAAAAAA      BBBB  ...then break existing A and B ranges up:
    //
    //             1111111122222233
    //           aaAAAAAAAA      BBbb
    //
    // Doing all this work up front should make handling each chunk much
    // easier.
    fn chunk_range_on_edges(
        &mut self,
        new_off: u64,
        new_len: u64,
        new_state: RangeState,
    ) -> Vec<(u64, u64, RangeState)> {
        let mut tmp_off = new_off;
        let mut tmp_len = new_len;
        let mut v = Vec::new();

        // cut previous overlapping range if needed
        let prev = self.used.range_mut(..tmp_off).next_back();
        if let Some((prev_off, (prev_len, prev_state))) = prev {
            let prev_state = *prev_state;
            let overlap = (*prev_off + *prev_len).saturating_sub(new_off);
            *prev_len -= overlap;
            if overlap > 0 {
                self.used.insert(new_off, (overlap, prev_state));
            }
        }

        let mut last_existing_remaining = None;
        for (off, (len, state)) in self.used.range(tmp_off..tmp_off + tmp_len) {
            // Create chunk for "overhang" before an existing range
            if tmp_off < *off {
                let sub_len = off - tmp_off;
                v.push((tmp_off, sub_len, new_state));
                tmp_off += sub_len;
                tmp_len -= sub_len;
            }

            // Create chunk to match existing range
            let sub_len = min(*len, tmp_len);
            let remaining_len = len - sub_len;
            if new_state == RangeState::Sent && *state == RangeState::Acked {
                qinfo!(
                    "Attempted to downgrade overlapping range Acked range {}-{} with Sent {}-{}",
                    off,
                    len,
                    new_off,
                    new_len
                );
            } else {
                v.push((tmp_off, sub_len, new_state));
            }
            tmp_off += sub_len;
            tmp_len -= sub_len;

            if remaining_len > 0 {
                last_existing_remaining = Some((*off, sub_len, remaining_len, *state));
            }
        }

        // Maybe break last existing range in two so that a final chunk will
        // have the same length as an existing range entry
        if let Some((off, sub_len, remaining_len, state)) = last_existing_remaining {
            *self.used.get_mut(&off).expect("must be there") = (sub_len, state);
            self.used.insert(off + sub_len, (remaining_len, state));
        }

        // Create final chunk if anything remains of the new range
        if tmp_len > 0 {
            v.push((tmp_off, tmp_len, new_state))
        }

        v
    }

    /// Merge contiguous Acked ranges into the first entry (0). This range may
    /// be dropped from the send buffer.
    fn coalesce_acked_from_zero(&mut self) {
        let acked_range_from_zero = self
            .used
            .get_mut(&0)
            .filter(|(_, state)| *state == RangeState::Acked)
            .map(|(len, _)| *len);

        if let Some(len_from_zero) = acked_range_from_zero {
            let mut to_remove = SmallVec::<[_; 8]>::new();

            let mut new_len_from_zero = len_from_zero;

            // See if there's another Acked range entry contiguous to this one
            while let Some((next_len, _)) = self
                .used
                .get(&new_len_from_zero)
                .filter(|(_, state)| *state == RangeState::Acked)
            {
                to_remove.push(new_len_from_zero);
                new_len_from_zero += *next_len;
            }

            if len_from_zero != new_len_from_zero {
                self.used.get_mut(&0).expect("must be there").0 = new_len_from_zero;
            }

            for val in to_remove {
                self.used.remove(&val);
            }
        }
    }

    fn mark_range(&mut self, off: u64, len: usize, state: RangeState) {
        if len == 0 {
            qinfo!("mark 0-length range at {}", off);
            return;
        }

        let subranges = self.chunk_range_on_edges(off, len as u64, state);

        for (sub_off, sub_len, sub_state) in subranges {
            self.used.insert(sub_off, (sub_len, sub_state));
        }

        self.coalesce_acked_from_zero()
    }

    fn unmark_range(&mut self, off: u64, len: usize) {
        if len == 0 {
            qdebug!("unmark 0-length range at {}", off);
            return;
        }

        let len = u64::try_from(len).unwrap();
        let end_off = off + len;

        let mut to_remove = SmallVec::<[_; 8]>::new();
        let mut to_add = None;

        // Walk backwards through possibly affected existing ranges
        for (cur_off, (cur_len, cur_state)) in self.used.range_mut(..off + len).rev() {
            // Maybe fixup range preceding the removed range
            if *cur_off < off {
                // Check for overlap
                if *cur_off + *cur_len > off {
                    if *cur_state == RangeState::Acked {
                        qdebug!(
                            "Attempted to unmark Acked range {}-{} with unmark_range {}-{}",
                            cur_off,
                            cur_len,
                            off,
                            off + len
                        );
                    } else {
                        *cur_len = off - cur_off;
                    }
                }
                break;
            }

            if *cur_state == RangeState::Acked {
                qdebug!(
                    "Attempted to unmark Acked range {}-{} with unmark_range {}-{}",
                    cur_off,
                    cur_len,
                    off,
                    off + len
                );
                continue;
            }

            // Add a new range for old subrange extending beyond
            // to-be-unmarked range
            let cur_end_off = cur_off + *cur_len;
            if cur_end_off > end_off {
                let new_cur_off = off + len;
                let new_cur_len = cur_end_off - end_off;
                assert_eq!(to_add, None);
                to_add = Some((new_cur_off, new_cur_len, *cur_state));
            }

            to_remove.push(*cur_off);
        }

        for remove_off in to_remove {
            self.used.remove(&remove_off);
        }

        if let Some((new_cur_off, new_cur_len, cur_state)) = to_add {
            self.used.insert(new_cur_off, (new_cur_len, cur_state));
        }
    }

    /// Unmark all sent ranges.
    pub fn unmark_sent(&mut self) {
        self.unmark_range(0, usize::try_from(self.highest_offset()).unwrap());
    }
}

/// Buffer to contain queued bytes and track their state.
#[derive(Debug, Default, PartialEq)]
pub struct TxBuffer {
    retired: u64,           // contig acked bytes, no longer in buffer
    send_buf: VecDeque<u8>, // buffer of not-acked bytes
    ranges: RangeTracker,   // ranges in buffer that have been sent or acked
}

impl TxBuffer {
    pub fn new() -> Self {
        Self::default()
    }

    /// Attempt to add some or all of the passed-in buffer to the TxBuffer.
    pub fn send(&mut self, buf: &[u8]) -> usize {
        let can_buffer = min(SEND_BUFFER_SIZE - self.buffered(), buf.len());
        if can_buffer > 0 {
            self.send_buf.extend(&buf[..can_buffer]);
            assert!(self.send_buf.len() <= SEND_BUFFER_SIZE);
        }
        can_buffer
    }

    pub fn next_bytes(&self) -> Option<(u64, &[u8])> {
        let (start, maybe_len) = self.ranges.first_unmarked_range();

        if start == self.retired + u64::try_from(self.buffered()).unwrap() {
            return None;
        }

        // Convert from ranges-relative-to-zero to
        // ranges-relative-to-buffer-start
        let buff_off = usize::try_from(start - self.retired).unwrap();

        // Deque returns two slices. Create a subslice from whichever
        // one contains the first unmarked data.
        let slc = if buff_off < self.send_buf.as_slices().0.len() {
            &self.send_buf.as_slices().0[buff_off..]
        } else {
            &self.send_buf.as_slices().1[buff_off - self.send_buf.as_slices().0.len()..]
        };

        let len = if let Some(range_len) = maybe_len {
            // Truncate if range crosses deque slices
            min(usize::try_from(range_len).unwrap(), slc.len())
        } else {
            slc.len()
        };

        debug_assert!(len > 0);
        debug_assert!(len <= slc.len());

        Some((start, &slc[..len]))
    }

    pub fn mark_as_sent(&mut self, offset: u64, len: usize) {
        self.ranges.mark_range(offset, len, RangeState::Sent)
    }

    pub fn mark_as_acked(&mut self, offset: u64, len: usize) {
        self.ranges.mark_range(offset, len, RangeState::Acked);

        // We can drop contig acked range from the buffer
        let new_retirable = self.ranges.acked_from_zero() - self.retired;
        debug_assert!(new_retirable <= self.buffered() as u64);
        let keep_len =
            self.buffered() - usize::try_from(new_retirable).expect("should fit in usize");

        // Truncate front
        self.send_buf.rotate_left(self.buffered() - keep_len);
        self.send_buf.truncate(keep_len);

        self.retired += new_retirable;
    }

    pub fn mark_as_lost(&mut self, offset: u64, len: usize) {
        self.ranges.unmark_range(offset, len)
    }

    /// Forget about anything that was marked as sent.
    pub fn unmark_sent(&mut self) {
        self.ranges.unmark_sent();
    }

    fn buffered(&self) -> usize {
        self.send_buf.len()
    }

    fn avail(&self) -> usize {
        SEND_BUFFER_SIZE - self.buffered()
    }

    fn used(&self) -> u64 {
        self.retired + u64::try_from(self.buffered()).unwrap()
    }
}

/// QUIC sending stream states, based on -transport 3.1.
#[derive(Debug)]
pub(crate) enum SendStreamState {
    Ready {
        fc: SenderFlowControl<StreamId>,
        conn_fc: Rc<RefCell<SenderFlowControl<()>>>,
    },
    Send {
        fc: SenderFlowControl<StreamId>,
        conn_fc: Rc<RefCell<SenderFlowControl<()>>>,
        send_buf: TxBuffer,
    },
    // Note: `DataSent` is entered when the stream is closed, not when all data has been
    // sent for the first time.
    DataSent {
        send_buf: TxBuffer,
        fin_sent: bool,
        fin_acked: bool,
    },
    DataRecvd,
    ResetSent {
        err: AppError,
        final_size: u64,
        priority: Option<TransmissionPriority>,
    },
    ResetRecvd,
}

impl SendStreamState {
    fn tx_buf_mut(&mut self) -> Option<&mut TxBuffer> {
        match self {
            Self::Send { send_buf, .. } | Self::DataSent { send_buf, .. } => Some(send_buf),
            Self::Ready { .. }
            | Self::DataRecvd { .. }
            | Self::ResetSent { .. }
            | Self::ResetRecvd => None,
        }
    }

    fn tx_avail(&self) -> usize {
        match self {
            // In Ready, TxBuffer not yet allocated but size is known
            Self::Ready { .. } => SEND_BUFFER_SIZE,
            Self::Send { send_buf, .. } | Self::DataSent { send_buf, .. } => send_buf.avail(),
            Self::DataRecvd { .. } | Self::ResetSent { .. } | Self::ResetRecvd => 0,
        }
    }

    fn name(&self) -> &str {
        match self {
            Self::Ready { .. } => "Ready",
            Self::Send { .. } => "Send",
            Self::DataSent { .. } => "DataSent",
            Self::DataRecvd { .. } => "DataRecvd",
            Self::ResetSent { .. } => "ResetSent",
            Self::ResetRecvd => "ResetRecvd",
        }
    }

    fn transition(&mut self, new_state: Self) {
        qtrace!("SendStream state {} -> {}", self.name(), new_state.name());
        *self = new_state;
    }
}

/// Implement a QUIC send stream.
#[derive(Debug)]
pub struct SendStream {
    stream_id: StreamId,
    state: SendStreamState,
    conn_events: ConnectionEvents,
    priority: TransmissionPriority,
    retransmission_priority: RetransmissionPriority,
    retransmission_offset: u64,
}

impl SendStream {
    pub fn new(
        stream_id: StreamId,
        max_stream_data: u64,
        conn_fc: Rc<RefCell<SenderFlowControl<()>>>,
        conn_events: ConnectionEvents,
    ) -> Self {
        let ss = Self {
            stream_id,
            state: SendStreamState::Ready {
                fc: SenderFlowControl::new(stream_id, max_stream_data),
                conn_fc,
            },
            conn_events,
            priority: TransmissionPriority::default(),
            retransmission_priority: RetransmissionPriority::default(),
            retransmission_offset: 0,
        };
        if ss.avail() > 0 {
            ss.conn_events.send_stream_writable(stream_id);
        }
        ss
    }

    pub fn set_priority(
        &mut self,
        transmission: TransmissionPriority,
        retransmission: RetransmissionPriority,
    ) {
        self.priority = transmission;
        self.retransmission_priority = retransmission;
    }

    /// If all data has been buffered or written, how much was sent.
    pub fn final_size(&self) -> Option<u64> {
        match &self.state {
            SendStreamState::DataSent { send_buf, .. } => Some(send_buf.used()),
            SendStreamState::ResetSent { final_size, .. } => Some(*final_size),
            _ => None,
        }
    }

    /// Return the next range to be sent, if any.
    /// If this is a retransmission, cut off what is sent at the retransmission
    /// offset.
    fn next_bytes(&mut self, retransmission_only: bool) -> Option<(u64, &[u8])> {
        match self.state {
            SendStreamState::Send { ref send_buf, .. } => {
                send_buf.next_bytes().and_then(|(offset, slice)| {
                    if retransmission_only {
                        qtrace!(
                            [self],
                            "next_bytes apply retransmission limit at {}",
                            self.retransmission_offset
                        );
                        if self.retransmission_offset > offset {
                            let len = min(
                                usize::try_from(self.retransmission_offset - offset).unwrap(),
                                slice.len(),
                            );
                            Some((offset, &slice[..len]))
                        } else {
                            None
                        }
                    } else {
                        Some((offset, slice))
                    }
                })
            }
            SendStreamState::DataSent {
                ref send_buf,
                fin_sent,
                ..
            } => {
                let bytes = send_buf.next_bytes();
                if bytes.is_some() {
                    bytes
                } else if fin_sent {
                    None
                } else {
                    // Send empty stream frame with fin set
                    Some((send_buf.used(), &[]))
                }
            }
            SendStreamState::Ready { .. }
            | SendStreamState::DataRecvd { .. }
            | SendStreamState::ResetSent { .. }
            | SendStreamState::ResetRecvd => None,
        }
    }

    /// Calculate how many bytes (length) can fit into available space and whether
    /// the remainder of the space can be filled (or if a length field is needed).
    fn length_and_fill(data_len: usize, space: usize) -> (usize, bool) {
        if data_len >= space {
            // More data than space allows, or an exact fit => fast path.
            qtrace!("SendStream::length_and_fill fill {}", space);
            return (space, true);
        }

        // Estimate size of the length field based on the available space,
        // less 1, which is the worst case.
        let length = min(space.saturating_sub(1), data_len);
        let length_len = Encoder::varint_len(u64::try_from(length).unwrap());
        debug_assert!(length_len <= space); // We don't depend on this being true, but it is true.

        // From here we can always fit `data_len`, but we might as well fill
        // if there is no space for the length field plus another frame.
        let fill = data_len + length_len + PacketBuilder::MINIMUM_FRAME_SIZE > space;
        qtrace!("SendStream::length_and_fill {} fill {}", data_len, fill);
        (data_len, fill)
    }

    /// Maybe write a `STREAM` frame.
    fn write_stream_frame(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        let retransmission = if priority == self.priority {
            false
        } else if priority == self.priority + self.retransmission_priority {
            true
        } else {
            return;
        };

        let id = self.stream_id;
        let final_size = self.final_size();
        if let Some((offset, data)) = self.next_bytes(retransmission) {
            let overhead = 1 // Frame type
                + Encoder::varint_len(id.as_u64())
                + if offset > 0 {
                    Encoder::varint_len(offset)
                } else {
                    0
                };
            if overhead > builder.remaining() {
                qtrace!([self], "write_frame no space for header");
                return;
            }

            let (length, fill) = Self::length_and_fill(data.len(), builder.remaining() - overhead);
            let fin = final_size.map_or(false, |fs| fs == offset + u64::try_from(length).unwrap());
            if length == 0 && !fin {
                qtrace!([self], "write_frame no data, no fin");
                return;
            }

            // Write the stream out.
            builder.encode_varint(Frame::stream_type(fin, offset > 0, fill));
            builder.encode_varint(id.as_u64());
            if offset > 0 {
                builder.encode_varint(offset);
            }
            if fill {
                builder.encode(&data[..length]);
                builder.mark_full();
            } else {
                builder.encode_vvec(&data[..length]);
            }
            debug_assert!(builder.len() <= builder.limit());

            self.mark_as_sent(offset, length, fin);
            tokens.push(RecoveryToken::Stream(StreamRecoveryToken::Stream(
                SendStreamRecoveryToken {
                    id,
                    offset,
                    length,
                    fin,
                },
            )));
            stats.stream += 1;
        }
    }

    pub fn reset_acked(&mut self) {
        match self.state {
            SendStreamState::Ready { .. }
            | SendStreamState::Send { .. }
            | SendStreamState::DataSent { .. }
            | SendStreamState::DataRecvd { .. } => {
                qtrace!([self], "Reset acked while in {} state?", self.state.name())
            }
            SendStreamState::ResetSent { .. } => self.state.transition(SendStreamState::ResetRecvd),
            SendStreamState::ResetRecvd => qtrace!([self], "already in ResetRecvd state"),
        };
    }

    pub fn reset_lost(&mut self) {
        match self.state {
            SendStreamState::ResetSent {
                ref mut priority, ..
            } => {
                *priority = Some(self.priority + self.retransmission_priority);
            }
            SendStreamState::ResetRecvd => (),
            _ => unreachable!(),
        }
    }

    /// Maybe write a `RESET_STREAM` frame.
    pub fn write_reset_frame(
        &mut self,
        p: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> bool {
        if let SendStreamState::ResetSent {
            final_size,
            err,
            ref mut priority,
        } = self.state
        {
            if *priority != Some(p) {
                return false;
            }
            if builder.write_varint_frame(&[
                FRAME_TYPE_RESET_STREAM,
                self.stream_id.as_u64(),
                err,
                final_size,
            ]) {
                tokens.push(RecoveryToken::Stream(StreamRecoveryToken::ResetStream {
                    stream_id: self.stream_id,
                }));
                stats.reset_stream += 1;
                *priority = None;
                true
            } else {
                false
            }
        } else {
            false
        }
    }

    pub fn blocked_lost(&mut self, limit: u64) {
        if let SendStreamState::Ready { fc, .. } | SendStreamState::Send { fc, .. } =
            &mut self.state
        {
            fc.frame_lost(limit);
        } else {
            qtrace!([self], "Ignoring lost STREAM_DATA_BLOCKED({})", limit);
        }
    }

    /// Maybe write a `STREAM_DATA_BLOCKED` frame.
    pub fn write_blocked_frame(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        // Send STREAM_DATA_BLOCKED at normal priority always.
        if priority == self.priority {
            if let SendStreamState::Ready { fc, .. } | SendStreamState::Send { fc, .. } =
                &mut self.state
            {
                fc.write_frames(builder, tokens, stats);
            }
        }
    }

    pub fn mark_as_sent(&mut self, offset: u64, len: usize, fin: bool) {
        if let Some(buf) = self.state.tx_buf_mut() {
            buf.mark_as_sent(offset, len);
            self.send_blocked_if_space_needed(0);
        };

        if fin {
            if let SendStreamState::DataSent { fin_sent, .. } = &mut self.state {
                *fin_sent = true;
            }
        }
    }

    pub fn mark_as_acked(&mut self, offset: u64, len: usize, fin: bool) {
        match self.state {
            SendStreamState::Send {
                ref mut send_buf, ..
            } => {
                send_buf.mark_as_acked(offset, len);
                if self.avail() > 0 {
                    self.conn_events.send_stream_writable(self.stream_id)
                }
            }
            SendStreamState::DataSent {
                ref mut send_buf,
                ref mut fin_acked,
                ..
            } => {
                send_buf.mark_as_acked(offset, len);
                if fin {
                    *fin_acked = true;
                }
                if *fin_acked && send_buf.buffered() == 0 {
                    self.conn_events.send_stream_complete(self.stream_id);
                    self.state.transition(SendStreamState::DataRecvd);
                }
            }
            _ => qtrace!(
                [self],
                "mark_as_acked called from state {}",
                self.state.name()
            ),
        }
    }

    pub fn mark_as_lost(&mut self, offset: u64, len: usize, fin: bool) {
        self.retransmission_offset = max(
            self.retransmission_offset,
            offset + u64::try_from(len).unwrap(),
        );
        qtrace!(
            [self],
            "mark_as_lost retransmission offset={}",
            self.retransmission_offset
        );
        if let Some(buf) = self.state.tx_buf_mut() {
            buf.mark_as_lost(offset, len);
        }

        if fin {
            if let SendStreamState::DataSent {
                fin_sent,
                fin_acked,
                ..
            } = &mut self.state
            {
                *fin_sent = *fin_acked;
            }
        }
    }

    /// Bytes sendable on stream. Constrained by stream credit available,
    /// connection credit available, and space in the tx buffer.
    pub fn avail(&self) -> usize {
        if let SendStreamState::Ready { fc, conn_fc } | SendStreamState::Send { fc, conn_fc, .. } =
            &self.state
        {
            min(
                min(fc.available(), conn_fc.borrow().available()),
                self.state.tx_avail(),
            )
        } else {
            0
        }
    }

    pub fn set_max_stream_data(&mut self, limit: u64) {
        if let SendStreamState::Ready { fc, .. } | SendStreamState::Send { fc, .. } =
            &mut self.state
        {
            let stream_was_blocked = fc.available() == 0;
            fc.update(limit);
            if stream_was_blocked && self.avail() > 0 {
                self.conn_events.send_stream_writable(self.stream_id)
            }
        }
    }

    pub fn is_terminal(&self) -> bool {
        matches!(
            self.state,
            SendStreamState::DataRecvd { .. } | SendStreamState::ResetRecvd
        )
    }

    pub fn send(&mut self, buf: &[u8]) -> Res<usize> {
        self.send_internal(buf, false)
    }

    pub fn send_atomic(&mut self, buf: &[u8]) -> Res<usize> {
        self.send_internal(buf, true)
    }

    fn send_blocked_if_space_needed(&mut self, needed_space: usize) {
        if let SendStreamState::Ready { fc, conn_fc } | SendStreamState::Send { fc, conn_fc, .. } =
            &mut self.state
        {
            if fc.available() <= needed_space {
                fc.blocked();
            }

            if conn_fc.borrow().available() <= needed_space {
                conn_fc.borrow_mut().blocked();
            }
        }
    }

    fn send_internal(&mut self, buf: &[u8], atomic: bool) -> Res<usize> {
        if buf.is_empty() {
            qerror!([self], "zero-length send on stream");
            return Err(Error::InvalidInput);
        }

        if let SendStreamState::Ready { fc, conn_fc } = &mut self.state {
            let owned_fc = mem::replace(fc, SenderFlowControl::new(self.stream_id, 0));
            let owned_conn_fc = Rc::clone(conn_fc);
            self.state.transition(SendStreamState::Send {
                fc: owned_fc,
                conn_fc: owned_conn_fc,
                send_buf: TxBuffer::new(),
            });
        }

        if !matches!(self.state, SendStreamState::Send { .. }) {
            return Err(Error::FinalSizeError);
        }

        let buf = if buf.is_empty() || (self.avail() == 0) {
            return Ok(0);
        } else if self.avail() < buf.len() {
            if atomic {
                self.send_blocked_if_space_needed(buf.len());
                return Ok(0);
            } else {
                &buf[..self.avail()]
            }
        } else {
            buf
        };

        match &mut self.state {
            SendStreamState::Ready { .. } => unreachable!(),
            SendStreamState::Send {
                fc,
                conn_fc,
                send_buf,
            } => {
                let sent = send_buf.send(buf);
                fc.consume(sent);
                conn_fc.borrow_mut().consume(sent);
                Ok(sent)
            }
            _ => Err(Error::FinalSizeError),
        }
    }

    pub fn close(&mut self) {
        match &mut self.state {
            SendStreamState::Ready { .. } => {
                self.state.transition(SendStreamState::DataSent {
                    send_buf: TxBuffer::new(),
                    fin_sent: false,
                    fin_acked: false,
                });
            }
            SendStreamState::Send { send_buf, .. } => {
                let owned_buf = mem::replace(send_buf, TxBuffer::new());
                self.state.transition(SendStreamState::DataSent {
                    send_buf: owned_buf,
                    fin_sent: false,
                    fin_acked: false,
                });
            }
            SendStreamState::DataSent { .. } => qtrace!([self], "already in DataSent state"),
            SendStreamState::DataRecvd { .. } => qtrace!([self], "already in DataRecvd state"),
            SendStreamState::ResetSent { .. } => qtrace!([self], "already in ResetSent state"),
            SendStreamState::ResetRecvd => qtrace!([self], "already in ResetRecvd state"),
        }
    }

    pub fn reset(&mut self, err: AppError) {
        match &self.state {
            SendStreamState::Ready { fc, .. } | SendStreamState::Send { fc, .. } => {
                let final_size = fc.used();
                self.state.transition(SendStreamState::ResetSent {
                    err,
                    final_size,
                    priority: Some(self.priority),
                });
            }
            SendStreamState::DataSent { send_buf, .. } => {
                let final_size = send_buf.used();
                self.state.transition(SendStreamState::ResetSent {
                    err,
                    final_size,
                    priority: Some(self.priority),
                });
            }
            SendStreamState::DataRecvd { .. } => qtrace!([self], "already in DataRecvd state"),
            SendStreamState::ResetSent { .. } => qtrace!([self], "already in ResetSent state"),
            SendStreamState::ResetRecvd => qtrace!([self], "already in ResetRecvd state"),
        };
    }

    #[cfg(test)]
    pub(crate) fn state(&mut self) -> &mut SendStreamState {
        &mut self.state
    }
}

impl ::std::fmt::Display for SendStream {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "SendStream {}", self.stream_id)
    }
}

#[derive(Debug, Default)]
pub(crate) struct SendStreams(IndexMap<StreamId, SendStream>);

impl SendStreams {
    pub fn get(&self, id: StreamId) -> Res<&SendStream> {
        self.0.get(&id).ok_or(Error::InvalidStreamId)
    }

    pub fn get_mut(&mut self, id: StreamId) -> Res<&mut SendStream> {
        self.0.get_mut(&id).ok_or(Error::InvalidStreamId)
    }

    pub fn exists(&self, id: StreamId) -> bool {
        self.0.contains_key(&id)
    }

    pub fn insert(&mut self, id: StreamId, stream: SendStream) {
        self.0.insert(id, stream);
    }

    pub fn acked(&mut self, token: &SendStreamRecoveryToken) {
        if let Some(ss) = self.0.get_mut(&token.id) {
            ss.mark_as_acked(token.offset, token.length, token.fin);
        }
    }

    pub fn reset_acked(&mut self, id: StreamId) {
        if let Some(ss) = self.0.get_mut(&id) {
            ss.reset_acked()
        }
    }

    pub fn lost(&mut self, token: &SendStreamRecoveryToken) {
        if let Some(ss) = self.0.get_mut(&token.id) {
            ss.mark_as_lost(token.offset, token.length, token.fin);
        }
    }

    pub fn reset_lost(&mut self, stream_id: StreamId) {
        if let Some(ss) = self.0.get_mut(&stream_id) {
            ss.reset_lost();
        }
    }

    pub fn blocked_lost(&mut self, stream_id: StreamId, limit: u64) {
        if let Some(ss) = self.0.get_mut(&stream_id) {
            ss.blocked_lost(limit);
        }
    }

    pub fn clear(&mut self) {
        self.0.clear()
    }

    pub fn clear_terminal(&mut self) {
        self.0.retain(|_, stream| !stream.is_terminal())
    }

    pub(crate) fn write_frames(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        qtrace!("write STREAM frames at priority {:?}", priority);
        for stream in self.0.values_mut() {
            if !stream.write_reset_frame(priority, builder, tokens, stats) {
                stream.write_blocked_frame(priority, builder, tokens, stats);
                stream.write_stream_frame(priority, builder, tokens, stats);
            }
        }
    }

    pub fn update_initial_limit(&mut self, remote: &TransportParameters) {
        for (id, ss) in self.0.iter_mut() {
            let limit = if id.is_bidi() {
                assert!(!id.is_remote_initiated(Role::Client));
                remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE)
            } else {
                remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_UNI)
            };
            ss.set_max_stream_data(limit);
        }
    }
}

impl<'a> IntoIterator for &'a mut SendStreams {
    type Item = (&'a StreamId, &'a mut SendStream);
    type IntoIter = indexmap::map::IterMut<'a, StreamId, SendStream>;

    fn into_iter(self) -> indexmap::map::IterMut<'a, StreamId, SendStream> {
        self.0.iter_mut()
    }
}

#[derive(Debug, Clone)]
pub struct SendStreamRecoveryToken {
    pub(crate) id: StreamId,
    offset: u64,
    length: usize,
    fin: bool,
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::events::ConnectionEvent;
    use neqo_common::{event::Provider, hex_with_len, qtrace};

    fn connection_fc(limit: u64) -> Rc<RefCell<SenderFlowControl<()>>> {
        Rc::new(RefCell::new(SenderFlowControl::new((), limit)))
    }

    #[test]
    fn test_mark_range() {
        let mut rt = RangeTracker::default();

        // ranges can go from nothing->Sent if queued for retrans and then
        // acks arrive
        rt.mark_range(5, 5, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 10);
        assert_eq!(rt.acked_from_zero(), 0);
        rt.mark_range(10, 4, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 14);
        assert_eq!(rt.acked_from_zero(), 0);

        rt.mark_range(0, 5, RangeState::Sent);
        assert_eq!(rt.highest_offset(), 14);
        assert_eq!(rt.acked_from_zero(), 0);
        rt.mark_range(0, 5, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 14);
        assert_eq!(rt.acked_from_zero(), 14);

        rt.mark_range(12, 20, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 32);
        assert_eq!(rt.acked_from_zero(), 32);

        // ack the lot
        rt.mark_range(0, 400, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 400);
        assert_eq!(rt.acked_from_zero(), 400);

        // acked trumps sent
        rt.mark_range(0, 200, RangeState::Sent);
        assert_eq!(rt.highest_offset(), 400);
        assert_eq!(rt.acked_from_zero(), 400);
    }

    #[test]
    fn unmark_sent_start() {
        let mut rt = RangeTracker::default();

        rt.mark_range(0, 5, RangeState::Sent);
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 0);

        rt.unmark_sent();
        assert_eq!(rt.highest_offset(), 0);
        assert_eq!(rt.acked_from_zero(), 0);
        assert_eq!(rt.first_unmarked_range(), (0, None));
    }

    #[test]
    fn unmark_sent_middle() {
        let mut rt = RangeTracker::default();

        rt.mark_range(0, 5, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 5);
        rt.mark_range(5, 5, RangeState::Sent);
        assert_eq!(rt.highest_offset(), 10);
        assert_eq!(rt.acked_from_zero(), 5);
        rt.mark_range(10, 5, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 15);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (15, None));

        rt.unmark_sent();
        assert_eq!(rt.highest_offset(), 15);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (5, Some(5)));
    }

    #[test]
    fn unmark_sent_end() {
        let mut rt = RangeTracker::default();

        rt.mark_range(0, 5, RangeState::Acked);
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 5);
        rt.mark_range(5, 5, RangeState::Sent);
        assert_eq!(rt.highest_offset(), 10);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (10, None));

        rt.unmark_sent();
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (5, None));
    }

    #[test]
    fn truncate_front() {
        let mut v = VecDeque::new();
        v.push_back(5);
        v.push_back(6);
        v.push_back(7);
        v.push_front(4usize);

        v.rotate_left(1);
        v.truncate(3);
        assert_eq!(*v.front().unwrap(), 5);
        assert_eq!(*v.back().unwrap(), 7);
    }

    #[test]
    fn test_unmark_range() {
        let mut rt = RangeTracker::default();

        rt.mark_range(5, 5, RangeState::Acked);
        rt.mark_range(10, 5, RangeState::Sent);

        // Should unmark sent but not acked range
        rt.unmark_range(7, 6);

        let res = rt.first_unmarked_range();
        assert_eq!(res, (0, Some(5)));
        assert_eq!(
            rt.used.iter().next().unwrap(),
            (&5, &(5, RangeState::Acked))
        );
        assert_eq!(
            rt.used.iter().nth(1).unwrap(),
            (&13, &(2, RangeState::Sent))
        );
        assert!(rt.used.iter().nth(2).is_none());
        rt.mark_range(0, 5, RangeState::Sent);

        let res = rt.first_unmarked_range();
        assert_eq!(res, (10, Some(3)));
        rt.mark_range(10, 3, RangeState::Sent);

        let res = rt.first_unmarked_range();
        assert_eq!(res, (15, None));
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    fn tx_buffer_next_bytes_1() {
        let mut txb = TxBuffer::new();

        assert_eq!(txb.avail(), SEND_BUFFER_SIZE);

        // Fill the buffer
        assert_eq!(txb.send(&[1; SEND_BUFFER_SIZE * 2]), SEND_BUFFER_SIZE);
        assert!(matches!(txb.next_bytes(),
			 Some((0, x)) if x.len()==SEND_BUFFER_SIZE
			 && x.iter().all(|ch| *ch == 1)));

        // Mark almost all as sent. Get what's left
        let one_byte_from_end = SEND_BUFFER_SIZE as u64 - 1;
        txb.mark_as_sent(0, one_byte_from_end as usize);
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 1
			 && start == one_byte_from_end
			 && x.iter().all(|ch| *ch == 1)));

        // Mark all as sent. Get nothing
        txb.mark_as_sent(0, SEND_BUFFER_SIZE);
        assert!(matches!(txb.next_bytes(), None));

        // Mark as lost. Get it again
        txb.mark_as_lost(one_byte_from_end, 1);
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 1
			 && start == one_byte_from_end
			 && x.iter().all(|ch| *ch == 1)));

        // Mark a larger range lost, including beyond what's in the buffer even.
        // Get a little more
        let five_bytes_from_end = SEND_BUFFER_SIZE as u64 - 5;
        txb.mark_as_lost(five_bytes_from_end, 100);
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 5
			 && start == five_bytes_from_end
			 && x.iter().all(|ch| *ch == 1)));

        // Contig acked range at start means it can be removed from buffer
        // Impl of vecdeque should now result in a split buffer when more data
        // is sent
        txb.mark_as_acked(0, five_bytes_from_end as usize);
        assert_eq!(txb.send(&[2; 30]), 30);
        // Just get 5 even though there is more
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 5
			 && start == five_bytes_from_end
			 && x.iter().all(|ch| *ch == 1)));
        assert_eq!(txb.retired, five_bytes_from_end);
        assert_eq!(txb.buffered(), 35);

        // Marking that bit as sent should let the last contig bit be returned
        // when called again
        txb.mark_as_sent(five_bytes_from_end, 5);
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 30
			 && start == SEND_BUFFER_SIZE as u64
			 && x.iter().all(|ch| *ch == 2)));
    }

    #[test]
    fn tx_buffer_next_bytes_2() {
        let mut txb = TxBuffer::new();

        assert_eq!(txb.avail(), SEND_BUFFER_SIZE);

        // Fill the buffer
        assert_eq!(txb.send(&[1; SEND_BUFFER_SIZE * 2]), SEND_BUFFER_SIZE);
        assert!(matches!(txb.next_bytes(),
			 Some((0, x)) if x.len()==SEND_BUFFER_SIZE
			 && x.iter().all(|ch| *ch == 1)));

        // As above
        let forty_bytes_from_end = SEND_BUFFER_SIZE as u64 - 40;

        txb.mark_as_acked(0, forty_bytes_from_end as usize);
        assert!(matches!(txb.next_bytes(),
                 Some((start, x)) if x.len() == 40
                 && start == forty_bytes_from_end
        ));

        // Valid new data placed in split locations
        assert_eq!(txb.send(&[2; 100]), 100);

        // Mark a little more as sent
        txb.mark_as_sent(forty_bytes_from_end, 10);
        let thirty_bytes_from_end = forty_bytes_from_end + 10;
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 30
			 && start == thirty_bytes_from_end
			 && x.iter().all(|ch| *ch == 1)));

        // Mark a range 'A' in second slice as sent. Should still return the same
        let range_a_start = SEND_BUFFER_SIZE as u64 + 30;
        let range_a_end = range_a_start + 10;
        txb.mark_as_sent(range_a_start, 10);
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 30
			 && start == thirty_bytes_from_end
			 && x.iter().all(|ch| *ch == 1)));

        // Ack entire first slice and into second slice
        let ten_bytes_past_end = SEND_BUFFER_SIZE as u64 + 10;
        txb.mark_as_acked(0, ten_bytes_past_end as usize);

        // Get up to marked range A
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 20
			 && start == ten_bytes_past_end
			 && x.iter().all(|ch| *ch == 2)));

        txb.mark_as_sent(ten_bytes_past_end, 20);

        // Get bit after earlier marked range A
        assert!(matches!(txb.next_bytes(),
			 Some((start, x)) if x.len() == 60
			 && start == range_a_end
			 && x.iter().all(|ch| *ch == 2)));

        // No more bytes.
        txb.mark_as_sent(range_a_end, 60);
        assert!(matches!(txb.next_bytes(), None));
    }

    #[test]
    fn test_stream_tx() {
        let conn_fc = connection_fc(4096);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(4.into(), 1024, Rc::clone(&conn_fc), conn_events);

        let res = s.send(&[4; 100]).unwrap();
        assert_eq!(res, 100);
        s.mark_as_sent(0, 50, false);
        if let SendStreamState::Send { fc, .. } = s.state() {
            assert_eq!(fc.used(), 100);
        } else {
            panic!("unexpected stream state");
        }

        // Should hit stream flow control limit before filling up send buffer
        let res = s.send(&[4; SEND_BUFFER_SIZE]).unwrap();
        assert_eq!(res, 1024 - 100);

        // should do nothing, max stream data already 1024
        s.set_max_stream_data(1024);
        let res = s.send(&[4; SEND_BUFFER_SIZE]).unwrap();
        assert_eq!(res, 0);

        // should now hit the conn flow control (4096)
        s.set_max_stream_data(1_048_576);
        let res = s.send(&[4; SEND_BUFFER_SIZE]).unwrap();
        assert_eq!(res, 3072);

        // should now hit the tx buffer size
        conn_fc.borrow_mut().update(SEND_BUFFER_SIZE as u64);
        let res = s.send(&[4; SEND_BUFFER_SIZE + 100]).unwrap();
        assert_eq!(res, SEND_BUFFER_SIZE - 4096);

        // TODO(agrover@mozilla.com): test ooo acks somehow
        s.mark_as_acked(0, 40, false);
    }

    #[test]
    fn test_tx_buffer_acks() {
        let mut tx = TxBuffer::new();
        assert_eq!(tx.send(&[4; 100]), 100);
        let res = tx.next_bytes().unwrap();
        assert_eq!(res.0, 0);
        assert_eq!(res.1.len(), 100);
        tx.mark_as_sent(0, 100);
        let res = tx.next_bytes();
        assert_eq!(res, None);

        tx.mark_as_acked(0, 100);
        let res = tx.next_bytes();
        assert_eq!(res, None);
    }

    #[test]
    fn send_stream_writable_event_gen() {
        let conn_fc = connection_fc(2);
        let mut conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(4.into(), 0, Rc::clone(&conn_fc), conn_events.clone());

        // Stream is initially blocked (conn:2, stream:0)
        // and will not accept data.
        assert_eq!(s.send(b"hi").unwrap(), 0);

        // increasing to (conn:2, stream:2) will allow 2 bytes, and also
        // generate a SendStreamWritable event.
        s.set_max_stream_data(2);
        let evts = conn_events.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 1);
        assert!(matches!(
            evts[0],
            ConnectionEvent::SendStreamWritable { .. }
        ));
        assert_eq!(s.send(b"hello").unwrap(), 2);

        // increasing to (conn:2, stream:4) will not generate an event or allow
        // sending anything.
        s.set_max_stream_data(4);
        assert_eq!(conn_events.events().count(), 0);
        assert_eq!(s.send(b"hello").unwrap(), 0);

        // Increasing conn max (conn:4, stream:4) will unblock but not emit
        // event b/c that happens in Connection::emit_frame() (tested in
        // connection.rs)
        assert!(conn_fc.borrow_mut().update(4));
        assert_eq!(conn_events.events().count(), 0);
        assert_eq!(s.avail(), 2);
        assert_eq!(s.send(b"hello").unwrap(), 2);

        // No event because still blocked by conn
        s.set_max_stream_data(1_000_000_000);
        assert_eq!(conn_events.events().count(), 0);

        // No event because happens in emit_frame()
        conn_fc.borrow_mut().update(1_000_000_000);
        assert_eq!(conn_events.events().count(), 0);

        // Unblocking both by a large amount will cause avail() to be limited by
        // tx buffer size.
        assert_eq!(s.avail(), SEND_BUFFER_SIZE - 4);

        assert_eq!(
            s.send(&[b'a'; SEND_BUFFER_SIZE]).unwrap(),
            SEND_BUFFER_SIZE - 4
        );

        // No event because still blocked by tx buffer full
        s.set_max_stream_data(2_000_000_000);
        assert_eq!(conn_events.events().count(), 0);
        assert_eq!(s.send(b"hello").unwrap(), 0);
    }

    #[test]
    fn send_stream_writable_event_new_stream() {
        let conn_fc = connection_fc(2);
        let mut conn_events = ConnectionEvents::default();

        let _s = SendStream::new(4.into(), 100, conn_fc, conn_events.clone());

        // Creating a new stream with conn and stream credits should result in
        // an event.
        let evts = conn_events.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 1);
        assert!(matches!(
            evts[0],
            ConnectionEvent::SendStreamWritable { .. }
        ));
    }

    fn as_stream_token(t: &RecoveryToken) -> &SendStreamRecoveryToken {
        if let RecoveryToken::Stream(StreamRecoveryToken::Stream(rt)) = &t {
            rt
        } else {
            panic!();
        }
    }

    #[test]
    // Verify lost frames handle fin properly
    fn send_stream_get_frame_data() {
        let conn_fc = connection_fc(100);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(0.into(), 100, conn_fc, conn_events);
        s.send(&[0; 10]).unwrap();
        s.close();

        let mut ss = SendStreams::default();
        ss.insert(StreamId::from(0), s);

        let mut tokens = Vec::new();
        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);

        // Write a small frame: no fin.
        let written = builder.len();
        builder.set_limit(written + 6);
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 6);
        assert_eq!(tokens.len(), 1);
        let f1_token = tokens.remove(0);
        assert!(!as_stream_token(&f1_token).fin);

        // Write the rest: fin.
        let written = builder.len();
        builder.set_limit(written + 200);
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 10);
        assert_eq!(tokens.len(), 1);
        let f2_token = tokens.remove(0);
        assert!(as_stream_token(&f2_token).fin);

        // Should be no more data to frame.
        let written = builder.len();
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written);
        assert!(tokens.is_empty());

        // Mark frame 1 as lost
        ss.lost(as_stream_token(&f1_token));

        // Next frame should not set fin even though stream has fin but frame
        // does not include end of stream
        let written = builder.len();
        ss.write_frames(
            TransmissionPriority::default() + RetransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 7); // Needs a length this time.
        assert_eq!(tokens.len(), 1);
        let f4_token = tokens.remove(0);
        assert!(!as_stream_token(&f4_token).fin);

        // Mark frame 2 as lost
        ss.lost(as_stream_token(&f2_token));

        // Next frame should set fin because it includes end of stream
        let written = builder.len();
        ss.write_frames(
            TransmissionPriority::default() + RetransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 10);
        assert_eq!(tokens.len(), 1);
        let f5_token = tokens.remove(0);
        assert!(as_stream_token(&f5_token).fin);
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    // Verify lost frames handle fin properly with zero length fin
    fn send_stream_get_frame_zerolength_fin() {
        let conn_fc = connection_fc(100);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(0.into(), 100, conn_fc, conn_events);
        s.send(&[0; 10]).unwrap();

        let mut ss = SendStreams::default();
        ss.insert(StreamId::from(0), s);

        let mut tokens = Vec::new();
        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f1_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f1_token).offset, 0);
        assert_eq!(as_stream_token(&f1_token).length, 10);
        assert!(!as_stream_token(&f1_token).fin);

        // Should be no more data to frame
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert!(tokens.is_empty());

        ss.get_mut(StreamId::from(0)).unwrap().close();

        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f2_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f2_token).offset, 10);
        assert_eq!(as_stream_token(&f2_token).length, 0);
        assert!(as_stream_token(&f2_token).fin);

        // Mark frame 2 as lost
        ss.lost(as_stream_token(&f2_token));

        // Next frame should set fin
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f3_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f3_token).offset, 10);
        assert_eq!(as_stream_token(&f3_token).length, 0);
        assert!(as_stream_token(&f3_token).fin);

        // Mark frame 1 as lost
        ss.lost(as_stream_token(&f1_token));

        // Next frame should set fin and include all data
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f4_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f4_token).offset, 0);
        assert_eq!(as_stream_token(&f4_token).length, 10);
        assert!(as_stream_token(&f4_token).fin);
    }

    #[test]
    fn data_blocked() {
        let conn_fc = connection_fc(5);
        let conn_events = ConnectionEvents::default();

        let stream_id = StreamId::from(4);
        let mut s = SendStream::new(stream_id, 2, Rc::clone(&conn_fc), conn_events);

        // Only two bytes can be sent due to the stream limit.
        assert_eq!(s.send(b"abc").unwrap(), 2);
        assert_eq!(s.next_bytes(false), Some((0, &b"ab"[..])));

        // This doesn't report blocking yet.
        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_blocked_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream_data_blocked, 0);

        // Blocking is reported after sending the last available credit.
        s.mark_as_sent(0, 2, false);
        s.write_blocked_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream_data_blocked, 1);

        // Now increase the stream limit and test the connection limit.
        s.set_max_stream_data(10);

        assert_eq!(s.send(b"abcd").unwrap(), 3);
        assert_eq!(s.next_bytes(false), Some((2, &b"abc"[..])));
        // DATA_BLOCKED is not sent yet.
        conn_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut tokens, &mut stats);
        assert_eq!(stats.data_blocked, 0);

        // DATA_BLOCKED is queued once bytes using all credit are sent.
        s.mark_as_sent(2, 3, false);
        conn_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut tokens, &mut stats);
        assert_eq!(stats.data_blocked, 1);
    }

    #[test]
    fn data_blocked_atomic() {
        let conn_fc = connection_fc(5);
        let conn_events = ConnectionEvents::default();

        let stream_id = StreamId::from(4);
        let mut s = SendStream::new(stream_id, 2, Rc::clone(&conn_fc), conn_events);

        // Stream is initially blocked (conn:5, stream:2)
        // and will not accept atomic write of 3 bytes.
        assert_eq!(s.send_atomic(b"abc").unwrap(), 0);

        // Assert that STREAM_DATA_BLOCKED is sent.
        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_blocked_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream_data_blocked, 1);

        // Assert that a non-atomic write works.
        assert_eq!(s.send(b"abc").unwrap(), 2);
        assert_eq!(s.next_bytes(false), Some((0, &b"ab"[..])));
        s.mark_as_sent(0, 2, false);

        // Set limits to (conn:5, stream:10).
        s.set_max_stream_data(10);

        // An atomic write of 4 bytes exceeds the remaining limit of 3.
        assert_eq!(s.send_atomic(b"abcd").unwrap(), 0);

        // Assert that DATA_BLOCKED is sent.
        conn_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut tokens, &mut stats);
        assert_eq!(stats.data_blocked, 1);

        // Check that a non-atomic write works.
        assert_eq!(s.send(b"abcd").unwrap(), 3);
        assert_eq!(s.next_bytes(false), Some((2, &b"abc"[..])));
        s.mark_as_sent(2, 3, false);

        // Increase limits to (conn:15, stream:15).
        s.set_max_stream_data(15);
        conn_fc.borrow_mut().update(15);

        // Check that atomic writing right up to the limit works.
        assert_eq!(s.send_atomic(b"abcdefghij").unwrap(), 10);
    }

    #[test]
    fn ack_fin_first() {
        const MESSAGE: &[u8] = b"hello";
        let len_u64 = u64::try_from(MESSAGE.len()).unwrap();

        let conn_fc = connection_fc(len_u64);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(StreamId::new(100), 0, conn_fc, conn_events);
        s.set_max_stream_data(len_u64);

        // Send all the data, then the fin.
        let _ = s.send(MESSAGE).unwrap();
        s.mark_as_sent(0, MESSAGE.len(), false);
        s.close();
        s.mark_as_sent(len_u64, 0, true);

        // Ack the fin, then the data.
        s.mark_as_acked(len_u64, 0, true);
        s.mark_as_acked(0, MESSAGE.len(), false);
        assert!(s.is_terminal());
    }

    #[test]
    fn ack_then_lose_fin() {
        const MESSAGE: &[u8] = b"hello";
        let len_u64 = u64::try_from(MESSAGE.len()).unwrap();

        let conn_fc = connection_fc(len_u64);
        let conn_events = ConnectionEvents::default();

        let id = StreamId::new(100);
        let mut s = SendStream::new(id, 0, conn_fc, conn_events);
        s.set_max_stream_data(len_u64);

        // Send all the data, then the fin.
        let _ = s.send(MESSAGE).unwrap();
        s.mark_as_sent(0, MESSAGE.len(), false);
        s.close();
        s.mark_as_sent(len_u64, 0, true);

        // Ack the fin, then mark it lost.
        s.mark_as_acked(len_u64, 0, true);
        s.mark_as_lost(len_u64, 0, true);

        // No frame should be sent here.
        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_stream_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream, 0);
    }

    /// Create a `SendStream` and force it into a state where it believes that
    /// `offset` bytes have already been sent and acknowledged.
    fn stream_with_sent(stream: u64, offset: usize) -> SendStream {
        const MAX_VARINT: u64 = (1 << 62) - 1;

        let conn_fc = connection_fc(MAX_VARINT);
        let mut s = SendStream::new(
            StreamId::from(stream),
            MAX_VARINT,
            conn_fc,
            ConnectionEvents::default(),
        );

        let mut send_buf = TxBuffer::new();
        send_buf.retired = u64::try_from(offset).unwrap();
        send_buf.ranges.mark_range(0, offset, RangeState::Acked);
        let mut fc = SenderFlowControl::new(StreamId::from(stream), MAX_VARINT);
        fc.consume(offset);
        let conn_fc = Rc::new(RefCell::new(SenderFlowControl::new((), MAX_VARINT)));
        s.state = SendStreamState::Send {
            fc,
            conn_fc,
            send_buf,
        };
        s
    }

    fn frame_sent_sid(stream: u64, offset: usize, len: usize, fin: bool, space: usize) -> bool {
        const BUF: &[u8] = &[0x42; 128];

        qtrace!(
            "frame_sent stream={} offset={} len={} fin={}, space={}",
            stream,
            offset,
            len,
            fin,
            space
        );

        let mut s = stream_with_sent(stream, offset);

        // Now write out the proscribed data and maybe close.
        if len > 0 {
            s.send(&BUF[..len]).unwrap();
        }
        if fin {
            s.close();
        }

        let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
        let header_len = builder.len();
        builder.set_limit(header_len + space);

        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_stream_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        qtrace!(
            "STREAM frame: {}",
            hex_with_len(&builder.as_ref()[header_len..])
        );
        stats.stream > 0
    }

    fn frame_sent(offset: usize, len: usize, fin: bool, space: usize) -> bool {
        frame_sent_sid(0, offset, len, fin, space)
    }

    #[test]
    fn stream_frame_empty() {
        // Stream frames with empty data and no fin never work.
        assert!(!frame_sent(10, 0, false, 2));
        assert!(!frame_sent(10, 0, false, 3));
        assert!(!frame_sent(10, 0, false, 4));
        assert!(!frame_sent(10, 0, false, 5));
        assert!(!frame_sent(10, 0, false, 100));

        // Empty data with fin is only a problem if there is no space.
        assert!(!frame_sent(0, 0, true, 1));
        assert!(frame_sent(0, 0, true, 2));
        assert!(!frame_sent(10, 0, true, 2));
        assert!(frame_sent(10, 0, true, 3));
        assert!(frame_sent(10, 0, true, 4));
        assert!(frame_sent(10, 0, true, 5));
        assert!(frame_sent(10, 0, true, 100));
    }

    #[test]
    fn stream_frame_minimum() {
        // Add minimum data
        assert!(!frame_sent(10, 1, false, 3));
        assert!(!frame_sent(10, 1, true, 3));
        assert!(frame_sent(10, 1, false, 4));
        assert!(frame_sent(10, 1, true, 4));
        assert!(frame_sent(10, 1, false, 5));
        assert!(frame_sent(10, 1, true, 5));
        assert!(frame_sent(10, 1, false, 100));
        assert!(frame_sent(10, 1, true, 100));
    }

    #[test]
    fn stream_frame_more() {
        // Try more data
        assert!(!frame_sent(10, 100, false, 3));
        assert!(!frame_sent(10, 100, true, 3));
        assert!(frame_sent(10, 100, false, 4));
        assert!(frame_sent(10, 100, true, 4));
        assert!(frame_sent(10, 100, false, 5));
        assert!(frame_sent(10, 100, true, 5));
        assert!(frame_sent(10, 100, false, 100));
        assert!(frame_sent(10, 100, true, 100));

        assert!(frame_sent(10, 100, false, 1000));
        assert!(frame_sent(10, 100, true, 1000));
    }

    #[test]
    fn stream_frame_big_id() {
        // A value that encodes to the largest varint.
        const BIG: u64 = 1 << 30;
        const BIGSZ: usize = 1 << 30;

        assert!(!frame_sent_sid(BIG, BIGSZ, 0, false, 16));
        assert!(!frame_sent_sid(BIG, BIGSZ, 0, true, 16));
        assert!(!frame_sent_sid(BIG, BIGSZ, 0, false, 17));
        assert!(frame_sent_sid(BIG, BIGSZ, 0, true, 17));
        assert!(!frame_sent_sid(BIG, BIGSZ, 0, false, 18));
        assert!(frame_sent_sid(BIG, BIGSZ, 0, true, 18));

        assert!(!frame_sent_sid(BIG, BIGSZ, 1, false, 17));
        assert!(!frame_sent_sid(BIG, BIGSZ, 1, true, 17));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, false, 18));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, true, 18));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, false, 19));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, true, 19));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, false, 100));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, true, 100));
    }

    fn stream_frame_at_boundary(data: &[u8]) {
        fn send_with_extra_capacity(data: &[u8], extra: usize, expect_full: bool) -> Vec<u8> {
            qtrace!("send_with_extra_capacity {} + {}", data.len(), extra);
            let mut s = stream_with_sent(0, 0);
            s.send(data).unwrap();
            s.close();

            let mut builder = PacketBuilder::short(Encoder::new(), false, &[]);
            let header_len = builder.len();
            // Add 2 for the frame type and stream ID, then add the extra.
            builder.set_limit(header_len + data.len() + 2 + extra);
            let mut tokens = Vec::new();
            let mut stats = FrameStats::default();
            s.write_stream_frame(
                TransmissionPriority::default(),
                &mut builder,
                &mut tokens,
                &mut stats,
            );
            assert_eq!(stats.stream, 1);
            assert_eq!(builder.is_full(), expect_full);
            Vec::from(Encoder::from(builder)).split_off(header_len)
        }

        // The minimum amount of extra space for getting another frame in.
        let mut enc = Encoder::new();
        enc.encode_varint(u64::try_from(data.len()).unwrap());
        let len_buf = Vec::from(enc);
        let minimum_extra = len_buf.len() + PacketBuilder::MINIMUM_FRAME_SIZE;

        // For anything short of the minimum extra, the frame should fill the packet.
        for i in 0..minimum_extra {
            let frame = send_with_extra_capacity(data, i, true);
            let (header, body) = frame.split_at(2);
            assert_eq!(header, &[0b1001, 0]);
            assert_eq!(body, data);
        }

        // Once there is space for another packet AND a length field,
        // then a length will be added.
        let frame = send_with_extra_capacity(data, minimum_extra, false);
        let (header, rest) = frame.split_at(2);
        assert_eq!(header, &[0b1011, 0]);
        let (len, body) = rest.split_at(len_buf.len());
        assert_eq!(len, &len_buf);
        assert_eq!(body, data);
    }

    /// 16383/16384 is an odd boundary in STREAM frame construction.
    /// That is the boundary where a length goes from 2 bytes to 4 bytes.
    /// Test that we correctly add a length field to the frame; and test
    /// that if we don't, then we don't allow other frames to be added.
    #[test]
    fn stream_frame_16384() {
        stream_frame_at_boundary(&[4; 16383]);
        stream_frame_at_boundary(&[4; 16384]);
    }

    /// 63/64 is the other odd boundary.
    #[test]
    fn stream_frame_64() {
        stream_frame_at_boundary(&[2; 63]);
        stream_frame_at_boundary(&[2; 64]);
    }
}
