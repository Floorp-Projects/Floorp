/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{fmt::Write, mem, sync::Arc};

use atomic_refcell::AtomicRefCell;
use interrupt_support::Interruptee;
use moz_task::{DispatchOptions, Task, TaskRunnable, ThreadPtrHandle, ThreadPtrHolder};
use nserror::nsresult;
use nsstring::{nsACString, nsCString};
use sync15_traits::{ApplyResults, BridgedEngine};
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
pub struct FerryTask<N: ?Sized + BridgedEngine, S> {
    engine: Arc<N>,
    ferry: Ferry<S>,
    callback: ThreadPtrHandle<mozIBridgedSyncEngineCallback>,
    result: AtomicRefCell<Result<FerryResult, N::Error>>,
}

impl<N, S> FerryTask<N, S>
where
    N: ?Sized + BridgedEngine + Send + Sync + 'static,
    S: Interruptee + Send + Sync + 'static,
    N::Error: BridgedError,
{
    /// Creates a task to initialize the engine.
    #[inline]
    pub fn for_initialize(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::Initialize, callback)
    }

    /// Creates a task to fetch the engine's last sync time, in milliseconds.
    #[inline]
    pub fn for_last_sync(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::LastSync, callback)
    }

    /// Creates a task to set the engine's last sync time, in milliseconds.
    #[inline]
    pub fn for_set_last_sync(
        engine: &Arc<N>,
        last_sync_millis: i64,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::SetLastSync(last_sync_millis), callback)
    }

    /// Creates a task to fetch the engine's sync ID.
    #[inline]
    pub fn for_sync_id(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::SyncId, callback)
    }

    /// Creates a task to reset the engine's sync ID and all its local Sync
    /// metadata.
    #[inline]
    pub fn for_reset_sync_id(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
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
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(
            engine,
            Ferry::EnsureCurrentSyncId(std::str::from_utf8(new_sync_id)?.into()),
            callback,
        )
    }

    /// Creates a task to store incoming records.
    pub fn for_store_incoming(
        engine: &Arc<N>,
        incoming_cleartexts: &[nsCString],
        signal: &Arc<S>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        let incoming_cleartexts = incoming_cleartexts.iter().try_fold(
            Vec::with_capacity(incoming_cleartexts.len()),
            |mut cleartexts, cleartext| -> error::Result<_> {
                // We need to clone the string for the task to take ownership
                // of it, anyway; might as well convert to a Rust string while
                // we're here.
                cleartexts.push(std::str::from_utf8(&*cleartext)?.into());
                Ok(cleartexts)
            },
        )?;
        Self::with_ferry(
            engine,
            Ferry::StoreIncoming(incoming_cleartexts, Arc::clone(signal)),
            callback,
        )
    }

    /// Creates a task to mark a subset of outgoing records as uploaded. This
    /// may be called multiple times per sync, or not at all if there are no
    /// records to upload.
    pub fn for_set_uploaded(
        engine: &Arc<N>,
        server_modified_millis: i64,
        uploaded_ids: &[nsCString],
        signal: &Arc<S>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        let uploaded_ids = uploaded_ids.iter().try_fold(
            Vec::with_capacity(uploaded_ids.len()),
            |mut ids, id| -> error::Result<_> {
                ids.push(std::str::from_utf8(&*id)?.into());
                Ok(ids)
            },
        )?;
        Self::with_ferry(
            engine,
            Ferry::SetUploaded(server_modified_millis, uploaded_ids, Arc::clone(signal)),
            callback,
        )
    }

    /// Creates a task to signal that all records have been uploaded, and
    /// the engine has been synced. This is called even if there were no
    /// records uploaded.
    #[inline]
    pub fn for_sync_finished(
        engine: &Arc<N>,
        signal: &Arc<S>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::SyncFinished(Arc::clone(signal)), callback)
    }

    /// Creates a task to reset all local Sync state for the engine, without
    /// erasing user data.
    #[inline]
    pub fn for_reset(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::Reset, callback)
    }

    /// Creates a task to erase all local user data for the engine.
    #[inline]
    pub fn for_wipe(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::Wipe, callback)
    }

    /// Creates a task to tear down the engine.
    #[inline]
    pub fn for_finalize(
        engine: &Arc<N>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        Self::with_ferry(engine, Ferry::Finalize, callback)
    }

    /// Creates a task for a ferry. The `callback` is bound to the current
    /// thread, and will be called once, after the ferry returns from the
    /// background thread.
    fn with_ferry(
        engine: &Arc<N>,
        ferry: Ferry<S>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> error::Result<FerryTask<N, S>> {
        let name = ferry.name();
        Ok(FerryTask {
            engine: Arc::clone(engine),
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

impl<N, S> Task for FerryTask<N, S>
where
    N: ?Sized + BridgedEngine,
    S: Interruptee,
    N::Error: BridgedError,
{
    fn run(&self) {
        *self.result.borrow_mut() = match &self.ferry {
            Ferry::Initialize => self.engine.initialize().map(FerryResult::from),
            Ferry::LastSync => self.engine.last_sync().map(FerryResult::LastSync),
            Ferry::SetLastSync(last_sync_millis) => self
                .engine
                .set_last_sync(*last_sync_millis)
                .map(FerryResult::from),
            Ferry::SyncId => self.engine.sync_id().map(FerryResult::SyncId),
            Ferry::ResetSyncId => self.engine.reset_sync_id().map(FerryResult::AssignedSyncId),
            Ferry::EnsureCurrentSyncId(new_sync_id) => self
                .engine
                .ensure_current_sync_id(&*new_sync_id)
                .map(FerryResult::AssignedSyncId),
            Ferry::StoreIncoming(incoming_cleartexts, signal) => self
                .engine
                .store_incoming(incoming_cleartexts.as_slice(), signal.as_ref())
                .map(FerryResult::from),
            Ferry::SetUploaded(server_modified_millis, uploaded_ids, signal) => self
                .engine
                .set_uploaded(
                    *server_modified_millis,
                    uploaded_ids.as_slice(),
                    signal.as_ref(),
                )
                .map(FerryResult::from),
            Ferry::SyncFinished(signal) => self
                .engine
                .sync_finished(signal.as_ref())
                .map(FerryResult::from),
            Ferry::Reset => self.engine.reset().map(FerryResult::from),
            Ferry::Wipe => self.engine.wipe().map(FerryResult::from),
            Ferry::Finalize => self.engine.finalize().map(FerryResult::from),
        };
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
pub struct ApplyTask<N: ?Sized + BridgedEngine, S> {
    engine: Arc<N>,
    signal: Arc<S>,
    callback: ThreadPtrHandle<mozIBridgedSyncEngineApplyCallback>,
    result: AtomicRefCell<Result<ApplyResults, N::Error>>,
}

impl<N, S> ApplyTask<N, S>
where
    N: ?Sized + BridgedEngine,
{
    /// Returns the task name for debugging.
    pub fn name() -> &'static str {
        concat!(module_path!(), "apply")
    }
}

impl<N, S> ApplyTask<N, S>
where
    N: ?Sized + BridgedEngine + Send + Sync + 'static,
    S: Interruptee + Send + Sync + 'static,
    N::Error: BridgedError,
{
    /// Creates a task. The `callback` is bound to the current thread, and will
    /// be called once, after the records are applied on the background thread.
    pub fn new(
        engine: &Arc<N>,
        signal: &Arc<S>,
        callback: &mozIBridgedSyncEngineApplyCallback,
    ) -> error::Result<ApplyTask<N, S>> {
        Ok(ApplyTask {
            engine: Arc::clone(engine),
            signal: Arc::clone(signal),
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

impl<N, S> Task for ApplyTask<N, S>
where
    N: ?Sized + BridgedEngine,
    S: Interruptee,
    N::Error: BridgedError,
{
    fn run(&self) {
        *self.result.borrow_mut() = self.engine.apply(self.signal.as_ref());
    }

    fn done(&self) -> Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match mem::replace(
            &mut *self.result.borrow_mut(),
            Err(Error::DidNotRun(Self::name()).into()),
        ) {
            Ok(ApplyResults {
                records,
                num_reconciled: _,
            }) => {
                let result = records
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
