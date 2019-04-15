/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::{
    error::{BitsTaskError, ErrorCode, ErrorType},
    BitsRequest,
};
use log::{error, info, warn};
use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use xpcom::{
    interfaces::{nsIBitsCallback, nsIBitsNewRequestCallback},
    RefPtr,
};

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum IsCallbackExpected {
    CallbackExpected,
    CallbackOptional,
}
pub use self::IsCallbackExpected::{CallbackExpected, CallbackOptional};

// This is meant to be called at the end of a nsIBits Task. It attempts to
// return the result via the callback given. If the callback is unavailable, a
// log message will be printed indicating the results and (possibly) warning
// than an expected callback was missing.
pub fn maybe_dispatch_request_via_callback(
    result: Result<RefPtr<BitsRequest>, BitsTaskError>,
    maybe_callback: Result<&nsIBitsNewRequestCallback, BitsTaskError>,
    expected: IsCallbackExpected,
) -> Result<(), nsresult> {
    if let Err(error) = maybe_callback.as_ref() {
        if expected == CallbackExpected || error.error_type == ErrorType::CallbackOnWrongThread {
            error!(
                "Unexpected error when {} - No callback: {:?}",
                error.error_action.description(),
                error,
            );
        }
    }
    match result {
        Ok(request) => match (maybe_callback, expected) {
            (Ok(callback), _) => unsafe { callback.Success(request.coerce()) },
            (Err(error), CallbackExpected) => {
                error!(
                    "Success {} but there is no callback to return the result with",
                    error.error_action.description(),
                );
                NS_ERROR_FAILURE
            }
            (Err(error), CallbackOptional) => {
                info!("Success {}", error.error_action.description());
                NS_OK
            }
        },
        Err(error) => match (maybe_callback, expected) {
            (Ok(callback), _) => match error.error_code {
                ErrorCode::None => unsafe {
                    callback.Failure(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                    )
                },
                ErrorCode::Hresult(error_code) => unsafe {
                    callback.FailureHresult(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                        error_code,
                    )
                },
                ErrorCode::Nsresult(error_code) => unsafe {
                    callback.FailureNsresult(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                        error_code,
                    )
                },
                ErrorCode::Message(message) => unsafe {
                    callback.FailureString(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                        &*message,
                    )
                },
            },
            (Err(_), CallbackExpected) => {
                error!("Error {}: {:?}", error.error_action.description(), error);
                NS_ERROR_FAILURE
            }
            (Err(_), CallbackOptional) => {
                warn!("Error {}: {:?}", error.error_action.description(), error);
                NS_ERROR_FAILURE
            }
        },
    }
    .to_result()
}

// Intended to be used by an nsIBits XPCOM wrapper to return errors that occur
// before dispatching a task off-thread. No return value is returned because it
// will represent the return value of the callback function, which should not be
// propagated.
pub fn dispatch_pretask_interface_error(
    error: BitsTaskError,
    callback: &nsIBitsNewRequestCallback,
) {
    let _ = maybe_dispatch_request_via_callback(Err(error), Ok(callback), CallbackExpected);
}

// This is meant to be called at the end of a nsIBitsRequest Task. It attempts
// to return the result via the callback given. If the callback is unavailable,
// a log message will be printed indicating the results and (possibly) warning
// than an expected callback was missing.
pub fn maybe_dispatch_via_callback(
    result: Result<(), BitsTaskError>,
    maybe_callback: Result<&nsIBitsCallback, BitsTaskError>,
    expected: IsCallbackExpected,
) -> Result<(), nsresult> {
    if let Err(error) = maybe_callback.as_ref() {
        if expected == CallbackExpected || error.error_type == ErrorType::CallbackOnWrongThread {
            error!(
                "Unexpected error when {} - No callback: {:?}",
                error.error_action.description(),
                error,
            );
        }
    }
    match result {
        Ok(()) => match (maybe_callback, expected) {
            (Ok(callback), _) => unsafe { callback.Success() },
            (Err(error), CallbackExpected) => {
                error!(
                    "Success {} but there is no callback to return the result with",
                    error.error_action.description(),
                );
                NS_ERROR_FAILURE
            }
            (Err(error), CallbackOptional) => {
                info!("Success {}", error.error_action.description());
                NS_OK
            }
        },
        Err(error) => match (maybe_callback, expected) {
            (Ok(callback), _) => match error.error_code {
                ErrorCode::None => unsafe {
                    callback.Failure(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                    )
                },
                ErrorCode::Hresult(error_code) => unsafe {
                    callback.FailureHresult(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                        error_code,
                    )
                },
                ErrorCode::Nsresult(error_code) => unsafe {
                    callback.FailureNsresult(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                        error_code,
                    )
                },
                ErrorCode::Message(message) => unsafe {
                    callback.FailureString(
                        error.error_type.bits_code(),
                        error.error_action.as_error_code(),
                        error.error_stage.bits_code(),
                        &*message,
                    )
                },
            },
            (Err(_), CallbackExpected) => {
                error!("Error {}: {:?}", error.error_action.description(), error);
                NS_ERROR_FAILURE
            }
            (Err(_), CallbackOptional) => {
                warn!("Error {}: {:?}", error.error_action.description(), error);
                NS_ERROR_FAILURE
            }
        },
    }
    .to_result()
}

// Intended to be used by an nsIBitsRequest XPCOM wrapper to return errors that
// occur before dispatching a task off-thread. No return value is returned
// because it will represent the return value of the callback function, which
// should not be propagated.
pub fn dispatch_pretask_request_error(error: BitsTaskError, callback: &nsIBitsCallback) {
    let _ = maybe_dispatch_via_callback(Err(error), Ok(callback), CallbackExpected);
}
