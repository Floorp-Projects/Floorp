// Deprecated in 1.26, needed until our minimum version is >=1.23.
#[allow(unused, deprecated)]
use std::ascii::AsciiExt;
use std::{cmp, fmt, str};
use std::hash::{Hash, Hasher};
use std::str::FromStr;

use bytes::Bytes;

use byte_str::ByteStr;
use convert::HttpTryFrom;
use super::{ErrorKind, InvalidUri, InvalidUriBytes, URI_CHARS, Port};

/// Represents the authority component of a URI.
#[derive(Clone)]
pub struct Authority {
    pub(super) data: ByteStr,
}

impl Authority {
    pub(super) fn empty() -> Self {
        Authority { data: ByteStr::new() }
    }

    /// Attempt to convert an `Authority` from `Bytes`.
    ///
    /// This function will be replaced by a `TryFrom` implementation once the
    /// trait lands in stable.
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate http;
    /// # use http::uri::*;
    /// extern crate bytes;
    ///
    /// use bytes::Bytes;
    ///
    /// # pub fn main() {
    /// let bytes = Bytes::from("example.com");
    /// let authority = Authority::from_shared(bytes).unwrap();
    ///
    /// assert_eq!(authority.host(), "example.com");
    /// # }
    /// ```
    pub fn from_shared(s: Bytes) -> Result<Self, InvalidUriBytes> {
        let authority_end = Authority::parse_non_empty(&s[..]).map_err(InvalidUriBytes)?;

        if authority_end != s.len() {
            return Err(ErrorKind::InvalidUriChar.into());
        }

        Ok(Authority {
            data: unsafe { ByteStr::from_utf8_unchecked(s) },
        })
    }

    /// Attempt to convert an `Authority` from a static string.
    ///
    /// This function will not perform any copying, and the string will be
    /// checked if it is empty or contains an invalid character.
    ///
    /// # Panics
    ///
    /// This function panics if the argument contains invalid characters or
    /// is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::uri::Authority;
    /// let authority = Authority::from_static("example.com");
    /// assert_eq!(authority.host(), "example.com");
    /// ```
    pub fn from_static(src: &'static str) -> Self {
        let s = src.as_bytes();
        let b = Bytes::from_static(s);
        let authority_end = Authority::parse_non_empty(&b[..]).expect("static str is not valid authority");

        if authority_end != b.len() {
            panic!("static str is not valid authority");
        }

        Authority {
            data: unsafe { ByteStr::from_utf8_unchecked(b) },
        }
    }

    // Note: this may return an *empty* Authority. You might want `parse_non_empty`.
    pub(super) fn parse(s: &[u8]) -> Result<usize, InvalidUri> {
        let mut colon_cnt = 0;
        let mut start_bracket = false;
        let mut end_bracket = false;
        let mut has_percent = false;
        let mut end = s.len();
        let mut at_sign_pos = None;

        for (i, &b) in s.iter().enumerate() {
            match URI_CHARS[b as usize] {
                b'/' | b'?' | b'#' => {
                    end = i;
                    break;
                }
                b':' => {
                    colon_cnt += 1;
                },
                b'[' => {
                    start_bracket = true;
                }
                b']' => {
                    end_bracket = true;

                    // Those were part of an IPv6 hostname, so forget them...
                    colon_cnt = 0;
                }
                b'@' => {
                    at_sign_pos = Some(i);

                    // Those weren't a port colon, but part of the
                    // userinfo, so it needs to be forgotten.
                    colon_cnt = 0;
                    has_percent = false;
                }
                0 if b == b'%' => {
                    // Per https://tools.ietf.org/html/rfc3986#section-3.2.1 and
                    // https://url.spec.whatwg.org/#authority-state
                    // the userinfo can have a percent-encoded username and password,
                    // so record that a `%` was found. If this turns out to be
                    // part of the userinfo, this flag will be cleared.
                    // If the flag hasn't been cleared at the end, that means this
                    // was part of the hostname, and will fail with an error.
                    has_percent = true;
                }
                0 => {
                    return Err(ErrorKind::InvalidUriChar.into());
                }
                _ => {}
            }
        }

        if start_bracket ^ end_bracket {
            return Err(ErrorKind::InvalidAuthority.into());
        }

        if colon_cnt > 1 {
            // Things like 'localhost:8080:3030' are rejected.
            return Err(ErrorKind::InvalidAuthority.into());
        }

        if end > 0 && at_sign_pos == Some(end - 1) {
            // If there's nothing after an `@`, this is bonkers.
            return Err(ErrorKind::InvalidAuthority.into());
        }

        if has_percent {
            // Something after the userinfo has a `%`, so reject it.
            return Err(ErrorKind::InvalidAuthority.into());
        }

        Ok(end)
    }

