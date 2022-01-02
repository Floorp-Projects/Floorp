use super::store::Resolve;
use super::*;

use crate::frame::{Reason, StreamId};

use crate::codec::UserError;
use crate::codec::UserError::*;

use bytes::buf::ext::{BufExt, Take};
use std::io;
use std::task::{Context, Poll, Waker};
use std::{cmp, fmt, mem};

/// # Warning
///
/// Queued streams are ordered by stream ID, as we need to ensure that
/// lower-numbered streams are sent headers before higher-numbered ones.
/// This is because "idle" stream IDs – those which have been initiated but
/// have yet to receive frames – will be implicitly closed on receipt of a
/// frame on a higher stream ID. If these queues was not ordered by stream
/// IDs, some mechanism would be necessary to ensure that the lowest-numberedh]
/// idle stream is opened first.
#[derive(Debug)]
pub(super) struct Prioritize {
    /// Queue of streams waiting for socket capacity to send a frame.
    pending_send: store::Queue<stream::NextSend>,

    /// Queue of streams waiting for window capacity to produce data.
    pending_capacity: store::Queue<stream::NextSendCapacity>,

    /// Streams waiting for capacity due to max concurrency
    ///
    /// The `SendRequest` handle is `Clone`. This enables initiating requests
    /// from many tasks. However, offering this capability while supporting
    /// backpressure at some level is tricky. If there are many `SendRequest`
    /// handles and a single stream becomes available, which handle gets
    /// assigned that stream? Maybe that handle is no longer ready to send a
    /// request.
    ///
    /// The strategy used is to allow each `SendRequest` handle one buffered
    /// request. A `SendRequest` handle is ready to send a request if it has no
    /// associated buffered requests. This is the same strategy as `mpsc` in the
    /// futures library.
    pending_open: store::Queue<stream::NextOpen>,

    /// Connection level flow control governing sent data
    flow: FlowControl,

    /// Stream ID of the last stream opened.
    last_opened_id: StreamId,

    /// What `DATA` frame is currently being sent in the codec.
    in_flight_data_frame: InFlightData,
}

#[derive(Debug, Eq, PartialEq)]
enum InFlightData {
    /// There is no `DATA` frame in flight.
    Nothing,
    /// There is a `DATA` frame in flight belonging to the given stream.
    DataFrame(store::Key),
    /// There was a `DATA` frame, but the stream's queue was since cleared.
    Drop,
}

pub(crate) struct Prioritized<B> {
    // The buffer
    inner: Take<B>,

    end_of_stream: bool,

    // The stream that this is associated with
    stream: store::Key,
}

// ===== impl Prioritize =====

impl Prioritize {
    pub fn new(config: &Config) -> Prioritize {
        let mut flow = FlowControl::new();

        flow.inc_window(config.remote_init_window_sz)
            .expect("invalid initial window size");

        flow.assign_capacity(config.remote_init_window_sz);

        log::trace!("Prioritize::new; flow={:?}", flow);

        Prioritize {
            pending_send: store::Queue::new(),
            pending_capacity: store::Queue::new(),
            pending_open: store::Queue::new(),
            flow,
            last_opened_id: StreamId::ZERO,
            in_flight_data_frame: InFlightData::Nothing,
        }
    }

    /// Queue a frame to be sent to the remote
    pub fn queue_frame<B>(
        &mut self,
        frame: Frame<B>,
        buffer: &mut Buffer<Frame<B>>,
        stream: &mut store::Ptr,
        task: &mut Option<Waker>,
    ) {
        // Queue the frame in the buffer
        stream.pending_send.push_back(buffer, frame);
        self.schedule_send(stream, task);
    }

    pub fn schedule_send(&mut self, stream: &mut store::Ptr, task: &mut Option<Waker>) {
        // If the stream is waiting to be opened, nothing more to do.
        if stream.is_send_ready() {
            log::trace!("schedule_send; {:?}", stream.id);
            // Queue the stream
            self.pending_send.push(stream);

            // Notify the connection.
            if let Some(task) = task.take() {
                task.wake();
            }
        }
    }

    pub fn queue_open(&mut self, stream: &mut store::Ptr) {
        self.pending_open.push(stream);
    }

