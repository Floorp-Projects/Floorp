use std::borrow::Cow;

use time::{Tm, Duration};

use ::{Cookie, SameSite};

/// Structure that follows the builder pattern for building `Cookie` structs.
///
/// To construct a cookie:
///
///   1. Call [`Cookie::build`](struct.Cookie.html#method.build) to start building.
///   2. Use any of the builder methods to set fields in the cookie.
///   3. Call [finish](#method.finish) to retrieve the built cookie.
///
/// # Example
///
/// ```rust
/// # extern crate cookie;
/// extern crate time;
///
/// use cookie::Cookie;
/// use time::Duration;
///
/// # fn main() {
/// let cookie: Cookie = Cookie::build("name", "value")
///     .domain("www.rust-lang.org")
///     .path("/")
///     .secure(true)
///     .http_only(true)
///     .max_age(Duration::days(1))
///     .finish();
/// # }
/// ```
#[derive(Debug, Clone)]
pub struct CookieBuilder {
    /// The cookie being built.
    cookie: Cookie<'static>,
}

impl CookieBuilder {
    /// Creates a new `CookieBuilder` instance from the given name and value.
    ///
    /// This method is typically called indirectly via
    /// [Cookie::build](struct.Cookie.html#method.build).
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::Cookie;
    ///
    /// let c = Cookie::build("foo", "bar").finish();
    /// assert_eq!(c.name_value(), ("foo", "bar"));
    /// ```
    pub fn new<N, V>(name: N, value: V) -> CookieBuilder
        where N: Into<Cow<'static, str>>,
              V: Into<Cow<'static, str>>
    {
        CookieBuilder { cookie: Cookie::new(name, value) }
    }

    /// Sets the `expires` field in the cookie being built.
    ///
    /// # Example
    ///
    /// ```rust
    /// # extern crate cookie;
    /// extern crate time;
    ///
    /// use cookie::Cookie;
    ///
    /// # fn main() {
    /// let c = Cookie::build("foo", "bar")
    ///     .expires(time::now())
    ///     .finish();
    ///
    /// assert!(c.expires().is_some());
    /// # }
    /// ```
    #[inline]
    pub fn expires(mut self, when: Tm) -> CookieBuilder {
        self.cookie.set_expires(when);
        self
    }

    /// Sets the `max_age` field in the cookie being built.
    ///
    /// # Example
    ///
    /// ```rust
    /// # extern crate cookie;
    /// extern crate time;
    /// use time::Duration;
    ///
    /// use cookie::Cookie;
    ///
    /// # fn main() {
    /// let c = Cookie::build("foo", "bar")
    ///     .max_age(Duration::minutes(30))
    ///     .finish();
    ///
    /// assert_eq!(c.max_age(), Some(Duration::seconds(30 * 60)));
    /// # }
    /// ```
    #[inline]
    pub fn max_age(mut self, value: Duration) -> CookieBuilder {
        self.cookie.set_max_age(value);
        self
    }

    /// Sets the `domain` field in the cookie being built.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::Cookie;
    ///
    /// let c = Cookie::build("foo", "bar")
    ///     .domain("www.rust-lang.org")
    ///     .finish();
    ///
    /// assert_eq!(c.domain(), Some("www.rust-lang.org"));
    /// ```
    pub fn domain<D: Into<Cow<'static, str>>>(mut self, value: D) -> CookieBuilder {
        self.cookie.set_domain(value);
        self
    }

    /// Sets the `path` field in the cookie being built.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::Cookie;
    ///
    /// let c = Cookie::build("foo", "bar")
    ///     .path("/")
    ///     .finish();
    ///
    /// assert_eq!(c.path(), Some("/"));
    /// ```
    pub fn path<P: Into<Cow<'static, str>>>(mut self, path: P) -> CookieBuilder {
        self.cookie.set_path(path);
        self
    }

    /// Sets the `secure` field in the cookie being built.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::Cookie;
    ///
    /// let c = Cookie::build("foo", "bar")
    ///     .secure(true)
    ///     .finish();
    ///
    /// assert_eq!(c.secure(), Some(true));
    /// ```
    #[inline]
    pub fn secure(mut self, value: bool) -> CookieBuilder {
        self.cookie.set_secure(value);
        self
    }

    /// Sets the `http_only` field in the cookie being built.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::Cookie;
    ///
    /// let c = Cookie::build("foo", "bar")
    ///     .http_only(true)
    ///     .finish();
    ///
    /// assert_eq!(c.http_only(), Some(true));
    /// ```
    #[inline]
    pub fn http_only(mut self, value: bool) -> CookieBuilder {
        self.cookie.set_http_only(value);
        self
    }

    /// Sets the `same_site` field in the cookie being built.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::{Cookie, SameSite};
    ///
    /// let c = Cookie::build("foo", "bar")
    ///     .same_site(SameSite::Strict)
    ///     .finish();
    ///
    /// assert_eq!(c.same_site(), Some(SameSite::Strict));
    /// ```
    #[inline]
    pub fn same_site(mut self, value: SameSite) -> CookieBuilder {
        self.cookie.set_same_site(value);
        self
    }

    /// Makes the cookie being built 'permanent' by extending its expiration and
    /// max age 20 years into the future.
    ///
    /// # Example
    ///
    /// ```rust
    /// # extern crate cookie;
    /// extern crate time;
    ///
    /// use cookie::Cookie;
    /// use time::Duration;
    ///
    /// # fn main() {
    /// let c = Cookie::build("foo", "bar")
    ///     .permanent()
    ///     .finish();
    ///
    /// assert_eq!(c.max_age(), Some(Duration::days(365 * 20)));
    /// # assert!(c.expires().is_some());
    /// # }
    /// ```
    #[inline]
    pub fn permanent(mut self) -> CookieBuilder {
        self.cookie.make_permanent();
        self
    }

    /// Finishes building and returns the built `Cookie`.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::Cookie;
    ///
    /// let c = Cookie::build("foo", "bar")
    ///     .domain("crates.io")
    ///     .path("/")
    ///     .finish();
    ///
    /// assert_eq!(c.name_value(), ("foo", "bar"));
    /// assert_eq!(c.domain(), Some("crates.io"));
    /// assert_eq!(c.path(), Some("/"));
    /// ```
    #[inline]
    pub fn finish(self) -> Cookie<'static> {
        self.cookie
    }
}
