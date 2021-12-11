use std::error::Error as StdError;
use std::marker::Unpin;
#[cfg(feature = "runtime")]
use std::time::Duration;

use h2::server::{Connection, Handshake, SendResponse};
use h2::Reason;
use pin_project::{pin_project, project};
use tokio::io::{AsyncRead, AsyncWrite};

use super::{decode_content_length, ping, PipeToSendStream, SendBuf};
use crate::body::HttpBody;
use crate::common::exec::H2Exec;
use crate::common::{task, Future, Pin, Poll};
use crate::headers;
use crate::proto::Dispatched;
use crate::service::HttpService;

use crate::{Body, Response};

// Our defaults are chosen for the "majority" case, which usually are not
// resource constrained, and so the spec default of 64kb can be too limiting
// for performance.
//
// At the same time, a server more often has multiple clients connected, and
// so is more likely to use more resources than a client would.
const DEFAULT_CONN_WINDOW: u32 = 1024 * 1024; // 1mb
const DEFAULT_STREAM_WINDOW: u32 = 1024 * 1024; // 1mb
const DEFAULT_MAX_FRAME_SIZE: u32 = 1024 * 16; // 16kb

#[derive(Clone, Debug)]
pub(crate) struct Config {
    pub(crate) adaptive_window: bool,
    pub(crate) initial_conn_window_size: u32,
    pub(crate) initial_stream_window_size: u32,
    pub(crate) max_frame_size: u32,
    pub(crate) max_concurrent_streams: Option<u32>,
    #[cfg(feature = "runtime")]
    pub(crate) keep_alive_interval: Option<Duration>,
    #[cfg(feature = "runtime")]
    pub(crate) keep_alive_timeout: Duration,
}

impl Default for Config {
    fn default() -> Config {
        Config {
            adaptive_window: false,
            initial_conn_window_size: DEFAULT_CONN_WINDOW,
            initial_stream_window_size: DEFAULT_STREAM_WINDOW,
            max_frame_size: DEFAULT_MAX_FRAME_SIZE,
            max_concurrent_streams: None,
            #[cfg(feature = "runtime")]
            keep_alive_interval: None,
            #[cfg(feature = "runtime")]
            keep_alive_timeout: Duration::from_secs(20),
        }
    }
}

#[pin_project]
pub(crate) struct Server<T, S, B, E>
where
    S: HttpService<Body>,
    B: HttpBody,
{
    exec: E,
    service: S,
    state: State<T, B>,
}

enum State<T, B>
where
    B: HttpBody,
{
    Handshaking {
        ping_config: ping::Config,
        hs: Handshake<T, SendBuf<B::Data>>,
    },
    Serving(Serving<T, B>),
    Closed,
}

struct Serving<T, B>
where
    B: HttpBody,
{
    ping: Option<(ping::Recorder, ping::Ponger)>,
    conn: Connection<T, SendBuf<B::Data>>,
    closing: Option<crate::Error>,
}

impl<T, S, B, E> Server<T, S, B, E>
where
    T: AsyncRead + AsyncWrite + Unpin,
    S: HttpService<Body, ResBody = B>,
    S::Error: Into<Box<dyn StdError + Send + Sync>>,
    B: HttpBody + 'static,
    E: H2Exec<S::Future, B>,
{
    pub(crate) fn new(io: T, service: S, config: &Config, exec: E) -> Server<T, S, B, E> {
        let mut builder = h2::server::Builder::default();
        builder
            .initial_window_size(config.initial_stream_window_size)
            .initial_connection_window_size(config.initial_conn_window_size)
            .max_frame_size(config.max_frame_size);
        if let Some(max) = config.max_concurrent_streams {
            builder.max_concurrent_streams(max);
        }
        let handshake = builder.handshake(io);

        let bdp = if config.adaptive_window {
            Some(config.initial_stream_window_size)
        } else {
            None
        };

        let ping_config = ping::Config {
            bdp_initial_window: bdp,
            #[cfg(feature = "runtime")]
            keep_alive_interval: config.keep_alive_interval,
            #[cfg(feature = "runtime")]
            keep_alive_timeout: config.keep_alive_timeout,
            // If keep-alive is enabled for servers, always enabled while
            // idle, so it can more aggresively close dead connections.
            #[cfg(feature = "runtime")]
            keep_alive_while_idle: true,
        };

        Server {
            exec,
            state: State::Handshaking {
                ping_config,
                hs: handshake,
            },
            service,
        }
    }

    pub fn graceful_shutdown(&mut self) {
        trace!("graceful_shutdown");
        match self.state {
            State::Handshaking { .. } => {
                // fall-through, to replace state with Closed
            }
            State::Serving(ref mut srv) => {
                if srv.closing.is_none() {
                    srv.conn.graceful_shutdown();
                }
                return;
            }
            State::Closed => {
                return;
            }
        }
        self.state = State::Closed;
    }
}

