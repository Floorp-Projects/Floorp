/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::{Connection, Transaction};
use sql_support::open_database::{self, ConnectionInitializer};

pub const VERSION: u32 = 8;

pub const SQL: &str = "
    CREATE TABLE meta(
        key TEXT PRIMARY KEY,
        value NOT NULL
    ) WITHOUT ROWID;

    CREATE TABLE keywords(
        keyword TEXT NOT NULL,
        suggestion_id INTEGER NOT NULL REFERENCES suggestions(id) ON DELETE CASCADE,
        rank INTEGER NOT NULL,
        PRIMARY KEY (keyword, suggestion_id)
    ) WITHOUT ROWID;

    CREATE TABLE prefix_keywords(
        keyword_prefix TEXT NOT NULL,
        keyword_suffix TEXT NOT NULL DEFAULT '',
        confidence INTEGER NOT NULL DEFAULT 0,
        rank INTEGER NOT NULL,
        suggestion_id INTEGER NOT NULL REFERENCES suggestions(id),
        PRIMARY KEY (keyword_prefix, keyword_suffix, suggestion_id) 
    ) WITHOUT ROWID;

    CREATE UNIQUE INDEX keywords_suggestion_id_rank ON keywords(suggestion_id, rank);

    CREATE TABLE suggestions(
        id INTEGER PRIMARY KEY,
        record_id TEXT NOT NULL,
        provider INTEGER NOT NULL,
        title TEXT NOT NULL,
        url TEXT NOT NULL
    );

    CREATE TABLE amp_custom_details(
        suggestion_id INTEGER PRIMARY KEY,
        advertiser TEXT NOT NULL,
        block_id INTEGER NOT NULL,
        iab_category TEXT NOT NULL,
        impression_url TEXT NOT NULL,
        click_url TEXT NOT NULL,
        icon_id TEXT NOT NULL,
        FOREIGN KEY(suggestion_id) REFERENCES suggestions(id)
        ON DELETE CASCADE
    );

    CREATE TABLE pocket_custom_details(
        suggestion_id INTEGER PRIMARY KEY REFERENCES suggestions(id) ON DELETE CASCADE,
        score REAL NOT NULL
    );

    CREATE TABLE wikipedia_custom_details(
        suggestion_id INTEGER PRIMARY KEY REFERENCES suggestions(id) ON DELETE CASCADE,
        icon_id TEXT NOT NULL
    );

    CREATE TABLE amo_custom_details(
        suggestion_id INTEGER PRIMARY KEY,
        description TEXT NOT NULL,
        guid TEXT NOT NULL,
        icon_url TEXT NOT NULL,
        rating TEXT,
        number_of_ratings INTEGER NOT NULL,
        score REAL NOT NULL,
        FOREIGN KEY(suggestion_id) REFERENCES suggestions(id)
        ON DELETE CASCADE
    );

    CREATE INDEX suggestions_record_id ON suggestions(record_id);

    CREATE TABLE icons(
        id TEXT PRIMARY KEY,
        data BLOB NOT NULL
    ) WITHOUT ROWID;
";

/// Initializes an SQLite connection to the Suggest database, performing
/// migrations as needed.
pub struct SuggestConnectionInitializer;

impl ConnectionInitializer for SuggestConnectionInitializer {
    const NAME: &'static str = "suggest db";
    const END_VERSION: u32 = VERSION;

    fn prepare(&self, conn: &Connection, _db_empty: bool) -> open_database::Result<()> {
        let initial_pragmas = "
            -- Use in-memory storage for TEMP tables.
            PRAGMA temp_store = 2;

            PRAGMA journal_mode = WAL;
            PRAGMA foreign_keys = ON;
        ";
        conn.execute_batch(initial_pragmas)?;
        sql_support::debug_tools::define_debug_functions(conn)?;

        Ok(())
    }

    fn init(&self, db: &Transaction<'_>) -> open_database::Result<()> {
        Ok(db.execute_batch(SQL)?)
    }

    fn upgrade_from(&self, _db: &Transaction<'_>, version: u32) -> open_database::Result<()> {
        match version {
            1..=7 => {
                // These schema versions were used during development, and never
                // shipped in any applications. Treat these databases as
                // corrupt, so that they'll be replaced.
                Err(open_database::Error::Corrupt)
            }
            _ => Err(open_database::Error::IncompatibleVersion(version)),
        }
    }
}