    /// Send a data frame
    pub fn send_data<B>(
        &mut self,
        frame: frame::Data<B>,
        buffer: &mut Buffer<Frame<B>>,
        stream: &mut store::Ptr,
        counts: &mut Counts,
        task: &mut Option<Waker>,
    ) -> Result<(), UserError>
    where
        B: Buf,
    {
        let sz = frame.payload().remaining();

        if sz > MAX_WINDOW_SIZE as usize {
            return Err(UserError::PayloadTooBig);
        }

        let sz = sz as WindowSize;

        if !stream.state.is_send_streaming() {
            if stream.state.is_closed() {
                return Err(InactiveStreamId);
            } else {
                return Err(UnexpectedFrameType);
            }
        }

        // Update the buffered data counter
        stream.buffered_send_data += sz;

        log::trace!(
            "send_data; sz={}; buffered={}; requested={}",
            sz,
            stream.buffered_send_data,
            stream.requested_send_capacity
        );

        // Implicitly request more send capacity if not enough has been
        // requested yet.
        if stream.requested_send_capacity < stream.buffered_send_data {
            // Update the target requested capacity
            stream.requested_send_capacity = stream.buffered_send_data;

            self.try_assign_capacity(stream);
        }

        if frame.is_end_stream() {
            stream.state.send_close();
            self.reserve_capacity(0, stream, counts);
        }

        log::trace!(
            "send_data (2); available={}; buffered={}",
            stream.send_flow.available(),
            stream.buffered_send_data
        );

        // The `stream.buffered_send_data == 0` check is here so that, if a zero
        // length data frame is queued to the front (there is no previously
        // queued data), it gets sent out immediately even if there is no
        // available send window.
        //
        // Sending out zero length data frames can be done to signal
        // end-of-stream.
        //
        if stream.send_flow.available() > 0 || stream.buffered_send_data == 0 {
            // The stream currently has capacity to send the data frame, so
            // queue it up and notify the connection task.
            self.queue_frame(frame.into(), buffer, stream, task);
        } else {
            // The stream has no capacity to send the frame now, save it but
            // don't notify the connection task. Once additional capacity
            // becomes available, the frame will be flushed.
            stream.pending_send.push_back(buffer, frame.into());
        }

        Ok(())
    }

    /// Request capacity to send data
    pub fn reserve_capacity(
        &mut self,
        capacity: WindowSize,
        stream: &mut store::Ptr,
        counts: &mut Counts,
    ) {
        log::trace!(
            "reserve_capacity; stream={:?}; requested={:?}; effective={:?}; curr={:?}",
            stream.id,
            capacity,
            capacity + stream.buffered_send_data,
            stream.requested_send_capacity
        );

        // Actual capacity is `capacity` + the current amount of buffered data.
        // If it were less, then we could never send out the buffered data.
        let capacity = capacity + stream.buffered_send_data;

        if capacity == stream.requested_send_capacity {
            // Nothing to do
        } else if capacity < stream.requested_send_capacity {
            // Update the target requested capacity
            stream.requested_send_capacity = capacity;

            // Currently available capacity assigned to the stream
            let available = stream.send_flow.available().as_size();

            // If the stream has more assigned capacity than requested, reclaim
            // some for the connection
            if available > capacity {
                let diff = available - capacity;

                stream.send_flow.claim_capacity(diff);

                self.assign_connection_capacity(diff, stream, counts);
            }
        } else {
            // If trying to *add* capacity, but the stream send side is closed,
            // there's nothing to be done.
            if stream.state.is_send_closed() {
                return;
            }

            // Update the target requested capacity
            stream.requested_send_capacity = capacity;

            // Try to assign additional capacity to the stream. If none is
            // currently available, the stream will be queued to receive some
            // when more becomes available.
            self.try_assign_capacity(stream);
        }
    }

    pub fn recv_stream_window_update(
        &mut self,
        inc: WindowSize,
        stream: &mut store::Ptr,
    ) -> Result<(), Reason> {
        log::trace!(
            "recv_stream_window_update; stream={:?}; state={:?}; inc={}; flow={:?}",
            stream.id,
            stream.state,
            inc,
            stream.send_flow
        );

        if stream.state.is_send_closed() && stream.buffered_send_data == 0 {
            // We can't send any data, so don't bother doing anything else.
            return Ok(());
        }

        // Update the stream level flow control.
        stream.send_flow.inc_window(inc)?;

        // If the stream is waiting on additional capacity, then this will
        // assign it (if available on the connection) and notify the producer
        self.try_assign_capacity(stream);

        Ok(())
    }

    pub fn recv_connection_window_update(
        &mut self,
        inc: WindowSize,
        store: &mut Store,
        counts: &mut Counts,
    ) -> Result<(), Reason> {
        // Update the connection's window
        self.flow.inc_window(inc)?;

        self.assign_connection_capacity(inc, store, counts);
        Ok(())
    }

