use {client, proto, server};
use codec::{Codec, RecvError, SendError, UserError};
use frame::{self, Frame, Reason};
use proto::{peer, Peer, Open, WindowSize};
use super::{Buffer, Config, Counts, Prioritized, Recv, Send, Stream, StreamId};
use super::recv::RecvHeaderBlockError;
use super::store::{self, Entry, Resolve, Store};

use bytes::{Buf, Bytes};
use futures::{task, Async, Poll};
use http::{HeaderMap, Request, Response};
use tokio_io::AsyncWrite;

use std::{fmt, io};
use std::sync::{Arc, Mutex};

#[derive(Debug)]
pub(crate) struct Streams<B, P>
where
    P: Peer,
{
    /// Holds most of the connection and stream related state for processing
    /// HTTP/2.0 frames associated with streams.
    inner: Arc<Mutex<Inner>>,

    /// This is the queue of frames to be written to the wire. This is split out
    /// to avoid requiring a `B` generic on all public API types even if `B` is
    /// not technically required.
    ///
    /// Currently, splitting this out requires a second `Arc` + `Mutex`.
    /// However, it should be possible to avoid this duplication with a little
    /// bit of unsafe code. This optimization has been postponed until it has
    /// been shown to be necessary.
    send_buffer: Arc<SendBuffer<B>>,

    _p: ::std::marker::PhantomData<P>,
}

/// Reference to the stream state
#[derive(Debug)]
pub(crate) struct StreamRef<B> {
    opaque: OpaqueStreamRef,
    send_buffer: Arc<SendBuffer<B>>,
}

/// Reference to the stream state that hides the send data chunk generic
pub(crate) struct OpaqueStreamRef {
    inner: Arc<Mutex<Inner>>,
    key: store::Key,
}

/// Fields needed to manage state related to managing the set of streams. This
/// is mostly split out to make ownership happy.
///
/// TODO: better name
#[derive(Debug)]
struct Inner {
    /// Tracks send & recv stream concurrency.
    counts: Counts,

    /// Connection level state and performs actions on streams
    actions: Actions,

    /// Stores stream state
    store: Store,
}

#[derive(Debug)]
struct Actions {
    /// Manages state transitions initiated by receiving frames
    recv: Recv,

    /// Manages state transitions initiated by sending frames
    send: Send,

    /// Task that calls `poll_complete`.
    task: Option<task::Task>,

    /// If the connection errors, a copy is kept for any StreamRefs.
    conn_error: Option<proto::Error>,
}

/// Contains the buffer of frames to be written to the wire.
#[derive(Debug)]
struct SendBuffer<B> {
    inner: Mutex<Buffer<Frame<B>>>,
}

// ===== impl Streams =====

