/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::{Connection, Transaction};
use sql_support::open_database::{self, ConnectionInitializer};

/// The current database schema version.
///
/// For any changes to the schema [`SQL`], please make sure to:
///
///  1. Bump this version.
///  2. Add a migration from the old version to the new version in
///     [`RelevancyConnectionInitializer::upgrade_from`].
pub const VERSION: u32 = 13;

/// The current database schema.
pub const SQL: &str = "
    CREATE TABLE url_interest(
        url_hash BLOB NOT NULL,
        interest_code INTEGER NOT NULL,
        PRIMARY KEY (url_hash, interest_code)
    ) WITHOUT ROWID;
";

/// Initializes an SQLite connection to the Relevancy database, performing
/// migrations as needed.
pub struct RelevancyConnectionInitializer;

impl ConnectionInitializer for RelevancyConnectionInitializer {
    const NAME: &'static str = "relevancy db";
    const END_VERSION: u32 = VERSION;

    fn prepare(&self, conn: &Connection, _db_empty: bool) -> open_database::Result<()> {
        let initial_pragmas = "
            -- Use in-memory storage for TEMP tables.
            PRAGMA temp_store = 2;
            PRAGMA journal_mode = WAL;
            PRAGMA foreign_keys = ON;
        ";
        conn.execute_batch(initial_pragmas)?;
        Ok(())
    }

    fn init(&self, db: &Transaction<'_>) -> open_database::Result<()> {
        Ok(db.execute_batch(SQL)?)
    }

    fn upgrade_from(&self, _db: &Transaction<'_>, version: u32) -> open_database::Result<()> {
        Err(open_database::Error::IncompatibleVersion(version))
    }
}
