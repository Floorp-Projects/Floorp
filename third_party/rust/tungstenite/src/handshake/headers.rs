//! HTTP Request and response header handling.

use std::str::from_utf8;
use std::slice;

use httparse;
use httparse::Status;

use error::Result;
use super::machine::TryParse;

/// Limit for the number of header lines.
pub const MAX_HEADERS: usize = 124;

/// HTTP request or response headers.
#[derive(Debug)]
pub struct Headers {
    data: Vec<(String, Box<[u8]>)>,
}

impl Headers {

    /// Get first header with the given name, if any.
    pub fn find_first(&self, name: &str) -> Option<&[u8]> {
        self.find(name).next()
    }

    /// Iterate over all headers with the given name.
    pub fn find<'headers, 'name>(&'headers self, name: &'name str) -> HeadersIter<'name, 'headers> {
        HeadersIter {
            name,
            iter: self.data.iter()
        }
    }

    /// Check if the given header has the given value.
    pub fn header_is(&self, name: &str, value: &str) -> bool {
        self.find_first(name)
            .map(|v| v == value.as_bytes())
            .unwrap_or(false)
    }

    /// Check if the given header has the given value (case-insensitive).
    pub fn header_is_ignore_case(&self, name: &str, value: &str) -> bool {
        self.find_first(name).ok_or(())
            .and_then(|val_raw| from_utf8(val_raw).map_err(|_| ()))
            .map(|val| val.eq_ignore_ascii_case(value))
            .unwrap_or(false)
    }

    /// Allows to iterate over available headers.
    pub fn iter(&self) -> slice::Iter<(String, Box<[u8]>)> {
        self.data.iter()
    }

}

/// The iterator over headers.
#[derive(Debug)]
pub struct HeadersIter<'name, 'headers> {
    name: &'name str,
    iter: slice::Iter<'headers, (String, Box<[u8]>)>,
}

impl<'name, 'headers> Iterator for HeadersIter<'name, 'headers> {
    type Item = &'headers [u8];
    fn next(&mut self) -> Option<Self::Item> {
        while let Some(&(ref name, ref value)) = self.iter.next() {
            if name.eq_ignore_ascii_case(self.name) {
                return Some(value)
            }
        }
        None
    }
}


/// Trait to convert raw objects into HTTP parseables.
pub trait FromHttparse<T>: Sized {
    /// Convert raw object into parsed HTTP headers.
    fn from_httparse(raw: T) -> Result<Self>;
}

impl TryParse for Headers {
    fn try_parse(buf: &[u8]) -> Result<Option<(usize, Self)>> {
        let mut hbuffer = [httparse::EMPTY_HEADER; MAX_HEADERS];
        Ok(match httparse::parse_headers(buf, &mut hbuffer)? {
            Status::Partial => None,
            Status::Complete((size, hdr)) => Some((size, Headers::from_httparse(hdr)?)),
        })
    }
}

impl<'b: 'h, 'h> FromHttparse<&'b [httparse::Header<'h>]> for Headers {
    fn from_httparse(raw: &'b [httparse::Header<'h>]) -> Result<Self> {
        Ok(Headers {
            data: raw.iter()
                     .map(|h| (h.name.into(), Vec::from(h.value).into_boxed_slice()))
                     .collect(),
        })
    }
}

#[cfg(test)]
mod tests {

    use super::Headers;
    use super::super::machine::TryParse;

    #[test]
    fn headers() {
        const DATA: &'static [u8] =
            b"Host: foo.com\r\n\
             Connection: Upgrade\r\n\
             Upgrade: websocket\r\n\
             \r\n";
        let (_, hdr) = Headers::try_parse(DATA).unwrap().unwrap();
        assert_eq!(hdr.find_first("Host"), Some(&b"foo.com"[..]));
        assert_eq!(hdr.find_first("Upgrade"), Some(&b"websocket"[..]));
        assert_eq!(hdr.find_first("Connection"), Some(&b"Upgrade"[..]));

        assert!(hdr.header_is("upgrade", "websocket"));
        assert!(!hdr.header_is("upgrade", "Websocket"));
        assert!(hdr.header_is_ignore_case("upgrade", "Websocket"));
    }

    #[test]
    fn headers_iter() {
        const DATA: &'static [u8] =
            b"Host: foo.com\r\n\
              Sec-WebSocket-Extensions: permessage-deflate\r\n\
              Connection: Upgrade\r\n\
              Sec-WebSocket-ExtenSIONS: permessage-unknown\r\n\
              Upgrade: websocket\r\n\
              \r\n";
        let (_, hdr) = Headers::try_parse(DATA).unwrap().unwrap();
        let mut iter = hdr.find("Sec-WebSocket-Extensions");
        assert_eq!(iter.next(), Some(&b"permessage-deflate"[..]));
        assert_eq!(iter.next(), Some(&b"permessage-unknown"[..]));
        assert_eq!(iter.next(), None);
    }

    #[test]
    fn headers_incomplete() {
        const DATA: &'static [u8] =
            b"Host: foo.com\r\n\
              Connection: Upgrade\r\n\
              Upgrade: websocket\r\n";
        let hdr = Headers::try_parse(DATA).unwrap();
        assert!(hdr.is_none());
    }

}
