use std::fmt;
use std::io::Write;
use std::net::SocketAddr;
use std::str::from_utf8;

use httparse;
use rand;
use sha1::{self, Digest};
use url;

use result::{Error, Kind, Result};

static WS_GUID: &'static str = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static BASE64: &'static [u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const MAX_HEADERS: usize = 124;

fn generate_key() -> String {
    let key: [u8; 16] = rand::random();
    encode_base64(&key)
}

pub fn hash_key(key: &[u8]) -> String {
    let mut hasher = sha1::Sha1::new();

    hasher.input(key);
    hasher.input(WS_GUID.as_bytes());

    encode_base64(&hasher.result())
}

// This code is based on rustc_serialize base64 STANDARD
fn encode_base64(data: &[u8]) -> String {
    let len = data.len();
    let mod_len = len % 3;

    let mut encoded = vec![b'='; (len + 2) / 3 * 4];
    {
        let mut in_iter = data[..len - mod_len].iter().map(|&c| u32::from(c));
        let mut out_iter = encoded.iter_mut();

        let enc = |val| BASE64[val as usize];
        let mut write = |val| *out_iter.next().unwrap() = val;

        while let (Some(one), Some(two), Some(three)) =
            (in_iter.next(), in_iter.next(), in_iter.next())
        {
            let g24 = one << 16 | two << 8 | three;
            write(enc((g24 >> 18) & 63));
            write(enc((g24 >> 12) & 63));
            write(enc((g24 >> 6) & 63));
            write(enc(g24 & 63));
        }

        match mod_len {
            1 => {
                let pad = (u32::from(data[len - 1])) << 16;
                write(enc((pad >> 18) & 63));
                write(enc((pad >> 12) & 63));
            }
            2 => {
                let pad = (u32::from(data[len - 2])) << 16 | (u32::from(data[len - 1])) << 8;
                write(enc((pad >> 18) & 63));
                write(enc((pad >> 12) & 63));
                write(enc((pad >> 6) & 63));
            }
            _ => (),
        }
    }

    String::from_utf8(encoded).unwrap()
}

/// A struct representing the two halves of the WebSocket handshake.
#[derive(Debug)]
pub struct Handshake {
    /// The HTTP request sent to begin the handshake.
    pub request: Request,
    /// The HTTP response from the server confirming the handshake.
    pub response: Response,
    /// The socket address of the other endpoint. This address may
    /// be an intermediary such as a proxy server.
    pub peer_addr: Option<SocketAddr>,
    /// The socket address of this endpoint.
    pub local_addr: Option<SocketAddr>,
}

impl Handshake {
    /// Get the IP address of the remote connection.
    ///
    /// This is the preferred method of obtaining the client's IP address.
    /// It will attempt to retrieve the most likely IP address based on request
    /// headers, falling back to the address of the peer.
    ///
    /// # Note
    /// This assumes that the peer is a client. If you are implementing a
    /// WebSocket client and want to obtain the address of the server, use
    /// `Handshake::peer_addr` instead.
    ///
    /// This method does not ensure that the address is a valid IP address.
    #[allow(dead_code)]
    pub fn remote_addr(&self) -> Result<Option<String>> {
        Ok(self.request.client_addr()?.map(String::from).or_else(|| {
            if let Some(addr) = self.peer_addr {
                Some(addr.ip().to_string())
            } else {
                None
            }
        }))
    }
}

/// The handshake request.
#[derive(Debug)]
pub struct Request {
    path: String,
    method: String,
    headers: Vec<(String, Vec<u8>)>,
}

impl Request {
    /// Get the value of the first instance of an HTTP header.
    pub fn header(&self, header: &str) -> Option<&Vec<u8>> {
        self.headers
            .iter()
            .find(|&&(ref key, _)| key.to_lowercase() == header.to_lowercase())
            .map(|&(_, ref val)| val)
    }

    /// Edit the value of the first instance of an HTTP header.
    pub fn header_mut(&mut self, header: &str) -> Option<&mut Vec<u8>> {
        self.headers
            .iter_mut()
            .find(|&&mut (ref key, _)| key.to_lowercase() == header.to_lowercase())
            .map(|&mut (_, ref mut val)| val)
    }

    /// Access the request headers.
    #[allow(dead_code)]
    #[inline]
    pub fn headers(&self) -> &Vec<(String, Vec<u8>)> {
        &self.headers
    }

    /// Edit the request headers.
    #[allow(dead_code)]
    #[inline]
    pub fn headers_mut(&mut self) -> &mut Vec<(String, Vec<u8>)> {
        &mut self.headers
    }

    /// Get the origin of the request if it comes from a browser.
    #[allow(dead_code)]
    pub fn origin(&self) -> Result<Option<&str>> {
        if let Some(origin) = self.header("origin") {
            Ok(Some(from_utf8(origin)?))
        } else {
            Ok(None)
        }
    }

    /// Get the unhashed WebSocket key sent in the request.
    pub fn key(&self) -> Result<&Vec<u8>> {
        self.header("sec-websocket-key")
            .ok_or_else(|| Error::new(Kind::Protocol, "Unable to parse WebSocket key."))
    }

    /// Get the hashed WebSocket key from this request.
    pub fn hashed_key(&self) -> Result<String> {
        Ok(hash_key(self.key()?))
    }

    /// Get the WebSocket protocol version from the request (should be 13).
    #[allow(dead_code)]
    pub fn version(&self) -> Result<&str> {
        if let Some(version) = self.header("sec-websocket-version") {
            from_utf8(version).map_err(Error::from)
        } else {
            Err(Error::new(
                Kind::Protocol,
                "The Sec-WebSocket-Version header is missing.",
            ))
        }
    }

    /// Get the request method.
    #[inline]
    pub fn method(&self) -> &str {
        &self.method
    }

    /// Get the path of the request.
    #[allow(dead_code)]
    #[inline]
    pub fn resource(&self) -> &str {
        &self.path
    }

    /// Get the possible protocols for the WebSocket connection.
    #[allow(dead_code)]
    pub fn protocols(&self) -> Result<Vec<&str>> {
        if let Some(protos) = self.header("sec-websocket-protocol") {
            Ok(from_utf8(protos)?
                .split(',')
                .map(|proto| proto.trim())
                .collect())
        } else {
            Ok(Vec::new())
        }
    }

    /// Add a possible protocol to this request.
    /// This may result in duplicate protocols listed.
    #[allow(dead_code)]
    pub fn add_protocol(&mut self, protocol: &str) {
        if let Some(protos) = self.header_mut("sec-websocket-protocol") {
            protos.push(b","[0]);
            protos.extend(protocol.as_bytes());
            return;
        }
        self.headers_mut()
            .push(("Sec-WebSocket-Protocol".into(), protocol.into()))
    }

    /// Remove a possible protocol from this request.
    #[allow(dead_code)]
    pub fn remove_protocol(&mut self, protocol: &str) {
        if let Some(protos) = self.header_mut("sec-websocket-protocol") {
            let mut new_protos = Vec::with_capacity(protos.len());

            if let Ok(protos_str) = from_utf8(protos) {
                new_protos = protos_str
                    .split(',')
                    .filter(|proto| proto.trim() == protocol)
                    .collect::<Vec<&str>>()
                    .join(",")
                    .into();
            }
            if new_protos.len() < protos.len() {
                *protos = new_protos
            }
        }
    }

    /// Get the possible extensions for the WebSocket connection.
    #[allow(dead_code)]
    pub fn extensions(&self) -> Result<Vec<&str>> {
        if let Some(exts) = self.header("sec-websocket-extensions") {
            Ok(from_utf8(exts)?.split(',').map(|ext| ext.trim()).collect())
        } else {
            Ok(Vec::new())
        }
    }

    /// Add a possible extension to this request.
    /// This may result in duplicate extensions listed. Also, the order of extensions
    /// indicates preference, so if the preference matters, consider using the
    /// `Sec-WebSocket-Protocol` header directly.
    #[allow(dead_code)]
    pub fn add_extension(&mut self, ext: &str) {
        if let Some(exts) = self.header_mut("sec-websocket-extensions") {
            exts.push(b","[0]);
            exts.extend(ext.as_bytes());
            return;
        }
        self.headers_mut()
            .push(("Sec-WebSocket-Extensions".into(), ext.into()))
    }

    /// Remove a possible extension from this request.
    /// This will remove all configurations of the extension.
    #[allow(dead_code)]
    pub fn remove_extension(&mut self, ext: &str) {
        if let Some(exts) = self.header_mut("sec-websocket-extensions") {
            let mut new_exts = Vec::with_capacity(exts.len());

            if let Ok(exts_str) = from_utf8(exts) {
                new_exts = exts_str
                    .split(',')
                    .filter(|e| e.trim().starts_with(ext))
                    .collect::<Vec<&str>>()
                    .join(",")
                    .into();
            }
            if new_exts.len() < exts.len() {
                *exts = new_exts
            }
        }
    }

    /// Get the IP address of the client.
    ///
    /// This method will attempt to retrieve the most likely IP address of the requester
    /// in the following manner:
    ///
    /// If the `X-Forwarded-For` header exists, this method will return the left most
    /// address in the list.
    ///
    /// If the [Forwarded HTTP Header Field](https://tools.ietf.org/html/rfc7239) exits,
    /// this method will return the left most address indicated by the `for` parameter,
    /// if it exists.
    ///
    /// # Note
    /// This method does not ensure that the address is a valid IP address.
    #[allow(dead_code)]
    pub fn client_addr(&self) -> Result<Option<&str>> {
        if let Some(x_forward) = self.header("x-forwarded-for") {
            return Ok(from_utf8(x_forward)?.split(',').next());
        }

        // We only care about the first forwarded header, so header is ok
        if let Some(forward) = self.header("forwarded") {
            if let Some(_for) = from_utf8(forward)?
                .split(';')
                .find(|f| f.trim().starts_with("for"))
            {
                if let Some(_for_eq) = _for.trim().split(',').next() {
                    let mut it = _for_eq.split('=');
                    it.next();
                    return Ok(it.next());
                }
            }
        }
        Ok(None)
    }

    /// Attempt to parse an HTTP request from a buffer. If the buffer does not contain a complete
    /// request, this will return `Ok(None)`.
    pub fn parse(buf: &[u8]) -> Result<Option<Request>> {
        let mut headers = [httparse::EMPTY_HEADER; MAX_HEADERS];
        let mut req = httparse::Request::new(&mut headers);
        let parsed = req.parse(buf)?;
        if !parsed.is_partial() {
            Ok(Some(Request {
                path: req.path.unwrap().into(),
                method: req.method.unwrap().into(),
                headers: req.headers
                    .iter()
                    .map(|h| (h.name.into(), h.value.into()))
                    .collect(),
            }))
        } else {
            Ok(None)
        }
    }

    /// Construct a new WebSocket handshake HTTP request from a url.
    pub fn from_url(url: &url::Url) -> Result<Request> {
        let query = if let Some(q) = url.query() {
            format!("?{}", q)
        } else {
            "".into()
        };

        let mut headers = vec![
            ("Connection".into(), "Upgrade".into()),
            (
                "Host".into(),
                format!(
                    "{}:{}",
                    url.host_str().ok_or_else(|| Error::new(
                        Kind::Internal,
                        "No host passed for WebSocket connection.",
                    ))?,
                    url.port_or_known_default().unwrap_or(80)
                ).into(),
            ),
            ("Sec-WebSocket-Version".into(), "13".into()),
            ("Sec-WebSocket-Key".into(), generate_key().into()),
            ("Upgrade".into(), "websocket".into()),
        ];

        if url.password().is_some() || url.username() != "" {
            let basic = encode_base64(format!("{}:{}", url.username(), url.password().unwrap_or("")).as_bytes());
            headers.push(("Authorization".into(), format!("Basic {}", basic).into()))
        }

        let req = Request {
            path: format!("{}{}", url.path(), query),
            method: "GET".to_owned(),
            headers: headers,
        };

        debug!("Built request from URL:\n{}", req);

        Ok(req)
    }

    /// Write a request out to a buffer
    pub fn format<W>(&self, w: &mut W) -> Result<()>
    where
        W: Write,
    {
        write!(w, "{} {} HTTP/1.1\r\n", self.method, self.path)?;
        for &(ref key, ref val) in &self.headers {
            write!(w, "{}: ", key)?;
            w.write_all(val)?;
            write!(w, "\r\n")?;
        }
        write!(w, "\r\n")?;
        Ok(())
    }
}

impl fmt::Display for Request {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut s = Vec::with_capacity(2048);
        self.format(&mut s).map_err(|err| {
            error!("{:?}", err);
            fmt::Error
        })?;
        write!(
            f,
            "{}",
            from_utf8(&s).map_err(|err| {
                error!("Unable to format request as utf8: {:?}", err);
                fmt::Error
            })?
        )
    }
}

