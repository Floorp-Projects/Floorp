use std::mem::replace;

#[cfg(feature = "ssl")]
use openssl::ssl::SslStream;
#[cfg(feature = "nativetls")]
use native_tls::TlsStream as SslStream;
use url;

use frame::Frame;
use handler::Handler;
use handshake::{Handshake, Request, Response};
use message::Message;
use protocol::{CloseCode, OpCode};
use result::{Error, Kind, Result};
#[cfg(any(feature = "ssl", feature = "nativetls"))]
use util::TcpStream;
use util::{Timeout, Token};

use super::context::{Compressor, Decompressor};

/// Deflate Extension Handler Settings
#[derive(Debug, Clone, Copy)]
pub struct DeflateSettings {
    /// The max size of the sliding window. If the other endpoint selects a smaller size, that size
    /// will be used instead. This must be an integer between 9 and 15 inclusive.
    /// Default: 15
    pub max_window_bits: u8,
    /// Indicates whether to ask the other endpoint to reset the sliding window for each message.
    /// Default: false
    pub request_no_context_takeover: bool,
    /// Indicates whether this endpoint will agree to reset the sliding window for each message it
    /// compresses. If this endpoint won't agree to reset the sliding window, then the handshake
    /// will fail if this endpoint is a client and the server requests no context takeover.
    /// Default: true
    pub accept_no_context_takeover: bool,
    /// The number of WebSocket frames to store when defragmenting an incoming fragmented
    /// compressed message.
    /// This setting may be different from the `fragments_capacity` setting of the WebSocket in order to
    /// allow for differences between compressed and uncompressed messages.
    /// Default: 10
    pub fragments_capacity: usize,
    /// Indicates whether the extension handler will reallocate if the `fragments_capacity` is
    /// exceeded. If this is not true, a capacity error will be triggered instead.
    /// Default: true
    pub fragments_grow: bool,
}

impl Default for DeflateSettings {
    fn default() -> DeflateSettings {
        DeflateSettings {
            max_window_bits: 15,
            request_no_context_takeover: false,
            accept_no_context_takeover: true,
            fragments_capacity: 10,
            fragments_grow: true,
        }
    }
}

/// Utility for applying the permessage-deflate extension to a handler with particular deflate
/// settings.
#[derive(Debug, Clone, Copy)]
pub struct DeflateBuilder {
    settings: DeflateSettings,
}

impl DeflateBuilder {
    /// Create a new DeflateBuilder with the default settings.
    pub fn new() -> DeflateBuilder {
        DeflateBuilder {
            settings: DeflateSettings::default(),
        }
    }

    /// Configure the DeflateBuilder with the given deflate settings.
    pub fn with_settings(&mut self, settings: DeflateSettings) -> &mut DeflateBuilder {
        self.settings = settings;
        self
    }

    /// Wrap another handler in with a deflate handler as configured.
    pub fn build<H: Handler>(&self, handler: H) -> DeflateHandler<H> {
        DeflateHandler {
            com: Compressor::new(self.settings.max_window_bits as i8),
            dec: Decompressor::new(self.settings.max_window_bits as i8),
            fragments: Vec::with_capacity(self.settings.fragments_capacity),
            compress_reset: false,
            decompress_reset: false,
            pass: false,
            settings: self.settings,
            inner: handler,
        }
    }
}

/// A WebSocket handler that implements the permessage-deflate extension.
///
/// This handler wraps a child handler and proxies all handler methods to it. The handler will
/// decompress incoming WebSocket message frames in their reserved bits match the
/// permessage-deflate specification and pass them to the child handler. Message frames sent from
/// the child handler will be compressed and sent to the other endpoint using deflate compression.
pub struct DeflateHandler<H: Handler> {
    com: Compressor,
    dec: Decompressor,
    fragments: Vec<Frame>,
    compress_reset: bool,
    decompress_reset: bool,
    pass: bool,
    settings: DeflateSettings,
    inner: H,
}

impl<H: Handler> DeflateHandler<H> {
    /// Wrap a child handler to provide the permessage-deflate extension.
    pub fn new(handler: H) -> DeflateHandler<H> {
        trace!("Using permessage-deflate handler.");
        let settings = DeflateSettings::default();
        DeflateHandler {
            com: Compressor::new(settings.max_window_bits as i8),
            dec: Decompressor::new(settings.max_window_bits as i8),
            fragments: Vec::with_capacity(settings.fragments_capacity),
            compress_reset: false,
            decompress_reset: false,
            pass: false,
            settings: settings,
            inner: handler,
        }
    }

