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
///     [`SuggestConnectionInitializer::upgrade_from`].
///    a. If suggestions should be re-ingested after the migration, call `clear_database()` inside
///       the migration.
pub const VERSION: u32 = 20;

/// The current Suggest database schema.
pub const SQL: &str = "
CREATE TABLE meta(
    key TEXT PRIMARY KEY,
    value NOT NULL
) WITHOUT ROWID;

CREATE TABLE keywords(
    keyword TEXT NOT NULL,
    suggestion_id INTEGER NOT NULL,
    full_keyword_id INTEGER NULL,
    rank INTEGER NOT NULL,
    PRIMARY KEY (keyword, suggestion_id)
) WITHOUT ROWID;

-- full keywords are what we display to the user when a (partial) keyword matches
CREATE TABLE full_keywords(
    id INTEGER PRIMARY KEY,
    suggestion_id INTEGER NOT NULL,
    full_keyword TEXT NOT NULL
);

CREATE TABLE prefix_keywords(
    keyword_prefix TEXT NOT NULL,
    keyword_suffix TEXT NOT NULL DEFAULT '',
    confidence INTEGER NOT NULL DEFAULT 0,
    rank INTEGER NOT NULL,
    suggestion_id INTEGER NOT NULL,
    PRIMARY KEY (keyword_prefix, keyword_suffix, suggestion_id)
) WITHOUT ROWID;

CREATE UNIQUE INDEX keywords_suggestion_id_rank ON keywords(suggestion_id, rank);

CREATE TABLE suggestions(
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL,
    provider INTEGER NOT NULL,
    title TEXT NOT NULL,
    url TEXT NOT NULL,
    score REAL NOT NULL
);

CREATE TABLE amp_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    advertiser TEXT NOT NULL,
    block_id INTEGER NOT NULL,
    iab_category TEXT NOT NULL,
    impression_url TEXT NOT NULL,
    click_url TEXT NOT NULL,
    icon_id TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
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
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE INDEX suggestions_record_id ON suggestions(record_id);

