//! Connectors used by the `Client`.
//!
//! This module contains:
//!
//! - A default [`HttpConnector`][] that does DNS resolution and establishes
//!   connections over TCP.
//! - Types to build custom connectors.
//!
//! # Connectors
//!
//! A "connector" is a [`Service`][] that takes a [`Uri`][] destination, and
//! its `Response` is some type implementing [`AsyncRead`][], [`AsyncWrite`][],
//! and [`Connection`][].
//!
//! ## Custom Connectors
//!
//! A simple connector that ignores the `Uri` destination and always returns
//! a TCP connection to the same address could be written like this:
//!
//! ```rust,ignore
//! let connector = tower::service_fn(|_dst| async {
//!     tokio::net::TcpStream::connect("127.0.0.1:1337")
//! })
//! ```
//!
//! Or, fully written out:
//!
//! ```
//! use std::{future::Future, net::SocketAddr, pin::Pin, task::{self, Poll}};
//! use hyper::{service::Service, Uri};
//! use tokio::net::TcpStream;
//!
//! #[derive(Clone)]
//! struct LocalConnector;
//!
//! impl Service<Uri> for LocalConnector {
//!     type Response = TcpStream;
//!     type Error = std::io::Error;
//!     // We can't "name" an `async` generated future.
//!     type Future = Pin<Box<
//!         dyn Future<Output = Result<Self::Response, Self::Error>> + Send
//!     >>;
//!
//!     fn poll_ready(&mut self, _: &mut task::Context<'_>) -> Poll<Result<(), Self::Error>> {
//!         // This connector is always ready, but others might not be.
//!         Poll::Ready(Ok(()))
//!     }
//!
//!     fn call(&mut self, _: Uri) -> Self::Future {
//!         Box::pin(TcpStream::connect(SocketAddr::from(([127, 0, 0, 1], 1337))))
//!     }
//! }
//! ```
//!
//! It's worth noting that for `TcpStream`s, the [`HttpConnector`][] is a
//! better starting place to extend from.
//!
//! Using either of the above connector examples, it can be used with the
//! `Client` like this:
//!
//! ```
//! # let connector = hyper::client::HttpConnector::new();
//! // let connector = ...
//!
//! let client = hyper::Client::builder()
//!     .build::<_, hyper::Body>(connector);
//! ```
//!
//!
//! [`HttpConnector`]: HttpConnector
//! [`Service`]: crate::service::Service
//! [`Uri`]: http::Uri
//! [`AsyncRead`]: tokio::io::AsyncRead
//! [`AsyncWrite`]: tokio::io::AsyncWrite
//! [`Connection`]: Connection
use std::fmt;

use ::http::Response;

#[cfg(feature = "tcp")]
pub mod dns;
#[cfg(feature = "tcp")]
mod http;
#[cfg(feature = "tcp")]
pub use self::http::{HttpConnector, HttpInfo};
pub use self::sealed::Connect;

/// Describes a type returned by a connector.
pub trait Connection {
    /// Return metadata describing the connection.
    fn connected(&self) -> Connected;
}

/// Extra information about the connected transport.
///
/// This can be used to inform recipients about things like if ALPN
/// was used, or if connected to an HTTP proxy.
#[derive(Debug)]
pub struct Connected {
    pub(super) alpn: Alpn,
    pub(super) is_proxied: bool,
    pub(super) extra: Option<Extra>,
}

pub(super) struct Extra(Box<dyn ExtraInner>);

#[derive(Clone, Copy, Debug, PartialEq)]
pub(super) enum Alpn {
    H2,
    None,
}

impl Connected {
    /// Create new `Connected` type with empty metadata.
    pub fn new() -> Connected {
        Connected {
            alpn: Alpn::None,
            is_proxied: false,
            extra: None,
        }
    }

    /// Set whether the connected transport is to an HTTP proxy.
    ///
    /// This setting will affect if HTTP/1 requests written on the transport
    /// will have the request-target in absolute-form or origin-form:
    ///
    /// - When `proxy(false)`:
    ///
    /// ```http
    /// GET /guide HTTP/1.1
    /// ```
    ///
    /// - When `proxy(true)`:
    ///
    /// ```http
    /// GET http://hyper.rs/guide HTTP/1.1
    /// ```
    ///
    /// Default is `false`.
    pub fn proxy(mut self, is_proxied: bool) -> Connected {
        self.is_proxied = is_proxied;
        self
    }