    #[doc(hidden)]
    #[inline]
    fn decline(&mut self, mut res: Response) -> Result<Response> {
        trace!("Declined permessage-deflate offer");
        self.pass = true;
        res.remove_extension("permessage-deflate");
        Ok(res)
    }
}

impl<H: Handler> Handler for DeflateHandler<H> {
    fn build_request(&mut self, url: &url::Url) -> Result<Request> {
        let mut req = self.inner.build_request(url)?;
        let mut req_ext = String::with_capacity(100);
        req_ext.push_str("permessage-deflate");
        if self.settings.max_window_bits < 15 {
            req_ext.push_str(&format!(
                "; client_max_window_bits={}; server_max_window_bits={}",
                self.settings.max_window_bits, self.settings.max_window_bits
            ))
        } else {
            req_ext.push_str("; client_max_window_bits")
        }
        if self.settings.request_no_context_takeover {
            req_ext.push_str("; server_no_context_takeover")
        }
        req.add_extension(&req_ext);
        Ok(req)
    }

    fn on_request(&mut self, req: &Request) -> Result<Response> {
        let mut res = self.inner.on_request(req)?;

        'ext: for req_ext in req.extensions()?
            .iter()
            .filter(|&&ext| ext.contains("permessage-deflate"))
        {
            let mut res_ext = String::with_capacity(req_ext.len());
            let mut s_takeover = false;
            let mut c_takeover = false;
            let mut s_max = false;
            let mut c_max = false;

            for param in req_ext.split(';') {
                match param.trim() {
                    "permessage-deflate" => res_ext.push_str("permessage-deflate"),
                    "server_no_context_takeover" => {
                        if s_takeover {
                            return self.decline(res);
                        } else {
                            s_takeover = true;
                            if self.settings.accept_no_context_takeover {
                                self.compress_reset = true;
                                res_ext.push_str("; server_no_context_takeover");
                            } else {
                                continue 'ext;
                            }
                        }
                    }
                    "client_no_context_takeover" => {
                        if c_takeover {
                            return self.decline(res);
                        } else {
                            c_takeover = true;
                            self.decompress_reset = true;
                            res_ext.push_str("; client_no_context_takeover");
                        }
                    }
                    param if param.starts_with("server_max_window_bits") => {
                        if s_max {
                            return self.decline(res);
                        } else {
                            s_max = true;
                            let mut param_iter = param.split('=');
                            param_iter.next(); // we already know the name
                            if let Some(window_bits_str) = param_iter.next() {
                                if let Ok(window_bits) = window_bits_str.trim().parse() {
                                    if window_bits >= 9 && window_bits <= 15 {
                                        if window_bits < self.settings.max_window_bits as i8 {
                                            self.com = Compressor::new(window_bits);
                                            res_ext.push_str("; ");
                                            res_ext.push_str(param)
                                        }
                                    } else {
                                        return self.decline(res);
                                    }
                                } else {
                                    return self.decline(res);
                                }
                            }
                        }
                    }
                    param if param.starts_with("client_max_window_bits") => {
                        if c_max {
                            return self.decline(res);
                        } else {
                            c_max = true;
                            let mut param_iter = param.split('=');
                            param_iter.next(); // we already know the name
                            if let Some(window_bits_str) = param_iter.next() {
                                if let Ok(window_bits) = window_bits_str.trim().parse() {
                                    if window_bits >= 9 && window_bits <= 15 {
                                        if window_bits < self.settings.max_window_bits as i8 {
                                            self.dec = Decompressor::new(window_bits);
                                            res_ext.push_str("; ");
                                            res_ext.push_str(param);
                                            continue;
                                        }
                                    } else {
                                        return self.decline(res);
                                    }
                                } else {
                                    return self.decline(res);
                                }
                            }
                            res_ext.push_str("; ");
                            res_ext.push_str(&format!(
                                "client_max_window_bits={}",
                                self.settings.max_window_bits
                            ))
                        }
                    }
                    _ => {
                        // decline all extension offers because we got a bad parameter
                        return self.decline(res);
                    }
                }
            }

            if !res_ext.contains("client_no_context_takeover")
                && self.settings.request_no_context_takeover
            {
                self.decompress_reset = true;
                res_ext.push_str("; client_no_context_takeover");
            }

            if !res_ext.contains("server_max_window_bits") {
                res_ext.push_str("; ");
                res_ext.push_str(&format!(
                    "server_max_window_bits={}",
                    self.settings.max_window_bits
                ))
            }

            if !res_ext.contains("client_max_window_bits") && self.settings.max_window_bits < 15 {
                continue;
            }

            res.add_extension(&res_ext);
            return Ok(res);
        }
        self.decline(res)
    }

