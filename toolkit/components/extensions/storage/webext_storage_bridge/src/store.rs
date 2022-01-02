/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    fs::remove_file,
    mem,
    path::PathBuf,
    sync::{Mutex, MutexGuard},
};

use golden_gate::{ApplyResults, BridgedEngine, Guid, IncomingEnvelope};
use once_cell::sync::OnceCell;
use sql_support::SqlInterruptHandle;
use webext_storage::store::Store;

use crate::error::{Error, Result};

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
                    let store = init_store(config)?;
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

// `Store::bridged_engine()` returns a `BridgedEngine` implementation, but we
// provide our own for `LazyStore` that forwards to it. This is for three
// reasons.
//
//   1. We need to override the associated `Error` type, since Golden Gate has
//      an `Into<nsresult>` bound for errors. We can't satisfy this bound in
//      `webext_storage` because `nsresult` is a Gecko-only type. We could try
//      to reduce the boilerplate by declaring an `AsBridgedEngine` trait, with
//      associated types for `Error` and `Engine`, but then we run into...
//   2. `Store::bridged_engine()` returns a type with a lifetime parameter,
//      because the engine can't outlive the store. But we can't represent that
//      in our `AsBridgedEngine` trait without an associated type constructor,
//      which Rust doesn't support yet (rust-lang/rfcs#1598).
//   3. Related to (2), our store is lazily initialized behind a mutex, so
//      `LazyStore::get()` returns a guard. But now, `Store::bridged_engine()`
//      must return a type that lives only as long as the guard...which it
//      can't, because it doesn't know that! This is another case where
//      higher-kinded types would be helpful, so that our hypothetical
//      `AsBridgedEngine::bridged_engine()` could return either a `T<'a>` or
//      `MutexGuard<'_, T<'a>>`, but that's not possible now.
//
// There are workarounds for Rust's lack of HKTs, but they all introduce
// indirection and cognitive overhead. So we do the simple thing and implement
// `BridgedEngine`, with a bit more boilerplate.
impl BridgedEngine for LazyStore {
    type Error = Error;

    fn last_sync(&self) -> Result<i64> {
        Ok(self.get()?.bridged_engine().last_sync()?)
    }

    fn set_last_sync(&self, last_sync_millis: i64) -> Result<()> {
        Ok(self
            .get()?
            .bridged_engine()
            .set_last_sync(last_sync_millis)?)
    }

    fn sync_id(&self) -> Result<Option<String>> {
        Ok(self.get()?.bridged_engine().sync_id()?)
    }

    fn reset_sync_id(&self) -> Result<String> {
        Ok(self.get()?.bridged_engine().reset_sync_id()?)
    }

    fn ensure_current_sync_id(&self, new_sync_id: &str) -> Result<String> {
        Ok(self
            .get()?
            .bridged_engine()
            .ensure_current_sync_id(new_sync_id)?)
    }

    fn sync_started(&self) -> Result<()> {
        Ok(self.get()?.bridged_engine().sync_started()?)
    }

    fn store_incoming(&self, envelopes: &[IncomingEnvelope]) -> Result<()> {
        Ok(self.get()?.bridged_engine().store_incoming(envelopes)?)
    }

    fn apply(&self) -> Result<ApplyResults> {
        Ok(self.get()?.bridged_engine().apply()?)
    }

    fn set_uploaded(&self, server_modified_millis: i64, ids: &[Guid]) -> Result<()> {
        Ok(self
            .get()?
            .bridged_engine()
            .set_uploaded(server_modified_millis, ids)?)
    }

    fn sync_finished(&self) -> Result<()> {
        Ok(self.get()?.bridged_engine().sync_finished()?)
    }

    fn reset(&self) -> Result<()> {
        Ok(self.get()?.bridged_engine().reset()?)
    }

    fn wipe(&self) -> Result<()> {
        Ok(self.get()?.bridged_engine().wipe()?)
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
fn init_store(config: &LazyStoreConfig) -> Result<Store> {
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
                println!("extension-storage: migration failure: {}", e);
                if let Err((store, e)) = store.close() {
                    // welp, this probably isn't going to end well...
                    println!(
                        "extension-storage: failed to close the store after migration failure: {}",
                        e
                    );
                    // I don't think we should hit this in this case - I guess we
                    // could sleep and retry if we thought we were.
                    mem::drop(store);
                }
                if let Err(e) = remove_file(&config.path) {
                    // this is bad - if it happens regularly it will defeat
                    // out entire migration strategy - we'll assume it
                    // worked.
                    // So it's desirable to make noise if this happens.
                    println!("Failed to remove file after failed migration: {}", e);
                }
                Err(Error::MigrationFailed(e))
            }
        }
    } else {
        Ok(store)
    }
}
