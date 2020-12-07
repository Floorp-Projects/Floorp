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
    statics::get_database,
};
use crossbeam_utils::atomic::AtomicCell;
use lmdb::Error as LmdbError;
use moz_task::{dispatch_background_task_with_options, DispatchOptions, Task, TaskRunnable};
use nserror::nsresult;
use once_cell::sync::Lazy;
use rkv::{StoreError as RkvStoreError, Value};
use std::{collections::HashMap, sync::Mutex, thread::sleep, time::Duration};
use xpcom::RefPtr;

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
static CHANGES: Lazy<Mutex<Option<HashMap<String, Option<String>>>>> =
    Lazy::new(|| Mutex::new(None));

/// A Mutex that prevents two PersistTasks from running at the same time,
/// since each task opens the database, and we need to ensure there is only
/// one open database handle for the database at any given time.
static PERSIST: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));

/// Synchronously persists changes recorded in memory to disk. Typically
/// called from a background thread, however this can be called from the main
/// thread in Gecko during shutdown (via flush_writes).
fn sync_persist() -> XULStoreResult<()> {
    // Get the map of key/value pairs from the mutex, replacing it
    // with None.  To avoid janking the main thread (if it decides
    // to makes more changes while we're persisting to disk), we only
    // lock the map long enough to move it out of the Mutex.
    let writes = CHANGES.lock()?.take();

    // Return an error early if there's nothing to actually write
    let writes = writes.ok_or(XULStoreError::Unavailable)?;

    let db = get_database()?;
    let mut writer = db.env.write()?;

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
}

pub(crate) fn flush_writes() -> XULStoreResult<()> {
    // One of three things will happen here (barring unexpected errors):
    // - There are no writes queued and the background thread is idle. In which
    //   case, we will get the lock, see that there's nothing to write, and
    //   return (with data in memory and on disk in sync).
    // - There are no writes queued because the background thread is writing
    //   them. In this case, we will block waiting for the lock held by the
    //   writing thread (which will ensure that the changes are flushed), then
    //   discover there are no more to write, and return.
    // - The background thread is busy writing changes, and another thread has
    //   in the mean time added some. In this case, we will block waiting for
    //   the lock held by the writing thread, discover that there are more
    //   changes left, flush them ourselves, and return.
    //
    // This is not airtight, if changes are being added on a different thread
    // than the one calling this. However it should be a reasonably strong
    // guarantee even so.
    let _lock = PERSIST.lock()?;
    match sync_persist() {
        Ok(_) => (),

        // It's no problem (in fact it's generally expected) that there's just
        // nothing to write.
        Err(XULStoreError::Unavailable) => {
            info!("Unable to persist xulstore");
        }

        Err(err) => return Err(err.into()),
    }
    Ok(())
}

pub(crate) fn persist(key: String, value: Option<String>) -> XULStoreResult<()> {
    let mut changes = CHANGES.lock()?;

    if changes.is_none() {
        *changes = Some(HashMap::new());

        // If *changes* was `None`, then this is the first change since
        // the last time we persisted, so dispatch a new PersistTask.
        let task = Box::new(PersistTask::new());
        dispatch_background_task_with_options(
            RefPtr::new(TaskRunnable::new("XULStore::Persist", task)?.coerce()),
            DispatchOptions::default().may_block(true),
        )?;
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
            sync_persist()
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
