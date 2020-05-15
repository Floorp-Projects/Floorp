use crate::error::*;
use crate::mac::Mac;
use std::borrow::Cow;
use std::str;
use std::str::FromStr;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

/// A Bewit is a piece of data attached to a GET request that functions in place of a Hawk
/// Authentication header.  It contains an id, a timestamp, a MAC, and an optional `ext` value.
/// These are available using accessor functions.
#[derive(Clone, Debug, PartialEq)]
pub struct Bewit<'a> {
    id: Cow<'a, str>,
    exp: SystemTime,
    mac: Cow<'a, Mac>,
    ext: Option<Cow<'a, str>>,
}

impl<'a> Bewit<'a> {
    /// Create a new Bewit with the given values.
    ///
    /// See Request.make_bewit for an easier way to make a Bewit
    pub fn new(id: &'a str, exp: SystemTime, mac: Mac, ext: Option<&'a str>) -> Bewit<'a> {
        Bewit {
            id: Cow::Borrowed(id),
            exp,
            mac: Cow::Owned(mac),
            ext: match ext {
                Some(s) => Some(Cow::Borrowed(s)),
                None => None,
            },
        }
    }

    /// Generate the fully-encoded string for this Bewit
    pub fn to_str(&self) -> String {
        use base64::display::Base64Display;
        let raw = format!(
            "{}\\{}\\{}\\{}",
            self.id,
            self.exp
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs(),
            Base64Display::with_config(self.mac.as_ref(), base64::STANDARD),
            match self.ext {
                Some(ref cow) => cow.as_ref(),
                None => "",
            }
        );

        base64::encode_config(&raw, base64::URL_SAFE_NO_PAD)
    }

    /// Get the Bewit's client identifier
    pub fn id(&self) -> &str {
        self.id.as_ref()
    }

    /// Get the expiration time of the bewit
    pub fn exp(&self) -> SystemTime {
        self.exp
    }

    /// Get the MAC included in the Bewit
    pub fn mac(&self) -> &Mac {
        self.mac.as_ref()
    }

    /// Get the Bewit's `ext` field.
    pub fn ext(&self) -> Option<&str> {
        match self.ext {
            Some(ref cow) => Some(cow.as_ref()),
            None => None,
        }
    }
}

const BACKSLASH: u8 = b'\\';

impl<'a> FromStr for Bewit<'a> {
    type Err = Error;
    fn from_str(bewit: &str) -> Result<Bewit<'a>> {
        let bewit = base64::decode(bewit)?;

        let parts: Vec<&[u8]> = bewit.split(|c| *c == BACKSLASH).collect();
        if parts.len() != 4 {
            return Err(InvalidBewit::Format.into());
        }

        let id = String::from_utf8(parts[0].to_vec()).map_err(|_| InvalidBewit::Id)?;

        let exp = str::from_utf8(parts[1]).map_err(|_| InvalidBewit::Exp)?;
        let exp = u64::from_str(exp).map_err(|_| InvalidBewit::Exp)?;
        let exp = UNIX_EPOCH + Duration::new(exp, 0);

        let mac = str::from_utf8(parts[2]).map_err(|_| InvalidBewit::Mac)?;
        let mac = Mac::from(base64::decode(mac).map_err(|_| InvalidBewit::Mac)?);

        let ext = match parts[3].len() {
            0 => None,
            _ => Some(Cow::Owned(
                String::from_utf8(parts[3].to_vec()).map_err(|_| InvalidBewit::Ext)?,
            )),
        };

        Ok(Bewit {
            id: Cow::Owned(id),
            exp,
            mac: Cow::Owned(mac),
            ext,
        })
    }
}

#[cfg(all(test, any(feature = "use_ring", feature = "use_openssl")))]
mod test {
    use super::*;
    use crate::credentials::Key;
    use crate::mac::{Mac, MacType};
    use std::str::FromStr;

    const BEWIT_STR: &str =
        "bWVcMTM1MzgzMjgzNFxmaXk0ZTV3QmRhcEROeEhIZUExOE5yU3JVMVUzaVM2NmdtMFhqVEpwWXlVPVw";
    const BEWIT_WITH_EXT_STR: &str =
        "bWVcMTM1MzgzMjgzNFxmaXk0ZTV3QmRhcEROeEhIZUExOE5yU3JVMVUzaVM2NmdtMFhqVEpwWXlVPVxhYmNk";

    fn make_mac() -> Mac {
        let key = Key::new(
            vec![
                11u8, 19, 228, 209, 79, 189, 200, 59, 166, 47, 86, 254, 235, 184, 120, 197, 75,
                152, 201, 79, 115, 61, 111, 242, 219, 187, 173, 14, 227, 108, 60, 232,
            ],
            crate::DigestAlgorithm::Sha256,
        )
        .unwrap();
        Mac::new(
            MacType::Header,
            &key,
            UNIX_EPOCH + Duration::new(1353832834, 100),
            "nonny",
            "POST",
            "mysite.com",
            443,
            "/v1/api",
            None,
            None,
        )
        .unwrap()
    }

    #[test]
    fn test_to_str() {
        let bewit = Bewit::new(
            "me",
            UNIX_EPOCH + Duration::new(1353832834, 0),
            make_mac(),
            None,
        );
        assert_eq!(bewit.to_str(), BEWIT_STR);
        let bewit = Bewit::new(
            "me",
            UNIX_EPOCH + Duration::new(1353832834, 0),
            make_mac(),
            Some("abcd"),
        );
        assert_eq!(bewit.to_str(), BEWIT_WITH_EXT_STR);
    }

    #[test]
    fn test_accessors() {
        let bewit = Bewit::from_str(BEWIT_STR).unwrap();
        assert_eq!(bewit.id(), "me");
        assert_eq!(bewit.exp(), UNIX_EPOCH + Duration::new(1353832834, 0));
        assert_eq!(bewit.mac(), &make_mac());
        assert_eq!(bewit.ext(), None);
    }

    #[test]
    fn test_from_str_invalid_base64() {
        assert!(Bewit::from_str("!/==").is_err());
    }

    #[test]
    fn test_from_str_invalid_too_many_parts() {
        let bewit = base64::encode(&"a\\123\\abc\\ext\\WHUT?".as_bytes());
        assert!(Bewit::from_str(&bewit).is_err());
    }

    #[test]
    fn test_from_str_invalid_too_few_parts() {
        let bewit = base64::encode(&"a\\123\\abc".as_bytes());
        assert!(Bewit::from_str(&bewit).is_err());
    }

    #[test]
    fn test_from_str_invalid_not_utf8() {
        let a = 'a' as u8;
        let one = '1' as u8;
        let slash = '\\' as u8;
        let invalid1 = 0u8;
        let invalid2 = 159u8;
        let bewit = base64::encode(&[invalid1, invalid2, slash, one, slash, a, slash, a]);
        assert!(Bewit::from_str(&bewit).is_err());
        let bewit = base64::encode(&[a, slash, invalid1, invalid2, slash, a, slash, a]);
        assert!(Bewit::from_str(&bewit).is_err());
        let bewit = base64::encode(&[a, slash, one, slash, invalid1, invalid2, slash, a]);
        assert!(Bewit::from_str(&bewit).is_err());
        let bewit = base64::encode(&[a, slash, one, slash, a, slash, invalid1, invalid2]);
        assert!(Bewit::from_str(&bewit).is_err());
    }
}
