/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! C foreign-function interface for this crate

use super::detect::{get_conflicting_mitigations, ConflictingMitigationStatus};

/// C FFI for `get_conflicting_mitigations`
///
/// Checks the current process to see if any conflicting mitigations would be enabled
/// by Windows Exploit Protection
///
/// `conflicting_mitigations` is an out pointer that will receive the status of the
/// Win32k conflicting mitigations
///
/// On success, returns `1`. On failure, returns `0` and `conflicting_mitigations` is left
/// untouched
///
/// # Safety
///
/// `conflicting_mitigations` must not be a non-const, non-null ptr and **must be initialized**
///
/// # Example
///
/// ```C++
/// void myFunc() {
///     ConflictingMitigationStatus status = {}; // GOOD - value-initializes the object
///     if(!detect_win32k_conflicting_mitigations(&status)) {
///         // error
///     }
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn detect_win32k_conflicting_mitigations(
    conflicting_mitigations: &mut ConflictingMitigationStatus,
) -> bool {
    let process_path =
        std::env::current_exe().expect("unable to determine the path of our executable");
    let file_name = process_path
        .file_name()
        .expect("executable doesn't have a file name");

    match get_conflicting_mitigations(file_name) {
        Ok(status) => {
            *conflicting_mitigations = status;
            true
        }
        Err(e) => {
            log::error!(
                "Failed to get Win32k Lockdown conflicting mitigation status: {}",
                e
            );
            false
        }
    }
}
