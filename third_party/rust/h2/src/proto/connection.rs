use {client, frame, proto, server};
use codec::RecvError;
use frame::{Reason, StreamId};

use frame::DEFAULT_INITIAL_WINDOW_SIZE;
use proto::*;

use bytes::{Bytes, IntoBuf};
use futures::Stream;
use tokio_io::{AsyncRead, AsyncWrite};

use std::marker::PhantomData;
use std::io;
use std::time::Duration;

/// An H2 connection
#[derive(Debug)]
pub(crate) struct Connection<T, P, B: IntoBuf = Bytes>
where
    P: Peer,
{
    /// Tracks the connection level state transitions.
    state: State,

    /// An error to report back once complete.
    ///
    /// This exists separately from State in order to support
    /// graceful shutdown.
    error: Option<Reason>,

    /// Read / write frame values
    codec: Codec<T, Prioritized<B::Buf>>,

    /// Pending GOAWAY frames to write.
    go_away: GoAway,

    /// Ping/pong handler
    ping_pong: PingPong,

    /// Connection settings
    settings: Settings,

    /// Stream state handler
    streams: Streams<B::Buf, P>,

    /// Client or server
    _phantom: PhantomData<P>,
}

#[derive(Debug, Clone)]
pub(crate) struct Config {
    pub next_stream_id: StreamId,
    pub initial_max_send_streams: usize,
    pub reset_stream_duration: Duration,
    pub reset_stream_max: usize,
    pub settings: frame::Settings,
}

#[derive(Debug)]
enum State {
    /// Currently open in a sane state
    Open,

    /// The codec must be flushed
    Closing(Reason),

    /// In a closed state
    Closed(Reason),
}