impl<B, P> Streams<B, P>
where
    B: Buf,
    P: Peer,
{
    pub fn new(config: Config) -> Self {
        let peer = P::dyn();

        Streams {
            inner: Arc::new(Mutex::new(Inner {
                counts: Counts::new(peer, &config),
                actions: Actions {
                    recv: Recv::new(peer, &config),
                    send: Send::new(&config),
                    task: None,
                    conn_error: None,
                },
                store: Store::new(),
            })),
            send_buffer: Arc::new(SendBuffer::new()),
            _p: ::std::marker::PhantomData,
        }
    }

    pub fn set_target_connection_window_size(&mut self, size: WindowSize) {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        me.actions
            .recv
            .set_target_connection_window(size, &mut me.actions.task)
    }

    /// Process inbound headers
    pub fn recv_headers(&mut self, frame: frame::Headers) -> Result<(), RecvError> {
        let id = frame.stream_id();
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        // The GOAWAY process has begun. All streams with a greater ID than
        // specified as part of GOAWAY should be ignored.
        if id > me.actions.recv.max_stream_id() {
            trace!("id ({:?}) > max_stream_id ({:?}), ignoring HEADERS", id, me.actions.recv.max_stream_id());
            return Ok(());
        }

        let key = match me.store.find_entry(id) {
            Entry::Occupied(e) => e.key(),
            Entry::Vacant(e) => match me.actions.recv.open(id, Open::Headers, &mut me.counts)? {
                Some(stream_id) => {
                    let stream = Stream::new(
                        stream_id,
                        me.actions.send.init_window_sz(),
                        me.actions.recv.init_window_sz(),
                    );

                    e.insert(stream)
                },
                None => return Ok(()),
            },
        };

        let stream = me.store.resolve(key);

        if stream.state.is_local_reset() {
            // Locally reset streams must ignore frames "for some time".
            // This is because the remote may have sent trailers before
            // receiving the RST_STREAM frame.
            trace!("recv_headers; ignoring trailers on {:?}", stream.id);
            return Ok(());
        }

        let actions = &mut me.actions;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.transition(stream, |counts, stream| {
            trace!(
                "recv_headers; stream={:?}; state={:?}",
                stream.id,
                stream.state
            );

            let res = if stream.state.is_recv_headers() {
                match actions.recv.recv_headers(frame, stream, counts) {
                    Ok(()) => Ok(()),
                    Err(RecvHeaderBlockError::Oversize(resp)) => {
                        if let Some(resp) = resp {
                            let _ = actions.send.send_headers(
                                resp, send_buffer, stream, counts, &mut actions.task);

                            actions.send.schedule_implicit_reset(
                                stream,
                                Reason::REFUSED_STREAM,
                                counts,
                                &mut actions.task);

                            actions.recv.enqueue_reset_expiration(stream, counts);

                            Ok(())
                        } else {
                            Err(RecvError::Stream {
                                id: stream.id,
                                reason: Reason::REFUSED_STREAM,
                            })
                        }
                    },
                    Err(RecvHeaderBlockError::State(err)) => Err(err),
                }
            } else {
                if !frame.is_end_stream() {
                    // TODO: Is this the right error
                    return Err(RecvError::Connection(Reason::PROTOCOL_ERROR));
                }

                actions.recv.recv_trailers(frame, stream)
            };

            actions.reset_on_recv_stream_err(send_buffer, stream, counts, res)
        })
    }

    pub fn recv_data(&mut self, frame: frame::Data) -> Result<(), RecvError> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let id = frame.stream_id();

        let stream = match me.store.find_mut(&id) {
            Some(stream) => stream,
            None => {
                // The GOAWAY process has begun. All streams with a greater ID
                // than specified as part of GOAWAY should be ignored.
                if id > me.actions.recv.max_stream_id() {
                    trace!("id ({:?}) > max_stream_id ({:?}), ignoring DATA", id, me.actions.recv.max_stream_id());
                    return Ok(());
                }

                trace!("recv_data; stream not found: {:?}", id);
                return Err(RecvError::Connection(Reason::PROTOCOL_ERROR));
            },
        };

        let actions = &mut me.actions;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.transition(stream, |counts, stream| {
            let sz = frame.payload().len();
            let res = actions.recv.recv_data(frame, stream);

            // Any stream error after receiving a DATA frame means
            // we won't give the data to the user, and so they can't
            // release the capacity. We do it automatically.
            if let Err(RecvError::Stream { .. }) = res {
                actions.recv.release_connection_capacity(sz as WindowSize, &mut None);
            }

            actions.reset_on_recv_stream_err(send_buffer, stream, counts, res)
        })
    }

    pub fn recv_reset(&mut self, frame: frame::Reset) -> Result<(), RecvError> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let id = frame.stream_id();

        if id.is_zero() {
            return Err(RecvError::Connection(Reason::PROTOCOL_ERROR));
        }

        // The GOAWAY process has begun. All streams with a greater ID than
        // specified as part of GOAWAY should be ignored.
        if id > me.actions.recv.max_stream_id() {
            trace!("id ({:?}) > max_stream_id ({:?}), ignoring RST_STREAM", id, me.actions.recv.max_stream_id());
            return Ok(());
        }

        let stream = match me.store.find_mut(&id) {
            Some(stream) => stream,
            None => {
                // TODO: Are there other error cases?
                me.actions
                    .ensure_not_idle(me.counts.peer(), id)
                    .map_err(RecvError::Connection)?;

                return Ok(());
            },
        };

        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        let actions = &mut me.actions;

        me.counts.transition(stream, |_, stream| {
            actions.recv.recv_reset(frame, stream);
            actions.send.recv_reset(send_buffer, stream);
            assert!(stream.state.is_closed());
            Ok(())
        })
    }

    /// Handle a received error and return the ID of the last processed stream.
    pub fn recv_err(&mut self, err: &proto::Error) -> StreamId {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let actions = &mut me.actions;
        let counts = &mut me.counts;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        let last_processed_id = actions.recv.last_processed_id();

        me.store
            .for_each(|stream| {
                counts.transition(stream, |counts, stream| {
                    actions.recv.recv_err(err, &mut *stream);
                    actions.send.recv_err(send_buffer, stream, counts);
                    Ok::<_, ()>(())
                })
            })
            .unwrap();

        actions.conn_error = Some(err.shallow_clone());

        last_processed_id
    }

    pub fn recv_go_away(&mut self, frame: &frame::GoAway) -> Result<(), RecvError> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let actions = &mut me.actions;
        let counts = &mut me.counts;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        let last_stream_id = frame.last_stream_id();
        let err = frame.reason().into();

        if last_stream_id > actions.recv.max_stream_id() {
            // The remote endpoint sent a `GOAWAY` frame indicating a stream
            // that we never sent, or that we have already terminated on account
            // of previous `GOAWAY` frame. In either case, that is illegal.
            // (When sending multiple `GOAWAY`s, "Endpoints MUST NOT increase
            // the value they send in the last stream identifier, since the
            // peers might already have retried unprocessed requests on another
            // connection.")
            return Err(RecvError::Connection(Reason::PROTOCOL_ERROR));
        }

        actions.recv.go_away(last_stream_id);

        me.store
            .for_each(|stream| if stream.id > last_stream_id {
                counts.transition(stream, |counts, stream| {
                    actions.recv.recv_err(&err, &mut *stream);
                    actions.send.recv_err(send_buffer, stream, counts);
                    Ok::<_, ()>(())
                })
            } else {
                Ok::<_, ()>(())
            })
            .unwrap();

        actions.conn_error = Some(err);

        Ok(())
    }

    pub fn last_processed_id(&self) -> StreamId {
        self.inner.lock().unwrap().actions.recv.last_processed_id()
    }

    pub fn recv_window_update(&mut self, frame: frame::WindowUpdate) -> Result<(), RecvError> {
        let id = frame.stream_id();
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        if id.is_zero() {
            me.actions
                .send
                .recv_connection_window_update(frame, &mut me.store, &mut me.counts)
                .map_err(RecvError::Connection)?;
        } else {
            // The remote may send window updates for streams that the local now
            // considers closed. It's ok...
            if let Some(mut stream) = me.store.find_mut(&id) {
                // This result is ignored as there is nothing to do when there
                // is an error. The stream is reset by the function on error and
                // the error is informational.
                let _ = me.actions.send.recv_stream_window_update(
                    frame.size_increment(),
                    send_buffer,
                    &mut stream,
                    &mut me.counts,
                    &mut me.actions.task,
                );
            } else {
                me.actions
                    .recv
                    .ensure_not_idle(id)
                    .map_err(RecvError::Connection)?;
            }
        }

        Ok(())
    }

    pub fn recv_push_promise(&mut self, frame: frame::PushPromise) -> Result<(), RecvError> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let id = frame.stream_id();
        let promised_id = frame.promised_id();

        // First, ensure that the initiating stream is still in a valid state.
        let parent_key = match me.store.find_mut(&id) {
            Some(stream) => {
                // The GOAWAY process has begun. All streams with a greater ID
                // than specified as part of GOAWAY should be ignored.
                if id > me.actions.recv.max_stream_id() {
                    trace!("id ({:?}) > max_stream_id ({:?}), ignoring PUSH_PROMISE", id, me.actions.recv.max_stream_id());
                    return Ok(());
                }

                // The stream must be receive open
                stream.state.ensure_recv_open()?;
                stream.key()
            }
            None => return Err(RecvError::Connection(Reason::PROTOCOL_ERROR)),
        };

        // Ensure that we can reserve streams
        me.actions.recv.ensure_can_reserve()?;

        // Next, open the stream.
        //
        // If `None` is returned, then the stream is being refused. There is no
        // further work to be done.
        if me.actions.recv.open(promised_id, Open::PushPromise, &mut me.counts)?.is_none() {
            return Ok(());
        }

        // Create a scope
        let child_key = {
            // Create state for the stream
            let stream = me.store.insert(promised_id, {
                Stream::new(
                    promised_id,
                    me.actions.send.init_window_sz(),
                    me.actions.recv.init_window_sz())
            });

            let actions = &mut me.actions;

            me.counts.transition(stream, |counts, stream| {
                let res = actions.recv.recv_push_promise(frame, stream);

                let mut send_buffer = self.send_buffer.inner.lock().unwrap();
                actions.reset_on_recv_stream_err(&mut *send_buffer, stream, counts, res)
                    .map(|_| stream.key())
            })?
        };

        // Push the stream... this requires a bit of indirection to make
        // the borrow checker happy.
        let mut ppp = me.store[parent_key].pending_push_promises.take();
        ppp.push(&mut me.store.resolve(child_key));

        let parent = &mut me.store[parent_key];

        parent.pending_push_promises = ppp;
        parent.notify_recv();

        Ok(())
    }

    pub fn next_incoming(&mut self) -> Option<StreamRef<B>> {
        let key = {
            let mut me = self.inner.lock().unwrap();
            let me = &mut *me;

            match me.actions.recv.next_incoming(&mut me.store) {
                Some(key) => {
                    let mut stream = me.store.resolve(key);
                    trace!("next_incoming; id={:?}, state={:?}", stream.id, stream.state);
                    // Increment the ref count
                    stream.ref_inc();

                    // Return the key
                    Some(key)
                },
                None => None,
            }
        };

        key.map(|key| {
            StreamRef {
                opaque: OpaqueStreamRef {
                    inner: self.inner.clone(),
                    key,
                },
                send_buffer: self.send_buffer.clone(),
            }
        })
    }

    pub fn send_pending_refusal<T>(
        &mut self,
        dst: &mut Codec<T, Prioritized<B>>,
    ) -> Poll<(), io::Error>
    where
        T: AsyncWrite,
    {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;
        me.actions.recv.send_pending_refusal(dst)
    }

    pub fn clear_expired_reset_streams(&mut self) {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;
        me.actions.recv.clear_expired_reset_streams(&mut me.store, &mut me.counts);
    }

    pub fn poll_complete<T>(&mut self, dst: &mut Codec<T, Prioritized<B>>) -> Poll<(), io::Error>
    where
        T: AsyncWrite,
    {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        // Send WINDOW_UPDATE frames first
        //
        // TODO: It would probably be better to interleave updates w/ data
        // frames.
        try_ready!(me.actions.recv.poll_complete(&mut me.store, &mut me.counts, dst));

        // Send any other pending frames
        try_ready!(me.actions.send.poll_complete(
            send_buffer,
            &mut me.store,
            &mut me.counts,
            dst
        ));

        // Nothing else to do, track the task
        me.actions.task = Some(task::current());

        Ok(().into())
    }

    pub fn apply_remote_settings(&mut self, frame: &frame::Settings) -> Result<(), RecvError> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.apply_remote_settings(frame);

        me.actions.send.apply_remote_settings(
            frame, send_buffer, &mut me.store, &mut me.counts, &mut me.actions.task)
    }

    pub fn send_request(
        &mut self,
        request: Request<()>,
        end_of_stream: bool,
        pending: Option<&OpaqueStreamRef>,
    ) -> Result<StreamRef<B>, SendError> {
        use http::Method;
        use super::stream::ContentLength;

        // TODO: There is a hazard with assigning a stream ID before the
        // prioritize layer. If prioritization reorders new streams, this
        // implicitly closes the earlier stream IDs.
        //
        // See: carllerche/h2#11
        let key = {
            let mut me = self.inner.lock().unwrap();
            let me = &mut *me;

            let mut send_buffer = self.send_buffer.inner.lock().unwrap();
            let send_buffer = &mut *send_buffer;

            me.actions.ensure_no_conn_error()?;
            me.actions.send.ensure_next_stream_id()?;

            // The `pending` argument is provided by the `Client`, and holds
            // a store `Key` of a `Stream` that may have been not been opened
            // yet.
            //
            // If that stream is still pending, the Client isn't allowed to
            // queue up another pending stream. They should use `poll_ready`.
            if let Some(stream) = pending {
                if me.store.resolve(stream.key).is_pending_open {
                    return Err(UserError::Rejected.into());
                }
            }

            if me.counts.peer().is_server() {
                // Servers cannot open streams. PushPromise must first be reserved.
                return Err(UserError::UnexpectedFrameType.into());
            }

            let stream_id = me.actions.send.open()?;

            let mut stream = Stream::new(
                stream_id,
                me.actions.send.init_window_sz(),
                me.actions.recv.init_window_sz(),
            );

            if *request.method() == Method::HEAD {
                stream.content_length = ContentLength::Head;
            }

            // Convert the message
            let headers = client::Peer::convert_send_message(
                stream_id, request, end_of_stream)?;

            let mut stream = me.store.insert(stream.id, stream);

            let sent = me.actions.send.send_headers(
                headers,
                send_buffer,
                &mut stream,
                &mut me.counts,
                &mut me.actions.task,
            );

            // send_headers can return a UserError, if it does,
            // we should forget about this stream.
            if let Err(err) = sent {
                stream.unlink();
                stream.remove();
                return Err(err.into());
            }

            // Given that the stream has been initialized, it should not be in the
            // closed state.
            debug_assert!(!stream.state.is_closed());

            // Increment the stream ref count as we will be returning a handle.
            stream.ref_inc();

            stream.key()
        };

        Ok(StreamRef {
            opaque: OpaqueStreamRef {
                inner: self.inner.clone(),
                key: key,
            },
            send_buffer: self.send_buffer.clone(),
        })
    }

    pub fn send_reset(&mut self, id: StreamId, reason: Reason) {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let key = match me.store.find_entry(id) {
            Entry::Occupied(e) => e.key(),
            Entry::Vacant(e) => match me.actions.recv.open(id, Open::Headers, &mut me.counts) {
                Ok(Some(stream_id)) => {
                    let stream = Stream::new(stream_id, 0, 0);

                    e.insert(stream)
                },
                _ => return,
            },
        };

        let stream = me.store.resolve(key);
        let actions = &mut me.actions;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.transition(stream, |counts, stream| {
            actions.send.send_reset(
                reason, send_buffer, stream, counts, &mut actions.task);
            actions.recv.enqueue_reset_expiration(stream, counts)
        })
    }

    pub fn send_go_away(&mut self, last_processed_id: StreamId) {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;
        let actions = &mut me.actions;
        actions.recv.go_away(last_processed_id);
    }
}

