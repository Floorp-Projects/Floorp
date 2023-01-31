/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::*;
use rusqlite::{named_params, Connection, OpenFlags, Transaction};
use serde_json::{Map, Value};
use sql_support::ConnExt;
use std::collections::HashSet;
use std::path::Path;

// Simple migration from the "old" kinto-with-sqlite-backing implementation
// to ours.
// Could almost be trivially done in JS using the regular public API if not
// for:
// * We don't want to enforce the same quotas when migrating.
// * We'd rather do the entire migration in a single transaction for perf
//   reasons.

// The sqlite database we migrate from has a very simple structure:
// * table collection_data with columns collection_name, record_id and record
// * `collection_name` is a string of form "default/{extension_id}"
// * `record_id` is `key-{key}`
// * `record` is a string with json, of form: {
//     id: {the record id repeated},
//     key: {the key},
//     data: {the actual data},
//     _status: {sync status},
//     last_modified: {timestamp},
//  }
// So the info we need is stored somewhat redundantly.
// Further:
// * There's a special collection_name "default/storage-sync-crypto" that
//   we don't want to migrate. Its record_id is 'keys' and its json has no
//   `data`

// Note we don't enforce a quota - we migrate everything - even if this means
// it's too big for the server to store. This is a policy decision - it's better
// to not lose data than to try and work out what data can be disposed of, as
// the addon has the ability to determine this.

// Our error strategy is "ignore read errors, propagate write errors" under the
// assumption that the former tends to mean a damaged DB or file-system and is
// unlikely to work if we try later (eg, replacing the disk isn't likely to
// uncorrupt the DB), where the latter is likely to be disk-space or file-system
// error, but retry might work (eg, replacing the disk then trying again might
// make the writes work)

// The struct we read from the DB.
struct LegacyRow {
    col_name: String, // collection_name column
    record: String,   // record column
}

impl LegacyRow {
    // Parse the 2 columns from the DB into the data we need to insert into
    // our target database.
    fn parse(&self) -> Option<Parsed<'_>> {
        if self.col_name.len() < 8 {
            log::trace!("collection_name of '{}' is too short", self.col_name);
            return None;
        }
        if &self.col_name[..8] != "default/" {
            log::trace!("collection_name of '{}' isn't ours", self.col_name);
            return None;
        }
        let ext_id = &self.col_name[8..];
        let mut record_map = match serde_json::from_str(&self.record) {
            Ok(Value::Object(m)) => m,
            Ok(o) => {
                log::info!("skipping non-json-object 'record' column");
                log::trace!("record value is json, but not an object: {}", o);
                return None;
            }
            Err(e) => {
                log::info!("skipping non-json 'record' column");
                log::trace!("record value isn't json: {}", e);
                return None;
            }
        };

        let key = match record_map.remove("key") {
            Some(Value::String(s)) if !s.is_empty() => s,
            Some(o) => {
                log::trace!("key is json but not a string: {}", o);
                return None;
            }
            _ => {
                log::trace!("key doesn't exist in the map");
                return None;
            }
        };
        let data = match record_map.remove("data") {
            Some(d) => d,
            _ => {
                log::trace!("data doesn't exist in the map");
                return None;
            }
        };
        Some(Parsed { ext_id, key, data })
    }
}

// The info we parse from the raw DB strings.
struct Parsed<'a> {
    ext_id: &'a str,
    key: String,
    data: serde_json::Value,
}

