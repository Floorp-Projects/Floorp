// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use once_cell::sync::{Lazy, OnceCell};
use std::sync::RwLock;

use super::{DispatchError, DispatchGuard, Dispatcher};

const GLOBAL_DISPATCHER_LIMIT: usize = 100;
static GLOBAL_DISPATCHER: Lazy<RwLock<Option<Dispatcher>>> =
    Lazy::new(|| RwLock::new(Some(Dispatcher::new(GLOBAL_DISPATCHER_LIMIT))));

fn guard() -> &'static DispatchGuard {
    static GLOBAL_GUARD: OnceCell<DispatchGuard> = OnceCell::new();

    GLOBAL_GUARD.get_or_init(|| {
        let lock = GLOBAL_DISPATCHER.read().unwrap();
        lock.as_ref().unwrap().guard()
    })
}

/// Launches a new task on the global dispatch queue.
///
/// The new task will be enqueued immediately.
/// If the pre-init queue was already flushed,
/// the background thread will process tasks in the queue (see [`flush_init`]).
///
/// This will not block.
///
/// [`flush_init`]: fn.flush_init.html
pub fn launch(task: impl FnOnce() + Send + 'static) {
    match guard().launch(task) {
        Ok(_) => {}
        Err(DispatchError::QueueFull) => {
            log::info!("Exceeded maximum queue size, discarding task");
            // TODO: Record this as an error.
        }
        Err(_) => {
            log::info!("Failed to launch a task on the queue. Discarding task.");
        }
    }
}

/// Starts processing queued tasks in the global dispatch queue.
///
/// This function blocks until queued tasks prior to this call are finished.
/// Once the initial queue is empty the dispatcher will wait for new tasks to be launched.
pub fn flush_init() -> Result<(), DispatchError> {
    GLOBAL_DISPATCHER
        .write()
        .unwrap()
        .as_mut()
        .map(|dispatcher| dispatcher.flush_init())
        .unwrap()
}

/// Shuts down the dispatch queue.
///
/// This will initiate a shutdown of the worker thread
/// and no new tasks will be processed after this.
/// It will not block on the worker thread.
pub fn try_shutdown() -> Result<(), DispatchError> {
    guard().shutdown()
}
