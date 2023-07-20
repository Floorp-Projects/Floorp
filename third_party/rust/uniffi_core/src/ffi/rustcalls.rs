/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Low-level support for calling rust functions
//!
//! This module helps the scaffolding code make calls to rust functions and pass back the result to the FFI bindings code.
//!
//! It handles:
//!    - Catching panics
//!    - Adapting `Result<>` types into either a return value or an error

use super::FfiDefault;
use crate::{FfiConverter, RustBuffer, RustBufferFfiConverter};
use anyhow::Result;
use std::mem::MaybeUninit;
use std::panic;

/// Represents the success/error of a rust call
///
/// ## Usage
///
/// - The consumer code creates a `RustCallStatus` with an empty `RustBuffer` and `CALL_SUCCESS`
///   (0) as the status code
/// - A pointer to this object is passed to the rust FFI function.  This is an
///   "out parameter" which will be updated with any error that occurred during the function's
///   execution.
/// - After the call, if `code` is `CALL_ERROR` then `error_buf` will be updated to contain
///   the serialized error object.   The consumer is responsible for freeing `error_buf`.
///
/// ## Layout/fields
///
/// The layout of this struct is important since consumers on the other side of the FFI need to
/// construct it.  If this were a C struct, it would look like:
///
/// ```c,no_run
/// struct RustCallStatus {
///     int8_t code;
///     RustBuffer error_buf;
/// };
/// ```
///
/// #### The `code` field.
///
///  - `CALL_SUCCESS` (0) for successful calls
///  - `CALL_ERROR` (1) for calls that returned an `Err` value
///  - `CALL_PANIC` (2) for calls that panicked
///
/// #### The `error_buf` field.
///
/// - For `CALL_ERROR` this is a `RustBuffer` with the serialized error.  The consumer code is
///   responsible for freeing this `RustBuffer`.
#[repr(C)]
pub struct RustCallStatus {
    pub code: i8,
    // code is signed because unsigned types are experimental in Kotlin
    pub error_buf: MaybeUninit<RustBuffer>,
    // error_buf is MaybeUninit to avoid dropping the value that the consumer code sends in:
    //   - Consumers should send in a zeroed out RustBuffer.  In this case dropping is a no-op and
    //     avoiding the drop is a small optimization.
    //   - If consumers pass in invalid data, then we should avoid trying to drop it.  In
    //     particular, we don't want to try to free any data the consumer has allocated.
    //
    // `MaybeUninit` requires unsafe code, since we are preventing rust from dropping the value.
    // To use this safely we need to make sure that no code paths set this twice, since that will
    // leak the first `RustBuffer`.
}

impl Default for RustCallStatus {
    fn default() -> Self {
        Self {
            code: 0,
            error_buf: MaybeUninit::uninit(),
        }
    }
}

#[allow(dead_code)]
const CALL_SUCCESS: i8 = 0; // CALL_SUCCESS is set by the calling code
const CALL_ERROR: i8 = 1;
const CALL_PANIC: i8 = 2;

// A trait for errors that can be thrown to the FFI code
//
// This gets implemented in uniffi_bindgen/src/scaffolding/templates/ErrorTemplate.rs
pub trait FfiError: RustBufferFfiConverter {}

// Generalized rust call handling function
fn make_call<F, R>(out_status: &mut RustCallStatus, callback: F) -> R
where
    F: panic::UnwindSafe + FnOnce() -> Result<R, RustBuffer>,
    R: FfiDefault,
{
    let result = panic::catch_unwind(|| {
        crate::panichook::ensure_setup();
        callback()
    });
    match result {
        // Happy path.  Note: no need to update out_status in this case because the calling code
        // initializes it to CALL_SUCCESS
        Ok(Ok(v)) => v,
        // Callback returned an Err.
        Ok(Err(buf)) => {
            out_status.code = CALL_ERROR;
            unsafe {
                // Unsafe because we're setting the `MaybeUninit` value, see above for safety
                // invariants.
                out_status.error_buf.as_mut_ptr().write(buf);
            }
            R::ffi_default()
        }
        // Callback panicked
        Err(cause) => {
            out_status.code = CALL_PANIC;
            // Try to coerce the cause into a RustBuffer containing a String.  Since this code can
            // panic, we need to use a second catch_unwind().
            let message_result = panic::catch_unwind(panic::AssertUnwindSafe(move || {
                // The documentation suggests that it will *usually* be a str or String.
                let message = if let Some(s) = cause.downcast_ref::<&'static str>() {
                    (*s).to_string()
                } else if let Some(s) = cause.downcast_ref::<String>() {
                    s.clone()
                } else {
                    "Unknown panic!".to_string()
                };
                log::error!("Caught a panic calling rust code: {:?}", message);
                String::lower(message)
            }));
            if let Ok(buf) = message_result {
                unsafe {
                    // Unsafe because we're setting the `MaybeUninit` value, see above for safety
                    // invariants.
                    out_status.error_buf.as_mut_ptr().write(buf);
                }
            }
            // Ignore the error case.  We've done all that we can at this point.  In the bindings
            // code, we handle this by checking if `error_buf` still has an empty `RustBuffer` and
            // using a generic message.
            R::ffi_default()
        }
    }
}

