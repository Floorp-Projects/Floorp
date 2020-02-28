// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! *Version* data types.

use core::fmt;

#[cfg(feature = "unstable")]
use core::char;

/// Represents a *Unicode Version* type.
///
/// UNIC's *Unicode Version* type is used for Unicode datasets and specifications, including The
/// Unicode Standard (TUS), Unicode Character Database (UCD), Common Local Data Repository (CLDR),
/// IDNA, Emoji, etc.
///
/// TODO: *Unicode Version* is guaranteed to have three integer fields between 0 and 255. We are
/// going to switch over to `u8` after Unicode 11.0.0 release.
///
/// Refs:
/// - <https://www.unicode.org/versions/>
/// - <https://www.unicode.org/L2/L2017/17222.htm#152-C3>
#[derive(Clone, Copy, Eq, PartialEq, Ord, PartialOrd, Debug, Hash, Default)]
pub struct UnicodeVersion {
    /// Major version.
    pub major: u16,

    /// Minor version.
    pub minor: u16,

    /// Micro (or Update) version.
    pub micro: u16,
}

impl fmt::Display for UnicodeVersion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.micro)
    }
}

#[cfg(feature = "unstable")]
/// Convert from Rust's internal Unicode Version.
impl From<char::UnicodeVersion> for UnicodeVersion {
    fn from(value: char::UnicodeVersion) -> UnicodeVersion {
        UnicodeVersion {
            major: value.major as u16,
            minor: value.minor as u16,
            micro: value.micro as u16,
        }
    }
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "unstable")]
    #[test]
    fn test_against_rust_internal() {
        use core::char;

        use super::UnicodeVersion;

        let core_unicode_version: UnicodeVersion = char::UNICODE_VERSION.into();
        assert!(core_unicode_version.major >= 10);
    }
}