/// The handshake response.
#[derive(Debug)]
pub struct Response {
    status: u16,
    reason: String,
    headers: Vec<(String, Vec<u8>)>,
    body: Vec<u8>,
}

impl Response {
    // TODO: resolve the overlap with Request

    /// Construct a generic HTTP response with a body.
    pub fn new<R>(status: u16, reason: R, body: Vec<u8>) -> Response
    where
        R: Into<String>,
    {
        Response {
            status,
            reason: reason.into(),
            headers: vec![("Content-Length".into(), body.len().to_string().into())],
            body,
        }
    }

    /// Get the response body.
    #[inline]
    pub fn body(&self) -> &[u8] {
        &self.body
    }

    /// Get the value of the first instance of an HTTP header.
    fn header(&self, header: &str) -> Option<&Vec<u8>> {
        self.headers
            .iter()
            .find(|&&(ref key, _)| key.to_lowercase() == header.to_lowercase())
            .map(|&(_, ref val)| val)
    }
    /// Edit the value of the first instance of an HTTP header.
    pub fn header_mut(&mut self, header: &str) -> Option<&mut Vec<u8>> {
        self.headers
            .iter_mut()
            .find(|&&mut (ref key, _)| key.to_lowercase() == header.to_lowercase())
            .map(|&mut (_, ref mut val)| val)
    }

