/// Verify we're built with run-time integer overflow detection.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#[test]
#[should_panic(expected = "attempt to add with overflow")]
fn overflow_protection() {
    let edge = u32::max_value();
    assert_eq!(0u32, edge + 1);

    let edge = u64::max_value();
    assert_eq!(0u64, edge + 1);
}
