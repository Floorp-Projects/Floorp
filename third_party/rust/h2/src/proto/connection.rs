use crate::codec::{RecvError, UserError};
use crate::frame::{Reason, StreamId};
use crate::{client, frame, proto, server};

use crate::frame::DEFAULT_INITIAL_WINDOW_SIZE;
use crate::proto::*;

use bytes::{Buf, Bytes};
use futures_core::Stream;
use std::io;
use std::marker::PhantomData;
use std::pin::Pin;
use std::task::{Context, Poll};
use std::time::Duration;
use tokio::io::{AsyncRead, AsyncWrite};

/// An H2 connection
#[derive(Debug)]
pub(crate) struct Connection<T, P, B: Buf = Bytes>
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
    codec: Codec<T, Prioritized<B>>,

    /// Pending GOAWAY frames to write.
    go_away: GoAway,

    /// Ping/pong handler
    ping_pong: PingPong,

    /// Connection settings
    settings: Settings,

    /// Stream state handler
    streams: Streams<B, P>,

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
    T: AsyncRead + AsyncWrite + Unpin,
    P: Peer,
    B: Buf,
{
    pub fn new(codec: Codec<T, Prioritized<B>>, config: Config) -> Connection<T, P, B> {
        let streams = Streams::new(streams::Config {
            local_init_window_sz: config
                .settings
                .initial_window_size()
                .unwrap_or(DEFAULT_INITIAL_WINDOW_SIZE),
            initial_max_send_streams: config.initial_max_send_streams,
            local_next_stream_id: config.next_stream_id,
            local_push_enabled: config.settings.is_push_enabled(),
            local_reset_duration: config.reset_stream_duration,
            local_reset_max: config.reset_stream_max,
            remote_init_window_sz: DEFAULT_INITIAL_WINDOW_SIZE,
            remote_max_initiated: config
                .settings
                .max_concurrent_streams()
                .map(|max| max as usize),
        });
        Connection {
            state: State::Open,
            error: None,
            codec,
            go_away: GoAway::new(),
            ping_pong: PingPong::new(),
            settings: Settings::new(config.settings),
            streams,
            _phantom: PhantomData,
        }
    }

    /// connection flow control
    pub(crate) fn set_target_window_size(&mut self, size: WindowSize) {
        self.streams.set_target_connection_window_size(size);
    }

    /// Send a new SETTINGS frame with an updated initial window size.
    pub(crate) fn set_initial_window_size(&mut self, size: WindowSize) -> Result<(), UserError> {
        let mut settings = frame::Settings::default();
        settings.set_initial_window_size(Some(size));
        self.settings.send_settings(settings)
    }

    /// Returns `Ready` when the connection is ready to receive a frame.
    ///
    /// Returns `RecvError` as this may raise errors that are caused by delayed
    /// processing of received frames.
    fn poll_ready(&mut self, cx: &mut Context) -> Poll<Result<(), RecvError>> {
        // The order of these calls don't really matter too much
        ready!(self.ping_pong.send_pending_pong(cx, &mut self.codec))?;
        ready!(self.ping_pong.send_pending_ping(cx, &mut self.codec))?;
        ready!(self
            .settings
            .poll_send(cx, &mut self.codec, &mut self.streams))?;
        ready!(self.streams.send_pending_refusal(cx, &mut self.codec))?;

        Poll::Ready(Ok(()))
    }

    /// Send any pending GOAWAY frames.
    ///
    /// This will return `Some(reason)` if the connection should be closed
    /// afterwards. If this is a graceful shutdown, this returns `None`.
    fn poll_go_away(&mut self, cx: &mut Context) -> Poll<Option<io::Result<Reason>>> {
        self.go_away.send_pending_go_away(cx, &mut self.codec)
    }

    fn go_away(&mut self, id: StreamId, e: Reason) {
        let frame = frame::GoAway::new(id, e);
        self.streams.send_go_away(id);
        self.go_away.go_away(frame);
    }

    fn go_away_now(&mut self, e: Reason) {
        let last_processed_id = self.streams.last_processed_id();
        let frame = frame::GoAway::new(last_processed_id, e);
        self.go_away.go_away_now(frame);
    }

    pub fn go_away_from_user(&mut self, e: Reason) {
        let last_processed_id = self.streams.last_processed_id();
        let frame = frame::GoAway::new(last_processed_id, e);
        self.go_away.go_away_from_user(frame);

        // Notify all streams of reason we're abruptly closing.
        self.streams.recv_err(&proto::Error::Proto(e));
    }

    fn take_error(&mut self, ours: Reason) -> Poll<Result<(), proto::Error>> {
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
            Poll::Ready(Ok(()))
        } else {
            Poll::Ready(Err(proto::Error::Proto(reason)))
        }
    }

    /// Closes the connection by transitioning to a GOAWAY state
    /// iff there are no streams or references
    pub fn maybe_close_connection_if_no_streams(&mut self) {
        // If we poll() and realize that there are no streams or references
        // then we can close the connection by transitioning to GOAWAY
        if !self.streams.has_streams_or_other_references() {
            self.go_away_now(Reason::NO_ERROR);
        }
    }

    pub(crate) fn take_user_pings(&mut self) -> Option<UserPings> {
        self.ping_pong.take_user_pings()
    }

    /// Advances the internal state of the connection.
    pub fn poll(&mut self, cx: &mut Context) -> Poll<Result<(), proto::Error>> {
        use crate::codec::RecvError::*;

        loop {
            // TODO: probably clean up this glob of code
            match self.state {
                // When open, continue to poll a frame
                State::Open => {
                    match self.poll2(cx) {
                        // The connection has shutdown normally
                        Poll::Ready(Ok(())) => self.state = State::Closing(Reason::NO_ERROR),
                        // The connection is not ready to make progress
                        Poll::Pending => {
                            // Ensure all window updates have been sent.
                            //
                            // This will also handle flushing `self.codec`
                            ready!(self.streams.poll_complete(cx, &mut self.codec))?;

                            if (self.error.is_some() || self.go_away.should_close_on_idle())
                                && !self.streams.has_streams()
                            {
                                self.go_away_now(Reason::NO_ERROR);
                                continue;
                            }

                            return Poll::Pending;
                        }
                        // Attempting to read a frame resulted in a connection level
                        // error. This is handled by setting a GOAWAY frame followed by
                        // terminating the connection.
                        Poll::Ready(Err(Connection(e))) => {
                            log::debug!("Connection::poll; connection error={:?}", e);

                            // We may have already sent a GOAWAY for this error,
                            // if so, don't send another, just flush and close up.
                            if let Some(reason) = self.go_away.going_away_reason() {
                                if reason == e {
                                    log::trace!("    -> already going away");
                                    self.state = State::Closing(e);
                                    continue;
                                }
                            }

                            // Reset all active streams
                            self.streams.recv_err(&e.into());
                            self.go_away_now(e);
                        }
                        // Attempting to read a frame resulted in a stream level error.
                        // This is handled by resetting the frame then trying to read
                        // another frame.
                        Poll::Ready(Err(Stream { id, reason })) => {
                            log::trace!("stream error; id={:?}; reason={:?}", id, reason);
                            self.streams.send_reset(id, reason);
                        }
                        // Attempting to read a frame resulted in an I/O error. All
                        // active streams must be reset.
                        //
                        // TODO: Are I/O errors recoverable?
                        Poll::Ready(Err(Io(e))) => {
                            log::debug!("Connection::poll; IO error={:?}", e);
                            let e = e.into();

                            // Reset all active streams
                            self.streams.recv_err(&e);

                            // Return the error
                            return Poll::Ready(Err(e));
                        }
                    }
                }
                State::Closing(reason) => {
                    log::trace!("connection closing after flush");
                    // Flush/shutdown the codec
                    ready!(self.codec.shutdown(cx))?;

                    // Transition the state to error
                    self.state = State::Closed(reason);
                }
                State::Closed(reason) => return self.take_error(reason),
            }
        }
    }

    fn poll2(&mut self, cx: &mut Context) -> Poll<Result<(), RecvError>> {
        use crate::frame::Frame::*;

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
            if let Some(reason) = ready!(self.poll_go_away(cx)?) {
                if self.go_away.should_close_now() {
                    if self.go_away.is_user_initiated() {
                        // A user initiated abrupt shutdown shouldn't return
                        // the same error back to the user.
                        return Poll::Ready(Ok(()));
                    } else {
                        return Poll::Ready(Err(RecvError::Connection(reason)));
                    }
                }
                // Only NO_ERROR should be waiting for idle
                debug_assert_eq!(
                    reason,
                    Reason::NO_ERROR,
                    "graceful GOAWAY should be NO_ERROR"
                );
            }
            ready!(self.poll_ready(cx))?;

            match ready!(Pin::new(&mut self.codec).poll_next(cx)?) {
                Some(Headers(frame)) => {
                    log::trace!("recv HEADERS; frame={:?}", frame);
                    self.streams.recv_headers(frame)?;
                }
                Some(Data(frame)) => {
                    log::trace!("recv DATA; frame={:?}", frame);
                    self.streams.recv_data(frame)?;
                }
                Some(Reset(frame)) => {
                    log::trace!("recv RST_STREAM; frame={:?}", frame);
                    self.streams.recv_reset(frame)?;
                }
                Some(PushPromise(frame)) => {
                    log::trace!("recv PUSH_PROMISE; frame={:?}", frame);
                    self.streams.recv_push_promise(frame)?;
                }
                Some(Settings(frame)) => {
                    log::trace!("recv SETTINGS; frame={:?}", frame);
                    self.settings
                        .recv_settings(frame, &mut self.codec, &mut self.streams)?;
                }
                Some(GoAway(frame)) => {
                    log::trace!("recv GOAWAY; frame={:?}", frame);
                    // This should prevent starting new streams,
                    // but should allow continuing to process current streams
                    // until they are all EOS. Once they are, State should
                    // transition to GoAway.
                    self.streams.recv_go_away(&frame)?;
                    self.error = Some(frame.reason());
                }
                Some(Ping(frame)) => {
                    log::trace!("recv PING; frame={:?}", frame);
                    let status = self.ping_pong.recv_ping(frame);
                    if status.is_shutdown() {
                        assert!(
                            self.go_away.is_going_away(),
                            "received unexpected shutdown ping"
                        );

                        let last_processed_id = self.streams.last_processed_id();
                        self.go_away(last_processed_id, Reason::NO_ERROR);
                    }
                }
                Some(WindowUpdate(frame)) => {
                    log::trace!("recv WINDOW_UPDATE; frame={:?}", frame);
                    self.streams.recv_window_update(frame)?;
                }
                Some(Priority(frame)) => {
                    log::trace!("recv PRIORITY; frame={:?}", frame);
                    // TODO: handle
                }
                None => {
                    log::trace!("codec closed");
                    self.streams.recv_eof(false).expect("mutex poisoned");
                    return Poll::Ready(Ok(()));
                }
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
    B: Buf,
{
    pub(crate) fn streams(&self) -> &Streams<B, client::Peer> {
        &self.streams
    }
}

impl<T, B> Connection<T, server::Peer, B>
where
    T: AsyncRead + AsyncWrite + Unpin,
    B: Buf,
{
    pub fn next_incoming(&mut self) -> Option<StreamRef<B>> {
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
    B: Buf,
{
    fn drop(&mut self) {
        // Ignore errors as this indicates that the mutex is poisoned.
        let _ = self.streams.recv_eof(true);
    }
}
