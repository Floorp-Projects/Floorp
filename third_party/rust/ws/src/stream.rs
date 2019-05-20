use std::io;
use std::io::ErrorKind::WouldBlock;
#[cfg(any(feature = "ssl", feature = "nativetls"))]
use std::mem::replace;
use std::net::SocketAddr;

use bytes::{Buf, BufMut};
use mio::tcp::TcpStream;
#[cfg(feature = "nativetls")]
use native_tls::{
    HandshakeError, MidHandshakeTlsStream as MidHandshakeSslStream, TlsStream as SslStream,
};
#[cfg(feature = "ssl")]
use openssl::ssl::{ErrorCode as SslErrorCode, HandshakeError, MidHandshakeSslStream, SslStream};

use result::{Error, Kind, Result};

fn map_non_block<T>(res: io::Result<T>) -> io::Result<Option<T>> {
    match res {
        Ok(value) => Ok(Some(value)),
        Err(err) => {
            if let WouldBlock = err.kind() {
                Ok(None)
            } else {
                Err(err)
            }
        }
    }
}

pub trait TryReadBuf: io::Read {
    fn try_read_buf<B: BufMut>(&mut self, buf: &mut B) -> io::Result<Option<usize>>
    where
        Self: Sized,
    {
        // Reads the length of the slice supplied by buf.mut_bytes into the buffer
        // This is not guaranteed to consume an entire datagram or segment.
        // If your protocol is msg based (instead of continuous stream) you should
        // ensure that your buffer is large enough to hold an entire segment (1532 bytes if not jumbo
        // frames)
        let res = map_non_block(self.read(unsafe { buf.bytes_mut() }));

        if let Ok(Some(cnt)) = res {
            unsafe {
                buf.advance_mut(cnt);
            }
        }

        res
    }
}

pub trait TryWriteBuf: io::Write {
    fn try_write_buf<B: Buf>(&mut self, buf: &mut B) -> io::Result<Option<usize>>
    where
        Self: Sized,
    {
        let res = map_non_block(self.write(buf.bytes()));

        if let Ok(Some(cnt)) = res {
            buf.advance(cnt);
        }

        res
    }
}

impl<T: io::Read> TryReadBuf for T {}
impl<T: io::Write> TryWriteBuf for T {}

use self::Stream::*;
pub enum Stream {
    Tcp(TcpStream),
    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    Tls(TlsStream),
}

impl Stream {
    pub fn tcp(stream: TcpStream) -> Stream {
        Tcp(stream)
    }

    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    pub fn tls(stream: MidHandshakeSslStream<TcpStream>) -> Stream {
        Tls(TlsStream::Handshake {
            sock: stream,
            negotiating: false,
        })
    }

    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    pub fn tls_live(stream: SslStream<TcpStream>) -> Stream {
        Tls(TlsStream::Live(stream))
    }

    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    pub fn is_tls(&self) -> bool {
        match *self {
            Tcp(_) => false,
            Tls(_) => true,
        }
    }

    pub fn evented(&self) -> &TcpStream {
        match *self {
            Tcp(ref sock) => sock,
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(ref inner) => inner.evented(),
        }
    }

    pub fn is_negotiating(&self) -> bool {
        match *self {
            Tcp(_) => false,
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(ref inner) => inner.is_negotiating(),
        }
    }

    pub fn clear_negotiating(&mut self) -> Result<()> {
        match *self {
            Tcp(_) => Err(Error::new(
                Kind::Internal,
                "Attempted to clear negotiating flag on non ssl connection.",
            )),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(ref mut inner) => inner.clear_negotiating(),
        }
    }

    pub fn peer_addr(&self) -> io::Result<SocketAddr> {
        match *self {
            Tcp(ref sock) => sock.peer_addr(),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(ref inner) => inner.peer_addr(),
        }
    }

    pub fn local_addr(&self) -> io::Result<SocketAddr> {
        match *self {
            Tcp(ref sock) => sock.local_addr(),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(ref inner) => inner.local_addr(),
        }
    }
}

impl io::Read for Stream {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match *self {
            Tcp(ref mut sock) => sock.read(buf),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(TlsStream::Live(ref mut sock)) => sock.read(buf),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(ref mut tls_stream) => {
                trace!("Attempting to read ssl handshake.");
                match replace(tls_stream, TlsStream::Upgrading) {
                    TlsStream::Live(_) | TlsStream::Upgrading => unreachable!(),
                    TlsStream::Handshake {
                        sock,
                        mut negotiating,
                    } => match sock.handshake() {
                        Ok(mut sock) => {
                            trace!("Completed SSL Handshake");
                            let res = sock.read(buf);
                            *tls_stream = TlsStream::Live(sock);
                            res
                        }
                        #[cfg(feature = "ssl")]
                        Err(HandshakeError::SetupFailure(err)) => {
                            Err(io::Error::new(io::ErrorKind::Other, err))
                        }
                        #[cfg(feature = "ssl")]
                        Err(HandshakeError::Failure(mid))
                        | Err(HandshakeError::WouldBlock(mid)) => {
                            if mid.error().code() == SslErrorCode::WANT_READ {
                                negotiating = true;
                            }
                            let err = if let Some(io_error) = mid.error().io_error() {
                                Err(io::Error::new(
                                    io_error.kind(),
                                    format!("{:?}", io_error.get_ref()),
                                ))
                            } else {
                                Err(io::Error::new(
                                    io::ErrorKind::Other,
                                    format!("{}", mid.error()),
                                ))
                            };
                            *tls_stream = TlsStream::Handshake {
                                sock: mid,
                                negotiating,
                            };
                            err
                        }
                        #[cfg(feature = "nativetls")]
                        Err(HandshakeError::WouldBlock(mid)) => {
                            negotiating = true;
                            *tls_stream = TlsStream::Handshake {
                                sock: mid,
                                negotiating: negotiating,
                            };
                            Err(io::Error::new(io::ErrorKind::WouldBlock, "SSL would block"))
                        }
                        #[cfg(feature = "nativetls")]
                        Err(HandshakeError::Failure(err)) => {
                            Err(io::Error::new(io::ErrorKind::Other, format!("{}", err)))
                        }
                    },
                }
            }
        }
    }
}

