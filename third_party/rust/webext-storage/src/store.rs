/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::api::{self, StorageChanges};
use crate::db::{StorageDb, ThreadSafeStorageDb};
use crate::error::*;
use crate::migration::{migrate, MigrationInfo};
use crate::sync;
use std::path::Path;
use std::sync::Arc;

use interrupt_support::SqlInterruptHandle;
use serde_json::Value as JsonValue;

/// A store is used to access `storage.sync` data. It manages an underlying
/// database connection, and exposes methods for reading and writing storage
/// items scoped to an extension ID. Each item is a JSON object, with one or
/// more string keys, and values of any type that can serialize to JSON.
///
/// An application should create only one store, and manage the instance as a
/// singleton. While this isn't enforced, if you make multiple stores pointing
/// to the same database file, you are going to have a bad time: each store will
/// create its own database connection, using up extra memory and CPU cycles,
/// and causing write contention. For this reason, you should only call
/// `Store::new()` (or `webext_store_new()`, from the FFI) once.
///
/// Note that our Db implementation is behind an Arc<> because we share that
/// connection with our sync engines - ie, these engines also hold an Arc<>
/// around the same object.
pub struct WebExtStorageStore {
    db: Arc<ThreadSafeStorageDb>,
}

impl WebExtStorageStore {
    /// Creates a store backed by a database at `db_path`. The path can be a
    /// file path or `file:` URI.
    pub fn new(db_path: impl AsRef<Path>) -> Result<Self> {
        let db = StorageDb::new(db_path)?;
        Ok(Self {
            db: Arc::new(ThreadSafeStorageDb::new(db)),
        })
    }

    /// Creates a store backed by an in-memory database.
    #[cfg(test)]
    pub fn new_memory(db_path: &str) -> Result<Self> {
        let db = StorageDb::new_memory(db_path)?;
        Ok(Self {
            db: Arc::new(ThreadSafeStorageDb::new(db)),
        })
    }

    /// Returns an interrupt handle for this store.
    pub fn interrupt_handle(&self) -> Arc<SqlInterruptHandle> {
        self.db.interrupt_handle()
    }

    /// Sets one or more JSON key-value pairs for an extension ID. Returns a
    /// list of changes, with existing and new values for each key in `val`.
    pub fn set(&self, ext_id: &str, val: JsonValue) -> Result<StorageChanges> {
        let db = self.db.lock();
        let tx = db.unchecked_transaction()?;
        let result = api::set(&tx, ext_id, val)?;
        tx.commit()?;
        Ok(result)
    }

    /// Returns information about per-extension usage
    pub fn usage(&self) -> Result<Vec<crate::UsageInfo>> {
        let db = self.db.lock();
        api::usage(&db)
    }

    /// Returns the values for one or more keys `keys` can be:
    ///
    /// - `null`, in which case all key-value pairs for the extension are
    ///   returned, or an empty object if the extension doesn't have any
    ///   stored data.
    /// - A single string key, in which case an object with only that key
    ///   and its value is returned, or an empty object if the key doesn't
    //    exist.
    /// - An array of string keys, in which case an object with only those
    ///   keys and their values is returned. Any keys that don't exist will be
    ///   omitted.
    /// - An object where the property names are keys, and each value is the
    ///   default value to return if the key doesn't exist.
    ///
    /// This method always returns an object (that is, a
    /// `serde_json::Value::Object`).
    pub fn get(&self, ext_id: &str, keys: JsonValue) -> Result<JsonValue> {
        // Don't care about transactions here.
        let db = self.db.lock();
        api::get(&db, ext_id, keys)
    }

    /// Deletes the values for one or more keys. As with `get`, `keys` can be
    /// either a single string key, or an array of string keys. Returns a list
    /// of changes, where each change contains the old value for each deleted
    /// key.
    pub fn remove(&self, ext_id: &str, keys: JsonValue) -> Result<StorageChanges> {
        let db = self.db.lock();
        let tx = db.unchecked_transaction()?;
        let result = api::remove(&tx, ext_id, keys)?;
        tx.commit()?;
        Ok(result)
    }

