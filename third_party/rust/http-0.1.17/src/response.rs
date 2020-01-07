//! HTTP response types.
//!
//! This module contains structs related to HTTP responses, notably the
//! `Response` type itself as well as a builder to create responses. Typically
//! you'll import the `http::Response` type rather than reaching into this
//! module itself.
//!
//! # Examples
//!
//! Creating a `Response` to return
//!
//! ```
//! use http::{Request, Response, StatusCode};
//!
//! fn respond_to(req: Request<()>) -> http::Result<Response<()>> {
//!     let mut response = Response::builder();
//!     response.header("Foo", "Bar")
//!             .status(StatusCode::OK);
//!
//!     if req.headers().contains_key("Another-Header") {
//!         response.header("Another-Header", "Ack");
//!     }
//!
//!     response.body(())
//! }
//! ```
//!
//! A simple 404 handler
//!
//! ```
//! use http::{Request, Response, StatusCode};
//!
//! fn not_found(_req: Request<()>) -> http::Result<Response<()>> {
//!     Response::builder()
//!         .status(StatusCode::NOT_FOUND)
//!         .body(())
//! }
//! ```
//!
//! Or otherwise inspecting the result of a request:
//!
//! ```no_run
//! use http::{Request, Response};
//!
//! fn get(url: &str) -> http::Result<Response<()>> {
//!     // ...
//! # panic!()
//! }
//!
//! let response = get("https://www.rust-lang.org/").unwrap();
//!
//! if !response.status().is_success() {
//!     panic!("failed to get a successful response status!");
//! }
//!
//! if let Some(date) = response.headers().get("Date") {
//!     // we've got a `Date` header!
//! }
//!
//! let body = response.body();
//! // ...
//! ```

use std::any::Any;
use std::fmt;

use {Error, Result, HttpTryFrom, Extensions};
use header::{HeaderMap, HeaderName, HeaderValue};
use status::StatusCode;
use version::Version;

/// Represents an HTTP response
///
/// An HTTP response consists of a head and a potentially optional body. The body
/// component is generic, enabling arbitrary types to represent the HTTP body.
/// For example, the body could be `Vec<u8>`, a `Stream` of byte chunks, or a
/// value that has been deserialized.
///
/// Typically you'll work with responses on the client side as the result of
/// sending a `Request` and on the server you'll be generating a `Request` to
/// send back to the client.
///
/// # Examples
///
/// Creating a `Response` to return
///
/// ```
/// use http::{Request, Response, StatusCode};
///
/// fn respond_to(req: Request<()>) -> http::Result<Response<()>> {
///     let mut response = Response::builder();
///     response.header("Foo", "Bar")
///             .status(StatusCode::OK);
///
///     if req.headers().contains_key("Another-Header") {
///         response.header("Another-Header", "Ack");
///     }
///
///     response.body(())
/// }
/// ```
///
/// A simple 404 handler
///
/// ```
/// use http::{Request, Response, StatusCode};
///
/// fn not_found(_req: Request<()>) -> http::Result<Response<()>> {
///     Response::builder()
///         .status(StatusCode::NOT_FOUND)
///         .body(())
/// }
/// ```
///
/// Or otherwise inspecting the result of a request:
///
/// ```no_run
/// use http::{Request, Response};
///
/// fn get(url: &str) -> http::Result<Response<()>> {
///     // ...
/// # panic!()
/// }
///
/// let response = get("https://www.rust-lang.org/").unwrap();
///
/// if !response.status().is_success() {
///     panic!("failed to get a successful response status!");
/// }
///
/// if let Some(date) = response.headers().get("Date") {
///     // we've got a `Date` header!
/// }
///
/// let body = response.body();
/// // ...
/// ```
///
/// Deserialize a response of bytes via json:
///
/// ```
/// # extern crate serde;
/// # extern crate serde_json;
/// # extern crate http;
/// use http::Response;
/// use serde::de;
///
/// fn deserialize<T>(req: Response<Vec<u8>>) -> serde_json::Result<Response<T>>
///     where for<'de> T: de::Deserialize<'de>,
/// {
///     let (parts, body) = req.into_parts();
///     let body = serde_json::from_slice(&body)?;
///     Ok(Response::from_parts(parts, body))
/// }
/// #
/// # fn main() {}
/// ```
///
/// Or alternatively, serialize the body of a response to json
///
/// ```
/// # extern crate serde;
/// # extern crate serde_json;
/// # extern crate http;
/// use http::Response;
/// use serde::ser;
///
/// fn serialize<T>(req: Response<T>) -> serde_json::Result<Response<Vec<u8>>>
///     where T: ser::Serialize,
/// {
///     let (parts, body) = req.into_parts();
///     let body = serde_json::to_vec(&body)?;
///     Ok(Response::from_parts(parts, body))
/// }
/// #
/// # fn main() {}
/// ```
pub struct Response<T> {
    head: Parts,
    body: T,
}