impl<B> Streams<B, client::Peer>
where
    B: Buf,
{
    pub fn poll_pending_open(&mut self, pending: Option<&OpaqueStreamRef>) -> Poll<(), ::Error> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        me.actions.ensure_no_conn_error()?;
        me.actions.send.ensure_next_stream_id()?;

        if let Some(pending) = pending {
            let mut stream = me.store.resolve(pending.key);
            trace!("poll_pending_open; stream = {:?}", stream.is_pending_open);
            if stream.is_pending_open {
                stream.wait_send();
                return Ok(Async::NotReady);
            }
        }
        Ok(().into())
    }
}

impl<B, P> Streams<B, P>
where
    P: Peer,
{
    /// This function is safe to call multiple times.
    ///
    /// A `Result` is returned to avoid panicking if the mutex is poisoned.
    pub fn recv_eof(&mut self, clear_pending_accept: bool) -> Result<(), ()> {
        let mut me = self.inner.lock().map_err(|_| ())?;
        let me = &mut *me;

        let actions = &mut me.actions;
        let counts = &mut me.counts;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        if actions.conn_error.is_none() {
            actions.conn_error = Some(io::Error::from(io::ErrorKind::BrokenPipe).into());
        }

        trace!("Streams::recv_eof");

        me.store
            .for_each(|stream| {
                counts.transition(stream, |counts, stream| {
                    actions.recv.recv_eof(stream);

                    // This handles resetting send state associated with the
                    // stream
                    actions.send.recv_err(send_buffer, stream, counts);
                    Ok::<_, ()>(())
                })
            })
            .expect("recv_eof");

        actions.clear_queues(clear_pending_accept, &mut me.store, counts);
        Ok(())
    }

    pub fn num_active_streams(&self) -> usize {
        let me = self.inner.lock().unwrap();
        me.store.num_active_streams()
    }

    pub fn has_streams_or_other_references(&self) -> bool {
        if Arc::strong_count(&self.inner) > 1 {
            return true;
        }

        if Arc::strong_count(&self.send_buffer) > 1 {
            return true;
        }

        let me = self.inner.lock().unwrap();
        me.counts.has_streams()
    }

    #[cfg(feature = "unstable")]
    pub fn num_wired_streams(&self) -> usize {
        let me = self.inner.lock().unwrap();
        me.store.num_wired_streams()
    }
}

