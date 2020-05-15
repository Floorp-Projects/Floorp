use crate::error::*;
use crate::mac::Mac;
use base64::display::Base64Display;
use std::fmt;
use std::str::FromStr;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
/// Representation of a Hawk `Authorization` header value (the part following "Hawk ").
///
/// Headers can be derived from strings using the `FromStr` trait, and formatted into a
/// string using the `fmt_header` method.
///
/// All fields are optional, although for specific purposes some fields must be present.
#[derive(Clone, PartialEq, Debug)]
pub struct Header {
    pub id: Option<String>,
    pub ts: Option<SystemTime>,
    pub nonce: Option<String>,
    pub mac: Option<Mac>,
    pub ext: Option<String>,
    pub hash: Option<Vec<u8>>,
    pub app: Option<String>,
    pub dlg: Option<String>,
}

impl Header {
    /// Create a new Header with the full set of Hawk fields.
    ///
    /// This is a low-level function. Headers are more often created from Requests or Responses.
    ///
    /// Note that none of the string-formatted header components can contain the character `\"`.
    pub fn new<S>(
        id: Option<S>,
        ts: Option<SystemTime>,
        nonce: Option<S>,
        mac: Option<Mac>,
        ext: Option<S>,
        hash: Option<Vec<u8>>,
        app: Option<S>,
        dlg: Option<S>,
    ) -> Result<Header>
    where
        S: Into<String>,
    {
        Ok(Header {
            id: Header::check_component(id)?,
            ts,
            nonce: Header::check_component(nonce)?,
            mac,
            ext: Header::check_component(ext)?,
            hash,
            app: Header::check_component(app)?,
            dlg: Header::check_component(dlg)?,
        })
    }

    /// Check a header component for validity.
    fn check_component<S>(value: Option<S>) -> Result<Option<String>>
    where
        S: Into<String>,
    {
        if let Some(value) = value {
            let value = value.into();
            if value.contains('\"') {
                return Err(Error::HeaderParseError(
                    "Hawk headers cannot contain `\\`".into(),
                ));
            }
            Ok(Some(value))
        } else {
            Ok(None)
        }
    }

    /// Format the header for transmission in an Authorization header, omitting the `"Hawk "`
    /// prefix.
    pub fn fmt_header(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut sep = "";
        if let Some(ref id) = self.id {
            write!(f, "{}id=\"{}\"", sep, id)?;
            sep = ", ";
        }
        if let Some(ref ts) = self.ts {
            write!(
                f,
                "{}ts=\"{}\"",
                sep,
                ts.duration_since(UNIX_EPOCH).unwrap_or_default().as_secs()
            )?;
            sep = ", ";
        }
        if let Some(ref nonce) = self.nonce {
            write!(f, "{}nonce=\"{}\"", sep, nonce)?;
            sep = ", ";
        }
        if let Some(ref mac) = self.mac {
            write!(
                f,
                "{}mac=\"{}\"",
                sep,
                Base64Display::with_config(mac, base64::STANDARD)
            )?;
            sep = ", ";
        }
        if let Some(ref ext) = self.ext {
            write!(f, "{}ext=\"{}\"", sep, ext)?;
            sep = ", ";
        }
        if let Some(ref hash) = self.hash {
            write!(
                f,
                "{}hash=\"{}\"",
                sep,
                Base64Display::with_config(hash, base64::STANDARD)
            )?;
            sep = ", ";
        }
        if let Some(ref app) = self.app {
            write!(f, "{}app=\"{}\"", sep, app)?;
            sep = ", ";
        }
        if let Some(ref dlg) = self.dlg {
            write!(f, "{}dlg=\"{}\"", sep, dlg)?;
        }
        Ok(())
    }
}

impl fmt::Display for Header {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.fmt_header(f)
    }
}

