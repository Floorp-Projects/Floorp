/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Use this module to open a new SQLite database connection.
///
/// Usage:
///    - Define a struct that implements ConnectionInitializer.  This handles:
///      - Initializing the schema for a new database
///      - Upgrading the schema for an existing database
///      - Extra preparation/finishing steps, for example setting up SQLite functions
///
///    - Call open_database() in your database constructor:
///      - The first method called is `prepare()`.  This is executed outside of a transaction
///        and is suitable for executing pragmas (eg, `PRAGMA journal_mode=wal`), defining
///        functions, etc.
///      - If the database file is not present and the connection is writable, open_database()
///        will create a new DB and call init(), then finish(). If the connection is not
///        writable it will panic, meaning that if you support ReadOnly connections, they must
///        be created after a writable connection is open.
///      - If the database file exists and the connection is writable, open_database() will open
///        it and call prepare(), upgrade_from() for each upgrade that needs to be applied, then
///        finish(). As above, a read-only connection will panic if upgrades are necessary, so
///        you should ensure the first connection opened is writable.
///      - If the connection is not writable, `finish()` will be called (ie, `finish()`, like
///        `prepare()`, is called for all connections)
///
///  See the autofill DB code for an example.
///
use crate::ConnExt;
use rusqlite::{Connection, OpenFlags, Transaction, NO_PARAMS};
use std::path::Path;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum Error {
    #[error("Incompatible database version: {0}")]
    IncompatibleVersion(u32),
    #[error("Error executing SQL: {0}")]
    SqlError(#[from] rusqlite::Error),
}

pub type Result<T> = std::result::Result<T, Error>;

pub trait ConnectionInitializer {
    // Name to display in the logs
    const NAME: &'static str;

    // The version that the last upgrade function upgrades to.
    const END_VERSION: u32;

    // Functions called only for writable connections all take a Transaction
    // Initialize a newly created database to END_VERSION
    fn init(&self, tx: &Transaction<'_>) -> Result<()>;

    // Upgrade schema from version -> version + 1
    fn upgrade_from(&self, conn: &Transaction<'_>, version: u32) -> Result<()>;

    // Runs immediately after creation for all types of connections. If writable,
    // will *not* be in the transaction created for the "only writable" functions above.
    fn prepare(&self, _conn: &Connection) -> Result<()> {
        Ok(())
    }

    // Runs for all types of connections. If a writable connection is being
    // initialized, this will be called after all initialization functions,
    // but inside their transaction.
    fn finish(&self, _conn: &Connection) -> Result<()> {
        Ok(())
    }
}

pub fn open_database<CI: ConnectionInitializer, P: AsRef<Path>>(
    path: P,
    connection_initializer: &CI,
) -> Result<Connection> {
    open_database_with_flags(path, OpenFlags::default(), connection_initializer)
}

pub fn open_memory_database<CI: ConnectionInitializer>(
    conn_initializer: &CI,
) -> Result<Connection> {
    open_memory_database_with_flags(OpenFlags::default(), conn_initializer)
}

pub fn open_database_with_flags<CI: ConnectionInitializer, P: AsRef<Path>>(
    path: P,
    open_flags: OpenFlags,
    connection_initializer: &CI,
) -> Result<Connection> {
    // Try running the migration logic with an existing file
    log::debug!("{}: opening database", CI::NAME);
    let mut conn = Connection::open_with_flags(path, open_flags)?;
    let run_init = should_init(&conn)?;

    log::debug!("{}: preparing", CI::NAME);
    connection_initializer.prepare(&conn)?;

    if open_flags.contains(OpenFlags::SQLITE_OPEN_READ_WRITE) {
        let tx = conn.transaction()?;
        if run_init {
            log::debug!("{}: initializing new database", CI::NAME);
            connection_initializer.init(&tx)?;
        } else {
            let mut current_version = get_schema_version(&tx)?;
            if current_version > CI::END_VERSION {
                return Err(Error::IncompatibleVersion(current_version));
            }
            while current_version < CI::END_VERSION {
                log::debug!(
                    "{}: upgrading database to {}",
                    CI::NAME,
                    current_version + 1
                );
                connection_initializer.upgrade_from(&tx, current_version)?;
                current_version += 1;
            }
        }
        log::debug!("{}: finishing writable database open", CI::NAME);
        connection_initializer.finish(&tx)?;
        set_schema_version(&tx, CI::END_VERSION)?;
        tx.commit()?;
    } else {
        // There's an implied requirement that the first connection to a DB is
        // writable, so read-only connections do much less, but panic if stuff is wrong
        assert!(!run_init, "existing writer must have initialized");
        assert!(
            get_schema_version(&conn)? == CI::END_VERSION,
            "existing writer must have migrated"
        );
        log::debug!("{}: finishing readonly database open", CI::NAME);
        connection_initializer.finish(&conn)?;
    }
    log::debug!("{}: database open successful", CI::NAME);
    Ok(conn)
}

pub fn open_memory_database_with_flags<CI: ConnectionInitializer>(
    flags: OpenFlags,
    conn_initializer: &CI,
) -> Result<Connection> {
    open_database_with_flags(":memory:", flags, conn_initializer)
}

fn should_init(conn: &Connection) -> Result<bool> {
    Ok(conn.query_one::<u32>("SELECT COUNT(*) FROM sqlite_master")? == 0)
}

fn get_schema_version(conn: &Connection) -> Result<u32> {
    let version = conn.query_row_and_then("PRAGMA user_version", NO_PARAMS, |row| row.get(0))?;
    Ok(version)
}

fn set_schema_version(conn: &Connection, version: u32) -> Result<()> {
    conn.set_pragma("user_version", version)?;
    Ok(())
}

// It would be nice for this to be #[cfg(test)], but that doesn't allow it to be used in tests for
// our other crates.
pub mod test_utils {
    use super::*;
    use std::path::PathBuf;
    use tempfile::TempDir;

    // Database file that we can programatically run upgrades on
    //
    // We purposefully don't keep a connection to the database around to force upgrades to always
    // run against a newly opened DB, like they would in the real world.  See #4106 for
    // details.
    pub struct MigratedDatabaseFile<CI: ConnectionInitializer> {
        // Keep around a TempDir to ensure the database file stays around until this struct is
        // dropped
        _tempdir: TempDir,
        pub connection_initializer: CI,
        pub path: PathBuf,
    }

    impl<CI: ConnectionInitializer> MigratedDatabaseFile<CI> {
        pub fn new(connection_initializer: CI, init_sql: &str) -> Self {
            Self::new_with_flags(connection_initializer, init_sql, OpenFlags::default())
        }

        pub fn new_with_flags(
            connection_initializer: CI,
            init_sql: &str,
            open_flags: OpenFlags,
        ) -> Self {
            let tempdir = tempfile::tempdir().unwrap();
            let path = tempdir.path().join(Path::new("db.sql"));
            let conn = Connection::open_with_flags(&path, open_flags).unwrap();
            conn.execute_batch(init_sql).unwrap();
            Self {
                _tempdir: tempdir,
                connection_initializer,
                path,
            }
        }

        pub fn upgrade_to(&self, version: u32) {
            let mut conn = self.open();
            let tx = conn.transaction().unwrap();
            let mut current_version = get_schema_version(&tx).unwrap();
            while current_version < version {
                self.connection_initializer
                    .upgrade_from(&tx, current_version)
                    .unwrap();
                current_version += 1;
            }
            set_schema_version(&tx, current_version).unwrap();
            self.connection_initializer.finish(&tx).unwrap();
            tx.commit().unwrap();
        }

        pub fn run_all_upgrades(&self) {
            let current_version = get_schema_version(&self.open()).unwrap();
            for version in current_version..CI::END_VERSION {
                self.upgrade_to(version + 1);
            }
        }

        pub fn open(&self) -> Connection {
            Connection::open(&self.path).unwrap()
        }
    }
}

#[cfg(test)]
mod test {
    use super::test_utils::MigratedDatabaseFile;
    use super::*;
    use std::cell::RefCell;

    struct TestConnectionInitializer {
        pub calls: RefCell<Vec<&'static str>>,
        pub buggy_v3_upgrade: bool,
    }

    impl TestConnectionInitializer {
        pub fn new() -> Self {
            let _ = env_logger::try_init();
            Self {
                calls: RefCell::new(Vec::new()),
                buggy_v3_upgrade: false,
            }
        }
        pub fn new_with_buggy_logic() -> Self {
            let _ = env_logger::try_init();
            Self {
                calls: RefCell::new(Vec::new()),
                buggy_v3_upgrade: true,
            }
        }

        pub fn clear_calls(&self) {
            self.calls.borrow_mut().clear();
        }

        pub fn push_call(&self, call: &'static str) {
            self.calls.borrow_mut().push(call);
        }

        pub fn check_calls(&self, expected: Vec<&'static str>) {
            assert_eq!(*self.calls.borrow(), expected);
        }
    }

    impl ConnectionInitializer for TestConnectionInitializer {
        const NAME: &'static str = "test db";
        const END_VERSION: u32 = 4;

        fn prepare(&self, conn: &Connection) -> Result<()> {
            self.push_call("prep");
            conn.execute_batch(
                "
                PRAGMA journal_mode = wal;
                ",
            )?;
            Ok(())
        }

        fn init(&self, conn: &Transaction<'_>) -> Result<()> {
            self.push_call("init");
            conn.execute_batch(
                "
                CREATE TABLE prep_table(col);
                INSERT INTO prep_table(col) VALUES ('correct-value');
                CREATE TABLE my_table(col);
                ",
            )
            .map_err(|e| e.into())
        }

        fn upgrade_from(&self, conn: &Transaction<'_>, version: u32) -> Result<()> {
            match version {
                2 => {
                    self.push_call("upgrade_from_v2");
                    conn.execute_batch(
                        "
                        ALTER TABLE my_old_table_name RENAME TO my_table;
                        ",
                    )?;
                    Ok(())
                }
                3 => {
                    self.push_call("upgrade_from_v3");

                    if self.buggy_v3_upgrade {
                        conn.execute_batch("ILLEGAL_SQL_CODE")?;
                    }

                    conn.execute_batch(
                        "
                        ALTER TABLE my_table RENAME COLUMN old_col to col;
                        ",
                    )?;
                    Ok(())
                }
                _ => {
                    panic!("Unexpected version: {}", version);
                }
            }
        }

        fn finish(&self, conn: &Connection) -> Result<()> {
            self.push_call("finish");
            conn.execute_batch(
                "
                INSERT INTO my_table(col) SELECT col FROM prep_table;
                ",
            )?;
            Ok(())
        }
    }

    // Initialize the database to v2 to test upgrading from there
    static INIT_V2: &str = "
        CREATE TABLE prep_table(col);
        INSERT INTO prep_table(col) VALUES ('correct-value');
        CREATE TABLE my_old_table_name(old_col);
        PRAGMA user_version=2;
    ";

    fn check_final_data(conn: &Connection) {
        let value: String = conn
            .query_row("SELECT col FROM my_table", NO_PARAMS, |r| r.get(0))
            .unwrap();
        assert_eq!(value, "correct-value");
        assert_eq!(get_schema_version(&conn).unwrap(), 4);
    }

    #[test]
    fn test_init() {
        let connection_initializer = TestConnectionInitializer::new();
        let conn = open_memory_database(&connection_initializer).unwrap();
        check_final_data(&conn);
        connection_initializer.check_calls(vec!["prep", "init", "finish"]);
    }

    #[test]
    fn test_upgrades() {
        let db_file = MigratedDatabaseFile::new(TestConnectionInitializer::new(), INIT_V2);
        let conn = open_database(db_file.path.clone(), &db_file.connection_initializer).unwrap();
        check_final_data(&conn);
        db_file.connection_initializer.check_calls(vec![
            "prep",
            "upgrade_from_v2",
            "upgrade_from_v3",
            "finish",
        ]);
    }

    #[test]
    fn test_open_current_version() {
        let db_file = MigratedDatabaseFile::new(TestConnectionInitializer::new(), INIT_V2);
        db_file.upgrade_to(4);
        db_file.connection_initializer.clear_calls();
        let conn = open_database(db_file.path.clone(), &db_file.connection_initializer).unwrap();
        check_final_data(&conn);
        db_file
            .connection_initializer
            .check_calls(vec!["prep", "finish"]);
    }

    #[test]
    fn test_pragmas() {
        let db_file = MigratedDatabaseFile::new(TestConnectionInitializer::new(), INIT_V2);
        let conn = open_database(db_file.path.clone(), &db_file.connection_initializer).unwrap();
        assert_eq!(
            conn.query_one::<String>("PRAGMA journal_mode").unwrap(),
            "wal"
        );
    }

    #[test]
    fn test_migration_error() {
        let db_file =
            MigratedDatabaseFile::new(TestConnectionInitializer::new_with_buggy_logic(), INIT_V2);
        db_file
            .open()
            .execute(
                "INSERT INTO my_old_table_name(old_col) VALUES ('I should not be deleted')",
                NO_PARAMS,
            )
            .unwrap();

        open_database(db_file.path.clone(), &db_file.connection_initializer).unwrap_err();
        // Even though the upgrades failed, the data should still be there.  The changes that
        // upgrade_to_v3 made should have been rolled back.
        assert_eq!(
            db_file
                .open()
                .query_one::<i32>("SELECT COUNT(*) FROM my_old_table_name")
                .unwrap(),
            1
        );
    }

    #[test]
    fn test_version_too_new() {
        let db_file = MigratedDatabaseFile::new(TestConnectionInitializer::new(), INIT_V2);
        set_schema_version(&db_file.open(), 5).unwrap();

        db_file
            .open()
            .execute(
                "INSERT INTO my_old_table_name(old_col) VALUES ('I should not be deleted')",
                NO_PARAMS,
            )
            .unwrap();

        assert!(matches!(
            open_database(db_file.path.clone(), &db_file.connection_initializer,),
            Err(Error::IncompatibleVersion(5))
        ));
        // Make sure that even when DeleteAndRecreate is specified, we don't delete the database
        // file when the schema is newer
        assert_eq!(
            db_file
                .open()
                .query_one::<i32>("SELECT COUNT(*) FROM my_old_table_name")
                .unwrap(),
            1
        );
    }
}
