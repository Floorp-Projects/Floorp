use log::Level::Error as ErrorLevel;
#[cfg(feature = "nativetls")]
use native_tls::{TlsConnector, TlsStream as SslStream};
#[cfg(feature = "ssl")]
use openssl::ssl::{SslConnector, SslMethod, SslStream};
use url;

use frame::Frame;
use handshake::{Handshake, Request, Response};
use message::Message;
use protocol::CloseCode;
use result::{Error, Kind, Result};
use util::{Timeout, Token};

#[cfg(any(feature = "ssl", feature = "nativetls"))]
use util::TcpStream;

/// The core trait of this library.
/// Implementing this trait provides the business logic of the WebSocket application.
pub trait Handler {
    // general

    /// Called when a request to shutdown all connections has been received.
    #[inline]
    fn on_shutdown(&mut self) {
        debug!("Handler received WebSocket shutdown request.");
    }

    // WebSocket events

    /// Called when the WebSocket handshake is successful and the connection is open for sending
    /// and receiving messages.
    fn on_open(&mut self, shake: Handshake) -> Result<()> {
        if let Some(addr) = shake.remote_addr()? {
            debug!("Connection with {} now open", addr);
        }
        Ok(())
    }

    /// Called on incoming messages.
    fn on_message(&mut self, msg: Message) -> Result<()> {
        debug!("Received message {:?}", msg);
        Ok(())
    }

    /// Called any time this endpoint receives a close control frame.
    /// This may be because the other endpoint is initiating a closing handshake,
    /// or it may be the other endpoint confirming the handshake initiated by this endpoint.
    fn on_close(&mut self, code: CloseCode, reason: &str) {
        debug!("Connection closing due to ({:?}) {}", code, reason);
    }

    /// Called when an error occurs on the WebSocket.
    fn on_error(&mut self, err: Error) {
        // Ignore connection reset errors by default, but allow library clients to see them by
        // overriding this method if they want
        if let Kind::Io(ref err) = err.kind {
            if let Some(104) = err.raw_os_error() {
                return;
            }
        }

        error!("{:?}", err);
        if !log_enabled!(ErrorLevel) {
            println!(
                "Encountered an error: {}\nEnable a logger to see more information.",
                err
            );
        }
    }

    // handshake events

    /// A method for handling the low-level workings of the request portion of the WebSocket
    /// handshake.
    ///
    /// Implementors should select a WebSocket protocol and extensions where they are supported.
    ///
    /// Implementors can inspect the Request and must return a Response or an error
    /// indicating that the handshake failed. The default implementation provides conformance with
    /// the WebSocket protocol, and implementors should use the `Response::from_request` method and
    /// then modify the resulting response as necessary in order to maintain conformance.
    ///
    /// This method will not be called when the handler represents a client endpoint. Use
    /// `build_request` to provide an initial handshake request.
    ///
    /// # Examples
    ///
    /// ```ignore
    /// let mut res = try!(Response::from_request(req));
    /// if try!(req.extensions()).iter().find(|&&ext| ext.contains("myextension-name")).is_some() {
    ///     res.add_extension("myextension-name")
    /// }
    /// Ok(res)
    /// ```
    #[inline]
    fn on_request(&mut self, req: &Request) -> Result<Response> {
        debug!("Handler received request:\n{}", req);
        Response::from_request(req)
    }

    /// A method for handling the low-level workings of the response portion of the WebSocket
    /// handshake.
    ///
    /// Implementors can inspect the Response and choose to fail the connection by
    /// returning an error. This method will not be called when the handler represents a server
    /// endpoint. The response should indicate which WebSocket protocol and extensions the server
    /// has agreed to if any.
    #[inline]
    fn on_response(&mut self, res: &Response) -> Result<()> {
        debug!("Handler received response:\n{}", res);
        Ok(())
    }

    // timeout events

    /// Called when a timeout is triggered.
    ///
    /// This method will be called when the eventloop encounters a timeout on the specified
    /// token. To schedule a timeout with your specific token use the `Sender::timeout` method.
    ///
    /// # Examples
    ///
    /// ```ignore
    /// const GRATI: Token = Token(1);
    ///
    /// ... Handler
    ///
    /// fn on_open(&mut self, _: Handshake) -> Result<()> {
    ///     // schedule a timeout to send a gratuitous pong every 5 seconds
    ///     self.ws.timeout(5_000, GRATI)
    /// }
    ///
    /// fn on_timeout(&mut self, event: Token) -> Result<()> {
    ///     if event == GRATI {
    ///         // send gratuitous pong
    ///         try!(self.ws.pong(vec![]))
    ///         // reschedule the timeout
    ///         self.ws.timeout(5_000, GRATI)
    ///     } else {
    ///         Err(Error::new(ErrorKind::Internal, "Invalid timeout token encountered!"))
    ///     }
    /// }
    /// ```
    #[inline]
    fn on_timeout(&mut self, event: Token) -> Result<()> {
        debug!("Handler received timeout token: {:?}", event);
        Ok(())
    }