    /// Access the request headers.
    #[allow(dead_code)]
    #[inline]
    pub fn headers(&self) -> &Vec<(String, Vec<u8>)> {
        &self.headers
    }

    /// Edit the request headers.
    #[allow(dead_code)]
    #[inline]
    pub fn headers_mut(&mut self) -> &mut Vec<(String, Vec<u8>)> {
        &mut self.headers
    }

    /// Get the HTTP status code.
    #[allow(dead_code)]
    #[inline]
    pub fn status(&self) -> u16 {
        self.status
    }

    /// Set the HTTP status code.
    #[allow(dead_code)]
    #[inline]
    pub fn set_status(&mut self, status: u16) {
        self.status = status
    }

    /// Get the HTTP status reason.
    #[allow(dead_code)]
    #[inline]
    pub fn reason(&self) -> &str {
        &self.reason
    }

    /// Set the HTTP status reason.
    #[allow(dead_code)]
    #[inline]
    pub fn set_reason<R>(&mut self, reason: R)
    where
        R: Into<String>,
    {
        self.reason = reason.into()
    }

    /// Get the hashed WebSocket key.
    pub fn key(&self) -> Result<&Vec<u8>> {
        self.header("sec-websocket-accept")
            .ok_or_else(|| Error::new(Kind::Protocol, "Unable to parse WebSocket key."))
    }

