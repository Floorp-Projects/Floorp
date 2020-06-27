/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    cell::{Ref, RefCell},
    convert::TryInto,
    ffi::OsString,
    mem,
    path::PathBuf,
    str,
    sync::Arc,
};

use golden_gate::{ApplyTask, FerryTask};
use moz_task::{self, DispatchOptions, TaskRunnable};
use nserror::{nsresult, NS_OK};
use nsstring::{nsACString, nsCString, nsString};
use thin_vec::ThinVec;
use webext_storage::STORAGE_VERSION;
use xpcom::{
    interfaces::{
        mozIBridgedSyncEngineApplyCallback, mozIBridgedSyncEngineCallback,
        mozIExtensionStorageCallback, mozIServicesLogSink, nsIFile, nsISerialEventTarget,
    },
    RefPtr,
};

use crate::error::{Error, Result};
use crate::punt::{Punt, PuntTask, TeardownTask};
use crate::store::{LazyStore, LazyStoreConfig};

fn path_from_nsifile(file: &nsIFile) -> Result<PathBuf> {
    let mut raw_path = nsString::new();
    // `nsIFile::GetPath` gives us a UTF-16-encoded version of its
    // native path, which we must turn back into a platform-native
    // string. We can't use `nsIFile::nativePath()` here because
    // it's marked as `nostdcall`, which Rust doesn't support.
    unsafe { file.GetPath(&mut *raw_path) }.to_result()?;
    let native_path = {
        // On Windows, we can create a native string directly from the
        // encoded path.
        #[cfg(windows)]
        {
            use std::os::windows::prelude::*;
            OsString::from_wide(&*raw_path)
        }
        // On other platforms, we must first decode the raw path from
        // UTF-16, and then create our native string.
        #[cfg(not(windows))]
        OsString::from(String::from_utf16(&*raw_path)?)
    };
    Ok(native_path.into())
}

/// An XPCOM component class for the Rust extension storage API. This class
/// implements the interfaces needed for syncing and storage.
///
/// This class can be created on any thread, but must not be shared between
/// threads. In Rust terms, it's `Send`, but not `Sync`.
#[derive(xpcom)]
#[xpimplements(
    mozIExtensionStorageArea,
    mozIConfigurableExtensionStorageArea,
    mozISyncedExtensionStorageArea,
    mozIInterruptible,
    mozIBridgedSyncEngine
)]
#[refcnt = "nonatomic"]
pub struct InitStorageSyncArea {
    /// A background task queue, used to run all our storage operations on a
    /// thread pool. Using a serial event target here means that all operations
    /// will execute sequentially.
    queue: RefPtr<nsISerialEventTarget>,
    /// The store is lazily initialized on the task queue the first time it's
    /// used.
    store: RefCell<Option<Arc<LazyStore>>>,
}

/// `mozIExtensionStorageArea` implementation.
impl StorageSyncArea {
    /// Creates a storage area and its task queue.
    pub fn new() -> Result<RefPtr<StorageSyncArea>> {
        let queue = moz_task::create_background_task_queue(cstr!("StorageSyncArea"))?;
        Ok(StorageSyncArea::allocate(InitStorageSyncArea {
            queue,
            store: RefCell::new(Some(Arc::default())),
        }))
    }