    /// Reclaim all capacity assigned to the stream and re-assign it to the
    /// connection
    pub fn reclaim_all_capacity(&mut self, stream: &mut store::Ptr, counts: &mut Counts) {
        let available = stream.send_flow.available().as_size();
        stream.send_flow.claim_capacity(available);
        // Re-assign all capacity to the connection
        self.assign_connection_capacity(available, stream, counts);
    }

    /// Reclaim just reserved capacity, not buffered capacity, and re-assign
    /// it to the connection
    pub fn reclaim_reserved_capacity(&mut self, stream: &mut store::Ptr, counts: &mut Counts) {
        // only reclaim requested capacity that isn't already buffered
        if stream.requested_send_capacity > stream.buffered_send_data {
            let reserved = stream.requested_send_capacity - stream.buffered_send_data;

            stream.send_flow.claim_capacity(reserved);
            self.assign_connection_capacity(reserved, stream, counts);
        }
    }

    pub fn clear_pending_capacity(&mut self, store: &mut Store, counts: &mut Counts) {
        while let Some(stream) = self.pending_capacity.pop(store) {
            counts.transition(stream, |_, stream| {
                log::trace!("clear_pending_capacity; stream={:?}", stream.id);
            })
        }
    }

    pub fn assign_connection_capacity<R>(
        &mut self,
        inc: WindowSize,
        store: &mut R,
        counts: &mut Counts,
    ) where
        R: Resolve,
    {
        log::trace!("assign_connection_capacity; inc={}", inc);

        self.flow.assign_capacity(inc);

        // Assign newly acquired capacity to streams pending capacity.
        while self.flow.available() > 0 {
            let stream = match self.pending_capacity.pop(store) {
                Some(stream) => stream,
                None => return,
            };

            // Streams pending capacity may have been reset before capacity
            // became available. In that case, the stream won't want any
            // capacity, and so we shouldn't "transition" on it, but just evict
            // it and continue the loop.
            if !(stream.state.is_send_streaming() || stream.buffered_send_data > 0) {
                continue;
            }

            counts.transition(stream, |_, mut stream| {
                // Try to assign capacity to the stream. This will also re-queue the
                // stream if there isn't enough connection level capacity to fulfill
                // the capacity request.
                self.try_assign_capacity(&mut stream);
            })
        }
    }

    /// Request capacity to send data
    fn try_assign_capacity(&mut self, stream: &mut store::Ptr) {
        let total_requested = stream.requested_send_capacity;

        // Total requested should never go below actual assigned
        // (Note: the window size can go lower than assigned)
        debug_assert!(total_requested >= stream.send_flow.available());

        // The amount of additional capacity that the stream requests.
        // Don't assign more than the window has available!
        let additional = cmp::min(
            total_requested - stream.send_flow.available().as_size(),
            // Can't assign more than what is available
            stream.send_flow.window_size() - stream.send_flow.available().as_size(),
        );

        log::trace!(
            "try_assign_capacity; stream={:?}, requested={}; additional={}; buffered={}; window={}; conn={}",
            stream.id,
            total_requested,
            additional,
            stream.buffered_send_data,
            stream.send_flow.window_size(),
            self.flow.available()
        );

        if additional == 0 {
            // Nothing more to do
            return;
        }

        // If the stream has requested capacity, then it must be in the
        // streaming state (more data could be sent) or there is buffered data
        // waiting to be sent.
        debug_assert!(
            stream.state.is_send_streaming() || stream.buffered_send_data > 0,
            "state={:?}",
            stream.state
        );

        // The amount of currently available capacity on the connection
        let conn_available = self.flow.available().as_size();

        // First check if capacity is immediately available
        if conn_available > 0 {
            // The amount of capacity to assign to the stream
            // TODO: Should prioritization factor into this?
            let assign = cmp::min(conn_available, additional);

            log::trace!("  assigning; stream={:?}, capacity={}", stream.id, assign,);

            // Assign the capacity to the stream
            stream.assign_capacity(assign);

            // Claim the capacity from the connection
            self.flow.claim_capacity(assign);
        }

        log::trace!(
            "try_assign_capacity(2); available={}; requested={}; buffered={}; has_unavailable={:?}",
            stream.send_flow.available(),
            stream.requested_send_capacity,
            stream.buffered_send_data,
            stream.send_flow.has_unavailable()
        );

        if stream.send_flow.available() < stream.requested_send_capacity
            && stream.send_flow.has_unavailable()
        {
            // The stream requires additional capacity and the stream's
            // window has available capacity, but the connection window
            // does not.
            //
            // In this case, the stream needs to be queued up for when the
            // connection has more capacity.
            self.pending_capacity.push(stream);
        }

        // If data is buffered and the stream is send ready, then
        // schedule the stream for execution
        if stream.buffered_send_data > 0 && stream.is_send_ready() {
            // TODO: This assertion isn't *exactly* correct. There can still be
            // buffered send data while the stream's pending send queue is
            // empty. This can happen when a large data frame is in the process
            // of being **partially** sent. Once the window has been sent, the
            // data frame will be returned to the prioritization layer to be
            // re-scheduled.
            //
            // That said, it would be nice to figure out how to make this
            // assertion correctly.
            //
            // debug_assert!(!stream.pending_send.is_empty());

            self.pending_send.push(stream);
        }
    }

