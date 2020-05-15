use crate::bewit::Bewit;
use crate::credentials::{Credentials, Key};
use crate::error::*;
use crate::header::Header;
use crate::mac::{Mac, MacType};
use crate::response::ResponseBuilder;
use log::debug;
use std::borrow::Cow;
use std::str;
use std::str::FromStr;
use std::time::{Duration, SystemTime};
use url::{Position, Url};

/// Request represents a single HTTP request.
///
/// The structure is created using (RequestBuilder)[struct.RequestBuilder.html]. Most uses of this
/// library will hold several of the fields in this structure fixed. Cloning the structure with
/// these fields applied is a convenient way to avoid repeating those fields. Most fields are
/// references, since in common use the values already exist and will outlive the request.
///
/// A request can be used on the client, to generate a header or a bewit, or on the server, to
/// validate the same.
///
/// # Examples
///
/// ```
/// use hawk::RequestBuilder;
/// let bldr = RequestBuilder::new("GET", "mysite.com", 443, "/");
/// let request1 = bldr.clone().method("POST").path("/api/user").request();
/// let request2 = bldr.path("/api/users").request();
/// ```
///
/// See the documentation in the crate root for examples of creating and validating headers.
#[derive(Debug, Clone)]
pub struct Request<'a> {
    method: &'a str,
    host: &'a str,
    port: u16,
    path: Cow<'a, str>,
    hash: Option<&'a [u8]>,
    ext: Option<&'a str>,
    app: Option<&'a str>,
    dlg: Option<&'a str>,
}

impl<'a> Request<'a> {
    /// Create a new Header for this request, inventing a new nonce and setting the
    /// timestamp to the current time.
    pub fn make_header(&self, credentials: &Credentials) -> Result<Header> {
        let nonce = random_string(10)?;
        self.make_header_full(credentials, SystemTime::now(), nonce)
    }

    /// Similar to `make_header`, but allowing specification of the timestamp
    /// and nonce.
    pub fn make_header_full<S>(
        &self,
        credentials: &Credentials,
        ts: SystemTime,
        nonce: S,
    ) -> Result<Header>
    where
        S: Into<String>,
    {
        let nonce = nonce.into();
        let mac = Mac::new(
            MacType::Header,
            &credentials.key,
            ts,
            &nonce,
            self.method,
            self.host,
            self.port,
            self.path.as_ref(),
            self.hash,
            self.ext,
        )?;
        Header::new(
            Some(credentials.id.clone()),
            Some(ts),
            Some(nonce),
            Some(mac),
            match self.ext {
                None => None,
                Some(v) => Some(v.to_string()),
            },
            match self.hash {
                None => None,
                Some(v) => Some(v.to_vec()),
            },
            match self.app {
                None => None,
                Some(v) => Some(v.to_string()),
            },
            match self.dlg {
                None => None,
                Some(v) => Some(v.to_string()),
            },
        )
    }

    /// Make a "bewit" that can be attached to a URL to authenticate GET access.
    ///
    /// The ttl gives the time for which this bewit is valid, starting now.
    pub fn make_bewit(&self, credentials: &'a Credentials, exp: SystemTime) -> Result<Bewit<'a>> {
        // note that this includes `method` and `hash` even though they must always be GET and None
        // for bewits.  If they aren't, then the bewit just won't validate -- no need to catch
        // that now
        let mac = Mac::new(
            MacType::Bewit,
            &credentials.key,
            exp,
            "",
            self.method,
            self.host,
            self.port,
            self.path.as_ref(),
            self.hash,
            self.ext,
        )?;
        let bewit = Bewit::new(&credentials.id, exp, mac, self.ext);
        Ok(bewit)
    }

    /// Variant of `make_bewit` that takes a Duration (starting from now)
    /// instead of a SystemTime, provided for convenience.
    pub fn make_bewit_with_ttl(
        &self,
        credentials: &'a Credentials,
        ttl: Duration,
    ) -> Result<Bewit<'a>> {
        let exp = SystemTime::now() + ttl;
        self.make_bewit(credentials, exp)
    }

