//! Client handshake machine.

use std::borrow::Cow;
use std::io::{Read, Write};
use std::marker::PhantomData;

use base64;
use httparse::Status;
use httparse;
use rand;
use url::Url;

use error::{Error, Result};
use protocol::{WebSocket, WebSocketConfig, Role};
use super::headers::{Headers, FromHttparse, MAX_HEADERS};
use super::machine::{HandshakeMachine, StageResult, TryParse};
use super::{MidHandshake, HandshakeRole, ProcessingResult, convert_key};

/// Client request.
#[derive(Debug)]
pub struct Request<'t> {
    /// `ws://` or `wss://` URL to connect to.
    pub url: Url,
    /// Extra HTTP headers to append to the request.
    pub extra_headers: Option<Vec<(Cow<'t, str>, Cow<'t, str>)>>,
}

impl<'t> Request<'t> {
    /// Returns the GET part of the request.
    fn get_path(&self) -> String {
        if let Some(query) = self.url.query() {
            format!("{path}?{query}", path = self.url.path(), query = query)
        } else {
            self.url.path().into()
        }
    }

    /// Returns the host part of the request.
    fn get_host(&self) -> String {
        let host = self.url.host_str().expect("Bug: URL without host");
        if let Some(port) = self.url.port() {
            format!("{host}:{port}", host = host, port = port)
        } else {
            host.into()
        }
    }

    /// Adds a WebSocket protocol to the request.
    pub fn add_protocol(&mut self, protocol: Cow<'t, str>) {
        self.add_header(Cow::from("Sec-WebSocket-Protocol"), protocol);
    }

    /// Adds a custom header to the request.
    pub fn add_header(&mut self, name: Cow<'t, str>, value: Cow<'t, str>) {
        let mut headers = self.extra_headers.take().unwrap_or_else(Vec::new);
        headers.push((name, value));
        self.extra_headers = Some(headers);
    }
}

impl From<Url> for Request<'static> {
    fn from(value: Url) -> Self {
        Request {
            url: value,
            extra_headers: None,
        }
    }
}

/// Client handshake role.
#[derive(Debug)]
pub struct ClientHandshake<S> {
    verify_data: VerifyData,
    config: Option<WebSocketConfig>,
    _marker: PhantomData<S>,
}

impl<S: Read + Write> ClientHandshake<S> {
    /// Initiate a client handshake.
    pub fn start(
        stream: S,
        request: Request,
        config: Option<WebSocketConfig>
    ) -> MidHandshake<Self> {
        let key = generate_key();

        let machine = {
            let mut req = Vec::new();
            write!(req, "\
                GET {path} HTTP/1.1\r\n\
                Host: {host}\r\n\
                Connection: upgrade\r\n\
                Upgrade: websocket\r\n\
                Sec-WebSocket-Version: 13\r\n\
                Sec-WebSocket-Key: {key}\r\n",
                host = request.get_host(), path = request.get_path(), key = key).unwrap();
            if let Some(eh) = request.extra_headers {
                for (k, v) in eh {
                    write!(req, "{}: {}\r\n", k, v).unwrap();
                }
            }
            write!(req, "\r\n").unwrap();
            HandshakeMachine::start_write(stream, req)
        };

        let client = {
            let accept_key = convert_key(key.as_ref()).unwrap();
            ClientHandshake {
                verify_data: VerifyData { accept_key },
                config,
                _marker: PhantomData,
            }
        };

        trace!("Client handshake initiated.");
        MidHandshake { role: client, machine }
    }
}

impl<S: Read + Write> HandshakeRole for ClientHandshake<S> {
    type IncomingData = Response;
    type InternalStream = S;
    type FinalResult = (WebSocket<S>, Response);
    fn stage_finished(&mut self, finish: StageResult<Self::IncomingData, Self::InternalStream>)
        -> Result<ProcessingResult<Self::InternalStream, Self::FinalResult>>
    {
        Ok(match finish {
            StageResult::DoneWriting(stream) => {
                ProcessingResult::Continue(HandshakeMachine::start_read(stream))
            }
            StageResult::DoneReading { stream, result, tail, } => {
                self.verify_data.verify_response(&result)?;
                debug!("Client handshake done.");
                let websocket = WebSocket::from_partially_read(
                    stream,
                    tail,
                    Role::Client,
                    self.config.clone(),
                );
                ProcessingResult::Done((websocket, result))
            }
        })
    }
}

/// Information for handshake verification.
#[derive(Debug)]
struct VerifyData {
    /// Accepted server key.
    accept_key: String,
}

