use crate::credentials::Key;
use crate::error::*;
use base64::{display::Base64Display, STANDARD};
use std::io::Write;
use std::ops::Deref;
use std::time::{SystemTime, UNIX_EPOCH};

/// The kind of MAC calcuation (corresponding to the first line of the message)
pub enum MacType {
    Header,
    Response,
    Bewit,
}

/// Mac represents a message authentication code, the signature in a Hawk transaction.
///
/// This class supports creating Macs using the Hawk specification, and comparing Macs
/// using a cosntant-time comparison (thus preventing timing side-channel attacks).
#[derive(Debug, Clone)]
pub struct Mac(Vec<u8>);

impl Mac {
    pub fn new(
        mac_type: MacType,
        key: &Key,
        ts: SystemTime,
        nonce: &str,
        method: &str,
        host: &str,
        port: u16,
        path: &str,
        hash: Option<&[u8]>,
        ext: Option<&str>,
    ) -> Result<Mac> {
        // Note: there's a \n after each item.
        let mut buffer: Vec<u8> = Vec::with_capacity(
            15 + 1 + // mac_type (worst case since it doesn't really matter)
            10 + 1 + // ts (in practice this will be 10 bytes)
            nonce.len() + 1 +
            host.len() + 1 +
            6 + 1 + // Longer than 6 bytes of port seems very unlikely
            path.len() + 1 +
            hash.map_or(0, |h| h.len() * 4 / 3) + 1 +
            ext.map_or(0, str::len) + 1,
        );

        writeln!(
            buffer,
            "{mac_type}\n{ts}\n{nonce}\n{method}\n{path}\n{host}\n{port}",
            mac_type = match mac_type {
                MacType::Header => "hawk.1.header",
                MacType::Response => "hawk.1.response",
                MacType::Bewit => "hawk.1.bewit",
            },
            ts = ts.duration_since(UNIX_EPOCH).unwrap_or_default().as_secs(),
            nonce = nonce,
            method = method,
            path = path,
            host = host,
            port = port,
        )?;

        if let Some(h) = hash {
            writeln!(buffer, "{}", Base64Display::with_config(h, STANDARD))?;
        } else {
            writeln!(buffer)?;
        }
        writeln!(buffer, "{}", ext.unwrap_or_default())?;

        Ok(Mac(key.sign(buffer.as_ref())?))
    }
}

impl AsRef<[u8]> for Mac {
    fn as_ref(&self) -> &[u8] {
        &self.0[..]
    }
}

impl From<Vec<u8>> for Mac {
    fn from(original: Vec<u8>) -> Self {
        Mac(original)
    }
}

impl Deref for Mac {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl PartialEq for Mac {
    fn eq(&self, other: &Mac) -> bool {
        crate::crypto::constant_time_compare(&self.0, &other.0)
    }
}

#[cfg(all(test, any(feature = "use_ring", feature = "use_openssl")))]
mod test {
    use super::{Mac, MacType};
    use crate::credentials::Key;
    use std::time::{Duration, SystemTime, UNIX_EPOCH};

    fn key() -> Key {
        Key::new(
            vec![
                11u8, 19, 228, 209, 79, 189, 200, 59, 166, 47, 86, 254, 235, 184, 120, 197, 75,
                152, 201, 79, 115, 61, 111, 242, 219, 187, 173, 14, 227, 108, 60, 232,
            ],
            crate::SHA256,
        )
        .unwrap()
    }

    fn sys_time(secs: u64, ns: u32) -> SystemTime {
        UNIX_EPOCH + Duration::new(secs, ns)
    }

    #[test]
    fn test_make_mac() {
        let key = key();
        let mac = Mac::new(
            MacType::Header,
            &key,
            sys_time(1000, 100),
            "nonny",
            "POST",
            "mysite.com",
            443,
            "/v1/api",
            None,
            None,
        )
        .unwrap();
        println!("got {:?}", mac);
        assert!(
            mac.0
                == vec![
                    192, 227, 235, 121, 157, 185, 197, 79, 189, 214, 235, 139, 9, 232, 99, 55, 67,
                    30, 68, 0, 150, 187, 192, 238, 21, 200, 209, 107, 245, 159, 243, 178
                ]
        );
    }

    #[test]
    fn test_make_mac_hash() {
        let key = key();
        let hash = vec![1, 2, 3, 4, 5];
        let mac = Mac::new(
            MacType::Header,
            &key,
            sys_time(1000, 100),
            "nonny",
            "POST",
            "mysite.com",
            443,
            "/v1/api",
            Some(&hash),
            None,
        )
        .unwrap();
        println!("got {:?}", mac);
        assert!(
            mac.0
                == vec![
                    61, 128, 208, 253, 88, 135, 190, 196, 1, 69, 153, 193, 124, 4, 195, 87, 38, 96,
                    181, 34, 65, 234, 58, 157, 175, 175, 145, 151, 61, 0, 57, 5
                ]
        );
    }

    #[test]
    fn test_make_mac_ext() {
        let key = key();
        let ext = "ext-data".to_string();
        let mac = Mac::new(
            MacType::Header,
            &key,
            sys_time(1000, 100),
            "nonny",
            "POST",
            "mysite.com",
            443,
            "/v1/api",
            None,
            Some(&ext),
        )
        .unwrap();
        println!("got {:?}", mac);
        assert!(
            mac.0
                == vec![
                    187, 104, 238, 100, 168, 112, 37, 68, 187, 141, 168, 155, 177, 193, 113, 0, 50,
                    105, 127, 36, 24, 117, 200, 251, 138, 199, 108, 14, 105, 123, 234, 119
                ]
        );
    }
}