impl<T, P, B> Connection<T, P, B>
where
    T: AsyncRead + AsyncWrite,
    P: Peer,
    B: IntoBuf,
{
    pub fn new(
        codec: Codec<T, Prioritized<B::Buf>>,
        config: Config,
    ) -> Connection<T, P, B> {
        let streams = Streams::new(streams::Config {
            local_init_window_sz: config.settings
                .initial_window_size()
                .unwrap_or(DEFAULT_INITIAL_WINDOW_SIZE),
            initial_max_send_streams: config.initial_max_send_streams,
            local_next_stream_id: config.next_stream_id,
            local_push_enabled: config.settings.is_push_enabled(),
            local_reset_duration: config.reset_stream_duration,
            local_reset_max: config.reset_stream_max,
            remote_init_window_sz: DEFAULT_INITIAL_WINDOW_SIZE,
            remote_max_initiated: config.settings
                .max_concurrent_streams()
                .map(|max| max as usize),
        });
        Connection {
            state: State::Open,
            error: None,
            codec: codec,
            go_away: GoAway::new(),
            ping_pong: PingPong::new(),
            settings: Settings::new(),
            streams: streams,
            _phantom: PhantomData,
        }
    }

    pub fn set_target_window_size(&mut self, size: WindowSize) {
        self.streams.set_target_connection_window_size(size);
    }

    /// Returns `Ready` when the connection is ready to receive a frame.
    ///
    /// Returns `RecvError` as this may raise errors that are caused by delayed
    /// processing of received frames.
    fn poll_ready(&mut self) -> Poll<(), RecvError> {
        // The order of these calls don't really matter too much
        try_ready!(self.ping_pong.send_pending_pong(&mut self.codec));
        try_ready!(self.ping_pong.send_pending_ping(&mut self.codec));
        try_ready!(
            self.settings
                .send_pending_ack(&mut self.codec, &mut self.streams)
        );
        try_ready!(self.streams.send_pending_refusal(&mut self.codec));

        Ok(().into())
    }

    /// Send any pending GOAWAY frames.
    ///
    /// This will return `Some(reason)` if the connection should be closed
    /// afterwards. If this is a graceful shutdown, this returns `None`.
    fn poll_go_away(&mut self) -> Poll<Option<Reason>, io::Error> {
        self.go_away.send_pending_go_away(&mut self.codec)
    }

    fn go_away(&mut self, id: StreamId, e: Reason) {
        let frame = frame::GoAway::new(id, e);
        self.streams.send_go_away(id);
        self.go_away.go_away(frame);
    }

    pub fn go_away_now(&mut self, e: Reason) {
        let last_processed_id = self.streams.last_processed_id();
        let frame = frame::GoAway::new(last_processed_id, e);
        self.go_away.go_away_now(frame);
    }

    fn take_error(&mut self, ours: Reason) -> Poll<(), proto::Error> {
        let reason = if let Some(theirs) = self.error.take() {
            match (ours, theirs) {
                // If either side reported an error, return that
                // to the user.
                (Reason::NO_ERROR, err) | (err, Reason::NO_ERROR) => err,
                // If both sides reported an error, give their
                // error back to th user. We assume our error
                // was a consequence of their error, and less
                // important.
                (_, theirs) => theirs,
            }
        } else {
            ours
        };

        if reason == Reason::NO_ERROR {
            Ok(().into())
        } else {
            Err(proto::Error::Proto(reason))
        }
    }

    /// Closes the connection by transitioning to a GOAWAY state
    /// iff there are no streams or references
    pub fn maybe_close_connection_if_no_streams(&mut self) {
        // If we poll() and realize that there are no streams or references
        // then we can close the connection by transitioning to GOAWAY
        if self.streams.num_active_streams() == 0 && !self.streams.has_streams_or_other_references() {
            self.go_away_now(Reason::NO_ERROR);
        }
    }

    /// Advances the internal state of the connection.
    pub fn poll(&mut self) -> Poll<(), proto::Error> {
        use codec::RecvError::*;

        loop {
            // TODO: probably clean up this glob of code
            match self.state {
                // When open, continue to poll a frame
                State::Open => {
                    match self.poll2() {
                        // The connection has shutdown normally
                        Ok(Async::Ready(())) => return self.take_error(Reason::NO_ERROR),
                        // The connection is not ready to make progress
                        Ok(Async::NotReady) => {
                            // Ensure all window updates have been sent.
                            //
                            // This will also handle flushing `self.codec`
                            try_ready!(self.streams.poll_complete(&mut self.codec));

                            if self.error.is_some() || self.go_away.should_close_on_idle() {
                                if self.streams.num_active_streams() == 0 {
                                    self.go_away_now(Reason::NO_ERROR);
                                    continue;
                                }
                            }

                            return Ok(Async::NotReady);
                        },
                        // Attempting to read a frame resulted in a connection level
                        // error. This is handled by setting a GOAWAY frame followed by
                        // terminating the connection.
                        Err(Connection(e)) => {
                            debug!("Connection::poll; err={:?}", e);

                            // We may have already sent a GOAWAY for this error,
                            // if so, don't send another, just flush and close up.
                            if let Some(reason) = self.go_away.going_away_reason() {
                                if reason == e {
                                    trace!("    -> already going away");
                                    self.state = State::Closing(e);
                                    continue;
                                }
                            }

                            // Reset all active streams
                            self.streams.recv_err(&e.into());
                            self.go_away_now(e);
                        },
                        // Attempting to read a frame resulted in a stream level error.
                        // This is handled by resetting the frame then trying to read
                        // another frame.
                        Err(Stream {
                            id,
                            reason,
                        }) => {
                            trace!("stream level error; id={:?}; reason={:?}", id, reason);
                            self.streams.send_reset(id, reason);
                        },
                        // Attempting to read a frame resulted in an I/O error. All
                        // active streams must be reset.
                        //
                        // TODO: Are I/O errors recoverable?
                        Err(Io(e)) => {
                            let e = e.into();

                            // Reset all active streams
                            self.streams.recv_err(&e);

                            // Return the error
                            return Err(e);
                        },
                    }
                }
                State::Closing(reason) => {
                    trace!("connection closing after flush, reason={:?}", reason);
                    // Flush the codec
                    try_ready!(self.codec.flush());

                    // Transition the state to error
                    self.state = State::Closed(reason);
                },
                State::Closed(reason) => return self.take_error(reason),
            }
        }
    }

    fn poll2(&mut self) -> Poll<(), RecvError> {
        use frame::Frame::*;

        // This happens outside of the loop to prevent needing to do a clock
        // check and then comparison of the queue possibly multiple times a
        // second (and thus, the clock wouldn't have changed enough to matter).
        self.clear_expired_reset_streams();

        loop {
            // First, ensure that the `Connection` is able to receive a frame
            //
            // The order here matters:
            // - poll_go_away may buffer a graceful shutdown GOAWAY frame
            // - If it has, we've also added a PING to be sent in poll_ready
            if let Some(reason) = try_ready!(self.poll_go_away()) {
                if self.go_away.should_close_now() {
                    return Err(RecvError::Connection(reason));
                }
                // Only NO_ERROR should be waiting for idle
                debug_assert_eq!(reason, Reason::NO_ERROR, "graceful GOAWAY should be NO_ERROR");
            }
            try_ready!(self.poll_ready());

            match try_ready!(self.codec.poll()) {
                Some(Headers(frame)) => {
                    trace!("recv HEADERS; frame={:?}", frame);
                    self.streams.recv_headers(frame)?;
                },
                Some(Data(frame)) => {
                    trace!("recv DATA; frame={:?}", frame);
                    self.streams.recv_data(frame)?;
                },
                Some(Reset(frame)) => {
                    trace!("recv RST_STREAM; frame={:?}", frame);
                    self.streams.recv_reset(frame)?;
                },
                Some(PushPromise(frame)) => {
                    trace!("recv PUSH_PROMISE; frame={:?}", frame);
                    self.streams.recv_push_promise(frame)?;
                },
                Some(Settings(frame)) => {
                    trace!("recv SETTINGS; frame={:?}", frame);
                    self.settings.recv_settings(frame);
                },
                Some(GoAway(frame)) => {
                    trace!("recv GOAWAY; frame={:?}", frame);
                    // This should prevent starting new streams,
                    // but should allow continuing to process current streams
                    // until they are all EOS. Once they are, State should
                    // transition to GoAway.
                    self.streams.recv_go_away(&frame)?;
                    self.error = Some(frame.reason());
                },
                Some(Ping(frame)) => {
                    trace!("recv PING; frame={:?}", frame);
                    let status = self.ping_pong.recv_ping(frame);
                    if status.is_shutdown() {
                        assert!(
                            self.go_away.is_going_away(),
                            "received unexpected shutdown ping"
                        );

                        let last_processed_id = self.streams.last_processed_id();
                        self.go_away(last_processed_id, Reason::NO_ERROR);
                    }
                },
                Some(WindowUpdate(frame)) => {
                    trace!("recv WINDOW_UPDATE; frame={:?}", frame);
                    self.streams.recv_window_update(frame)?;
                },
                Some(Priority(frame)) => {
                    trace!("recv PRIORITY; frame={:?}", frame);
                    // TODO: handle
                },
                None => {
                    trace!("codec closed");
                    self.streams.recv_eof(false)
                        .ok().expect("mutex poisoned");
                    return Ok(Async::Ready(()));
                },
            }
        }
    }

    fn clear_expired_reset_streams(&mut self) {
        self.streams.clear_expired_reset_streams();
    }
}

