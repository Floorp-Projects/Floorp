//! Server implementation of the HTTP/2.0 protocol.
//!
//! # Getting started
//!
//! Running an HTTP/2.0 server requires the caller to manage accepting the
//! connections as well as getting the connections to a state that is ready to
//! begin the HTTP/2.0 handshake. See [here](../index.html#handshake) for more
//! details.
//!
//! This could be as basic as using Tokio's [`TcpListener`] to accept
//! connections, but usually it means using either ALPN or HTTP/1.1 protocol
//! upgrades.
//!
//! Once a connection is obtained, it is passed to [`handshake`],
//! which will begin the [HTTP/2.0 handshake]. This returns a future that
//! completes once the handshake process is performed and HTTP/2.0 streams may
//! be received.
//!
//! [`handshake`] uses default configuration values. There are a number of
//! settings that can be changed by using [`Builder`] instead.
//!
//! # Inbound streams
//!
//! The [`Connection`] instance is used to accept inbound HTTP/2.0 streams. It
//! does this by implementing [`futures::Stream`]. When a new stream is
//! received, a call to [`Connection::poll`] will return `(request, response)`.
//! The `request` handle (of type [`http::Request<RecvStream>`]) contains the
//! HTTP request head as well as provides a way to receive the inbound data
//! stream and the trailers. The `response` handle (of type [`SendStream`])
//! allows responding to the request, stream the response payload, send
//! trailers, and send push promises.
//!
//! The send ([`SendStream`]) and receive ([`RecvStream`]) halves of the stream
//! can be operated independently.
//!
//! # Managing the connection
//!
//! The [`Connection`] instance is used to manage connection state. The caller
//! is required to call either [`Connection::poll`] or
//! [`Connection::poll_close`] in order to advance the connection state. Simply
//! operating on [`SendStream`] or [`RecvStream`] will have no effect unless the
//! connection state is advanced.
//!
//! It is not required to call **both** [`Connection::poll`] and
//! [`Connection::poll_close`]. If the caller is ready to accept a new stream,
//! then only [`Connection::poll`] should be called. When the caller **does
//! not** want to accept a new stream, [`Connection::poll_close`] should be
//! called.
//!
//! The [`Connection`] instance should only be dropped once
//! [`Connection::poll_close`] returns `Ready`. Once [`Connection::poll`]
//! returns `Ready(None)`, there will no longer be any more inbound streams. At
//! this point, only [`Connection::poll_close`] should be called.
//!
//! # Shutting down the server
//!
//! Graceful shutdown of the server is [not yet
//! implemented](https://github.com/carllerche/h2/issues/69).
//!
//! # Example
//!
//! A basic HTTP/2.0 server example that runs over TCP and assumes [prior
//! knowledge], i.e. both the client and the server assume that the TCP socket
//! will use the HTTP/2.0 protocol without prior negotiation.
//!
//! ```rust
//! extern crate futures;
//! extern crate h2;
//! extern crate http;
//! extern crate tokio_core;
//!
//! use futures::{Future, Stream};
//! # use futures::future::ok;
//! use h2::server;
//! use http::{Response, StatusCode};
//! use tokio_core::reactor;
//! use tokio_core::net::TcpListener;
//!
//! pub fn main () {
//!     let mut core = reactor::Core::new().unwrap();
//!     let handle = core.handle();
//!
//!     let addr = "127.0.0.1:5928".parse().unwrap();
//!     let listener = TcpListener::bind(&addr, &handle).unwrap();
//!
//!     core.run({
//!         // Accept all incoming TCP connections.
//!         listener.incoming().for_each(move |(socket, _)| {
//!             // Spawn a new task to process each connection.
//!             handle.spawn({
//!                 // Start the HTTP/2.0 connection handshake
//!                 server::handshake(socket)
//!                     .and_then(|h2| {
//!                         // Accept all inbound HTTP/2.0 streams sent over the
//!                         // connection.
//!                         h2.for_each(|(request, mut respond)| {
//!                             println!("Received request: {:?}", request);
//!
//!                             // Build a response with no body
//!                             let response = Response::builder()
//!                                 .status(StatusCode::OK)
//!                                 .body(())
//!                                 .unwrap();
//!
//!                             // Send the response back to the client
//!                             respond.send_response(response, true)
//!                                 .unwrap();
//!
//!                             Ok(())
//!                         })
//!                     })
//!                     .map_err(|e| panic!("unexpected error = {:?}", e))
//!             });
//!
//!             Ok(())
//!         })
//!         # .select(ok(()))
//!     }).ok().expect("failed to run HTTP/2.0 server");
//! }
//! ```
//!
//! [prior knowledge]: http://httpwg.org/specs/rfc7540.html#known-http
//! [`handshake`]: fn.handshake.html
//! [HTTP/2.0 handshake]: http://httpwg.org/specs/rfc7540.html#ConnectionHeader
//! [`Builder`]: struct.Builder.html
//! [`Connection`]: struct.Connection.html
//! [`Connection::poll`]: struct.Connection.html#method.poll
//! [`Connection::poll_close`]: struct.Connection.html#method.poll_close
//! [`futures::Stream`]: https://docs.rs/futures/0.1/futures/stream/trait.Stream.html
//! [`http::Request<RecvStream>`]: ../struct.RecvStream.html
//! [`RecvStream`]: ../struct.RecvStream.html
//! [`SendStream`]: ../struct.SendStream.html
//! [`TcpListener`]: https://docs.rs/tokio-core/0.1/tokio_core/net/struct.TcpListener.html