// no derive because we don't need B and P to be Clone.
impl<B, P> Clone for Streams<B, P>
where
    P: Peer,
{
    fn clone(&self) -> Self {
        Streams {
            inner: self.inner.clone(),
            send_buffer: self.send_buffer.clone(),
            _p: ::std::marker::PhantomData,
        }
    }
}

// ===== impl StreamRef =====

impl<B> StreamRef<B> {
    pub fn send_data(&mut self, data: B, end_stream: bool) -> Result<(), UserError>
    where
        B: Buf,
    {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let stream = me.store.resolve(self.opaque.key);
        let actions = &mut me.actions;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.transition(stream, |counts, stream| {
            // Create the data frame
            let mut frame = frame::Data::new(stream.id, data);
            frame.set_end_stream(end_stream);

            // Send the data frame
            actions.send.send_data(
                frame,
                send_buffer,
                stream,
                counts,
                &mut actions.task)
        })
    }

    pub fn send_trailers(&mut self, trailers: HeaderMap) -> Result<(), UserError> {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let stream = me.store.resolve(self.opaque.key);
        let actions = &mut me.actions;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.transition(stream, |counts, stream| {
            // Create the trailers frame
            let frame = frame::Headers::trailers(stream.id, trailers);

            // Send the trailers frame
            actions.send.send_trailers(
                frame, send_buffer, stream, counts, &mut actions.task)
        })
    }

