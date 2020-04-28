/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    cell::{Ref, RefCell},
    ffi::OsString,
    mem, str,
    sync::Arc,
};

use moz_task::{self, DispatchOptions, TaskRunnable};
use nserror::{nsresult, NS_OK};
use nsstring::{nsACString, nsString};
use xpcom::{
    interfaces::{mozIExtensionStorageCallback, nsIFile, nsISerialEventTarget},
    RefPtr,
};

use crate::error::{Error, Result};
use crate::op::{StorageOp, StorageTask, TeardownTask};
use crate::store::{LazyStore, LazyStoreConfig};

/// An XPCOM component class for the Rust extension storage API. This class
/// implements the interfaces needed for syncing and storage.
#[derive(xpcom)]
#[xpimplements(mozIConfigurableExtensionStorageArea)]
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
    fn dispatch(
        &self,
        ext_id: &nsACString,
        op: StorageOp,
        callback: &mozIExtensionStorageCallback,
    ) -> Result<()> {
        let name = op.name();
        let task = StorageTask::new(
            Arc::downgrade(&*self.store()?),
            str::from_utf8(&*ext_id)?,
            op,
            callback,
        )?;
        let runnable = TaskRunnable::new(name, Box::new(task))?;
        // `may_block` schedules the runnable on a dedicated I/O pool.
        runnable
            .dispatch_with_options(self.queue.coerce(), DispatchOptions::new().may_block(true))?;
        Ok(())
    }

    xpcom_method!(
        configure => Configure(
            database_file: *const nsIFile
        )
    );
    /// Sets up the storage area.
    fn configure(&self, database_file: &nsIFile) -> Result<()> {
        let mut raw_path = nsString::new();
        // `nsIFile::GetPath` gives us a UTF-16-encoded version of its
        // native path, which we must turn back into a platform-native
        // string. We can't use `nsIFile::nativePath()` here because
        // it's marked as `nostdcall`, which Rust doesn't support.
        unsafe { database_file.GetPath(&mut *raw_path) }.to_result()?;
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
        self.store()?.configure(LazyStoreConfig {
            path: native_path.into(),
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
        let value = serde_json::from_str(str::from_utf8(&*json)?)?;
        self.dispatch(ext_id, StorageOp::Set(value), callback)?;
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
        let keys = serde_json::from_str(str::from_utf8(&*json)?)?;
        self.dispatch(ext_id, StorageOp::Get(keys), callback)
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
        let keys = serde_json::from_str(str::from_utf8(&*json)?)?;
        self.dispatch(ext_id, StorageOp::Remove(keys), callback)
    }

    xpcom_method!(
        clear => Clear(
            ext_id: *const ::nsstring::nsACString,
            callback: *const mozIExtensionStorageCallback
        )
    );
    /// Removes all keys and values.
    fn clear(&self, ext_id: &nsACString, callback: &mozIExtensionStorageCallback) -> Result<()> {
        self.dispatch(ext_id, StorageOp::Clear, callback)
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
                // "Hey, wait a second, if `store` is our only strong reference,
                // why are we cloning it here?" Because we want to put our owned
                // reference back if dispatch fails, so that we don't leak the
                // store.
                if let Err(error) = teardown(&self.queue, Arc::clone(&store), callback) {
                    *maybe_store = Some(store);
                    return Err(error);
                }
            }
            None => return Err(Error::AlreadyTornDown),
        }
        Ok(())
    }
}

fn teardown(
    queue: &nsISerialEventTarget,
    store: Arc<LazyStore>,
    callback: &mozIExtensionStorageCallback,
) -> Result<()> {
    let task = TeardownTask::new(store, callback)?;
    let runnable = TaskRunnable::new(TeardownTask::name(), Box::new(task))?;
    runnable.dispatch_with_options(queue.coerce(), DispatchOptions::new().may_block(true))?;
    Ok(())
}
