/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    fmt::Write,
    mem,
    sync::{Arc, Weak},
};

use atomic_refcell::AtomicRefCell;
use moz_task::{DispatchOptions, Task, TaskRunnable, ThreadPtrHandle, ThreadPtrHolder};
use nserror::nsresult;
use nsstring::{nsACString, nsCString};
use sync15_traits::{ApplyResults, BridgedEngine, Guid};
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{
        mozIBridgedSyncEngineApplyCallback, mozIBridgedSyncEngineCallback, nsIEventTarget,
    },
    RefPtr,
};

use crate::error::{self, BridgedError, Error};
use crate::ferry::{Ferry, FerryResult};

/// A ferry task sends (or ferries) an operation to a bridged engine on a
/// background thread or task queue, and ferries back an optional result to
/// a callback.
pub struct FerryTask<N: ?Sized + BridgedEngine> {
    /// A ferry task holds a weak reference to the bridged engine, and upgrades
    /// it to a strong reference when run on a background thread. This avoids
    /// scheduled ferries blocking finalization: if the main thread holds the
    /// only strong reference to the engine, it can be unwrapped (using
    /// `Arc::try_unwrap`) and dropped, either on the main thread, or as part of
    /// a teardown task.
    engine: Weak<N>,
    ferry: Ferry,
    callback: ThreadPtrHandle<mozIBridgedSyncEngineCallback>,
    result: AtomicRefCell<Result<FerryResult, N::Error>>,
}

impl<N> FerryTask<N>
where
    N: ?Sized + BridgedEngine + Send + Sync + 'static,
    N::Error: BridgedError,
{
    /// Creates a task to fetch the engine's last sync time, in milliseconds.
    #[inline]
    pub fn for_last_sync(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::LastSync, callback)
    }

    /// Creates a task to set the engine's last sync time, in milliseconds.
    #[inline]
    pub fn for_set_last_sync(
        engine: &Arc<N>,
        last_sync_millis: i64,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::SetLastSync(last_sync_millis), callback)
    }

    /// Creates a task to fetch the engine's sync ID.
    #[inline]
    pub fn for_sync_id(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::SyncId, callback)
    }

    /// Creates a task to reset the engine's sync ID and all its local Sync
    /// metadata.
    #[inline]
    pub fn for_reset_sync_id(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::ResetSyncId, callback)
    }

    /// Creates a task to compare the bridged engine's local sync ID with
    /// the `new_sync_id` from `meta/global`, and ferry back the final sync ID
    /// to use.
    #[inline]
    pub fn for_ensure_current_sync_id(
        engine: &Arc<N>,
        new_sync_id: &nsACString,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(
            engine,
            Ferry::EnsureCurrentSyncId(std::str::from_utf8(new_sync_id)?.into()),
            callback,
        )
    }

    /// Creates a task to signal that the engine is about to sync.
    #[inline]
    pub fn for_sync_started(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::SyncStarted, callback)
    }

    /// Creates a task to store incoming records.
    pub fn for_store_incoming(
        engine: &Arc<N>,
        incoming_envelopes_json: &[nsCString],
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        let incoming_envelopes = incoming_envelopes_json.iter().try_fold(
            Vec::with_capacity(incoming_envelopes_json.len()),
            |mut envelopes, envelope| -> error::Result<_> {
                envelopes.push(serde_json::from_slice(&*envelope)?);
                Ok(envelopes)
            },
        )?;
        Self::with_ferry(engine, Ferry::StoreIncoming(incoming_envelopes), callback)
    }

    /// Creates a task to mark a subset of outgoing records as uploaded. This
    /// may be called multiple times per sync, or not at all if there are no
    /// records to upload.
    pub fn for_set_uploaded(
        engine: &Arc<N>,
        server_modified_millis: i64,
        uploaded_ids: &[nsCString],
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        let uploaded_ids = uploaded_ids
            .iter()
            .map(|id| Guid::from_slice(&*id))
            .collect();
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
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::SyncFinished, callback)
    }

    /// Creates a task to reset all local Sync state for the engine, without
    /// erasing user data.
    #[inline]
    pub fn for_reset(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::Reset, callback)
    }

    /// Creates a task to erase all local user data for the engine.
    #[inline]
    pub fn for_wipe(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        Self::with_ferry(engine, Ferry::Wipe, callback)
    }

    /// Creates a task for a ferry. The `callback` is bound to the current
    /// thread, and will be called once, after the ferry returns from the
    /// background thread.
    fn with_ferry(
        engine: &Arc<N>,
        ferry: Ferry,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N>> {
        let name = ferry.name();
        Ok(FerryTask {
            engine: Arc::downgrade(engine),
            ferry,
            callback: ThreadPtrHolder::new(
                cstr!("mozIBridgedSyncEngineCallback"),
                RefPtr::new(callback),
            )?,
            result: AtomicRefCell::new(Err(Error::DidNotRun(name).into())),
        })
    }

    /// Dispatches the task to the given thread `target`.
    pub fn dispatch(self, target: &nsIEventTarget) -> Result<(), Error> {
        let runnable = TaskRunnable::new(self.ferry.name(), Box::new(self))?;
        // `may_block` schedules the task on the I/O thread pool, since we
        // expect most operations to wait on I/O.
        runnable.dispatch_with_options(target, DispatchOptions::default().may_block(true))?;
        Ok(())
    }
}