    pub fn send_reset(&mut self, reason: Reason) {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let stream = me.store.resolve(self.opaque.key);
        let actions = &mut me.actions;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.transition(stream, |counts, stream| {
            actions.send.send_reset(
                reason, send_buffer, stream, counts, &mut actions.task)
        })
    }

    pub fn send_response(
        &mut self,
        response: Response<()>,
        end_of_stream: bool,
    ) -> Result<(), UserError> {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let stream = me.store.resolve(self.opaque.key);
        let actions = &mut me.actions;
        let mut send_buffer = self.send_buffer.inner.lock().unwrap();
        let send_buffer = &mut *send_buffer;

        me.counts.transition(stream, |counts, stream| {
            let frame = server::Peer::convert_send_message(stream.id, response, end_of_stream);

            actions.send.send_headers(
                frame, send_buffer, stream, counts, &mut actions.task)
        })
    }

    /// Called by the server after the stream is accepted. Given that clients
    /// initialize streams by sending HEADERS, the request will always be
    /// available.
    ///
    /// # Panics
    ///
    /// This function panics if the request isn't present.
    pub fn take_request(&self) -> Request<()> {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.opaque.key);
        me.actions.recv.take_request(&mut stream)
    }

    /// Called by a client to see if the current stream is pending open
    pub fn is_pending_open(&self) -> bool {
        let mut me = self.opaque.inner.lock().unwrap();
        me.store.resolve(self.opaque.key).is_pending_open
    }

    /// Request capacity to send data
    pub fn reserve_capacity(&mut self, capacity: WindowSize) {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.opaque.key);

        me.actions.send.reserve_capacity(capacity, &mut stream, &mut me.counts)
    }

    /// Returns the stream's current send capacity.
    pub fn capacity(&self) -> WindowSize {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.opaque.key);

        me.actions.send.capacity(&mut stream)
    }

    /// Request to be notified when the stream's capacity increases
    pub fn poll_capacity(&mut self) -> Poll<Option<WindowSize>, UserError> {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.opaque.key);

        me.actions.send.poll_capacity(&mut stream)
    }

    /// Request to be notified for if a `RST_STREAM` is received for this stream.
    pub(crate) fn poll_reset(&mut self, mode: proto::PollReset) -> Poll<Reason, ::Error> {
        let mut me = self.opaque.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.opaque.key);

        me.actions.send.poll_reset(&mut stream, mode)
            .map_err(From::from)
    }

    pub fn clone_to_opaque(&self) -> OpaqueStreamRef
        where B: 'static,
    {
        self.opaque.clone()
    }

    pub fn stream_id(&self) -> StreamId {
        self.opaque.stream_id()
    }
}