impl FromStr for Header {
    type Err = Error;
    fn from_str(s: &str) -> Result<Header> {
        let mut p = &s[..];

        // Required attributes
        let mut id: Option<&str> = None;
        let mut ts: Option<SystemTime> = None;
        let mut nonce: Option<&str> = None;
        let mut mac: Option<Vec<u8>> = None;
        // Optional attributes
        let mut hash: Option<Vec<u8>> = None;
        let mut ext: Option<&str> = None;
        let mut app: Option<&str> = None;
        let mut dlg: Option<&str> = None;

        while !p.is_empty() {
            // Skip whitespace and commas used as separators
            p = p.trim_start_matches(|c| c == ',' || char::is_whitespace(c));
            // Find first '=' which delimits attribute name from value
            let assign_end = p
                .find('=')
                .ok_or_else(|| Error::HeaderParseError("Expected '='".into()))?;
            let attr = &p[..assign_end].trim();
            if p.len() < assign_end + 1 {
                return Err(Error::HeaderParseError(
                    "Missing right hand side of =".into(),
                ));
            }
            p = (&p[assign_end + 1..]).trim_start();
            if !p.starts_with('\"') {
                return Err(Error::HeaderParseError("Expected opening quote".into()));
            }
            p = &p[1..];
            // We have poor RFC 7235 compliance here as we ought to support backslash
            // escaped characters, but hawk doesn't allow this we won't either.  All
            // strings must be surrounded by ".." and contain no such characters.
            let end = p.find('\"');
            let val_end =
                end.ok_or_else(|| Error::HeaderParseError("Expected closing quote".into()))?;
            let val = &p[..val_end];
            match *attr {
                "id" => id = Some(val),
                "ts" => {
                    let epoch = u64::from_str(val)
                        .map_err(|_| Error::HeaderParseError("Error parsing `ts` field".into()))?;
                    ts = Some(UNIX_EPOCH + Duration::new(epoch, 0));
                }
                "mac" => {
                    mac = Some(base64::decode(val).map_err(|_| {
                        Error::HeaderParseError("Error parsing `mac` field".into())
                    })?);
                }
                "nonce" => nonce = Some(val),
                "ext" => ext = Some(val),
                "hash" => {
                    hash = Some(base64::decode(val).map_err(|_| {
                        Error::HeaderParseError("Error parsing `hash` field".into())
                    })?);
                }
                "app" => app = Some(val),
                "dlg" => dlg = Some(val),
                _ => {
                    return Err(Error::HeaderParseError(format!(
                        "Invalid Hawk field {}",
                        *attr
                    )))
                }
            };
            // Break if we are at end of string, otherwise skip separator
            if p.len() < val_end + 1 {
                break;
            }
            p = p[val_end + 1..].trim_start();
        }

        Ok(Header {
            id: match id {
                Some(id) => Some(id.to_string()),
                None => None,
            },
            ts,
            nonce: match nonce {
                Some(nonce) => Some(nonce.to_string()),
                None => None,
            },
            mac: match mac {
                Some(mac) => Some(Mac::from(mac)),
                None => None,
            },
            ext: match ext {
                Some(ext) => Some(ext.to_string()),
                None => None,
            },
            hash,
            app: match app {
                Some(app) => Some(app.to_string()),
                None => None,
            },
            dlg: match dlg {
                Some(dlg) => Some(dlg.to_string()),
                None => None,
            },
        })
    }
}

#[cfg(test)]
mod test {
    use super::Header;
    use crate::mac::Mac;
    use std::str::FromStr;
    use std::time::{Duration, UNIX_EPOCH};

    #[test]
    fn illegal_id() {
        assert!(Header::new(
            Some("ab\"cdef"),
            Some(UNIX_EPOCH + Duration::new(1234, 0)),
            Some("nonce"),
            Some(Mac::from(vec![])),
            Some("ext"),
            None,
            None,
            None
        )
        .is_err());
    }

    #[test]
    fn illegal_nonce() {
        assert!(Header::new(
            Some("abcdef"),
            Some(UNIX_EPOCH + Duration::new(1234, 0)),
            Some("no\"nce"),
            Some(Mac::from(vec![])),
            Some("ext"),
            None,
            None,
            None
        )
        .is_err());
    }

    #[test]
    fn illegal_ext() {
        assert!(Header::new(
            Some("abcdef"),
            Some(UNIX_EPOCH + Duration::new(1234, 0)),
            Some("nonce"),
            Some(Mac::from(vec![])),
            Some("ex\"t"),
            None,
            None,
            None
        )
        .is_err());
    }