use {SendStream, RecvStream, ReleaseCapacity};
use codec::{Codec, RecvError};
use frame::{self, Reason, Settings, StreamId};
use proto::{self, Config, Prioritized};

use bytes::{Buf, Bytes, IntoBuf};
use futures::{self, Async, Future, Poll};
use http::{Request, Response};
use std::{convert, fmt, io, mem};
use std::time::Duration;
use tokio_io::{AsyncRead, AsyncWrite};

/// In progress HTTP/2.0 connection handshake future.
///
/// This type implements `Future`, yielding a `Connection` instance once the
/// handshake has completed.
///
/// The handshake is completed once the connection preface is fully received
/// from the client **and** the initial settings frame is sent to the client.
///
/// The handshake future does not wait for the initial settings frame from the
/// client.
///
/// See [module] level docs for more details.
///
/// [module]: index.html
#[must_use = "futures do nothing unless polled"]
pub struct Handshake<T, B: IntoBuf = Bytes> {
    /// The config to pass to Connection::new after handshake succeeds.
    builder: Builder,
    /// The current state of the handshake.
    state: Handshaking<T, B>
}

/// Accepts inbound HTTP/2.0 streams on a connection.
///
/// A `Connection` is backed by an I/O resource (usually a TCP socket) and
/// implements the HTTP/2.0 server logic for that connection. It is responsible
/// for receiving inbound streams initiated by the client as well as driving the
/// internal state forward.
///
/// `Connection` values are created by calling [`handshake`]. Once a
/// `Connection` value is obtained, the caller must call [`poll`] or
/// [`poll_close`] in order to drive the internal connection state forward.
///
/// See [module level] documentation for more details
///
/// [module level]: index.html
/// [`handshake`]: struct.Connection.html#method.handshake
/// [`poll`]: struct.Connection.html#method.poll
/// [`poll_close`]: struct.Connection.html#method.poll_close
///
/// # Examples
///
/// ```
/// # extern crate futures;
/// # extern crate h2;
/// # extern crate tokio_io;
/// # use futures::{Future, Stream};
/// # use tokio_io::*;
/// # use h2::server;
/// # use h2::server::*;
/// #
/// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T) {
/// server::handshake(my_io)
///     .and_then(|server| {
///         server.for_each(|(request, respond)| {
///             // Process the request and send the response back to the client
///             // using `respond`.
///             # Ok(())
///         })
///     })
/// # .wait().unwrap();
/// # }
/// #
/// # pub fn main() {}
/// ```
#[must_use = "streams do nothing unless polled"]
pub struct Connection<T, B: IntoBuf> {
    connection: proto::Connection<T, Peer, B>,
}

/// Builds server connections with custom configuration values.
///
/// Methods can be chained in order to set the configuration values.
///
/// The server is constructed by calling [`handshake`] and passing the I/O
/// handle that will back the HTTP/2.0 server.
///
/// New instances of `Builder` are obtained via [`Builder::new`].
///
/// See function level documentation for details on the various server
/// configuration settings.
///
/// [`Builder::new`]: struct.Builder.html#method.new
/// [`handshake`]: struct.Builder.html#method.handshake
///
/// # Examples
///
/// ```
/// # extern crate h2;
/// # extern crate tokio_io;
/// # use tokio_io::*;
/// # use h2::server::*;
/// #
/// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
/// # -> Handshake<T>
/// # {
/// // `server_fut` is a future representing the completion of the HTTP/2.0
/// // handshake.
/// let server_fut = Builder::new()
///     .initial_window_size(1_000_000)
///     .max_concurrent_streams(1000)
///     .handshake(my_io);
/// # server_fut
/// # }
/// #
/// # pub fn main() {}
/// ```
#[derive(Clone, Debug)]
pub struct Builder {
    /// Time to keep locally reset streams around before reaping.
    reset_stream_duration: Duration,

