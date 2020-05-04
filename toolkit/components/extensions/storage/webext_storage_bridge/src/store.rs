/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    mem,
    path::PathBuf,
    sync::{Mutex, MutexGuard},
};

use once_cell::sync::OnceCell;
use sql_support::SqlInterruptHandle;
use webext_storage::store::Store;

use crate::error::{Error, Result};

/// Options for an extension storage area.
pub struct LazyStoreConfig {
    /// The path to the database file for this storage area.
    pub path: PathBuf,
}

/// A lazy store is automatically initialized on a background thread with its
/// configuration the first time it's used.
#[derive(Default)]
pub struct LazyStore {
    store: OnceCell<InterruptStore>,
    config: OnceCell<LazyStoreConfig>,
}

/// An `InterruptStore` wraps an inner extension store, and its interrupt
/// handle. The inner store is protected by a mutex, which we don't want to
/// lock on the main thread because it'll block waiting on any storage
/// operations running on the background thread, which defeats the point of the
/// interrupt. The interrupt handle is safe to access on the main thread, and
/// doesn't require locking.
struct InterruptStore {
    inner: Mutex<Store>,
    handle: SqlInterruptHandle,
}

impl LazyStore {
    /// Configures the lazy store. Returns an error if the store has already
    /// been configured. This method should be called from the main thread.
    pub fn configure(&self, config: LazyStoreConfig) -> Result<()> {
        self.config
            .set(config)
            .map_err(|_| Error::AlreadyConfigured)
    }

    /// Interrupts all pending operations on the store. If a database statement
    /// is currently running, this will interrupt that statement. If the
    /// statement is a write inside an active transaction, the entire
    /// transaction will be rolled back. This method should be called from the
    /// main thread.
    pub fn interrupt(&self) {
        if let Some(outer) = self.store.get() {
            outer.handle.interrupt();
        }
    }

    /// Returns the underlying store, initializing it if needed. This method
    /// should only be called from a background thread or task queue, since
    /// opening the database does I/O.
    pub fn get(&self) -> Result<MutexGuard<'_, Store>> {
        Ok(self
            .store
            .get_or_try_init(|| match self.config.get() {
                Some(config) => {
                    let store = Store::new(&config.path)?;
                    let handle = store.interrupt_handle();
                    Ok(InterruptStore {
                        inner: Mutex::new(store),
                        handle,
                    })
                }
                None => Err(Error::NotConfigured),
            })?
            .inner
            .lock()
            .unwrap())
    }

    /// Tears down the store. If the store wasn't initialized, this is a no-op.
    /// This should only be called from a background thread or task queue,
    /// because closing the database also does I/O.
    pub fn teardown(self) -> Result<()> {
        if let Some(store) = self
            .store
            .into_inner()
            .map(|outer| outer.inner.into_inner().unwrap())
        {
            if let Err((store, error)) = store.close() {
                // Since we're most likely being called during shutdown, leak
                // the store on error...it'll be cleaned up when the process
                // quits, anyway. We don't want to drop it, because its
                // destructor will try to close it again, and panic on error.
                // That'll become a shutdown crash, which we want to avoid.
                mem::forget(store);
                return Err(error.into());
            }
        }
        Ok(())
    }
}
