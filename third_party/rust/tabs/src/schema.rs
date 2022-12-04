/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tabs is a bit special - it's a trivial SQL schema and is only used as a persistent
// cache, and the semantics of the "tabs" collection means there's no need for
// syncChangeCounter/syncStatus nor a mirror etc.

use rusqlite::{Connection, Transaction};
use sql_support::open_database::{
    ConnectionInitializer as MigrationLogic, Error as MigrationError, Result as MigrationResult,
};

// The payload is json and this module doesn't need to deserialize, so we just
// store each "payload" as a row.
// On each Sync we delete all local rows re-populate them with every record on
// the server. When we read the DB, we also read every single record.
// So we have no primary keys, no foreign keys, and really completely waste the
// fact we are using sql.
const CREATE_SCHEMA_SQL: &str = "
    CREATE TABLE IF NOT EXISTS tabs (
        payload TEXT NOT NULL
    );
";

pub struct TabsMigrationLogin;

impl MigrationLogic for TabsMigrationLogin {
    const NAME: &'static str = "tabs storage db";
    const END_VERSION: u32 = 1;

    fn prepare(&self, conn: &Connection, _db_empty: bool) -> MigrationResult<()> {
        let initial_pragmas = "
            -- We don't care about temp tables being persisted to disk.
            PRAGMA temp_store = 2;
            -- we unconditionally want write-ahead-logging mode.
            PRAGMA journal_mode=WAL;
            -- foreign keys seem worth enforcing (and again, we don't care in practice)
            PRAGMA foreign_keys = ON;
        ";
        conn.execute_batch(initial_pragmas)?;
        // This is where we'd define our sql functions if we had any!
        conn.set_prepared_statement_cache_capacity(128);
        Ok(())
    }

    fn init(&self, db: &Transaction<'_>) -> MigrationResult<()> {
        log::debug!("Creating schema");
        db.execute_batch(CREATE_SCHEMA_SQL)?;
        Ok(())
    }

    fn upgrade_from(&self, _db: &Transaction<'_>, version: u32) -> MigrationResult<()> {
        Err(MigrationError::IncompatibleVersion(version))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::storage::TabsStorage;

    #[test]
    fn test_create_schema_twice() {
        let mut db = TabsStorage::new_with_mem_path("test");
        let conn = db.open_or_create().unwrap();
        conn.execute_batch(CREATE_SCHEMA_SQL)
            .expect("should allow running twice");
    }
}
