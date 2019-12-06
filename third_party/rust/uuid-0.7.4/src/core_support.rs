// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::{fmt, str};
use parser;
use prelude::*;

impl From<super::BytesError> for super::Error {
    fn from(err: super::BytesError) -> Self {
        super::Error::Bytes(err)
    }
}

impl fmt::Debug for Uuid {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl fmt::Display for super::Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            super::Error::Bytes(ref err) => fmt::Display::fmt(&err, f),
            super::Error::Parse(ref err) => fmt::Display::fmt(&err, f),
        }
    }
}

impl fmt::Display for Uuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl fmt::Display for Variant {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Variant::NCS => write!(f, "NCS"),
            Variant::RFC4122 => write!(f, "RFC4122"),
            Variant::Microsoft => write!(f, "Microsoft"),
            Variant::Future => write!(f, "Future"),
        }
    }
}

impl fmt::Display for ::BytesError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "invalid bytes length: expected {}, found {}",
            self.expected(),
            self.found()
        )
    }
}

impl fmt::LowerHex for Uuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(&self.to_hyphenated_ref(), f)
    }
}

impl fmt::UpperHex for Uuid {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::UpperHex::fmt(&self.to_hyphenated_ref(), f)
    }
}

impl str::FromStr for Uuid {
    type Err = parser::ParseError;

    fn from_str(uuid_str: &str) -> Result<Self, Self::Err> {
        Uuid::parse_str(uuid_str)
    }
}

impl Default for Uuid {
    #[inline]
    fn default() -> Self {
        Uuid::nil()
    }
}

#[cfg(test)]
mod tests {
    extern crate std;

    use self::std::prelude::v1::*;
    use prelude::*;
    use test_util;

    macro_rules! check {
        ($buf:ident, $format:expr, $target:expr, $len:expr, $cond:expr) => {
            $buf.clear();
            write!($buf, $format, $target).unwrap();
            assert!($buf.len() == $len);
            assert!($buf.chars().all($cond), "{}", $buf);
        };
    }

    #[test]
    fn test_uuid_compare() {
        let uuid1 = test_util::new();
        let uuid2 = test_util::new2();

        assert_eq!(uuid1, uuid1);
        assert_eq!(uuid2, uuid2);

        assert_ne!(uuid1, uuid2);
        assert_ne!(uuid2, uuid1);
    }

    #[test]
    fn test_uuid_default() {
        let default_uuid = Uuid::default();
        let nil_uuid = Uuid::nil();

        assert_eq!(default_uuid, nil_uuid);
    }

    #[test]
    fn test_uuid_display() {
        use super::fmt::Write;

        let uuid = test_util::new();
        let s = uuid.to_string();
        let mut buffer = String::new();

        assert_eq!(s, uuid.to_hyphenated().to_string());

        check!(buffer, "{}", uuid, 36, |c| c.is_lowercase()
            || c.is_digit(10)
            || c == '-');
    }

    #[test]
    fn test_uuid_lowerhex() {
        use super::fmt::Write;

        let mut buffer = String::new();
        let uuid = test_util::new();

        check!(buffer, "{:x}", uuid, 36, |c| c.is_lowercase()
            || c.is_digit(10)
            || c == '-');
    }

    // noinspection RsAssertEqual
    #[test]
    fn test_uuid_operator_eq() {
        let uuid1 = test_util::new();
        let uuid1_dup = uuid1.clone();
        let uuid2 = test_util::new2();

        assert!(uuid1 == uuid1);
        assert!(uuid1 == uuid1_dup);
        assert!(uuid1_dup == uuid1);

        assert!(uuid1 != uuid2);
        assert!(uuid2 != uuid1);
        assert!(uuid1_dup != uuid2);
        assert!(uuid2 != uuid1_dup);
    }

    #[test]
    fn test_uuid_to_string() {
        use super::fmt::Write;

        let uuid = test_util::new();
        let s = uuid.to_string();
        let mut buffer = String::new();

        assert_eq!(s.len(), 36);

        check!(buffer, "{}", s, 36, |c| c.is_lowercase()
            || c.is_digit(10)
            || c == '-');
    }

    #[test]
    fn test_uuid_upperhex() {
        use super::fmt::Write;

        let mut buffer = String::new();
        let uuid = test_util::new();

        check!(buffer, "{:X}", uuid, 36, |c| c.is_uppercase()
            || c.is_digit(10)
            || c == '-');
    }
}