    /// Get the protocol that the server has decided to use.
    #[allow(dead_code)]
    pub fn protocol(&self) -> Result<Option<&str>> {
        if let Some(proto) = self.header("sec-websocket-protocol") {
            Ok(Some(from_utf8(proto)?))
        } else {
            Ok(None)
        }
    }

    /// Set the protocol that the server has decided to use.
    #[allow(dead_code)]
    pub fn set_protocol(&mut self, protocol: &str) {
        if let Some(proto) = self.header_mut("sec-websocket-protocol") {
            *proto = protocol.into();
            return;
        }
        self.headers_mut()
            .push(("Sec-WebSocket-Protocol".into(), protocol.into()))
    }

    /// Get the extensions that the server has decided to use. If these are unacceptable, it is
    /// appropriate to send an Extension close code.
    #[allow(dead_code)]
    pub fn extensions(&self) -> Result<Vec<&str>> {
        if let Some(exts) = self.header("sec-websocket-extensions") {
            Ok(from_utf8(exts)?
                .split(',')
                .map(|proto| proto.trim())
                .collect())
        } else {
            Ok(Vec::new())
        }
    }

    /// Add an accepted extension to this response.
    /// This may result in duplicate extensions listed.
    #[allow(dead_code)]
    pub fn add_extension(&mut self, ext: &str) {
        if let Some(exts) = self.header_mut("sec-websocket-extensions") {
            exts.push(b","[0]);
            exts.extend(ext.as_bytes());
            return;
        }
        self.headers_mut()
            .push(("Sec-WebSocket-Extensions".into(), ext.into()))
    }

