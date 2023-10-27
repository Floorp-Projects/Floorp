// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Building a stream of ordered bytes to give the application from a series of
// incoming STREAM frames.

use std::cmp::max;
use std::collections::BTreeMap;
use std::convert::TryFrom;
use std::mem;
use std::rc::{Rc, Weak};

use smallvec::SmallVec;

use crate::events::ConnectionEvents;
use crate::fc::ReceiverFlowControl;
use crate::frame::FRAME_TYPE_STOP_SENDING;
use crate::packet::PacketBuilder;
use crate::recovery::{RecoveryToken, StreamRecoveryToken};
use crate::send_stream::SendStreams;
use crate::stats::FrameStats;
use crate::stream_id::StreamId;
use crate::{AppError, Error, Res};
use neqo_common::{qtrace, Role};
use std::cell::RefCell;

const RX_STREAM_DATA_WINDOW: u64 = 0x10_0000; // 1MiB

// Export as usize for consistency with SEND_BUFFER_SIZE
pub const RECV_BUFFER_SIZE: usize = RX_STREAM_DATA_WINDOW as usize;

#[derive(Debug, Default)]
pub(crate) struct RecvStreams {
    streams: BTreeMap<StreamId, RecvStream>,
    keep_alive: Weak<()>,
}

impl RecvStreams {
    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        for stream in self.streams.values_mut() {
            stream.write_frame(builder, tokens, stats);
            if builder.is_full() {
                return;
            }
        }
    }

    pub fn insert(&mut self, id: StreamId, stream: RecvStream) {
        self.streams.insert(id, stream);
    }

    pub fn get_mut(&mut self, id: StreamId) -> Res<&mut RecvStream> {
        self.streams.get_mut(&id).ok_or(Error::InvalidStreamId)
    }

    pub fn keep_alive(&mut self, id: StreamId, k: bool) -> Res<()> {
        let self_ka = &mut self.keep_alive;
        let s = self.streams.get_mut(&id).ok_or(Error::InvalidStreamId)?;
        s.keep_alive = if k {
            Some(self_ka.upgrade().unwrap_or_else(|| {
                let r = Rc::new(());
                *self_ka = Rc::downgrade(&r);
                r
            }))
        } else {
            None
        };
        Ok(())
    }

    pub fn need_keep_alive(&mut self) -> bool {
        self.keep_alive.strong_count() > 0
    }

    pub fn clear(&mut self) {
        self.streams.clear();
    }

    pub fn clear_terminal(&mut self, send_streams: &SendStreams, role: Role) -> (u64, u64) {
        let recv_to_remove = self
            .streams
            .iter()
            .filter_map(|(id, stream)| {
                // Remove all streams for which the receiving is done (or aborted).
                // But only if they are unidirectional, or we have finished sending.
                if stream.is_terminal() && (id.is_uni() || !send_streams.exists(*id)) {
                    Some(*id)
                } else {
                    None
                }
            })
            .collect::<Vec<_>>();

        let mut removed_bidi = 0;
        let mut removed_uni = 0;
        for id in &recv_to_remove {
            self.streams.remove(id);
            if id.is_remote_initiated(role) {
                if id.is_bidi() {
                    removed_bidi += 1;
                } else {
                    removed_uni += 1;
                }
            }
        }

        (removed_bidi, removed_uni)
    }
}

/// Holds data not yet read by application. Orders and dedupes data ranges
/// from incoming STREAM frames.
#[derive(Debug, Default)]
pub struct RxStreamOrderer {
    data_ranges: BTreeMap<u64, Vec<u8>>, // (start_offset, data)
    retired: u64,                        // Number of bytes the application has read
    received: u64,                       // The number of bytes has stored in `data_ranges`
}

impl RxStreamOrderer {
    pub fn new() -> Self {
        Self::default()
    }

    /// Process an incoming stream frame off the wire. This may result in data
    /// being available to upper layers if frame is not out of order (ooo) or
    /// if the frame fills a gap.
    pub fn inbound_frame(&mut self, mut new_start: u64, mut new_data: &[u8]) {
        qtrace!("Inbound data offset={} len={}", new_start, new_data.len());

        // Get entry before where new entry would go, so we can see if we already
        // have the new bytes.
        // Avoid copies and duplicated data.
        let new_end = new_start + u64::try_from(new_data.len()).unwrap();

        if new_end <= self.retired {
            // Range already read by application, this frame is very late and unneeded.
            return;
        }

        if new_start < self.retired {
            new_data = &new_data[usize::try_from(self.retired - new_start).unwrap()..];
            new_start = self.retired;
        }

        if new_data.is_empty() {
            // No data to insert
            return;
        }

        let extend = if let Some((&prev_start, prev_vec)) =
            self.data_ranges.range_mut(..=new_start).next_back()
        {
            let prev_end = prev_start + u64::try_from(prev_vec.len()).unwrap();
            if new_end > prev_end {
                // PPPPPP    ->  PPPPPP
                //   NNNNNN            NN
                // NNNNNNNN            NN
                // Add a range containing only new data
                // (In-order frames will take this path, with no overlap)
                let overlap = prev_end.saturating_sub(new_start);
                qtrace!(
                    "New frame {}-{} received, overlap: {}",
                    new_start,
                    new_end,
                    overlap
                );
                new_start += overlap;
                new_data = &new_data[usize::try_from(overlap).unwrap()..];
                // If it is small enough, extend the previous buffer.
                // This can't always extend, because otherwise the buffer could end up
                // growing indefinitely without being released.
                prev_vec.len() < 4096 && prev_end == new_start
            } else {
                // PPPPPP    ->  PPPPPP
                //   NNNN
                // NNNN
                // Do nothing
                qtrace!(
                    "Dropping frame with already-received range {}-{}",
                    new_start,
                    new_end
                );
                return;
            }
        } else {
            qtrace!("New frame {}-{} received", new_start, new_end);
            false
        };

        // Now handle possible overlap with next entries
        let mut to_remove = SmallVec::<[_; 8]>::new();
        let mut to_add = new_data;

        for (&next_start, next_data) in self.data_ranges.range_mut(new_start..) {
            let next_end = next_start + u64::try_from(next_data.len()).unwrap();
            let overlap = new_end.saturating_sub(next_start);
            if overlap == 0 {
                break;
            } else if next_end >= new_end {
                qtrace!(
                    "New frame {}-{} overlaps with next frame by {}, truncating",
                    new_start,
                    new_end,
                    overlap
                );
                let truncate_to = new_data.len() - usize::try_from(overlap).unwrap();
                to_add = &new_data[..truncate_to];
                break;
            } else {
                qtrace!(
                    "New frame {}-{} spans entire next frame {}-{}, replacing",
                    new_start,
                    new_end,
                    next_start,
                    next_end
                );
                to_remove.push(next_start);
            }
        }

        for start in to_remove {
            self.data_ranges.remove(&start);
        }

        if !to_add.is_empty() {
            self.received += u64::try_from(to_add.len()).unwrap();
            if extend {
                let (_, buf) = self
                    .data_ranges
                    .range_mut(..=new_start)
                    .next_back()
                    .unwrap();
                buf.extend_from_slice(to_add);
            } else {
                self.data_ranges.insert(new_start, to_add.to_vec());
            }
        }
    }

    /// Are any bytes readable?
    pub fn data_ready(&self) -> bool {
        self.data_ranges
            .keys()
            .next()
            .map_or(false, |&start| start <= self.retired)
    }

    /// How many bytes are readable?
    fn bytes_ready(&self) -> usize {
        let mut prev_end = self.retired;
        self.data_ranges
            .iter()
            .map(|(start_offset, data)| {
                // All ranges don't overlap but we could have partially
                // retired some of the first entry's data.
                let data_len = data.len() as u64 - self.retired.saturating_sub(*start_offset);
                (start_offset, data_len)
            })
            .take_while(|(start_offset, data_len)| {
                if **start_offset <= prev_end {
                    prev_end += data_len;
                    true
                } else {
                    false
                }
            })
            .map(|(_, data_len)| data_len as usize)
            .sum()
    }

    /// Bytes read by the application.
    fn retired(&self) -> u64 {
        self.retired
    }

    fn received(&self) -> u64 {
        self.received
    }

    /// Data bytes buffered. Could be more than bytes_readable if there are
    /// ranges missing.
    fn buffered(&self) -> u64 {
        self.data_ranges
            .iter()
            .map(|(&start, data)| data.len() as u64 - (self.retired.saturating_sub(start)))
            .sum()
    }