pub fn migrate(tx: &Transaction<'_>, filename: &Path) -> Result<MigrationInfo> {
    // We do the grouping manually, collecting string values as we go.
    let mut last_ext_id = "".to_string();
    let mut curr_values: Vec<(String, serde_json::Value)> = Vec::new();
    let (rows, mut mi) = read_rows(filename);
    for row in rows {
        log::trace!("processing '{}' - '{}'", row.col_name, row.record);
        let parsed = match row.parse() {
            Some(p) => p,
            None => continue,
        };
        // Do our "grouping"
        if parsed.ext_id != last_ext_id {
            if !last_ext_id.is_empty() && !curr_values.is_empty() {
                // a different extension id - write what we have to the DB.
                let entries = do_insert(tx, &last_ext_id, curr_values)?;
                mi.extensions_successful += 1;
                mi.entries_successful += entries;
            }
            last_ext_id = parsed.ext_id.to_string();
            curr_values = Vec::new();
        }
        // no 'else' here - must also enter this block on ext_id change.
        if parsed.ext_id == last_ext_id {
            curr_values.push((parsed.key.to_string(), parsed.data));
            log::trace!(
                "extension {} now has {} keys",
                parsed.ext_id,
                curr_values.len()
            );
        }
    }
    // and the last one
    if !last_ext_id.is_empty() && !curr_values.is_empty() {
        // a different extension id - write what we have to the DB.
        let entries = do_insert(tx, &last_ext_id, curr_values)?;
        mi.extensions_successful += 1;
        mi.entries_successful += entries;
    }
    log::info!("migrated {} extensions: {:?}", mi.extensions_successful, mi);
    Ok(mi)
}

fn read_rows(filename: &Path) -> (Vec<LegacyRow>, MigrationInfo) {
    let flags = OpenFlags::SQLITE_OPEN_NO_MUTEX | OpenFlags::SQLITE_OPEN_READ_ONLY;
    let src_conn = match Connection::open_with_flags(filename, flags) {
        Ok(conn) => conn,
        Err(e) => {
            log::warn!("Failed to open the source DB: {}", e);
            return (Vec::new(), MigrationInfo::open_failure());
        }
    };
    // Failure to prepare the statement probably just means the source DB is
    // damaged.
    let mut stmt = match src_conn.prepare(
        "SELECT collection_name, record FROM collection_data
         WHERE collection_name != 'default/storage-sync-crypto'
         ORDER BY collection_name",
    ) {
        Ok(stmt) => stmt,
        Err(e) => {
            log::warn!("Failed to prepare the statement: {}", e);
            return (Vec::new(), MigrationInfo::open_failure());
        }
    };
    let rows = match stmt.query_and_then([], |row| -> Result<LegacyRow> {
        Ok(LegacyRow {
            col_name: row.get(0)?,
            record: row.get(1)?,
        })
    }) {
        Ok(r) => r,
        Err(e) => {
            log::warn!("Failed to read any rows from the source DB: {}", e);
            return (Vec::new(), MigrationInfo::open_failure());
        }
    };
    let all_rows: Vec<Result<LegacyRow>> = rows.collect();
    let entries = all_rows.len();
    let successful_rows: Vec<LegacyRow> = all_rows.into_iter().filter_map(Result::ok).collect();
    let distinct_extensions: HashSet<_> = successful_rows.iter().map(|c| &c.col_name).collect();

    let mi = MigrationInfo {
        entries,
        extensions: distinct_extensions.len(),
        // Populated later.
        extensions_successful: 0,
        entries_successful: 0,
        open_failure: false,
    };

    (successful_rows, mi)
}