/// Component parts of an HTTP `Response`
///
/// The HTTP response head consists of a status, version, and a set of
/// header fields.
pub struct Parts {
    /// The response's status
    pub status: StatusCode,

    /// The response's version
    pub version: Version,

    /// The response's headers
    pub headers: HeaderMap<HeaderValue>,

    /// The response's extensions
    pub extensions: Extensions,

    _priv: (),
}

/// An HTTP response builder
///
/// This type can be used to construct an instance of `Response` through a
/// builder-like pattern.
#[derive(Debug)]
pub struct Builder {
    head: Option<Parts>,
    err: Option<Error>,
}

impl Response<()> {
    /// Creates a new builder-style object to manufacture a `Response`
    ///
    /// This method returns an instance of `Builder` which can be used to
    /// create a `Response`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response = Response::builder()
    ///     .status(200)
    ///     .header("X-Custom-Foo", "Bar")
    ///     .body(())
    ///     .unwrap();
    /// ```
    #[inline]
    pub fn builder() -> Builder {
        Builder::new()
    }
}

impl<T> Response<T> {
    /// Creates a new blank `Response` with the body
    ///
    /// The component ports of this response will be set to their default, e.g.
    /// the ok status, no headers, etc.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response = Response::new("hello world");
    ///
    /// assert_eq!(response.status(), StatusCode::OK);
    /// assert_eq!(*response.body(), "hello world");
    /// ```
    #[inline]
    pub fn new(body: T) -> Response<T> {
        Response {
            head: Parts::new(),
            body: body,
        }
    }

    /// Creates a new `Response` with the given head and body
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response = Response::new("hello world");
    /// let (mut parts, body) = response.into_parts();
    ///
    /// parts.status = StatusCode::BAD_REQUEST;
    /// let response = Response::from_parts(parts, body);
    ///
    /// assert_eq!(response.status(), StatusCode::BAD_REQUEST);
    /// assert_eq!(*response.body(), "hello world");
    /// ```
    #[inline]
    pub fn from_parts(parts: Parts, body: T) -> Response<T> {
        Response {
            head: parts,
            body: body,
        }
    }

    /// Returns the `StatusCode`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response: Response<()> = Response::default();
    /// assert_eq!(response.status(), StatusCode::OK);
    /// ```
    #[inline]
    pub fn status(&self) -> StatusCode {
        self.head.status
    }

    /// Returns a mutable reference to the associated `StatusCode`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let mut response: Response<()> = Response::default();
    /// *response.status_mut() = StatusCode::CREATED;
    /// assert_eq!(response.status(), StatusCode::CREATED);
    /// ```
    #[inline]
    pub fn status_mut(&mut self) -> &mut StatusCode {
        &mut self.head.status
    }

    /// Returns a reference to the associated version.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response: Response<()> = Response::default();
    /// assert_eq!(response.version(), Version::HTTP_11);
    /// ```
    #[inline]
    pub fn version(&self) -> Version {
        self.head.version
    }

    /// Returns a mutable reference to the associated version.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let mut response: Response<()> = Response::default();
    /// *response.version_mut() = Version::HTTP_2;
    /// assert_eq!(response.version(), Version::HTTP_2);
    /// ```
    #[inline]
    pub fn version_mut(&mut self) -> &mut Version {
        &mut self.head.version
    }

    /// Returns a reference to the associated header field map.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response: Response<()> = Response::default();
    /// assert!(response.headers().is_empty());
    /// ```
    #[inline]
    pub fn headers(&self) -> &HeaderMap<HeaderValue> {
        &self.head.headers
    }

    /// Returns a mutable reference to the associated header field map.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// # use http::header::*;
    /// let mut response: Response<()> = Response::default();
    /// response.headers_mut().insert(HOST, HeaderValue::from_static("world"));
    /// assert!(!response.headers().is_empty());
    /// ```
    #[inline]
    pub fn headers_mut(&mut self) -> &mut HeaderMap<HeaderValue> {
        &mut self.head.headers
    }

    /// Returns a reference to the associated extensions.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response: Response<()> = Response::default();
    /// assert!(response.extensions().get::<i32>().is_none());
    /// ```
    #[inline]
    pub fn extensions(&self) -> &Extensions {
        &self.head.extensions
    }

    /// Returns a mutable reference to the associated extensions.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// # use http::header::*;
    /// let mut response: Response<()> = Response::default();
    /// response.extensions_mut().insert("hello");
    /// assert_eq!(response.extensions().get(), Some(&"hello"));
    /// ```
    #[inline]
    pub fn extensions_mut(&mut self) -> &mut Extensions {
        &mut self.head.extensions
    }