    /// Set extra connection information to be set in the extensions of every `Response`.
    pub fn extra<T: Clone + Send + Sync + 'static>(mut self, extra: T) -> Connected {
        if let Some(prev) = self.extra {
            self.extra = Some(Extra(Box::new(ExtraChain(prev.0, extra))));
        } else {
            self.extra = Some(Extra(Box::new(ExtraEnvelope(extra))));
        }
        self
    }

    /// Set that the connected transport negotiated HTTP/2 as it's
    /// next protocol.
    pub fn negotiated_h2(mut self) -> Connected {
        self.alpn = Alpn::H2;
        self
    }

    // Don't public expose that `Connected` is `Clone`, unsure if we want to
    // keep that contract...
    pub(super) fn clone(&self) -> Connected {
        Connected {
            alpn: self.alpn.clone(),
            is_proxied: self.is_proxied,
            extra: self.extra.clone(),
        }
    }
}

// ===== impl Extra =====

impl Extra {
    pub(super) fn set(&self, res: &mut Response<crate::Body>) {
        self.0.set(res);
    }
}

impl Clone for Extra {
    fn clone(&self) -> Extra {
        Extra(self.0.clone_box())
    }
}

impl fmt::Debug for Extra {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Extra").finish()
    }
}

trait ExtraInner: Send + Sync {
    fn clone_box(&self) -> Box<dyn ExtraInner>;
    fn set(&self, res: &mut Response<crate::Body>);
}

// This indirection allows the `Connected` to have a type-erased "extra" value,
// while that type still knows its inner extra type. This allows the correct
// TypeId to be used when inserting into `res.extensions_mut()`.
#[derive(Clone)]
struct ExtraEnvelope<T>(T);

impl<T> ExtraInner for ExtraEnvelope<T>
where
    T: Clone + Send + Sync + 'static,
{
    fn clone_box(&self) -> Box<dyn ExtraInner> {
        Box::new(self.clone())
    }

    fn set(&self, res: &mut Response<crate::Body>) {
        res.extensions_mut().insert(self.0.clone());
    }
}

struct ExtraChain<T>(Box<dyn ExtraInner>, T);

impl<T: Clone> Clone for ExtraChain<T> {
    fn clone(&self) -> Self {
        ExtraChain(self.0.clone_box(), self.1.clone())
    }
}

impl<T> ExtraInner for ExtraChain<T>
where
    T: Clone + Send + Sync + 'static,
{
    fn clone_box(&self) -> Box<dyn ExtraInner> {
        Box::new(self.clone())
    }

    fn set(&self, res: &mut Response<crate::Body>) {
        self.0.set(res);
        res.extensions_mut().insert(self.1.clone());
    }
}

pub(super) mod sealed {
    use std::error::Error as StdError;

    use ::http::Uri;
    use tokio::io::{AsyncRead, AsyncWrite};

    use super::Connection;
    use crate::common::{Future, Unpin};

    /// Connect to a destination, returning an IO transport.
    ///
    /// A connector receives a [`Uri`](::http::Uri) and returns a `Future` of the
    /// ready connection.
    ///
    /// # Trait Alias
    ///
    /// This is really just an *alias* for the `tower::Service` trait, with
    /// additional bounds set for convenience *inside* hyper. You don't actually
    /// implement this trait, but `tower::Service<Uri>` instead.
    // The `Sized` bound is to prevent creating `dyn Connect`, since they cannot
    // fit the `Connect` bounds because of the blanket impl for `Service`.
    pub trait Connect: Sealed + Sized {
        #[doc(hidden)]
        type _Svc: ConnectSvc;
        #[doc(hidden)]
        fn connect(self, internal_only: Internal, dst: Uri) -> <Self::_Svc as ConnectSvc>::Future;
    }