impl VerifyData {
    pub fn verify_response(&self, response: &Response) -> Result<()> {
        // 1. If the status code received from the server is not 101, the
        // client handles the response per HTTP [RFC2616] procedures. (RFC 6455)
        if response.code != 101 {
            return Err(Error::Http(response.code));
        }
        // 2. If the response lacks an |Upgrade| header field or the |Upgrade|
        // header field contains a value that is not an ASCII case-
        // insensitive match for the value "websocket", the client MUST
        // _Fail the WebSocket Connection_. (RFC 6455)
        if !response.headers.header_is_ignore_case("Upgrade", "websocket") {
            return Err(Error::Protocol("No \"Upgrade: websocket\" in server reply".into()));
        }
        // 3.  If the response lacks a |Connection| header field or the
        // |Connection| header field doesn't contain a token that is an
        // ASCII case-insensitive match for the value "Upgrade", the client
        // MUST _Fail the WebSocket Connection_. (RFC 6455)
        if !response.headers.header_is_ignore_case("Connection", "Upgrade") {
            return Err(Error::Protocol("No \"Connection: upgrade\" in server reply".into()));
        }
        // 4.  If the response lacks a |Sec-WebSocket-Accept| header field or
        // the |Sec-WebSocket-Accept| contains a value other than the
        // base64-encoded SHA-1 of ... the client MUST _Fail the WebSocket
        // Connection_. (RFC 6455)
        if !response.headers.header_is("Sec-WebSocket-Accept", &self.accept_key) {
            return Err(Error::Protocol("Key mismatch in Sec-WebSocket-Accept".into()));
        }
        // 5.  If the response includes a |Sec-WebSocket-Extensions| header
        // field and this header field indicates the use of an extension
        // that was not present in the client's handshake (the server has
        // indicated an extension not requested by the client), the client
        // MUST _Fail the WebSocket Connection_. (RFC 6455)
        // TODO

        // 6.  If the response includes a |Sec-WebSocket-Protocol| header field
        // and this header field indicates the use of a subprotocol that was
        // not present in the client's handshake (the server has indicated a
        // subprotocol not requested by the client), the client MUST _Fail
        // the WebSocket Connection_. (RFC 6455)
        // TODO

        Ok(())
    }
}

/// Server response.
#[derive(Debug)]
pub struct Response {
    /// HTTP response code of the response.
    pub code: u16,
    /// Received headers.
    pub headers: Headers,
}

impl TryParse for Response {
    fn try_parse(buf: &[u8]) -> Result<Option<(usize, Self)>> {
        let mut hbuffer = [httparse::EMPTY_HEADER; MAX_HEADERS];
        let mut req = httparse::Response::new(&mut hbuffer);
        Ok(match req.parse(buf)? {
            Status::Partial => None,
            Status::Complete(size) => Some((size, Response::from_httparse(req)?)),
        })
    }
}

impl<'h, 'b: 'h> FromHttparse<httparse::Response<'h, 'b>> for Response {
    fn from_httparse(raw: httparse::Response<'h, 'b>) -> Result<Self> {
        if raw.version.expect("Bug: no HTTP version") < /*1.*/1 {
            return Err(Error::Protocol("HTTP version should be 1.1 or higher".into()));
        }
        Ok(Response {
            code: raw.code.expect("Bug: no HTTP response code"),
            headers: Headers::from_httparse(raw.headers)?,
        })
    }
}

/// Generate a random key for the `Sec-WebSocket-Key` header.
fn generate_key() -> String {
    // a base64-encoded (see Section 4 of [RFC4648]) value that,
    // when decoded, is 16 bytes in length (RFC 6455)
    let r: [u8; 16] = rand::random();
    base64::encode(&r)
}

#[cfg(test)]
mod tests {
    use super::{Response, generate_key};
    use super::super::machine::TryParse;

    #[test]
    fn random_keys() {
        let k1 = generate_key();
        println!("Generated random key 1: {}", k1);
        let k2 = generate_key();
        println!("Generated random key 2: {}", k2);
        assert_ne!(k1, k2);
        assert_eq!(k1.len(), k2.len());
        assert_eq!(k1.len(), 24);
        assert_eq!(k2.len(), 24);
        assert!(k1.ends_with("=="));
        assert!(k2.ends_with("=="));
        assert!(k1[..22].find("=").is_none());
        assert!(k2[..22].find("=").is_none());
    }

    #[test]
    fn response_parsing() {
        const DATA: &'static [u8] = b"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        let (_, resp) = Response::try_parse(DATA).unwrap().unwrap();
        assert_eq!(resp.code, 200);
        assert_eq!(resp.headers.find_first("Content-Type"), Some(&b"text/html"[..]));
    }
}