CREATE TABLE icons(
    id TEXT PRIMARY KEY,
    data BLOB NOT NULL,
    mimetype TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_subjects(
    keyword TEXT PRIMARY KEY,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_modifiers(
    type INTEGER NOT NULL,
    keyword TEXT NOT NULL,
    record_id TEXT NOT NULL,
    PRIMARY KEY (type, keyword)
) WITHOUT ROWID;

CREATE TABLE yelp_location_signs(
    keyword TEXT PRIMARY KEY,
    need_location INTEGER NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_custom_details(
    icon_id TEXT PRIMARY KEY,
    score REAL NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE mdn_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    description TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE TABLE dismissed_suggestions (
    url TEXT PRIMARY KEY
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

    fn upgrade_from(&self, tx: &Transaction<'_>, version: u32) -> open_database::Result<()> {
        match version {
            1..=15 => {
                // Treat databases with these older schema versions as corrupt,
                // so that they'll be replaced by a fresh, empty database with
                // the current schema.
                Err(open_database::Error::Corrupt)
            }
            16 => {
                tx.execute(
                    "
CREATE TABLE dismissed_suggestions (
    url_hash INTEGER PRIMARY KEY
) WITHOUT ROWID;",
                    (),
                )?;
                Ok(())
            }
            17 => {
                tx.execute(
                    "
DROP TABLE dismissed_suggestions;
CREATE TABLE dismissed_suggestions (
    url TEXT PRIMARY KEY
) WITHOUT ROWID;",
                    (),
                )?;
                Ok(())
            }
            18 => {
                tx.execute_batch(
                    "
CREATE TABLE IF NOT EXISTS dismissed_suggestions (
    url TEXT PRIMARY KEY
) WITHOUT ROWID;",
                )?;
                Ok(())
            }
            19 => {
                // Clear the database since we're going to be dropping the keywords table and
                // re-creating it
                clear_database(tx)?;
                tx.execute_batch(
                    "
-- Recreate the various keywords table to drop the foreign keys.
DROP TABLE keywords;
DROP TABLE full_keywords;
DROP TABLE prefix_keywords;
CREATE TABLE keywords(
    keyword TEXT NOT NULL,
    suggestion_id INTEGER NOT NULL,
    full_keyword_id INTEGER NULL,
    rank INTEGER NOT NULL,
    PRIMARY KEY (keyword, suggestion_id)
) WITHOUT ROWID;
CREATE TABLE full_keywords(
    id INTEGER PRIMARY KEY,
    suggestion_id INTEGER NOT NULL,
    full_keyword TEXT NOT NULL
);
CREATE TABLE prefix_keywords(
    keyword_prefix TEXT NOT NULL,
    keyword_suffix TEXT NOT NULL DEFAULT '',
    confidence INTEGER NOT NULL DEFAULT 0,
    rank INTEGER NOT NULL,
    suggestion_id INTEGER NOT NULL,
    PRIMARY KEY (keyword_prefix, keyword_suffix, suggestion_id)
) WITHOUT ROWID;
CREATE UNIQUE INDEX keywords_suggestion_id_rank ON keywords(suggestion_id, rank);
                    ",
                )?;
                Ok(())
            }
            _ => Err(open_database::Error::IncompatibleVersion(version)),
        }
    }
}

/// Clears the database, removing all suggestions, icons, and metadata.
pub fn clear_database(db: &Connection) -> rusqlite::Result<()> {
    db.execute_batch(
        "
        DELETE FROM meta;
        DELETE FROM keywords;
        DELETE FROM full_keywords;
        DELETE FROM prefix_keywords;
        DELETE FROM suggestions;
        DELETE FROM icons;
        DELETE FROM yelp_subjects;
        DELETE FROM yelp_modifiers;
        DELETE FROM yelp_location_signs;
        DELETE FROM yelp_custom_details;
        ",
    )
}

#[cfg(test)]
mod test {
    use super::*;
    use sql_support::open_database::test_utils::MigratedDatabaseFile;

    // Snapshot of the v16 schema.  We use this to test that we can migrate from there to the
    // current schema.
    const V16_SCHEMA: &str = r#"
CREATE TABLE meta(
    key TEXT PRIMARY KEY,
    value NOT NULL
) WITHOUT ROWID;

CREATE TABLE keywords(
    keyword TEXT NOT NULL,
    suggestion_id INTEGER NOT NULL REFERENCES suggestions(id) ON DELETE CASCADE,
    full_keyword_id INTEGER NULL REFERENCES full_keywords(id) ON DELETE SET NULL,
    rank INTEGER NOT NULL,
    PRIMARY KEY (keyword, suggestion_id)
) WITHOUT ROWID;

-- full keywords are what we display to the user when a (partial) keyword matches
-- The FK to suggestion_id makes it so full keywords get deleted when the parent suggestion is deleted.
CREATE TABLE full_keywords(
    id INTEGER PRIMARY KEY,
    suggestion_id INTEGER NOT NULL REFERENCES suggestions(id) ON DELETE CASCADE,
    full_keyword TEXT NOT NULL
);

CREATE TABLE prefix_keywords(
    keyword_prefix TEXT NOT NULL,
    keyword_suffix TEXT NOT NULL DEFAULT '',
    confidence INTEGER NOT NULL DEFAULT 0,
    rank INTEGER NOT NULL,
    suggestion_id INTEGER NOT NULL REFERENCES suggestions(id) ON DELETE CASCADE,
    PRIMARY KEY (keyword_prefix, keyword_suffix, suggestion_id)
) WITHOUT ROWID;

CREATE UNIQUE INDEX keywords_suggestion_id_rank ON keywords(suggestion_id, rank);

CREATE TABLE suggestions(
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL,
    provider INTEGER NOT NULL,
    title TEXT NOT NULL,
    url TEXT NOT NULL,
    score REAL NOT NULL
);

CREATE TABLE amp_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    advertiser TEXT NOT NULL,
    block_id INTEGER NOT NULL,
    iab_category TEXT NOT NULL,
    impression_url TEXT NOT NULL,
    click_url TEXT NOT NULL,
    icon_id TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
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
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE INDEX suggestions_record_id ON suggestions(record_id);

CREATE TABLE icons(
    id TEXT PRIMARY KEY,
    data BLOB NOT NULL,
    mimetype TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_subjects(
    keyword TEXT PRIMARY KEY,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_modifiers(
    type INTEGER NOT NULL,
    keyword TEXT NOT NULL,
    record_id TEXT NOT NULL,
    PRIMARY KEY (type, keyword)
) WITHOUT ROWID;

CREATE TABLE yelp_location_signs(
    keyword TEXT PRIMARY KEY,
    need_location INTEGER NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_custom_details(
    icon_id TEXT PRIMARY KEY,
    score REAL NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE mdn_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    description TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

PRAGMA user_version=16;
"#;

    /// Test running all schema upgrades from V16, which was the first schema with a "real"
    /// migration.
    ///
    /// If an upgrade fails, then this test will fail with a panic.
    #[test]
    fn test_all_upgrades() {
        let db_file = MigratedDatabaseFile::new(SuggestConnectionInitializer, V16_SCHEMA);
        db_file.run_all_upgrades();
        db_file.assert_schema_matches_new_database();
    }
}
