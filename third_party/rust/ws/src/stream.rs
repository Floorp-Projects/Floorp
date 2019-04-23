use std::io;
use std::io::ErrorKind::WouldBlock;
use std::net::SocketAddr;
#[cfg(feature="ssl")]
use std::mem::replace;

use mio::tcp::TcpStream;
#[cfg(feature="ssl")]
use openssl::ssl::{SslStream, MidHandshakeSslStream, HandshakeError};
#[cfg(feature="ssl")]
use openssl::ssl::Error as SslError;
use bytes::{Buf, BufMut};

use result::{Result, Error, Kind};

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
        where Self : Sized
    {
        // Reads the length of the slice supplied by buf.mut_bytes into the buffer
        // This is not guaranteed to consume an entire datagram or segment.
        // If your protocol is msg based (instead of continuous stream) you should
        // ensure that your buffer is large enough to hold an entire segment (1532 bytes if not jumbo
        // frames)
        let res = map_non_block(self.read(unsafe { buf.bytes_mut() }));

        if let Ok(Some(cnt)) = res {
            unsafe { buf.advance_mut(cnt); }
        }

        res
    }
}

pub trait TryWriteBuf: io::Write {
    fn try_write_buf<B: Buf>(&mut self, buf: &mut B) -> io::Result<Option<usize>>
        where Self : Sized
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
    #[cfg(feature="ssl")]
    Tls(TlsStream),
}

impl Stream {

    pub fn tcp(stream: TcpStream) -> Stream {
        Tcp(stream)
    }

    #[cfg(feature="ssl")]
    pub fn tls(stream: MidHandshakeSslStream<TcpStream>) -> Stream {
        Tls(TlsStream::Handshake {sock: stream, negotiating: false })
    }

    #[cfg(feature="ssl")]
    pub fn tls_live(stream: SslStream<TcpStream>) -> Stream {
        Tls(TlsStream::Live(stream))
    }

    #[cfg(feature="ssl")]
    pub fn is_tls(&self) -> bool {
        match *self {
            Tcp(_) => false,
            Tls(_) => true,
        }
    }

    pub fn evented(&self) -> &TcpStream {
        match *self {
            Tcp(ref sock) => sock,
            #[cfg(feature="ssl")]
            Tls(ref inner) => inner.evented(),
        }
    }

    pub fn is_negotiating(&self) -> bool {
        match *self {
            Tcp(_) => false,
            #[cfg(feature="ssl")]
            Tls(ref inner) => inner.is_negotiating(),
        }
    }

    pub fn clear_negotiating(&mut self) -> Result<()> {
        match *self {
            Tcp(_) => Err(Error::new(Kind::Internal, "Attempted to clear negotiating flag on non ssl connection.")),
            #[cfg(feature="ssl")]
            Tls(ref mut inner) => inner.clear_negotiating(),
        }
    }

    pub fn peer_addr(&self) -> io::Result<SocketAddr> {
        match *self {
            Tcp(ref sock) => sock.peer_addr(),
            #[cfg(feature="ssl")]
            Tls(ref inner) => inner.peer_addr(),
        }
    }

    pub fn local_addr(&self) -> io::Result<SocketAddr> {
        match *self {
            Tcp(ref sock) => sock.local_addr(),
            #[cfg(feature="ssl")]
            Tls(ref inner) => inner.local_addr(),
        }
    }
}

