use std::fmt;

use bytes::Bytes;
use http::uri::Authority;

/// The `Host` header.
#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd)]
pub struct Host(Authority);

impl Host {
    /// Get the hostname, such as example.domain.
    pub fn hostname(&self) -> &str {
        self.0.host()
    }

    /// Get the optional port number.
    pub fn port(&self) -> Option<u16> {
        self.0.port_part().map(|p| p.as_u16())
    }
}

impl ::Header for Host {
    fn name() -> &'static ::HeaderName {
        &::http::header::HOST
    }

    fn decode<'i, I: Iterator<Item = &'i ::HeaderValue>>(values: &mut I) -> Result<Self, ::Error> {
        values
            .next()
            .cloned()
            .map(Bytes::from)
            .and_then(|bytes| Authority::from_shared(bytes).ok())
            .map(Host)
            .ok_or_else(::Error::invalid)
    }

    fn encode<E: Extend<::HeaderValue>>(&self, values: &mut E) {
        let bytes = Bytes::from(self.0.clone());

        let val = ::HeaderValue::from_shared(bytes)
            .expect("Authority is a valid HeaderValue");

        values.extend(::std::iter::once(val));
    }
}

impl From<Authority> for Host {
    fn from(auth: Authority) -> Host {
        Host(auth)
    }
}

impl fmt::Display for Host {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.0, f)
    }
}