    /// Returns the store for this area, or an error if it's been torn down.
    fn store(&self) -> Result<Ref<'_, Arc<LazyStore>>> {
        let maybe_store = self.store.borrow();
        if maybe_store.is_some() {
            Ok(Ref::map(maybe_store, |s| s.as_ref().unwrap()))
        } else {
            Err(Error::AlreadyTornDown)
        }
    }

    /// Dispatches a task for a storage operation to the task queue.
    fn dispatch(&self, punt: Punt, callback: &mozIExtensionStorageCallback) -> Result<()> {
        let name = punt.name();
        let task = PuntTask::new(Arc::downgrade(&*self.store()?), punt, callback)?;
        let runnable = TaskRunnable::new(name, Box::new(task))?;
        // `may_block` schedules the runnable on a dedicated I/O pool.
        TaskRunnable::dispatch_with_options(
            runnable,
            self.queue.coerce(),
            DispatchOptions::new().may_block(true),
        )?;
        Ok(())
    }

    xpcom_method!(
        configure => Configure(
            database_file: *const nsIFile,
            kinto_file: *const nsIFile
        )
    );
    /// Sets up the storage area.
    fn configure(&self, database_file: &nsIFile, kinto_file: &nsIFile) -> Result<()> {
        self.store()?.configure(LazyStoreConfig {
            path: path_from_nsifile(database_file)?,
            kinto_path: path_from_nsifile(kinto_file)?,
        })?;
        Ok(())
    }

    xpcom_method!(
        set => Set(
            ext_id: *const ::nsstring::nsACString,
            json: *const ::nsstring::nsACString,
            callback: *const mozIExtensionStorageCallback
        )
    );
    /// Sets one or more key-value pairs.
    fn set(
        &self,
        ext_id: &nsACString,
        json: &nsACString,
        callback: &mozIExtensionStorageCallback,
    ) -> Result<()> {
        self.dispatch(
            Punt::Set {
                ext_id: str::from_utf8(&*ext_id)?.into(),
                value: serde_json::from_str(str::from_utf8(&*json)?)?,
            },
            callback,
        )?;
        Ok(())
    }

    xpcom_method!(
        get => Get(
            ext_id: *const ::nsstring::nsACString,
            json: *const ::nsstring::nsACString,
            callback: *const mozIExtensionStorageCallback
        )
    );
    /// Gets values for one or more keys.
    fn get(
        &self,
        ext_id: &nsACString,
        json: &nsACString,
        callback: &mozIExtensionStorageCallback,
    ) -> Result<()> {
        self.dispatch(
            Punt::Get {
                ext_id: str::from_utf8(&*ext_id)?.into(),
                keys: serde_json::from_str(str::from_utf8(&*json)?)?,
            },
            callback,
        )
    }

    xpcom_method!(
        remove => Remove(
            ext_id: *const ::nsstring::nsACString,
            json: *const ::nsstring::nsACString,
            callback: *const mozIExtensionStorageCallback
        )
    );
    /// Removes one or more keys and their values.
    fn remove(
        &self,
        ext_id: &nsACString,
        json: &nsACString,
        callback: &mozIExtensionStorageCallback,
    ) -> Result<()> {
        self.dispatch(
            Punt::Remove {
                ext_id: str::from_utf8(&*ext_id)?.into(),
                keys: serde_json::from_str(str::from_utf8(&*json)?)?,
            },
            callback,
        )
    }

    xpcom_method!(
        clear => Clear(
            ext_id: *const ::nsstring::nsACString,
            callback: *const mozIExtensionStorageCallback
        )
    );
    /// Removes all keys and values for the specified extension.
    fn clear(&self, ext_id: &nsACString, callback: &mozIExtensionStorageCallback) -> Result<()> {
        self.dispatch(
            Punt::Clear {
                ext_id: str::from_utf8(&*ext_id)?.into(),
            },
            callback,
        )
    }

    xpcom_method!(
        getBytesInUse => GetBytesInUse(
            ext_id: *const ::nsstring::nsACString,
            keys: *const ::nsstring::nsACString,
            callback: *const mozIExtensionStorageCallback
        )
    );
    /// Obtains the count of bytes in use for the specified key or for all keys.
    fn getBytesInUse(
        &self,
        ext_id: &nsACString,
        keys: &nsACString,
        callback: &mozIExtensionStorageCallback,
    ) -> Result<()> {
        self.dispatch(
            Punt::GetBytesInUse {
                ext_id: str::from_utf8(&*ext_id)?.into(),
                keys: serde_json::from_str(str::from_utf8(&*keys)?)?,
            },
            callback,
        )
    }

    xpcom_method!(teardown => Teardown(callback: *const mozIExtensionStorageCallback));
    /// Tears down the storage area, closing the backing database connection.
    fn teardown(&self, callback: &mozIExtensionStorageCallback) -> Result<()> {
        // Each storage task holds a `Weak` reference to the store, which it
        // upgrades to an `Arc` (strong reference) when the task runs on the
        // background queue. The strong reference is dropped when the task
        // finishes. When we tear down the storage area, we relinquish our one
        // owned strong reference to the `TeardownTask`. Because we're using a
        // task queue, when the `TeardownTask` runs, it should have the only
        // strong reference to the store, since all other tasks that called
        // `Weak::upgrade` will have already finished. The `TeardownTask` can
        // then consume the `Arc` and destroy the store.
        let mut maybe_store = self.store.borrow_mut();
        match mem::take(&mut *maybe_store) {
            Some(store) => {
                // Interrupt any currently-running statements.
                store.interrupt();
                // If dispatching the runnable fails, we'll leak the store
                // without closing its database connection.
                teardown(&self.queue, store, callback)?;
            }
            None => return Err(Error::AlreadyTornDown),
        }
        Ok(())
    }

    xpcom_method!(takeMigrationInfo => TakeMigrationInfo(callback: *const mozIExtensionStorageCallback));

    /// Fetch-and-delete (e.g. `take`) information about the migration from the
    /// kinto-based extension-storage to the rust-based storage.
    fn takeMigrationInfo(&self, callback: &mozIExtensionStorageCallback) -> Result<()> {
        self.dispatch(Punt::TakeMigrationInfo, callback)
    }
}

