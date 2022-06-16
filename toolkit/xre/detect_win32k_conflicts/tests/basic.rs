/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use detect_win32k_conflicts::{detect_win32k_conflicting_mitigations, ConflictingMitigationStatus};

#[test]
fn basic() {
    let mut status = ConflictingMitigationStatus::default();

    let result = unsafe { detect_win32k_conflicting_mitigations(&mut status) };

    assert!(result);
}
