//! Methods to connect to an WebSocket as a client.

use std::net::{TcpStream, SocketAddr, ToSocketAddrs};
use std::result::Result as StdResult;
use std::io::{Read, Write};

use url::Url;

use handshake::client::Response;
use protocol::WebSocketConfig;

#[cfg(feature="tls")]
mod encryption {
    use std::net::TcpStream;
    use native_tls::{TlsConnector, HandshakeError as TlsHandshakeError};
    pub use native_tls::TlsStream;

    pub use stream::Stream as StreamSwitcher;
    /// TCP stream switcher (plain/TLS).
    pub type AutoStream = StreamSwitcher<TcpStream, TlsStream<TcpStream>>;

    use stream::Mode;
    use error::Result;

    pub fn wrap_stream(stream: TcpStream, domain: &str, mode: Mode) -> Result<AutoStream> {
        match mode {
            Mode::Plain => Ok(StreamSwitcher::Plain(stream)),
            Mode::Tls => {
                let connector = TlsConnector::builder().build()?;
                connector.connect(domain, stream)
                    .map_err(|e| match e {
                        TlsHandshakeError::Failure(f) => f.into(),
                        TlsHandshakeError::WouldBlock(_) => panic!("Bug: TLS handshake not blocked"),
                    })
                    .map(StreamSwitcher::Tls)
            }
        }
    }
}

#[cfg(not(feature="tls"))]
mod encryption {
    use std::net::TcpStream;

    use stream::Mode;
    use error::{Error, Result};

    /// TLS support is nod compiled in, this is just standard `TcpStream`.
    pub type AutoStream = TcpStream;

    pub fn wrap_stream(stream: TcpStream, _domain: &str, mode: Mode) -> Result<AutoStream> {
        match mode {
            Mode::Plain => Ok(stream),
            Mode::Tls => Err(Error::Url("TLS support not compiled in.".into())),
        }
    }
}

pub use self::encryption::AutoStream;
use self::encryption::wrap_stream;

use protocol::WebSocket;
use handshake::HandshakeError;
use handshake::client::{ClientHandshake, Request};
use stream::{NoDelay, Mode};
use error::{Error, Result};


/// Connect to the given WebSocket in blocking mode.
///
/// Uses a websocket configuration passed as an argument to the function. Calling it with `None` is
/// equal to calling `connect()` function.
///
/// The URL may be either ws:// or wss://.
/// To support wss:// URLs, feature "tls" must be turned on.
///
/// This function "just works" for those who wants a simple blocking solution
/// similar to `std::net::TcpStream`. If you want a non-blocking or other
/// custom stream, call `client` instead.
///
/// This function uses `native_tls` to do TLS. If you want to use other TLS libraries,
/// use `client` instead. There is no need to enable the "tls" feature if you don't call
/// `connect` since it's the only function that uses native_tls.
pub fn connect_with_config<'t, Req: Into<Request<'t>>>(
    request: Req,
    config: Option<WebSocketConfig>
) -> Result<(WebSocket<AutoStream>, Response)> {
    let request: Request = request.into();
    let mode = url_mode(&request.url)?;
    let addrs = request.url.to_socket_addrs()?;
    let mut stream = connect_to_some(addrs, &request.url, mode)?;
    NoDelay::set_nodelay(&mut stream, true)?;
    client_with_config(request, stream, config)
        .map_err(|e| match e {
            HandshakeError::Failure(f) => f,
            HandshakeError::Interrupted(_) => panic!("Bug: blocking handshake not blocked"),
        })
}

/// Connect to the given WebSocket in blocking mode.
///
/// The URL may be either ws:// or wss://.
/// To support wss:// URLs, feature "tls" must be turned on.
///
/// This function "just works" for those who wants a simple blocking solution
/// similar to `std::net::TcpStream`. If you want a non-blocking or other
/// custom stream, call `client` instead.
///
/// This function uses `native_tls` to do TLS. If you want to use other TLS libraries,
/// use `client` instead. There is no need to enable the "tls" feature if you don't call
/// `connect` since it's the only function that uses native_tls.
pub fn connect<'t, Req: Into<Request<'t>>>(request: Req)
    -> Result<(WebSocket<AutoStream>, Response)>
{
    connect_with_config(request, None)
}

fn connect_to_some<A>(addrs: A, url: &Url, mode: Mode) -> Result<AutoStream>
    where A: Iterator<Item=SocketAddr>
{
    let domain = url.host_str().ok_or_else(|| Error::Url("No host name in the URL".into()))?;
    for addr in addrs {
        debug!("Trying to contact {} at {}...", url, addr);
        if let Ok(raw_stream) = TcpStream::connect(addr) {
            if let Ok(stream) = wrap_stream(raw_stream, domain, mode) {
                return Ok(stream)
            }
        }
    }
    Err(Error::Url(format!("Unable to connect to {}", url).into()))
}

/// Get the mode of the given URL.
///
/// This function may be used to ease the creation of custom TLS streams
/// in non-blocking algorithmss or for use with TLS libraries other than `native_tls`.
pub fn url_mode(url: &Url) -> Result<Mode> {
    match url.scheme() {
        "ws" => Ok(Mode::Plain),
        "wss" => Ok(Mode::Tls),
        _ => Err(Error::Url("URL scheme not supported".into()))
    }
}

/// Do the client handshake over the given stream given a web socket configuration. Passing `None`
/// as configuration is equal to calling `client()` function.
///
/// Use this function if you need a nonblocking handshake support or if you
/// want to use a custom stream like `mio::tcp::TcpStream` or `openssl::ssl::SslStream`.
/// Any stream supporting `Read + Write` will do.
pub fn client_with_config<'t, Stream, Req>(
    request: Req,
    stream: Stream,
    config: Option<WebSocketConfig>,
) -> StdResult<(WebSocket<Stream>, Response), HandshakeError<ClientHandshake<Stream>>>
where
    Stream: Read + Write,
    Req: Into<Request<'t>>,
{
    ClientHandshake::start(stream, request.into(), config).handshake()
}

/// Do the client handshake over the given stream.
///
/// Use this function if you need a nonblocking handshake support or if you
/// want to use a custom stream like `mio::tcp::TcpStream` or `openssl::ssl::SslStream`.
/// Any stream supporting `Read + Write` will do.
pub fn client<'t, Stream, Req>(request: Req, stream: Stream)
    -> StdResult<(WebSocket<Stream>, Response), HandshakeError<ClientHandshake<Stream>>>
where
    Stream: Read + Write,
    Req: Into<Request<'t>>,
{
    client_with_config(request, stream, None)
}
