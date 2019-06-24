//! Generic WebSocket message stream.

pub mod frame;

mod message;

pub use self::message::Message;
pub use self::frame::CloseFrame;

use std::collections::VecDeque;
use std::io::{Read, Write, ErrorKind as IoErrorKind};
use std::mem::replace;

use error::{Error, Result};
use self::message::{IncompleteMessage, IncompleteMessageType};
use self::frame::{Frame, FrameCodec};
use self::frame::coding::{OpCode, Data as OpData, Control as OpCtl, CloseCode};
use util::NonBlockingResult;

/// Indicates a Client or Server role of the websocket
#[derive(Debug, Clone, Copy)]
pub enum Role {
    /// This socket is a server
    Server,
    /// This socket is a client
    Client,
}

/// The configuration for WebSocket connection.
#[derive(Debug, Clone, Copy)]
pub struct WebSocketConfig {
    /// The size of the send queue. You can use it to turn on/off the backpressure features. `None`
    /// means here that the size of the queue is unlimited. The default value is the unlimited
    /// queue.
    pub max_send_queue: Option<usize>,
    /// The maximum size of a message. `None` means no size limit. The default value is 64 megabytes
    /// which should be reasonably big for all normal use-cases but small enough to prevent
    /// memory eating by a malicious user.
    pub max_message_size: Option<usize>,
    /// The maximum size of a single message frame. `None` means no size limit. The limit is for
    /// frame payload NOT including the frame header. The default value is 16 megabytes which should
    /// be reasonably big for all normal use-cases but small enough to prevent memory eating
    /// by a malicious user.
    pub max_frame_size: Option<usize>,
}

impl Default for WebSocketConfig {
    fn default() -> Self {
        WebSocketConfig {
            max_send_queue: None,
            max_message_size: Some(64 << 20),
            max_frame_size: Some(16 << 20),
        }
    }
}

/// WebSocket input-output stream.
///
/// This is THE structure you want to create to be able to speak the WebSocket protocol.
/// It may be created by calling `connect`, `accept` or `client` functions.
#[derive(Debug)]
pub struct WebSocket<Stream> {
    /// The underlying socket.
    socket: Stream,
    /// The context for managing a WebSocket.
    context: WebSocketContext,
}

impl<Stream> WebSocket<Stream> {
    /// Convert a raw socket into a WebSocket without performing a handshake.
    ///
    /// Call this function if you're using Tungstenite as a part of a web framework
    /// or together with an existing one. If you need an initial handshake, use
    /// `connect()` or `accept()` functions of the crate to construct a websocket.
    pub fn from_raw_socket(stream: Stream, role: Role, config: Option<WebSocketConfig>) -> Self {
        WebSocket {
            socket: stream,
            context: WebSocketContext::new(role, config),
        }
    }

    /// Convert a raw socket into a WebSocket without performing a handshake.
    ///
    /// Call this function if you're using Tungstenite as a part of a web framework
    /// or together with an existing one. If you need an initial handshake, use
    /// `connect()` or `accept()` functions of the crate to construct a websocket.
    pub fn from_partially_read(
        stream: Stream,
        part: Vec<u8>,
        role: Role,
        config: Option<WebSocketConfig>,
    ) -> Self {
        WebSocket {
            socket: stream,
            context: WebSocketContext::from_partially_read(part, role, config),
        }
    }

    /// Returns a shared reference to the inner stream.
    pub fn get_ref(&self) -> &Stream {
        &self.socket
    }
    /// Returns a mutable reference to the inner stream.
    pub fn get_mut(&mut self) -> &mut Stream {
        &mut self.socket
    }

    /// Change the configuration.
    pub fn set_config(&mut self, set_func: impl FnOnce(&mut WebSocketConfig)) {
        self.context.set_config(set_func)
    }
}

impl<Stream: Read + Write> WebSocket<Stream> {
    /// Read a message from stream, if possible.
    ///
    /// This function sends pong and close responses automatically.
    /// However, it never blocks on write.
    pub fn read_message(&mut self) -> Result<Message> {
        self.context.read_message(&mut self.socket)
    }