    /// Returns a reference to the associated HTTP body.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response: Response<String> = Response::default();
    /// assert!(response.body().is_empty());
    /// ```
    #[inline]
    pub fn body(&self) -> &T {
        &self.body
    }

    /// Returns a mutable reference to the associated HTTP body.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let mut response: Response<String> = Response::default();
    /// response.body_mut().push_str("hello world");
    /// assert!(!response.body().is_empty());
    /// ```
    #[inline]
    pub fn body_mut(&mut self) -> &mut T {
        &mut self.body
    }

    /// Consumes the response, returning just the body.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::Response;
    /// let response = Response::new(10);
    /// let body = response.into_body();
    /// assert_eq!(body, 10);
    /// ```
    #[inline]
    pub fn into_body(self) -> T {
        self.body
    }

    /// Consumes the response returning the head and body parts.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response: Response<()> = Response::default();
    /// let (parts, body) = response.into_parts();
    /// assert_eq!(parts.status, StatusCode::OK);
    /// ```
    #[inline]
    pub fn into_parts(self) -> (Parts, T) {
        (self.head, self.body)
    }

    /// Consumes the response returning a new response with body mapped to the
    /// return type of the passed in function.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// let response = Response::builder().body("some string").unwrap();
    /// let mapped_response: Response<&[u8]> = response.map(|b| {
    ///   assert_eq!(b, "some string");
    ///   b.as_bytes()
    /// });
    /// assert_eq!(mapped_response.body(), &"some string".as_bytes());
    /// ```
    #[inline]
    pub fn map<F, U>(self, f: F) -> Response<U>
        where F: FnOnce(T) -> U
    {
        Response { body: f(self.body), head: self.head }
    }
}

impl<T: Default> Default for Response<T> {
    #[inline]
    fn default() -> Response<T> {
        Response::new(T::default())
    }
}

impl<T: fmt::Debug> fmt::Debug for Response<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Response")
            .field("status", &self.status())
            .field("version", &self.version())
            .field("headers", self.headers())
            // omits Extensions because not useful
            .field("body", self.body())
            .finish()
    }
}

impl Parts {
    /// Creates a new default instance of `Parts`
    fn new() -> Parts {
        Parts{
            status: StatusCode::default(),
            version: Version::default(),
            headers: HeaderMap::default(),
            extensions: Extensions::default(),
            _priv: (),
        }
    }
}

impl fmt::Debug for Parts {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Parts")
            .field("status", &self.status)
            .field("version", &self.version)
            .field("headers", &self.headers)
            // omits Extensions because not useful
            // omits _priv because not useful
            .finish()
    }
}

impl Builder {
    /// Creates a new default instance of `Builder` to construct either a
    /// `Head` or a `Response`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    ///
    /// let response = response::Builder::new()
    ///     .status(200)
    ///     .body(())
    ///     .unwrap();
    /// ```
    #[inline]
    pub fn new() -> Builder {
        Builder::default()
    }

    /// Set the HTTP status for this response.
    ///
    /// This function will configure the HTTP status code of the `Response` that
    /// will be returned from `Builder::build`.
    ///
    /// By default this is `200`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    ///
    /// let response = Response::builder()
    ///     .status(200)
    ///     .body(())
    ///     .unwrap();
    /// ```
    pub fn status<T>(&mut self, status: T) -> &mut Builder
        where StatusCode: HttpTryFrom<T>,
    {
        if let Some(head) = head(&mut self.head, &self.err) {
            match HttpTryFrom::try_from(status) {
                Ok(s) => head.status = s,
                Err(e) => self.err = Some(e.into()),
            }
        }
        self
    }

    /// Set the HTTP version for this response.
    ///
    /// This function will configure the HTTP version of the `Response` that
    /// will be returned from `Builder::build`.
    ///
    /// By default this is HTTP/1.1
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    ///
    /// let response = Response::builder()
    ///     .version(Version::HTTP_2)
    ///     .body(())
    ///     .unwrap();
    /// ```
    pub fn version(&mut self, version: Version) -> &mut Builder {
        if let Some(head) = head(&mut self.head, &self.err) {
            head.version = version;
        }
        self
    }