impl<T, B> Connection<T, client::Peer, B>
where
    T: AsyncRead + AsyncWrite,
    B: IntoBuf,
{
    pub(crate) fn streams(&self) -> &Streams<B::Buf, client::Peer> {
        &self.streams
    }
}

impl<T, B> Connection<T, server::Peer, B>
where
    T: AsyncRead + AsyncWrite,
    B: IntoBuf,
{
    pub fn next_incoming(&mut self) -> Option<StreamRef<B::Buf>> {
        self.streams.next_incoming()
    }

    // Graceful shutdown only makes sense for server peers.
    pub fn go_away_gracefully(&mut self) {
        if self.go_away.is_going_away() {
            // No reason to start a new one.
            return;
        }

        // According to http://httpwg.org/specs/rfc7540.html#GOAWAY:
        //
        // > A server that is attempting to gracefully shut down a connection
        // > SHOULD send an initial GOAWAY frame with the last stream
        // > identifier set to 2^31-1 and a NO_ERROR code. This signals to the
        // > client that a shutdown is imminent and that initiating further
        // > requests is prohibited. After allowing time for any in-flight
        // > stream creation (at least one round-trip time), the server can
        // > send another GOAWAY frame with an updated last stream identifier.
        // > This ensures that a connection can be cleanly shut down without
        // > losing requests.
        self.go_away(StreamId::MAX, Reason::NO_ERROR);

        // We take the advice of waiting 1 RTT literally, and wait
        // for a pong before proceeding.
        self.ping_pong.ping_shutdown();
    }
}

impl<T, P, B> Drop for Connection<T, P, B>
where
    P: Peer,
    B: IntoBuf,
{
    fn drop(&mut self) {
        // Ignore errors as this indicates that the mutex is poisoned.
        let _ = self.streams.recv_eof(true);
    }
}