    /// Called when a timeout has been scheduled on the eventloop.
    ///
    /// This method is the hook for obtaining a Timeout object that may be used to cancel a
    /// timeout. This is a noop by default.
    ///
    /// # Examples
    ///
    /// ```ignore
    /// const PING: Token = Token(1);
    /// const EXPIRE: Token = Token(2);
    ///
    /// ... Handler
    ///
    /// fn on_open(&mut self, _: Handshake) -> Result<()> {
    ///     // schedule a timeout to send a ping every 5 seconds
    ///     try!(self.ws.timeout(5_000, PING));
    ///     // schedule a timeout to close the connection if there is no activity for 30 seconds
    ///     self.ws.timeout(30_000, EXPIRE)
    /// }
    ///
    /// fn on_timeout(&mut self, event: Token) -> Result<()> {
    ///     match event {
    ///         PING => {
    ///             self.ws.ping(vec![]);
    ///             self.ws.timeout(5_000, PING)
    ///         }
    ///         EXPIRE => self.ws.close(CloseCode::Away),
    ///         _ => Err(Error::new(ErrorKind::Internal, "Invalid timeout token encountered!")),
    ///     }
    /// }
    ///
    /// fn on_new_timeout(&mut self, event: Token, timeout: Timeout) -> Result<()> {
    ///     if event == EXPIRE {
    ///         if let Some(t) = self.timeout.take() {
    ///             try!(self.ws.cancel(t))
    ///         }
    ///         self.timeout = Some(timeout)
    ///     }
    ///     Ok(())
    /// }
    ///
    /// fn on_frame(&mut self, frame: Frame) -> Result<Option<Frame>> {
    ///     // some activity has occurred, let's reset the expiration
    ///     try!(self.ws.timeout(30_000, EXPIRE));
    ///     Ok(Some(frame))
    /// }
    /// ```
    #[inline]
    fn on_new_timeout(&mut self, _: Token, _: Timeout) -> Result<()> {
        // default implementation discards the timeout handle
        Ok(())
    }

    // frame events

    /// A method for handling incoming frames.
    ///
    /// This method provides very low-level access to the details of the WebSocket protocol. It may
    /// be necessary to implement this method in order to provide a particular extension, but
    /// incorrect implementation may cause the other endpoint to fail the connection.
    ///
    /// Returning `Ok(None)` will cause the connection to forget about a particular frame. This is
    /// useful if you want ot filter out a frame or if you don't want any of the default handler
    /// methods to run.
    ///
    /// By default this method simply ensures that no reserved bits are set.
    #[inline]
    fn on_frame(&mut self, frame: Frame) -> Result<Option<Frame>> {
        debug!("Handler received: {}", frame);
        // default implementation doesn't allow for reserved bits to be set
        if frame.has_rsv1() || frame.has_rsv2() || frame.has_rsv3() {
            Err(Error::new(
                Kind::Protocol,
                "Encountered frame with reserved bits set.",
            ))
        } else {
            Ok(Some(frame))
        }
    }

    /// A method for handling outgoing frames.
    ///
    /// This method provides very low-level access to the details of the WebSocket protocol. It may
    /// be necessary to implement this method in order to provide a particular extension, but
    /// incorrect implementation may cause the other endpoint to fail the connection.
    ///
    /// Returning `Ok(None)` will cause the connection to forget about a particular frame, meaning
    /// that it will not be sent. You can use this approach to merge multiple frames into a single
    /// frame before sending the message.
    ///
    /// For messages, this method will be called with a single complete, final frame before any
    /// fragmentation is performed. Automatic fragmentation will be performed on the returned
    /// frame, if any, based on the `fragment_size` setting.
    ///
    /// By default this method simply ensures that no reserved bits are set.
    #[inline]
    fn on_send_frame(&mut self, frame: Frame) -> Result<Option<Frame>> {
        trace!("Handler will send: {}", frame);
        // default implementation doesn't allow for reserved bits to be set
        if frame.has_rsv1() || frame.has_rsv2() || frame.has_rsv3() {
            Err(Error::new(
                Kind::Protocol,
                "Encountered frame with reserved bits set.",
            ))
        } else {
            Ok(Some(frame))
        }
    }

    // constructors