    /// Copy received data (if any) into the buffer. Returns bytes copied.
    fn read(&mut self, buf: &mut [u8]) -> usize {
        qtrace!("Reading {} bytes, {} available", buf.len(), self.buffered());
        let mut copied = 0;

        for (&range_start, range_data) in &mut self.data_ranges {
            let mut keep = false;
            if self.retired >= range_start {
                // Frame data has new contiguous bytes.
                let copy_offset =
                    usize::try_from(max(range_start, self.retired) - range_start).unwrap();
                assert!(range_data.len() >= copy_offset);
                let available = range_data.len() - copy_offset;
                let space = buf.len() - copied;
                let copy_bytes = if available > space {
                    keep = true;
                    space
                } else {
                    available
                };

                if copy_bytes > 0 {
                    let copy_slc = &range_data[copy_offset..copy_offset + copy_bytes];
                    buf[copied..copied + copy_bytes].copy_from_slice(copy_slc);
                    copied += copy_bytes;
                    self.retired += u64::try_from(copy_bytes).unwrap();
                }
            } else {
                // The data in the buffer isn't contiguous.
                keep = true;
            }
            if keep {
                let mut keep = self.data_ranges.split_off(&range_start);
                mem::swap(&mut self.data_ranges, &mut keep);
                return copied;
            }
        }

        self.data_ranges.clear();
        copied
    }

    /// Extend the given Vector with any available data.
    pub fn read_to_end(&mut self, buf: &mut Vec<u8>) -> usize {
        let orig_len = buf.len();
        buf.resize(orig_len + self.bytes_ready(), 0);
        self.read(&mut buf[orig_len..])
    }
}

/// QUIC receiving states, based on -transport 3.2.
#[derive(Debug)]
#[allow(dead_code)]
// Because a dead_code warning is easier than clippy::unused_self, see https://github.com/rust-lang/rust/issues/68408
enum RecvStreamState {
    Recv {
        fc: ReceiverFlowControl<StreamId>,
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
        recv_buf: RxStreamOrderer,
    },
    SizeKnown {
        fc: ReceiverFlowControl<StreamId>,
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
        recv_buf: RxStreamOrderer,
    },
    DataRecvd {
        fc: ReceiverFlowControl<StreamId>,
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
        recv_buf: RxStreamOrderer,
    },
    DataRead {
        final_received: u64,
        final_read: u64,
    },
    AbortReading {
        fc: ReceiverFlowControl<StreamId>,
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
        final_size_reached: bool,
        frame_needed: bool,
        err: AppError,
        final_received: u64,
        final_read: u64,
    },
    WaitForReset {
        fc: ReceiverFlowControl<StreamId>,
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
        final_received: u64,
        final_read: u64,
    },
    ResetRecvd {
        final_received: u64,
        final_read: u64,
    },
    // Defined by spec but we don't use it: ResetRead
}

impl RecvStreamState {
    fn new(
        max_bytes: u64,
        stream_id: StreamId,
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
    ) -> Self {
        Self::Recv {
            fc: ReceiverFlowControl::new(stream_id, max_bytes),
            recv_buf: RxStreamOrderer::new(),
            session_fc,
        }
    }

    fn name(&self) -> &str {
        match self {
            Self::Recv { .. } => "Recv",
            Self::SizeKnown { .. } => "SizeKnown",
            Self::DataRecvd { .. } => "DataRecvd",
            Self::DataRead { .. } => "DataRead",
            Self::AbortReading { .. } => "AbortReading",
            Self::WaitForReset { .. } => "WaitForReset",
            Self::ResetRecvd { .. } => "ResetRecvd",
        }
    }

    fn recv_buf(&self) -> Option<&RxStreamOrderer> {
        match self {
            Self::Recv { recv_buf, .. }
            | Self::SizeKnown { recv_buf, .. }
            | Self::DataRecvd { recv_buf, .. } => Some(recv_buf),
            Self::DataRead { .. }
            | Self::AbortReading { .. }
            | Self::WaitForReset { .. }
            | Self::ResetRecvd { .. } => None,
        }
    }

    fn flow_control_consume_data(&mut self, consumed: u64, fin: bool) -> Res<()> {
        let (fc, session_fc, final_size_reached, retire_data) = match self {
            Self::Recv { fc, session_fc, .. } => (fc, session_fc, false, false),
            Self::WaitForReset { fc, session_fc, .. } => (fc, session_fc, false, true),
            Self::SizeKnown { fc, session_fc, .. } | Self::DataRecvd { fc, session_fc, .. } => {
                (fc, session_fc, true, false)
            }
            Self::AbortReading {
                fc,
                session_fc,
                final_size_reached,
                ..
            } => {
                let old_final_size_reached = *final_size_reached;
                *final_size_reached |= fin;
                (fc, session_fc, old_final_size_reached, true)
            }
            Self::DataRead { .. } | Self::ResetRecvd { .. } => {
                return Ok(());
            }
        };

        // Check final size:
        let final_size_ok = match (fin, final_size_reached) {
            (true, true) => consumed == fc.consumed(),
            (false, true) => consumed <= fc.consumed(),
            (true, false) => consumed >= fc.consumed(),
            (false, false) => true,
        };

        if !final_size_ok {
            return Err(Error::FinalSizeError);
        }

        let new_bytes_consumed = fc.set_consumed(consumed)?;
        session_fc.borrow_mut().consume(new_bytes_consumed)?;
        if retire_data {
            // Let's also retire this data since the stream has been aborted
            RecvStream::flow_control_retire_data(fc.consumed() - fc.retired(), fc, session_fc);
        }
        Ok(())
    }
}

// See https://www.w3.org/TR/webtransport/#receive-stream-stats
#[derive(Debug, Clone, Copy)]
pub struct RecvStreamStats {
    // An indicator of progress on how many of the server application’s bytes
    // intended for this stream have been received so far.
    // Only sequential bytes up to, but not including, the first missing byte,
    // are counted. This number can only increase.
    pub bytes_received: u64,
    // The total number of bytes the application has successfully read from this
    // stream. This number can only increase, and is always less than or equal
    // to bytes_received.
    pub bytes_read: u64,
}

impl RecvStreamStats {
    #[must_use]
    pub fn new(bytes_received: u64, bytes_read: u64) -> Self {
        Self {
            bytes_received,
            bytes_read,
        }
    }

    #[must_use]
    pub fn bytes_received(&self) -> u64 {
        self.bytes_received
    }

    #[must_use]
    pub fn bytes_read(&self) -> u64 {
        self.bytes_read
    }
}

/// Implement a QUIC receive stream.
#[derive(Debug)]
pub struct RecvStream {
    stream_id: StreamId,
    state: RecvStreamState,
    conn_events: ConnectionEvents,
    keep_alive: Option<Rc<()>>,
}

impl RecvStream {
    pub fn new(
        stream_id: StreamId,
        max_stream_data: u64,
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
        conn_events: ConnectionEvents,
    ) -> Self {
        Self {
            stream_id,
            state: RecvStreamState::new(max_stream_data, stream_id, session_fc),
            conn_events,
            keep_alive: None,
        }
    }

    fn set_state(&mut self, new_state: RecvStreamState) {
        debug_assert_ne!(
            mem::discriminant(&self.state),
            mem::discriminant(&new_state)
        );
        qtrace!(
            "RecvStream {} state {} -> {}",
            self.stream_id.as_u64(),
            self.state.name(),
            new_state.name()
        );

        match new_state {
            // Receiving all data, or receiving or requesting RESET_STREAM
            // is cause to stop keep-alives.
            RecvStreamState::DataRecvd { .. }
            | RecvStreamState::AbortReading { .. }
            | RecvStreamState::ResetRecvd { .. } => {
                self.keep_alive = None;
            }
            // Once all the data is read, generate an event.
            RecvStreamState::DataRead { .. } => {
                self.conn_events.recv_stream_complete(self.stream_id);
            }
            _ => {}
        }

        self.state = new_state;
    }

    pub fn stats(&self) -> RecvStreamStats {
        match &self.state {
            RecvStreamState::Recv { recv_buf, .. }
            | RecvStreamState::SizeKnown { recv_buf, .. }
            | RecvStreamState::DataRecvd { recv_buf, .. } => {
                let received = recv_buf.received();
                let read = recv_buf.retired();
                RecvStreamStats::new(received, read)
            }
            RecvStreamState::AbortReading {
                final_received,
                final_read,
                ..
            }
            | RecvStreamState::WaitForReset {
                final_received,
                final_read,
                ..
            }
            | RecvStreamState::DataRead {
                final_received,
                final_read,
            }
            | RecvStreamState::ResetRecvd {
                final_received,
                final_read,
            } => {
                let received = *final_received;
                let read = *final_read;
                RecvStreamStats::new(received, read)
            }
        }
    }

