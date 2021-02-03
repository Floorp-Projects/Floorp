// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use once_cell::sync::Lazy;
use std::sync::RwLock;

use super::{DispatchError, DispatchGuard, Dispatcher};

pub const GLOBAL_DISPATCHER_LIMIT: usize = 100;
static GLOBAL_DISPATCHER: Lazy<RwLock<Option<Dispatcher>>> =
    Lazy::new(|| RwLock::new(Some(Dispatcher::new(GLOBAL_DISPATCHER_LIMIT))));

/// Get a dispatcher for the global queue.
///
/// A dispatcher is cheap to create, so we create one on every access instead of caching it.
/// This avoids troubles for tests where the global dispatcher _can_ change.
fn guard() -> DispatchGuard {
    GLOBAL_DISPATCHER
        .read()
        .unwrap()
        .as_ref()
        .map(|dispatcher| dispatcher.guard())
        .unwrap()
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

/// Block until all tasks prior to this call are processed.
pub fn block_on_queue() {
    guard().block_on_queue();
}

/// Starts processing queued tasks in the global dispatch queue.
///
/// This function blocks until queued tasks prior to this call are finished.
/// Once the initial queue is empty the dispatcher will wait for new tasks to be launched.
///
/// # Returns
///
/// Returns the total number of items that were added to the queue before being flushed,
/// or an error if the queue couldn't be flushed.
pub fn flush_init() -> Result<usize, DispatchError> {
    guard().flush_init()
}

fn join_dispatcher_thread() -> Result<(), DispatchError> {
    // After we issue the shutdown command, make sure to wait for the
    // worker thread to join.
    let mut lock = GLOBAL_DISPATCHER.write().unwrap();
    let dispatcher = lock.as_mut().expect("Global dispatcher has gone missing");

    if let Some(worker) = dispatcher.worker.take() {
        return worker.join().map_err(|_| DispatchError::WorkerPanic);
    }

    Ok(())
}

/// Kill the blocked dispatcher without processing the queue.
///
/// This will immediately shutdown the worker thread
/// and no other tasks will be processed.
/// This only has an effect when the queue is still blocked.
pub fn kill() -> Result<(), DispatchError> {
    guard().kill()?;
    join_dispatcher_thread()
}

/// Shuts down the dispatch queue.
///
/// This will initiate a shutdown of the worker thread
/// and no new tasks will be processed after this.
pub fn shutdown() -> Result<(), DispatchError> {
    guard().shutdown()?;
    join_dispatcher_thread()
}

/// TEST ONLY FUNCTION.
/// Resets the Glean state and triggers init again.
pub(crate) fn reset_dispatcher() {
    // We don't care about shutdown errors, since they will
    // definitely happen if this
    let _ = shutdown();

    // Now that the dispatcher is shut down, replace it.
    // For that we
    // 1. Create a new
    // 2. Replace the global one
    // 3. Only then return (and thus release the lock)
    let mut lock = GLOBAL_DISPATCHER.write().unwrap();
    let new_dispatcher = Some(Dispatcher::new(GLOBAL_DISPATCHER_LIMIT));
    *lock = new_dispatcher;
}

#[cfg(test)]
mod test {
    use std::sync::{Arc, Mutex};

    use super::*;

    #[test]
    #[ignore] // We can't reset the queue at the moment, so filling it up breaks other tests.
    fn global_fills_up_in_order_and_works() {
        let _ = env_logger::builder().is_test(true).try_init();

        let result = Arc::new(Mutex::new(vec![]));

        for i in 1..=GLOBAL_DISPATCHER_LIMIT {
            let result = Arc::clone(&result);
            launch(move || {
                result.lock().unwrap().push(i);
            });
        }

        {
            let result = Arc::clone(&result);
            launch(move || {
                result.lock().unwrap().push(150);
            });
        }

        flush_init().unwrap();

        {
            let result = Arc::clone(&result);
            launch(move || {
                result.lock().unwrap().push(200);
            });
        }

        block_on_queue();

        let mut expected = (1..=GLOBAL_DISPATCHER_LIMIT).collect::<Vec<_>>();
        expected.push(200);
        assert_eq!(&*result.lock().unwrap(), &expected);
    }

    #[test]
    #[ignore] // We can't reset the queue at the moment, so flushing it breaks other tests.
    fn global_nested_calls() {
        let _ = env_logger::builder().is_test(true).try_init();

        let result = Arc::new(Mutex::new(vec![]));

        {
            let result = Arc::clone(&result);
            launch(move || {
                result.lock().unwrap().push(1);
            });
        }

        flush_init().unwrap();

        {
            let result = Arc::clone(&result);
            launch(move || {
                result.lock().unwrap().push(21);

                {
                    let result = Arc::clone(&result);
                    launch(move || {
                        result.lock().unwrap().push(3);
                    });
                }

                result.lock().unwrap().push(22);
            });
        }

        block_on_queue();

        let expected = vec![1, 21, 22, 3];
        assert_eq!(&*result.lock().unwrap(), &expected);
    }
}
