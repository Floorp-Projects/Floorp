// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use unic_ucd_version::UnicodeVersion;

#[test]
fn test_display() {
    assert_eq!(
        format!(
            "Unicode {}",
            UnicodeVersion {
                major: 1,
                minor: 2,
                micro: 0,
            }
        ),
        "Unicode 1.2.0"
    );
}