    pub fn inbound_stream_frame(&mut self, fin: bool, offset: u64, data: &[u8]) -> Res<()> {
        // We should post a DataReadable event only once when we change from no-data-ready to
        // data-ready. Therefore remember the state before processing a new frame.
        let already_data_ready = self.data_ready();
        let new_end = offset + u64::try_from(data.len()).unwrap();

        self.state.flow_control_consume_data(new_end, fin)?;

        match &mut self.state {
            RecvStreamState::Recv {
                recv_buf,
                fc,
                session_fc,
            } => {
                recv_buf.inbound_frame(offset, data);
                if fin {
                    let all_recv =
                        fc.consumed() == recv_buf.retired() + recv_buf.bytes_ready() as u64;
                    let buf = mem::replace(recv_buf, RxStreamOrderer::new());
                    let fc_copy = mem::take(fc);
                    let session_fc_copy = mem::take(session_fc);
                    if all_recv {
                        self.set_state(RecvStreamState::DataRecvd {
                            fc: fc_copy,
                            session_fc: session_fc_copy,
                            recv_buf: buf,
                        });
                    } else {
                        self.set_state(RecvStreamState::SizeKnown {
                            fc: fc_copy,
                            session_fc: session_fc_copy,
                            recv_buf: buf,
                        });
                    }
                }
            }
            RecvStreamState::SizeKnown {
                recv_buf,
                fc,
                session_fc,
            } => {
                recv_buf.inbound_frame(offset, data);
                if fc.consumed() == recv_buf.retired() + recv_buf.bytes_ready() as u64 {
                    let buf = mem::replace(recv_buf, RxStreamOrderer::new());
                    let fc_copy = mem::take(fc);
                    let session_fc_copy = mem::take(session_fc);
                    self.set_state(RecvStreamState::DataRecvd {
                        fc: fc_copy,
                        session_fc: session_fc_copy,
                        recv_buf: buf,
                    });
                }
            }
            RecvStreamState::DataRecvd { .. }
            | RecvStreamState::DataRead { .. }
            | RecvStreamState::AbortReading { .. }
            | RecvStreamState::WaitForReset { .. }
            | RecvStreamState::ResetRecvd { .. } => {
                qtrace!("data received when we are in state {}", self.state.name())
            }
        }

        if !already_data_ready && (self.data_ready() || self.needs_to_inform_app_about_fin()) {
            self.conn_events.recv_stream_readable(self.stream_id)
        }

        Ok(())
    }

    pub fn reset(&mut self, application_error_code: AppError, final_size: u64) -> Res<()> {
        self.state.flow_control_consume_data(final_size, true)?;
        match &mut self.state {
            RecvStreamState::Recv {
                fc,
                session_fc,
                recv_buf,
            }
            | RecvStreamState::SizeKnown {
                fc,
                session_fc,
                recv_buf,
            } => {
                // make flow control consumes new data that not really exist.
                Self::flow_control_retire_data(final_size - fc.retired(), fc, session_fc);
                self.conn_events
                    .recv_stream_reset(self.stream_id, application_error_code);
                let received = recv_buf.received();
                let read = recv_buf.retired();
                self.set_state(RecvStreamState::ResetRecvd {
                    final_received: received,
                    final_read: read,
                });
            }
            RecvStreamState::AbortReading {
                fc,
                session_fc,
                final_received,
                final_read,
                ..
            }
            | RecvStreamState::WaitForReset {
                fc,
                session_fc,
                final_received,
                final_read,
            } => {
                // make flow control consumes new data that not really exist.
                Self::flow_control_retire_data(final_size - fc.retired(), fc, session_fc);
                self.conn_events
                    .recv_stream_reset(self.stream_id, application_error_code);
                let received = *final_received;
                let read = *final_read;
                self.set_state(RecvStreamState::ResetRecvd {
                    final_received: received,
                    final_read: read,
                });
            }
            _ => {
                // Ignore reset if in DataRecvd, DataRead, or ResetRecvd
            }
        }
        Ok(())
    }

    /// If we should tell the sender they have more credit, return an offset
    fn flow_control_retire_data(
        new_read: u64,
        fc: &mut ReceiverFlowControl<StreamId>,
        session_fc: &mut Rc<RefCell<ReceiverFlowControl<()>>>,
    ) {
        if new_read > 0 {
            fc.add_retired(new_read);
            session_fc.borrow_mut().add_retired(new_read);
        }
    }

    /// Send a flow control update.
    /// This is used when a peer declares that they are blocked.
    /// This sends `MAX_STREAM_DATA` if there is any increase possible.
    pub fn send_flowc_update(&mut self) {
        if let RecvStreamState::Recv { fc, .. } = &mut self.state {
            fc.send_flowc_update();
        }
    }

    pub fn set_stream_max_data(&mut self, max_data: u64) {
        if let RecvStreamState::Recv { fc, .. } = &mut self.state {
            fc.set_max_active(max_data);
        }
    }

    pub fn is_terminal(&self) -> bool {
        matches!(
            self.state,
            RecvStreamState::ResetRecvd { .. } | RecvStreamState::DataRead { .. }
        )
    }

    // App got all data but did not get the fin signal.
    fn needs_to_inform_app_about_fin(&self) -> bool {
        matches!(self.state, RecvStreamState::DataRecvd { .. })
    }

    fn data_ready(&self) -> bool {
        self.state
            .recv_buf()
            .map_or(false, RxStreamOrderer::data_ready)
    }

    /// # Errors
    /// `NoMoreData` if data and fin bit were previously read by the application.
    pub fn read(&mut self, buf: &mut [u8]) -> Res<(usize, bool)> {
        let data_recvd_state = matches!(self.state, RecvStreamState::DataRecvd { .. });
        match &mut self.state {
            RecvStreamState::Recv {
                recv_buf,
                fc,
                session_fc,
            }
            | RecvStreamState::SizeKnown {
                recv_buf,
                fc,
                session_fc,
                ..
            }
            | RecvStreamState::DataRecvd {
                recv_buf,
                fc,
                session_fc,
            } => {
                let bytes_read = recv_buf.read(buf);
                Self::flow_control_retire_data(u64::try_from(bytes_read).unwrap(), fc, session_fc);
                let fin_read = if data_recvd_state {
                    if recv_buf.buffered() == 0 {
                        let received = recv_buf.received();
                        let read = recv_buf.retired();
                        self.set_state(RecvStreamState::DataRead {
                            final_received: received,
                            final_read: read,
                        });
                        true
                    } else {
                        false
                    }
                } else {
                    false
                };
                Ok((bytes_read, fin_read))
            }
            RecvStreamState::DataRead { .. }
            | RecvStreamState::AbortReading { .. }
            | RecvStreamState::WaitForReset { .. }
            | RecvStreamState::ResetRecvd { .. } => Err(Error::NoMoreData),
        }
    }

    pub fn stop_sending(&mut self, err: AppError) {
        qtrace!("stop_sending called when in state {}", self.state.name());
        match &mut self.state {
            RecvStreamState::Recv {
                fc,
                session_fc,
                recv_buf,
            }
            | RecvStreamState::SizeKnown {
                fc,
                session_fc,
                recv_buf,
            } => {
                // Retire data
                Self::flow_control_retire_data(fc.consumed() - fc.retired(), fc, session_fc);
                let fc_copy = mem::take(fc);
                let session_fc_copy = mem::take(session_fc);
                let received = recv_buf.received();
                let read = recv_buf.retired();
                self.set_state(RecvStreamState::AbortReading {
                    fc: fc_copy,
                    session_fc: session_fc_copy,
                    final_size_reached: matches!(self.state, RecvStreamState::SizeKnown { .. }),
                    frame_needed: true,
                    err,
                    final_received: received,
                    final_read: read,
                })
            }
            RecvStreamState::DataRecvd {
                fc,
                session_fc,
                recv_buf,
            } => {
                Self::flow_control_retire_data(fc.consumed() - fc.retired(), fc, session_fc);
                let received = recv_buf.received();
                let read = recv_buf.retired();
                self.set_state(RecvStreamState::DataRead {
                    final_received: received,
                    final_read: read,
                });
            }
            RecvStreamState::DataRead { .. }
            | RecvStreamState::AbortReading { .. }
            | RecvStreamState::WaitForReset { .. }
            | RecvStreamState::ResetRecvd { .. } => {
                // Already in terminal state
            }
        }
    }