    /// Deletes all key-value pairs for the extension. As with `remove`, returns
    /// a list of changes, where each change contains the old value for each
    /// deleted key.
    pub fn clear(&self, ext_id: &str) -> Result<StorageChanges> {
        let db = self.db.lock();
        let tx = db.unchecked_transaction()?;
        let result = api::clear(&tx, ext_id)?;
        tx.commit()?;
        Ok(result)
    }

    /// Returns the bytes in use for the specified items (which can be null,
    /// a string, or an array)
    pub fn get_bytes_in_use(&self, ext_id: &str, keys: JsonValue) -> Result<usize> {
        let db = self.db.lock();
        api::get_bytes_in_use(&db, ext_id, keys)
    }

    /// Returns a bridged sync engine for Desktop for this store.
    pub fn bridged_engine(&self) -> sync::BridgedEngine {
        sync::BridgedEngine::new(&self.db)
    }

    /// Closes the store and its database connection. See the docs for
    /// `StorageDb::close` for more details on when this can fail.
    pub fn close(self) -> Result<()> {
        // Even though this consumes `self`, the fact we use an Arc<> means
        // we can't guarantee we can actually consume the inner DB - so do
        // the best we can.
        let shared: ThreadSafeStorageDb = match Arc::try_unwrap(self.db) {
            Ok(shared) => shared,
            _ => {
                // The only way this is possible is if the sync engine has an operation
                // running - but that shouldn't be possible in practice because desktop
                // uses a single "task queue" such that the close operation can't possibly
                // be running concurrently with any sync or storage tasks.

                // If this *could* get hit, rusqlite will attempt to close the DB connection
                // as it is dropped, and if that close fails, then rusqlite 0.28.0 and earlier
                // would panic - but even that only happens if prepared statements are
                // not finalized, which ruqlite also does.

                // tl;dr - this should be impossible. If it was possible, rusqlite might panic,
                // but we've never seen it panic in practice other places we don't close
                // connections, and the next rusqlite version will not panic anyway.
                // So this-is-fine.jpg
                log::warn!("Attempting to close a store while other DB references exist.");
                return Err(Error::OtherConnectionReferencesExist);
            }
        };
        // consume the mutex and get back the inner.
        let db = shared.into_inner();
        db.close()
    }

    /// Gets the changes which the current sync applied. Should be used
    /// immediately after the bridged engine is told to apply incoming changes,
    /// and can be used to notify observers of the StorageArea of the changes
    /// that were applied.
    /// The result is a Vec of already JSON stringified changes.
    pub fn get_synced_changes(&self) -> Result<Vec<sync::SyncedExtensionChange>> {
        let db = self.db.lock();
        sync::get_synced_changes(&db)
    }

    /// Migrates data from a database in the format of the "old" kinto
    /// implementation. Information about how the migration went is stored in
    /// the database, and can be read using `Self::take_migration_info`.
    ///
    /// Note that `filename` isn't normalized or canonicalized.
    pub fn migrate(&self, filename: impl AsRef<Path>) -> Result<()> {
        let db = self.db.lock();
        let tx = db.unchecked_transaction()?;
        let result = migrate(&tx, filename.as_ref())?;
        tx.commit()?;
        // Failing to store this information should not cause migration failure.
        if let Err(e) = result.store(&db) {
            debug_assert!(false, "Migration error: {:?}", e);
            log::warn!("Failed to record migration telmetry: {}", e);
        }
        Ok(())
    }

    /// Read-and-delete (e.g. `take` in rust parlance, see Option::take)
    /// operation for any MigrationInfo stored in this database.
    pub fn take_migration_info(&self) -> Result<Option<MigrationInfo>> {
        let db = self.db.lock();
        let tx = db.unchecked_transaction()?;
        let result = MigrationInfo::take(&tx)?;
        tx.commit()?;
        Ok(result)
    }
}

#[cfg(test)]
pub mod test {
    use super::*;
    #[test]
    fn test_send() {
        fn ensure_send<T: Send>() {}
        // Compile will fail if not send.
        ensure_send::<WebExtStorageStore>();
    }

    pub fn new_mem_store() -> WebExtStorageStore {
        WebExtStorageStore {
            db: Arc::new(ThreadSafeStorageDb::new(crate::db::test::new_mem_db())),
        }
    }
}
