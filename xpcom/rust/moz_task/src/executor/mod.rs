/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::get_current_thread;
use std::{fmt::Debug, future::Future};
use thiserror::Error;

mod future_task;

pub fn spawn_current_thread<Fut>(future: Fut) -> Result<(), Shutdown>
where
    Fut: Future<Output = ()> + 'static,
{
    let current_thread = get_current_thread().map_err(|_| Shutdown { _priv: () })?;
    // # Safety
    //
    // It's safe to use `local_task` since `future` is dispatched `current_thread`.
    unsafe {
        future_task::local_task(future, current_thread.coerce());
    }
    Ok(())
}

/// An error that occurred during spawning on a shutdown event queue
#[derive(Error, Debug)]
#[error("Event target is shutdown")]
pub struct Shutdown {
    _priv: (),
}
