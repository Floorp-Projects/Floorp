/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    borrow::Borrow,
    fmt::Write,
    mem, result, str,
    sync::{Arc, Weak},
};

use atomic_refcell::AtomicRefCell;
use moz_task::{Task, ThreadPtrHandle, ThreadPtrHolder};
use nserror::nsresult;
use nsstring::nsCString;
use serde::Serialize;
use serde_json::Value as JsonValue;
use storage_variant::VariantType;
use xpcom::{
    interfaces::{mozIExtensionStorageCallback, mozIExtensionStorageListener},
    RefPtr, XpCom,
};

use crate::error::{Error, Result};
use crate::store::LazyStore;

/// A storage operation that's punted from the main thread to the background
/// task queue.
pub enum Punt {
    /// Get the values of the keys for an extension.
    Get { ext_id: String, keys: JsonValue },
    /// Set a key-value pair for an extension.
    Set { ext_id: String, value: JsonValue },
    /// Remove one or more keys for an extension.
    Remove { ext_id: String, keys: JsonValue },
    /// Clear all keys and values for an extension.
    Clear { ext_id: String },
    /// Returns the bytes in use for the specified, or all, keys.
    GetBytesInUse { ext_id: String, keys: JsonValue },
    /// Fetches all pending Sync change notifications to pass to
    /// `storage.onChanged` listeners.
    FetchPendingSyncChanges,
    /// Fetch-and-delete (e.g. `take`) information about the migration from the
    /// kinto-based extension-storage to the rust-based storage.
    ///
    /// This data is stored in the database instead of just being returned by
    /// the call to `migrate`, as we may migrate prior to telemetry being ready.
    TakeMigrationInfo,
}

impl Punt {
    /// Returns the operation name, used to label the task runnable and report
    /// errors.
    pub fn name(&self) -> &'static str {
        match self {
            Punt::Get { .. } => "webext_storage::get",
            Punt::Set { .. } => "webext_storage::set",
            Punt::Remove { .. } => "webext_storage::remove",
            Punt::Clear { .. } => "webext_storage::clear",
            Punt::GetBytesInUse { .. } => "webext_storage::get_bytes_in_use",
            Punt::FetchPendingSyncChanges => "webext_storage::fetch_pending_sync_changes",
            Punt::TakeMigrationInfo => "webext_storage::take_migration_info",
        }
    }
}

/// A storage operation result, punted from the background queue back to the
/// main thread.
#[derive(Default)]
struct PuntResult {
    changes: Vec<Change>,
    value: Option<String>,
}

/// A change record for an extension.
struct Change {
    ext_id: String,
    json: String,
}

impl PuntResult {
    /// Creates a result with a single change to pass to `onChanged`, and no
    /// return value for `handleSuccess`. The `Borrow` bound lets this method
    /// take either a borrowed reference or an owned value.
    fn with_change<T: Borrow<S>, S: Serialize>(ext_id: &str, changes: T) -> Result<Self> {
        Ok(PuntResult {
            changes: vec![Change {
                ext_id: ext_id.into(),
                json: serde_json::to_string(changes.borrow())?,
            }],
            value: None,
        })
    }

    /// Creates a result with changes for multiple extensions to pass to
    /// `onChanged`, and no return value for `handleSuccess`.
    fn with_changes(changes: Vec<Change>) -> Self {
        PuntResult {
            changes,
            value: None,
        }
    }

    /// Creates a result with no changes to pass to `onChanged`, and a return
    /// value for `handleSuccess`.
    fn with_value<T: Borrow<S>, S: Serialize>(value: T) -> Result<Self> {
        Ok(PuntResult {
            changes: Vec::new(),
            value: Some(serde_json::to_string(value.borrow())?),
        })
    }
}

/// A generic task used for all storage operations. Punts the operation to the
/// background task queue, receives a result back on the main thread, and calls
/// the callback with it.
pub struct PuntTask {
    name: &'static str,
    /// Storage tasks hold weak references to the store, which they upgrade
    /// to strong references when running on the background queue. This
    /// ensures that pending storage tasks don't block teardown (for example,
    /// if a consumer calls `get` and then `teardown`, without waiting for
    /// `get` to finish).
    store: Weak<LazyStore>,
    punt: AtomicRefCell<Option<Punt>>,
    callback: ThreadPtrHandle<mozIExtensionStorageCallback>,
    result: AtomicRefCell<Result<PuntResult>>,
}

impl PuntTask {
    /// Creates a storage task that punts an operation to the background queue.
    /// Returns an error if the task couldn't be created because the thread
    /// manager is shutting down.
    pub fn new(
        store: Weak<LazyStore>,
        punt: Punt,
        callback: &mozIExtensionStorageCallback,
    ) -> Result<Self> {
        let name = punt.name();
        Ok(Self {
            name,
            store,
            punt: AtomicRefCell::new(Some(punt)),
            callback: ThreadPtrHolder::new(
                cstr!("mozIExtensionStorageCallback"),
                RefPtr::new(callback),
            )?,
            result: AtomicRefCell::new(Err(Error::DidNotRun(name))),
        })
    }

    /// Upgrades the task's weak `LazyStore` reference to a strong one. Returns
    /// an error if the store has been torn down.
    ///
    /// It's important that this is called on the background queue, after the
    /// task has been dispatched. Storage tasks shouldn't hold strong references
    /// to the store on the main thread, because then they might block teardown.
    fn store(&self) -> Result<Arc<LazyStore>> {
        match self.store.upgrade() {
            Some(store) => Ok(store),
            None => Err(Error::AlreadyTornDown),
        }
    }