    fn on_response(&mut self, res: &Response) -> Result<()> {
        if let Some(res_ext) = res.extensions()?
            .iter()
            .find(|&&ext| ext.contains("permessage-deflate"))
        {
            let mut name = false;
            let mut s_takeover = false;
            let mut c_takeover = false;
            let mut s_max = false;
            let mut c_max = false;

            for param in res_ext.split(';') {
                match param.trim() {
                    "permessage-deflate" => {
                        if name {
                            return Err(Error::new(
                                Kind::Protocol,
                                format!("Duplicate extension name permessage-deflate"),
                            ));
                        } else {
                            name = true;
                        }
                    }
                    "server_no_context_takeover" => {
                        if s_takeover {
                            return Err(Error::new(
                                Kind::Protocol,
                                format!("Duplicate extension parameter server_no_context_takeover"),
                            ));
                        } else {
                            s_takeover = true;
                            self.decompress_reset = true;
                        }
                    }
                    "client_no_context_takeover" => {
                        if c_takeover {
                            return Err(Error::new(
                                Kind::Protocol,
                                format!("Duplicate extension parameter client_no_context_takeover"),
                            ));
                        } else {
                            c_takeover = true;
                            if self.settings.accept_no_context_takeover {
                                self.compress_reset = true;
                            } else {
                                return Err(Error::new(
                                    Kind::Protocol,
                                    format!("The client requires context takeover."),
                                ));
                            }
                        }
                    }
                    param if param.starts_with("server_max_window_bits") => {
                        if s_max {
                            return Err(Error::new(
                                Kind::Protocol,
                                format!("Duplicate extension parameter server_max_window_bits"),
                            ));
                        } else {
                            s_max = true;
                            let mut param_iter = param.split('=');
                            param_iter.next(); // we already know the name
                            if let Some(window_bits_str) = param_iter.next() {
                                if let Ok(window_bits) = window_bits_str.trim().parse() {
                                    if window_bits >= 9 && window_bits <= 15 {
                                        if window_bits as u8 != self.settings.max_window_bits {
                                            self.dec = Decompressor::new(window_bits);
                                        }
                                    } else {
                                        return Err(Error::new(
                                            Kind::Protocol,
                                            format!(
                                                "Invalid server_max_window_bits parameter: {}",
                                                window_bits
                                            ),
                                        ));
                                    }
                                } else {
                                    return Err(Error::new(
                                        Kind::Protocol,
                                        format!(
                                            "Invalid server_max_window_bits parameter: {}",
                                            window_bits_str
                                        ),
                                    ));
                                }
                            }
                        }
                    }
                    param if param.starts_with("client_max_window_bits") => {
                        if c_max {
                            return Err(Error::new(
                                Kind::Protocol,
                                format!("Duplicate extension parameter client_max_window_bits"),
                            ));
                        } else {
                            c_max = true;
                            let mut param_iter = param.split('=');
                            param_iter.next(); // we already know the name
                            if let Some(window_bits_str) = param_iter.next() {
                                if let Ok(window_bits) = window_bits_str.trim().parse() {
                                    if window_bits >= 9 && window_bits <= 15 {
                                        if window_bits as u8 != self.settings.max_window_bits {
                                            self.com = Compressor::new(window_bits);
                                        }
                                    } else {
                                        return Err(Error::new(
                                            Kind::Protocol,
                                            format!(
                                                "Invalid client_max_window_bits parameter: {}",
                                                window_bits
                                            ),
                                        ));
                                    }
                                } else {
                                    return Err(Error::new(
                                        Kind::Protocol,
                                        format!(
                                            "Invalid client_max_window_bits parameter: {}",
                                            window_bits_str
                                        ),
                                    ));
                                }
                            }
                        }
                    }
                    param => {
                        // fail the connection because we got a bad parameter
                        return Err(Error::new(
                            Kind::Protocol,
                            format!("Bad extension parameter: {}", param),
                        ));
                    }
                }
            }
        } else {
            self.pass = true
        }

        Ok(())
    }

