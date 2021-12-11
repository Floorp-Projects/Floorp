// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Glean does not pass errors through the FFI component upwards.
//! Most errors are not propagated and simply only logged for debugging.
//! Errors should be recoverable and the platform-side should mostly ignore them.
//! Additionally the platform-side can work asynchronously anyway, leaving us with no good way to
//! pass back errors.
//!
//! The `HandleMapExtension` extension traits adds additional methods that log potential errors.
//!
//! **Note: the platform-side still needs to check for null pointers or other default values before
//! using returned values.
//! This is only relevant for creation of the main object and metrics as its the only things where
//! we return something potentially fallible.

use std::panic::UnwindSafe;

use ffi_support::{ConcurrentHandleMap, ExternError, IntoFfi};

pub fn handle_result<R, F>(callback: F) -> R::Value
where
    F: UnwindSafe + FnOnce() -> Result<R, glean_core::Error>,
    R: IntoFfi,
{
    let mut error = ffi_support::ExternError::success();
    let res = ffi_support::abort_on_panic::call_with_result(&mut error, callback);
    log_if_error(error);
    res
}

/// Warns if an error occurred and then release the allocated memory.
///
/// This is a helper for the case where we aren't exposing this back over the FFI.
///
/// Adopted from the `consume_and_log_if_error` method, but with a changed log message.
///
/// We assume we're not inside a `catch_unwind`, and so we wrap inside one ourselves.
pub fn log_if_error(error: ExternError) {
    if !error.get_code().is_success() {
        // in practice this should never panic, but you never know...
        ffi_support::abort_on_panic::call_with_output(|| {
            log::error!(
                "Glean failed ({:?}): {}",
                error.get_code(),
                error.get_message().as_str()
            );
            unsafe {
                error.manually_release();
            }
        })
    }
}

pub trait HandleMapExtension {
    type Output;

    /// Insert a newly constructed object and return a handle to it.
    ///
    /// This will catch and log any errors.
    /// This will not panic on errors in the constructor.
    ///
    /// On success, it returns a new valid handle.
    /// On failure, it returns the default FFI value for a handler (`0`).
    fn insert_with_log<F>(&self, constructor: F) -> u64
    where
        F: UnwindSafe + FnOnce() -> Result<Self::Output, glean_core::Error>;

    /// Call an infallible callback with the object identified by a handle.
    ///
    /// This will ignore panics.
    ///
    /// On success, it convert the callback return value into an FFI value and returns it.
    /// On failure, it will return the default FFI value.
    fn call_infallible<R, F>(&self, h: u64, callback: F) -> R::Value
    where
        F: UnwindSafe + FnOnce(&Self::Output) -> R,
        R: IntoFfi;

    /// Call an infallible callback with the object identified by a handle.
    ///
    /// This will ignore panics.
    ///
    /// On success, it convert the callback return value into an FFI value and returns it.
    /// On failure, it will return the default FFI value.
    fn call_infallible_mut<R, F>(&self, h: u64, callback: F) -> R::Value
    where
        F: UnwindSafe + FnOnce(&mut Self::Output) -> R,
        R: IntoFfi;

    /// Call a callback with the object identified by a handle.
    ///
    /// This will catch and log any errors of the callback.
    /// This will not panic on errors in the callback.
    ///
    /// On success, it convert the callback return value into an FFI value and returns it.
    /// On failure, it will return the default FFI value.
    fn call_with_log<R, F>(&self, h: u64, callback: F) -> R::Value
    where
        F: UnwindSafe + FnOnce(&Self::Output) -> Result<R, glean_core::Error>,
        R: IntoFfi;
}

impl<T> HandleMapExtension for ConcurrentHandleMap<T> {
    type Output = T;

    fn insert_with_log<F>(&self, constructor: F) -> u64
    where
        F: UnwindSafe + FnOnce() -> Result<Self::Output, glean_core::Error>,
    {
        let mut error = ExternError::success();
        let res = self.insert_with_result(&mut error, constructor);
        log_if_error(error);
        res
    }

    fn call_infallible<R, F>(&self, h: u64, callback: F) -> R::Value
    where
        F: UnwindSafe + FnOnce(&Self::Output) -> R,
        R: IntoFfi,
    {
        let mut error = ExternError::success();
        let res = self.call_with_output(&mut error, h, callback);
        debug_assert!(error.get_code().is_success());
        res
    }

    fn call_infallible_mut<R, F>(&self, h: u64, callback: F) -> R::Value
    where
        F: UnwindSafe + FnOnce(&mut Self::Output) -> R,
        R: IntoFfi,
    {
        let mut error = ExternError::success();
        let res = self.call_with_output_mut(&mut error, h, callback);
        debug_assert!(error.get_code().is_success());
        res
    }

    fn call_with_log<R, F>(&self, h: u64, callback: F) -> R::Value
    where
        F: UnwindSafe + FnOnce(&Self::Output) -> Result<R, glean_core::Error>,
        R: IntoFfi,
    {
        let mut error = ExternError::success();
        let res = self.call_with_result(&mut error, h, callback);
        log_if_error(error);
        res
    }
}
