/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::{
    action::Action,
    error::{BitsTaskError, ErrorStage, ErrorType},
};
use log::warn;
use xpcom::{RefCounted, ThreadBoundRefPtr};

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum DataType {
    Callback,
    BitsService,
    BitsRequest,
    Observer,
    Context,
}

#[derive(Debug, PartialEq, Clone, Copy)]
enum GetThreadboundError {
    Missing,
    WrongThread,
}

impl DataType {
    fn error_type(&self, error: GetThreadboundError) -> ErrorType {
        match self {
            DataType::Callback => match error {
                GetThreadboundError::Missing => ErrorType::MissingCallback,
                GetThreadboundError::WrongThread => ErrorType::CallbackOnWrongThread,
            },
            DataType::BitsService => match error {
                GetThreadboundError::Missing => ErrorType::MissingBitsService,
                GetThreadboundError::WrongThread => ErrorType::BitsServiceOnWrongThread,
            },
            DataType::BitsRequest => match error {
                GetThreadboundError::Missing => ErrorType::MissingBitsRequest,
                GetThreadboundError::WrongThread => ErrorType::BitsRequestOnWrongThread,
            },
            DataType::Observer => match error {
                GetThreadboundError::Missing => ErrorType::MissingObserver,
                GetThreadboundError::WrongThread => ErrorType::ObserverOnWrongThread,
            },
            DataType::Context => match error {
                GetThreadboundError::Missing => ErrorType::MissingContext,
                GetThreadboundError::WrongThread => ErrorType::ContextOnWrongThread,
            },
        }
    }

    fn name(&self) -> &'static str {
        match self {
            DataType::Callback => "Callback",
            DataType::BitsService => "BITS Service",
            DataType::BitsRequest => "BITS Request",
            DataType::Observer => "Observer",
            DataType::Context => "Context",
        }
    }
}

/// Given a reference to a threadbound option
/// (i.e. `&Option<ThreadBoundRefPtr<_>>`), this function will attempt to
/// retrieve a reference to the value stored within. If it is not available
/// (option is `None` or value is on the wrong thread), `None` is returned
/// instead.
pub fn get_from_threadbound_option<T>(
    maybe_threadbound: &Option<ThreadBoundRefPtr<T>>,
    data_type: DataType,
    action: Action,
) -> Option<&T>
where
    T: RefCounted + 'static,
{
    maybe_threadbound.as_ref().and_then(|threadbound| {
        let maybe_reference = threadbound.get_ref();
        if maybe_reference.is_none() {
            warn!(
                "Unexpected error {}: {} is on the wrong thread",
                action.description(),
                data_type.name(),
            );
        }
        maybe_reference
    })
}

/// Given a reference to a threadbound option
/// (i.e. `&Option<ThreadBoundRefPtr<_>>`), this function will attempt to
/// retrieve a reference to the value stored within. If it is not available
/// (option is `None` or value is on the wrong thread), a `BitsTaskError` is
/// returned instead.
pub fn expect_from_threadbound_option<T>(
    maybe_threadbound: &Option<ThreadBoundRefPtr<T>>,
    data_type: DataType,
    action: Action,
) -> Result<&T, BitsTaskError>
where
    T: RefCounted + 'static,
{
    match maybe_threadbound.as_ref() {
        Some(threadbound) => {
            match threadbound.get_ref() {
                Some(reference) => Ok(reference),
                None => Err(BitsTaskError::new(
                    data_type.error_type(GetThreadboundError::WrongThread),
                    action,
                    // Retrieving data from threadbounds all happens on the main thread.
                    // No data is ever bound to other threads so there would be no
                    // reason to retrieve it there.
                    ErrorStage::MainThread,
                )),
            }
        }
        None => Err(BitsTaskError::new(
            data_type.error_type(GetThreadboundError::Missing),
            action,
            // Retrieving data from threadbounds all happens on the main thread.
            // No data is ever bound to other threads so there would be no
            // reason to retrieve it there.
            ErrorStage::MainThread,
        )),
    }
}