    /// Maybe write a `MAX_STREAM_DATA` frame.
    pub fn write_frame(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        match &mut self.state {
            // Maybe send MAX_STREAM_DATA
            RecvStreamState::Recv { fc, .. } => fc.write_frames(builder, tokens, stats),
            // Maybe send STOP_SENDING
            RecvStreamState::AbortReading {
                frame_needed, err, ..
            } => {
                if *frame_needed
                    && builder.write_varint_frame(&[
                        FRAME_TYPE_STOP_SENDING,
                        self.stream_id.as_u64(),
                        *err,
                    ])
                {
                    tokens.push(RecoveryToken::Stream(StreamRecoveryToken::StopSending {
                        stream_id: self.stream_id,
                    }));
                    stats.stop_sending += 1;
                    *frame_needed = false;
                }
            }
            _ => {}
        }
    }

    pub fn max_stream_data_lost(&mut self, maximum_data: u64) {
        if let RecvStreamState::Recv { fc, .. } = &mut self.state {
            fc.frame_lost(maximum_data);
        }
    }

    pub fn stop_sending_lost(&mut self) {
        if let RecvStreamState::AbortReading { frame_needed, .. } = &mut self.state {
            *frame_needed = true;
        }
    }

    pub fn stop_sending_acked(&mut self) {
        if let RecvStreamState::AbortReading {
            fc,
            session_fc,
            final_size_reached,
            final_received,
            final_read,
            ..
        } = &mut self.state
        {
            let received = *final_received;
            let read = *final_read;
            if *final_size_reached {
                // We already know the final_size of the stream therefore we
                // do not need to wait for RESET.
                self.set_state(RecvStreamState::ResetRecvd {
                    final_received: received,
                    final_read: read,
                });
            } else {
                let fc_copy = mem::take(fc);
                let session_fc_copy = mem::take(session_fc);
                self.set_state(RecvStreamState::WaitForReset {
                    fc: fc_copy,
                    session_fc: session_fc_copy,
                    final_received: received,
                    final_read: read,
                });
            }
        }
    }

    #[cfg(test)]
    pub fn has_frames_to_write(&self) -> bool {
        if let RecvStreamState::Recv { fc, .. } = &self.state {
            fc.frame_needed()
        } else {
            false
        }
    }

    #[cfg(test)]
    pub fn fc(&self) -> Option<&ReceiverFlowControl<StreamId>> {
        match &self.state {
            RecvStreamState::Recv { fc, .. }
            | RecvStreamState::SizeKnown { fc, .. }
            | RecvStreamState::DataRecvd { fc, .. }
            | RecvStreamState::AbortReading { fc, .. }
            | RecvStreamState::WaitForReset { fc, .. } => Some(fc),
            _ => None,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use neqo_common::Encoder;
    use std::ops::Range;

    const SESSION_WINDOW: usize = 1024;

    fn recv_ranges(ranges: &[Range<u64>], available: usize) {
        const ZEROES: &[u8] = &[0; 100];
        qtrace!("recv_ranges {:?}", ranges);

        let mut s = RxStreamOrderer::default();
        for r in ranges {
            let data = &ZEROES[..usize::try_from(r.end - r.start).unwrap()];
            s.inbound_frame(r.start, data);
        }

        let mut buf = [0xff; 100];
        let mut total_recvd = 0;
        loop {
            let recvd = s.read(&mut buf[..]);
            qtrace!("recv_ranges read {}", recvd);
            total_recvd += recvd;
            if recvd == 0 {
                assert_eq!(total_recvd, available);
                break;
            }
        }
    }

    #[test]
    #[allow(unknown_lints, clippy::single_range_in_vec_init)] // Because that lint makes no sense here.
    fn recv_noncontiguous() {
        // Non-contiguous with the start, no data available.
        recv_ranges(&[10..20], 0);
    }

    /// Overlaps with the start of a 10..20 range of bytes.
    #[test]
    fn recv_overlap_start() {
        // Overlap the start, with a larger new value.
        // More overlap than not.
        recv_ranges(&[10..20, 4..18, 0..4], 20);
        // Overlap the start, with a larger new value.
        // Less overlap than not.
        recv_ranges(&[10..20, 2..15, 0..2], 20);
        // Overlap the start, with a smaller new value.
        // More overlap than not.
        recv_ranges(&[10..20, 8..14, 0..8], 20);
        // Overlap the start, with a smaller new value.
        // Less overlap than not.
        recv_ranges(&[10..20, 6..13, 0..6], 20);

        // Again with some of the first range split in two.
        recv_ranges(&[10..11, 11..20, 4..18, 0..4], 20);
        recv_ranges(&[10..11, 11..20, 2..15, 0..2], 20);
        recv_ranges(&[10..11, 11..20, 8..14, 0..8], 20);
        recv_ranges(&[10..11, 11..20, 6..13, 0..6], 20);

        // Again with a gap in the first range.
        recv_ranges(&[10..11, 12..20, 4..18, 0..4], 20);
        recv_ranges(&[10..11, 12..20, 2..15, 0..2], 20);
        recv_ranges(&[10..11, 12..20, 8..14, 0..8], 20);
        recv_ranges(&[10..11, 12..20, 6..13, 0..6], 20);
    }

    /// Overlaps with the end of a 10..20 range of bytes.
    #[test]
    fn recv_overlap_end() {
        // Overlap the end, with a larger new value.
        // More overlap than not.
        recv_ranges(&[10..20, 12..25, 0..10], 25);
        // Overlap the end, with a larger new value.
        // Less overlap than not.
        recv_ranges(&[10..20, 17..33, 0..10], 33);
        // Overlap the end, with a smaller new value.
        // More overlap than not.
        recv_ranges(&[10..20, 15..21, 0..10], 21);
        // Overlap the end, with a smaller new value.
        // Less overlap than not.
        recv_ranges(&[10..20, 17..25, 0..10], 25);

        // Again with some of the first range split in two.
        recv_ranges(&[10..19, 19..20, 12..25, 0..10], 25);
        recv_ranges(&[10..19, 19..20, 17..33, 0..10], 33);
        recv_ranges(&[10..19, 19..20, 15..21, 0..10], 21);
        recv_ranges(&[10..19, 19..20, 17..25, 0..10], 25);

        // Again with a gap in the first range.
        recv_ranges(&[10..18, 19..20, 12..25, 0..10], 25);
        recv_ranges(&[10..18, 19..20, 17..33, 0..10], 33);
        recv_ranges(&[10..18, 19..20, 15..21, 0..10], 21);
        recv_ranges(&[10..18, 19..20, 17..25, 0..10], 25);
    }

    /// Complete overlaps with the start of a 10..20 range of bytes.
    #[test]
    fn recv_overlap_complete() {
        // Complete overlap, more at the end.
        recv_ranges(&[10..20, 9..23, 0..9], 23);
        // Complete overlap, more at the start.
        recv_ranges(&[10..20, 3..23, 0..3], 23);
        // Complete overlap, to end.
        recv_ranges(&[10..20, 5..20, 0..5], 20);
        // Complete overlap, from start.
        recv_ranges(&[10..20, 10..27, 0..10], 27);
        // Complete overlap, from 0 and more.
        recv_ranges(&[10..20, 0..23], 23);

        // Again with the first range split in two.
        recv_ranges(&[10..14, 14..20, 9..23, 0..9], 23);
        recv_ranges(&[10..14, 14..20, 3..23, 0..3], 23);
        recv_ranges(&[10..14, 14..20, 5..20, 0..5], 20);
        recv_ranges(&[10..14, 14..20, 10..27, 0..10], 27);
        recv_ranges(&[10..14, 14..20, 0..23], 23);

        // Again with the a gap in the first range.
        recv_ranges(&[10..13, 14..20, 9..23, 0..9], 23);
        recv_ranges(&[10..13, 14..20, 3..23, 0..3], 23);
        recv_ranges(&[10..13, 14..20, 5..20, 0..5], 20);
        recv_ranges(&[10..13, 14..20, 10..27, 0..10], 27);
        recv_ranges(&[10..13, 14..20, 0..23], 23);
    }

    /// An overlap with no new bytes.
    #[test]
    fn recv_overlap_duplicate() {
        recv_ranges(&[10..20, 11..12, 0..10], 20);
        recv_ranges(&[10..20, 10..15, 0..10], 20);
        recv_ranges(&[10..20, 14..20, 0..10], 20);
        // Now with the first range split.
        recv_ranges(&[10..14, 14..20, 10..15, 0..10], 20);
        recv_ranges(&[10..15, 16..20, 21..25, 10..25, 0..10], 25);
    }

    /// Reading exactly one chunk works, when the next chunk starts immediately.
    #[test]
    fn stop_reading_at_chunk() {
        const CHUNK_SIZE: usize = 10;
        const EXTRA_SIZE: usize = 3;
        let mut s = RxStreamOrderer::new();

        // Add three chunks.
        s.inbound_frame(0, &[0; CHUNK_SIZE]);
        let offset = u64::try_from(CHUNK_SIZE).unwrap();
        s.inbound_frame(offset, &[0; EXTRA_SIZE]);
        let offset = u64::try_from(CHUNK_SIZE + EXTRA_SIZE).unwrap();
        s.inbound_frame(offset, &[0; EXTRA_SIZE]);

        // Read, providing only enough space for the first.
        let mut buf = [0; 100];
        let count = s.read(&mut buf[..CHUNK_SIZE]);
        assert_eq!(count, CHUNK_SIZE);
        let count = s.read(&mut buf[..]);
        assert_eq!(count, EXTRA_SIZE * 2);
    }

    #[test]
    fn recv_overlap_while_reading() {
        let mut s = RxStreamOrderer::new();

        // Add a chunk
        s.inbound_frame(0, &[0; 150]);
        assert_eq!(s.data_ranges.get(&0).unwrap().len(), 150);
        // Read, providing only enough space for the first 100.
        let mut buf = [0; 100];
        let count = s.read(&mut buf[..]);
        assert_eq!(count, 100);
        assert_eq!(s.retired, 100);

        // Add a second frame that overlaps.
        // This shouldn't truncate the first frame, as we're already
        // Reading from it.
        s.inbound_frame(120, &[0; 60]);
        assert_eq!(s.data_ranges.get(&0).unwrap().len(), 180);
        // Read second part of first frame and all of the second frame
        let count = s.read(&mut buf[..]);
        assert_eq!(count, 80);
    }

    /// Reading exactly one chunk works, when there is a gap.
    #[test]
    fn stop_reading_at_gap() {
        const CHUNK_SIZE: usize = 10;
        const EXTRA_SIZE: usize = 3;
        let mut s = RxStreamOrderer::new();

        // Add three chunks.
        s.inbound_frame(0, &[0; CHUNK_SIZE]);
        let offset = u64::try_from(CHUNK_SIZE + EXTRA_SIZE).unwrap();
        s.inbound_frame(offset, &[0; EXTRA_SIZE]);

        // Read, providing only enough space for the first chunk.
        let mut buf = [0; 100];
        let count = s.read(&mut buf[..CHUNK_SIZE]);
        assert_eq!(count, CHUNK_SIZE);

        // Now fill the gap and ensure that everything can be read.
        let offset = u64::try_from(CHUNK_SIZE).unwrap();
        s.inbound_frame(offset, &[0; EXTRA_SIZE]);
        let count = s.read(&mut buf[..]);
        assert_eq!(count, EXTRA_SIZE * 2);
    }

    /// Reading exactly one chunk works, when there is a gap.
    #[test]
    fn stop_reading_in_chunk() {
        const CHUNK_SIZE: usize = 10;
        const EXTRA_SIZE: usize = 3;
        let mut s = RxStreamOrderer::new();

        // Add two chunks.
        s.inbound_frame(0, &[0; CHUNK_SIZE]);
        let offset = u64::try_from(CHUNK_SIZE).unwrap();
        s.inbound_frame(offset, &[0; EXTRA_SIZE]);

        // Read, providing only enough space for some of the first chunk.
        let mut buf = [0; 100];
        let count = s.read(&mut buf[..CHUNK_SIZE - EXTRA_SIZE]);
        assert_eq!(count, CHUNK_SIZE - EXTRA_SIZE);

        let count = s.read(&mut buf[..]);
        assert_eq!(count, EXTRA_SIZE * 2);
    }

    /// Read one byte at a time.
    #[test]
    fn read_byte_at_a_time() {
        const CHUNK_SIZE: usize = 10;
        const EXTRA_SIZE: usize = 3;
        let mut s = RxStreamOrderer::new();

        // Add two chunks.
        s.inbound_frame(0, &[0; CHUNK_SIZE]);
        let offset = u64::try_from(CHUNK_SIZE).unwrap();
        s.inbound_frame(offset, &[0; EXTRA_SIZE]);

        let mut buf = [0; 1];
        for _ in 0..CHUNK_SIZE + EXTRA_SIZE {
            let count = s.read(&mut buf[..]);
            assert_eq!(count, 1);
        }
        assert_eq!(0, s.read(&mut buf[..]));
    }

    fn check_stats(stream: &RecvStream, expected_received: u64, expected_read: u64) {
        let stream_stats = stream.stats();
        assert_eq!(expected_received, stream_stats.bytes_received());
        assert_eq!(expected_read, stream_stats.bytes_read());
    }

    #[test]
    fn stream_rx() {
        let conn_events = ConnectionEvents::default();

        let mut s = RecvStream::new(
            StreamId::from(567),
            1024,
            Rc::new(RefCell::new(ReceiverFlowControl::new((), 1024 * 1024))),
            conn_events,
        );

        // test receiving a contig frame and reading it works
        s.inbound_stream_frame(false, 0, &[1; 10]).unwrap();
        assert!(s.data_ready());
        check_stats(&s, 10, 0);

        let mut buf = vec![0u8; 100];
        assert_eq!(s.read(&mut buf).unwrap(), (10, false));
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 0);

        check_stats(&s, 10, 10);

        // test receiving a noncontig frame
        s.inbound_stream_frame(false, 12, &[2; 12]).unwrap();
        assert!(!s.data_ready());
        assert_eq!(s.read(&mut buf).unwrap(), (0, false));
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 12);

        check_stats(&s, 22, 10);

        // another frame that overlaps the first
        s.inbound_stream_frame(false, 14, &[3; 8]).unwrap();
        assert!(!s.data_ready());
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 12);

