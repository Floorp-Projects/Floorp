/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    mem,
    path::PathBuf,
    sync::{Mutex, MutexGuard},
};

use once_cell::sync::OnceCell;
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
    store: OnceCell<Mutex<Store>>,
    config: OnceCell<LazyStoreConfig>,
}

impl LazyStore {
    /// Configures the lazy store. Returns an error if the store has already
    /// been configured. This method should be called from the main thread.
    pub fn configure(&self, config: LazyStoreConfig) -> Result<()> {
        self.config
            .set(config)
            .map_err(|_| Error::AlreadyConfigured)
    }

    /// Returns the underlying store, initializing it if needed. This method
    /// should only be called from a background thread or task queue, since
    /// opening the database does I/O.
    pub fn get(&self) -> Result<MutexGuard<'_, Store>> {
        Ok(self
            .store
            .get_or_try_init(|| match self.config.get() {
                Some(config) => Ok(Mutex::new(Store::new(&config.path)?)),
                None => Err(Error::NotConfigured),
            })?
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
            .map(|mutex| mutex.into_inner().unwrap())
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
