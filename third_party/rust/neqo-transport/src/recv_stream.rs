// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Building a stream of ordered bytes to give the application from a series of
// incoming STREAM frames.

use std::cell::RefCell;
use std::cmp::{max, min};
use std::collections::BTreeMap;
use std::convert::TryInto;
use std::mem;
use std::ops::Bound::{Included, Unbounded};
use std::rc::Rc;

use smallvec::SmallVec;

use crate::events::ConnectionEvents;
use crate::flow_mgr::FlowMgr;
use crate::stream_id::StreamId;
use crate::{AppError, Error, Res};
use neqo_common::{matches, qtrace};

pub const RX_STREAM_DATA_WINDOW: u64 = 0xFFFF; // 64 KiB

pub(crate) type RecvStreams = BTreeMap<StreamId, RecvStream>;

/// Holds data not yet read by application. Orders and dedupes data ranges
/// from incoming STREAM frames.
#[derive(Debug, Default)]
pub struct RxStreamOrderer {
    data_ranges: BTreeMap<u64, Vec<u8>>, // (start_offset, data)
    retired: u64,                        // Number of bytes the application has read
}

impl RxStreamOrderer {
    pub fn new() -> Self {
        Self::default()
    }

    /// Process an incoming stream frame off the wire. This may result in data
    /// being available to upper layers if frame is not out of order (ooo) or
    /// if the frame fills a gap.
    pub fn inbound_frame(&mut self, new_start: u64, mut new_data: Vec<u8>) -> Res<()> {
        qtrace!("Inbound data offset={} len={}", new_start, new_data.len());

        // Get entry before where new entry would go, so we can see if we already
        // have the new bytes.
        // Avoid copies and duplicated data.
        let new_end = new_start + new_data.len() as u64;

        if new_end <= self.retired {
            // Range already read by application, this frame is very late and unneeded.
            return Ok(());
        }

        if new_data.is_empty() {
            // No data to insert
            return Ok(());
        }

        let (insert_new, remove_prev) = if let Some((&prev_start, prev_vec)) = self
            .data_ranges
            .range_mut((Unbounded, Included(new_start)))
            .next_back()
        {
            let prev_end = prev_start + prev_vec.len() as u64;

            match (new_start > prev_start, new_end > prev_end) {
                (true, true) => {
                    // PPPPPP    ->  PP
                    //   NNNNNN        NNNNNN
                    // Truncate prev if overlap. Insert new.
                    // (In-order frames will take this path, with no overlap)
                    let overlap = prev_end.saturating_sub(new_start);
                    if overlap != 0 {
                        let truncate_to = new_data.len() - overlap as usize;
                        prev_vec.truncate(truncate_to)
                    }
                    qtrace!(
                        "New frame {}-{} received, overlap: {}",
                        new_start,
                        new_end,
                        overlap
                    );
                    (true, None)
                }
                (true, false) => {
                    // PPPPPP    ->  PPPPPP
                    //   NNNN
                    // Do nothing
                    qtrace!(
                        "Dropping frame with already-received range {}-{}",
                        new_start,
                        new_end
                    );
                    (false, None)
                }
                (false, true) => {
                    // PPPP      ->  NNNNNN
                    // NNNNNN
                    // Drop Prev, Insert New
                    qtrace!(
                        "New frame with {}-{} replaces existing {}-{}",
                        new_start,
                        new_end,
                        prev_start,
                        prev_end
                    );
                    (true, Some(prev_start))
                }
                (false, false) => {
                    // PPPPPP    ->  PPPPPP
                    // NNNN
                    // Do nothing
                    qtrace!(
                        "Dropping frame with already-received range {}-{}",
                        new_start,
                        new_end
                    );
                    (false, None)
                }
            }
        } else {
            qtrace!("New frame {}-{} received", new_start, new_end);
            (true, None) // Nothing previous
        };

        if let Some(remove_prev) = &remove_prev {
            self.data_ranges.remove(remove_prev);
        }

        if insert_new {
            // Now handle possible overlap with next entries
            let mut to_remove = SmallVec::<[_; 8]>::new();

            for (&next_start, next_data) in self.data_ranges.range_mut(new_start..) {
                let next_end = next_start + next_data.len() as u64;
                let overlap = new_end.saturating_sub(next_start);
                if overlap == 0 {
                    break;
                } else if next_end > new_end {
                    qtrace!(
                        "New frame {}-{} overlaps with next frame by {}, truncating",
                        new_start,
                        new_end,
                        overlap
                    );
                    let truncate_to = new_data.len() - overlap as usize;
                    new_data.truncate(truncate_to);
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

            self.data_ranges.insert(new_start, new_data);
        };

        Ok(())
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

    /// Data bytes buffered. Could be more than bytes_readable if there are
    /// ranges missing.
    fn buffered(&self) -> u64 {
        self.data_ranges
            .iter()
            .map(|(&start, data)| data.len() as u64 - (self.retired.saturating_sub(start)))
            .sum()
    }

    /// Copy received data (if any) into the buffer. Returns bytes copied.
    fn read(&mut self, buf: &mut [u8]) -> Res<u64> {
        qtrace!("Reading {} bytes, {} available", buf.len(), self.buffered());
        let mut buf_remaining = buf.len() as usize;
        let mut copied = 0;

        for (&range_start, range_data) in &mut self.data_ranges {
            if self.retired >= range_start {
                // Frame data has some new contig bytes after some old bytes

                // Convert to offset into data vec and move past bytes we
                // already have
                let copy_offset = (max(range_start, self.retired) - range_start).try_into()?;
                let copy_bytes = min(range_data.len() - copy_offset, buf_remaining);
                let copy_slc = &mut range_data[copy_offset..copy_offset + copy_bytes];
                buf[copied..copied + copy_bytes].copy_from_slice(copy_slc);
                copied += copy_bytes;
                buf_remaining -= copy_bytes;
                self.retired += copy_bytes as u64;
            } else {
                break; // we're missing bytes
            }
        }

        // Remove map items that are consumed
        let to_remove = self
            .data_ranges
            .iter()
            .take_while(|(start, data)| self.retired >= *start + data.len() as u64)
            .map(|(k, _)| *k)
            .collect::<Vec<_>>();
        for key in to_remove {
            self.data_ranges.remove(&key);
        }

        Ok(copied as u64)
    }

    /// Extend the given Vector with any available data.
    pub fn read_to_end(&mut self, buf: &mut Vec<u8>) -> Res<u64> {
        let orig_len = buf.len();
        buf.resize(orig_len + self.bytes_ready(), 0);
        self.read(&mut buf[orig_len..])
    }

    fn highest_seen_offset(&self) -> u64 {
        let maybe_ooo_last = self
            .data_ranges
            .iter()
            .next_back()
            .map(|(start, data)| *start + data.len() as u64);
        maybe_ooo_last.unwrap_or(self.retired)
    }
}

/// QUIC receiving states, based on -transport 3.2.
#[derive(Debug)]
#[allow(dead_code)]
// Because a dead_code warning is easier than clippy::unused_self, see https://github.com/rust-lang/rust/issues/68408
enum RecvStreamState {
    Recv {
        recv_buf: RxStreamOrderer,
        max_bytes: u64, // Maximum size of recv_buf
        max_stream_data: u64,
    },
    SizeKnown {
        recv_buf: RxStreamOrderer,
        final_size: u64,
    },
    DataRecvd {
        recv_buf: RxStreamOrderer,
    },
    DataRead,
    ResetRecvd,
    // Defined by spec but we don't use it: ResetRead
}

impl RecvStreamState {
    fn new(max_bytes: u64) -> Self {
        Self::Recv {
            recv_buf: RxStreamOrderer::new(),
            max_bytes,
            max_stream_data: max_bytes,
        }
    }

    fn name(&self) -> &str {
        match self {
            Self::Recv { .. } => "Recv",
            Self::SizeKnown { .. } => "SizeKnown",
            Self::DataRecvd { .. } => "DataRecvd",
            Self::DataRead => "DataRead",
            Self::ResetRecvd => "ResetRecvd",
        }
    }

    fn recv_buf(&self) -> Option<&RxStreamOrderer> {
        match self {
            Self::Recv { recv_buf, .. }
            | Self::SizeKnown { recv_buf, .. }
            | Self::DataRecvd { recv_buf } => Some(recv_buf),
            Self::DataRead | Self::ResetRecvd => None,
        }
    }

    fn final_size(&self) -> Option<u64> {
        match self {
            Self::SizeKnown { final_size, .. } => Some(*final_size),
            _ => None,
        }
    }
}

/// Implement a QUIC receive stream.
#[derive(Debug)]
pub struct RecvStream {
    stream_id: StreamId,
    state: RecvStreamState,
    flow_mgr: Rc<RefCell<FlowMgr>>,
    conn_events: ConnectionEvents,
}

impl RecvStream {
    pub fn new(
        stream_id: StreamId,
        max_stream_data: u64,
        flow_mgr: Rc<RefCell<FlowMgr>>,
        conn_events: ConnectionEvents,
    ) -> Self {
        Self {
            stream_id,
            state: RecvStreamState::new(max_stream_data),
            flow_mgr,
            conn_events,
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

        if let RecvStreamState::Recv { .. } = &self.state {
            self.flow_mgr
                .borrow_mut()
                .clear_max_stream_data(self.stream_id)
        }

        if let RecvStreamState::DataRead = new_state {
            self.conn_events.recv_stream_complete(self.stream_id);
        }

        self.state = new_state;
    }

    pub fn inbound_stream_frame(&mut self, fin: bool, offset: u64, data: Vec<u8>) -> Res<()> {
        let new_end = offset + data.len() as u64;

        // Send final size errors even if stream is closed
        if let Some(final_size) = self.state.final_size() {
            if new_end > final_size || (fin && new_end != final_size) {
                return Err(Error::FinalSizeError);
            }
        }

        match &mut self.state {
            RecvStreamState::Recv {
                recv_buf,
                max_stream_data,
                ..
            } => {
                if new_end > *max_stream_data {
                    qtrace!("Stream RX window {} exceeded: {}", max_stream_data, new_end);
                    return Err(Error::FlowControlError);
                }

                if fin {
                    let final_size = offset + data.len() as u64;
                    if final_size < recv_buf.highest_seen_offset() {
                        return Err(Error::FinalSizeError);
                    }
                    recv_buf.inbound_frame(offset, data)?;

                    let buf = mem::replace(recv_buf, RxStreamOrderer::new());
                    if final_size == buf.retired() + buf.bytes_ready() as u64 {
                        self.set_state(RecvStreamState::DataRecvd { recv_buf: buf });
                    } else {
                        self.set_state(RecvStreamState::SizeKnown {
                            recv_buf: buf,
                            final_size,
                        });
                    }
                } else {
                    recv_buf.inbound_frame(offset, data)?;
                }
            }
            RecvStreamState::SizeKnown {
                recv_buf,
                final_size,
            } => {
                recv_buf.inbound_frame(offset, data)?;
                if *final_size == recv_buf.retired() + recv_buf.bytes_ready() as u64 {
                    let buf = mem::replace(recv_buf, RxStreamOrderer::new());
                    self.set_state(RecvStreamState::DataRecvd { recv_buf: buf });
                }
            }
            RecvStreamState::DataRecvd { .. }
            | RecvStreamState::DataRead
            | RecvStreamState::ResetRecvd => {
                qtrace!("data received when we are in state {}", self.state.name())
            }
        }

        if self.data_ready() || self.needs_to_inform_app_about_fin() {
            self.conn_events.recv_stream_readable(self.stream_id)
        }

        Ok(())
    }

    pub fn reset(&mut self, application_error_code: AppError) {
        match self.state {
            RecvStreamState::Recv { .. } | RecvStreamState::SizeKnown { .. } => {
                self.conn_events
                    .recv_stream_reset(self.stream_id, application_error_code);
                self.set_state(RecvStreamState::ResetRecvd);
            }
            _ => {
                // Ignore reset if in DataRecvd, DataRead, or ResetRecvd
            }
        }
    }

    /// If we should tell the sender they have more credit, return an offset
    pub fn maybe_send_flowc_update(&mut self) {
        if let RecvStreamState::Recv {
            max_bytes,
            max_stream_data,
            recv_buf,
        } = &mut self.state
        {
            {
                // Algo: send an update if app has consumed more than half
                // the data in the current window
                // TODO(agrover@mozilla.com): This algo is not great but
                // should prevent Silly Window Syndrome. Spec refers to using
                // highest seen offset somehow? RTT maybe?
                let maybe_new_max = recv_buf.retired() + *max_bytes;
                if maybe_new_max > (*max_bytes / 2) + *max_stream_data {
                    *max_stream_data = maybe_new_max;
                    self.flow_mgr
                        .borrow_mut()
                        .max_stream_data(self.stream_id, maybe_new_max)
                }
            }
        }
    }

    pub fn is_terminal(&self) -> bool {
        matches!(
            self.state,
            RecvStreamState::ResetRecvd | RecvStreamState::DataRead
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

    pub fn read(&mut self, buf: &mut [u8]) -> Res<(u64, bool)> {
        let res = match &mut self.state {
            RecvStreamState::Recv { recv_buf, .. }
            | RecvStreamState::SizeKnown { recv_buf, .. } => Ok((recv_buf.read(buf)?, false)),
            RecvStreamState::DataRecvd { recv_buf } => {
                let bytes_read = recv_buf.read(buf)?;
                let fin_read = recv_buf.buffered() == 0;
                if fin_read {
                    self.set_state(RecvStreamState::DataRead)
                }
                Ok((bytes_read, fin_read))
            }
            RecvStreamState::DataRead | RecvStreamState::ResetRecvd => Err(Error::NoMoreData),
        };
        self.maybe_send_flowc_update();
        res
    }

    pub fn stop_sending(&mut self, err: AppError) {
        qtrace!("stop_sending called when in state {}", self.state.name());
        match &self.state {
            RecvStreamState::Recv { .. } | RecvStreamState::SizeKnown { .. } => {
                self.set_state(RecvStreamState::ResetRecvd);
                self.flow_mgr.borrow_mut().stop_sending(self.stream_id, err)
            }
            RecvStreamState::DataRecvd { .. } => self.set_state(RecvStreamState::DataRead),
            RecvStreamState::DataRead | RecvStreamState::ResetRecvd => {
                // Already in terminal state
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::frame::Frame;
    use neqo_common::matches;

    #[test]
    fn test_stream_rx() {
        let flow_mgr = Rc::new(RefCell::new(FlowMgr::default()));
        let conn_events = ConnectionEvents::default();

        let mut s = RecvStream::new(567.into(), 1024, Rc::clone(&flow_mgr), conn_events);

        // test receiving a contig frame and reading it works
        s.inbound_stream_frame(false, 0, vec![1; 10]).unwrap();
        assert_eq!(s.data_ready(), true);
        let mut buf = vec![0u8; 100];
        assert_eq!(s.read(&mut buf).unwrap(), (10, false));
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 0);

        // test receiving a noncontig frame
        s.inbound_stream_frame(false, 12, vec![2; 12]).unwrap();
        assert_eq!(s.data_ready(), false);
        assert_eq!(s.read(&mut buf).unwrap(), (0, false));
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 12);

        // another frame that overlaps the first
        s.inbound_stream_frame(false, 14, vec![3; 8]).unwrap();
        assert_eq!(s.data_ready(), false);
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 12);

        // fill in the gap, but with a FIN
        s.inbound_stream_frame(true, 10, vec![4; 6]).unwrap_err();
        assert_eq!(s.data_ready(), false);
        assert_eq!(s.read(&mut buf).unwrap(), (0, false));
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 12);

        // fill in the gap
        s.inbound_stream_frame(false, 10, vec![5; 10]).unwrap();
        assert_eq!(s.data_ready(), true);
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 14);

        // a legit FIN
        s.inbound_stream_frame(true, 24, vec![6; 18]).unwrap();
        assert_eq!(s.state.recv_buf().unwrap().retired(), 10);
        assert_eq!(s.state.recv_buf().unwrap().buffered(), 32);
        assert_eq!(s.data_ready(), true);
        assert_eq!(s.read(&mut buf).unwrap(), (32, true));

        // Stream now no longer readable (is in DataRead state)
        s.read(&mut buf).unwrap_err();
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    fn test_stream_rx_dedupe() {
        let flow_mgr = Rc::new(RefCell::new(FlowMgr::default()));
        let conn_events = ConnectionEvents::default();

        let mut s = RecvStream::new(3.into(), 1024, Rc::clone(&flow_mgr), conn_events);

        let mut buf = vec![0u8; 100];

        // test receiving a contig frame and reading it works
        s.inbound_stream_frame(false, 0, vec![1; 6]).unwrap();

        // See inbound_frame(). Test (true, true) case
        s.inbound_stream_frame(false, 2, vec![2; 6]).unwrap();
        {
            let mut i = s.state.recv_buf().unwrap().data_ranges.iter();
            let item = i.next().unwrap();
            assert_eq!(*item.0, 0);
            assert_eq!(item.1.len(), 2);
            let item = i.next().unwrap();
            assert_eq!(*item.0, 2);
            assert_eq!(item.1.len(), 6);
        }

        // Test (true, false) case
        s.inbound_stream_frame(false, 4, vec![3; 4]).unwrap();
        {
            let mut i = s.state.recv_buf().unwrap().data_ranges.iter();
            let item = i.next().unwrap();
            assert_eq!(*item.0, 0);
            assert_eq!(item.1.len(), 2);
            let item = i.next().unwrap();
            assert_eq!(*item.0, 2);
            assert_eq!(item.1.len(), 6);
        }

        // Test (false, true) case
        s.inbound_stream_frame(false, 2, vec![4; 8]).unwrap();
        {
            let mut i = s.state.recv_buf().unwrap().data_ranges.iter();
            let item = i.next().unwrap();
            assert_eq!(*item.0, 0);
            assert_eq!(item.1.len(), 2);
            let item = i.next().unwrap();
            assert_eq!(*item.0, 2);
            assert_eq!(item.1.len(), 8);
        }

        // Test (false, false) case
        s.inbound_stream_frame(false, 2, vec![5; 2]).unwrap();
        {
            let mut i = s.state.recv_buf().unwrap().data_ranges.iter();
            let item = i.next().unwrap();
            assert_eq!(*item.0, 0);
            assert_eq!(item.1.len(), 2);
            let item = i.next().unwrap();
            assert_eq!(*item.0, 2);
            assert_eq!(item.1.len(), 8);
        }

        assert_eq!(s.read(&mut buf).unwrap(), (10, false));
        assert_eq!(buf[..10], [1, 1, 4, 4, 4, 4, 4, 4, 4, 4]);

        // Test truncation/span-drop on insert
        s.inbound_stream_frame(false, 100, vec![6; 6]).unwrap();
        // a. insert where new frame gets truncated
        s.inbound_stream_frame(false, 99, vec![7; 6]).unwrap();
        {
            let mut i = s.state.recv_buf().unwrap().data_ranges.iter();
            let item = i.next().unwrap();
            assert_eq!(*item.0, 99);
            assert_eq!(item.1.len(), 1);
            let item = i.next().unwrap();
            assert_eq!(*item.0, 100);
            assert_eq!(item.1.len(), 6);
            assert_eq!(i.next(), None);
        }

        // b. insert where new frame spans next frame
        s.inbound_stream_frame(false, 98, vec![8; 10]).unwrap();
        {
            let mut i = s.state.recv_buf().unwrap().data_ranges.iter();
            let item = i.next().unwrap();
            assert_eq!(*item.0, 98);
            assert_eq!(item.1.len(), 10);
            assert_eq!(i.next(), None);
        }
    }

    #[test]
    fn test_stream_flowc_update() {
        let flow_mgr = Rc::new(RefCell::new(FlowMgr::default()));
        let conn_events = ConnectionEvents::default();

        let frame1 = vec![0; RX_STREAM_DATA_WINDOW as usize];

        let mut s = RecvStream::new(
            4.into(),
            RX_STREAM_DATA_WINDOW,
            Rc::clone(&flow_mgr),
            conn_events,
        );

        let mut buf = vec![0u8; RX_STREAM_DATA_WINDOW as usize * 4]; // Make it overlarge

        s.maybe_send_flowc_update();
        assert_eq!(s.flow_mgr.borrow().peek(), None);
        s.inbound_stream_frame(false, 0, frame1).unwrap();
        s.maybe_send_flowc_update();
        assert_eq!(s.flow_mgr.borrow().peek(), None);
        assert_eq!(s.read(&mut buf).unwrap(), (RX_STREAM_DATA_WINDOW, false));
        assert_eq!(s.data_ready(), false);
        s.maybe_send_flowc_update();

        // flow msg generated!
        assert!(s.flow_mgr.borrow().peek().is_some());

        // consume it
        s.flow_mgr.borrow_mut().next().unwrap();

        // it should be gone
        s.maybe_send_flowc_update();
        assert_eq!(s.flow_mgr.borrow().peek(), None);
    }

    #[test]
    fn test_stream_max_stream_data() {
        let flow_mgr = Rc::new(RefCell::new(FlowMgr::default()));
        let conn_events = ConnectionEvents::default();

        let frame1 = vec![0; RX_STREAM_DATA_WINDOW as usize];

        let mut s = RecvStream::new(
            67.into(),
            RX_STREAM_DATA_WINDOW,
            Rc::clone(&flow_mgr),
            conn_events,
        );

        s.maybe_send_flowc_update();
        assert_eq!(s.flow_mgr.borrow().peek(), None);
        s.inbound_stream_frame(false, 0, frame1).unwrap();
        s.inbound_stream_frame(false, RX_STREAM_DATA_WINDOW, vec![1; 1])
            .unwrap_err();
    }

    #[test]
    fn test_stream_orderer_bytes_ready() {
        let mut rx_ord = RxStreamOrderer::new();

        let mut buf = vec![0u8; 100];

        rx_ord.inbound_frame(0, vec![1; 6]).unwrap();
        assert_eq!(rx_ord.bytes_ready(), 6);
        assert_eq!(rx_ord.buffered(), 6);
        assert_eq!(rx_ord.retired(), 0);
        // read some so there's an offset into the first frame
        rx_ord.read(&mut buf[..2]).unwrap();
        assert_eq!(rx_ord.bytes_ready(), 4);
        assert_eq!(rx_ord.buffered(), 4);
        assert_eq!(rx_ord.retired(), 2);
        // an overlapping frame
        rx_ord.inbound_frame(5, vec![2; 6]).unwrap();
        assert_eq!(rx_ord.bytes_ready(), 9);
        assert_eq!(rx_ord.buffered(), 9);
        assert_eq!(rx_ord.retired(), 2);
        // a noncontig frame
        rx_ord.inbound_frame(20, vec![3; 6]).unwrap();
        assert_eq!(rx_ord.bytes_ready(), 9);
        assert_eq!(rx_ord.buffered(), 15);
        assert_eq!(rx_ord.retired(), 2);
        // an old frame
        rx_ord.inbound_frame(0, vec![4; 2]).unwrap();
        assert_eq!(rx_ord.bytes_ready(), 9);
        assert_eq!(rx_ord.buffered(), 15);
        assert_eq!(rx_ord.retired(), 2);
    }

    #[test]
    fn no_stream_flowc_event_after_exiting_recv() {
        let flow_mgr = Rc::new(RefCell::new(FlowMgr::default()));
        let conn_events = ConnectionEvents::default();

        let frame1 = vec![0; RX_STREAM_DATA_WINDOW as usize];

        let mut s = RecvStream::new(
            67.into(),
            RX_STREAM_DATA_WINDOW,
            Rc::clone(&flow_mgr),
            conn_events,
        );

        s.inbound_stream_frame(false, 0, frame1).unwrap();
        flow_mgr.borrow_mut().max_stream_data(67.into(), 100);
        assert!(matches!(s.flow_mgr.borrow().peek().unwrap(), Frame::MaxStreamData{..}));
        s.inbound_stream_frame(true, RX_STREAM_DATA_WINDOW, vec![])
            .unwrap();
        assert!(matches!(s.flow_mgr.borrow().peek(), None));
    }
}