    /// Send a message to stream, if possible.
    ///
    /// WebSocket will buffer a configurable number of messages at a time, except to reply to Ping
    /// and Close requests. If the WebSocket's send queue is full, `SendQueueFull` will be returned
    /// along with the passed message. Otherwise, the message is queued and Ok(()) is returned.
    ///
    /// Note that only the last pong frame is stored to be sent, and only the
    /// most recent pong frame is sent if multiple pong frames are queued.
    pub fn write_message(&mut self, message: Message) -> Result<()> {
        self.context.write_message(&mut self.socket, message)
    }

    /// Flush the pending send queue.
    pub fn write_pending(&mut self) -> Result<()> {
        self.context.write_pending(&mut self.socket)
    }

    /// Close the connection.
    ///
    /// This function guarantees that the close frame will be queued.
    /// There is no need to call it again. Calling this function is
    /// the same as calling `write(Message::Close(..))`.
    pub fn close(&mut self, code: Option<CloseFrame>) -> Result<()> {
        self.context.close(&mut self.socket, code)
    }
}


/// A context for managing WebSocket stream.
#[derive(Debug)]
pub struct WebSocketContext {
    /// Server or client?
    role: Role,
    /// encoder/decoder of frame.
    frame: FrameCodec,
    /// The state of processing, either "active" or "closing".
    state: WebSocketState,
    /// Receive: an incomplete message being processed.
    incomplete: Option<IncompleteMessage>,
    /// Send: a data send queue.
    send_queue: VecDeque<Frame>,
    /// Send: an OOB pong message.
    pong: Option<Frame>,
    /// The configuration for the websocket session.
    config: WebSocketConfig,
}

impl WebSocketContext {
    /// Create a WebSocket context that manages a post-handshake stream.
    pub fn new(role: Role, config: Option<WebSocketConfig>) -> Self {
        WebSocketContext {
            role,
            frame: FrameCodec::new(),
            state: WebSocketState::Active,
            incomplete: None,
            send_queue: VecDeque::new(),
            pong: None,
            config: config.unwrap_or_else(WebSocketConfig::default),
        }
    }

    /// Create a WebSocket context that manages an post-handshake stream.
    pub fn from_partially_read(
        part: Vec<u8>,
        role: Role,
        config: Option<WebSocketConfig>,
    ) -> Self {
        WebSocketContext {
            frame: FrameCodec::from_partially_read(part),
            ..WebSocketContext::new(role, config)
        }
    }

    /// Change the configuration.
    pub fn set_config(&mut self, set_func: impl FnOnce(&mut WebSocketConfig)) {
        set_func(&mut self.config)
    }

    /// Read a message from the provided stream, if possible.
    ///
    /// This function sends pong and close responses automatically.
    /// However, it never blocks on write.
    pub fn read_message<Stream>(&mut self, stream: &mut Stream) -> Result<Message>
    where
        Stream: Read + Write,
    {
        // Do not read from already closed connections.
        self.state.check_active()?;

        loop {
            // Since we may get ping or close, we need to reply to the messages even during read.
            // Thus we call write_pending() but ignore its blocking.
            self.write_pending(stream).no_block()?;
            // If we get here, either write blocks or we have nothing to write.
            // Thus if read blocks, just let it return WouldBlock.
            if let Some(message) = self.read_message_frame(stream)? {
                trace!("Received message {}", message);
                return Ok(message)
            }
        }
    }

    /// Send a message to the provided stream, if possible.
    ///
    /// WebSocket will buffer a configurable number of messages at a time, except to reply to Ping
    /// and Close requests. If the WebSocket's send queue is full, `SendQueueFull` will be returned
    /// along with the passed message. Otherwise, the message is queued and Ok(()) is returned.
    ///
    /// Note that only the last pong frame is stored to be sent, and only the
    /// most recent pong frame is sent if multiple pong frames are queued.
    pub fn write_message<Stream>(&mut self, stream: &mut Stream, message: Message) -> Result<()>
    where
        Stream: Read + Write,
    {
        // Do not write to already closed connections.
        self.state.check_active()?;

        if let Some(max_send_queue) = self.config.max_send_queue {
            if self.send_queue.len() >= max_send_queue {
                // Try to make some room for the new message.
                // Do not return here if write would block, ignore WouldBlock silently
                // since we must queue the message anyway.
                self.write_pending(stream).no_block()?;
            }

            if self.send_queue.len() >= max_send_queue {
                return Err(Error::SendQueueFull(message));
            }
        }

        let frame = match message {
            Message::Text(data) => {
                Frame::message(data.into(), OpCode::Data(OpData::Text), true)
            }
            Message::Binary(data) => {
                Frame::message(data, OpCode::Data(OpData::Binary), true)
            }
            Message::Ping(data) => Frame::ping(data),
            Message::Pong(data) => {
                self.pong = Some(Frame::pong(data));
                return self.write_pending(stream)
            }
            Message::Close(code) => {
                return self.close(stream, code)
            }
        };

        self.send_queue.push_back(frame);
        self.write_pending(stream)
    }

