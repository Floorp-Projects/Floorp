/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::api::{self, StorageChanges};
use crate::db::StorageDb;
use crate::error::*;
use crate::sync;
use std::path::Path;
use std::result;

use serde_json::Value as JsonValue;
use sql_support::SqlInterruptHandle;

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
pub struct Store {
    db: StorageDb,
}

impl Store {
    /// Creates a store backed by a database at `db_path`. The path can be a
    /// file path or `file:` URI.
    pub fn new(db_path: impl AsRef<Path>) -> Result<Self> {
        Ok(Self {
            db: StorageDb::new(db_path)?,
        })
    }

    /// Creates a store backed by an in-memory database.
    #[cfg(test)]
    pub fn new_memory(db_path: &str) -> Result<Self> {
        Ok(Self {
            db: StorageDb::new_memory(db_path)?,
        })
    }

    /// Returns an interrupt handle for this store.
    pub fn interrupt_handle(&self) -> SqlInterruptHandle {
        self.db.interrupt_handle()
    }

    /// Sets one or more JSON key-value pairs for an extension ID. Returns a
    /// list of changes, with existing and new values for each key in `val`.
    pub fn set(&self, ext_id: &str, val: JsonValue) -> Result<StorageChanges> {
        let tx = self.db.unchecked_transaction()?;
        let result = api::set(&tx, ext_id, val)?;
        tx.commit()?;
        Ok(result)
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
        api::get(&self.db, ext_id, keys)
    }

    /// Deletes the values for one or more keys. As with `get`, `keys` can be
    /// either a single string key, or an array of string keys. Returns a list
    /// of changes, where each change contains the old value for each deleted
    /// key.
    pub fn remove(&self, ext_id: &str, keys: JsonValue) -> Result<StorageChanges> {
        let tx = self.db.unchecked_transaction()?;
        let result = api::remove(&tx, ext_id, keys)?;
        tx.commit()?;
        Ok(result)
    }

    /// Deletes all key-value pairs for the extension. As with `remove`, returns
    /// a list of changes, where each change contains the old value for each
    /// deleted key.
    pub fn clear(&self, ext_id: &str) -> Result<StorageChanges> {
        let tx = self.db.unchecked_transaction()?;
        let result = api::clear(&tx, ext_id)?;
        tx.commit()?;
        Ok(result)
    }

    /// Wipe all local data without syncing or returning any information about
    /// the deletion.
    pub fn wipe_all(&self) -> Result<()> {
        let tx = self.db.unchecked_transaction()?;
        api::wipe_all(&tx)?;
        tx.commit()?;
        Ok(())
    }

    /// Returns a bridged sync engine for Desktop for this store.
    pub fn bridged_engine(&self) -> sync::BridgedEngine<'_> {
        sync::BridgedEngine::new(&self.db)
    }

    /// Closes the store and its database connection. See the docs for
    /// `StorageDb::close` for more details on when this can fail.
    pub fn close(self) -> result::Result<(), (Store, Error)> {
        self.db.close().map_err(|(db, err)| (Store { db }, err))
    }
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_send() {
        fn ensure_send<T: Send>() {}
        // Compile will fail if not send.
        ensure_send::<Store>();
    }
}