    // Parse bytes as an Authority, not allowing an empty string.
    //
    // This should be used by functions that allow a user to parse
    // an `Authority` by itself.
    fn parse_non_empty(s: &[u8]) -> Result<usize, InvalidUri> {
        if s.is_empty() {
            return Err(ErrorKind::Empty.into());
        }
        Authority::parse(s)
    }

    /// Get the host of this `Authority`.
    ///
    /// The host subcomponent of authority is identified by an IP literal
    /// encapsulated within square brackets, an IPv4 address in dotted- decimal
    /// form, or a registered name.  The host subcomponent is **case-insensitive**.
    ///
    /// ```notrust
    /// abc://username:password@example.com:123/path/data?key=value&key2=value2#fragid1
    ///                         |---------|
    ///                              |
    ///                             host
    /// ```
    ///
    /// # Examples
    ///
    /// ```
    /// # use http::uri::*;
    /// let authority: Authority = "example.org:80".parse().unwrap();
    ///
    /// assert_eq!(authority.host(), "example.org");
    /// ```
    #[inline]
    pub fn host(&self) -> &str {
        host(self.as_str())
    }

    #[deprecated(since="0.1.14", note="use `port_part` or `port_u16` instead")]
    #[doc(hidden)]
    pub fn port(&self) -> Option<u16> {
        self.port_u16()
    }

    /// Get the port part of this `Authority`.
    ///
    /// The port subcomponent of authority is designated by an optional port
    /// number following the host and delimited from it by a single colon (":")
    /// character. It can be turned into a decimal port number with the `as_u16`
    /// method or as a `str` with the `as_str` method.
    ///
    /// ```notrust
    /// abc://username:password@example.com:123/path/data?key=value&key2=value2#fragid1
    ///                                     |-|
    ///                                      |
    ///                                     port
    /// ```
    ///
    /// # Examples
    ///
    /// Authority with port
    ///
    /// ```
    /// # use http::uri::Authority;
    /// let authority: Authority = "example.org:80".parse().unwrap();
    ///
    /// let port = authority.port_part().unwrap();
    /// assert_eq!(port.as_u16(), 80);
    /// assert_eq!(port.as_str(), "80");
    /// ```
    ///
    /// Authority without port
    ///
    /// ```
    /// # use http::uri::Authority;
    /// let authority: Authority = "example.org".parse().unwrap();
    ///
    /// assert!(authority.port_part().is_none());
    /// ```
    pub fn port_part(&self) -> Option<Port<&str>> {
        let bytes = self.as_str();
        bytes
            .rfind(":")
            .and_then(|i| Port::from_str(&bytes[i + 1..]).ok())
    }

    /// Get the port of this `Authority` as a `u16`.
    ///
    /// # Example
    ///
    /// ```
    /// # use http::uri::Authority;
    /// let authority: Authority = "example.org:80".parse().unwrap();
    ///
    /// assert_eq!(authority.port_u16(), Some(80));
    /// ```
    pub fn port_u16(&self) -> Option<u16> {
        self.port_part().and_then(|p| Some(p.as_u16()))
    }

    /// Return a str representation of the authority
    #[inline]
    pub fn as_str(&self) -> &str {
        &self.data[..]
    }

    /// Converts this `Authority` back to a sequence of bytes
    #[inline]
    pub fn into_bytes(self) -> Bytes {
        self.into()
    }
}

