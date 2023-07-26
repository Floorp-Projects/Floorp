/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::db::sql_fns;
use crate::error::Result;
use rusqlite::{Connection, Transaction};
use sql_support::open_database::{
    ConnectionInitializer as MigrationLogic, Error as MigrationError, Result as MigrationResult,
};

const CREATE_SCHEMA_SQL: &str = include_str!("../sql/create_schema.sql");
const CREATE_SYNC_TEMP_TABLES_SQL: &str = include_str!("../sql/create_sync_temp_tables.sql");

pub struct WebExtMigrationLogin;

impl MigrationLogic for WebExtMigrationLogin {
    const NAME: &'static str = "webext storage db";
    const END_VERSION: u32 = 2;

    fn prepare(&self, conn: &Connection, _db_empty: bool) -> MigrationResult<()> {
        let initial_pragmas = "
            -- We don't care about temp tables being persisted to disk.
            PRAGMA temp_store = 2;
            -- we unconditionally want write-ahead-logging mode
            PRAGMA journal_mode=WAL;
            -- foreign keys seem worth enforcing!
            PRAGMA foreign_keys = ON;
        ";
        conn.execute_batch(initial_pragmas)?;
        define_functions(conn)?;
        conn.set_prepared_statement_cache_capacity(128);
        Ok(())
    }

    fn init(&self, db: &Transaction<'_>) -> MigrationResult<()> {
        log::debug!("Creating schema");
        db.execute_batch(CREATE_SCHEMA_SQL)?;
        Ok(())
    }

    fn upgrade_from(&self, db: &Transaction<'_>, version: u32) -> MigrationResult<()> {
        match version {
            1 => upgrade_from_1(db),
            _ => Err(MigrationError::IncompatibleVersion(version)),
        }
    }
}

fn define_functions(c: &Connection) -> MigrationResult<()> {
    use rusqlite::functions::FunctionFlags;
    c.create_scalar_function(
        "generate_guid",
        0,
        FunctionFlags::SQLITE_UTF8,
        sql_fns::generate_guid,
    )?;
    Ok(())
}

fn upgrade_from_1(db: &Connection) -> MigrationResult<()> {
    // We changed a not null constraint
    db.execute_batch("ALTER TABLE storage_sync_mirror RENAME TO old_mirror;")?;
    // just re-run the full schema commands to recreate the able.
    db.execute_batch(CREATE_SCHEMA_SQL)?;
    db.execute_batch(
        "INSERT OR IGNORE INTO storage_sync_mirror(guid, ext_id, data)
         SELECT guid, ext_id, data FROM old_mirror;",
    )?;
    db.execute_batch("DROP TABLE old_mirror;")?;
    db.execute_batch("PRAGMA user_version = 2;")?;
    Ok(())
}

// Note that we expect this to be called before and after a sync - before to
// ensure we are syncing with a clean state, after to be good memory citizens
// given the temp tables are in memory.
pub fn create_empty_sync_temp_tables(db: &Connection) -> Result<()> {
    log::debug!("Initializing sync temp tables");
    db.execute_batch(CREATE_SYNC_TEMP_TABLES_SQL)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::db::test::new_mem_db;
    use rusqlite::Error;
    use sql_support::open_database::test_utils::MigratedDatabaseFile;
    use sql_support::ConnExt;

    const CREATE_SCHEMA_V1_SQL: &str = include_str!("../sql/tests/create_schema_v1.sql");

    #[test]
    fn test_create_schema_twice() {
        let db = new_mem_db();
        db.execute_batch(CREATE_SCHEMA_SQL)
            .expect("should allow running twice");
    }

    #[test]
    fn test_create_empty_sync_temp_tables_twice() {
        let db = new_mem_db();
        create_empty_sync_temp_tables(&db).expect("should work first time");
        // insert something into our new temp table and check it's there.
        db.execute_batch(
            "INSERT INTO temp.storage_sync_staging
                            (guid, ext_id) VALUES
                            ('guid', 'ext_id');",
        )
        .expect("should work once");
        let count = db
            .query_row_and_then(
                "SELECT COUNT(*) FROM temp.storage_sync_staging;",
                [],
                |row| row.get::<_, u32>(0),
            )
            .expect("query should work");
        assert_eq!(count, 1, "should be one row");

        // re-execute
        create_empty_sync_temp_tables(&db).expect("should second first time");
        // and it should have deleted existing data.
        let count = db
            .query_row_and_then(
                "SELECT COUNT(*) FROM temp.storage_sync_staging;",
                [],
                |row| row.get::<_, u32>(0),
            )
            .expect("query should work");
        assert_eq!(count, 0, "should be no rows");
    }

    #[test]
    fn test_all_upgrades() -> Result<()> {
        let db_file = MigratedDatabaseFile::new(WebExtMigrationLogin, CREATE_SCHEMA_V1_SQL);
        db_file.run_all_upgrades();
        let db = db_file.open();

        let get_id_data = |guid: &str| -> Result<(Option<String>, Option<String>)> {
            let (ext_id, data) = db
                .try_query_row::<_, Error, _, _>(
                    "SELECT ext_id, data FROM storage_sync_mirror WHERE guid = :guid",
                    &[(":guid", &guid.to_string())],
                    |row| Ok((row.get(0)?, row.get(1)?)),
                    true,
                )?
                .expect("row should exist.");
            Ok((ext_id, data))
        };
        assert_eq!(
            get_id_data("guid-1")?,
            (Some("ext-id-1".to_string()), Some("data-1".to_string()))
        );
        assert_eq!(
            get_id_data("guid-2")?,
            (Some("ext-id-2".to_string()), Some("data-2".to_string()))
        );
        Ok(())
    }

    #[test]
    fn test_upgrade_2() -> Result<()> {
        let _ = env_logger::try_init();

        let db_file = MigratedDatabaseFile::new(WebExtMigrationLogin, CREATE_SCHEMA_V1_SQL);
        db_file.upgrade_to(2);
        let db = db_file.open();

        // Should be able to insert a new with a NULL ext_id
        db.execute_batch(
            "INSERT INTO storage_sync_mirror(guid, ext_id, data)
             VALUES ('guid-3', NULL, NULL);",
        )?;
        Ok(())
    }
}