/// Wrap a rust function call and return the result directly
///
/// `callback` is responsible for making the call to the Rust function.  It must convert any return
/// value into a type that implements `IntoFfi` (typically handled with `FfiConverter::lower()`).
///
/// - If the function succeeds then the function's return value will be returned to the outer code
/// - If the function panics:
///     - `out_status.code` will be set to `CALL_PANIC`
///     - the return value is undefined
pub fn call_with_output<F, R>(out_status: &mut RustCallStatus, callback: F) -> R
where
    F: panic::UnwindSafe + FnOnce() -> R,
    R: FfiDefault,
{
    make_call(out_status, || Ok(callback()))
}

/// Wrap a rust function call that returns a `Result<_, RustBuffer>`
///
/// `callback` is responsible for making the call to the Rust function.
///   - `callback` must convert any return value into a type that implements `IntoFfi`
///   - `callback` must convert any `Error` the into a `RustBuffer` to be returned over the FFI
///   - (Both of these are typically handled with `FfiConverter::lower()`)
///
/// - If the function returns an `Ok` value it will be unwrapped and returned
/// - If the function returns an `Err`:
///     - `out_status.code` will be set to `CALL_ERROR`
///     - `out_status.error_buf` will be set to a newly allocated `RustBuffer` containing the error.  The calling
///       code is responsible for freeing the `RustBuffer`
///     - the return value is undefined
/// - If the function panics:
///     - `out_status.code` will be set to `CALL_PANIC`
///     - the return value is undefined
pub fn call_with_result<F, R>(out_status: &mut RustCallStatus, callback: F) -> R
where
    F: panic::UnwindSafe + FnOnce() -> Result<R, RustBuffer>,
    R: FfiDefault,
{
    make_call(out_status, callback)
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{FfiConverter, RustBufferFfiConverter};

    fn function(a: u8) -> i8 {
        match a {
            0 => 100,
            x => panic!("Unexpected value: {x}"),
        }
    }

    fn create_call_status() -> RustCallStatus {
        RustCallStatus {
            code: 0,
            error_buf: MaybeUninit::new(RustBuffer::new()),
        }
    }

    #[test]
    fn test_call_with_output() {
        let mut status = create_call_status();
        let return_value = call_with_output(&mut status, || function(0));
        assert_eq!(status.code, CALL_SUCCESS);
        assert_eq!(return_value, 100);

        call_with_output(&mut status, || function(1));
        assert_eq!(status.code, CALL_PANIC);
        unsafe {
            assert_eq!(
                String::try_lift(status.error_buf.assume_init()).unwrap(),
                "Unexpected value: 1"
            );
        }
    }

    #[derive(Debug, PartialEq)]
    struct TestError(String);

    // Use RustBufferFfiConverter to simplify lifting TestError out of RustBuffer to check it
    impl RustBufferFfiConverter for TestError {
        type RustType = Self;

        fn write(obj: Self::RustType, buf: &mut Vec<u8>) {
            <String as FfiConverter>::write(obj.0, buf);
        }

        fn try_read(buf: &mut &[u8]) -> Result<Self> {
            String::try_read(buf).map(TestError)
        }
    }

    impl FfiError for TestError {}

    fn function_with_result(a: u8) -> Result<i8, TestError> {
        match a {
            0 => Ok(100),
            1 => Err(TestError("Error".to_owned())),
            x => panic!("Unexpected value: {x}"),
        }
    }

    #[test]
    fn test_call_with_result() {
        let mut status = create_call_status();
        let return_value = call_with_result(&mut status, || {
            function_with_result(0).map_err(TestError::lower)
        });
        assert_eq!(status.code, CALL_SUCCESS);
        assert_eq!(return_value, 100);

        call_with_result(&mut status, || {
            function_with_result(1).map_err(TestError::lower)
        });
        assert_eq!(status.code, CALL_ERROR);
        unsafe {
            assert_eq!(
                TestError::try_lift(status.error_buf.assume_init()).unwrap(),
                TestError("Error".to_owned())
            );
        }

        let mut status = create_call_status();
        call_with_result(&mut status, || {
            function_with_result(2).map_err(TestError::lower)
        });
        assert_eq!(status.code, CALL_PANIC);
        unsafe {
            assert_eq!(
                String::try_lift(status.error_buf.assume_init()).unwrap(),
                "Unexpected value: 2"
            );
        }
    }
}