impl AsRef<str> for Authority {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl PartialEq for Authority {
    fn eq(&self, other: &Authority) -> bool {
        self.data.eq_ignore_ascii_case(&other.data)
    }
}

impl Eq for Authority {}

/// Case-insensitive equality
///
/// # Examples
///
/// ```
/// # use http::uri::Authority;
/// let authority: Authority = "HELLO.com".parse().unwrap();
/// assert_eq!(authority, "hello.coM");
/// assert_eq!("hello.com", authority);
/// ```
impl PartialEq<str> for Authority {
    fn eq(&self, other: &str) -> bool {
        self.data.eq_ignore_ascii_case(other)
    }
}

impl PartialEq<Authority> for str {
    fn eq(&self, other: &Authority) -> bool {
        self.eq_ignore_ascii_case(other.as_str())
    }
}

impl<'a> PartialEq<Authority> for &'a str {
    fn eq(&self, other: &Authority) -> bool {
        self.eq_ignore_ascii_case(other.as_str())
    }
}

impl<'a> PartialEq<&'a str> for Authority {
    fn eq(&self, other: &&'a str) -> bool {
        self.data.eq_ignore_ascii_case(other)
    }
}

impl PartialEq<String> for Authority {
    fn eq(&self, other: &String) -> bool {
        self.data.eq_ignore_ascii_case(other.as_str())
    }
}

impl PartialEq<Authority> for String {
    fn eq(&self, other: &Authority) -> bool {
        self.as_str().eq_ignore_ascii_case(other.as_str())
    }
}

/// Case-insensitive ordering
///
/// # Examples
///
/// ```
/// # use http::uri::Authority;
/// let authority: Authority = "DEF.com".parse().unwrap();
/// assert!(authority < "ghi.com");
/// assert!(authority > "abc.com");
/// ```
impl PartialOrd for Authority {
    fn partial_cmp(&self, other: &Authority) -> Option<cmp::Ordering> {
        let left = self.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        let right = other.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        left.partial_cmp(right)
    }
}

impl PartialOrd<str> for Authority {
    fn partial_cmp(&self, other: &str) -> Option<cmp::Ordering> {
        let left = self.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        let right = other.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        left.partial_cmp(right)
    }
}

impl PartialOrd<Authority> for str {
    fn partial_cmp(&self, other: &Authority) -> Option<cmp::Ordering> {
        let left = self.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        let right = other.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        left.partial_cmp(right)
    }
}

impl<'a> PartialOrd<Authority> for &'a str {
    fn partial_cmp(&self, other: &Authority) -> Option<cmp::Ordering> {
        let left = self.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        let right = other.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        left.partial_cmp(right)
    }
}

impl<'a> PartialOrd<&'a str> for Authority {
    fn partial_cmp(&self, other: &&'a str) -> Option<cmp::Ordering> {
        let left = self.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        let right = other.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        left.partial_cmp(right)
    }
}

impl PartialOrd<String> for Authority {
    fn partial_cmp(&self, other: &String) -> Option<cmp::Ordering> {
        let left = self.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        let right = other.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        left.partial_cmp(right)
    }
}

impl PartialOrd<Authority> for String {
    fn partial_cmp(&self, other: &Authority) -> Option<cmp::Ordering> {
        let left = self.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        let right = other.data.as_bytes().iter().map(|b| b.to_ascii_lowercase());
        left.partial_cmp(right)
    }
}

/// Case-insensitive hashing
///
/// # Examples
///
/// ```
/// # use http::uri::Authority;
/// # use std::hash::{Hash, Hasher};
/// # use std::collections::hash_map::DefaultHasher;
///
/// let a: Authority = "HELLO.com".parse().unwrap();
/// let b: Authority = "hello.coM".parse().unwrap();
///
/// let mut s = DefaultHasher::new();
/// a.hash(&mut s);
/// let a = s.finish();
///
/// let mut s = DefaultHasher::new();
/// b.hash(&mut s);
/// let b = s.finish();
///
/// assert_eq!(a, b);
/// ```
impl Hash for Authority {
    fn hash<H>(&self, state: &mut H) where H: Hasher {
        self.data.len().hash(state);
        for &b in self.data.as_bytes() {
            state.write_u8(b.to_ascii_lowercase());
        }
    }
}

impl HttpTryFrom<Bytes> for Authority {
    type Error = InvalidUriBytes;
    #[inline]
    fn try_from(bytes: Bytes) -> Result<Self, Self::Error> {
        Authority::from_shared(bytes)
    }
}

impl<'a> HttpTryFrom<&'a [u8]> for Authority {
    type Error = InvalidUri;
    #[inline]
    fn try_from(s: &'a [u8]) -> Result<Self, Self::Error> {
        // parse first, and only turn into Bytes if valid
        let end = Authority::parse_non_empty(s)?;

        if end != s.len() {
            return Err(ErrorKind::InvalidAuthority.into());
        }

        Ok(Authority {
            data: unsafe { ByteStr::from_utf8_unchecked(s.into()) },
        })
    }
}

impl<'a> HttpTryFrom<&'a str> for Authority {
    type Error = InvalidUri;
    #[inline]
    fn try_from(s: &'a str) -> Result<Self, Self::Error> {
        HttpTryFrom::try_from(s.as_bytes())
    }
}

impl FromStr for Authority {
    type Err = InvalidUri;