impl<N> FerryTask<N>
where
    N: ?Sized + BridgedEngine,
    N::Error: BridgedError,
{
    /// Runs the task on the background thread. This is split out into its own
    /// method to make error handling easier.
    fn inner_run(&self) -> Result<FerryResult, N::Error> {
        let engine = match self.engine.upgrade() {
            Some(outer) => outer,
            None => return Err(Error::DidNotRun(self.ferry.name()).into()),
        };
        Ok(match &self.ferry {
            Ferry::LastSync => FerryResult::LastSync(engine.last_sync()?),
            Ferry::SetLastSync(last_sync_millis) => {
                engine.set_last_sync(*last_sync_millis)?;
                FerryResult::default()
            }
            Ferry::SyncId => FerryResult::SyncId(engine.sync_id()?),
            Ferry::ResetSyncId => FerryResult::AssignedSyncId(engine.reset_sync_id()?),
            Ferry::EnsureCurrentSyncId(new_sync_id) => {
                FerryResult::AssignedSyncId(engine.ensure_current_sync_id(&*new_sync_id)?)
            }
            Ferry::SyncStarted => {
                engine.sync_started()?;
                FerryResult::default()
            }
            Ferry::StoreIncoming(incoming_envelopes) => {
                engine.store_incoming(incoming_envelopes.as_slice())?;
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

impl<N> Task for FerryTask<N>
where
    N: ?Sized + BridgedEngine,
    N::Error: BridgedError,
{
    fn run(&self) {
        *self.result.borrow_mut() = self.inner_run();
    }

    fn done(&self) -> Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match mem::replace(
            &mut *self.result.borrow_mut(),
            Err(Error::DidNotRun(self.ferry.name()).into()),
        ) {
            Ok(result) => unsafe { callback.HandleSuccess(result.into_variant().coerce()) },
            Err(err) => {
                let mut message = nsCString::new();
                write!(message, "{}", err).unwrap();
                unsafe { callback.HandleError(err.into(), &*message) }
            }
        }
        .to_result()
    }
}

/// An apply task ferries incoming records to an engine on a background
/// thread, and ferries back records to upload. It's separate from
/// `FerryTask` because its callback type is different.
pub struct ApplyTask<N: ?Sized + BridgedEngine> {
    engine: Weak<N>,
    callback: ThreadPtrHandle<mozIBridgedSyncEngineApplyCallback>,
    result: AtomicRefCell<Result<Vec<String>, N::Error>>,
}

impl<N> ApplyTask<N>
where
    N: ?Sized + BridgedEngine,
    N::Error: BridgedError,
{
    /// Returns the task name for debugging.
    pub fn name() -> &'static str {
        concat!(module_path!(), "apply")
    }

    /// Runs the task on the background thread.
    fn inner_run(&self) -> Result<Vec<String>, N::Error> {
        let engine = match self.engine.upgrade() {
            Some(outer) => outer,
            None => return Err(Error::DidNotRun(Self::name()).into()),
        };
        let ApplyResults {
            envelopes: outgoing_envelopes,
            ..
        } = engine.apply()?;
        let outgoing_envelopes_json = outgoing_envelopes.iter().try_fold(
            Vec::with_capacity(outgoing_envelopes.len()),
            |mut envelopes, envelope| {
                envelopes.push(serde_json::to_string(envelope)?);
                Ok(envelopes)
            },
        )?;
        Ok(outgoing_envelopes_json)
    }
}

impl<N> ApplyTask<N>
where
    N: ?Sized + BridgedEngine + Send + Sync + 'static,
    N::Error: BridgedError,
{
    /// Creates a task. The `callback` is bound to the current thread, and will
    /// be called once, after the records are applied on the background thread.
    pub fn new(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineApplyCallback,
    ) -> error::Result<ApplyTask<N>> {
        Ok(ApplyTask {
            engine: Arc::downgrade(engine),
            callback: ThreadPtrHolder::new(
                cstr!("mozIBridgedSyncEngineApplyCallback"),
                RefPtr::new(callback),
            )?,
            result: AtomicRefCell::new(Err(Error::DidNotRun(Self::name()).into())),
        })
    }

    /// Dispatches the task to the given thread `target`.
    pub fn dispatch(self, target: &nsIEventTarget) -> Result<(), Error> {
        let runnable = TaskRunnable::new(Self::name(), Box::new(self))?;
        runnable.dispatch_with_options(target, DispatchOptions::default().may_block(true))?;
        Ok(())
    }
}

impl<N> Task for ApplyTask<N>
where
    N: ?Sized + BridgedEngine,
    N::Error: BridgedError,
{
    fn run(&self) {
        *self.result.borrow_mut() = self.inner_run();
    }

    fn done(&self) -> Result<(), nsresult> {
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
                write!(message, "{}", err).unwrap();
                unsafe { callback.HandleError(err.into(), &*message) }
            }
        }
        .to_result()
    }
}