    /// Validate the given header.  This validates that the `mac` field matches that calculated
    /// using the other header fields and the given request information.
    ///
    /// The header's timestamp is verified to be within `ts_skew` of the current time.  If any of
    /// the required header fields are missing, the method will return false.
    ///
    /// It is up to the caller to examine the header's `id` field and supply the corresponding key.
    ///
    /// If desired, it is up to the caller to validate that `nonce` has not been used before.
    ///
    /// If a hash has been supplied, then the header must contain a matching hash. Note that this
    /// hash must be calculated based on the request body, not copied from the request header!
    pub fn validate_header(&self, header: &Header, key: &Key, ts_skew: Duration) -> bool {
        // extract required fields, returning early if they are not present
        let ts = match header.ts {
            Some(ts) => ts,
            None => {
                debug!("missing timestamp from header");
                return false;
            }
        };
        let nonce = match header.nonce {
            Some(ref nonce) => nonce,
            None => {
                debug!("missing nonce from header");
                return false;
            }
        };
        let header_mac = match header.mac {
            Some(ref mac) => mac,
            None => {
                debug!("missing mac from header");
                return false;
            }
        };
        let header_hash = match header.hash {
            Some(ref hash) => Some(&hash[..]),
            None => None,
        };
        let header_ext = match header.ext {
            Some(ref ext) => Some(&ext[..]),
            None => None,
        };

        // first verify the MAC
        match Mac::new(
            MacType::Header,
            key,
            ts,
            nonce,
            self.method,
            self.host,
            self.port,
            self.path.as_ref(),
            header_hash,
            header_ext,
        ) {
            Ok(calculated_mac) => {
                if &calculated_mac != header_mac {
                    debug!("calculated mac doesn't match header");
                    return false;
                }
            }
            Err(e) => {
                debug!("unexpected mac error: {:?}", e);
                return false;
            }
        };

        // ..then the hashes
        if let Some(local_hash) = self.hash {
            if let Some(server_hash) = header_hash {
                if local_hash != server_hash {
                    debug!("server hash doesn't match header");
                    return false;
                }
            } else {
                debug!("missing hash from header");
                return false;
            }
        }

        // ..then the timestamp
        let now = SystemTime::now();
        let skew = if now > ts {
            now.duration_since(ts).unwrap()
        } else {
            ts.duration_since(now).unwrap()
        };
        if skew > ts_skew {
            debug!(
                "bad timestamp skew, timestamp too old? detected skew: {:?}, ts_skew: {:?}",
                &skew, &ts_skew
            );
            return false;
        }

        true
    }

    /// Validate the given bewit matches this request.
    ///
    /// It is up to the caller to consult the Bewit's `id` and look up the
    /// corresponding key.
    ///
    /// Nonces and hashes do not apply when using bewits.
    pub fn validate_bewit(&self, bewit: &Bewit, key: &Key) -> bool {
        let calculated_mac = Mac::new(
            MacType::Bewit,
            key,
            bewit.exp(),
            "",
            self.method,
            self.host,
            self.port,
            self.path.as_ref(),
            self.hash,
            match bewit.ext() {
                Some(e) => Some(e),
                None => None,
            },
        );
        let calculated_mac = match calculated_mac {
            Ok(m) => m,
            Err(_) => {
                return false;
            }
        };

        if bewit.mac() != &calculated_mac {
            return false;
        }

        let now = SystemTime::now();
        if bewit.exp() < now {
            return false;
        }

        true
    }

    /// Get a Response instance for a response to this request.  This is a convenience
    /// wrapper around `Response::from_request_header`.
    pub fn make_response_builder(&'a self, req_header: &'a Header) -> ResponseBuilder<'a> {
        ResponseBuilder::from_request_header(
            req_header,
            self.method,
            self.host,
            self.port,
            self.path.as_ref(),
        )
    }
}

#[derive(Debug, Clone)]
pub struct RequestBuilder<'a>(Request<'a>);

