// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

/*!

# The HTTP/3 protocol

This crate implements [RFC9114](https://datatracker.ietf.org/doc/html/rfc9114).

The implementation depends on:
 - [neqo-transport](../neqo_transport/index.html) --- implements the QUIC protocol
   ([RFC9000](https://www.rfc-editor.org/info/rfc9000)) and
 - [neqo-qpack](../neqo_qpack/index.html) --- implements QPACK
   ([RFC9204](https://www.rfc-editor.org/info/rfc9204));

## Features

Both client and server-side HTTP/3 protocols are implemented, although the server-side
implementation is not meant to be used in production and its only purpose is to facilitate testing
of the client-side code.

__`WebTransport`__
([draft version 2](https://datatracker.ietf.org/doc/html/draft-vvv-webtransport-http3-02)) is
supported and can be enabled using [`Http3Parameters`](struct.Http3Parameters.html).

## Interaction with an application

### Driving HTTP/3  session

The crate does not create an OS level UDP socket, it produces, i.e. encodes, data that should be
sent as a payload in a UDP packet and consumes data received on the UDP socket. For example,
[`std::net::UdpSocket`](std::net::UdpSocket) or [`mio::net::UdpSocket`](https://crates.io/crates/mio)
could be used for creating UDP sockets.

The application is responsible for creating a socket, polling the socket, and sending and receiving
data from the socket.

In addition to receiving data HTTP/3 session’s actions may be triggered when a certain amount of
time passes, e.g. after a certain amount of time data may be considered lost and should be
retransmitted, packet pacing requires a timer, etc. The implementation does not use timers, but
instead informs the application when processing needs to be triggered.


The core functions for driving HTTP/3 sessions are:
 - __On the client-side__ :
   - [`process_output`](struct.Http3Client.html#method.process_output) used for producing UDP
payload. If a payload is not produced this function returns a callback time, e.g. the time when
[`process_output`](struct.Http3Client.html#method.process_output) should be called again.
   - [`process_input`](struct.Http3Client.html#method.process_input)  used consuming UDP payload.
   - [`process`](struct.Http3Client.html#method.process) combines the 2 functions into one, i.e. it
consumes UDP payload if available and produces some UDP payload to be sent or returns a
callback time.
- __On the server-side__ only [`process`](struct.Http3Server.html#method.process) is
available.

An example interaction with a socket:

```ignore
let socket = match UdpSocket::bind(local_addr) {
    Err(e) => {
        eprintln!("Unable to bind UDP socket: {}", e);
    }
    Ok(s) => s,
};
let mut client = Http3Client::new(...);

...

// process_output can return 3 values, data to be sent, time duration when process_output should
// be called, and None when Http3Client is done.
match client.process_output(Instant::now()) {
    Output::Datagram(dgram) => {
        // Send dgram on a socket.
        socket.send_to(&dgram[..], dgram.destination())

    }
    Output::Callback(duration) => {
        // the client is idle for “duration”, set read timeout on the socket to this value and
        // poll the socket for reading in the meantime.
        socket.set_read_timeout(Some(duration)).unwrap();
    }
    Output::None => {
        // client is done.
    }
};

...

// Reading new data coming for the network.
match socket.recv_from(&mut buf[..]) {
     Ok((sz, remote)) => {
        let d = Datagram::new(remote, *local_addr, &buf[..sz]);
        client.process_input(d, Instant::now());
    }
    Err(err) => {
         eprintln!("UDP error: {}", err);
    }
}
 ```

### HTTP/3 session events

[`Http3Client`](struct.Http3Client.html) and [`Http3Server`](struct.Http3Server.html) produce
events that can be obtain by calling
[`next_event`](neqo_common/event/trait.Provider.html#tymethod.next_event). The events are of type
[`Http3ClientEvent`](enum.Http3ClientEvent.html) and
[`Http3ServerEvent`](enum.Http3ServerEvent.html) respectively. They are informing the application
when the connection changes state, when new data is received on a stream, etc.

```ignore
...

while let Some(event) = client.next_event() {
    match event {
        Http3ClientEvent::DataReadable { stream_id } => {
            println!("New data available on stream {}", stream_id);
        }
        Http3ClientEvent::StateChange(Http3State::Connected) => {
            println!("Http3 session is in state Connected now");
        }
        _ => {
            println!("Unhandled event {:?}", event);
        }
    }
}
```



*/