    /// Flush the pending send queue.
    pub fn write_pending<Stream>(&mut self, stream: &mut Stream) -> Result<()>
    where
        Stream: Read + Write,
    {
        // First, make sure we have no pending frame sending.
        self.frame.write_pending(stream)?;

        // Upon receipt of a Ping frame, an endpoint MUST send a Pong frame in
        // response, unless it already received a Close frame. It SHOULD
        // respond with Pong frame as soon as is practical. (RFC 6455)
        if let Some(pong) = self.pong.take() {
            trace!("Sending pong reply");
            self.send_one_frame(stream, pong)?;
        }
        // If we have any unsent frames, send them.
        trace!("Frames still in queue: {}", self.send_queue.len());
        while let Some(data) = self.send_queue.pop_front() {
            self.send_one_frame(stream, data)?;
        }

        // If we get to this point, the send queue is empty and the underlying socket is still
        // willing to take more data.

        // If we're closing and there is nothing to send anymore, we should close the connection.
        if let (Role::Server, WebSocketState::ClosedByPeer) = (&self.role, &self.state) {
            // The underlying TCP connection, in most normal cases, SHOULD be closed
            // first by the server, so that it holds the TIME_WAIT state and not the
            // client (as this would prevent it from re-opening the connection for 2
            // maximum segment lifetimes (2MSL), while there is no corresponding
            // server impact as a TIME_WAIT connection is immediately reopened upon
            // a new SYN with a higher seq number). (RFC 6455)
            self.state = WebSocketState::Terminated;
            Err(Error::ConnectionClosed)
        } else {
            Ok(())
        }
    }

    /// Close the connection.
    ///
    /// This function guarantees that the close frame will be queued.
    /// There is no need to call it again. Calling this function is
    /// the same as calling `write(Message::Close(..))`.
    pub fn close<Stream>(&mut self, stream: &mut Stream, code: Option<CloseFrame>) -> Result<()>
    where
        Stream: Read + Write,
    {
        if let WebSocketState::Active = self.state {
            self.state = WebSocketState::ClosedByUs;
            let frame = Frame::close(code);
            self.send_queue.push_back(frame);
        } else {
            // Already closed, nothing to do.
        }
        self.write_pending(stream)
    }
}

