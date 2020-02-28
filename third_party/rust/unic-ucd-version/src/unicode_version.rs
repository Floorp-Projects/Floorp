// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use unic_common::version::UnicodeVersion;

/// The [Version of The Unicode Standard](https://www.unicode.org/versions/) of the Unicode
/// Character Database in use.
pub const UNICODE_VERSION: UnicodeVersion = include!("../tables/unicode_version.rsv");

#[cfg(test)]
mod tests {
    use super::UNICODE_VERSION;

    #[test]
    fn validate_version_values() {
        assert!(UNICODE_VERSION.major > 0);

        // Current release schedule of Unicode is to have one Major version update each year, with
        // no Minor updates. We hard-code this internal policy while it stands.
        assert!(UNICODE_VERSION.minor == 0);
    }
}