mod buffered_send_stream;
mod client_events;
mod conn_params;
mod connection;
mod connection_client;
mod connection_server;
mod control_stream_local;
mod control_stream_remote;
pub mod features;
mod frames;
mod headers_checks;
mod priority;
mod push_controller;
mod qlog;
mod qpack_decoder_receiver;
mod qpack_encoder_receiver;
mod recv_message;
mod request_target;
mod send_message;
mod server;
mod server_connection_events;
mod server_events;
mod settings;
mod stream_type_reader;

use neqo_qpack::Error as QpackError;
pub use neqo_transport::{streams::SendOrder, Output, StreamId};
use neqo_transport::{
    AppError, Connection, Error as TransportError, RecvStreamStats, SendStreamStats,
};
use std::fmt::Debug;

use crate::priority::PriorityHandler;
use buffered_send_stream::BufferedStream;
pub use client_events::{Http3ClientEvent, WebTransportEvent};
pub use conn_params::Http3Parameters;
pub use connection::{Http3State, WebTransportSessionAcceptAction};
pub use connection_client::Http3Client;
use features::extended_connect::WebTransportSession;
use frames::HFrame;
pub use neqo_common::Header;
use neqo_common::MessageType;
pub use priority::Priority;
pub use server::Http3Server;
pub use server_events::{
    Http3OrWebTransportStream, Http3ServerEvent, WebTransportRequest, WebTransportServerEvent,
};
use std::any::Any;
use std::cell::RefCell;
use std::rc::Rc;
use stream_type_reader::NewStreamType;

type Res<T> = Result<T, Error>;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    HttpNoError,
    HttpGeneralProtocol,
    HttpGeneralProtocolStream, //this is the same as the above but it should only close a stream not a connection.
    // When using this error, you need to provide a value that is unique, which
    // will allow the specific error to be identified.  This will be validated in CI.
    HttpInternal(u16),
    HttpStreamCreation,
    HttpClosedCriticalStream,
    HttpFrameUnexpected,
    HttpFrame,
    HttpExcessiveLoad,
    HttpId,
    HttpSettings,
    HttpMissingSettings,
    HttpRequestRejected,
    HttpRequestCancelled,
    HttpRequestIncomplete,
    HttpConnect,
    HttpVersionFallback,
    HttpMessageError,
    QpackError(neqo_qpack::Error),

    // Internal errors from here.
    AlreadyClosed,
    AlreadyInitialized,
    DecodingFrame,
    FatalError,
    HttpGoaway,
    Internal,
    InvalidHeader,
    InvalidInput,
    InvalidRequestTarget,
    InvalidResumptionToken,
    InvalidState,
    InvalidStreamId,
    NoMoreData,
    NotEnoughData,
    StreamLimitError,
    TransportError(TransportError),
    TransportStreamDoesNotExist,
    Unavailable,
    Unexpected,
}

impl Error {
    #[must_use]
    pub fn code(&self) -> AppError {
        match self {
            Self::HttpNoError => 0x100,
            Self::HttpGeneralProtocol | Self::HttpGeneralProtocolStream | Self::InvalidHeader => {
                0x101
            }
            Self::HttpInternal(..) => 0x102,
            Self::HttpStreamCreation => 0x103,
            Self::HttpClosedCriticalStream => 0x104,
            Self::HttpFrameUnexpected => 0x105,
            Self::HttpFrame => 0x106,
            Self::HttpExcessiveLoad => 0x107,
            Self::HttpId => 0x108,
            Self::HttpSettings => 0x109,
            Self::HttpMissingSettings => 0x10a,
            Self::HttpRequestRejected => 0x10b,
            Self::HttpRequestCancelled => 0x10c,
            Self::HttpRequestIncomplete => 0x10d,
            Self::HttpMessageError => 0x10e,
            Self::HttpConnect => 0x10f,
            Self::HttpVersionFallback => 0x110,
            Self::QpackError(e) => e.code(),
            // These are all internal errors.
            _ => 3,
        }
    }