impl<'a> RequestBuilder<'a> {
    /// Create a new request with the given method, host, port, and path.
    pub fn new(method: &'a str, host: &'a str, port: u16, path: &'a str) -> Self {
        RequestBuilder(Request {
            method,
            host,
            port,
            path: Cow::Borrowed(path),
            hash: None,
            ext: None,
            app: None,
            dlg: None,
        })
    }

    /// Create a new request with the host, port, and path determined from the URL.
    pub fn from_url(method: &'a str, url: &'a Url) -> Result<Self> {
        let (host, port, path) = RequestBuilder::parse_url(url)?;
        Ok(RequestBuilder(Request {
            method,
            host,
            port,
            path: Cow::Borrowed(path),
            hash: None,
            ext: None,
            app: None,
            dlg: None,
        }))
    }

    /// Set the request method. This should be a capitalized string.
    pub fn method(mut self, method: &'a str) -> Self {
        self.0.method = method;
        self
    }

    /// Set the URL path for the request.
    pub fn path(mut self, path: &'a str) -> Self {
        self.0.path = Cow::Borrowed(path);
        self
    }

    /// Set the URL hostname for the request
    pub fn host(mut self, host: &'a str) -> Self {
        self.0.host = host;
        self
    }

    /// Set the URL port for the request
    pub fn port(mut self, port: u16) -> Self {
        self.0.port = port;
        self
    }

    /// Set the hostname, port, and path for the request, from a string URL.
    pub fn url(self, url: &'a Url) -> Result<Self> {
        let (host, port, path) = RequestBuilder::parse_url(url)?;
        Ok(self.path(path).host(host).port(port))
    }

    /// Set the content hash for the request
    pub fn hash<H: Into<Option<&'a [u8]>>>(mut self, hash: H) -> Self {
        self.0.hash = hash.into();
        self
    }

    /// Set the `ext` Hawk property for the request
    pub fn ext<S: Into<Option<&'a str>>>(mut self, ext: S) -> Self {
        self.0.ext = ext.into();
        self
    }

    /// Set the `app` Hawk property for the request
    pub fn app<S: Into<Option<&'a str>>>(mut self, app: S) -> Self {
        self.0.app = app.into();
        self
    }

    /// Set the `dlg` Hawk property for the request
    pub fn dlg<S: Into<Option<&'a str>>>(mut self, dlg: S) -> Self {
        self.0.dlg = dlg.into();
        self
    }

    /// Get the request from this builder
    pub fn request(self) -> Request<'a> {
        self.0
    }

    /// Extract the `bewit` query parameter, if any, from the path, and return it in the output
    /// parameter, returning a modified RequestBuilder omitting the `bewit=..` query parameter.  If
    /// no bewit is present, or if an error is returned, the output parameter is reset to None.
    ///
    /// The path manipulation is tested to correspond to that preformed by the hueniverse/hawk
    /// implementation-specification
    pub fn extract_bewit(mut self, bewit: &mut Option<Bewit<'a>>) -> Result<Self> {
        const PREFIX: &str = "bewit=";
        *bewit = None;

        if let Some(query_index) = self.0.path.find('?') {
            let (bewit_components, components): (Vec<&str>, Vec<&str>) = self.0.path
                [query_index + 1..]
                .split('&')
                .partition(|comp| comp.starts_with(PREFIX));

            if bewit_components.len() == 1 {
                let bewit_str = bewit_components[0];
                *bewit = Some(Bewit::from_str(&bewit_str[PREFIX.len()..])?);

                // update the path to omit the bewit=... segment
                let new_path = if !components.is_empty() {
                    format!("{}{}", &self.0.path[..=query_index], components.join("&")).to_string()
                } else {
                    // no query left, so return the remaining path, omitting the '?'
                    self.0.path[..query_index].to_string()
                };
                self.0.path = Cow::Owned(new_path);
                Ok(self)
            } else if bewit_components.is_empty() {
                Ok(self)
            } else {
                Err(InvalidBewit::Multiple.into())
            }
        } else {
            Ok(self)
        }
    }

    fn parse_url(url: &'a Url) -> Result<(&'a str, u16, &'a str)> {
        let host = url
            .host_str()
            .ok_or_else(|| Error::InvalidUrl(format!("url {} has no host", url)))?;
        let port = url
            .port_or_known_default()
            .ok_or_else(|| Error::InvalidUrl(format!("url {} has no port", url)))?;
        let path = &url[Position::BeforePath..];
        Ok((host, port, path))
    }
}

