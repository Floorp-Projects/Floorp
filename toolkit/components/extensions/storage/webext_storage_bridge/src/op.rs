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

/// A storage operation, dispatched to the background task queue.
pub enum StorageOp {
    /// Get the values of the keys for an extension.
    Get { ext_id: String, keys: JsonValue },
    /// Set a key-value pair for an extension.
    Set { ext_id: String, value: JsonValue },
    /// Remove one or more keys for an extension.
    Remove { ext_id: String, keys: JsonValue },
    /// Clear all keys and values for an extension.
    Clear { ext_id: String },
}

impl StorageOp {
    /// Returns the operation name, used to label the task runnable and report
    /// errors.
    pub fn name(&self) -> &'static str {
        match self {
            StorageOp::Get { .. } => "webext_storage::get",
            StorageOp::Set { .. } => "webext_storage::set",
            StorageOp::Remove { .. } => "webext_storage::remove",
            StorageOp::Clear { .. } => "webext_storage::clear",
        }
    }
}

/// A storage operation result, passed back to the main thread.
#[derive(Default)]
pub struct StorageResult {
    pub changes: Option<String>,
    pub value: Option<String>,
}

impl StorageResult {
    /// Creates a result with changes to pass to `onChanged`, and no return
    /// value for `handleSuccess`. The `Borrow` bound lets this method take
    /// either a borrowed reference or an owned value.
    pub fn with_changes<T: Borrow<S>, S: Serialize>(changes: T) -> Result<Self> {
        Ok(StorageResult {
            changes: Some(serde_json::to_string(changes.borrow())?),
            value: None,
        })
    }

    /// Creates a result with no changes to pass to `onChanged`, and a return
    /// value for `handleSuccess`.
    pub fn with_value<T: Borrow<S>, S: Serialize>(value: T) -> Result<Self> {
        Ok(StorageResult {
            changes: None,
            value: Some(serde_json::to_string(value.borrow())?),
        })
    }
}

/// A generic task used for all storage operations. Runs the operation on
/// the background task queue, and calls the callback with a result on the
/// main thread.
pub struct StorageTask {
    name: &'static str,
    /// Storage tasks hold weak references to the store, which they upgrade
    /// to strong references when running on the background queue. This
    /// ensures that pending storage tasks don't block teardown (for example,
    /// if a consumer calls `get` and then `teardown`, without waiting for
    /// `get` to finish).
    store: Weak<LazyStore>,
    op: AtomicRefCell<Option<StorageOp>>,
    callback: ThreadPtrHandle<mozIExtensionStorageCallback>,
    result: AtomicRefCell<Result<StorageResult>>,
}

impl StorageTask {
    /// Creates a storage task for the given operation. Returns an error if
    /// the task couldn't be created because the thread manager is shutting
    /// down.
    pub fn new(
        store: Weak<LazyStore>,
        op: StorageOp,
        callback: &mozIExtensionStorageCallback,
    ) -> Result<Self> {
        let name = op.name();
        Ok(Self {
            name,
            store,
            op: AtomicRefCell::new(Some(op)),
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
    fn run_with_op(&self, op: StorageOp) -> Result<StorageResult> {
        Ok(match op {
            StorageOp::Set { ext_id, value } => {
                StorageResult::with_changes(self.store()?.get()?.set(&ext_id, value)?)
            }
            StorageOp::Get { ext_id, keys } => {
                StorageResult::with_value(self.store()?.get()?.get(&ext_id, keys)?)
            }
            StorageOp::Remove { ext_id, keys } => {
                StorageResult::with_changes(self.store()?.get()?.remove(&ext_id, keys)?)
            }
            StorageOp::Clear { ext_id } => {
                StorageResult::with_changes(self.store()?.get()?.clear(&ext_id)?)
            }
        }?)
    }
}

impl Task for StorageTask {
    fn run(&self) {
        *self.result.borrow_mut() = match mem::take(&mut *self.op.borrow_mut()) {
            Some(op) => self.run_with_op(op),
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
            Ok(StorageResult { changes, value }) => {
                // If we have change data, and the callback implements the
                // listener interface, notify about it first.
                if let (Some(listener), Some(json)) = (
                    callback.query_interface::<mozIExtensionStorageListener>(),
                    changes,
                ) {
                    // Ignore errors.
                    let _ = unsafe { listener.OnChanged(&*nsCString::from(json)) };
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
    fn run_with_store(&self, store: Arc<LazyStore>) -> Result<()> {
        // At this point, we should be holding the only strong reference
        // to the store, since 1) `StorageSyncArea` gave its one strong
        // reference to our task, and 2) we're running on a background
        // task queue, which runs all tasks sequentially...so no other
        // `StorageTask`s should be running and trying to upgrade their
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
        *self.result.borrow_mut() = match mem::take(&mut *self.store.borrow_mut()) {
            Some(store) => self.run_with_store(store),
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