    pub trait ConnectSvc {
        type Connection: AsyncRead + AsyncWrite + Connection + Unpin + Send + 'static;
        type Error: Into<Box<dyn StdError + Send + Sync>>;
        type Future: Future<Output = Result<Self::Connection, Self::Error>> + Unpin + Send + 'static;

        fn connect(self, internal_only: Internal, dst: Uri) -> Self::Future;
    }

    impl<S, T> Connect for S
    where
        S: tower_service::Service<Uri, Response = T> + Send + 'static,
        S::Error: Into<Box<dyn StdError + Send + Sync>>,
        S::Future: Unpin + Send,
        T: AsyncRead + AsyncWrite + Connection + Unpin + Send + 'static,
    {
        type _Svc = S;

        fn connect(self, _: Internal, dst: Uri) -> crate::service::Oneshot<S, Uri> {
            crate::service::oneshot(self, dst)
        }
    }

    impl<S, T> ConnectSvc for S
    where
        S: tower_service::Service<Uri, Response = T> + Send + 'static,
        S::Error: Into<Box<dyn StdError + Send + Sync>>,
        S::Future: Unpin + Send,
        T: AsyncRead + AsyncWrite + Connection + Unpin + Send + 'static,
    {
        type Connection = T;
        type Error = S::Error;
        type Future = crate::service::Oneshot<S, Uri>;

        fn connect(self, _: Internal, dst: Uri) -> Self::Future {
            crate::service::oneshot(self, dst)
        }
    }

    impl<S, T> Sealed for S
    where
        S: tower_service::Service<Uri, Response = T> + Send,
        S::Error: Into<Box<dyn StdError + Send + Sync>>,
        S::Future: Unpin + Send,
        T: AsyncRead + AsyncWrite + Connection + Unpin + Send + 'static,
    {
    }

    pub trait Sealed {}
    #[allow(missing_debug_implementations)]
    pub struct Internal;
}

#[cfg(test)]
mod tests {
    use super::Connected;

    #[derive(Clone, Debug, PartialEq)]
    struct Ex1(usize);

    #[derive(Clone, Debug, PartialEq)]
    struct Ex2(&'static str);

    #[derive(Clone, Debug, PartialEq)]
    struct Ex3(&'static str);

    #[test]
    fn test_connected_extra() {
        let c1 = Connected::new().extra(Ex1(41));

        let mut res1 = crate::Response::new(crate::Body::empty());

        assert_eq!(res1.extensions().get::<Ex1>(), None);

        c1.extra.as_ref().expect("c1 extra").set(&mut res1);

        assert_eq!(res1.extensions().get::<Ex1>(), Some(&Ex1(41)));
    }

    #[test]
    fn test_connected_extra_chain() {
        // If a user composes connectors and at each stage, there's "extra"
        // info to attach, it shouldn't override the previous extras.

        let c1 = Connected::new()
            .extra(Ex1(45))
            .extra(Ex2("zoom"))
            .extra(Ex3("pew pew"));

        let mut res1 = crate::Response::new(crate::Body::empty());

        assert_eq!(res1.extensions().get::<Ex1>(), None);
        assert_eq!(res1.extensions().get::<Ex2>(), None);
        assert_eq!(res1.extensions().get::<Ex3>(), None);

        c1.extra.as_ref().expect("c1 extra").set(&mut res1);

        assert_eq!(res1.extensions().get::<Ex1>(), Some(&Ex1(45)));
        assert_eq!(res1.extensions().get::<Ex2>(), Some(&Ex2("zoom")));
        assert_eq!(res1.extensions().get::<Ex3>(), Some(&Ex3("pew pew")));

        // Just like extensions, inserting the same type overrides previous type.
        let c2 = Connected::new()
            .extra(Ex1(33))
            .extra(Ex2("hiccup"))
            .extra(Ex1(99));

        let mut res2 = crate::Response::new(crate::Body::empty());

        c2.extra.as_ref().expect("c2 extra").set(&mut res2);

        assert_eq!(res2.extensions().get::<Ex1>(), Some(&Ex1(99)));
        assert_eq!(res2.extensions().get::<Ex2>(), Some(&Ex2("hiccup")));
    }
}
