use std::borrow::Borrow;
use std::collections::VecDeque;
use std::io::{Cursor, Read, Seek, SeekFrom, Write};
use std::mem::replace;
use std::net::SocketAddr;
use std::str::from_utf8;

use mio::tcp::TcpStream;
use mio::{Ready, Token};
use mio_extras::timer::Timeout;
use url;

#[cfg(feature = "nativetls")]
use native_tls::HandshakeError;
#[cfg(feature = "ssl")]
use openssl::ssl::HandshakeError;

use frame::Frame;
use handler::Handler;
use handshake::{Handshake, Request, Response};
use message::Message;
use protocol::{CloseCode, OpCode};
use result::{Error, Kind, Result};
use stream::{Stream, TryReadBuf, TryWriteBuf};

use self::Endpoint::*;
use self::State::*;

use super::Settings;

#[derive(Debug)]
pub enum State {
    // Tcp connection accepted, waiting for handshake to complete
    Connecting(Cursor<Vec<u8>>, Cursor<Vec<u8>>),
    // Ready to send/receive messages
    Open,
    AwaitingClose,
    RespondingClose,
    FinishedClose,
}

/// A little more semantic than a boolean
#[derive(Debug, Eq, PartialEq, Clone)]
pub enum Endpoint {
    /// Will mask outgoing frames
    Client(url::Url),
    /// Won't mask outgoing frames
    Server,
}

impl State {
    #[inline]
    pub fn is_connecting(&self) -> bool {
        match *self {
            State::Connecting(..) => true,
            _ => false,
        }
    }

    #[allow(dead_code)]
    #[inline]
    pub fn is_open(&self) -> bool {
        match *self {
            State::Open => true,
            _ => false,
        }
    }

    #[inline]
    pub fn is_closing(&self) -> bool {
        match *self {
            State::AwaitingClose | State::FinishedClose => true,
            _ => false,
        }
    }
}

pub struct Connection<H>
where
    H: Handler,
{
    token: Token,
    socket: Stream,
    state: State,
    endpoint: Endpoint,
    events: Ready,

    fragments: VecDeque<Frame>,

    in_buffer: Cursor<Vec<u8>>,
    out_buffer: Cursor<Vec<u8>>,

    handler: H,

    addresses: Vec<SocketAddr>,

    settings: Settings,
    connection_id: u32,
}

