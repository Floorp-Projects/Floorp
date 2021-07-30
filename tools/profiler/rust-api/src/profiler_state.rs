/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Gecko profiler state.

/// Whether the Gecko profiler is currently active.
/// A typical use of this API:
/// ```rust
/// if gecko_profiler::is_active() {
///   // do something.
/// }
/// ```
///
/// This implementation must be kept in sync with
/// `mozilla::profiler::detail::RacyFeatures::IsActive`.
#[cfg(feature = "enabled")]
#[inline]
pub fn is_active() -> bool {
    use crate::gecko_bindings::structs::mozilla::profiler::detail;
    use std::mem;
    use std::sync::atomic::{AtomicU32, Ordering};

    // This is reaching for the C++ atomic value, instead of calling an FFI
    // function to return this value. Because calling an FFI function is much
    // more expensive compared to this method. That's why it's worth to go with
    // this solution for performance. But it's crucial to keep the implementation
    // in sync with the C++ counterpart.
    let active_and_features: &AtomicU32 =
        unsafe { mem::transmute(&detail::RacyFeatures_sActiveAndFeatures) };
    (active_and_features.load(Ordering::Relaxed) & detail::RacyFeatures_Active) != 0
}

/// Always false when the Gecko profiler is disabled.
#[cfg(not(feature = "enabled"))]
#[inline]
pub fn is_active() -> bool {
    false
}