    /// Remove an accepted extension from this response.
    /// This will remove all configurations of the extension.
    #[allow(dead_code)]
    pub fn remove_extension(&mut self, ext: &str) {
        if let Some(exts) = self.header_mut("sec-websocket-extensions") {
            let mut new_exts = Vec::with_capacity(exts.len());

            if let Ok(exts_str) = from_utf8(exts) {
                new_exts = exts_str
                    .split(',')
                    .filter(|e| e.trim().starts_with(ext))
                    .collect::<Vec<&str>>()
                    .join(",")
                    .into();
            }
            if new_exts.len() < exts.len() {
                *exts = new_exts
            }
        }
    }

    /// Attempt to parse an HTTP response from a buffer. If the buffer does not contain a complete
    /// response, thiw will return `Ok(None)`.
    pub fn parse(buf: &[u8]) -> Result<Option<Response>> {
        let mut headers = [httparse::EMPTY_HEADER; MAX_HEADERS];
        let mut res = httparse::Response::new(&mut headers);

        let parsed = res.parse(buf)?;
        if !parsed.is_partial() {
            Ok(Some(Response {
                status: res.code.unwrap(),
                reason: res.reason.unwrap().into(),
                headers: res.headers
                    .iter()
                    .map(|h| (h.name.into(), h.value.into()))
                    .collect(),
                body: Vec::new(),
            }))
        } else {
            Ok(None)
        }
    }

    /// Construct a new WebSocket handshake HTTP response from a request.
    /// This will create a response that ignores protocols and extensions. Edit this response to
    /// accept a protocol and extensions as necessary.
    pub fn from_request(req: &Request) -> Result<Response> {
        let res = Response {
            status: 101,
            reason: "Switching Protocols".into(),
            headers: vec![
                ("Connection".into(), "Upgrade".into()),
                ("Sec-WebSocket-Accept".into(), req.hashed_key()?.into()),
                ("Upgrade".into(), "websocket".into()),
            ],
            body: Vec::new(),
        };

        debug!("Built response from request:\n{}", res);
        Ok(res)
    }