    /// Maximum number of locally reset streams to keep at a time.
    reset_stream_max: usize,

    /// Initial `Settings` frame to send as part of the handshake.
    settings: Settings,

    /// Initial target window size for new connections.
    initial_target_connection_window_size: Option<u32>,
}

/// Send a response back to the client
///
/// A `SendResponse` instance is provided when receiving a request and is used
/// to send the associated response back to the client. It is also used to
/// explicitly reset the stream with a custom reason.
///
/// It will also be used to initiate push promises linked with the associated
/// stream. This is [not yet
/// implemented](https://github.com/carllerche/h2/issues/185).
///
/// If the `SendResponse` instance is dropped without sending a response, then
/// the HTTP/2.0 stream will be reset.
///
/// See [module] level docs for more details.
///
/// [module]: index.html
#[derive(Debug)]
pub struct SendResponse<B: IntoBuf> {
    inner: proto::StreamRef<B::Buf>,
}

/// Stages of an in-progress handshake.
enum Handshaking<T, B: IntoBuf> {
    /// State 1. Connection is flushing pending SETTINGS frame.
    Flushing(Flush<T, Prioritized<B::Buf>>),
    /// State 2. Connection is waiting for the client preface.
    ReadingPreface(ReadPreface<T, Prioritized<B::Buf>>),
    /// Dummy state for `mem::replace`.
    Empty,
}

/// Flush a Sink
struct Flush<T, B> {
    codec: Option<Codec<T, B>>,
}

/// Read the client connection preface
struct ReadPreface<T, B> {
    codec: Option<Codec<T, B>>,
    pos: usize,
}

#[derive(Debug)]
pub(crate) struct Peer;

const PREFACE: [u8; 24] = *b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

/// Creates a new configured HTTP/2.0 server with default configuration
/// values backed by `io`.
///
/// It is expected that `io` already be in an appropriate state to commence
/// the [HTTP/2.0 handshake]. See [Handshake] for more details.
///
/// Returns a future which resolves to the [`Connection`] instance once the
/// HTTP/2.0 handshake has been completed. The returned [`Connection`]
/// instance will be using default configuration values. Use [`Builder`] to
/// customize the configuration values used by a [`Connection`] instance.
///
/// [HTTP/2.0 handshake]: http://httpwg.org/specs/rfc7540.html#ConnectionHeader
/// [Handshake]: ../index.html#handshake
/// [`Connection`]: struct.Connection.html
///
/// # Examples
///
/// ```
/// # extern crate futures;
/// # extern crate h2;
/// # extern crate tokio_io;
/// # use tokio_io::*;
/// # use futures::*;
/// # use h2::server;
/// # use h2::server::*;
/// #
/// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
/// # {
/// server::handshake(my_io)
///     .and_then(|connection| {
///         // The HTTP/2.0 handshake has completed, now use `connection` to
///         // accept inbound HTTP/2.0 streams.
///         # Ok(())
///     })
///     # .wait().unwrap();
/// # }
/// #
/// # pub fn main() {}
/// ```
pub fn handshake<T>(io: T) -> Handshake<T, Bytes>
where T: AsyncRead + AsyncWrite,
{
    Builder::new().handshake(io)
}

// ===== impl Connection =====