    fn from_str(s: &str) -> Result<Self, InvalidUri> {
        HttpTryFrom::try_from(s)
    }
}

impl From<Authority> for Bytes {
    #[inline]
    fn from(src: Authority) -> Bytes {
        src.data.into()
    }
}

impl fmt::Debug for Authority {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl fmt::Display for Authority {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

fn host(auth: &str) -> &str {
    let host_port = auth.rsplitn(2, '@')
        .next()
        .expect("split always has at least 1 item");

    if host_port.as_bytes()[0] == b'[' {
        let i = host_port.find(']')
            .expect("parsing should validate brackets");
        // ..= ranges aren't available in 1.20, our minimum Rust version...
        &host_port[0 .. i + 1]
    } else {
        host_port.split(':')
            .next()
            .expect("split always has at least 1 item")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_empty_string_is_error() {
        let err = Authority::parse_non_empty(b"").unwrap_err();
        assert_eq!(err.0, ErrorKind::Empty);
    }

    #[test]
    fn equal_to_self_of_same_authority() {
        let authority1: Authority = "example.com".parse().unwrap();
        let authority2: Authority = "EXAMPLE.COM".parse().unwrap();
        assert_eq!(authority1, authority2);
        assert_eq!(authority2, authority1);
    }

    #[test]
    fn not_equal_to_self_of_different_authority() {
        let authority1: Authority = "example.com".parse().unwrap();
        let authority2: Authority = "test.com".parse().unwrap();
        assert_ne!(authority1, authority2);
        assert_ne!(authority2, authority1);
    }

    #[test]
    fn equates_with_a_str() {
        let authority: Authority = "example.com".parse().unwrap();
        assert_eq!(&authority, "EXAMPLE.com");
        assert_eq!("EXAMPLE.com", &authority);
        assert_eq!(authority, "EXAMPLE.com");
        assert_eq!("EXAMPLE.com", authority);
    }

    #[test]
    fn not_equal_with_a_str_of_a_different_authority() {
        let authority: Authority = "example.com".parse().unwrap();
        assert_ne!(&authority, "test.com");
        assert_ne!("test.com", &authority);
        assert_ne!(authority, "test.com");
        assert_ne!("test.com", authority);
    }

    #[test]
    fn equates_with_a_string() {
        let authority: Authority = "example.com".parse().unwrap();
        assert_eq!(authority, "EXAMPLE.com".to_string());
        assert_eq!("EXAMPLE.com".to_string(), authority);
    }

    #[test]
    fn equates_with_a_string_of_a_different_authority() {
        let authority: Authority = "example.com".parse().unwrap();
        assert_ne!(authority, "test.com".to_string());
        assert_ne!("test.com".to_string(), authority);
    }

    #[test]
    fn compares_to_self() {
        let authority1: Authority = "abc.com".parse().unwrap();
        let authority2: Authority = "def.com".parse().unwrap();
        assert!(authority1 < authority2);
        assert!(authority2 > authority1);
    }

    #[test]
    fn compares_with_a_str() {
        let authority: Authority = "def.com".parse().unwrap();
        // with ref
        assert!(&authority < "ghi.com");
        assert!("ghi.com" > &authority);
        assert!(&authority > "abc.com");
        assert!("abc.com" < &authority);

        // no ref
        assert!(authority < "ghi.com");
        assert!("ghi.com" > authority);
        assert!(authority > "abc.com");
        assert!("abc.com" < authority);
    }

    #[test]
    fn compares_with_a_string() {
        let authority: Authority = "def.com".parse().unwrap();
        assert!(authority < "ghi.com".to_string());
        assert!("ghi.com".to_string() > authority);
        assert!(authority > "abc.com".to_string());
        assert!("abc.com".to_string() < authority);
    }

    #[test]
    fn allows_percent_in_userinfo() {
        let authority_str = "a%2f:b%2f@example.com";
        let authority: Authority = authority_str.parse().unwrap();
        assert_eq!(authority, authority_str);
    }

    #[test]
    fn rejects_percent_in_hostname() {
        let err = Authority::parse_non_empty(b"example%2f.com").unwrap_err();
        assert_eq!(err.0, ErrorKind::InvalidAuthority);

        let err = Authority::parse_non_empty(b"a%2f:b%2f@example%2f.com").unwrap_err();
        assert_eq!(err.0, ErrorKind::InvalidAuthority);
    }
}