    #[must_use]
    pub fn connection_error(&self) -> bool {
        matches!(
            self,
            Self::HttpGeneralProtocol
                | Self::HttpInternal(..)
                | Self::HttpStreamCreation
                | Self::HttpClosedCriticalStream
                | Self::HttpFrameUnexpected
                | Self::HttpFrame
                | Self::HttpExcessiveLoad
                | Self::HttpId
                | Self::HttpSettings
                | Self::HttpMissingSettings
                | Self::QpackError(QpackError::EncoderStream | QpackError::DecoderStream)
        )
    }

    #[must_use]
    pub fn stream_reset_error(&self) -> bool {
        matches!(self, Self::HttpGeneralProtocolStream | Self::InvalidHeader)
    }

    /// # Panics
    /// On unexpected errors, in debug mode.
    #[must_use]
    pub fn map_stream_send_errors(err: &Error) -> Self {
        match err {
            Self::TransportError(
                TransportError::InvalidStreamId | TransportError::FinalSizeError,
            ) => Error::TransportStreamDoesNotExist,
            Self::TransportError(TransportError::InvalidInput) => Error::InvalidInput,
            _ => {
                debug_assert!(false, "Unexpected error");
                Error::TransportStreamDoesNotExist
            }
        }
    }

    /// # Panics
    /// On unexpected errors, in debug mode.
    #[must_use]
    pub fn map_stream_create_errors(err: &TransportError) -> Self {
        match err {
            TransportError::ConnectionState => Error::Unavailable,
            TransportError::StreamLimitError => Error::StreamLimitError,
            _ => {
                debug_assert!(false, "Unexpected error");
                Error::TransportStreamDoesNotExist
            }
        }
    }

    /// # Panics
    /// On unexpected errors, in debug mode.
    #[must_use]
    pub fn map_stream_recv_errors(err: &Error) -> Self {
        match err {
            Self::TransportError(TransportError::NoMoreData) => {
                debug_assert!(
                    false,
                    "Do not call stream_recv if FIN has been previously read"
                );
            }
            Self::TransportError(TransportError::InvalidStreamId) => {}
            _ => {
                debug_assert!(false, "Unexpected error");
            }
        };
        Error::TransportStreamDoesNotExist
    }

    #[must_use]
    pub fn map_set_resumption_errors(err: &TransportError) -> Self {
        match err {
            TransportError::ConnectionState => Error::InvalidState,
            _ => Error::InvalidResumptionToken,
        }
    }

    /// # Errors
    ///   Any error is mapped to the indicated type.
    /// # Panics
    /// On internal errors, in debug mode.
    fn map_error<R>(r: Result<R, impl Into<Self>>, err: Self) -> Result<R, Self> {
        r.map_err(|e| {
            debug_assert!(!matches!(e.into(), Self::HttpInternal(..)));
            debug_assert!(!matches!(err, Self::HttpInternal(..)));
            err
        })
    }
}

impl From<TransportError> for Error {
    fn from(err: TransportError) -> Self {
        Self::TransportError(err)
    }
}

impl From<QpackError> for Error {
    fn from(err: QpackError) -> Self {
        match err {
            QpackError::ClosedCriticalStream => Error::HttpClosedCriticalStream,
            e => Self::QpackError(e),
        }
    }
}

impl From<AppError> for Error {
    fn from(error: AppError) -> Self {
        match error {
            0x100 => Self::HttpNoError,
            0x101 => Self::HttpGeneralProtocol,
            0x103 => Self::HttpStreamCreation,
            0x104 => Self::HttpClosedCriticalStream,
            0x105 => Self::HttpFrameUnexpected,
            0x106 => Self::HttpFrame,
            0x107 => Self::HttpExcessiveLoad,
            0x108 => Self::HttpId,
            0x109 => Self::HttpSettings,
            0x10a => Self::HttpMissingSettings,
            0x10b => Self::HttpRequestRejected,
            0x10c => Self::HttpRequestCancelled,
            0x10d => Self::HttpRequestIncomplete,
            0x10f => Self::HttpConnect,
            0x110 => Self::HttpVersionFallback,
            0x200 => Self::QpackError(QpackError::DecompressionFailed),
            0x201 => Self::QpackError(QpackError::EncoderStream),
            0x202 => Self::QpackError(QpackError::DecoderStream),
            _ => Self::HttpInternal(0),
        }
    }
}