impl io::Write for Stream {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        match *self {
            Tcp(ref mut sock) => sock.write(buf),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(TlsStream::Live(ref mut sock)) => sock.write(buf),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(ref mut tls_stream) => {
                trace!("Attempting to write ssl handshake.");
                match replace(tls_stream, TlsStream::Upgrading) {
                    TlsStream::Live(_) | TlsStream::Upgrading => unreachable!(),
                    TlsStream::Handshake {
                        sock,
                        mut negotiating,
                    } => match sock.handshake() {
                        Ok(mut sock) => {
                            trace!("Completed SSL Handshake");
                            let res = sock.write(buf);
                            *tls_stream = TlsStream::Live(sock);
                            res
                        }
                        #[cfg(feature = "ssl")]
                        Err(HandshakeError::SetupFailure(err)) => {
                            Err(io::Error::new(io::ErrorKind::Other, err))
                        }
                        #[cfg(feature = "ssl")]
                        Err(HandshakeError::Failure(mid))
                        | Err(HandshakeError::WouldBlock(mid)) => {
                            if mid.error().code() == SslErrorCode::WANT_READ {
                                negotiating = true;
                            } else {
                                negotiating = false;
                            }
                            let err = if let Some(io_error) = mid.error().io_error() {
                                Err(io::Error::new(
                                    io_error.kind(),
                                    format!("{:?}", io_error.get_ref()),
                                ))
                            } else {
                                Err(io::Error::new(
                                    io::ErrorKind::Other,
                                    format!("{}", mid.error()),
                                ))
                            };
                            *tls_stream = TlsStream::Handshake {
                                sock: mid,
                                negotiating,
                            };
                            err
                        }
                        #[cfg(feature = "nativetls")]
                        Err(HandshakeError::WouldBlock(mid)) => {
                            negotiating = true;
                            *tls_stream = TlsStream::Handshake {
                                sock: mid,
                                negotiating: negotiating,
                            };
                            Err(io::Error::new(io::ErrorKind::WouldBlock, "SSL would block"))
                        }
                        #[cfg(feature = "nativetls")]
                        Err(HandshakeError::Failure(err)) => {
                            Err(io::Error::new(io::ErrorKind::Other, format!("{}", err)))
                        }
                    },
                }
            }
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        match *self {
            Tcp(ref mut sock) => sock.flush(),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(TlsStream::Live(ref mut sock)) => sock.flush(),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(TlsStream::Handshake { ref mut sock, .. }) => sock.get_mut().flush(),
            #[cfg(any(feature = "ssl", feature = "nativetls"))]
            Tls(TlsStream::Upgrading) => panic!("Tried to access actively upgrading TlsStream"),
        }
    }
}

#[cfg(any(feature = "ssl", feature = "nativetls"))]
pub enum TlsStream {
    Live(SslStream<TcpStream>),
    Handshake {
        sock: MidHandshakeSslStream<TcpStream>,
        negotiating: bool,
    },
    Upgrading,
}

#[cfg(any(feature = "ssl", feature = "nativetls"))]
impl TlsStream {
    pub fn evented(&self) -> &TcpStream {
        match *self {
            TlsStream::Live(ref sock) => sock.get_ref(),
            TlsStream::Handshake { ref sock, .. } => sock.get_ref(),
            TlsStream::Upgrading => panic!("Tried to access actively upgrading TlsStream"),
        }
    }

    pub fn is_negotiating(&self) -> bool {
        match *self {
            TlsStream::Live(_) => false,
            TlsStream::Handshake {
                sock: _,
                negotiating,
            } => negotiating,
            TlsStream::Upgrading => panic!("Tried to access actively upgrading TlsStream"),
        }
    }

    pub fn clear_negotiating(&mut self) -> Result<()> {
        match *self {
            TlsStream::Live(_) => Err(Error::new(
                Kind::Internal,
                "Attempted to clear negotiating flag on live ssl connection.",
            )),
            TlsStream::Handshake {
                sock: _,
                ref mut negotiating,
            } => Ok(*negotiating = false),
            TlsStream::Upgrading => panic!("Tried to access actively upgrading TlsStream"),
        }
    }

    pub fn peer_addr(&self) -> io::Result<SocketAddr> {
        match *self {
            TlsStream::Live(ref sock) => sock.get_ref().peer_addr(),
            TlsStream::Handshake { ref sock, .. } => sock.get_ref().peer_addr(),
            TlsStream::Upgrading => panic!("Tried to access actively upgrading TlsStream"),
        }
    }

    pub fn local_addr(&self) -> io::Result<SocketAddr> {
        match *self {
            TlsStream::Live(ref sock) => sock.get_ref().local_addr(),
            TlsStream::Handshake { ref sock, .. } => sock.get_ref().local_addr(),
            TlsStream::Upgrading => panic!("Tried to access actively upgrading TlsStream"),
        }
    }
}