    pub fn poll_complete<T, B>(
        &mut self,
        cx: &mut Context,
        buffer: &mut Buffer<Frame<B>>,
        store: &mut Store,
        counts: &mut Counts,
        dst: &mut Codec<T, Prioritized<B>>,
    ) -> Poll<io::Result<()>>
    where
        T: AsyncWrite + Unpin,
        B: Buf,
    {
        // Ensure codec is ready
        ready!(dst.poll_ready(cx))?;

        // Reclaim any frame that has previously been written
        self.reclaim_frame(buffer, store, dst);

        // The max frame length
        let max_frame_len = dst.max_send_frame_size();

        log::trace!("poll_complete");

        loop {
            self.schedule_pending_open(store, counts);

            match self.pop_frame(buffer, store, max_frame_len, counts) {
                Some(frame) => {
                    log::trace!("writing frame={:?}", frame);

                    debug_assert_eq!(self.in_flight_data_frame, InFlightData::Nothing);
                    if let Frame::Data(ref frame) = frame {
                        self.in_flight_data_frame = InFlightData::DataFrame(frame.payload().stream);
                    }
                    dst.buffer(frame).expect("invalid frame");

                    // Ensure the codec is ready to try the loop again.
                    ready!(dst.poll_ready(cx))?;

                    // Because, always try to reclaim...
                    self.reclaim_frame(buffer, store, dst);
                }
                None => {
                    // Try to flush the codec.
                    ready!(dst.flush(cx))?;

                    // This might release a data frame...
                    if !self.reclaim_frame(buffer, store, dst) {
                        return Poll::Ready(Ok(()));
                    }

                    // No need to poll ready as poll_complete() does this for
                    // us...
                }
            }
        }
    }

    /// Tries to reclaim a pending data frame from the codec.
    ///
    /// Returns true if a frame was reclaimed.
    ///
    /// When a data frame is written to the codec, it may not be written in its
    /// entirety (large chunks are split up into potentially many data frames).
    /// In this case, the stream needs to be reprioritized.
    fn reclaim_frame<T, B>(
        &mut self,
        buffer: &mut Buffer<Frame<B>>,
        store: &mut Store,
        dst: &mut Codec<T, Prioritized<B>>,
    ) -> bool
    where
        B: Buf,
    {
        log::trace!("try reclaim frame");

        // First check if there are any data chunks to take back
        if let Some(frame) = dst.take_last_data_frame() {
            log::trace!(
                "  -> reclaimed; frame={:?}; sz={}",
                frame,
                frame.payload().inner.get_ref().remaining()
            );

            let mut eos = false;
            let key = frame.payload().stream;

            match mem::replace(&mut self.in_flight_data_frame, InFlightData::Nothing) {
                InFlightData::Nothing => panic!("wasn't expecting a frame to reclaim"),
                InFlightData::Drop => {
                    log::trace!("not reclaiming frame for cancelled stream");
                    return false;
                }
                InFlightData::DataFrame(k) => {
                    debug_assert_eq!(k, key);
                }
            }

            let mut frame = frame.map(|prioritized| {
                // TODO: Ensure fully written
                eos = prioritized.end_of_stream;
                prioritized.inner.into_inner()
            });

            if frame.payload().has_remaining() {
                let mut stream = store.resolve(key);

                if eos {
                    frame.set_end_stream(true);
                }

                self.push_back_frame(frame.into(), buffer, &mut stream);

                return true;
            }
        }

        false
    }