impl<T, B> Connection<T, B>
where
    T: AsyncRead + AsyncWrite,
    B: IntoBuf,
{
    fn handshake2(io: T, builder: Builder) -> Handshake<T, B> {
        // Create the codec.
        let mut codec = Codec::new(io);

        if let Some(max) = builder.settings.max_frame_size() {
            codec.set_max_recv_frame_size(max as usize);
        }

        if let Some(max) = builder.settings.max_header_list_size() {
            codec.set_max_recv_header_list_size(max as usize);
        }

        // Send initial settings frame.
        codec
            .buffer(builder.settings.clone().into())
            .expect("invalid SETTINGS frame");

        // Create the handshake future.
        let state = Handshaking::from(codec);

        Handshake { builder, state }
    }

    /// Sets the target window size for the whole connection.
    ///
    /// If `size` is greater than the current value, then a `WINDOW_UPDATE`
    /// frame will be immediately sent to the remote, increasing the connection
    /// level window by `size - current_value`.
    ///
    /// If `size` is less than the current value, nothing will happen
    /// immediately. However, as window capacity is released by
    /// [`ReleaseCapacity`] instances, no `WINDOW_UPDATE` frames will be sent
    /// out until the number of "in flight" bytes drops below `size`.
    ///
    /// The default value is 65,535.
    ///
    /// See [`ReleaseCapacity`] documentation for more details.
    ///
    /// [`ReleaseCapacity`]: ../struct.ReleaseCapacity.html
    /// [library level]: ../index.html#flow-control
    pub fn set_target_window_size(&mut self, size: u32) {
        assert!(size <= proto::MAX_WINDOW_SIZE);
        self.connection.set_target_window_size(size);
    }

    /// Returns `Ready` when the underlying connection has closed.
    ///
    /// If any new inbound streams are received during a call to `poll_close`,
    /// they will be queued and returned on the next call to [`poll`].
    ///
    /// This function will advance the internal connection state, driving
    /// progress on all the other handles (e.g. [`RecvStream`] and [`SendStream`]).
    ///
    /// See [here](index.html#managing-the-connection) for more details.
    ///
    /// [`poll`]: struct.Connection.html#method.poll
    /// [`RecvStream`]: ../struct.RecvStream.html
    /// [`SendStream`]: ../struct.SendStream.html
    pub fn poll_close(&mut self) -> Poll<(), ::Error> {
        self.connection.poll().map_err(Into::into)
    }

    #[deprecated(note="use abrupt_shutdown or graceful_shutdown instead", since="0.1.4")]
    #[doc(hidden)]
    pub fn close_connection(&mut self) {
        self.graceful_shutdown();
    }

    /// Sets the connection to a GOAWAY state.
    ///
    /// Does not terminate the connection. Must continue being polled to close
    /// connection.
    ///
    /// After flushing the GOAWAY frame, the connection is closed. Any
    /// outstanding streams do not prevent the connection from closing. This
    /// should usually be reserved for shutting down when something bad
    /// external to `h2` has happened, and open streams cannot be properly
    /// handled.
    ///
    /// For graceful shutdowns, see [`graceful_shutdown`](Connection::graceful_shutdown).
    pub fn abrupt_shutdown(&mut self, reason: Reason) {
        self.connection.go_away_now(reason);
    }

    /// Starts a [graceful shutdown][1] process.
    ///
    /// Must continue being polled to close connection.
    ///
    /// It's possible to receive more requests after calling this method, since
    /// they might have been in-flight from the client already. After about
    /// 1 RTT, no new requests should be accepted. Once all active streams
    /// have completed, the connection is closed.
    ///
    /// [1]: http://httpwg.org/specs/rfc7540.html#GOAWAY
    pub fn graceful_shutdown(&mut self) {
        self.connection.go_away_gracefully();
    }
}

impl<T, B> futures::Stream for Connection<T, B>
where
    T: AsyncRead + AsyncWrite,
    B: IntoBuf,
    B::Buf: 'static,
{
    type Item = (Request<RecvStream>, SendResponse<B>);
    type Error = ::Error;

    fn poll(&mut self) -> Poll<Option<Self::Item>, ::Error> {
        // Always try to advance the internal state. Getting NotReady also is
        // needed to allow this function to return NotReady.
        match self.poll_close()? {
            Async::Ready(_) => {
                // If the socket is closed, don't return anything
                // TODO: drop any pending streams
                return Ok(None.into());
            },
            _ => {},
        }

        if let Some(inner) = self.connection.next_incoming() {
            trace!("received incoming");
            let (head, _) = inner.take_request().into_parts();
            let body = RecvStream::new(ReleaseCapacity::new(inner.clone_to_opaque()));

            let request = Request::from_parts(head, body);
            let respond = SendResponse { inner };

            return Ok(Some((request, respond)).into());
        }

        Ok(Async::NotReady)
    }
}

impl<T, B> fmt::Debug for Connection<T, B>
where
    T: fmt::Debug,
    B: fmt::Debug + IntoBuf,
    B::Buf: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Connection")
            .field("connection", &self.connection)
            .finish()
    }
}

// ===== impl Builder =====

impl Builder {
    /// Returns a new server builder instance initialized with default
    /// configuration values.
    ///
    /// Configuration methods can be chained on the return value.
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .initial_window_size(1_000_000)
    ///     .max_concurrent_streams(1000)
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn new() -> Builder {
        Builder {
            reset_stream_duration: Duration::from_secs(proto::DEFAULT_RESET_STREAM_SECS),
            reset_stream_max: proto::DEFAULT_RESET_STREAM_MAX,
            settings: Settings::default(),
            initial_target_connection_window_size: None,
        }
    }

    /// Indicates the initial window size (in octets) for stream-level
    /// flow control for received data.
    ///
    /// The initial window of a stream is used as part of flow control. For more
    /// details, see [`ReleaseCapacity`].
    ///
    /// The default value is 65,535.
    ///
    /// [`ReleaseCapacity`]: ../struct.ReleaseCapacity.html
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .initial_window_size(1_000_000)
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn initial_window_size(&mut self, size: u32) -> &mut Self {
        self.settings.set_initial_window_size(Some(size));
        self
    }