    #[test]
    fn illegal_app() {
        assert!(Header::new(
            Some("abcdef"),
            Some(UNIX_EPOCH + Duration::new(1234, 0)),
            Some("nonce"),
            Some(Mac::from(vec![])),
            None,
            None,
            Some("a\"pp"),
            None
        )
        .is_err());
    }

    #[test]
    fn illegal_dlg() {
        assert!(Header::new(
            Some("abcdef"),
            Some(UNIX_EPOCH + Duration::new(1234, 0)),
            Some("nonce"),
            Some(Mac::from(vec![])),
            None,
            None,
            None,
            Some("d\"lg")
        )
        .is_err());
    }

    #[test]
    fn from_str() {
        let s = Header::from_str(
            "id=\"dh37fgj492je\", ts=\"1353832234\", \
             nonce=\"j4h3g2\", ext=\"some-app-ext-data\", \
             mac=\"6R4rV5iE+NPoym+WwjeHzjAGXUtLNIxmo1vpMofpLAE=\", \
             hash=\"6R4rV5iE+NPoym+WwjeHzjAGXUtLNIxmo1vpMofpLAE=\", \
             app=\"my-app\", dlg=\"my-authority\"",
        )
        .unwrap();
        assert!(s.id == Some("dh37fgj492je".to_string()));
        assert!(s.ts == Some(UNIX_EPOCH + Duration::new(1353832234, 0)));
        assert!(s.nonce == Some("j4h3g2".to_string()));
        assert!(
            s.mac
                == Some(Mac::from(vec![
                    233, 30, 43, 87, 152, 132, 248, 211, 232, 202, 111, 150, 194, 55, 135, 206, 48,
                    6, 93, 75, 75, 52, 140, 102, 163, 91, 233, 50, 135, 233, 44, 1
                ]))
        );
        assert!(s.ext == Some("some-app-ext-data".to_string()));
        assert!(s.app == Some("my-app".to_string()));
        assert!(s.dlg == Some("my-authority".to_string()));
    }

    #[test]
    fn from_str_invalid_mac() {
        let r = Header::from_str(
            "id=\"dh37fgj492je\", ts=\"1353832234\", \
             nonce=\"j4h3g2\", ext=\"some-app-ext-data\", \
             mac=\"6!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!AE=\", \
             app=\"my-app\", dlg=\"my-authority\"",
        );
        assert!(r.is_err());
    }

    #[test]
    fn from_str_no_field() {
        let s = Header::from_str("").unwrap();
        assert!(s.id == None);
        assert!(s.ts == None);
        assert!(s.nonce == None);
        assert!(s.mac == None);
        assert!(s.ext == None);
        assert!(s.app == None);
        assert!(s.dlg == None);
    }

    #[test]
    fn from_str_few_field() {
        let s = Header::from_str(
            "id=\"xyz\", ts=\"1353832234\", \
             nonce=\"abc\", \
             mac=\"6R4rV5iE+NPoym+WwjeHzjAGXUtLNIxmo1vpMofpLAE=\"",
        )
        .unwrap();
        assert!(s.id == Some("xyz".to_string()));
        assert!(s.ts == Some(UNIX_EPOCH + Duration::new(1353832234, 0)));
        assert!(s.nonce == Some("abc".to_string()));
        assert!(
            s.mac
                == Some(Mac::from(vec![
                    233, 30, 43, 87, 152, 132, 248, 211, 232, 202, 111, 150, 194, 55, 135, 206, 48,
                    6, 93, 75, 75, 52, 140, 102, 163, 91, 233, 50, 135, 233, 44, 1
                ]))
        );
        assert!(s.ext == None);
        assert!(s.app == None);
        assert!(s.dlg == None);
    }