impl<B> Clone for StreamRef<B> {
    fn clone(&self) -> Self {
        StreamRef {
            opaque: self.opaque.clone(),
            send_buffer: self.send_buffer.clone(),
        }
    }
}

// ===== impl OpaqueStreamRef =====

impl OpaqueStreamRef {
    /// Called by a client to check for a received response.
    pub fn poll_response(&mut self) -> Poll<Response<()>, proto::Error> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.key);

        me.actions.recv.poll_response(&mut stream)
    }

    pub fn body_is_empty(&self) -> bool {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let stream = me.store.resolve(self.key);

        me.actions.recv.body_is_empty(&stream)
    }

    pub fn is_end_stream(&self) -> bool {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let stream = me.store.resolve(self.key);

        me.actions.recv.is_end_stream(&stream)
    }

    pub fn poll_data(&mut self) -> Poll<Option<Bytes>, proto::Error> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.key);

        me.actions.recv.poll_data(&mut stream)
    }

    pub fn poll_trailers(&mut self) -> Poll<Option<HeaderMap>, proto::Error> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.key);

        me.actions.recv.poll_trailers(&mut stream)
    }

    /// Releases recv capacity back to the peer. This may result in sending
    /// WINDOW_UPDATE frames on both the stream and connection.
    pub fn release_capacity(&mut self, capacity: WindowSize) -> Result<(), UserError> {
        let mut me = self.inner.lock().unwrap();
        let me = &mut *me;

        let mut stream = me.store.resolve(self.key);

        me.actions
            .recv
            .release_capacity(capacity, &mut stream, &mut me.actions.task)
    }

    pub fn stream_id(&self) -> StreamId {
        self.inner.lock()
            .unwrap()
            .store[self.key]
            .id
    }
}