    /// Indicates the initial window size (in octets) for connection-level flow control
    /// for received data.
    ///
    /// The initial window of a connection is used as part of flow control. For more details,
    /// see [`ReleaseCapacity`].
    ///
    /// The default value is 65,535.
    ///
    /// [`ReleaseCapacity`]: ../struct.ReleaseCapacity.html
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .initial_connection_window_size(1_000_000)
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn initial_connection_window_size(&mut self, size: u32) -> &mut Self {
        self.initial_target_connection_window_size = Some(size);
        self
    }

    /// Indicates the size (in octets) of the largest HTTP/2.0 frame payload that the
    /// configured server is able to accept.
    ///
    /// The sender may send data frames that are **smaller** than this value,
    /// but any data larger than `max` will be broken up into multiple `DATA`
    /// frames.
    ///
    /// The value **must** be between 16,384 and 16,777,215. The default value is 16,384.
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .max_frame_size(1_000_000)
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    ///
    /// # Panics
    ///
    /// This function panics if `max` is not within the legal range specified
    /// above.
    pub fn max_frame_size(&mut self, max: u32) -> &mut Self {
        self.settings.set_max_frame_size(Some(max));
        self
    }

    /// Sets the max size of received header frames.
    ///
    /// This advisory setting informs a peer of the maximum size of header list
    /// that the sender is prepared to accept, in octets. The value is based on
    /// the uncompressed size of header fields, including the length of the name
    /// and value in octets plus an overhead of 32 octets for each header field.
    ///
    /// This setting is also used to limit the maximum amount of data that is
    /// buffered to decode HEADERS frames.
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .max_header_list_size(16 * 1024)
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn max_header_list_size(&mut self, max: u32) -> &mut Self {
        self.settings.set_max_header_list_size(Some(max));
        self
    }

    /// Sets the maximum number of concurrent streams.
    ///
    /// The maximum concurrent streams setting only controls the maximum number
    /// of streams that can be initiated by the remote peer. In other words,
    /// when this setting is set to 100, this does not limit the number of
    /// concurrent streams that can be created by the caller.
    ///
    /// It is recommended that this value be no smaller than 100, so as to not
    /// unnecessarily limit parallelism. However, any value is legal, including
    /// 0. If `max` is set to 0, then the remote will not be permitted to
    /// initiate streams.
    ///
    /// Note that streams in the reserved state, i.e., push promises that have
    /// been reserved but the stream has not started, do not count against this
    /// setting.
    ///
    /// Also note that if the remote *does* exceed the value set here, it is not
    /// a protocol level error. Instead, the `h2` library will immediately reset
    /// the stream.
    ///
    /// See [Section 5.1.2] in the HTTP/2.0 spec for more details.
    ///
    /// [Section 5.1.2]: https://http2.github.io/http2-spec/#rfc.section.5.1.2
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .max_concurrent_streams(1000)
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn max_concurrent_streams(&mut self, max: u32) -> &mut Self {
        self.settings.set_max_concurrent_streams(Some(max));
        self
    }

    /// Sets the maximum number of concurrent locally reset streams.
    ///
    /// When a stream is explicitly reset by either calling
    /// [`SendResponse::send_reset`] or by dropping a [`SendResponse`] instance
    /// before completing the stream, the HTTP/2.0 specification requires that
    /// any further frames received for that stream must be ignored for "some
    /// time".
    ///
    /// In order to satisfy the specification, internal state must be maintained
    /// to implement the behavior. This state grows linearly with the number of
    /// streams that are locally reset.
    ///
    /// The `max_concurrent_reset_streams` setting configures sets an upper
    /// bound on the amount of state that is maintained. When this max value is
    /// reached, the oldest reset stream is purged from memory.
    ///
    /// Once the stream has been fully purged from memory, any additional frames
    /// received for that stream will result in a connection level protocol
    /// error, forcing the connection to terminate.
    ///
    /// The default value is 10.
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .max_concurrent_reset_streams(1000)
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn max_concurrent_reset_streams(&mut self, max: usize) -> &mut Self {
        self.reset_stream_max = max;
        self
    }