    /// Push the frame to the front of the stream's deque, scheduling the
    /// stream if needed.
    fn push_back_frame<B>(
        &mut self,
        frame: Frame<B>,
        buffer: &mut Buffer<Frame<B>>,
        stream: &mut store::Ptr,
    ) {
        // Push the frame to the front of the stream's deque
        stream.pending_send.push_front(buffer, frame);

        // If needed, schedule the sender
        if stream.send_flow.available() > 0 {
            debug_assert!(!stream.pending_send.is_empty());
            self.pending_send.push(stream);
        }
    }

    pub fn clear_queue<B>(&mut self, buffer: &mut Buffer<Frame<B>>, stream: &mut store::Ptr) {
        log::trace!("clear_queue; stream={:?}", stream.id);

        // TODO: make this more efficient?
        while let Some(frame) = stream.pending_send.pop_front(buffer) {
            log::trace!("dropping; frame={:?}", frame);
        }

        stream.buffered_send_data = 0;
        stream.requested_send_capacity = 0;
        if let InFlightData::DataFrame(key) = self.in_flight_data_frame {
            if stream.key() == key {
                // This stream could get cleaned up now - don't allow the buffered frame to get reclaimed.
                self.in_flight_data_frame = InFlightData::Drop;
            }
        }
    }

    pub fn clear_pending_send(&mut self, store: &mut Store, counts: &mut Counts) {
        while let Some(stream) = self.pending_send.pop(store) {
            let is_pending_reset = stream.is_pending_reset_expiration();
            counts.transition_after(stream, is_pending_reset);
        }
    }

    pub fn clear_pending_open(&mut self, store: &mut Store, counts: &mut Counts) {
        while let Some(stream) = self.pending_open.pop(store) {
            let is_pending_reset = stream.is_pending_reset_expiration();
            counts.transition_after(stream, is_pending_reset);
        }
    }