impl fmt::Debug for OpaqueStreamRef {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self.inner.lock() {
            Ok(me) => {
                let stream = &me.store[self.key];
                fmt.debug_struct("OpaqueStreamRef")
                    .field("stream_id", &stream.id)
                    .field("ref_count", &stream.ref_count)
                    .finish()
            },
            Err(_poisoned) => fmt.debug_struct("OpaqueStreamRef")
                .field("inner", &"<Poisoned>")
                .finish(),
        }
    }
}

impl Clone for OpaqueStreamRef {
    fn clone(&self) -> Self {
        // Increment the ref count
        self.inner.lock().unwrap().store.resolve(self.key).ref_inc();

        OpaqueStreamRef {
            inner: self.inner.clone(),
            key: self.key.clone(),
        }
    }
}

impl Drop for OpaqueStreamRef {
    fn drop(&mut self) {
        drop_stream_ref(&self.inner, self.key);
    }
}

// TODO: Move back in fn above
fn drop_stream_ref(inner: &Mutex<Inner>, key: store::Key) {
    let mut me = match inner.lock() {
        Ok(inner) => inner,
        Err(_) => if ::std::thread::panicking() {
            trace!("StreamRef::drop; mutex poisoned");
            return;
        } else {
            panic!("StreamRef::drop; mutex poisoned");
        },
    };

    let me = &mut *me;

    let mut stream = me.store.resolve(key);

    trace!("drop_stream_ref; stream={:?}", stream);

    // decrement the stream's ref count by 1.
    stream.ref_dec();

    let actions = &mut me.actions;

    // If the stream is not referenced and it is already
    // closed (does not have to go through logic below
    // of canceling the stream), we should notify the task
    // (connection) so that it can close properly
    if stream.ref_count == 0 && stream.is_closed() {
        if let Some(task) = actions.task.take() {
            task.notify();
        }
    }

    me.counts.transition(stream, |counts, stream| {
        maybe_cancel(stream, actions, counts);

        if stream.ref_count == 0 {
            let mut ppp = stream.pending_push_promises.take();
            while let Some(promise) = ppp.pop(stream.store_mut()) {
                counts.transition(promise, |counts, stream| {
                    maybe_cancel(stream, actions, counts);
                });
            }
        }
    });
}