impl WebSocketContext {
    /// Try to decode one message frame. May return None.
    fn read_message_frame<Stream>(&mut self, stream: &mut Stream) -> Result<Option<Message>>
    where
        Stream: Read + Write,
    {
        if let Some(mut frame) = self.frame.read_frame(stream, self.config.max_frame_size)? {

            // MUST be 0 unless an extension is negotiated that defines meanings
            // for non-zero values.  If a nonzero value is received and none of
            // the negotiated extensions defines the meaning of such a nonzero
            // value, the receiving endpoint MUST _Fail the WebSocket
            // Connection_.
            {
                let hdr = frame.header();
                if hdr.rsv1 || hdr.rsv2 || hdr.rsv3 {
                    return Err(Error::Protocol("Reserved bits are non-zero".into()))
                }
            }

            match self.role {
                Role::Server => {
                    if frame.is_masked() {
                        // A server MUST remove masking for data frames received from a client
                        // as described in Section 5.3. (RFC 6455)
                        frame.apply_mask()
                    } else {
                        // The server MUST close the connection upon receiving a
                        // frame that is not masked. (RFC 6455)
                        return Err(Error::Protocol("Received an unmasked frame from client".into()))
                    }
                }
                Role::Client => {
                    if frame.is_masked() {
                        // A client MUST close a connection if it detects a masked frame. (RFC 6455)
                        return Err(Error::Protocol("Received a masked frame from server".into()))
                    }
                }
            }

            match frame.header().opcode {

                OpCode::Control(ctl) => {
                    match ctl {
                        // All control frames MUST have a payload length of 125 bytes or less
                        // and MUST NOT be fragmented. (RFC 6455)
                        _ if !frame.header().is_final => {
                            Err(Error::Protocol("Fragmented control frame".into()))
                        }
                        _ if frame.payload().len() > 125 => {
                            Err(Error::Protocol("Control frame too big".into()))
                        }
                        OpCtl::Close => {
                            Ok(self.do_close(frame.into_close()?).map(Message::Close))
                        }
                        OpCtl::Reserved(i) => {
                            Err(Error::Protocol(format!("Unknown control frame type {}", i).into()))
                        }
                        OpCtl::Ping | OpCtl::Pong if !self.state.is_active() => {
                            // No ping processing while closing.
                            Ok(None)
                        }
                        OpCtl::Ping => {
                            let data = frame.into_data();
                            self.pong = Some(Frame::pong(data.clone()));
                            Ok(Some(Message::Ping(data)))
                        }
                        OpCtl::Pong => {
                            Ok(Some(Message::Pong(frame.into_data())))
                        }
                    }
                }

                OpCode::Data(_) if !self.state.is_active() => {
                    // No data processing while closing.
                    Ok(None)
                }

                OpCode::Data(data) => {
                    let fin = frame.header().is_final;
                    match data {
                        OpData::Continue => {
                            if let Some(ref mut msg) = self.incomplete {
                                msg.extend(frame.into_data(), self.config.max_message_size)?;
                            } else {
                                return Err(Error::Protocol("Continue frame but nothing to continue".into()))
                            }
                            if fin {
                                Ok(Some(self.incomplete.take().unwrap().complete()?))
                            } else {
                                Ok(None)
                            }
                        }
                        c if self.incomplete.is_some() => {
                            Err(Error::Protocol(
                                format!("Received {} while waiting for more fragments", c).into()
                            ))
                        }
                        OpData::Text | OpData::Binary => {
                            let msg = {
                                let message_type = match data {
                                    OpData::Text => IncompleteMessageType::Text,
                                    OpData::Binary => IncompleteMessageType::Binary,
                                    _ => panic!("Bug: message is not text nor binary"),
                                };
                                let mut m = IncompleteMessage::new(message_type);
                                m.extend(frame.into_data(), self.config.max_message_size)?;
                                m
                            };
                            if fin {
                                Ok(Some(msg.complete()?))
                            } else {
                                self.incomplete = Some(msg);
                                Ok(None)
                            }
                        }
                        OpData::Reserved(i) => {
                            Err(Error::Protocol(format!("Unknown data frame type {}", i).into()))
                        }
                    }
                }

            } // match opcode

        } else {
            // Connection closed by peer
            match replace(&mut self.state, WebSocketState::Terminated) {
                WebSocketState::ClosedByPeer | WebSocketState::CloseAcknowledged => {
                    Err(Error::ConnectionClosed)
                }
                _ => {
                    Err(Error::Protocol("Connection reset without closing handshake".into()))
                }
            }
        }
    }

    /// Received a close frame. Tells if we need to return a close frame to the user.
    fn do_close<'t>(&mut self, close: Option<CloseFrame<'t>>) -> Option<Option<CloseFrame<'t>>> {
        debug!("Received close frame: {:?}", close);
        match self.state {
            WebSocketState::Active => {
                let close_code = close.as_ref().map(|f| f.code);
                self.state = WebSocketState::ClosedByPeer;
                let reply = if let Some(code) = close_code {
                    if code.is_allowed() {
                        Frame::close(Some(CloseFrame {
                            code: CloseCode::Normal,
                            reason: "".into(),
                        }))
                    } else {
                        Frame::close(Some(CloseFrame {
                            code: CloseCode::Protocol,
                            reason: "Protocol violation".into()
                        }))
                    }
                } else {
                    Frame::close(None)
                };
                debug!("Replying to close with {:?}", reply);
                self.send_queue.push_back(reply);

                Some(close)
            }
            WebSocketState::ClosedByPeer | WebSocketState::CloseAcknowledged => {
                // It is already closed, just ignore.
                None
            }
            WebSocketState::ClosedByUs => {
                // We received a reply.
                self.state = WebSocketState::CloseAcknowledged;
                Some(close)
            }
            WebSocketState::Terminated => unreachable!(),
        }
    }

    /// Send a single pending frame.
    fn send_one_frame<Stream>(&mut self, stream: &mut Stream, mut frame: Frame) -> Result<()>
    where
        Stream: Read + Write,
    {
        match self.role {
            Role::Server => {
            }
            Role::Client => {
                // 5.  If the data is being sent by the client, the frame(s) MUST be
                // masked as defined in Section 5.3. (RFC 6455)
                frame.set_random_mask();
            }
        }

        trace!("Sending frame: {:?}", frame);
        let res = self.frame.write_frame(stream, frame);
        // An expected "Connection reset by peer" is not fatal
        match res {
            Err(Error::Io(err)) => Err({
                match self.state {
                    WebSocketState::ClosedByPeer | WebSocketState::CloseAcknowledged
                        if err.kind() == IoErrorKind::ConnectionReset =>
                            Error::ConnectionClosed,
                    _ => Error::Io(err),
                }
            }),
            x => x,
        }
    }
}