    fn pop_frame<B>(
        &mut self,
        buffer: &mut Buffer<Frame<B>>,
        store: &mut Store,
        max_len: usize,
        counts: &mut Counts,
    ) -> Option<Frame<Prioritized<B>>>
    where
        B: Buf,
    {
        log::trace!("pop_frame");

        loop {
            match self.pending_send.pop(store) {
                Some(mut stream) => {
                    log::trace!(
                        "pop_frame; stream={:?}; stream.state={:?}",
                        stream.id,
                        stream.state
                    );

                    // It's possible that this stream, besides having data to send,
                    // is also queued to send a reset, and thus is already in the queue
                    // to wait for "some time" after a reset.
                    //
                    // To be safe, we just always ask the stream.
                    let is_pending_reset = stream.is_pending_reset_expiration();

                    log::trace!(
                        " --> stream={:?}; is_pending_reset={:?};",
                        stream.id,
                        is_pending_reset
                    );

                    let frame = match stream.pending_send.pop_front(buffer) {
                        Some(Frame::Data(mut frame)) => {
                            // Get the amount of capacity remaining for stream's
                            // window.
                            let stream_capacity = stream.send_flow.available();
                            let sz = frame.payload().remaining();

                            log::trace!(
                                " --> data frame; stream={:?}; sz={}; eos={:?}; window={}; \
                                 available={}; requested={}; buffered={};",
                                frame.stream_id(),
                                sz,
                                frame.is_end_stream(),
                                stream_capacity,
                                stream.send_flow.available(),
                                stream.requested_send_capacity,
                                stream.buffered_send_data,
                            );

                            // Zero length data frames always have capacity to
                            // be sent.
                            if sz > 0 && stream_capacity == 0 {
                                log::trace!(
                                    " --> stream capacity is 0; requested={}",
                                    stream.requested_send_capacity
                                );

                                // Ensure that the stream is waiting for
                                // connection level capacity
                                //
                                // TODO: uncomment
                                // debug_assert!(stream.is_pending_send_capacity);

                                // The stream has no more capacity, this can
                                // happen if the remote reduced the stream
                                // window. In this case, we need to buffer the
                                // frame and wait for a window update...
                                stream.pending_send.push_front(buffer, frame.into());

                                continue;
                            }

                            // Only send up to the max frame length
                            let len = cmp::min(sz, max_len);

                            // Only send up to the stream's window capacity
                            let len =
                                cmp::min(len, stream_capacity.as_size() as usize) as WindowSize;

                            // There *must* be be enough connection level
                            // capacity at this point.
                            debug_assert!(len <= self.flow.window_size());

                            log::trace!(" --> sending data frame; len={}", len);

                            // Update the flow control
                            log::trace!(" -- updating stream flow --");
                            stream.send_flow.send_data(len);

                            // Decrement the stream's buffered data counter
                            debug_assert!(stream.buffered_send_data >= len);
                            stream.buffered_send_data -= len;
                            stream.requested_send_capacity -= len;

                            // Assign the capacity back to the connection that
                            // was just consumed from the stream in the previous
                            // line.
                            self.flow.assign_capacity(len);

                            log::trace!(" -- updating connection flow --");
                            self.flow.send_data(len);

                            // Wrap the frame's data payload to ensure that the
                            // correct amount of data gets written.

                            let eos = frame.is_end_stream();
                            let len = len as usize;

                            if frame.payload().remaining() > len {
                                frame.set_end_stream(false);
                            }

                            Frame::Data(frame.map(|buf| Prioritized {
                                inner: buf.take(len),
                                end_of_stream: eos,
                                stream: stream.key(),
                            }))
                        }
                        Some(Frame::PushPromise(pp)) => {
                            let mut pushed =
                                stream.store_mut().find_mut(&pp.promised_id()).unwrap();
                            pushed.is_pending_push = false;
                            // Transition stream from pending_push to pending_open
                            // if possible
                            if !pushed.pending_send.is_empty() {
                                if counts.can_inc_num_send_streams() {
                                    counts.inc_num_send_streams(&mut pushed);
                                    self.pending_send.push(&mut pushed);
                                } else {
                                    self.queue_open(&mut pushed);
                                }
                            }
                            Frame::PushPromise(pp)
                        }
                        Some(frame) => frame.map(|_| {
                            unreachable!(
                                "Frame::map closure will only be called \
                                 on DATA frames."
                            )
                        }),
                        None => {
                            if let Some(reason) = stream.state.get_scheduled_reset() {
                                stream.state.set_reset(reason);

                                let frame = frame::Reset::new(stream.id, reason);
                                Frame::Reset(frame)
                            } else {
                                // If the stream receives a RESET from the peer, it may have
                                // had data buffered to be sent, but all the frames are cleared
                                // in clear_queue(). Instead of doing O(N) traversal through queue
                                // to remove, lets just ignore the stream here.
                                log::trace!("removing dangling stream from pending_send");
                                // Since this should only happen as a consequence of `clear_queue`,
                                // we must be in a closed state of some kind.
                                debug_assert!(stream.state.is_closed());
                                counts.transition_after(stream, is_pending_reset);
                                continue;
                            }
                        }
                    };

                    log::trace!("pop_frame; frame={:?}", frame);

                    if cfg!(debug_assertions) && stream.state.is_idle() {
                        debug_assert!(stream.id > self.last_opened_id);
                        self.last_opened_id = stream.id;
                    }

                    if !stream.pending_send.is_empty() || stream.state.is_scheduled_reset() {
                        // TODO: Only requeue the sender IF it is ready to send
                        // the next frame. i.e. don't requeue it if the next
                        // frame is a data frame and the stream does not have
                        // any more capacity.
                        self.pending_send.push(&mut stream);
                    }

                    counts.transition_after(stream, is_pending_reset);

                    return Some(frame);
                }
                None => return None,
            }
        }
    }

    fn schedule_pending_open(&mut self, store: &mut Store, counts: &mut Counts) {
        log::trace!("schedule_pending_open");
        // check for any pending open streams
        while counts.can_inc_num_send_streams() {
            if let Some(mut stream) = self.pending_open.pop(store) {
                log::trace!("schedule_pending_open; stream={:?}", stream.id);

                counts.inc_num_send_streams(&mut stream);
                self.pending_send.push(&mut stream);
                stream.notify_send();
            } else {
                return;
            }
        }
    }
}

// ===== impl Prioritized =====

impl<B> Buf for Prioritized<B>
where
    B: Buf,
{
    fn remaining(&self) -> usize {
        self.inner.remaining()
    }

    fn bytes(&self) -> &[u8] {
        self.inner.bytes()
    }

    fn advance(&mut self, cnt: usize) {
        self.inner.advance(cnt)
    }
}

impl<B: Buf> fmt::Debug for Prioritized<B> {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Prioritized")
            .field("remaining", &self.inner.get_ref().remaining())
            .field("end_of_stream", &self.end_of_stream)
            .field("stream", &self.stream)
            .finish()
    }
}