impl<T, S, B, E> Future for Server<T, S, B, E>
where
    T: AsyncRead + AsyncWrite + Unpin,
    S: HttpService<Body, ResBody = B>,
    S::Error: Into<Box<dyn StdError + Send + Sync>>,
    B: HttpBody + 'static,
    E: H2Exec<S::Future, B>,
{
    type Output = crate::Result<Dispatched>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut task::Context<'_>) -> Poll<Self::Output> {
        let me = &mut *self;
        loop {
            let next = match me.state {
                State::Handshaking {
                    ref mut hs,
                    ref ping_config,
                } => {
                    let mut conn = ready!(Pin::new(hs).poll(cx).map_err(crate::Error::new_h2))?;
                    let ping = if ping_config.is_enabled() {
                        let pp = conn.ping_pong().expect("conn.ping_pong");
                        Some(ping::channel(pp, ping_config.clone()))
                    } else {
                        None
                    };
                    State::Serving(Serving {
                        ping,
                        conn,
                        closing: None,
                    })
                }
                State::Serving(ref mut srv) => {
                    ready!(srv.poll_server(cx, &mut me.service, &mut me.exec))?;
                    return Poll::Ready(Ok(Dispatched::Shutdown));
                }
                State::Closed => {
                    // graceful_shutdown was called before handshaking finished,
                    // nothing to do here...
                    return Poll::Ready(Ok(Dispatched::Shutdown));
                }
            };
            me.state = next;
        }
    }
}

impl<T, B> Serving<T, B>
where
    T: AsyncRead + AsyncWrite + Unpin,
    B: HttpBody + 'static,
{
    fn poll_server<S, E>(
        &mut self,
        cx: &mut task::Context<'_>,
        service: &mut S,
        exec: &mut E,
    ) -> Poll<crate::Result<()>>
    where
        S: HttpService<Body, ResBody = B>,
        S::Error: Into<Box<dyn StdError + Send + Sync>>,
        E: H2Exec<S::Future, B>,
    {
        if self.closing.is_none() {
            loop {
                self.poll_ping(cx);

                // Check that the service is ready to accept a new request.
                //
                // - If not, just drive the connection some.
                // - If ready, try to accept a new request from the connection.
                match service.poll_ready(cx) {
                    Poll::Ready(Ok(())) => (),
                    Poll::Pending => {
                        // use `poll_closed` instead of `poll_accept`,
                        // in order to avoid accepting a request.
                        ready!(self.conn.poll_closed(cx).map_err(crate::Error::new_h2))?;
                        trace!("incoming connection complete");
                        return Poll::Ready(Ok(()));
                    }
                    Poll::Ready(Err(err)) => {
                        let err = crate::Error::new_user_service(err);
                        debug!("service closed: {}", err);

                        let reason = err.h2_reason();
                        if reason == Reason::NO_ERROR {
                            // NO_ERROR is only used for graceful shutdowns...
                            trace!("interpretting NO_ERROR user error as graceful_shutdown");
                            self.conn.graceful_shutdown();
                        } else {
                            trace!("abruptly shutting down with {:?}", reason);
                            self.conn.abrupt_shutdown(reason);
                        }
                        self.closing = Some(err);
                        break;
                    }
                }

                // When the service is ready, accepts an incoming request.
                match ready!(self.conn.poll_accept(cx)) {
                    Some(Ok((req, respond))) => {
                        trace!("incoming request");
                        let content_length = decode_content_length(req.headers());
                        let ping = self
                            .ping
                            .as_ref()
                            .map(|ping| ping.0.clone())
                            .unwrap_or_else(ping::disabled);

                        // Record the headers received
                        ping.record_non_data();

                        let req = req.map(|stream| crate::Body::h2(stream, content_length, ping));
                        let fut = H2Stream::new(service.call(req), respond);
                        exec.execute_h2stream(fut);
                    }
                    Some(Err(e)) => {
                        return Poll::Ready(Err(crate::Error::new_h2(e)));
                    }
                    None => {
                        // no more incoming streams...
                        if let Some((ref ping, _)) = self.ping {
                            ping.ensure_not_timed_out()?;
                        }

                        trace!("incoming connection complete");
                        return Poll::Ready(Ok(()));
                    }
                }
            }
        }

        debug_assert!(
            self.closing.is_some(),
            "poll_server broke loop without closing"
        );

        ready!(self.conn.poll_closed(cx).map_err(crate::Error::new_h2))?;

        Poll::Ready(Err(self.closing.take().expect("polled after error")))
    }

    fn poll_ping(&mut self, cx: &mut task::Context<'_>) {
        if let Some((_, ref mut estimator)) = self.ping {
            match estimator.poll(cx) {
                Poll::Ready(ping::Ponged::SizeUpdate(wnd)) => {
                    self.conn.set_target_window_size(wnd);
                    let _ = self.conn.set_initial_window_size(wnd);
                }
                #[cfg(feature = "runtime")]
                Poll::Ready(ping::Ponged::KeepAliveTimedOut) => {
                    debug!("keep-alive timed out, closing connection");
                    self.conn.abrupt_shutdown(h2::Reason::NO_ERROR);
                }
                Poll::Pending => {}
            }
        }
    }
}

