/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The XULStore API is synchronous for both C++ and JS consumers and accessed
//! on the main thread, so we persist its data to disk on a background thread
//! to avoid janking the UI.
//!
//! We also re-open the database each time we write to it in order to conserve
//! heap memory, since holding a database connection open would consume at least
//! 3MB of heap memory in perpetuity.
//!
//! Since re-opening the database repeatedly to write individual changes can be
//! expensive when there are many of them in quick succession, we batch changes
//! and write them in batches.

use crate::{
    error::{XULStoreError, XULStoreResult},
    ffi::XpcomShutdownObserver,
    statics::get_database,
};
use crossbeam_utils::atomic::AtomicCell;
use lmdb::Error as LmdbError;
use moz_task::{create_thread, Task, TaskRunnable};
use nserror::nsresult;
use rkv::{StoreError as RkvStoreError, Value};
use std::{collections::HashMap, sync::Mutex, thread::sleep, time::Duration};
use xpcom::{interfaces::nsIThread, RefPtr, ThreadBoundRefPtr};

lazy_static! {
    /// A map of key/value pairs to persist.  Values are Options so we can
    /// use the same structure for both puts and deletes, with a `None` value
    /// identifying a key that should be deleted from the database.
    ///
    /// This is a map rather than a sequence in order to merge consecutive
    /// changes to the same key, i.e. when a consumer sets *foo* to `bar`
    /// and then sets it again to `baz` before we persist the first change.
    ///
    /// In that case, there's no point in setting *foo* to `bar` before we set
    /// it to `baz`, and the map ensures we only ever persist the latest value
    /// for any given key.
    static ref CHANGES: Mutex<Option<HashMap<String, Option<String>>>> = { Mutex::new(None) };

    /// A Mutex that prevents two PersistTasks from running at the same time,
    /// since each task opens the database, and we need to ensure there is only
    /// one open database handle for the database at any given time.
    static ref PERSIST: Mutex<()> = { Mutex::new(()) };

    static ref THREAD: Mutex<Option<ThreadBoundRefPtr<nsIThread>>> = {
        let thread: RefPtr<nsIThread> = match create_thread("XULStore") {
            Ok(thread) => thread,
            Err(err) => {
                error!("error creating XULStore thread: {}", err);
                return Mutex::new(None);
            }
        };

        // Observe XPCOM shutdown so we can clear the thread and thus not
        // "leak" it (from the perspective of the leak checker).
        observe_xpcom_shutdown();

        Mutex::new(Some(ThreadBoundRefPtr::new(thread)))
    };
}

fn observe_xpcom_shutdown() {
    (|| -> XULStoreResult<()> {
        let obs_svc = xpcom::services::get_ObserverService().ok_or(XULStoreError::Unavailable)?;
        let observer = XpcomShutdownObserver::new();
        unsafe {
            obs_svc
                .AddObserver(observer.coerce(), cstr!("xpcom-shutdown").as_ptr(), false)
                .to_result()?
        };
        Ok(())
    })()
    .unwrap_or_else(|err| error!("error observing XPCOM shutdown: {}", err));
}

pub(crate) fn clear_on_shutdown() {
    (|| -> XULStoreResult<()> {
        THREAD.lock()?.take();
        Ok(())
    })()
    .unwrap_or_else(|err| error!("error clearing thread: {}", err));
}

pub(crate) fn persist(key: String, value: Option<String>) -> XULStoreResult<()> {
    let mut changes = CHANGES.lock()?;

    if changes.is_none() {
        *changes = Some(HashMap::new());

        // If *changes* was `None`, then this is the first change since
        // the last time we persisted, so dispatch a new PersistTask.
        let task = Box::new(PersistTask::new());
        let thread_guard = THREAD.lock()?;
        let thread = thread_guard
            .as_ref()
            .ok_or(XULStoreError::Unavailable)?
            .get_ref()
            .ok_or(XULStoreError::Unavailable)?;
        TaskRunnable::new("XULStore::Persist", task)?.dispatch(thread)?;
    }

    // Now insert the key/value pair into the map.  The unwrap() call here
    // should never panic, since the code above sets `writes` to a Some(HashMap)
    // if it's None.
    changes.as_mut().unwrap().insert(key, value);

    Ok(())
}

pub struct PersistTask {
    result: AtomicCell<Option<Result<(), XULStoreError>>>,
}

impl PersistTask {
    pub fn new() -> PersistTask {
        PersistTask {
            result: AtomicCell::default(),
        }
    }
}

impl Task for PersistTask {
    fn run(&self) {
        self.result.store(Some(|| -> Result<(), XULStoreError> {
            // Avoid persisting too often.  We might want to adjust this value
            // in the future to trade durability for performance.
            sleep(Duration::from_millis(200));

            // Prevent another PersistTask from running until this one finishes.
            // We do this before getting the database to ensure that there is
            // only ever one open database handle at a given time.
            let _lock = PERSIST.lock()?;

            let db = get_database()?;
            let mut writer = db.env.write()?;

            // Get the map of key/value pairs from the mutex, replacing it
            // with None.  To avoid janking the main thread (if it decides
            // to makes more changes while we're persisting to disk), we only
            // lock the map long enough to move it out of the Mutex.
            let writes = CHANGES.lock()?.take();

            // The Option should be a Some(HashMap) (otherwise the task
            // shouldn't have been scheduled in the first place).  If it's None,
            // unexpectedly, then we return an error early.
            let writes = writes.ok_or(XULStoreError::Unavailable)?;

            for (key, value) in writes.iter() {
                match value {
                    Some(val) => db.store.put(&mut writer, &key, &Value::Str(val))?,
                    None => {
                        match db.store.delete(&mut writer, &key) {
                            Ok(_) => (),

                            // The XULStore API doesn't care if a consumer tries
                            // to remove a value that doesn't exist in the store,
                            // so we ignore the error (although in this case the key
                            // should exist, since it was in the cache!).
                            Err(RkvStoreError::LmdbError(LmdbError::NotFound)) => {
                                warn!("tried to remove key that isn't in the store");
                            }

                            Err(err) => return Err(err.into()),
                        }
                    }
                }
            }

            writer.commit()?;

            Ok(())
        }()));
    }

    fn done(&self) -> Result<(), nsresult> {
        match self.result.swap(None) {
            Some(Ok(())) => (),
            Some(Err(err)) => error!("removeDocument error: {}", err),
            None => error!("removeDocument error: unexpected result"),
        };

        Ok(())
    }
}
