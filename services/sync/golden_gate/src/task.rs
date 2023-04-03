/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{fmt::Write, mem, result};

use atomic_refcell::AtomicRefCell;
use moz_task::{DispatchOptions, Task, TaskRunnable, ThreadPtrHandle, ThreadPtrHolder};
use nserror::{nsresult, NS_ERROR_FAILURE};
use nsstring::{nsACString, nsCString};
use sync15::engine::{ApplyResults, BridgedEngine};
use sync15::Guid;
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{
        mozIBridgedSyncEngineApplyCallback, mozIBridgedSyncEngineCallback, nsIEventTarget,
    },
    RefPtr,
};

use crate::error::{Error, Result};
use crate::ferry::{Ferry, FerryResult};

/// A ferry task sends (or ferries) an operation to a bridged engine on a
/// background thread or task queue, and ferries back an optional result to
/// a callback.
pub struct FerryTask {
    /// We want to ensure scheduled ferries can't block finalization of the underlying
    /// store - we want a degree of confidence that closing the database will happen when
    /// we want even if tasks are queued up to run on another thread.
    /// We rely on the semantics of our BridgedEngines to help here:
    /// * A bridged engine is expected to hold a weak reference to its store.
    /// * Our LazyStore is the only thing holding a reference to the "real" store.
    /// Thus, when our LazyStore asks our "real" store to close, we can be confident
    /// a close will happen (ie, we assume that the real store will be able to unwrapp
    /// the underlying sqlite `Connection` (using `Arc::try_unwrap`) and close it.
    /// However, note that if an operation on the bridged engine is currently running,
    /// we will block waiting for that operation to complete, so while this isn't
    /// guaranteed to happen immediately, it should happen "soon enough".
    engine: Box<dyn BridgedEngine>,
    ferry: Ferry,
    callback: ThreadPtrHandle<mozIBridgedSyncEngineCallback>,
    result: AtomicRefCell<anyhow::Result<FerryResult>>,
}

impl FerryTask {
    /// Creates a task to fetch the engine's last sync time, in milliseconds.
    #[inline]
    pub fn for_last_sync(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::LastSync, callback)
    }

    /// Creates a task to set the engine's last sync time, in milliseconds.
    #[inline]
    pub fn for_set_last_sync(
        engine: Box<dyn BridgedEngine>,
        last_sync_millis: i64,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::SetLastSync(last_sync_millis), callback)
    }

    /// Creates a task to fetch the engine's sync ID.
    #[inline]
    pub fn for_sync_id(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::SyncId, callback)
    }

    /// Creates a task to reset the engine's sync ID and all its local Sync
    /// metadata.
    #[inline]
    pub fn for_reset_sync_id(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::ResetSyncId, callback)
    }

    /// Creates a task to compare the bridged engine's local sync ID with
    /// the `new_sync_id` from `meta/global`, and ferry back the final sync ID
    /// to use.
    #[inline]
    pub fn for_ensure_current_sync_id(
        engine: Box<dyn BridgedEngine>,
        new_sync_id: &nsACString,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(
            engine,
            Ferry::EnsureCurrentSyncId(std::str::from_utf8(new_sync_id)?.into()),
            callback,
        )
    }

    /// Creates a task to signal that the engine is about to sync.
    #[inline]
    pub fn for_sync_started(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::SyncStarted, callback)
    }

    /// Creates a task to store incoming records.
    pub fn for_store_incoming(
        engine: Box<dyn BridgedEngine>,
        incoming_envelopes_json: &[nsCString],
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(
            engine,
            Ferry::StoreIncoming(incoming_envelopes_json.to_vec()),
            callback,
        )
    }

    /// Creates a task to mark a subset of outgoing records as uploaded. This
    /// may be called multiple times per sync, or not at all if there are no
    /// records to upload.
    pub fn for_set_uploaded(
        engine: Box<dyn BridgedEngine>,
        server_modified_millis: i64,
        uploaded_ids: &[nsCString],
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        let uploaded_ids = uploaded_ids.iter().map(|id| Guid::from_slice(id)).collect();
        Self::with_ferry(
            engine,
            Ferry::SetUploaded(server_modified_millis, uploaded_ids),
            callback,
        )
    }

    /// Creates a task to signal that all records have been uploaded, and
    /// the engine has been synced. This is called even if there were no
    /// records uploaded.
    #[inline]
    pub fn for_sync_finished(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::SyncFinished, callback)
    }

    /// Creates a task to reset all local Sync state for the engine, without
    /// erasing user data.
    #[inline]
    pub fn for_reset(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::Reset, callback)
    }

    /// Creates a task to erase all local user data for the engine.
    #[inline]
    pub fn for_wipe(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        Self::with_ferry(engine, Ferry::Wipe, callback)
    }

    /// Creates a task for a ferry. The `callback` is bound to the current
    /// thread, and will be called once, after the ferry returns from the
    /// background thread.
    fn with_ferry(
        engine: Box<dyn BridgedEngine>,
        ferry: Ferry,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<FerryTask> {
        let name = ferry.name();
        Ok(FerryTask {
            engine,
            ferry,
            callback: ThreadPtrHolder::new(
                cstr!("mozIBridgedSyncEngineCallback"),
                RefPtr::new(callback),
            )?,
            result: AtomicRefCell::new(Err(Error::DidNotRun(name).into())),
        })
    }

    /// Dispatches the task to the given thread `target`.
    pub fn dispatch(self, target: &nsIEventTarget) -> Result<()> {
        let runnable = TaskRunnable::new(self.ferry.name(), Box::new(self))?;
        // `may_block` schedules the task on the I/O thread pool, since we
        // expect most operations to wait on I/O.
        TaskRunnable::dispatch_with_options(
            runnable,
            target,
            DispatchOptions::default().may_block(true),
        )?;
        Ok(())
    }

    /// Runs the task on the background thread. This is split out into its own
    /// method to make error handling easier.
    fn inner_run(&self) -> anyhow::Result<FerryResult> {
        let engine = &self.engine;
        Ok(match &self.ferry {
            Ferry::LastSync => FerryResult::LastSync(engine.last_sync()?),
            Ferry::SetLastSync(last_sync_millis) => {
                engine.set_last_sync(*last_sync_millis)?;
                FerryResult::default()
            }
            Ferry::SyncId => FerryResult::SyncId(engine.sync_id()?),
            Ferry::ResetSyncId => FerryResult::AssignedSyncId(engine.reset_sync_id()?),
            Ferry::EnsureCurrentSyncId(new_sync_id) => {
                FerryResult::AssignedSyncId(engine.ensure_current_sync_id(new_sync_id)?)
            }
            Ferry::SyncStarted => {
                engine.sync_started()?;
                FerryResult::default()
            }
            Ferry::StoreIncoming(incoming_envelopes_json) => {
                let incoming_envelopes = incoming_envelopes_json
                    .iter()
                    .map(|envelope| Ok(serde_json::from_slice(envelope)?))
                    .collect::<Result<_>>()?;

                engine.store_incoming(incoming_envelopes)?;
                FerryResult::default()
            }
            Ferry::SetUploaded(server_modified_millis, uploaded_ids) => {
                engine.set_uploaded(*server_modified_millis, uploaded_ids.as_slice())?;
                FerryResult::default()
            }
            Ferry::SyncFinished => {
                engine.sync_finished()?;
                FerryResult::default()
            }
            Ferry::Reset => {
                engine.reset()?;
                FerryResult::default()
            }
            Ferry::Wipe => {
                engine.wipe()?;
                FerryResult::default()
            }
        })
    }
}

