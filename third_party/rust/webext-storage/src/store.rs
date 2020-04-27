/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::api::{self, StorageChanges};
use crate::db::StorageDb;
use crate::error::*;
use std::path::Path;

use serde_json::Value as JsonValue;

pub struct Store {
    db: StorageDb,
}

impl Store {
    // functions to create instances.
    pub fn new(db_path: impl AsRef<Path>) -> Result<Self> {
        Ok(Self {
            db: StorageDb::new(db_path)?,
        })
    }

    #[cfg(test)]
    pub fn new_memory(db_path: &str) -> Result<Self> {
        Ok(Self {
            db: StorageDb::new_memory(db_path)?,
        })
    }

    // The "public API".
    pub fn set(&self, ext_id: &str, val: JsonValue) -> Result<StorageChanges> {
        let mut conn = self.db.writer.lock().unwrap();
        let tx = conn.transaction()?;
        let result = api::set(&tx, ext_id, val)?;
        tx.commit()?;
        Ok(result)
    }

    pub fn get(&self, ext_id: &str, keys: JsonValue) -> Result<JsonValue> {
        // Don't care about transactions here.
        let conn = self.db.writer.lock().unwrap();
        api::get(&conn, ext_id, keys)
    }

    pub fn remove(&self, ext_id: &str, keys: JsonValue) -> Result<StorageChanges> {
        let mut conn = self.db.writer.lock().unwrap();
        let tx = conn.transaction()?;
        let result = api::remove(&tx, ext_id, keys)?;
        tx.commit()?;
        Ok(result)
    }

    pub fn clear(&self, ext_id: &str) -> Result<StorageChanges> {
        let mut conn = self.db.writer.lock().unwrap();
        let tx = conn.transaction()?;
        let result = api::clear(&tx, ext_id)?;
        tx.commit()?;
        Ok(result)
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
