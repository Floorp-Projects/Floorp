/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::{
    action::Action,
    error::{BitsTaskError, ErrorStage::CommandThread, ErrorType::MissingBitsClient},
    string::nsCString_to_OsString,
};

use bits_client::BitsClient;
use nsstring::nsCString;
use std::cell::Cell;

thread_local! {
    // This is used to store the `BitsClient` on the Command thread.
    // Keeping it here solves the problem of how to allow multiple runnables to
    // be simultaneously queued on the Command thread while giving them all
    // access to the same `BitsClient`.
    static BITS_CLIENT: Cell<Option<BitsClient>> = Cell::new(None);
}

/// This structure holds the data needed to initialize `BitsClient` and
/// `BitsMonitorClient`.
#[derive(Debug, Clone)]
pub struct ClientInitData {
    pub job_name: nsCString,
    pub save_path_prefix: nsCString,
    pub monitor_timeout_ms: u32,
}

impl ClientInitData {
    pub fn new(
        job_name: nsCString,
        save_path_prefix: nsCString,
        monitor_timeout_ms: u32,
    ) -> ClientInitData {
        ClientInitData {
            job_name,
            save_path_prefix,
            monitor_timeout_ms,
        }
    }
}

/// This function constructs a `BitsClient`, if one does not already exist. If
/// the `BitsClient` cannot be constructed, a `BitsTaskError` will be returned.
/// If the `BitsClient` could be obtained, then the function then calls the
/// closure passed to it, passing a mutable reference to the `BitsClient`.
/// This function will then return whatever the closure returned, which must be
/// a `Result<_, BitsTaskError>`.
pub fn with_maybe_new_bits_client<F, R>(
    init_data: &ClientInitData,
    action: Action,
    closure: F,
) -> Result<R, BitsTaskError>
where
    F: FnOnce(&mut BitsClient) -> Result<R, BitsTaskError>,
{
    _with_bits_client(Some(init_data), action, closure)
}

/// This function assumes that a `BitsClient` has already been constructed. If
/// there is not one available, a `BitsTaskError` will be returned. Otherwise,
/// the function calls the closure passed to it, passing a mutable reference to
/// the `BitsClient`. This function will then return whatever the closure
/// returned, which must be a `Result<_, BitsTaskError>`.
pub fn with_bits_client<F, R>(action: Action, closure: F) -> Result<R, BitsTaskError>
where
    F: FnOnce(&mut BitsClient) -> Result<R, BitsTaskError>,
{
    _with_bits_client(None, action, closure)
}

fn _with_bits_client<F, R>(
    maybe_init_data: Option<&ClientInitData>,
    action: Action,
    closure: F,
) -> Result<R, BitsTaskError>
where
    F: FnOnce(&mut BitsClient) -> Result<R, BitsTaskError>,
{
    BITS_CLIENT.with(|cell| {
        let maybe_client = cell.take();
        let mut client = match (maybe_client, maybe_init_data) {
            (Some(r), _) => r,
            (None, Some(init_data)) => {
                // Immediately invoked function to allow for the ? operator
                BitsClient::new(
                    nsCString_to_OsString(&init_data.job_name, action, CommandThread)?,
                    nsCString_to_OsString(&init_data.save_path_prefix, action, CommandThread)?,
                )
                .map_err(|pipe_error| BitsTaskError::from_pipe(action, pipe_error))?
            }
            (None, None) => {
                return Err(BitsTaskError::new(MissingBitsClient, action, CommandThread));
            }
        };
        let result = closure(&mut client);
        cell.set(Some(client));
        result
    })
}