    /// Sets the maximum number of concurrent locally reset streams.
    ///
    /// When a stream is explicitly reset by either calling
    /// [`SendResponse::send_reset`] or by dropping a [`SendResponse`] instance
    /// before completing the stream, the HTTP/2.0 specification requires that
    /// any further frames received for that stream must be ignored for "some
    /// time".
    ///
    /// In order to satisfy the specification, internal state must be maintained
    /// to implement the behavior. This state grows linearly with the number of
    /// streams that are locally reset.
    ///
    /// The `reset_stream_duration` setting configures the max amount of time
    /// this state will be maintained in memory. Once the duration elapses, the
    /// stream state is purged from memory.
    ///
    /// Once the stream has been fully purged from memory, any additional frames
    /// received for that stream will result in a connection level protocol
    /// error, forcing the connection to terminate.
    ///
    /// The default value is 30 seconds.
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// # use std::time::Duration;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .reset_stream_duration(Duration::from_secs(10))
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn reset_stream_duration(&mut self, dur: Duration) -> &mut Self {
        self.reset_stream_duration = dur;
        self
    }

    /// Creates a new configured HTTP/2.0 server backed by `io`.
    ///
    /// It is expected that `io` already be in an appropriate state to commence
    /// the [HTTP/2.0 handshake]. See [Handshake] for more details.
    ///
    /// Returns a future which resolves to the [`Connection`] instance once the
    /// HTTP/2.0 handshake has been completed.
    ///
    /// This function also allows the caller to configure the send payload data
    /// type. See [Outbound data type] for more details.
    ///
    /// [HTTP/2.0 handshake]: http://httpwg.org/specs/rfc7540.html#ConnectionHeader
    /// [Handshake]: ../index.html#handshake
    /// [`Connection`]: struct.Connection.html
    /// [Outbound data type]: ../index.html#outbound-data-type.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut = Builder::new()
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    ///
    /// Configures the send-payload data type. In this case, the outbound data
    /// type will be `&'static [u8]`.
    ///
    /// ```
    /// # extern crate h2;
    /// # extern crate tokio_io;
    /// # use tokio_io::*;
    /// # use h2::server::*;
    /// #
    /// # fn doc<T: AsyncRead + AsyncWrite>(my_io: T)
    /// # -> Handshake<T, &'static [u8]>
    /// # {
    /// // `server_fut` is a future representing the completion of the HTTP/2.0
    /// // handshake.
    /// let server_fut: Handshake<_, &'static [u8]> = Builder::new()
    ///     .handshake(my_io);
    /// # server_fut
    /// # }
    /// #
    /// # pub fn main() {}
    /// ```
    pub fn handshake<T, B>(&self, io: T) -> Handshake<T, B>
    where
        T: AsyncRead + AsyncWrite,
        B: IntoBuf,
        B::Buf: 'static,
    {
        Connection::handshake2(io, self.clone())
    }
}

impl Default for Builder {
    fn default() -> Builder {
        Builder::new()
    }
}

// ===== impl SendResponse =====

impl<B: IntoBuf> SendResponse<B> {
    /// Send a response to a client request.
    ///
    /// On success, a [`SendStream`] instance is returned. This instance can be
    /// used to stream the response body and send trailers.
    ///
    /// If a body or trailers will be sent on the returned [`SendStream`]
    /// instance, then `end_of_stream` must be set to `false` when calling this
    /// function.
    ///
    /// The [`SendResponse`] instance is already associated with a received
    /// request.  This function may only be called once per instance and only if
    /// [`send_reset`] has not been previously called.
    ///
    /// [`SendResponse`]: #
    /// [`SendStream`]: ../struct.SendStream.html
    /// [`send_reset`]: #method.send_reset
    pub fn send_response(
        &mut self,
        response: Response<()>,
        end_of_stream: bool,
    ) -> Result<SendStream<B>, ::Error> {
        self.inner
            .send_response(response, end_of_stream)
            .map(|_| SendStream::new(self.inner.clone()))
            .map_err(Into::into)
    }

    /// Send a stream reset to the peer.
    ///
    /// This essentially cancels the stream, including any inbound or outbound
    /// data streams.
    ///
    /// If this function is called before [`send_response`], a call to
    /// [`send_response`] will result in an error.
    ///
    /// If this function is called while a [`SendStream`] instance is active,
    /// any further use of the instance will result in an error.
    ///
    /// This function should only be called once.
    ///
    /// [`send_response`]: #method.send_response
    /// [`SendStream`]: ../struct.SendStream.html
    pub fn send_reset(&mut self, reason: Reason) {
        self.inner.send_reset(reason)
    }

    /// Polls to be notified when the client resets this stream.
    ///
    /// If stream is still open, this returns `Ok(Async::NotReady)`, and
    /// registers the task to be notified if a `RST_STREAM` is received.
    ///
    /// If a `RST_STREAM` frame is received for this stream, calling this
    /// method will yield the `Reason` for the reset.
    ///
    /// # Error
    ///
    /// Calling this method after having called `send_response` will return
    /// a user error.
    pub fn poll_reset(&mut self) -> Poll<Reason, ::Error> {
        self.inner.poll_reset(proto::PollReset::AwaitingHeaders)
    }