impl ::std::error::Error for Error {
    fn source(&self) -> Option<&(dyn ::std::error::Error + 'static)> {
        match self {
            Self::TransportError(e) => Some(e),
            Self::QpackError(e) => Some(e),
            _ => None,
        }
    }
}

impl ::std::fmt::Display for Error {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "HTTP/3 error: {self:?}")
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Http3StreamType {
    Control,
    Decoder,
    Encoder,
    NewStream,
    Http,
    Push,
    ExtendedConnect,
    WebTransport(StreamId),
    Unknown,
}

#[must_use]
#[derive(PartialEq, Eq, Debug)]
enum ReceiveOutput {
    NoOutput,
    ControlFrames(Vec<HFrame>),
    UnblockedStreams(Vec<StreamId>),
    NewStream(NewStreamType),
}

impl Default for ReceiveOutput {
    fn default() -> Self {
        Self::NoOutput
    }
}

trait Stream: Debug {
    fn stream_type(&self) -> Http3StreamType;
}

trait RecvStream: Stream {
    /// The stream reads data from the corresponding quic stream and returns `ReceiveOutput`.
    /// The function also returns true as the second parameter if the stream is done and
    /// could be forgotten, i.e. removed from all records.
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn receive(&mut self, conn: &mut Connection) -> Res<(ReceiveOutput, bool)>;
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, etc.
    fn reset(&mut self, close_type: CloseType) -> Res<()>;
    /// The function allows an app to read directly from the quic stream. The function
    /// returns the number of bytes written into `buf` and true/false if the stream is
    /// completely done and can be forgotten, i.e. removed from all records.
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn read_data(&mut self, _conn: &mut Connection, _buf: &mut [u8]) -> Res<(usize, bool)> {
        Err(Error::InvalidStreamId)
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpRecvStream> {
        None
    }

    fn webtransport(&self) -> Option<Rc<RefCell<WebTransportSession>>> {
        None
    }

    /// This function is only implemented by `WebTransportRecvStream`.
    fn stats(&mut self, _conn: &mut Connection) -> Res<RecvStreamStats> {
        Err(Error::Unavailable)
    }
}

trait HttpRecvStream: RecvStream {
    /// This function is similar to the receive function and has the same output, i.e.
    /// a `ReceiveOutput` enum and bool. The bool is true if the stream is completely done
    /// and can be forgotten, i.e. removed from all records.
    /// # Errors
    /// An error may happen while reading a stream, e.g. early close, protocol error, etc.
    fn header_unblocked(&mut self, conn: &mut Connection) -> Res<(ReceiveOutput, bool)>;

    fn maybe_update_priority(&mut self, priority: Priority) -> bool;
    fn priority_update_frame(&mut self) -> Option<HFrame>;
    fn priority_update_sent(&mut self);

    fn set_new_listener(&mut self, _conn_events: Box<dyn HttpRecvStreamEvents>) {}
    fn extended_connect_wait_for_response(&self) -> bool {
        false
    }

    fn any(&self) -> &dyn Any;
}

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct Http3StreamInfo {
    stream_id: StreamId,
    stream_type: Http3StreamType,
}

impl Http3StreamInfo {
    #[must_use]
    pub fn new(stream_id: StreamId, stream_type: Http3StreamType) -> Self {
        Self {
            stream_id,
            stream_type,
        }
    }

    #[must_use]
    pub fn stream_id(&self) -> StreamId {
        self.stream_id
    }

    #[must_use]
    pub fn session_id(&self) -> Option<StreamId> {
        if let Http3StreamType::WebTransport(session) = self.stream_type {
            Some(session)
        } else {
            None
        }
    }