    /// Appends a header to this response builder.
    ///
    /// This function will append the provided key/value as a header to the
    /// internal `HeaderMap` being constructed. Essentially this is equivalent
    /// to calling `HeaderMap::append`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    /// # use http::header::HeaderValue;
    ///
    /// let response = Response::builder()
    ///     .header("Content-Type", "text/html")
    ///     .header("X-Custom-Foo", "bar")
    ///     .header("content-length", 0)
    ///     .body(())
    ///     .unwrap();
    /// ```
    pub fn header<K, V>(&mut self, key: K, value: V) -> &mut Builder
        where HeaderName: HttpTryFrom<K>,
              HeaderValue: HttpTryFrom<V>
    {
        if let Some(head) = head(&mut self.head, &self.err) {
            match <HeaderName as HttpTryFrom<K>>::try_from(key) {
                Ok(key) => {
                    match <HeaderValue as HttpTryFrom<V>>::try_from(value) {
                        Ok(value) => { head.headers.append(key, value); }
                        Err(e) => self.err = Some(e.into()),
                    }
                },
                Err(e) => self.err = Some(e.into()),
            };
        }
        self
    }

    /// Get header on this response builder.
    /// when builder has error returns None
    /// 
    /// # Example
    /// 
    /// ```
    /// # use http::*;
    /// # use http::header::HeaderValue;
    /// # use http::response::Builder;
    /// let mut res = Response::builder();
    /// res.header("Accept", "text/html")
    ///    .header("X-Custom-Foo", "bar");
    /// let headers = res.headers_ref().unwrap();
    /// assert_eq!( headers["Accept"], "text/html" );
    /// assert_eq!( headers["X-Custom-Foo"], "bar" );
    /// ```
    pub fn headers_ref(&self) -> Option<&HeaderMap<HeaderValue>> {
        if self.err.is_some() {
            return None;
        }
        match self.head
        {
            Some(ref head) => Some(&head.headers),
            None => None
        }
    }

    /// Get header on this response builder.
    /// when builder has error returns None
    /// 
    /// # Example
    /// 
    /// ```
    /// # use http::*;
    /// # use http::header::HeaderValue;
    /// # use http::response::Builder;
    /// let mut res = Response::builder();
    /// {
    ///   let headers = res.headers_mut().unwrap();
    ///   headers.insert("Accept", HeaderValue::from_static("text/html"));
    ///   headers.insert("X-Custom-Foo", HeaderValue::from_static("bar"));
    /// }
    /// let headers = res.headers_ref().unwrap();
    /// assert_eq!( headers["Accept"], "text/html" );
    /// assert_eq!( headers["X-Custom-Foo"], "bar" );
    /// ```
    pub fn headers_mut(&mut self) -> Option<&mut HeaderMap<HeaderValue>> {
        if self.err.is_some() {
            return None;
        }
        match self.head
        {
            Some(ref mut head) => Some(&mut head.headers),
            None => None
        }
    }

    /// Adds an extension to this builder
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    ///
    /// let response = Response::builder()
    ///     .extension("My Extension")
    ///     .body(())
    ///     .unwrap();
    ///
    /// assert_eq!(response.extensions().get::<&'static str>(),
    ///            Some(&"My Extension"));
    /// ```
    pub fn extension<T>(&mut self, extension: T) -> &mut Builder
        where T: Any + Send + Sync + 'static,
    {
        if let Some(head) = head(&mut self.head, &self.err) {
            head.extensions.insert(extension);
        }
        self
    }

    fn take_parts(&mut self) -> Result<Parts> {
        let ret = self.head.take().expect("cannot reuse response builder");
        if let Some(e) = self.err.take() {
            return Err(e)
        }
        Ok(ret)
    }

    /// "Consumes" this builder, using the provided `body` to return a
    /// constructed `Response`.
    ///
    /// # Errors
    ///
    /// This function may return an error if any previously configured argument
    /// failed to parse or get converted to the internal representation. For
    /// example if an invalid `head` was specified via `header("Foo",
    /// "Bar\r\n")` the error will be returned when this function is called
    /// rather than when `header` was called.
    ///
    /// # Panics
    ///
    /// This method will panic if the builder is reused. The `body` function can
    /// only be called once.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::*;
    ///
    /// let response = Response::builder()
    ///     .body(())
    ///     .unwrap();
    /// ```
    pub fn body<T>(&mut self, body: T) -> Result<Response<T>> {
        Ok(Response {
            head: self.take_parts()?,
            body: body,
        })
    }
}

fn head<'a>(head: &'a mut Option<Parts>, err: &Option<Error>)
    -> Option<&'a mut Parts>
{
    if err.is_some() {
        return None
    }
    head.as_mut()
}

impl Default for Builder {
    #[inline]
    fn default() -> Builder {
        Builder {
            head: Some(Parts::new()),
            err: None,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_can_map_a_body_from_one_type_to_another() {
        let response = Response::builder().body("some string").unwrap();
        let mapped_response = response.map(|s| {
            assert_eq!(s, "some string");
            123u32
        });
        assert_eq!(mapped_response.body(), &123u32);
    }
}