impl Task for FerryTask {
    fn run(&self) {
        *self.result.borrow_mut() = self.inner_run();
    }

    fn done(&self) -> result::Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match mem::replace(
            &mut *self.result.borrow_mut(),
            Err(Error::DidNotRun(self.ferry.name()).into()),
        ) {
            Ok(result) => unsafe { callback.HandleSuccess(result.into_variant().coerce()) },
            Err(err) => {
                let mut message = nsCString::new();
                write!(message, "{err}").unwrap();
                unsafe { callback.HandleError(NS_ERROR_FAILURE, &*message) }
            }
        }
        .to_result()
    }
}

/// An apply task ferries incoming records to an engine on a background
/// thread, and ferries back records to upload. It's separate from
/// `FerryTask` because its callback type is different.
pub struct ApplyTask {
    engine: Box<dyn BridgedEngine>,
    callback: ThreadPtrHandle<mozIBridgedSyncEngineApplyCallback>,
    result: AtomicRefCell<anyhow::Result<Vec<String>>>,
}

impl ApplyTask {
    /// Returns the task name for debugging.
    pub fn name() -> &'static str {
        concat!(module_path!(), "apply")
    }

    /// Runs the task on the background thread.
    fn inner_run(&self) -> anyhow::Result<Vec<String>> {
        let ApplyResults {
            records: outgoing_records,
            ..
        } = self.engine.apply()?;
        let outgoing_records_json = outgoing_records
            .iter()
            .map(|record| Ok(serde_json::to_string(record)?))
            .collect::<Result<_>>()?;
        Ok(outgoing_records_json)
    }

    /// Creates a task. The `callback` is bound to the current thread, and will
    /// be called once, after the records are applied on the background thread.
    pub fn new(
        engine: Box<dyn BridgedEngine>,
        callback: &mozIBridgedSyncEngineApplyCallback,
    ) -> Result<ApplyTask> {
        Ok(ApplyTask {
            engine,
            callback: ThreadPtrHolder::new(
                cstr!("mozIBridgedSyncEngineApplyCallback"),
                RefPtr::new(callback),
            )?,
            result: AtomicRefCell::new(Err(Error::DidNotRun(Self::name()).into())),
        })
    }

    /// Dispatches the task to the given thread `target`.
    pub fn dispatch(self, target: &nsIEventTarget) -> Result<()> {
        let runnable = TaskRunnable::new(Self::name(), Box::new(self))?;
        TaskRunnable::dispatch_with_options(
            runnable,
            target,
            DispatchOptions::default().may_block(true),
        )?;
        Ok(())
    }
}

impl Task for ApplyTask {
    fn run(&self) {
        *self.result.borrow_mut() = self.inner_run();
    }

    fn done(&self) -> result::Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match mem::replace(
            &mut *self.result.borrow_mut(),
            Err(Error::DidNotRun(Self::name()).into()),
        ) {
            Ok(envelopes) => {
                let result = envelopes
                    .into_iter()
                    .map(nsCString::from)
                    .collect::<ThinVec<_>>();
                unsafe { callback.HandleSuccess(&result) }
            }
            Err(err) => {
                let mut message = nsCString::new();
                write!(message, "{err}").unwrap();
                unsafe { callback.HandleError(NS_ERROR_FAILURE, &*message) }
            }
        }
        .to_result()
    }
}