    /// Runs this task's storage operation on the background queue.
    fn inner_run(&self, punt: Punt) -> Result<PuntResult> {
        Ok(match punt {
            Punt::Set { ext_id, value } => {
                PuntResult::with_change(&ext_id, self.store()?.get()?.set(&ext_id, value)?)?
            }
            Punt::Get { ext_id, keys } => {
                PuntResult::with_value(self.store()?.get()?.get(&ext_id, keys)?)?
            }
            Punt::Remove { ext_id, keys } => {
                PuntResult::with_change(&ext_id, self.store()?.get()?.remove(&ext_id, keys)?)?
            }
            Punt::Clear { ext_id } => {
                PuntResult::with_change(&ext_id, self.store()?.get()?.clear(&ext_id)?)?
            }
            Punt::GetBytesInUse { ext_id, keys } => {
                PuntResult::with_value(self.store()?.get()?.get_bytes_in_use(&ext_id, keys)?)?
            }
            Punt::FetchPendingSyncChanges => PuntResult::with_changes(
                self.store()?
                    .get()?
                    .get_synced_changes()?
                    .into_iter()
                    .map(|info| Change {
                        ext_id: info.ext_id,
                        json: info.changes,
                    })
                    .collect(),
            ),
            Punt::TakeMigrationInfo => {
                PuntResult::with_value(self.store()?.get()?.take_migration_info()?)?
            }
        })
    }
}

impl Task for PuntTask {
    fn run(&self) {
        *self.result.borrow_mut() = match self.punt.borrow_mut().take() {
            Some(punt) => self.inner_run(punt),
            // A task should never run on the background queue twice, but we
            // return an error just in case.
            None => Err(Error::AlreadyRan(self.name)),
        };
    }

    fn done(&self) -> result::Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        // As above, `done` should never be called multiple times, but we handle
        // that by returning an error.
        match mem::replace(
            &mut *self.result.borrow_mut(),
            Err(Error::AlreadyRan(self.name)),
        ) {
            Ok(PuntResult { changes, value }) => {
                // If we have change data, and the callback implements the
                // listener interface, notify about it first.
                if let Some(listener) = callback.query_interface::<mozIExtensionStorageListener>() {
                    for Change { ext_id, json } in changes {
                        // Ignore errors.
                        let _ = unsafe {
                            listener.OnChanged(&*nsCString::from(ext_id), &*nsCString::from(json))
                        };
                    }
                }
                let result = value.map(nsCString::from).into_variant();
                unsafe { callback.HandleSuccess(result.coerce()) }
            }
            Err(err) => {
                let mut message = nsCString::new();
                write!(message, "{}", err).unwrap();
                unsafe { callback.HandleError(err.into(), &*message) }
            }
        }
        .to_result()
    }
}

/// A task to tear down the store on the background task queue.
pub struct TeardownTask {
    /// Unlike storage tasks, the teardown task holds a strong reference to
    /// the store, which it drops on the background queue. This is the only
    /// task that should do that.
    store: AtomicRefCell<Option<Arc<LazyStore>>>,
    callback: ThreadPtrHandle<mozIExtensionStorageCallback>,
    result: AtomicRefCell<Result<()>>,
}

impl TeardownTask {
    /// Creates a teardown task. This should only be created and dispatched
    /// once, to clean up the store at shutdown. Returns an error if the task
    /// couldn't be created because the thread manager is shutting down.
    pub fn new(store: Arc<LazyStore>, callback: &mozIExtensionStorageCallback) -> Result<Self> {
        Ok(Self {
            store: AtomicRefCell::new(Some(store)),
            callback: ThreadPtrHolder::new(
                cstr!("mozIExtensionStorageCallback"),
                RefPtr::new(callback),
            )?,
            result: AtomicRefCell::new(Err(Error::DidNotRun(Self::name()))),
        })
    }

    /// Returns the task name, used to label its runnable and report errors.
    pub fn name() -> &'static str {
        "webext_storage::teardown"
    }

    /// Tears down and drops the store on the background queue.
    fn inner_run(&self, store: Arc<LazyStore>) -> Result<()> {
        // At this point, we should be holding the only strong reference
        // to the store, since 1) `StorageSyncArea` gave its one strong
        // reference to our task, and 2) we're running on a background
        // task queue, which runs all tasks sequentially...so no other
        // `PuntTask`s should be running and trying to upgrade their
        // weak references. So we can unwrap the `Arc` and take ownership
        // of the store.
        match Arc::try_unwrap(store) {
            Ok(store) => store.teardown(),
            Err(_) => {
                // If unwrapping the `Arc` fails, someone else must have
                // a strong reference to the store. We could sleep and
                // try again, but this is so unexpected that it's easier
                // to just leak the store, and return an error to the
                // callback. Except in tests, we only call `teardown` at
                // shutdown, so the resources will get reclaimed soon,
                // anyway.
                Err(Error::DidNotRun(Self::name()))
            }
        }
    }
}

impl Task for TeardownTask {
    fn run(&self) {
        *self.result.borrow_mut() = match self.store.borrow_mut().take() {
            Some(store) => self.inner_run(store),
            None => Err(Error::AlreadyRan(Self::name())),
        };
    }

    fn done(&self) -> result::Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match mem::replace(
            &mut *self.result.borrow_mut(),
            Err(Error::AlreadyRan(Self::name())),
        ) {
            Ok(()) => unsafe { callback.HandleSuccess(().into_variant().coerce()) },
            Err(err) => {
                let mut message = nsCString::new();
                write!(message, "{}", err).unwrap();
                unsafe { callback.HandleError(err.into(), &*message) }
            }
        }
        .to_result()
    }
}