#[allow(missing_debug_implementations)]
#[pin_project]
pub struct H2Stream<F, B>
where
    B: HttpBody,
{
    reply: SendResponse<SendBuf<B::Data>>,
    #[pin]
    state: H2StreamState<F, B>,
}

#[pin_project]
enum H2StreamState<F, B>
where
    B: HttpBody,
{
    Service(#[pin] F),
    Body(#[pin] PipeToSendStream<B>),
}

impl<F, B> H2Stream<F, B>
where
    B: HttpBody,
{
    fn new(fut: F, respond: SendResponse<SendBuf<B::Data>>) -> H2Stream<F, B> {
        H2Stream {
            reply: respond,
            state: H2StreamState::Service(fut),
        }
    }
}

macro_rules! reply {
    ($me:expr, $res:expr, $eos:expr) => {{
        match $me.reply.send_response($res, $eos) {
            Ok(tx) => tx,
            Err(e) => {
                debug!("send response error: {}", e);
                $me.reply.send_reset(Reason::INTERNAL_ERROR);
                return Poll::Ready(Err(crate::Error::new_h2(e)));
            }
        }
    }};
}

impl<F, B, E> H2Stream<F, B>
where
    F: Future<Output = Result<Response<B>, E>>,
    B: HttpBody,
    B::Error: Into<Box<dyn StdError + Send + Sync>>,
    E: Into<Box<dyn StdError + Send + Sync>>,
{
    #[project]
    fn poll2(self: Pin<&mut Self>, cx: &mut task::Context<'_>) -> Poll<crate::Result<()>> {
        let mut me = self.project();
        loop {
            #[project]
            let next = match me.state.as_mut().project() {
                H2StreamState::Service(h) => {
                    let res = match h.poll(cx) {
                        Poll::Ready(Ok(r)) => r,
                        Poll::Pending => {
                            // Response is not yet ready, so we want to check if the client has sent a
                            // RST_STREAM frame which would cancel the current request.
                            if let Poll::Ready(reason) =
                                me.reply.poll_reset(cx).map_err(crate::Error::new_h2)?
                            {
                                debug!("stream received RST_STREAM: {:?}", reason);
                                return Poll::Ready(Err(crate::Error::new_h2(reason.into())));
                            }
                            return Poll::Pending;
                        }
                        Poll::Ready(Err(e)) => {
                            let err = crate::Error::new_user_service(e);
                            warn!("http2 service errored: {}", err);
                            me.reply.send_reset(err.h2_reason());
                            return Poll::Ready(Err(err));
                        }
                    };

                    let (head, body) = res.into_parts();
                    let mut res = ::http::Response::from_parts(head, ());
                    super::strip_connection_headers(res.headers_mut(), false);

                    // set Date header if it isn't already set...
                    res.headers_mut()
                        .entry(::http::header::DATE)
                        .or_insert_with(crate::proto::h1::date::update_and_header_value);

                    // automatically set Content-Length from body...
                    if let Some(len) = body.size_hint().exact() {
                        headers::set_content_length_if_missing(res.headers_mut(), len);
                    }

                    if !body.is_end_stream() {
                        let body_tx = reply!(me, res, false);
                        H2StreamState::Body(PipeToSendStream::new(body, body_tx))
                    } else {
                        reply!(me, res, true);
                        return Poll::Ready(Ok(()));
                    }
                }
                H2StreamState::Body(pipe) => {
                    return pipe.poll(cx);
                }
            };
            me.state.set(next);
        }
    }
}

impl<F, B, E> Future for H2Stream<F, B>
where
    F: Future<Output = Result<Response<B>, E>>,
    B: HttpBody,
    B::Error: Into<Box<dyn StdError + Send + Sync>>,
    E: Into<Box<dyn StdError + Send + Sync>>,
{
    type Output = ();

    fn poll(self: Pin<&mut Self>, cx: &mut task::Context<'_>) -> Poll<Self::Output> {
        self.poll2(cx).map(|res| {
            if let Err(e) = res {
                debug!("stream error: {}", e);
            }
        })
    }
}