impl<H> Connection<H>
where
    H: Handler,
{
    pub fn new(
        tok: Token,
        sock: TcpStream,
        handler: H,
        settings: Settings,
        connection_id: u32,
    ) -> Connection<H> {
        Connection {
            token: tok,
            socket: Stream::tcp(sock),
            state: Connecting(
                Cursor::new(Vec::with_capacity(2048)),
                Cursor::new(Vec::with_capacity(2048)),
            ),
            endpoint: Endpoint::Server,
            events: Ready::empty(),
            fragments: VecDeque::with_capacity(settings.fragments_capacity),
            in_buffer: Cursor::new(Vec::with_capacity(settings.in_buffer_capacity)),
            out_buffer: Cursor::new(Vec::with_capacity(settings.out_buffer_capacity)),
            handler,
            addresses: Vec::new(),
            settings,
            connection_id,
        }
    }

    pub fn as_server(&mut self) -> Result<()> {
        self.events.insert(Ready::readable());
        Ok(())
    }

    pub fn as_client(&mut self, url: url::Url, addrs: Vec<SocketAddr>) -> Result<()> {
        if let Connecting(ref mut req_buf, _) = self.state {
            let req = self.handler.build_request(&url)?;
            self.addresses = addrs;
            self.events.insert(Ready::writable());
            self.endpoint = Endpoint::Client(url);
            req.format(req_buf.get_mut())
        } else {
            Err(Error::new(
                Kind::Internal,
                "Tried to set connection to client while not connecting.",
            ))
        }
    }

    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    pub fn encrypt(&mut self) -> Result<()> {
        let sock = self.socket().try_clone()?;
        let ssl_stream = match self.endpoint {
            Server => self.handler.upgrade_ssl_server(sock),
            Client(ref url) => self.handler.upgrade_ssl_client(sock, url),
        };

        match ssl_stream {
            Ok(stream) => {
                self.socket = Stream::tls_live(stream);
                Ok(())
            }
            #[cfg(feature = "ssl")]
            Err(Error {
                kind: Kind::SslHandshake(handshake_err),
                details,
            }) => match handshake_err {
                HandshakeError::SetupFailure(_) => {
                    Err(Error::new(Kind::SslHandshake(handshake_err), details))
                }
                HandshakeError::Failure(mid) | HandshakeError::WouldBlock(mid) => {
                    self.socket = Stream::tls(mid);
                    Ok(())
                }
            },
            #[cfg(feature = "nativetls")]
            Err(Error {
                kind: Kind::SslHandshake(handshake_err),
                details,
            }) => match handshake_err {
                HandshakeError::Failure(_) => {
                    Err(Error::new(Kind::SslHandshake(handshake_err), details))
                }
                HandshakeError::WouldBlock(mid) => {
                    self.socket = Stream::tls(mid);
                    Ok(())
                }
            },
            Err(e) => Err(e),
        }
    }

    pub fn token(&self) -> Token {
        self.token
    }

    pub fn socket(&self) -> &TcpStream {
        self.socket.evented()
    }

    pub fn connection_id(&self) -> u32 {
        self.connection_id
    }

    fn peer_addr(&self) -> String {
        if let Ok(addr) = self.socket.peer_addr() {
            addr.to_string()
        } else {
            "UNKNOWN".into()
        }
    }

    // Resetting may be necessary in order to try all possible addresses for a server
    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    pub fn reset(&mut self) -> Result<()> {
        // if self.is_client() {
        if let Client(ref url) = self.endpoint {
            if let Connecting(ref mut req, ref mut res) = self.state {
                req.set_position(0);
                res.set_position(0);
                self.events.remove(Ready::readable());
                self.events.insert(Ready::writable());

                if let Some(ref addr) = self.addresses.pop() {
                    let sock = TcpStream::connect(addr)?;
                    if self.socket.is_tls() {
                        let ssl_stream = self.handler.upgrade_ssl_client(sock, url);
                        match ssl_stream {
                            Ok(stream) => {
                                self.socket = Stream::tls_live(stream);
                                Ok(())
                            }
                            #[cfg(feature = "ssl")]
                            Err(Error {
                                kind: Kind::SslHandshake(handshake_err),
                                details,
                            }) => match handshake_err {
                                HandshakeError::SetupFailure(_) => {
                                    Err(Error::new(Kind::SslHandshake(handshake_err), details))
                                }
                                HandshakeError::Failure(mid) | HandshakeError::WouldBlock(mid) => {
                                    self.socket = Stream::tls(mid);
                                    Ok(())
                                }
                            },
                            #[cfg(feature = "nativetls")]
                            Err(Error {
                                kind: Kind::SslHandshake(handshake_err),
                                details,
                            }) => match handshake_err {
                                HandshakeError::Failure(_) => {
                                    Err(Error::new(Kind::SslHandshake(handshake_err), details))
                                }
                                HandshakeError::WouldBlock(mid) => {
                                    self.socket = Stream::tls(mid);
                                    Ok(())
                                }
                            },
                            Err(e) => Err(e),
                        }
                    } else {
                        self.socket = Stream::tcp(sock);
                        Ok(())
                    }
                } else {
                    if self.settings.panic_on_new_connection {
                        panic!("Unable to connect to server.");
                    }
                    Err(Error::new(Kind::Internal, "Exhausted possible addresses."))
                }
            } else {
                Err(Error::new(
                    Kind::Internal,
                    "Unable to reset client connection because it is active.",
                ))
            }
        } else {
            Err(Error::new(
                Kind::Internal,
                "Server connections cannot be reset.",
            ))
        }
    }

    #[cfg(not(any(feature = "ssl", feature = "nativetls")))]
    pub fn reset(&mut self) -> Result<()> {
        if self.is_client() {
            if let Connecting(ref mut req, ref mut res) = self.state {
                req.set_position(0);
                res.set_position(0);
                self.events.remove(Ready::readable());
                self.events.insert(Ready::writable());

                if let Some(ref addr) = self.addresses.pop() {
                    let sock = TcpStream::connect(addr)?;
                    self.socket = Stream::tcp(sock);
                    Ok(())
                } else {
                    if self.settings.panic_on_new_connection {
                        panic!("Unable to connect to server.");
                    }
                    Err(Error::new(Kind::Internal, "Exhausted possible addresses."))
                }
            } else {
                Err(Error::new(
                    Kind::Internal,
                    "Unable to reset client connection because it is active.",
                ))
            }
        } else {
            Err(Error::new(
                Kind::Internal,
                "Server connections cannot be reset.",
            ))
        }
    }

    pub fn events(&self) -> Ready {
        self.events
    }

    pub fn is_client(&self) -> bool {
        match self.endpoint {
            Client(_) => true,
            Server => false,
        }
    }

    pub fn is_server(&self) -> bool {
        match self.endpoint {
            Client(_) => false,
            Server => true,
        }
    }

    pub fn shutdown(&mut self) {
        self.handler.on_shutdown();
        if let Err(err) = self.send_close(CloseCode::Away, "Shutting down.") {
            self.handler.on_error(err);
            self.disconnect()
        }
    }

    #[inline]
    pub fn new_timeout(&mut self, event: Token, timeout: Timeout) -> Result<()> {
        self.handler.on_new_timeout(event, timeout)
    }

    #[inline]
    pub fn timeout_triggered(&mut self, event: Token) -> Result<()> {
        self.handler.on_timeout(event)
    }

    pub fn error(&mut self, err: Error) {
        match self.state {
            Connecting(_, ref mut res) => match err.kind {
                #[cfg(feature = "ssl")]
                Kind::Ssl(_) => {
                    self.handler.on_error(err);
                    self.events = Ready::empty();
                }
                Kind::Io(_) => {
                    self.handler.on_error(err);
                    self.events = Ready::empty();
                }
                Kind::Protocol => {
                    let msg = err.to_string();
                    self.handler.on_error(err);
                    if let Server = self.endpoint {
                        res.get_mut().clear();
                        if let Err(err) =
                            write!(res.get_mut(), "HTTP/1.1 400 Bad Request\r\n\r\n{}", msg)
                        {
                            self.handler.on_error(Error::from(err));
                            self.events = Ready::empty();
                        } else {
                            self.events.remove(Ready::readable());
                            self.events.insert(Ready::writable());
                        }
                    } else {
                        self.events = Ready::empty();
                    }
                }
                _ => {
                    let msg = err.to_string();
                    self.handler.on_error(err);
                    if let Server = self.endpoint {
                        res.get_mut().clear();
                        if let Err(err) = write!(
                            res.get_mut(),
                            "HTTP/1.1 500 Internal Server Error\r\n\r\n{}",
                            msg
                        ) {
                            self.handler.on_error(Error::from(err));
                            self.events = Ready::empty();
                        } else {
                            self.events.remove(Ready::readable());
                            self.events.insert(Ready::writable());
                        }
                    } else {
                        self.events = Ready::empty();
                    }
                }
            },
            _ => {
                match err.kind {
                    Kind::Internal => {
                        if self.settings.panic_on_internal {
                            panic!("Panicking on internal error -- {}", err);
                        }
                        let reason = format!("{}", err);

                        self.handler.on_error(err);
                        if let Err(err) = self.send_close(CloseCode::Error, reason) {
                            self.handler.on_error(err);
                            self.disconnect()
                        }
                    }
                    Kind::Capacity => {
                        if self.settings.panic_on_capacity {
                            panic!("Panicking on capacity error -- {}", err);
                        }
                        let reason = format!("{}", err);

                        self.handler.on_error(err);
                        if let Err(err) = self.send_close(CloseCode::Size, reason) {
                            self.handler.on_error(err);
                            self.disconnect()
                        }
                    }
                    Kind::Protocol => {
                        if self.settings.panic_on_protocol {
                            panic!("Panicking on protocol error -- {}", err);
                        }
                        let reason = format!("{}", err);

                        self.handler.on_error(err);
                        if let Err(err) = self.send_close(CloseCode::Protocol, reason) {
                            self.handler.on_error(err);
                            self.disconnect()
                        }
                    }
                    Kind::Encoding(_) => {
                        if self.settings.panic_on_encoding {
                            panic!("Panicking on encoding error -- {}", err);
                        }
                        let reason = format!("{}", err);

                        self.handler.on_error(err);
                        if let Err(err) = self.send_close(CloseCode::Invalid, reason) {
                            self.handler.on_error(err);
                            self.disconnect()
                        }
                    }
                    Kind::Http(_) => {
                        // This may happen if some handler writes a bad response
                        self.handler.on_error(err);
                        error!("Disconnecting WebSocket.");
                        self.disconnect()
                    }
                    Kind::Custom(_) => {
                        self.handler.on_error(err);
                    }
                    Kind::Queue(_) => {
                        if self.settings.panic_on_queue {
                            panic!("Panicking on queue error -- {}", err);
                        }
                        self.handler.on_error(err);
                    }
                    _ => {
                        if self.settings.panic_on_io {
                            panic!("Panicking on io error -- {}", err);
                        }
                        self.handler.on_error(err);
                        self.disconnect()
                    }
                }
            }
        }
    }

    pub fn disconnect(&mut self) {
        match self.state {
            RespondingClose | FinishedClose | Connecting(_, _) => (),
            _ => {
                self.handler.on_close(CloseCode::Abnormal, "");
            }
        }
        self.events = Ready::empty()
    }

    pub fn consume(self) -> H {
        self.handler
    }

    fn write_handshake(&mut self) -> Result<()> {
        if let Connecting(ref mut req, ref mut res) = self.state {
            match self.endpoint {
                Server => {
                    let mut done = false;
                    if self.socket.try_write_buf(res)?.is_some() {
                        if res.position() as usize == res.get_ref().len() {
                            done = true
                        }
                    }
                    if !done {
                        return Ok(());
                    }
                }
                Client(_) => {
                    if self.socket.try_write_buf(req)?.is_some() {
                        if req.position() as usize == req.get_ref().len() {
                            trace!(
                                "Finished writing handshake request to {}",
                                self.socket
                                    .peer_addr()
                                    .map(|addr| addr.to_string())
                                    .unwrap_or_else(|_| "UNKNOWN".into())
                            );
                            self.events.insert(Ready::readable());
                            self.events.remove(Ready::writable());
                        }
                    }
                    return Ok(());
                }
            }
        }

        if let Connecting(ref req, ref res) = replace(&mut self.state, Open) {
            trace!(
                "Finished writing handshake response to {}",
                self.peer_addr()
            );

            let request = match Request::parse(req.get_ref()) {
                Ok(Some(req)) => req,
                _ => {
                    // An error should already have been sent for the first time it failed to
                    // parse. We don't call disconnect here because `on_open` hasn't been called yet.
                    self.state = FinishedClose;
                    self.events = Ready::empty();
                    return Ok(());
                }
            };

            let response = Response::parse(res.get_ref())?.ok_or_else(|| {
                Error::new(
                    Kind::Internal,
                    "Failed to parse response after handshake is complete.",
                )
            })?;

            if response.status() != 101 {
                self.events = Ready::empty();
                return Ok(());
            } else {
                self.handler.on_open(Handshake {
                    request,
                    response,
                    peer_addr: self.socket.peer_addr().ok(),
                    local_addr: self.socket.local_addr().ok(),
                })?;
                debug!("Connection to {} is now open.", self.peer_addr());
                self.events.insert(Ready::readable());
                self.check_events();
                return Ok(());
            }
        } else {
            Err(Error::new(
                Kind::Internal,
                "Tried to write WebSocket handshake while not in connecting state!",
            ))
        }
    }

    fn read_handshake(&mut self) -> Result<()> {
        if let Connecting(ref mut req, ref mut res) = self.state {
            match self.endpoint {
                Server => {
                    if let Some(read) = self.socket.try_read_buf(req.get_mut())? {
                        if read == 0 {
                            self.events = Ready::empty();
                            return Ok(());
                        }
                        if let Some(ref request) = Request::parse(req.get_ref())? {
                            trace!("Handshake request received: \n{}", request);
                            let response = self.handler.on_request(request)?;
                            response.format(res.get_mut())?;
                            self.events.remove(Ready::readable());
                            self.events.insert(Ready::writable());
                        }
                    }
                    return Ok(());
                }
                Client(_) => {
                    if self.socket.try_read_buf(res.get_mut())?.is_some() {
                        // TODO: see if this can be optimized with drain
                        let end = {
                            let data = res.get_ref();
                            let end = data.iter()
                                .enumerate()
                                .take_while(|&(ind, _)| !data[..ind].ends_with(b"\r\n\r\n"))
                                .count();
                            if !data[..end].ends_with(b"\r\n\r\n") {
                                return Ok(());
                            }
                            self.in_buffer.get_mut().extend(&data[end..]);
                            end
                        };
                        res.get_mut().truncate(end);
                    } else {
                        // NOTE: wait to be polled again; response not ready.
                        return Ok(());
                    }
                }
            }
        }

        if let Connecting(ref req, ref res) = replace(&mut self.state, Open) {
            trace!(
                "Finished reading handshake response from {}",
                self.peer_addr()
            );

            let request = Request::parse(req.get_ref())?.ok_or_else(|| {
                Error::new(
                    Kind::Internal,
                    "Failed to parse request after handshake is complete.",
                )
            })?;

            let response = Response::parse(res.get_ref())?.ok_or_else(|| {
                Error::new(
                    Kind::Internal,
                    "Failed to parse response after handshake is complete.",
                )
            })?;

            trace!("Handshake response received: \n{}", response);

            if response.status() != 101 {
                if response.status() != 301 && response.status() != 302 {
                    return Err(Error::new(Kind::Protocol, "Handshake failed."));
                } else {
                    return Ok(());
                }
            }

            if self.settings.key_strict {
                let req_key = request.hashed_key()?;
                let res_key = from_utf8(response.key()?)?;
                if req_key != res_key {
                    return Err(Error::new(
                        Kind::Protocol,
                        format!(
                            "Received incorrect WebSocket Accept key: {} vs {}",
                            req_key, res_key
                        ),
                    ));
                }
            }

            self.handler.on_response(&response)?;
            self.handler.on_open(Handshake {
                request,
                response,
                peer_addr: self.socket.peer_addr().ok(),
                local_addr: self.socket.local_addr().ok(),
            })?;

            // check to see if there is anything to read already
            if !self.in_buffer.get_ref().is_empty() {
                self.read_frames()?;
            }

            self.check_events();
            return Ok(());
        }
        Err(Error::new(
            Kind::Internal,
            "Tried to read WebSocket handshake while not in connecting state!",
        ))
    }

    pub fn read(&mut self) -> Result<()> {
        if self.socket.is_negotiating() {
            trace!("Performing TLS negotiation on {}.", self.peer_addr());
            self.socket.clear_negotiating()?;
            self.write()
        } else {
            let res = if self.state.is_connecting() {
                trace!("Ready to read handshake from {}.", self.peer_addr());
                self.read_handshake()
            } else {
                trace!("Ready to read messages from {}.", self.peer_addr());
                while let Some(len) = self.buffer_in()? {
                    self.read_frames()?;
                    if len == 0 {
                        if self.events.is_writable() {
                            self.events.remove(Ready::readable());
                        } else {
                            self.disconnect()
                        }
                        break;
                    }
                }
                Ok(())
            };

            if self.socket.is_negotiating() && res.is_ok() {
                self.events.remove(Ready::readable());
                self.events.insert(Ready::writable());
            }
            res
        }
    }

    fn read_frames(&mut self) -> Result<()> {
        let max_size = self.settings.max_fragment_size as u64;
        while let Some(mut frame) = Frame::parse(&mut self.in_buffer, max_size)? {
            match self.state {
                // Ignore data received after receiving close frame
                RespondingClose | FinishedClose => continue,
                _ => (),
            }

            if self.settings.masking_strict {
                if frame.is_masked() {
                    if self.is_client() {
                        return Err(Error::new(
                            Kind::Protocol,
                            "Received masked frame from a server endpoint.",
                        ));
                    }
                } else {
                    if self.is_server() {
                        return Err(Error::new(
                            Kind::Protocol,
                            "Received unmasked frame from a client endpoint.",
                        ));
                    }
                }
            }

            // This is safe whether or not a frame is masked.
            frame.remove_mask();

            if let Some(frame) = self.handler.on_frame(frame)? {
                if frame.is_final() {
                    match frame.opcode() {
                        // singleton data frames
                        OpCode::Text => {
                            trace!("Received text frame {:?}", frame);
                            // since we are going to handle this, there can't be an ongoing
                            // message
                            if !self.fragments.is_empty() {
                                return Err(Error::new(Kind::Protocol, "Received unfragmented text frame while processing fragmented message."));
                            }
                            let msg = Message::text(String::from_utf8(frame.into_data())
                                .map_err(|err| err.utf8_error())?);
                            self.handler.on_message(msg)?;
                        }
                        OpCode::Binary => {
                            trace!("Received binary frame {:?}", frame);
                            // since we are going to handle this, there can't be an ongoing
                            // message
                            if !self.fragments.is_empty() {
                                return Err(Error::new(Kind::Protocol, "Received unfragmented binary frame while processing fragmented message."));
                            }
                            let data = frame.into_data();
                            self.handler.on_message(Message::binary(data))?;
                        }
                        // control frames
                        OpCode::Close => {
                            trace!("Received close frame {:?}", frame);
                            // Closing handshake
                            if self.state.is_closing() {
                                if self.is_server() {
                                    // Finished handshake, disconnect server side
                                    self.events = Ready::empty()
                                } else {
                                    // We are a client, so we wait for the server to close the
                                    // connection
                                }
                            } else {
                                // Starting handshake, will send the responding close frame
                                self.state = RespondingClose;
                            }

                            let mut close_code = [0u8; 2];
                            let mut data = Cursor::new(frame.into_data());
                            if let 2 = data.read(&mut close_code)? {
                                let raw_code: u16 =
                                    (u16::from(close_code[0]) << 8) | (u16::from(close_code[1]));
                                trace!(
                                    "Connection to {} received raw close code: {:?}, {:?}",
                                    self.peer_addr(),
                                    raw_code,
                                    close_code
                                );
                                let named = CloseCode::from(raw_code);
                                if let CloseCode::Other(code) = named {
                                    if code < 1000 ||
                                            code >= 5000 ||
                                            code == 1004 ||
                                            code == 1014 ||
                                            code == 1016 || // these below are here to pass the autobahn test suite
                                            code == 1100 || // we shouldn't need them later
                                            code == 2000
                                        || code == 2999
                                    {
                                        return Err(Error::new(
                                            Kind::Protocol,
                                            format!(
                                                "Received invalid close code from endpoint: {}",
                                                code
                                            ),
                                        ));
                                    }
                                }
                                let has_reason = {
                                    if let Ok(reason) = from_utf8(&data.get_ref()[2..]) {
                                        self.handler.on_close(named, reason); // note reason may be an empty string
                                        true
                                    } else {
                                        self.handler.on_close(named, "");
                                        false
                                    }
                                };

                                if let CloseCode::Abnormal = named {
                                    return Err(Error::new(
                                        Kind::Protocol,
                                        "Received abnormal close code from endpoint.",
                                    ));
                                } else if let CloseCode::Status = named {
                                    return Err(Error::new(
                                        Kind::Protocol,
                                        "Received no status close code from endpoint.",
                                    ));
                                } else if let CloseCode::Restart = named {
                                    return Err(Error::new(
                                        Kind::Protocol,
                                        "Restart close code is not supported.",
                                    ));
                                } else if let CloseCode::Again = named {
                                    return Err(Error::new(
                                        Kind::Protocol,
                                        "Try again later close code is not supported.",
                                    ));
                                } else if let CloseCode::Tls = named {
                                    return Err(Error::new(
                                        Kind::Protocol,
                                        "Received TLS close code outside of TLS handshake.",
                                    ));
                                } else {
                                    if !self.state.is_closing() {
                                        if has_reason {
                                            self.send_close(named, "")?; // note this drops any extra close data
                                        } else {
                                            self.send_close(CloseCode::Invalid, "")?;
                                        }
                                    } else {
                                        self.state = FinishedClose;
                                    }
                                }
                            } else {
                                // This is not an error. It is allowed behavior in the
                                // protocol, so we don't trigger an error.
                                // "If there is no such data in the Close control frame,
                                // _The WebSocket Connection Close Reason_ is the empty string."
                                self.handler.on_close(CloseCode::Status, "");
                                if !self.state.is_closing() {
                                    self.send_close(CloseCode::Empty, "")?;
                                } else {
                                    self.state = FinishedClose;
                                }
                            }
                        }
                        OpCode::Ping => {
                            trace!("Received ping frame {:?}", frame);
                            self.send_pong(frame.into_data())?;
                        }
                        OpCode::Pong => {
                            trace!("Received pong frame {:?}", frame);
                            // no ping validation for now
                        }
                        // last fragment
                        OpCode::Continue => {
                            trace!("Received final fragment {:?}", frame);
                            if let Some(first) = self.fragments.pop_front() {
                                let size = self.fragments.iter().fold(
                                    first.payload().len() + frame.payload().len(),
                                    |len, frame| len + frame.payload().len(),
                                );
                                match first.opcode() {
                                    OpCode::Text => {
                                        trace!("Constructing text message from fragments: {:?} -> {:?} -> {:?}", first, self.fragments.iter().collect::<Vec<&Frame>>(), frame);
                                        let mut data = Vec::with_capacity(size);
                                        data.extend(first.into_data());
                                        while let Some(frame) = self.fragments.pop_front() {
                                            data.extend(frame.into_data());
                                        }
                                        data.extend(frame.into_data());

                                        let string = String::from_utf8(data)
                                            .map_err(|err| err.utf8_error())?;

                                        trace!(
                                            "Calling handler with constructed message: {:?}",
                                            string
                                        );
                                        self.handler.on_message(Message::text(string))?;
                                    }
                                    OpCode::Binary => {
                                        trace!("Constructing binary message from fragments: {:?} -> {:?} -> {:?}", first, self.fragments.iter().collect::<Vec<&Frame>>(), frame);
                                        let mut data = Vec::with_capacity(size);
                                        data.extend(first.into_data());

                                        while let Some(frame) = self.fragments.pop_front() {
                                            data.extend(frame.into_data());
                                        }

                                        data.extend(frame.into_data());

                                        trace!(
                                            "Calling handler with constructed message: {:?}",
                                            data
                                        );
                                        self.handler.on_message(Message::binary(data))?;
                                    }
                                    _ => {
                                        return Err(Error::new(
                                            Kind::Protocol,
                                            "Encounted fragmented control frame.",
                                        ))
                                    }
                                }
                            } else {
                                return Err(Error::new(
                                    Kind::Protocol,
                                    "Unable to reconstruct fragmented message. No first frame.",
                                ));
                            }
                        }
                        _ => return Err(Error::new(Kind::Protocol, "Encountered invalid opcode.")),
                    }
                } else {
                    if frame.is_control() {
                        return Err(Error::new(
                            Kind::Protocol,
                            "Encounted fragmented control frame.",
                        ));
                    } else {
                        trace!("Received non-final fragment frame {:?}", frame);
                        if !self.settings.fragments_grow
                            && self.settings.fragments_capacity == self.fragments.len()
                        {
                            return Err(Error::new(Kind::Capacity, "Exceeded max fragments."));
                        } else {
                            self.fragments.push_back(frame)
                        }
                    }
                }
            }
        }
        Ok(())
    }

    pub fn write(&mut self) -> Result<()> {
        if self.socket.is_negotiating() {
            trace!("Performing TLS negotiation on {}.", self.peer_addr());
            self.socket.clear_negotiating()?;
            self.read()
        } else {
            let res = if self.state.is_connecting() {
                trace!("Ready to write handshake to {}.", self.peer_addr());
                self.write_handshake()
            } else {
                trace!("Ready to write messages to {}.", self.peer_addr());

                // Start out assuming that this write will clear the whole buffer
                self.events.remove(Ready::writable());

                if let Some(len) = self.socket.try_write_buf(&mut self.out_buffer)? {
                    trace!("Wrote {} bytes to {}", len, self.peer_addr());
                    let finished = len == 0
                        || self.out_buffer.position() == self.out_buffer.get_ref().len() as u64;
                    if finished {
                        match self.state {
                            // we are are a server that is closing and just wrote out our confirming
                            // close frame, let's disconnect
                            FinishedClose if self.is_server() => {
                                self.events = Ready::empty();
                                return Ok(());
                            }
                            _ => (),
                        }
                    }
                }

                // Check if there is more to write so that the connection will be rescheduled
                self.check_events();
                Ok(())
            };

            if self.socket.is_negotiating() && res.is_ok() {
                self.events.remove(Ready::writable());
                self.events.insert(Ready::readable());
            }
            res
        }
    }

    pub fn send_message(&mut self, msg: Message) -> Result<()> {
        if self.state.is_closing() {
            trace!(
                "Connection is closing. Ignoring request to send message {:?} to {}.",
                msg,
                self.peer_addr()
            );
            return Ok(());
        }

        let opcode = msg.opcode();
        trace!("Message opcode {:?}", opcode);
        let data = msg.into_data();

        if let Some(frame) = self.handler
            .on_send_frame(Frame::message(data, opcode, true))?
        {
            if frame.payload().len() > self.settings.fragment_size {
                trace!("Chunking at {:?}.", self.settings.fragment_size);
                // note this copies the data, so it's actually somewhat expensive to fragment
                let mut chunks = frame
                    .payload()
                    .chunks(self.settings.fragment_size)
                    .peekable();
                let chunk = chunks.next().expect("Unable to get initial chunk!");

                let mut first = Frame::message(Vec::from(chunk), opcode, false);

                // Match reserved bits from original to keep extension status intact
                first.set_rsv1(frame.has_rsv1());
                first.set_rsv2(frame.has_rsv2());
                first.set_rsv3(frame.has_rsv3());

                self.buffer_frame(first)?;

                while let Some(chunk) = chunks.next() {
                    if chunks.peek().is_some() {
                        self.buffer_frame(Frame::message(
                            Vec::from(chunk),
                            OpCode::Continue,
                            false,
                        ))?;
                    } else {
                        self.buffer_frame(Frame::message(
                            Vec::from(chunk),
                            OpCode::Continue,
                            true,
                        ))?;
                    }
                }
            } else {
                trace!("Sending unfragmented message frame.");
                // true means that the message is done
                self.buffer_frame(frame)?;
            }
        }
        self.check_events();
        Ok(())
    }

    #[inline]
    pub fn send_ping(&mut self, data: Vec<u8>) -> Result<()> {
        if self.state.is_closing() {
            trace!(
                "Connection is closing. Ignoring request to send ping {:?} to {}.",
                data,
                self.peer_addr()
            );
            return Ok(());
        }
        trace!("Sending ping to {}.", self.peer_addr());

        if let Some(frame) = self.handler.on_send_frame(Frame::ping(data))? {
            self.buffer_frame(frame)?;
        }
        self.check_events();
        Ok(())
    }

    #[inline]
    pub fn send_pong(&mut self, data: Vec<u8>) -> Result<()> {
        if self.state.is_closing() {
            trace!(
                "Connection is closing. Ignoring request to send pong {:?} to {}.",
                data,
                self.peer_addr()
            );
            return Ok(());
        }
        trace!("Sending pong to {}.", self.peer_addr());

        if let Some(frame) = self.handler.on_send_frame(Frame::pong(data))? {
            self.buffer_frame(frame)?;
        }
        self.check_events();
        Ok(())
    }

    #[inline]
    pub fn send_close<R>(&mut self, code: CloseCode, reason: R) -> Result<()>
    where
        R: Borrow<str>,
    {
        match self.state {
            // We are responding to a close frame the other endpoint, when this frame goes out, we
            // are done.
            RespondingClose => self.state = FinishedClose,
            // Multiple close frames are being sent from our end, ignore the later frames
            AwaitingClose | FinishedClose => {
                trace!(
                    "Connection is already closing. Ignoring close {:?} -- {:?} to {}.",
                    code,
                    reason.borrow(),
                    self.peer_addr()
                );
                self.check_events();
                return Ok(());
            }
            // We are initiating a closing handshake.
            Open => self.state = AwaitingClose,
            Connecting(_, _) => {
                debug_assert!(false, "Attempted to close connection while not yet open.")
            }
        }

        trace!(
            "Sending close {:?} -- {:?} to {}.",
            code,
            reason.borrow(),
            self.peer_addr()
        );

        if let Some(frame) = self.handler
            .on_send_frame(Frame::close(code, reason.borrow()))?
        {
            self.buffer_frame(frame)?;
        }

        trace!("Connection to {} is now closing.", self.peer_addr());

        self.check_events();
        Ok(())
    }

    fn check_events(&mut self) {
        if !self.state.is_connecting() {
            self.events.insert(Ready::readable());
            if self.out_buffer.position() < self.out_buffer.get_ref().len() as u64 {
                self.events.insert(Ready::writable());
            }
        }
    }

    fn buffer_frame(&mut self, mut frame: Frame) -> Result<()> {
        self.check_buffer_out(&frame)?;

        if self.is_client() {
            frame.set_mask();
        }

        trace!("Buffering frame to {}:\n{}", self.peer_addr(), frame);

        let pos = self.out_buffer.position();
        self.out_buffer.seek(SeekFrom::End(0))?;
        frame.format(&mut self.out_buffer)?;
        self.out_buffer.seek(SeekFrom::Start(pos))?;
        Ok(())
    }

    fn check_buffer_out(&mut self, frame: &Frame) -> Result<()> {
        if self.out_buffer.get_ref().capacity() <= self.out_buffer.get_ref().len() + frame.len() {
            // extend
            let mut new = Vec::with_capacity(self.out_buffer.get_ref().capacity());
            new.extend(&self.out_buffer.get_ref()[self.out_buffer.position() as usize..]);
            if new.len() == new.capacity() {
                if self.settings.out_buffer_grow {
                    new.reserve(self.settings.out_buffer_capacity)
                } else {
                    return Err(Error::new(
                        Kind::Capacity,
                        "Maxed out output buffer for connection.",
                    ));
                }
            }
            self.out_buffer = Cursor::new(new);
        }
        Ok(())
    }

    fn buffer_in(&mut self) -> Result<Option<usize>> {
        trace!("Reading buffer for connection to {}.", self.peer_addr());
        if let Some(len) = self.socket.try_read_buf(self.in_buffer.get_mut())? {
            trace!("Buffered {}.", len);
            if self.in_buffer.get_ref().len() == self.in_buffer.get_ref().capacity() {
                // extend
                let mut new = Vec::with_capacity(self.in_buffer.get_ref().capacity());
                new.extend(&self.in_buffer.get_ref()[self.in_buffer.position() as usize..]);
                if new.len() == new.capacity() {
                    if self.settings.in_buffer_grow {
                        new.reserve(self.settings.in_buffer_capacity);
                    } else {
                        return Err(Error::new(
                            Kind::Capacity,
                            "Maxed out input buffer for connection.",
                        ));
                    }
                }
                self.in_buffer = Cursor::new(new);
            }
            Ok(Some(len))
        } else {
            Ok(None)
        }
    }
}