    /// Returns the stream ID of the response stream.
    ///
    /// # Panics
    ///
    /// If the lock on the strean store has been poisoned.
    pub fn stream_id(&self) -> ::StreamId {
        ::StreamId::from_internal(self.inner.stream_id())
    }

    // TODO: Support reserving push promises.
}

// ===== impl Flush =====

impl<T, B: Buf> Flush<T, B> {
    fn new(codec: Codec<T, B>) -> Self {
        Flush {
            codec: Some(codec),
        }
    }
}

impl<T, B> Future for Flush<T, B>
where
    T: AsyncWrite,
    B: Buf,
{
    type Item = Codec<T, B>;
    type Error = ::Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        // Flush the codec
        try_ready!(self.codec.as_mut().unwrap().flush());

        // Return the codec
        Ok(Async::Ready(self.codec.take().unwrap()))
    }
}

impl<T, B: Buf> ReadPreface<T, B> {
    fn new(codec: Codec<T, B>) -> Self {
        ReadPreface {
            codec: Some(codec),
            pos: 0,
        }
    }

    fn inner_mut(&mut self) -> &mut T {
        self.codec.as_mut().unwrap().get_mut()
    }
}

impl<T, B> Future for ReadPreface<T, B>
where
    T: AsyncRead,
    B: Buf,
{
    type Item = Codec<T, B>;
    type Error = ::Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        let mut buf = [0; 24];
        let mut rem = PREFACE.len() - self.pos;

        while rem > 0 {
            let n = try_nb!(self.inner_mut().read(&mut buf[..rem]));
            if n == 0 {
                return Err(io::Error::new(
                    io::ErrorKind::ConnectionReset,
                    "connection closed unexpectedly",
                ).into());
            }

            if PREFACE[self.pos..self.pos + n] != buf[..n] {
                // TODO: Should this just write the GO_AWAY frame directly?
                return Err(Reason::PROTOCOL_ERROR.into());
            }

            self.pos += n;
            rem -= n; // TODO test
        }

        Ok(Async::Ready(self.codec.take().unwrap()))
    }
}

// ===== impl Handshake =====

impl<T, B: IntoBuf> Future for Handshake<T, B>
    where T: AsyncRead + AsyncWrite,
          B: IntoBuf,
{
    type Item = Connection<T, B>;
    type Error = ::Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        trace!("Handshake::poll(); state={:?};", self.state);
        use server::Handshaking::*;

        self.state = if let Flushing(ref mut flush) = self.state {
            // We're currently flushing a pending SETTINGS frame. Poll the
            // flush future, and, if it's completed, advance our state to wait
            // for the client preface.
            let codec = match flush.poll()? {
                Async::NotReady => {
                    trace!("Handshake::poll(); flush.poll()=NotReady");
                    return Ok(Async::NotReady);
                },
                Async::Ready(flushed) => {
                    trace!("Handshake::poll(); flush.poll()=Ready");
                    flushed
                }
            };
            Handshaking::from(ReadPreface::new(codec))
        } else {
            // Otherwise, we haven't actually advanced the state, but we have
            // to replace it with itself, because we have to return a value.
            // (note that the assignment to `self.state` has to be outside of
            // the `if let` block above in order to placate the borrow checker).
            mem::replace(&mut self.state, Handshaking::Empty)
        };
        let poll = if let ReadingPreface(ref mut read) = self.state {
            // We're now waiting for the client preface. Poll the `ReadPreface`
            // future. If it has completed, we will create a `Connection` handle
            // for the connection.
            read.poll()
            // Actually creating the `Connection` has to occur outside of this
            // `if let` block, because we've borrowed `self` mutably in order
            // to poll the state and won't be able to borrow the SETTINGS frame
            // as well until we release the borrow for `poll()`.
        } else {
            unreachable!("Handshake::poll() state was not advanced completely!")
        };
        let server = poll?.map(|codec| {
            let connection = proto::Connection::new(codec, Config {
                next_stream_id: 2.into(),
                // Server does not need to locally initiate any streams
                initial_max_send_streams: 0,
                reset_stream_duration: self.builder.reset_stream_duration,
                reset_stream_max: self.builder.reset_stream_max,
                settings: self.builder.settings.clone(),
            });

            trace!("Handshake::poll(); connection established!");
            let mut c = Connection { connection };
            if let Some(sz) = self.builder.initial_target_connection_window_size {
                c.set_target_window_size(sz);
            }
            c
        });
        Ok(server)
    }
}

