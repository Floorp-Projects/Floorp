use bytes::Bytes;

use std::{ops, str};

#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub(crate) struct ByteStr {
    bytes: Bytes,
}

impl ByteStr {
    #[inline]
    pub fn new() -> ByteStr {
        ByteStr { bytes: Bytes::new() }
    }

    #[inline]
    pub fn from_static(val: &'static str) -> ByteStr {
        ByteStr { bytes: Bytes::from_static(val.as_bytes()) }
    }

    #[inline]
    pub unsafe fn from_utf8_unchecked(bytes: Bytes) -> ByteStr {
        if cfg!(debug_assertions) {
            match str::from_utf8(&bytes) {
                Ok(_) => (),
                Err(err) => panic!("ByteStr::from_utf8_unchecked() with invalid bytes; error = {}, bytes = {:?}", err, bytes),
            }
        }
        ByteStr { bytes: bytes }
    }
}

impl ops::Deref for ByteStr {
    type Target = str;

    #[inline]
    fn deref(&self) -> &str {
        let b: &[u8] = self.bytes.as_ref();
        unsafe { str::from_utf8_unchecked(b) }
    }
}

impl From<String> for ByteStr {
    #[inline]
    fn from(src: String) -> ByteStr {
        ByteStr { bytes: Bytes::from(src) }
    }
}

impl<'a> From<&'a str> for ByteStr {
    #[inline]
    fn from(src: &'a str) -> ByteStr {
        ByteStr { bytes: Bytes::from(src) }
    }
}

impl From<ByteStr> for Bytes {
    fn from(src: ByteStr) -> Self {
        src.bytes
    }
}
