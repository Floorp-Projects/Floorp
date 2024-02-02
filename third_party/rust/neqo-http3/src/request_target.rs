// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use std::fmt::{Debug, Formatter};

use url::{ParseError, Url};

pub trait RequestTarget: Debug {
    fn scheme(&self) -> &str;
    fn authority(&self) -> &str;
    fn path(&self) -> &str;
}

pub struct RefRequestTarget<'s, 'a, 'p> {
    scheme: &'s str,
    authority: &'a str,
    path: &'p str,
}

impl RequestTarget for RefRequestTarget<'_, '_, '_> {
    fn scheme(&self) -> &str {
        self.scheme
    }

    fn authority(&self) -> &str {
        self.authority
    }

    fn path(&self) -> &str {
        self.path
    }
}

impl<'s, 'a, 'p> RefRequestTarget<'s, 'a, 'p> {
    #[must_use]
    pub fn new(scheme: &'s str, authority: &'a str, path: &'p str) -> Self {
        Self {
            scheme,
            authority,
            path,
        }
    }
}

impl Debug for RefRequestTarget<'_, '_, '_> {
    fn fmt(&self, f: &mut Formatter) -> ::std::fmt::Result {
        write!(f, "{}://{}{}", self.scheme, self.authority, self.path)
    }
}

/// `AsRequestTarget` is a trait that produces a `RequestTarget` that
/// refers to the identified object.
pub trait AsRequestTarget<'x> {
    type Target: RequestTarget;
    type Error;
    /// Produce a `RequestTarget` that refers to `self`.
    ///
    /// # Errors
    ///
    /// This method can generate an error of type `Self::Error`
    /// if the conversion is unsuccessful.
    fn as_request_target(&'x self) -> Result<Self::Target, Self::Error>;
}

impl<'x, S, A, P> AsRequestTarget<'x> for (S, A, P)
where
    S: AsRef<str> + 'x,
    A: AsRef<str> + 'x,
    P: AsRef<str> + 'x,
{
    type Target = RefRequestTarget<'x, 'x, 'x>;
    type Error = ();
    fn as_request_target(&'x self) -> Result<Self::Target, Self::Error> {
        Ok(RefRequestTarget::new(
            self.0.as_ref(),
            self.1.as_ref(),
            self.2.as_ref(),
        ))
    }
}

impl<'x> AsRequestTarget<'x> for Url {
    type Target = RefRequestTarget<'x, 'x, 'x>;
    type Error = ();
    fn as_request_target(&'x self) -> Result<Self::Target, Self::Error> {
        Ok(RefRequestTarget::new(
            self.scheme(),
            self.host_str().unwrap_or(""),
            self.path(),
        ))
    }
}

pub struct UrlRequestTarget {
    url: Url,
}

impl RequestTarget for UrlRequestTarget {
    fn scheme(&self) -> &str {
        self.url.scheme()
    }

    fn authority(&self) -> &str {
        self.url.host_str().unwrap_or("")
    }

    fn path(&self) -> &str {
        self.url.path()
    }
}

impl Debug for UrlRequestTarget {
    fn fmt(&self, f: &mut Formatter) -> ::std::fmt::Result {
        self.url.fmt(f)
    }
}

impl<'x> AsRequestTarget<'x> for str {
    type Target = UrlRequestTarget;
    type Error = ParseError;
    fn as_request_target(&'x self) -> Result<Self::Target, Self::Error> {
        let url = Url::parse(self)?;
        Ok(UrlRequestTarget { url })
    }
}