    fn on_frame(&mut self, mut frame: Frame) -> Result<Option<Frame>> {
        if !self.pass && !frame.is_control() {
            if !self.fragments.is_empty() || frame.has_rsv1() {
                frame.set_rsv1(false);

                if !frame.is_final() {
                    self.fragments.push(frame);
                    return Ok(None);
                } else {
                    if frame.opcode() == OpCode::Continue {
                        if self.fragments.is_empty() {
                            return Err(Error::new(
                                Kind::Protocol,
                                "Unable to reconstruct fragmented message. No first frame.",
                            ));
                        } else {
                            if !self.settings.fragments_grow
                                && self.settings.fragments_capacity == self.fragments.len()
                            {
                                return Err(Error::new(Kind::Capacity, "Exceeded max fragments."));
                            } else {
                                self.fragments.push(frame);
                            }

                            // it's safe to unwrap because of the above check for empty
                            let opcode = self.fragments.first().unwrap().opcode();
                            let size = self.fragments
                                .iter()
                                .fold(0, |len, frame| len + frame.payload().len());
                            let mut compressed = Vec::with_capacity(size);
                            let mut decompressed = Vec::with_capacity(size * 2);
                            for frag in replace(
                                &mut self.fragments,
                                Vec::with_capacity(self.settings.fragments_capacity),
                            ) {
                                compressed.extend(frag.into_data())
                            }

                            compressed.extend(&[0, 0, 255, 255]);
                            self.dec.decompress(&compressed, &mut decompressed)?;
                            frame = Frame::message(decompressed, opcode, true);
                        }
                    } else {
                        let mut decompressed = Vec::with_capacity(frame.payload().len() * 2);
                        frame.payload_mut().extend(&[0, 0, 255, 255]);

                        self.dec.decompress(frame.payload(), &mut decompressed)?;

                        *frame.payload_mut() = decompressed;
                    }

                    if self.decompress_reset {
                        self.dec.reset()?
                    }
                }
            }
        }
        self.inner.on_frame(frame)
    }

    fn on_send_frame(&mut self, frame: Frame) -> Result<Option<Frame>> {
        if let Some(mut frame) = self.inner.on_send_frame(frame)? {
            if !self.pass && !frame.is_control() {
                debug_assert!(
                    frame.is_final(),
                    "Received non-final frame from upstream handler!"
                );
                debug_assert!(
                    frame.opcode() != OpCode::Continue,
                    "Received continue frame from upstream handler!"
                );

                frame.set_rsv1(true);
                let mut compressed = Vec::with_capacity(frame.payload().len());
                self.com.compress(frame.payload(), &mut compressed)?;
                let len = compressed.len();
                compressed.truncate(len - 4);
                *frame.payload_mut() = compressed;

                if self.compress_reset {
                    self.com.reset()?
                }
            }
            Ok(Some(frame))
        } else {
            Ok(None)
        }
    }

    #[inline]
    fn on_shutdown(&mut self) {
        self.inner.on_shutdown()
    }

    #[inline]
    fn on_open(&mut self, shake: Handshake) -> Result<()> {
        self.inner.on_open(shake)
    }

    #[inline]
    fn on_message(&mut self, msg: Message) -> Result<()> {
        self.inner.on_message(msg)
    }

    #[inline]
    fn on_close(&mut self, code: CloseCode, reason: &str) {
        self.inner.on_close(code, reason)
    }

    #[inline]
    fn on_error(&mut self, err: Error) {
        self.inner.on_error(err)
    }

    #[inline]
    fn on_timeout(&mut self, event: Token) -> Result<()> {
        self.inner.on_timeout(event)
    }

    #[inline]
    fn on_new_timeout(&mut self, tok: Token, timeout: Timeout) -> Result<()> {
        self.inner.on_new_timeout(tok, timeout)
    }

    #[inline]
    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    fn upgrade_ssl_client(
        &mut self,
        stream: TcpStream,
        url: &url::Url,
    ) -> Result<SslStream<TcpStream>> {
        self.inner.upgrade_ssl_client(stream, url)
    }

    #[inline]
    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    fn upgrade_ssl_server(&mut self, stream: TcpStream) -> Result<SslStream<TcpStream>> {
        self.inner.upgrade_ssl_server(stream)
    }
}