    #[test]
    fn from_str_messy() {
        let s = Header::from_str(
            ", id  =  \"dh37fgj492je\", ts=\"1353832234\", \
             nonce=\"j4h3g2\"  , , ext=\"some-app-ext-data\", \
             mac=\"6R4rV5iE+NPoym+WwjeHzjAGXUtLNIxmo1vpMofpLAE=\"",
        )
        .unwrap();
        assert!(s.id == Some("dh37fgj492je".to_string()));
        assert!(s.ts == Some(UNIX_EPOCH + Duration::new(1353832234, 0)));
        assert!(s.nonce == Some("j4h3g2".to_string()));
        assert!(
            s.mac
                == Some(Mac::from(vec![
                    233, 30, 43, 87, 152, 132, 248, 211, 232, 202, 111, 150, 194, 55, 135, 206, 48,
                    6, 93, 75, 75, 52, 140, 102, 163, 91, 233, 50, 135, 233, 44, 1
                ]))
        );
        assert!(s.ext == Some("some-app-ext-data".to_string()));
        assert!(s.app == None);
        assert!(s.dlg == None);
    }

    #[test]
    fn to_str_no_fields() {
        // must supply a type for S, since it is otherwise unused
        let s = Header::new::<String>(None, None, None, None, None, None, None, None).unwrap();
        let formatted = format!("{}", s);
        println!("got: {}", formatted);
        assert!(formatted == "")
    }

    #[test]
    fn to_str_few_fields() {
        let s = Header::new(
            Some("dh37fgj492je"),
            Some(UNIX_EPOCH + Duration::new(1353832234, 0)),
            Some("j4h3g2"),
            Some(Mac::from(vec![
                8, 35, 182, 149, 42, 111, 33, 192, 19, 22, 94, 43, 118, 176, 65, 69, 86, 4, 156,
                184, 85, 107, 249, 242, 172, 200, 66, 209, 57, 63, 38, 83,
            ])),
            None,
            None,
            None,
            None,
        )
        .unwrap();
        let formatted = format!("{}", s);
        println!("got: {}", formatted);
        assert!(
            formatted
                == "id=\"dh37fgj492je\", ts=\"1353832234\", nonce=\"j4h3g2\", \
                    mac=\"CCO2lSpvIcATFl4rdrBBRVYEnLhVa/nyrMhC0Tk/JlM=\""
        )
    }

    #[test]
    fn to_str_maximal() {
        let s = Header::new(
            Some("dh37fgj492je"),
            Some(UNIX_EPOCH + Duration::new(1353832234, 0)),
            Some("j4h3g2"),
            Some(Mac::from(vec![
                8, 35, 182, 149, 42, 111, 33, 192, 19, 22, 94, 43, 118, 176, 65, 69, 86, 4, 156,
                184, 85, 107, 249, 242, 172, 200, 66, 209, 57, 63, 38, 83,
            ])),
            Some("my-ext-value"),
            Some(vec![1, 2, 3, 4]),
            Some("my-app"),
            Some("my-dlg"),
        )
        .unwrap();
        let formatted = format!("{}", s);
        println!("got: {}", formatted);
        assert!(
            formatted
                == "id=\"dh37fgj492je\", ts=\"1353832234\", nonce=\"j4h3g2\", \
                    mac=\"CCO2lSpvIcATFl4rdrBBRVYEnLhVa/nyrMhC0Tk/JlM=\", ext=\"my-ext-value\", \
                    hash=\"AQIDBA==\", app=\"my-app\", dlg=\"my-dlg\""
        )
    }

    #[test]
    fn round_trip() {
        let s = Header::new(
            Some("dh37fgj492je"),
            Some(UNIX_EPOCH + Duration::new(1353832234, 0)),
            Some("j4h3g2"),
            Some(Mac::from(vec![
                8, 35, 182, 149, 42, 111, 33, 192, 19, 22, 94, 43, 118, 176, 65, 69, 86, 4, 156,
                184, 85, 107, 249, 242, 172, 200, 66, 209, 57, 63, 38, 83,
            ])),
            Some("my-ext-value"),
            Some(vec![1, 2, 3, 4]),
            Some("my-app"),
            Some("my-dlg"),
        )
        .unwrap();
        let formatted = format!("{}", s);
        println!("got: {}", s);
        let s2 = Header::from_str(&formatted).unwrap();
        assert!(s2 == s);
    }
}
