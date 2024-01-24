/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{fs::remove_file, path::PathBuf, sync::Arc};

use interrupt_support::SqlInterruptHandle;
use once_cell::sync::OnceCell;
use webext_storage::store::WebExtStorageStore as Store;

use crate::error::{self, Error};

/// Options for an extension storage area.
pub struct LazyStoreConfig {
    /// The path to the database file for this storage area.
    pub path: PathBuf,
    /// The path to the old kinto database. If it exists, we should attempt to
    /// migrate from this database as soon as we open our DB. It's not Option<>
    /// because the caller will not have checked whether it exists or not, so
    /// will assume it might.
    pub kinto_path: PathBuf,
}

/// A lazy store is automatically initialized on a background thread with its
/// configuration the first time it's used.
#[derive(Default)]
pub struct LazyStore {
    store: OnceCell<InterruptStore>,
    config: OnceCell<LazyStoreConfig>,
}

/// An `InterruptStore` wraps an inner extension store, and its interrupt
/// handle.
struct InterruptStore {
    inner: Store,
    handle: Arc<SqlInterruptHandle>,
}

impl LazyStore {
    /// Configures the lazy store. Returns an error if the store has already
    /// been configured. This method should be called from the main thread.
    pub fn configure(&self, config: LazyStoreConfig) -> error::Result<()> {
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
    pub fn get(&self) -> error::Result<&Store> {
        Ok(&self
            .store
            .get_or_try_init(|| match self.config.get() {
                Some(config) => {
                    let store = init_store(config)?;
                    let handle = store.interrupt_handle();
                    Ok(InterruptStore {
                        inner: store,
                        handle,
                    })
                }
                None => Err(Error::NotConfigured),
            })?
            .inner)
    }

    /// Tears down the store. If the store wasn't initialized, this is a no-op.
    /// This should only be called from a background thread or task queue,
    /// because closing the database also does I/O.
    pub fn teardown(self) -> error::Result<()> {
        if let Some(store) = self.store.into_inner() {
            store.inner.close()?;
        }
        Ok(())
    }
}

// Initialize the store, performing a migration if necessary.
// The requirements for migration are, roughly:
// * If kinto_path doesn't exist, we don't try to migrate.
// * If our DB path exists, we assume we've already migrated and don't try again
// * If the migration fails, we close our store and delete the DB, then return
//   a special error code which tells our caller about the failure. It's then
//   expected to fallback to the "old" kinto store and we'll try next time.
// Note that the migrate() method on the store is written such that is should
// ignore all "read" errors from the source, but propagate "write" errors on our
// DB - the intention is that things like corrupted source databases never fail,
// but disk-space failures on our database does.
fn init_store(config: &LazyStoreConfig) -> error::Result<Store> {
    let should_migrate = config.kinto_path.exists() && !config.path.exists();
    let store = Store::new(&config.path)?;
    if should_migrate {
        match store.migrate(&config.kinto_path) {
            // It's likely to be too early for us to stick the MigrationInfo
            // into the sync telemetry, a separate call to `take_migration_info`
            // must be made to the store (this is done by telemetry after it's
            // ready to submit the data).
            Ok(()) => {
                // need logging, but for now let's print to stdout.
                println!("extension-storage: migration complete");
                Ok(store)
            }
            Err(e) => {
                println!("extension-storage: migration failure: {e}");
                if let Err(e) = store.close() {
                    // welp, this probably isn't going to end well...
                    println!(
                        "extension-storage: failed to close the store after migration failure: {e}"
                    );
                }
                if let Err(e) = remove_file(&config.path) {
                    // this is bad - if it happens regularly it will defeat
                    // out entire migration strategy - we'll assume it
                    // worked.
                    // So it's desirable to make noise if this happens.
                    println!("Failed to remove file after failed migration: {e}");
                }
                Err(Error::MigrationFailed(e))
            }
        }
    } else {
        Ok(store)
    }
}