fn maybe_cancel(stream: &mut store::Ptr, actions: &mut Actions, counts: &mut Counts) {
    if stream.is_canceled_interest() {
        actions.send.schedule_implicit_reset(
            stream,
            Reason::CANCEL,
            counts,
            &mut actions.task);
        actions.recv.enqueue_reset_expiration(stream, counts);
    }
}

// ===== impl SendBuffer =====

impl<B> SendBuffer<B> {
    fn new() -> Self {
        let inner = Mutex::new(Buffer::new());
        SendBuffer { inner }
    }
}

// ===== impl Actions =====

impl Actions {
    fn reset_on_recv_stream_err<B>(
        &mut self,
        buffer: &mut Buffer<Frame<B>>,
        stream: &mut store::Ptr,
        counts: &mut Counts,
        res: Result<(), RecvError>,
    ) -> Result<(), RecvError> {
        if let Err(RecvError::Stream {
            reason, ..
        }) = res
        {
            // Reset the stream.
            self.send.send_reset(reason, buffer, stream, counts, &mut self.task);
            Ok(())
        } else {
            res
        }
    }

    fn ensure_not_idle(&mut self, peer: peer::Dyn, id: StreamId) -> Result<(), Reason> {
        if peer.is_local_init(id) {
            self.send.ensure_not_idle(id)
        } else {
            self.recv.ensure_not_idle(id)
        }
    }

    fn ensure_no_conn_error(&self) -> Result<(), proto::Error> {
        if let Some(ref err) = self.conn_error {
            Err(err.shallow_clone())
        } else {
            Ok(())
        }
    }

    fn clear_queues(&mut self,
                    clear_pending_accept: bool,
                    store: &mut Store,
                    counts: &mut Counts)
    {
        self.recv.clear_queues(clear_pending_accept, store, counts);
        self.send.clear_queues(store, counts);
    }
}
