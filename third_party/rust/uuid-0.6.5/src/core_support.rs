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

use prelude::*;

cfg_if! {
    if #[cfg(feature = "std")] {
        use std::fmt;
        use std::str;
    } else {
        use core::fmt;
        use core::str;
    }
}

impl fmt::Display for Uuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl fmt::Display for UuidVariant {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            UuidVariant::NCS => write!(f, "NCS"),
            UuidVariant::RFC4122 => write!(f, "RFC4122"),
            UuidVariant::Microsoft => write!(f, "Microsoft"),
            UuidVariant::Future => write!(f, "Future"),
        }
    }
}

impl fmt::LowerHex for Uuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <super::Hyphenated as fmt::LowerHex>::fmt(&self.hyphenated(), f)
    }
}

impl fmt::UpperHex for Uuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <super::Hyphenated as fmt::UpperHex>::fmt(&self.hyphenated(), f)
    }
}

impl str::FromStr for Uuid {
    type Err = super::ParseError;

    fn from_str(uuid_str: &str) -> Result<Uuid, super::ParseError> {
        Uuid::parse_str(uuid_str)
    }
}

impl Default for Uuid {
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

        assert_eq!(s, uuid.hyphenated().to_string());

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