fn teardown(
    queue: &nsISerialEventTarget,
    store: Arc<LazyStore>,
    callback: &mozIExtensionStorageCallback,
) -> Result<()> {
    let task = TeardownTask::new(store, callback)?;
    let runnable = TaskRunnable::new(TeardownTask::name(), Box::new(task))?;
    TaskRunnable::dispatch_with_options(
        runnable,
        queue.coerce(),
        DispatchOptions::new().may_block(true),
    )?;
    Ok(())
}

/// `mozISyncedExtensionStorageArea` implementation.
impl StorageSyncArea {
    xpcom_method!(
        fetch_pending_sync_changes => FetchPendingSyncChanges(callback: *const mozIExtensionStorageCallback)
    );
    fn fetch_pending_sync_changes(&self, callback: &mozIExtensionStorageCallback) -> Result<()> {
        self.dispatch(Punt::FetchPendingSyncChanges, callback)
    }
}

/// `mozIInterruptible` implementation.
impl StorageSyncArea {
    xpcom_method!(
        interrupt => Interrupt()
    );
    /// Interrupts any operations currently running on the background task
    /// queue.
    fn interrupt(&self) -> Result<()> {
        self.store()?.interrupt();
        Ok(())
    }
}

/// `mozIBridgedSyncEngine` implementation.
impl StorageSyncArea {
    xpcom_method!(get_logger => GetLogger() -> *const mozIServicesLogSink);
    fn get_logger(&self) -> Result<RefPtr<mozIServicesLogSink>> {
        Err(NS_OK)?
    }

    xpcom_method!(set_logger => SetLogger(logger: *const mozIServicesLogSink));
    fn set_logger(&self, _logger: Option<&mozIServicesLogSink>) -> Result<()> {
        Ok(())
    }

    xpcom_method!(get_storage_version => GetStorageVersion() -> i32);
    fn get_storage_version(&self) -> Result<i32> {
        Ok(STORAGE_VERSION.try_into().unwrap())
    }

    // It's possible that migration, or even merging, will result in records
    // too large for the server. We tolerate that (and hope that the addons do
    // too :)
    xpcom_method!(get_allow_skipped_record => GetAllowSkippedRecord() -> bool);
    fn get_allow_skipped_record(&self) -> Result<bool> {
        Ok(true)
    }