/// Create a random string with `bytes` bytes of entropy.  The string
/// is base64-encoded. so it will be longer than bytes characters.
fn random_string(bytes: usize) -> Result<String> {
    let mut bytes = vec![0u8; bytes];
    crate::crypto::rand_bytes(&mut bytes)?;
    Ok(base64::encode(&bytes))
}

#[cfg(all(test, any(feature = "use_ring", feature = "use_openssl")))]
mod test {
    use super::*;
    use crate::credentials::{Credentials, Key};
    use crate::header::Header;
    use std::str::FromStr;
    use std::time::{Duration, SystemTime, UNIX_EPOCH};
    use url::Url;

    // this is a header from a real request using the JS Hawk library, to
    // https://pulse.taskcluster.net:443/v1/namespaces with credentials "me" / "tok"
    const REAL_HEADER: &'static str = "id=\"me\", ts=\"1491183061\", nonce=\"RVnYzW\", \
                                       mac=\"1kqRT9EoxiZ9AA/ayOCXB+AcjfK/BoJ+n7z0gfvZotQ=\"";
    const BEWIT_STR: &str =
        "bWVcMTM1MzgzMjgzNFxmaXk0ZTV3QmRhcEROeEhIZUExOE5yU3JVMVUzaVM2NmdtMFhqVEpwWXlVPVw";

    // this is used as the initial bewit when calling extract_bewit, to verify that it is
    // not allowing the original value of the parameter to remain in place.
    const INITIAL_BEWIT_STR: &str =
        "T0ggTk9FU1wxMzUzODMyODM0XGZpeTRlNXdCZGFwRE54SEhlQTE4TnJTclUxVTNpUzY2Z20wWGpUSnBZeVU9XCZtdXQgYmV3aXQgbm90IHJlc2V0IQ";

    #[test]
    fn test_empty() {
        let req = RequestBuilder::new("GET", "site", 80, "/").request();
        assert_eq!(req.method, "GET");
        assert_eq!(req.host, "site");
        assert_eq!(req.port, 80);
        assert_eq!(req.path, "/");
        assert_eq!(req.hash, None);
        assert_eq!(req.ext, None);
        assert_eq!(req.app, None);
        assert_eq!(req.dlg, None);
    }