    /// Write a response out to a buffer
    pub fn format<W>(&self, w: &mut W) -> Result<()>
    where
        W: Write,
    {
        write!(w, "HTTP/1.1 {} {}\r\n", self.status, self.reason)?;
        for &(ref key, ref val) in &self.headers {
            write!(w, "{}: ", key)?;
            w.write_all(val)?;
            write!(w, "\r\n")?;
        }
        write!(w, "\r\n")?;
        w.write_all(&self.body)?;
        Ok(())
    }
}

impl fmt::Display for Response {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut s = Vec::with_capacity(2048);
        self.format(&mut s).map_err(|err| {
            error!("{:?}", err);
            fmt::Error
        })?;
        write!(
            f,
            "{}",
            from_utf8(&s).map_err(|err| {
                error!("Unable to format response as utf8: {:?}", err);
                fmt::Error
            })?
        )
    }
}

mod test {
    #![allow(unused_imports, unused_variables, dead_code)]
    use super::*;
    use std::io::Write;
    use std::net::SocketAddr;
    use std::str::FromStr;

    #[test]
    fn remote_addr() {
        let mut buf = Vec::with_capacity(2048);
        write!(
            &mut buf,
            "GET / HTTP/1.1\r\n\
             Connection: Upgrade\r\n\
             Upgrade: websocket\r\n\
             Sec-WebSocket-Version: 13\r\n\
             Sec-WebSocket-Key: q16eN37NCfVwUChPvBdk4g==\r\n\r\n"
        ).unwrap();

        let req = Request::parse(&buf).unwrap().unwrap();
        let res = Response::from_request(&req).unwrap();
        let shake = Handshake {
            request: req,
            response: res,
            peer_addr: Some(SocketAddr::from_str("127.0.0.1:8888").unwrap()),
            local_addr: None,
        };
        assert_eq!(shake.remote_addr().unwrap().unwrap(), "127.0.0.1");
    }

    #[test]
    fn remote_addr_x_forwarded_for() {
        let mut buf = Vec::with_capacity(2048);
        write!(
            &mut buf,
            "GET / HTTP/1.1\r\n\
             Connection: Upgrade\r\n\
             Upgrade: websocket\r\n\
             X-Forwarded-For: 192.168.1.1, 192.168.1.2, 192.168.1.3\r\n\
             Sec-WebSocket-Version: 13\r\n\
             Sec-WebSocket-Key: q16eN37NCfVwUChPvBdk4g==\r\n\r\n"
        ).unwrap();

        let req = Request::parse(&buf).unwrap().unwrap();
        let res = Response::from_request(&req).unwrap();
        let shake = Handshake {
            request: req,
            response: res,
            peer_addr: None,
            local_addr: None,
        };
        assert_eq!(shake.remote_addr().unwrap().unwrap(), "192.168.1.1");
    }

    #[test]
    fn remote_addr_forwarded() {
        let mut buf = Vec::with_capacity(2048);
        write!(
            &mut buf,
            "GET / HTTP/1.1\r\n\
            Connection: Upgrade\r\n\
            Upgrade: websocket\r\n\
            Forwarded: by=192.168.1.1; for=192.0.2.43, for=\"[2001:db8:cafe::17]\", for=unknown\r\n\
            Sec-WebSocket-Version: 13\r\n\
            Sec-WebSocket-Key: q16eN37NCfVwUChPvBdk4g==\r\n\r\n")
            .unwrap();
        let req = Request::parse(&buf).unwrap().unwrap();
        let res = Response::from_request(&req).unwrap();
        let shake = Handshake {
            request: req,
            response: res,
            peer_addr: None,
            local_addr: None,
        };
        assert_eq!(shake.remote_addr().unwrap().unwrap(), "192.0.2.43");
    }
}