    #[must_use]
    pub fn is_http(&self) -> bool {
        self.stream_type == Http3StreamType::Http
    }
}

trait RecvStreamEvents: Debug {
    fn data_readable(&self, _stream_info: Http3StreamInfo) {}
    fn recv_closed(&self, _stream_info: Http3StreamInfo, _close_type: CloseType) {}
}

trait HttpRecvStreamEvents: RecvStreamEvents {
    fn header_ready(
        &self,
        stream_info: Http3StreamInfo,
        headers: Vec<Header>,
        interim: bool,
        fin: bool,
    );
    fn extended_connect_new_session(&self, _stream_id: StreamId, _headers: Vec<Header>) {}
}

trait SendStream: Stream {
    /// # Errors
    /// Error my occur during sending data, e.g. protocol error, etc.
    fn send(&mut self, conn: &mut Connection) -> Res<()>;
    fn has_data_to_send(&self) -> bool;
    fn stream_writable(&self);
    fn done(&self) -> bool;
    fn set_sendorder(&mut self, conn: &mut Connection, sendorder: Option<SendOrder>) -> Res<()>;
    fn set_fairness(&mut self, conn: &mut Connection, fairness: bool) -> Res<()>;
    /// # Errors
    /// Error my occur during sending data, e.g. protocol error, etc.
    fn send_data(&mut self, _conn: &mut Connection, _buf: &[u8]) -> Res<usize>;

    /// # Errors
    /// It may happen that the transport stream is already close. This is unlikely.
    fn close(&mut self, conn: &mut Connection) -> Res<()>;
    /// # Errors
    /// It may happen that the transport stream is already close. This is unlikely.
    fn close_with_message(
        &mut self,
        _conn: &mut Connection,
        _error: u32,
        _message: &str,
    ) -> Res<()> {
        Err(Error::InvalidStreamId)
    }
    /// This function is called when sending side is closed abruptly by the peer or
    /// the application.
    fn handle_stop_sending(&mut self, close_type: CloseType);
    fn http_stream(&mut self) -> Option<&mut dyn HttpSendStream> {
        None
    }

    /// # Errors
    /// It may happen that the transport stream is already close. This is unlikely.
    fn send_data_atomic(&mut self, _conn: &mut Connection, _buf: &[u8]) -> Res<()> {
        Err(Error::InvalidStreamId)
    }

    /// This function is only implemented by `WebTransportSendStream`.
    fn stats(&mut self, _conn: &mut Connection) -> Res<SendStreamStats> {
        Err(Error::Unavailable)
    }
}

trait HttpSendStream: SendStream {
    /// This function is used to supply headers to a http message. The
    /// function is used for request headers, response headers, 1xx response and
    /// trailers.
    /// # Errors
    /// This can also return an error if the underlying stream is closed.
    fn send_headers(&mut self, headers: &[Header], conn: &mut Connection) -> Res<()>;
    fn set_new_listener(&mut self, _conn_events: Box<dyn SendStreamEvents>) {}
    fn any(&self) -> &dyn Any;
}

trait SendStreamEvents: Debug {
    fn send_closed(&self, _stream_info: Http3StreamInfo, _close_type: CloseType) {}
    fn data_writable(&self, _stream_info: Http3StreamInfo) {}
}

/// This enum is used to mark a different type of closing a stream:
///   `ResetApp` - the application has closed the stream.
///   `ResetRemote` - the stream was closed by the peer.
///   `LocalError` - There was a stream error on the stream. The stream errors are errors
///                  that do not close the complete connection, e.g. unallowed headers.
///   `Done` - the stream was closed without an error.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum CloseType {
    ResetApp(AppError),
    ResetRemote(AppError),
    LocalError(AppError),
    Done,
}

impl CloseType {
    #[must_use]
    pub fn error(&self) -> Option<AppError> {
        match self {
            Self::ResetApp(error) | Self::ResetRemote(error) | Self::LocalError(error) => {
                Some(*error)
            }
            Self::Done => None,
        }
    }

    #[must_use]
    pub fn locally_initiated(&self) -> bool {
        matches!(self, CloseType::ResetApp(_))
    }
}