impl<T, B> fmt::Debug for Handshake<T, B>
    where T: AsyncRead + AsyncWrite + fmt::Debug,
          B: fmt::Debug + IntoBuf,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "server::Handshake")
    }
}

impl Peer {
    pub fn convert_send_message(
        id: StreamId,
        response: Response<()>,
        end_of_stream: bool) -> frame::Headers
    {
        use http::response::Parts;

        // Extract the components of the HTTP request
        let (
            Parts {
                status,
                headers,
                ..
            },
            _,
        ) = response.into_parts();

        // Build the set pseudo header set. All requests will include `method`
        // and `path`.
        let pseudo = frame::Pseudo::response(status);

        // Create the HEADERS frame
        let mut frame = frame::Headers::new(id, pseudo, headers);

        if end_of_stream {
            frame.set_end_stream()
        }

        frame
    }
}

impl proto::Peer for Peer {
    type Poll = Request<()>;

    fn is_server() -> bool {
        true
    }

    fn dyn() -> proto::DynPeer {
        proto::DynPeer::Server
    }

    fn convert_poll_message(headers: frame::Headers) -> Result<Self::Poll, RecvError> {
        use http::{uri, Version};

        let mut b = Request::builder();

        let stream_id = headers.stream_id();
        let (pseudo, fields) = headers.into_parts();

        macro_rules! malformed {
            ($($arg:tt)*) => {{
                debug!($($arg)*);
                return Err(RecvError::Stream {
                    id: stream_id,
                    reason: Reason::PROTOCOL_ERROR,
                });
            }}
        };

        b.version(Version::HTTP_2);

        if let Some(method) = pseudo.method {
            b.method(method);
        } else {
            malformed!("malformed headers: missing method");
        }

        // Specifying :status for a request is a protocol error
        if pseudo.status.is_some() {
            return Err(RecvError::Connection(Reason::PROTOCOL_ERROR));
        }

        // Convert the URI
        let mut parts = uri::Parts::default();

        if let Some(scheme) = pseudo.scheme {
            parts.scheme = Some(uri::Scheme::from_shared(scheme.into_inner())
                .or_else(|_| malformed!("malformed headers: malformed scheme"))?);
        } else {
            malformed!("malformed headers: missing scheme");
        }

        if let Some(authority) = pseudo.authority {
            parts.authority = Some(uri::Authority::from_shared(authority.into_inner())
                .or_else(|_| malformed!("malformed headers: malformed authority"))?);
        }

        if let Some(path) = pseudo.path {
            // This cannot be empty
            if path.is_empty() {
                malformed!("malformed headers: missing path");
            }

            parts.path_and_query = Some(uri::PathAndQuery::from_shared(path.into_inner())
                .or_else(|_| malformed!("malformed headers: malformed path"))?);
        }

        b.uri(parts);

        let mut request = match b.body(()) {
            Ok(request) => request,
            Err(_) => {
                // TODO: Should there be more specialized handling for different
                // kinds of errors
                return Err(RecvError::Stream {
                    id: stream_id,
                    reason: Reason::PROTOCOL_ERROR,
                });
            },
        };

        *request.headers_mut() = fields;

        Ok(request)
    }
}

// ===== impl Handshaking =====

impl<T, B> fmt::Debug for Handshaking<T, B>
where
    B: IntoBuf
{
    #[inline] fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match *self {
            Handshaking::Flushing(_) =>
                write!(f, "Handshaking::Flushing(_)"),
            Handshaking::ReadingPreface(_) =>
                write!(f, "Handshaking::ReadingPreface(_)"),
            Handshaking::Empty =>
                write!(f, "Handshaking::Empty"),
        }

    }
}

impl<T, B> convert::From<Flush<T, Prioritized<B::Buf>>> for Handshaking<T, B>
where
    T: AsyncRead + AsyncWrite,
    B: IntoBuf,
{
    #[inline] fn from(flush: Flush<T, Prioritized<B::Buf>>) -> Self {
        Handshaking::Flushing(flush)
    }
}

impl<T, B> convert::From<ReadPreface<T, Prioritized<B::Buf>>> for
    Handshaking<T, B>
where
    T: AsyncRead + AsyncWrite,
    B: IntoBuf,
{
    #[inline] fn from(read: ReadPreface<T, Prioritized<B::Buf>>) -> Self {
        Handshaking::ReadingPreface(read)
    }
}

impl<T, B> convert::From<Codec<T, Prioritized<B::Buf>>> for Handshaking<T, B>
where
    T: AsyncRead + AsyncWrite,
    B: IntoBuf,
{
    #[inline] fn from(codec: Codec<T, Prioritized<B::Buf>>) -> Self {
        Handshaking::from(Flush::new(codec))
    }
}