        check_stats(&s, 22, 10);

        // fill in the gap, but with a FIN
        s.inbound_stream_frame(true, 10, &[4; 6]).unwrap_err();
        assert!(!s.data_ready());
        assert_eq!(s.read(&mut buf).unwrap(), (0, false));
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 12);

        check_stats(&s, 22, 10);

        // fill in the gap
        s.inbound_stream_frame(false, 10, &[5; 10]).unwrap();
        assert!(s.data_ready());
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 14);

        check_stats(&s, 24, 10);

        // a legit FIN
        s.inbound_stream_frame(true, 24, &[6; 18]).unwrap();
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 32);
        assert!(s.data_ready());
        assert_eq!(s.read(&mut buf).unwrap(), (32, true));

        check_stats(&s, 42, 42);

        // Stream now no longer readable (is in DataRead state)
        s.read(&mut buf).unwrap_err();
    }

    fn check_chunks(s: &mut RxStreamOrderer, expected: &[(u64, usize)]) {
        assert_eq!(s.data_ranges.len(), expected.len());
        for ((start, buf), (expected_start, expected_len)) in s.data_ranges.iter().zip(expected) {
            assert_eq!((*start, buf.len()), (*expected_start, *expected_len));
        }
    }

    // Test deduplication when the new data is at the end.
    #[test]
    fn stream_rx_dedupe_tail() {
        let mut s = RxStreamOrderer::new();

        s.inbound_frame(0, &[1; 6]);
        check_chunks(&mut s, &[(0, 6)]);

        // New data that overlaps entirely (starting from the head), is ignored.
        s.inbound_frame(0, &[2; 3]);
        check_chunks(&mut s, &[(0, 6)]);

        // New data that overlaps at the tail has any new data appended.
        s.inbound_frame(2, &[3; 6]);
        check_chunks(&mut s, &[(0, 8)]);

        // New data that overlaps entirely (up to the tail), is ignored.
        s.inbound_frame(4, &[4; 4]);
        check_chunks(&mut s, &[(0, 8)]);

        // New data that overlaps, starting from the beginning is appended too.
        s.inbound_frame(0, &[5; 10]);
        check_chunks(&mut s, &[(0, 10)]);

        // New data that is entirely subsumed is ignored.
        s.inbound_frame(2, &[6; 2]);
        check_chunks(&mut s, &[(0, 10)]);

        let mut buf = [0; 16];
        assert_eq!(s.read(&mut buf[..]), 10);
        assert_eq!(buf[..10], [1, 1, 1, 1, 1, 1, 3, 3, 5, 5]);
    }

    /// When chunks are added before existing data, they aren't merged.
    #[test]
    fn stream_rx_dedupe_head() {
        let mut s = RxStreamOrderer::new();

        s.inbound_frame(1, &[6; 6]);
        check_chunks(&mut s, &[(1, 6)]);

        // Insertion before an existing chunk causes truncation of the new chunk.
        s.inbound_frame(0, &[7; 6]);
        check_chunks(&mut s, &[(0, 1), (1, 6)]);

        // Perfect overlap with existing slices has no effect.
        s.inbound_frame(0, &[8; 7]);
        check_chunks(&mut s, &[(0, 1), (1, 6)]);

        let mut buf = [0; 16];
        assert_eq!(s.read(&mut buf[..]), 7);
        assert_eq!(buf[..7], [7, 6, 6, 6, 6, 6, 6]);
    }

    #[test]
    fn stream_rx_dedupe_new_tail() {
        let mut s = RxStreamOrderer::new();

        s.inbound_frame(1, &[6; 6]);
        check_chunks(&mut s, &[(1, 6)]);

        // Insertion before an existing chunk causes truncation of the new chunk.
        s.inbound_frame(0, &[7; 6]);
        check_chunks(&mut s, &[(0, 1), (1, 6)]);

        // New data at the end causes the tail to be added to the first chunk,
        // replacing later chunks entirely.
        s.inbound_frame(0, &[9; 8]);
        check_chunks(&mut s, &[(0, 8)]);

        let mut buf = [0; 16];
        assert_eq!(s.read(&mut buf[..]), 8);
        assert_eq!(buf[..8], [7, 9, 9, 9, 9, 9, 9, 9]);
    }

    #[test]
    fn stream_rx_dedupe_replace() {
        let mut s = RxStreamOrderer::new();

        s.inbound_frame(2, &[6; 6]);
        check_chunks(&mut s, &[(2, 6)]);

        // Insertion before an existing chunk causes truncation of the new chunk.
        s.inbound_frame(1, &[7; 6]);
        check_chunks(&mut s, &[(1, 1), (2, 6)]);

        // New data at the start and end replaces all the slices.
        s.inbound_frame(0, &[9; 10]);
        check_chunks(&mut s, &[(0, 10)]);

        let mut buf = [0; 16];
        assert_eq!(s.read(&mut buf[..]), 10);
        assert_eq!(buf[..10], [9; 10]);
    }

    #[test]
    fn trim_retired() {
        let mut s = RxStreamOrderer::new();

        let mut buf = [0; 18];
        s.inbound_frame(0, &[1; 10]);

        // Partially read slices are retained.
        assert_eq!(s.read(&mut buf[..6]), 6);
        check_chunks(&mut s, &[(0, 10)]);

        // Partially read slices are kept and so are added to.
        s.inbound_frame(3, &buf[..10]);
        check_chunks(&mut s, &[(0, 13)]);

        // Wholly read pieces are dropped.
        assert_eq!(s.read(&mut buf[..]), 7);
        assert!(s.data_ranges.is_empty());

        // New data that overlaps with retired data is trimmed.
        s.inbound_frame(0, &buf[..]);
        check_chunks(&mut s, &[(13, 5)]);
    }

    #[test]
    fn stream_flowc_update() {
        let mut s = create_stream(1024 * RX_STREAM_DATA_WINDOW);
        let mut buf = vec![0u8; RECV_BUFFER_SIZE + 100]; // Make it overlarge

        assert!(!s.has_frames_to_write());
        s.inbound_stream_frame(false, 0, &[0; RECV_BUFFER_SIZE])
            .unwrap();
        assert!(!s.has_frames_to_write());
        assert_eq!(s.read(&mut buf).unwrap(), (RECV_BUFFER_SIZE, false));
        assert!(!s.data_ready());

        // flow msg generated!
        assert!(s.has_frames_to_write());

        // consume it
        let mut builder = PacketBuilder::short(Encoder::new(), false, []);
        let mut token = Vec::new();
        s.write_frame(&mut builder, &mut token, &mut FrameStats::default());

        // it should be gone
        assert!(!s.has_frames_to_write());
    }

    fn create_stream(session_fc: u64) -> RecvStream {
        let conn_events = ConnectionEvents::default();
        RecvStream::new(
            StreamId::from(67),
            RX_STREAM_DATA_WINDOW,
            Rc::new(RefCell::new(ReceiverFlowControl::new((), session_fc))),
            conn_events,
        )
    }

    #[test]
    fn stream_max_stream_data() {
        let mut s = create_stream(1024 * RX_STREAM_DATA_WINDOW);
        assert!(!s.has_frames_to_write());
        s.inbound_stream_frame(false, 0, &[0; RECV_BUFFER_SIZE])
            .unwrap();
        s.inbound_stream_frame(false, RX_STREAM_DATA_WINDOW, &[1; 1])
            .unwrap_err();
    }

    #[test]
    fn stream_orderer_bytes_ready() {
        let mut rx_ord = RxStreamOrderer::new();

        rx_ord.inbound_frame(0, &[1; 6]);
        assert_eq!(rx_ord.bytes_ready(), 6);
        assert_eq!(rx_ord.buffered(), 6);
        assert_eq!(rx_ord.retired(), 0);

        // read some so there's an offset into the first frame
        let mut buf = [0u8; 10];
        rx_ord.read(&mut buf[..2]);
        assert_eq!(rx_ord.bytes_ready(), 4);
        assert_eq!(rx_ord.buffered(), 4);
        assert_eq!(rx_ord.retired(), 2);

        // an overlapping frame
        rx_ord.inbound_frame(5, &[2; 6]);
        assert_eq!(rx_ord.bytes_ready(), 9);
        assert_eq!(rx_ord.buffered(), 9);
        assert_eq!(rx_ord.retired(), 2);

        // a noncontig frame
        rx_ord.inbound_frame(20, &[3; 6]);
        assert_eq!(rx_ord.bytes_ready(), 9);
        assert_eq!(rx_ord.buffered(), 15);
        assert_eq!(rx_ord.retired(), 2);

        // an old frame
        rx_ord.inbound_frame(0, &[4; 2]);
        assert_eq!(rx_ord.bytes_ready(), 9);
        assert_eq!(rx_ord.buffered(), 15);
        assert_eq!(rx_ord.retired(), 2);
    }

    #[test]
    fn no_stream_flowc_event_after_exiting_recv() {
        let mut s = create_stream(1024 * RX_STREAM_DATA_WINDOW);
        s.inbound_stream_frame(false, 0, &[0; RECV_BUFFER_SIZE])
            .unwrap();
        let mut buf = [0; RECV_BUFFER_SIZE];
        s.read(&mut buf).unwrap();
        assert!(s.has_frames_to_write());
        s.inbound_stream_frame(true, RX_STREAM_DATA_WINDOW, &[])
            .unwrap();
        assert!(!s.has_frames_to_write());
    }

    fn create_stream_with_fc(
        session_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
        fc_limit: u64,
    ) -> RecvStream {
        RecvStream::new(
            StreamId::from(567),
            fc_limit,
            session_fc,
            ConnectionEvents::default(),
        )
    }

    fn create_stream_session_flow_control() -> (RecvStream, Rc<RefCell<ReceiverFlowControl<()>>>) {
        assert!(RX_STREAM_DATA_WINDOW > u64::try_from(SESSION_WINDOW).unwrap());
        let session_fc = Rc::new(RefCell::new(ReceiverFlowControl::new(
            (),
            u64::try_from(SESSION_WINDOW).unwrap(),
        )));
        (
            create_stream_with_fc(Rc::clone(&session_fc), RX_STREAM_DATA_WINDOW),
            session_fc,
        )
    }

    #[test]
    fn session_flow_control() {
        let (mut s, session_fc) = create_stream_session_flow_control();

        s.inbound_stream_frame(false, 0, &[0; SESSION_WINDOW])
            .unwrap();
        assert!(!session_fc.borrow().frame_needed());
        // The buffer is big enough to hold SESSION_WINDOW, this will make sure that we always
        // read everything from he stream.
        let mut buf = [0; 2 * SESSION_WINDOW];
        s.read(&mut buf).unwrap();
        assert!(session_fc.borrow().frame_needed());
        // consume it
        let mut builder = PacketBuilder::short(Encoder::new(), false, []);
        let mut token = Vec::new();
        session_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut token, &mut FrameStats::default());

        // Switch to SizeKnown state
        s.inbound_stream_frame(true, 2 * u64::try_from(SESSION_WINDOW).unwrap() - 1, &[0])
            .unwrap();
        assert!(!session_fc.borrow().frame_needed());
        // Receive new data that can be read.
        s.inbound_stream_frame(
            false,
            u64::try_from(SESSION_WINDOW).unwrap(),
            &[0; SESSION_WINDOW / 2 + 1],
        )
        .unwrap();
        assert!(!session_fc.borrow().frame_needed());
        s.read(&mut buf).unwrap();
        assert!(session_fc.borrow().frame_needed());
        // consume it
        let mut builder = PacketBuilder::short(Encoder::new(), false, []);
        let mut token = Vec::new();
        session_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut token, &mut FrameStats::default());

        // Test DataRecvd state
        let session_fc = Rc::new(RefCell::new(ReceiverFlowControl::new(
            (),
            u64::try_from(SESSION_WINDOW).unwrap(),
        )));
        let mut s = RecvStream::new(
            StreamId::from(567),
            RX_STREAM_DATA_WINDOW,
            Rc::clone(&session_fc),
            ConnectionEvents::default(),
        );

        s.inbound_stream_frame(true, 0, &[0; SESSION_WINDOW])
            .unwrap();
        assert!(!session_fc.borrow().frame_needed());
        s.read(&mut buf).unwrap();
        assert!(session_fc.borrow().frame_needed());
    }

    #[test]
    fn session_flow_control_reset() {
        let (mut s, session_fc) = create_stream_session_flow_control();

        s.inbound_stream_frame(false, 0, &[0; SESSION_WINDOW / 2])
            .unwrap();
        assert!(!session_fc.borrow().frame_needed());

        s.reset(
            Error::NoError.code(),
            u64::try_from(SESSION_WINDOW).unwrap(),
        )
        .unwrap();
        assert!(session_fc.borrow().frame_needed());
    }

    fn check_fc<T: std::fmt::Debug>(fc: &ReceiverFlowControl<T>, consumed: u64, retired: u64) {
        assert_eq!(fc.consumed(), consumed);
        assert_eq!(fc.retired(), retired);
    }

    /// Test consuming the flow control in RecvStreamState::Recv
    #[test]
    fn fc_state_recv_1() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));
        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);

        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(false, 0, &[0; SW_US / 4]).unwrap();

        check_fc(&fc.borrow(), SW / 4, 0);
        check_fc(s.fc().unwrap(), SW / 4, 0);
    }

    /// Test consuming the flow control in RecvStreamState::Recv
    /// with multiple streams
    #[test]
    fn fc_state_recv_2() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));
        let mut s1 = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);
        let mut s2 = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);

        check_fc(&fc.borrow(), 0, 0);
        check_fc(s1.fc().unwrap(), 0, 0);
        check_fc(s2.fc().unwrap(), 0, 0);

        s1.inbound_stream_frame(false, 0, &[0; SW_US / 4]).unwrap();

        check_fc(&fc.borrow(), SW / 4, 0);
        check_fc(s1.fc().unwrap(), SW / 4, 0);
        check_fc(s2.fc().unwrap(), 0, 0);

        s2.inbound_stream_frame(false, 0, &[0; SW_US / 4]).unwrap();

        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s1.fc().unwrap(), SW / 4, 0);
        check_fc(s2.fc().unwrap(), SW / 4, 0);
    }

    /// Test retiring the flow control in RecvStreamState::Recv
    /// with multiple streams
    #[test]
    fn fc_state_recv_3() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));
        let mut s1 = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);
        let mut s2 = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);

        check_fc(&fc.borrow(), 0, 0);
        check_fc(s1.fc().unwrap(), 0, 0);
        check_fc(s2.fc().unwrap(), 0, 0);

        s1.inbound_stream_frame(false, 0, &[0; SW_US / 4]).unwrap();
        s2.inbound_stream_frame(false, 0, &[0; SW_US / 4]).unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s1.fc().unwrap(), SW / 4, 0);
        check_fc(s2.fc().unwrap(), SW / 4, 0);

        // Read data
        let mut buf = [1; SW_US];
        assert_eq!(s1.read(&mut buf).unwrap(), (SW_US / 4, false));
        check_fc(&fc.borrow(), SW / 2, SW / 4);
        check_fc(s1.fc().unwrap(), SW / 4, SW / 4);
        check_fc(s2.fc().unwrap(), SW / 4, 0);

        assert_eq!(s2.read(&mut buf).unwrap(), (SW_US / 4, false));
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s1.fc().unwrap(), SW / 4, SW / 4);
        check_fc(s2.fc().unwrap(), SW / 4, SW / 4);

        // Read when there is no more date to be read will not change fc.
        assert_eq!(s1.read(&mut buf).unwrap(), (0, false));
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s1.fc().unwrap(), SW / 4, SW / 4);
        check_fc(s2.fc().unwrap(), SW / 4, SW / 4);

        // Receiving more data on a stream.
        s1.inbound_stream_frame(false, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW * 3 / 4, SW / 2);
        check_fc(s1.fc().unwrap(), SW / 2, SW / 4);
        check_fc(s2.fc().unwrap(), SW / 4, SW / 4);

        // Read data
        assert_eq!(s1.read(&mut buf).unwrap(), (SW_US / 4, false));
        check_fc(&fc.borrow(), SW * 3 / 4, SW * 3 / 4);
        check_fc(s1.fc().unwrap(), SW / 2, SW / 2);
        check_fc(s2.fc().unwrap(), SW / 4, SW / 4);
    }

    /// Test consuming the flow control in RecvStreamState::Recv - duplicate data
    #[test]
    fn fc_state_recv_4() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));
        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);

        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(false, 0, &[0; SW_US / 4]).unwrap();

        check_fc(&fc.borrow(), SW / 4, 0);
        check_fc(s.fc().unwrap(), SW / 4, 0);

        // Receiving duplicate frames (already consumed data) will not cause an error or
        // change fc.
        s.inbound_stream_frame(false, 0, &[0; SW_US / 8]).unwrap();
        check_fc(&fc.borrow(), SW / 4, 0);
        check_fc(s.fc().unwrap(), SW / 4, 0);
    }

    /// Test consuming the flow control in RecvStreamState::Recv - filling a gap in the
    /// data stream.
    #[test]
    fn fc_state_recv_5() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));
        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);

        // Receive out of order data.
        s.inbound_stream_frame(false, SW / 8, &[0; SW_US / 8])
            .unwrap();
        check_fc(&fc.borrow(), SW / 4, 0);
        check_fc(s.fc().unwrap(), SW / 4, 0);

        // Filling in the gap will not change fc.
        s.inbound_stream_frame(false, 0, &[0; SW_US / 8]).unwrap();
        check_fc(&fc.borrow(), SW / 4, 0);
        check_fc(s.fc().unwrap(), SW / 4, 0);
    }

    /// Test consuming the flow control in RecvStreamState::Recv - receiving frame past
    /// the flow control will cause an error.
    #[test]
    fn fc_state_recv_6() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));
        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);

        // Receiving frame past the flow control will cause an error.
        assert_eq!(
            s.inbound_stream_frame(false, 0, &[0; SW_US * 3 / 4 + 1]),
            Err(Error::FlowControlError)
        );
    }

    /// Test that the flow controls will send updates.
    #[test]
    fn fc_state_recv_7() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));
        let mut s = create_stream_with_fc(Rc::clone(&fc), SW / 2);

        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(false, 0, &[0; SW_US / 4]).unwrap();
        let mut buf = [1; SW_US];
        assert_eq!(s.read(&mut buf).unwrap(), (SW_US / 4, false));
        check_fc(&fc.borrow(), SW / 4, SW / 4);
        check_fc(s.fc().unwrap(), SW / 4, SW / 4);

        // Still no fc update needed.
        assert!(!fc.borrow().frame_needed());
        assert!(!s.fc().unwrap().frame_needed());

        // Receive one more byte that will cause a fc update after it is read.
        s.inbound_stream_frame(false, SW / 4, &[0]).unwrap();
        check_fc(&fc.borrow(), SW / 4 + 1, SW / 4);
        check_fc(s.fc().unwrap(), SW / 4 + 1, SW / 4);
        // Only consuming data does not cause a fc update to be sent.
        assert!(!fc.borrow().frame_needed());
        assert!(!s.fc().unwrap().frame_needed());

        assert_eq!(s.read(&mut buf).unwrap(), (1, false));
        check_fc(&fc.borrow(), SW / 4 + 1, SW / 4 + 1);
        check_fc(s.fc().unwrap(), SW / 4 + 1, SW / 4 + 1);
        // Data are retired and the sttream fc will send an update.
        assert!(!fc.borrow().frame_needed());
        assert!(s.fc().unwrap().frame_needed());

        // Receive more data to increase fc further.
        s.inbound_stream_frame(false, SW / 4, &[0; SW_US / 4])
            .unwrap();
        assert_eq!(s.read(&mut buf).unwrap(), (SW_US / 4 - 1, false));
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);
        assert!(!fc.borrow().frame_needed());
        assert!(s.fc().unwrap().frame_needed());

        // Write the fc update frame
        let mut builder = PacketBuilder::short(Encoder::new(), false, []);
        let mut token = Vec::new();
        let mut stats = FrameStats::default();
        fc.borrow_mut()
            .write_frames(&mut builder, &mut token, &mut stats);
        assert_eq!(stats.max_data, 0);
        s.write_frame(&mut builder, &mut token, &mut stats);
        assert_eq!(stats.max_stream_data, 1);

        // Receive 1 byte that will case a session fc update after it is read.
        s.inbound_stream_frame(false, SW / 2, &[0]).unwrap();
        assert_eq!(s.read(&mut buf).unwrap(), (1, false));
        check_fc(&fc.borrow(), SW / 2 + 1, SW / 2 + 1);
        check_fc(s.fc().unwrap(), SW / 2 + 1, SW / 2 + 1);
        assert!(fc.borrow().frame_needed());
        assert!(!s.fc().unwrap().frame_needed());
        fc.borrow_mut()
            .write_frames(&mut builder, &mut token, &mut stats);
        assert_eq!(stats.max_data, 1);
        s.write_frame(&mut builder, &mut token, &mut stats);
        assert_eq!(stats.max_stream_data, 1);
    }

    /// Test flow control in RecvStreamState::SizeKnown
    #[test]
    fn fc_state_size_known() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));

        let mut s = create_stream_with_fc(Rc::clone(&fc), SW);

        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(true, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Receiving duplicate frames (already consumed data) will not cause an error or
        // change fc.
        s.inbound_stream_frame(true, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // The stream can still receive duplicate data without a fin bit.
        s.inbound_stream_frame(false, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Receiving frame past the final size of a stream will return an error.
        assert_eq!(
            s.inbound_stream_frame(true, SW / 4, &[0; SW_US / 4 + 1]),
            Err(Error::FinalSizeError)
        );
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Add new data to the gap will not change fc.
        s.inbound_stream_frame(false, SW / 8, &[0; SW_US / 8])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Fill the gap
        s.inbound_stream_frame(false, 0, &[0; SW_US / 8]).unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Read all data
        let mut buf = [1; SW_US];
        assert_eq!(s.read(&mut buf).unwrap(), (SW_US / 2, true));
        // the stream does not have fc any more. We can only check the session fc.
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        assert!(s.fc().is_none());
    }

    /// Test flow control in RecvStreamState::DataRecvd
    #[test]
    fn fc_state_data_recv() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));

        let mut s = create_stream_with_fc(Rc::clone(&fc), SW);

        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(true, 0, &[0; SW_US / 2]).unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Receiving duplicate frames (already consumed data) will not cause an error or
        // change fc.
        s.inbound_stream_frame(true, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // The stream can still receive duplicate data without a fin bit.
        s.inbound_stream_frame(false, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Receiving frame past the final size of a stream will return an error.
        assert_eq!(
            s.inbound_stream_frame(true, SW / 4, &[0; SW_US / 4 + 1]),
            Err(Error::FinalSizeError)
        );
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        // Read all data
        let mut buf = [1; SW_US];
        assert_eq!(s.read(&mut buf).unwrap(), (SW_US / 2, true));
        // the stream does not have fc any more. We can only check the session fc.
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        assert!(s.fc().is_none());
    }

    /// Test flow control in RecvStreamState::DataRead
    #[test]
    fn fc_state_data_read() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));

        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);
        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(true, 0, &[0; SW_US / 2]).unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        let mut buf = [1; SW_US];
        assert_eq!(s.read(&mut buf).unwrap(), (SW_US / 2, true));
        // the stream does not have fc any more. We can only check the session fc.
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        assert!(s.fc().is_none());

        // Receiving duplicate frames (already consumed data) will not cause an error or
        // change fc.
        s.inbound_stream_frame(true, 0, &[0; SW_US / 2]).unwrap();
        // the stream does not have fc any more. We can only check the session fc.
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        assert!(s.fc().is_none());

        // Receiving frame past the final size of a stream or the stream's fc limit
        // will NOT return an error.
        s.inbound_stream_frame(true, 0, &[0; SW_US / 2 + 1])
            .unwrap();
        s.inbound_stream_frame(true, 0, &[0; SW_US * 3 / 4 + 1])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        assert!(s.fc().is_none());
    }

    /// Test flow control in RecvStreamState::AbortReading and final size is known
    #[test]
    fn fc_state_abort_reading_1() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));

        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);
        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(true, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        s.stop_sending(Error::NoError.code());
        // All data will de retired
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving duplicate frames (already consumed data) will not cause an error or
        // change fc.
        s.inbound_stream_frame(true, 0, &[0; SW_US / 2]).unwrap();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // The stream can still receive duplicate data without a fin bit.
        s.inbound_stream_frame(false, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving frame past the final size of a stream will return an error.
        assert_eq!(
            s.inbound_stream_frame(true, SW / 4, &[0; SW_US / 4 + 1]),
            Err(Error::FinalSizeError)
        );
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);
    }

    /// Test flow control in RecvStreamState::AbortReading and final size is unknown
    #[test]
    fn fc_state_abort_reading_2() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));

        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);
        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(false, 0, &[0; SW_US / 2]).unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        s.stop_sending(Error::NoError.code());
        // All data will de retired
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving duplicate frames (already consumed data) will not cause an error or
        // change fc.
        s.inbound_stream_frame(false, 0, &[0; SW_US / 2]).unwrap();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving data past the flow control limit will cause an error.
        assert_eq!(
            s.inbound_stream_frame(false, 0, &[0; SW_US * 3 / 4 + 1]),
            Err(Error::FlowControlError)
        );

        // The stream can still receive duplicate data without a fin bit.
        s.inbound_stream_frame(false, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving more data will case the data to be retired.
        // The stream can still receive duplicate data without a fin bit.
        s.inbound_stream_frame(false, SW / 2, &[0; 10]).unwrap();
        check_fc(&fc.borrow(), SW / 2 + 10, SW / 2 + 10);
        check_fc(s.fc().unwrap(), SW / 2 + 10, SW / 2 + 10);

        // We can still receive the final size.
        s.inbound_stream_frame(true, SW / 2, &[0; 20]).unwrap();
        check_fc(&fc.borrow(), SW / 2 + 20, SW / 2 + 20);
        check_fc(s.fc().unwrap(), SW / 2 + 20, SW / 2 + 20);

        // Receiving frame past the final size of a stream will return an error.
        assert_eq!(
            s.inbound_stream_frame(true, SW / 2, &[0; 21]),
            Err(Error::FinalSizeError)
        );
        check_fc(&fc.borrow(), SW / 2 + 20, SW / 2 + 20);
        check_fc(s.fc().unwrap(), SW / 2 + 20, SW / 2 + 20);
    }

    /// Test flow control in RecvStreamState::WaitForReset
    #[test]
    fn fc_state_wait_for_reset() {
        const SW: u64 = 1024;
        const SW_US: usize = 1024;
        let fc = Rc::new(RefCell::new(ReceiverFlowControl::new((), SW)));

        let mut s = create_stream_with_fc(Rc::clone(&fc), SW * 3 / 4);
        check_fc(&fc.borrow(), 0, 0);
        check_fc(s.fc().unwrap(), 0, 0);

        s.inbound_stream_frame(false, 0, &[0; SW_US / 2]).unwrap();
        check_fc(&fc.borrow(), SW / 2, 0);
        check_fc(s.fc().unwrap(), SW / 2, 0);

        s.stop_sending(Error::NoError.code());
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        s.stop_sending_acked();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving duplicate frames (already consumed data) will not cause an error or
        // change fc.
        s.inbound_stream_frame(false, 0, &[0; SW_US / 2]).unwrap();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving data past the flow control limit will cause an error.
        assert_eq!(
            s.inbound_stream_frame(false, 0, &[0; SW_US * 3 / 4 + 1]),
            Err(Error::FlowControlError)
        );

        // The stream can still receive duplicate data without a fin bit.
        s.inbound_stream_frame(false, SW / 4, &[0; SW_US / 4])
            .unwrap();
        check_fc(&fc.borrow(), SW / 2, SW / 2);
        check_fc(s.fc().unwrap(), SW / 2, SW / 2);

        // Receiving more data will case the data to be retired.
        // The stream can still receive duplicate data without a fin bit.
        s.inbound_stream_frame(false, SW / 2, &[0; 10]).unwrap();
        check_fc(&fc.borrow(), SW / 2 + 10, SW / 2 + 10);
        check_fc(s.fc().unwrap(), SW / 2 + 10, SW / 2 + 10);
    }
}