/// The current connection state.
#[derive(Debug)]
enum WebSocketState {
    /// The connection is active.
    Active,
    /// We initiated a close handshake.
    ClosedByUs,
    /// The peer initiated a close handshake.
    ClosedByPeer,
    /// The peer replied to our close handshake.
    CloseAcknowledged,
    /// The connection does not exist anymore.
    Terminated,
}

impl WebSocketState {
    /// Tell if we're allowed to process normal messages.
    fn is_active(&self) -> bool {
        match *self {
            WebSocketState::Active => true,
            _ => false,
        }
    }

    /// Check if the state is active, return error if not.
    fn check_active(&self) -> Result<()> {
        match self {
            WebSocketState::Terminated
                => Err(Error::AlreadyClosed),
            _ => Ok(()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{WebSocket, Role, Message, WebSocketConfig};

    use std::io;
    use std::io::Cursor;

    struct WriteMoc<Stream>(Stream);

    impl<Stream> io::Write for WriteMoc<Stream> {
        fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
            Ok(buf.len())
        }
        fn flush(&mut self) -> io::Result<()> {
            Ok(())
        }
    }

    impl<Stream: io::Read> io::Read for WriteMoc<Stream> {
        fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
            self.0.read(buf)
        }
    }


    #[test]
    fn receive_messages() {
        let incoming = Cursor::new(vec![
            0x89, 0x02, 0x01, 0x02,
            0x8a, 0x01, 0x03,
            0x01, 0x07,
            0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20,
            0x80, 0x06,
            0x57, 0x6f, 0x72, 0x6c, 0x64, 0x21,
            0x82, 0x03,
            0x01, 0x02, 0x03,
        ]);
        let mut socket = WebSocket::from_raw_socket(WriteMoc(incoming), Role::Client, None);
        assert_eq!(socket.read_message().unwrap(), Message::Ping(vec![1, 2]));
        assert_eq!(socket.read_message().unwrap(), Message::Pong(vec![3]));
        assert_eq!(socket.read_message().unwrap(), Message::Text("Hello, World!".into()));
        assert_eq!(socket.read_message().unwrap(), Message::Binary(vec![0x01, 0x02, 0x03]));
    }


    #[test]
    fn size_limiting_text_fragmented() {
        let incoming = Cursor::new(vec![
            0x01, 0x07,
            0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20,
            0x80, 0x06,
            0x57, 0x6f, 0x72, 0x6c, 0x64, 0x21,
        ]);
        let limit = WebSocketConfig {
            max_message_size: Some(10),
            .. WebSocketConfig::default()
        };
        let mut socket = WebSocket::from_raw_socket(WriteMoc(incoming), Role::Client, Some(limit));
        assert_eq!(socket.read_message().unwrap_err().to_string(),
            "Space limit exceeded: Message too big: 7 + 6 > 10"
        );
    }

    #[test]
    fn size_limiting_binary() {
        let incoming = Cursor::new(vec![
            0x82, 0x03,
            0x01, 0x02, 0x03,
        ]);
        let limit = WebSocketConfig {
            max_message_size: Some(2),
            .. WebSocketConfig::default()
        };
        let mut socket = WebSocket::from_raw_socket(WriteMoc(incoming), Role::Client, Some(limit));
        assert_eq!(socket.read_message().unwrap_err().to_string(),
            "Space limit exceeded: Message too big: 0 + 3 > 2"
        );
    }
}