/// Insert the extension and values. If there are multiple values with the same
/// key (which shouldn't be possible but who knows, database corruption causes
/// strange things), chooses an arbitrary one. Returns the number of entries
/// inserted, which could be different from `vals.len()` if multiple entries in
/// `vals` have the same key.
fn do_insert(tx: &Transaction<'_>, ext_id: &str, vals: Vec<(String, Value)>) -> Result<usize> {
    let mut map = Map::with_capacity(vals.len());
    for (key, val) in vals {
        map.insert(key, val);
    }
    let num_entries = map.len();
    tx.execute_cached(
        "INSERT OR REPLACE INTO storage_sync_data(ext_id, data, sync_change_counter)
         VALUES (:ext_id, :data, 1)",
        rusqlite::named_params! {
            ":ext_id": &ext_id,
            ":data": &Value::Object(map),
        },
    )?;
    Ok(num_entries)
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct MigrationInfo {
    /// The number of entries (rows in the original table) we attempted to
    /// migrate. Zero if there was some error in computing this number.
    ///
    /// Note that for the original table, a single row stores a single
    /// preference for one extension. That is, if you view the set of
    /// preferences for a given extension as a HashMap (as we do), it would be a
    /// single entry/key-value-pair in the map.
    pub entries: usize,
    /// The number of records we successfully migrated (equal to `entries` for
    /// entirely successful migrations).
    pub entries_successful: usize,
    /// The number of extensions (distinct extension ids) in the original
    /// table.
    pub extensions: usize,
    /// The number of extensions we successfully migrated
    pub extensions_successful: usize,
    /// True iff we failed to open the source DB at all.
    pub open_failure: bool,
}

impl MigrationInfo {
    /// Returns a MigrationInfo indicating that we failed to read any rows due
    /// to some error case (e.g. the database open failed, or some other very
    /// early read error).
    fn open_failure() -> Self {
        Self {
            open_failure: true,
            ..Self::default()
        }
    }

    const META_KEY: &'static str = "migration_info";

    /// Store `self` in the provided database under `Self::META_KEY`.
    pub(crate) fn store(&self, conn: &Connection) -> Result<()> {
        let json = serde_json::to_string(self)?;
        conn.execute(
            "INSERT OR REPLACE INTO meta(key, value) VALUES (:k, :v)",
            named_params! {
                ":k": Self::META_KEY,
                ":v": &json
            },
        )?;
        Ok(())
    }

    /// Get the MigrationInfo stored under `Self::META_KEY` (if any) out of the
    /// DB, and delete it.
    pub(crate) fn take(tx: &Transaction<'_>) -> Result<Option<Self>> {
        let s = tx.try_query_one::<String, _>(
            "SELECT value FROM meta WHERE key = :k",
            named_params! {
                ":k": Self::META_KEY,
            },
            false,
        )?;
        tx.execute(
            "DELETE FROM meta WHERE key = :k",
            named_params! {
                ":k": Self::META_KEY,
            },
        )?;
        if let Some(s) = s {
            match serde_json::from_str(&s) {
                Ok(v) => Ok(Some(v)),
                Err(e) => {
                    // Force test failure, but just log an error otherwise so that
                    // we commit the transaction that wil.
                    debug_assert!(false, "Failed to read migration JSON: {:?}", e);
                    error_support::report_error!(
                        "webext-storage-migration-json",
                        "Failed to read migration JSON: {}",
                        e
                    );
                    Ok(None)
                }
            }
        } else {
            Ok(None)
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    use crate::api;
    use crate::db::{test::new_mem_db, StorageDb};
    use serde_json::json;
    use tempfile::tempdir;

    fn init_source_db(path: impl AsRef<Path>, f: impl FnOnce(&Connection)) {
        let flags = OpenFlags::SQLITE_OPEN_NO_MUTEX
            | OpenFlags::SQLITE_OPEN_CREATE
            | OpenFlags::SQLITE_OPEN_READ_WRITE;
        let mut conn = Connection::open_with_flags(path, flags).expect("open should work");
        let tx = conn.transaction().expect("should be able to get a tx");
        tx.execute_batch(
            "CREATE TABLE collection_data (
                collection_name TEXT,
                record_id TEXT,
                record TEXT
            );",
        )
        .expect("create should work");
        f(&tx);
        tx.commit().expect("should commit");
        conn.close().expect("close should work");
    }

    // Create a test database, populate it via the callback, migrate it, and
    // return a connection to the new, migrated DB for further checking.
    fn do_migrate<F>(expect_mi: MigrationInfo, f: F) -> StorageDb
    where
        F: FnOnce(&Connection),
    {
        let tmpdir = tempdir().unwrap();
        let path = tmpdir.path().join("source.db");
        init_source_db(path, f);

        // now migrate
        let mut db = new_mem_db();
        let tx = db.transaction().expect("tx should work");

        let mi = migrate(&tx, &tmpdir.path().join("source.db")).expect("migrate should work");
        tx.commit().expect("should work");
        assert_eq!(mi, expect_mi);
        db
    }

    fn assert_has(c: &Connection, ext_id: &str, expect: Value) {
        assert_eq!(
            api::get(c, ext_id, json!(null)).expect("should get"),
            expect
        );
    }

    const HAPPY_PATH_SQL: &str = r#"
        INSERT INTO collection_data(collection_name, record)
        VALUES
            ('default/{e7fefcf3-b39c-4f17-5215-ebfe120a7031}', '{"id":"key-userWelcomed","key":"userWelcomed","data":1570659224457,"_status":"synced","last_modified":1579755940527}'),
            ('default/{e7fefcf3-b39c-4f17-5215-ebfe120a7031}', '{"id":"key-isWho","key":"isWho","data":"4ec8109f","_status":"synced","last_modified":1579755940497}'),
            ('default/storage-sync-crypto', '{"id":"keys","keys":{"default":["rQ=","lR="],"collections":{"extension@redux.devtools":["Bd=","ju="]}}}'),
            ('default/https-everywhere@eff.org', '{"id":"key-userRules","key":"userRules","data":[],"_status":"synced","last_modified":1570079920045}'),
            ('default/https-everywhere@eff.org', '{"id":"key-ruleActiveStates","key":"ruleActiveStates","data":{},"_status":"synced","last_modified":1570079919993}'),
            ('default/https-everywhere@eff.org', '{"id":"key-migration_5F_version","key":"migration_version","data":2,"_status":"synced","last_modified":1570079919966}')
    "#;
    const HAPPY_PATH_MIGRATION_INFO: MigrationInfo = MigrationInfo {
        entries: 5,
        entries_successful: 5,
        extensions: 2,
        extensions_successful: 2,
        open_failure: false,
    };

    #[allow(clippy::unreadable_literal)]
    #[test]
    fn test_happy_paths() {
        // some real data.
        let conn = do_migrate(HAPPY_PATH_MIGRATION_INFO, |c| {
            c.execute_batch(HAPPY_PATH_SQL).expect("should populate")
        });

        assert_has(
            &conn,
            "{e7fefcf3-b39c-4f17-5215-ebfe120a7031}",
            json!({"userWelcomed": 1570659224457u64, "isWho": "4ec8109f"}),
        );
        assert_has(
            &conn,
            "https-everywhere@eff.org",
            json!({"userRules": [], "ruleActiveStates": {}, "migration_version": 2}),
        );
    }

    #[test]
    fn test_sad_paths() {
        do_migrate(
            MigrationInfo {
                entries: 10,
                entries_successful: 0,
                extensions: 6,
                extensions_successful: 0,
                open_failure: false,
            },
            |c| {
                c.execute_batch(
                    r#"INSERT INTO collection_data(collection_name, record)
                    VALUES
                    ('default/test', '{"key":2,"data":1}'), -- key not a string
                    ('default/test', '{"key":"","data":1}'), -- key empty string
                    ('default/test', '{"xey":"k","data":1}'), -- key missing
                    ('default/test', '{"key":"k","xata":1}'), -- data missing
                    ('default/test', '{"key":"k","data":1'), -- invalid json
                    ('xx/test', '{"key":"k","data":1}'), -- bad key format
                    ('default', '{"key":"k","data":1}'), -- bad key format 2
                    ('default/', '{"key":"k","data":1}'), -- bad key format 3
                    ('defaultx/test', '{"key":"k","data":1}'), -- bad key format 4
                    ('', '') -- empty strings
                    "#,
                )
                .expect("should populate");
            },
        );
    }

    #[test]
    fn test_migration_info_storage() {
        let tmpdir = tempdir().unwrap();
        let path = tmpdir.path().join("source.db");
        init_source_db(&path, |c| {
            c.execute_batch(HAPPY_PATH_SQL).expect("should populate")
        });

        // now migrate
        let db = crate::store::test::new_mem_store();
        db.migrate(&path).expect("migration should work");
        let mi = db
            .take_migration_info()
            .expect("take failed with info present");
        assert_eq!(mi, Some(HAPPY_PATH_MIGRATION_INFO));
        let mi2 = db
            .take_migration_info()
            .expect("take failed with info missing");
        assert_eq!(mi2, None);
    }
}