    /// A method for creating the initial handshake request for WebSocket clients.
    ///
    /// The default implementation provides conformance with the WebSocket protocol, but this
    /// method may be overridden. In order to facilitate conformance,
    /// implementors should use the `Request::from_url` method and then modify the resulting
    /// request as necessary.
    ///
    /// Implementors should indicate any available WebSocket extensions here.
    ///
    /// # Examples
    /// ```ignore
    /// let mut req = try!(Request::from_url(url));
    /// req.add_extension("permessage-deflate; client_max_window_bits");
    /// Ok(req)
    /// ```
    #[inline]
    fn build_request(&mut self, url: &url::Url) -> Result<Request> {
        trace!("Handler is building request to {}.", url);
        Request::from_url(url)
    }

    /// A method for wrapping a client TcpStream with Ssl Authentication machinery
    ///
    /// Override this method to customize how the connection is encrypted. By default
    /// this will use the Server Name Indication extension in conformance with RFC6455.
    #[inline]
    #[cfg(feature = "ssl")]
    fn upgrade_ssl_client(
        &mut self,
        stream: TcpStream,
        url: &url::Url,
    ) -> Result<SslStream<TcpStream>> {
        let domain = url.domain().ok_or(Error::new(
            Kind::Protocol,
            format!("Unable to parse domain from {}. Needed for SSL.", url),
        ))?;
        let connector = SslConnector::builder(SslMethod::tls())
            .map_err(|e| {
                Error::new(
                    Kind::Internal,
                    format!("Failed to upgrade client to SSL: {}", e),
                )
            })?
            .build();
        connector.connect(domain, stream).map_err(Error::from)
    }

    #[inline]
    #[cfg(feature = "nativetls")]
    fn upgrade_ssl_client(
        &mut self,
        stream: TcpStream,
        url: &url::Url,
    ) -> Result<SslStream<TcpStream>> {
        let domain = url.domain().ok_or(Error::new(
            Kind::Protocol,
            format!("Unable to parse domain from {}. Needed for SSL.", url),
        ))?;

        let connector = TlsConnector::new().map_err(|e| {
            Error::new(
                Kind::Internal,
                format!("Failed to upgrade client to SSL: {}", e),
            )
        })?;

        connector.connect(domain, stream).map_err(Error::from)
    }
    /// A method for wrapping a server TcpStream with Ssl Authentication machinery
    ///
    /// Override this method to customize how the connection is encrypted. By default
    /// this method is not implemented.
    #[inline]
    #[cfg(any(feature = "ssl", feature = "nativetls"))]
    fn upgrade_ssl_server(&mut self, _: TcpStream) -> Result<SslStream<TcpStream>> {
        unimplemented!()
    }
}

impl<F> Handler for F
where
    F: Fn(Message) -> Result<()>,
{
    fn on_message(&mut self, msg: Message) -> Result<()> {
        self(msg)
    }
}

mod test {
    #![allow(unused_imports, unused_variables, dead_code)]
    use super::*;
    use frame;
    use handshake::{Handshake, Request, Response};
    use message;
    use mio;
    use protocol::CloseCode;
    use result::Result;
    use url;

    #[derive(Debug, Eq, PartialEq)]
    struct M;
    impl Handler for M {
        fn on_message(&mut self, _: message::Message) -> Result<()> {
            println!("test");
            Ok(())
        }

        fn on_frame(&mut self, f: frame::Frame) -> Result<Option<frame::Frame>> {
            Ok(None)
        }
    }

    #[test]
    fn handler() {
        struct H;

        impl Handler for H {
            fn on_open(&mut self, shake: Handshake) -> Result<()> {
                assert!(shake.request.key().is_ok());
                assert!(shake.response.key().is_ok());
                Ok(())
            }

            fn on_message(&mut self, msg: message::Message) -> Result<()> {
                Ok(assert_eq!(
                    msg,
                    message::Message::Text(String::from("testme"))
                ))
            }

            fn on_close(&mut self, code: CloseCode, _: &str) {
                assert_eq!(code, CloseCode::Normal)
            }
        }

        let mut h = H;
        let url = url::Url::parse("wss://127.0.0.1:3012").unwrap();
        let req = Request::from_url(&url).unwrap();
        let res = Response::from_request(&req).unwrap();
        h.on_open(Handshake {
            request: req,
            response: res,
            peer_addr: None,
            local_addr: None,
        }).unwrap();
        h.on_message(message::Message::Text("testme".to_owned()))
            .unwrap();
        h.on_close(CloseCode::Normal, "");
    }

    #[test]
    fn closure_handler() {
        let mut close = |msg| {
            assert_eq!(msg, message::Message::Binary(vec![1, 2, 3]));
            Ok(())
        };

        close
            .on_message(message::Message::Binary(vec![1, 2, 3]))
            .unwrap();
    }
}