    xpcom_method!(
        get_last_sync => GetLastSync(
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn get_last_sync(&self, callback: &mozIBridgedSyncEngineCallback) -> Result<()> {
        Ok(FerryTask::for_last_sync(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }

    xpcom_method!(
        set_last_sync => SetLastSync(
            last_sync_millis: i64,
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn set_last_sync(
        &self,
        last_sync_millis: i64,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<()> {
        Ok(
            FerryTask::for_set_last_sync(&*self.store()?, last_sync_millis, callback)?
                .dispatch(&self.queue)?,
        )
    }

    xpcom_method!(
        get_sync_id => GetSyncId(
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn get_sync_id(&self, callback: &mozIBridgedSyncEngineCallback) -> Result<()> {
        Ok(FerryTask::for_sync_id(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }

    xpcom_method!(
        reset_sync_id => ResetSyncId(
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn reset_sync_id(&self, callback: &mozIBridgedSyncEngineCallback) -> Result<()> {
        Ok(FerryTask::for_reset_sync_id(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }

    xpcom_method!(
        ensure_current_sync_id => EnsureCurrentSyncId(
            new_sync_id: *const nsACString,
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn ensure_current_sync_id(
        &self,
        new_sync_id: &nsACString,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<()> {
        Ok(
            FerryTask::for_ensure_current_sync_id(&*self.store()?, new_sync_id, callback)?
                .dispatch(&self.queue)?,
        )
    }

    xpcom_method!(
        sync_started => SyncStarted(
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn sync_started(&self, callback: &mozIBridgedSyncEngineCallback) -> Result<()> {
        Ok(FerryTask::for_sync_started(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }

    xpcom_method!(
        store_incoming => StoreIncoming(
            incoming_envelopes_json: *const ThinVec<::nsstring::nsCString>,
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn store_incoming(
        &self,
        incoming_envelopes_json: Option<&ThinVec<nsCString>>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<()> {
        Ok(FerryTask::for_store_incoming(
            &*self.store()?,
            incoming_envelopes_json.map(|v| v.as_slice()).unwrap_or(&[]),
            callback,
        )?
        .dispatch(&self.queue)?)
    }

    xpcom_method!(apply => Apply(callback: *const mozIBridgedSyncEngineApplyCallback));
    fn apply(&self, callback: &mozIBridgedSyncEngineApplyCallback) -> Result<()> {
        Ok(ApplyTask::new(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }

    xpcom_method!(
        set_uploaded => SetUploaded(
            server_modified_millis: i64,
            uploaded_ids: *const ThinVec<::nsstring::nsCString>,
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn set_uploaded(
        &self,
        server_modified_millis: i64,
        uploaded_ids: Option<&ThinVec<nsCString>>,
        callback: &mozIBridgedSyncEngineCallback,
    ) -> Result<()> {
        Ok(FerryTask::for_set_uploaded(
            &*self.store()?,
            server_modified_millis,
            uploaded_ids.map(|v| v.as_slice()).unwrap_or(&[]),
            callback,
        )?
        .dispatch(&self.queue)?)
    }

    xpcom_method!(
        sync_finished => SyncFinished(
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn sync_finished(&self, callback: &mozIBridgedSyncEngineCallback) -> Result<()> {
        Ok(FerryTask::for_sync_finished(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }

    xpcom_method!(
        reset => Reset(
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn reset(&self, callback: &mozIBridgedSyncEngineCallback) -> Result<()> {
        Ok(FerryTask::for_reset(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }

    xpcom_method!(
        wipe => Wipe(
            callback: *const mozIBridgedSyncEngineCallback
        )
    );
    fn wipe(&self, callback: &mozIBridgedSyncEngineCallback) -> Result<()> {
        Ok(FerryTask::for_wipe(&*self.store()?, callback)?.dispatch(&self.queue)?)
    }
}