impl io::Read for Stream {

    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match *self {
            Tcp(ref mut sock) => sock.read(buf),
            #[cfg(feature="ssl")]
            Tls(TlsStream::Live(ref mut sock)) => sock.read(buf),
            #[cfg(feature="ssl")]
            Tls(ref mut tls_stream) => {
                trace!("Attempting to read ssl handshake.");
                match replace(tls_stream, TlsStream::Upgrading) {
                    TlsStream::Live(_) | TlsStream::Upgrading => unreachable!(),
                    TlsStream::Handshake {sock, mut negotiating } => {
                        match sock.handshake() {
                            Ok(mut sock) => {
                                trace!("Completed SSL Handshake");
                                let res = sock.read(buf);
                                *tls_stream = TlsStream::Live(sock);
                                res
                            }
                            Err(HandshakeError::SetupFailure(err)) =>
                                Err(io::Error::new(io::ErrorKind::Other, err)),
                            Err(HandshakeError::Failure(mid)) |
                            Err(HandshakeError::Interrupted(mid)) => {
                                let err = match *mid.error() {
                                    SslError::WantWrite(_) => {
                                        negotiating = true;
                                        Err(io::Error::new(
                                            io::ErrorKind::WouldBlock,
                                            "SSL wants writing"))
                                    },
                                    SslError::WantRead(_) => Err(io::Error::new(
                                        io::ErrorKind::WouldBlock,
                                        "SSL wants reading")),
                                    SslError::Stream(ref e) =>
                                        Err(From::from(e.kind())),
                                    ref err =>
                                        Err(io::Error::new(io::ErrorKind::Other, format!("{}", err))),
                                };
                                *tls_stream = TlsStream::Handshake { sock: mid, negotiating: negotiating };
                                err
                            }
                        }
                    }
                }
            }
        }
    }
}

impl io::Write for Stream {

    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        match *self {
            Tcp(ref mut sock) => sock.write(buf),
            #[cfg(feature="ssl")]
            Tls(TlsStream::Live(ref mut sock)) => sock.write(buf),
            #[cfg(feature="ssl")]
            Tls(ref mut tls_stream) => {
                trace!("Attempting to write ssl handshake.");
                match replace(tls_stream, TlsStream::Upgrading) {
                    TlsStream::Live(_) | TlsStream::Upgrading => unreachable!(),
                    TlsStream::Handshake {sock, mut negotiating } => {

                        negotiating = false;

                        match sock.handshake() {
                            Ok(mut sock) => {
                                trace!("Completed SSL Handshake");
                                let res = sock.write(buf);
                                *tls_stream = TlsStream::Live(sock);
                                res
                            }
                            Err(HandshakeError::SetupFailure(err)) =>
                                Err(io::Error::new(io::ErrorKind::Other, err)),
                            Err(HandshakeError::Failure(mid)) |
                            Err(HandshakeError::Interrupted(mid)) => {
                                let err = match *mid.error() {
                                    SslError::WantRead(_) => {
                                        negotiating = true;
                                        Err(io::Error::new(
                                                io::ErrorKind::WouldBlock,
                                                "SSL wants reading"))
                                    },
                                    SslError::WantWrite(_)=> Err(io::Error::new(
                                        io::ErrorKind::WouldBlock,
                                        "SSL wants writing")),
                                    SslError::Stream(ref e) =>
                                        Err(From::from(e.kind())),
                                    ref err =>
                                        Err(io::Error::new(io::ErrorKind::Other, format!("{}", err))),
                                };
                                *tls_stream = TlsStream::Handshake { sock: mid, negotiating: negotiating };
                                err
                            }
                        }
                    }
                }
            }
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        match *self {
            Tcp(ref mut sock) => sock.flush(),
            #[cfg(feature="ssl")]
            Tls(TlsStream::Live(ref mut sock)) => sock.flush(),
            #[cfg(feature="ssl")]
            Tls(TlsStream::Handshake { ref mut sock, .. }) => sock.get_mut().flush(),
            #[cfg(feature="ssl")]
            Tls(TlsStream::Upgrading) => panic!("Tried to access actively upgrading TlsStream"),
        }
    }
}

#[cfg(feature="ssl")]
pub enum TlsStream {
    Live(SslStream<TcpStream>),
    Handshake {
        sock: MidHandshakeSslStream<TcpStream>,
        negotiating: bool,
    },
    Upgrading,
}

#[cfg(feature="ssl")]
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
            TlsStream::Handshake { sock: _, negotiating } => negotiating,
            TlsStream::Upgrading => panic!("Tried to access actively upgrading TlsStream"),
        }

    }

    pub fn clear_negotiating(&mut self) -> Result<()> {
        match *self {
            TlsStream::Live(_) => Err(
                Error::new(Kind::Internal, "Attempted to clear negotiating flag on live ssl connection.")),
            TlsStream::Handshake { sock: _, ref mut negotiating } => Ok(*negotiating = false),
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
