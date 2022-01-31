/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Implementation note: It seems tempting to use if cfg!(feature = ...) directly
// in the macro, but that doesn't work, as the feature would be evaluated
// against the dependent crate.
//
// It'd also seem tempting to export different macros depending on the features
// enabled, but that's also not great because it causes warnings when the
// features are enabled, e.g.:
//
//   nightly_panic!("foo");
//   return SAFE_VALUE;
//
// would cause an unreachable code warning for Nightly builds. So instead we
// choose an exported constant to guard the condition. (For reference, this
// is also how rust's `debug_assert!` is implemented)

/// Whether Nightly-only assertions are enabled.
pub use mozbuild::config::NIGHTLY_BUILD;

/// Whether diagnostic assertions are enabled.
pub use mozbuild::config::MOZ_DIAGNOSTIC_ASSERT_ENABLED;

/// assert! on Nightly, gets compiled out otherwise.
#[macro_export]
macro_rules! nightly_assert {
    ($($arg:tt)*) => (if $crate::NIGHTLY_BUILD { assert!($($arg)*); })
}

/// assert_eq! on Nightly, gets compiled out otherwise.
#[macro_export]
macro_rules! nightly_assert_eq {
    ($($arg:tt)*) => (if $crate::NIGHTLY_BUILD { assert_eq!($($arg)*); })
}

/// assert_ne! on Nightly, gets compiled out otherwise.
#[macro_export]
macro_rules! nightly_assert_ne {
    ($($arg:tt)*) => (if $crate::NIGHTLY_BUILD { assert_ne!($($arg)*); })
}

/// panic! on Nightly, gets compiled out otherwise.
#[macro_export]
macro_rules! nightly_panic {
    ($($arg:tt)*) => (if $crate::NIGHTLY_BUILD { panic!($($arg)*); })
}

/// unreachable! on Nightly, `std::hint::unreachable_unchecked()` otherwise.
///
/// Use carefully! Consider using nightly_panic! and handling the failure
/// gracefully if you can't prove it's really unreachable.
///
/// See https://doc.rust-lang.org/std/hint/fn.unreachable_unchecked.html#safety
/// for safety details.
#[macro_export]
macro_rules! nightly_unreachable {
    ($($arg:tt)*) => {
        if $crate::NIGHTLY_BUILD {
            unreachable!($($arg)*);
        } else {
            std::hint::unreachable_unchecked()
        }
    }
}

/// assert! when diagnostic asserts are enabled, gets compiled out otherwise.
#[macro_export]
macro_rules! diagnostic_assert {
    ($($arg:tt)*) => (if $crate::MOZ_DIAGNOSTIC_ASSERT_ENABLED { assert!($($arg)*); })
}

/// assert_eq! when diagnostic asserts are enabled, gets compiled out otherwise.
#[macro_export]
macro_rules! diagnostic_assert_eq {
    ($($arg:tt)*) => (if $crate::MOZ_DIAGNOSTIC_ASSERT_ENABLED { assert_eq!($($arg)*); })
}

/// assert_ne! when diagnostic asserts are enabled, gets compiled out otherwise.
#[macro_export]
macro_rules! diagnostic_assert_ne {
    ($($arg:tt)*) => (if $crate::MOZ_DIAGNOSTIC_ASSERT_ENABLED { assert_ne!($($arg)*); })
}

/// unreachable! when diagnostic asserts are enabled,
/// `std::hint::unreachable_unchecked()` otherwise.
///
/// Use carefully! Consider using diagnostic_panic! and handling the failure
/// gracefully if you can't prove it's really unreachable.
///
/// See https://doc.rust-lang.org/std/hint/fn.unreachable_unchecked.html#safety
/// for safety details.
#[macro_export]
macro_rules! diagnostic_unreachable {
    ($($arg:tt)*) => {
        if $crate::MOZ_DIAGNOSTIC_ASSERT_ENABLED {
            unreachable!($($arg)*);
        } else {
            std::hint::unreachable_unchecked()
        }
    }
}