    #[test]
    fn test_builder() {
        let hash = vec![0u8];
        let req = RequestBuilder::new("GET", "example.com", 443, "/foo")
            .hash(Some(&hash[..]))
            .ext("ext")
            .app("app")
            .dlg("dlg")
            .request();

        assert_eq!(req.method, "GET");
        assert_eq!(req.path, "/foo");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443);
        assert_eq!(req.hash, Some(&hash[..]));
        assert_eq!(req.ext, Some("ext"));
        assert_eq!(req.app, Some("app"));
        assert_eq!(req.dlg, Some("dlg"));
    }

    #[test]
    fn test_builder_clone() {
        let rb = RequestBuilder::new("GET", "site", 443, "/foo");
        let req = rb.clone().request();
        let req2 = rb.path("/bar").request();

        assert_eq!(req.method, "GET");
        assert_eq!(req.path, "/foo");
        assert_eq!(req2.method, "GET");
        assert_eq!(req2.path, "/bar");
    }

    #[test]
    fn test_url_builder() {
        let url = Url::parse("https://example.com/foo").unwrap();
        let req = RequestBuilder::from_url("GET", &url).unwrap().request();

        assert_eq!(req.path, "/foo");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_query() {
        let url = Url::parse("https://example.com/foo?foo=bar").unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, None);

        let req = bldr.request();

        assert_eq!(req.path, "/foo?foo=bar");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_encodable_chars() {
        let url = Url::parse("https://example.com/ñoo?foo=año").unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, None);

        let req = bldr.request();

        assert_eq!(req.path, "/%C3%B1oo?foo=a%C3%B1o");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_empty_query() {
        let url = Url::parse("https://example.com/foo?").unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, None);

        let req = bldr.request();

        assert_eq!(req.path, "/foo?");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_bewit_alone() {
        let url = Url::parse(&format!("https://example.com/foo?bewit={}", BEWIT_STR)).unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, Some(Bewit::from_str(BEWIT_STR).unwrap()));

        let req = bldr.request();

        assert_eq!(req.path, "/foo"); // NOTE: strips the `?`
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_bewit_first() {
        let url = Url::parse(&format!("https://example.com/foo?bewit={}&a=1", BEWIT_STR)).unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, Some(Bewit::from_str(BEWIT_STR).unwrap()));

        let req = bldr.request();

        assert_eq!(req.path, "/foo?a=1");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_bewit_multiple() {
        let url = Url::parse(&format!(
            "https://example.com/foo?bewit={}&bewit={}",
            BEWIT_STR, BEWIT_STR
        ))
        .unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        assert!(bldr.extract_bewit(&mut bewit).is_err());
        assert_eq!(bewit, None);
    }

    #[test]
    fn test_url_builder_with_bewit_invalid() {
        let url = Url::parse("https://example.com/foo?bewit=1234").unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        assert!(bldr.extract_bewit(&mut bewit).is_err());
        assert_eq!(bewit, None);
    }

    #[test]
    fn test_url_builder_with_bewit_last() {
        let url = Url::parse(&format!("https://example.com/foo?a=1&bewit={}", BEWIT_STR)).unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, Some(Bewit::from_str(BEWIT_STR).unwrap()));

        let req = bldr.request();

        assert_eq!(req.path, "/foo?a=1");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_bewit_middle() {
        let url = Url::parse(&format!(
            "https://example.com/foo?a=1&bewit={}&b=2",
            BEWIT_STR
        ))
        .unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, Some(Bewit::from_str(BEWIT_STR).unwrap()));

        let req = bldr.request();

        assert_eq!(req.path, "/foo?a=1&b=2");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_bewit_percent_encoding() {
        // Note that this *over*-encodes things.  Perfectly legal, but the kind
        // of thing that incautious libraries can sometimes fail to reproduce,
        // causing Hawk validation failures
        let url = Url::parse(&format!(
            "https://example.com/foo?%66oo=1&bewit={}&%62ar=2",
            BEWIT_STR
        ))
        .unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, Some(Bewit::from_str(BEWIT_STR).unwrap()));

        let req = bldr.request();

        assert_eq!(req.path, "/foo?%66oo=1&%62ar=2");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_xxxbewit() {
        // check that we're not doing a simple string search for "bewit=.."
        let url = Url::parse(&format!(
            "https://example.com/foo?a=1&xxxbewit={}&b=2",
            BEWIT_STR
        ))
        .unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, None);

        let req = bldr.request();

        assert_eq!(req.path, format!("/foo?a=1&xxxbewit={}&b=2", BEWIT_STR));
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_url_builder_with_username_password() {
        let url = Url::parse("https://a:b@example.com/foo?x=y").unwrap();
        let bldr = RequestBuilder::from_url("GET", &url).unwrap();

        let mut bewit = Some(Bewit::from_str(INITIAL_BEWIT_STR).unwrap());
        let bldr = bldr.extract_bewit(&mut bewit).unwrap();
        assert_eq!(bewit, None);

        let req = bldr.request();

        assert_eq!(req.path, "/foo?x=y");
        assert_eq!(req.host, "example.com");
        assert_eq!(req.port, 443); // default for https
    }

    #[test]
    fn test_make_header_full() {
        let req = RequestBuilder::new("GET", "example.com", 443, "/foo").request();
        let credentials = Credentials {
            id: "me".to_string(),
            key: Key::new(vec![99u8; 32], crate::SHA256).unwrap(),
        };
        let header = req
            .make_header_full(&credentials, UNIX_EPOCH + Duration::new(1000, 100), "nonny")
            .unwrap();
        assert_eq!(
            header,
            Header {
                id: Some("me".to_string()),
                ts: Some(UNIX_EPOCH + Duration::new(1000, 100)),
                nonce: Some("nonny".to_string()),
                mac: Some(Mac::from(vec![
                    122, 47, 2, 53, 195, 247, 185, 107, 133, 250, 61, 134, 200, 35, 118, 94, 48,
                    175, 237, 108, 60, 71, 4, 2, 244, 66, 41, 172, 91, 7, 233, 140
                ])),
                ext: None,
                hash: None,
                app: None,
                dlg: None,
            }
        );
    }

    #[test]
    fn test_make_header_full_with_optional_fields() {
        let hash = vec![0u8];
        let req = RequestBuilder::new("GET", "example.com", 443, "/foo")
            .hash(Some(&hash[..]))
            .ext("ext")
            .app("app")
            .dlg("dlg")
            .request();
        let credentials = Credentials {
            id: "me".to_string(),
            key: Key::new(vec![99u8; 32], crate::SHA256).unwrap(),
        };
        let header = req
            .make_header_full(&credentials, UNIX_EPOCH + Duration::new(1000, 100), "nonny")
            .unwrap();
        assert_eq!(
            header,
            Header {
                id: Some("me".to_string()),
                ts: Some(UNIX_EPOCH + Duration::new(1000, 100)),
                nonce: Some("nonny".to_string()),
                mac: Some(Mac::from(vec![
                    72, 123, 243, 214, 145, 81, 129, 54, 183, 90, 22, 136, 192, 146, 208, 53, 216,
                    138, 145, 94, 175, 204, 217, 8, 77, 16, 202, 50, 10, 144, 133, 162
                ])),
                ext: Some("ext".to_string()),
                hash: Some(hash.clone()),
                app: Some("app".to_string()),
                dlg: Some("dlg".to_string()),
            }
        );
    }

    #[test]
    fn test_validate_matches_generated() {
        let req = RequestBuilder::new("GET", "example.com", 443, "/foo").request();
        let credentials = Credentials {
            id: "me".to_string(),
            key: Key::new(vec![99u8; 32], crate::SHA256).unwrap(),
        };
        let header = req
            .make_header_full(&credentials, SystemTime::now(), "nonny")
            .unwrap();
        assert!(req.validate_header(&header, &credentials.key, Duration::from_secs(1 * 60)));
    }

    // Well, close enough.
    const ONE_YEAR_IN_SECS: u64 = 365 * 24 * 60 * 60;

    #[test]
    fn test_validate_real_request() {
        let header = Header::from_str(REAL_HEADER).unwrap();
        let credentials = Credentials {
            id: "me".to_string(),
            key: Key::new("tok", crate::SHA256).unwrap(),
        };
        let req =
            RequestBuilder::new("GET", "pulse.taskcluster.net", 443, "/v1/namespaces").request();
        // allow 1000 years skew, since this was a real request that
        // happened back in 2017, when life was simple and carefree
        assert!(req.validate_header(
            &header,
            &credentials.key,
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));
    }

    #[test]
    fn test_validate_real_request_bad_creds() {
        let header = Header::from_str(REAL_HEADER).unwrap();
        let credentials = Credentials {
            id: "me".to_string(),
            key: Key::new("WRONG", crate::SHA256).unwrap(),
        };
        let req =
            RequestBuilder::new("GET", "pulse.taskcluster.net", 443, "/v1/namespaces").request();
        assert!(!req.validate_header(
            &header,
            &credentials.key,
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));
    }

    #[test]
    fn test_validate_real_request_bad_req_info() {
        let header = Header::from_str(REAL_HEADER).unwrap();
        let credentials = Credentials {
            id: "me".to_string(),
            key: Key::new("tok", crate::SHA256).unwrap(),
        };
        let req = RequestBuilder::new("GET", "pulse.taskcluster.net", 443, "WRONG PATH").request();
        assert!(!req.validate_header(
            &header,
            &credentials.key,
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));
    }

    fn make_header_without_hash() -> Header {
        Header::new(
            Some("dh37fgj492je"),
            Some(UNIX_EPOCH + Duration::new(1353832234, 0)),
            Some("j4h3g2"),
            Some(Mac::from(vec![
                161, 105, 122, 110, 248, 62, 129, 193, 148, 206, 239, 193, 219, 46, 137, 221, 51,
                170, 135, 114, 81, 68, 145, 182, 15, 165, 145, 168, 114, 237, 52, 35,
            ])),
            None,
            None,
            None,
            None,
        )
        .unwrap()
    }

    fn make_header_with_hash() -> Header {
        Header::new(
            Some("dh37fgj492je"),
            Some(UNIX_EPOCH + Duration::new(1353832234, 0)),
            Some("j4h3g2"),
            Some(Mac::from(vec![
                189, 53, 155, 244, 203, 150, 255, 238, 135, 144, 186, 93, 6, 189, 184, 21, 150,
                210, 226, 61, 93, 154, 17, 218, 142, 250, 254, 193, 123, 132, 131, 195,
            ])),
            None,
            Some(vec![1, 2, 3, 4]),
            None,
            None,
        )
        .unwrap()
    }

    #[test]
    fn test_validate_no_hash() {
        let header = make_header_without_hash();
        let req = RequestBuilder::new("", "", 0, "").request();
        assert!(req.validate_header(
            &header,
            &Key::new("tok", crate::SHA256).unwrap(),
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));
    }

    #[test]
    fn test_validate_hash_in_header() {
        let header = make_header_with_hash();
        let req = RequestBuilder::new("", "", 0, "").request();
        assert!(req.validate_header(
            &header,
            &Key::new("tok", crate::SHA256).unwrap(),
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));
    }

    #[test]
    fn test_validate_hash_required_but_not_given() {
        let header = make_header_without_hash();
        let hash = vec![1, 2, 3, 4];
        let req = RequestBuilder::new("", "", 0, "")
            .hash(Some(&hash[..]))
            .request();
        assert!(!req.validate_header(
            &header,
            &Key::new("tok", crate::SHA256).unwrap(),
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));
    }

    #[test]
    fn test_validate_hash_validated() {
        let header = make_header_with_hash();
        let hash = vec![1, 2, 3, 4];
        let req = RequestBuilder::new("", "", 0, "")
            .hash(Some(&hash[..]))
            .request();
        assert!(req.validate_header(
            &header,
            &Key::new("tok", crate::SHA256).unwrap(),
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));

        // ..but supplying the wrong hash will cause validation to fail
        let hash = vec![99, 99, 99, 99];
        let req = RequestBuilder::new("", "", 0, "")
            .hash(Some(&hash[..]))
            .request();
        assert!(!req.validate_header(
            &header,
            &Key::new("tok", crate::SHA256).unwrap(),
            Duration::from_secs(1000 * ONE_YEAR_IN_SECS)
        ));
    }

    fn round_trip_bewit(req: Request, ts: SystemTime, expected: bool) {
        let credentials = Credentials {
            id: "me".to_string(),
            key: Key::new("tok", crate::SHA256).unwrap(),
        };

        let bewit = req.make_bewit(&credentials, ts).unwrap();

        // convert to a string and back
        let bewit = bewit.to_str();
        let bewit = Bewit::from_str(&bewit).unwrap();

        // and validate it maches the original request
        assert_eq!(req.validate_bewit(&bewit, &credentials.key), expected);
    }

    #[test]
    fn test_validate_bewit() {
        let req = RequestBuilder::new("GET", "foo.com", 443, "/x/y/z").request();
        round_trip_bewit(req, SystemTime::now() + Duration::from_secs(10 * 60), true);
    }

    #[test]
    fn test_validate_bewit_ext() {
        let req = RequestBuilder::new("GET", "foo.com", 443, "/x/y/z")
            .ext("abcd")
            .request();
        round_trip_bewit(req, SystemTime::now() + Duration::from_secs(10 * 60), true);
    }

    #[test]
    fn test_validate_bewit_expired() {
        let req = RequestBuilder::new("GET", "foo.com", 443, "/x/y/z").request();
        round_trip_bewit(req, SystemTime::now() - Duration::from_secs(10 * 60), false);
    }
}
