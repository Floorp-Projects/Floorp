/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::*;
use crate::schema;
use rusqlite::types::{FromSql, ToSql};
use rusqlite::Connection;
use rusqlite::OpenFlags;
use sql_support::ConnExt;
use std::fs;
use std::ops::{Deref, DerefMut};
use std::path::{Path, PathBuf};
use std::result;
use url::Url;

/// A `StorageDb` wraps a read-write SQLite connection, and handles schema
/// migrations and recovering from database file corruption. It can be used
/// anywhere a `rusqlite::Connection` is expected, thanks to its `Deref{Mut}`
/// implementations.
///
/// We only support a single writer connection - so that's the only thing we
/// store. It's still a bit overkill, but there's only so many yaks in a day.
pub struct StorageDb {
    writer: Connection,
}
impl StorageDb {
    /// Create a new, or fetch an already open, StorageDb backed by a file on disk.
    pub fn new(db_path: impl AsRef<Path>) -> Result<Self> {
        let db_path = normalize_path(db_path)?;
        Self::new_named(db_path)
    }

    /// Create a new, or fetch an already open, memory-based StorageDb. You must
    /// provide a name, but you are still able to have a single writer and many
    /// reader connections to the same memory DB open.
    #[cfg(test)]
    pub fn new_memory(db_path: &str) -> Result<Self> {
        let name = PathBuf::from(format!("file:{}?mode=memory&cache=shared", db_path));
        Self::new_named(name)
    }

    fn new_named(db_path: PathBuf) -> Result<Self> {
        // We always create the read-write connection for an initial open so
        // we can create the schema and/or do version upgrades.
        let flags = OpenFlags::SQLITE_OPEN_NO_MUTEX
            | OpenFlags::SQLITE_OPEN_URI
            | OpenFlags::SQLITE_OPEN_CREATE
            | OpenFlags::SQLITE_OPEN_READ_WRITE;

        let conn = Connection::open_with_flags(db_path.clone(), flags)?;
        match init_sql_connection(&conn, true) {
            Ok(()) => Ok(Self { writer: conn }),
            Err(e) => {
                // like with places, failure to upgrade means "you lose your data"
                if let ErrorKind::DatabaseUpgradeError = e.kind() {
                    fs::remove_file(&db_path)?;
                    Self::new_named(db_path)
                } else {
                    Err(e)
                }
            }
        }
    }

    /// Closes the database connection. If there are any unfinalized prepared
    /// statements on the connection, `close` will fail and the `StorageDb` will
    /// be returned to the caller so that it can retry, drop (via `mem::drop`)
    // or leak (`mem::forget`) the connection.
    ///
    /// Keep in mind that dropping the connection tries to close it again, and
    /// panics on error.
    pub fn close(self) -> result::Result<(), (StorageDb, Error)> {
        self.writer
            .close()
            .map_err(|(writer, err)| (StorageDb { writer }, err.into()))
    }
}

impl Deref for StorageDb {
    type Target = Connection;

    fn deref(&self) -> &Self::Target {
        &self.writer
    }
}

impl DerefMut for StorageDb {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.writer
    }
}

fn init_sql_connection(conn: &Connection, is_writable: bool) -> Result<()> {
    let initial_pragmas = "
        -- We don't care about temp tables being persisted to disk.
        PRAGMA temp_store = 2;
        -- we unconditionally want write-ahead-logging mode
        PRAGMA journal_mode=WAL;
        -- foreign keys seem worth enforcing!
        PRAGMA foreign_keys = ON;
    ";

    conn.execute_batch(initial_pragmas)?;
    define_functions(&conn)?;
    conn.set_prepared_statement_cache_capacity(128);
    if is_writable {
        let tx = conn.unchecked_transaction()?;
        schema::init(&conn)?;
        tx.commit()?;
    };
    Ok(())
}

fn define_functions(_c: &Connection) -> Result<()> {
    Ok(())
}

// These should be somewhere else...
#[allow(dead_code)]
pub fn put_meta(db: &Connection, key: &str, value: &dyn ToSql) -> Result<()> {
    db.conn().execute_named_cached(
        "REPLACE INTO meta (key, value) VALUES (:key, :value)",
        &[(":key", &key), (":value", value)],
    )?;
    Ok(())
}

#[allow(dead_code)]
pub fn get_meta<T: FromSql>(db: &Connection, key: &str) -> Result<Option<T>> {
    let res = db.conn().try_query_one(
        "SELECT value FROM meta WHERE key = :key",
        &[(":key", &key)],
        true,
    )?;
    Ok(res)
}

#[allow(dead_code)]
pub fn delete_meta(db: &Connection, key: &str) -> Result<()> {
    db.conn()
        .execute_named_cached("DELETE FROM meta WHERE key = :key", &[(":key", &key)])?;
    Ok(())
}

// Utilities for working with paths.
// (From places_utils - ideally these would be shared, but the use of
// ErrorKind values makes that non-trivial.

/// `Path` is basically just a `str` with no validation, and so in practice it
/// could contain a file URL. Rusqlite takes advantage of this a bit, and says
/// `AsRef<Path>` but really means "anything sqlite can take as an argument".
///
/// Swift loves using file urls (the only support it has for file manipulation
/// is through file urls), so it's handy to support them if possible.
fn unurl_path(p: impl AsRef<Path>) -> PathBuf {
    p.as_ref()
        .to_str()
        .and_then(|s| Url::parse(s).ok())
        .and_then(|u| {
            if u.scheme() == "file" {
                u.to_file_path().ok()
            } else {
                None
            }
        })
        .unwrap_or_else(|| p.as_ref().to_owned())
}

/// If `p` is a file URL, return it, otherwise try and make it one.
///
/// Errors if `p` is a relative non-url path, or if it's a URL path
/// that's isn't a `file:` URL.
#[allow(dead_code)]
pub fn ensure_url_path(p: impl AsRef<Path>) -> Result<Url> {
    if let Some(u) = p.as_ref().to_str().and_then(|s| Url::parse(s).ok()) {
        if u.scheme() == "file" {
            Ok(u)
        } else {
            Err(ErrorKind::IllegalDatabasePath(p.as_ref().to_owned()).into())
        }
    } else {
        let p = p.as_ref();
        let u = Url::from_file_path(p).map_err(|_| ErrorKind::IllegalDatabasePath(p.to_owned()))?;
        Ok(u)
    }
}

/// As best as possible, convert `p` into an absolute path, resolving
/// all symlinks along the way.
///
/// If `p` is a file url, it's converted to a path before this.
fn normalize_path(p: impl AsRef<Path>) -> Result<PathBuf> {
    let path = unurl_path(p);
    if let Ok(canonical) = path.canonicalize() {
        return Ok(canonical);
    }
    // It probably doesn't exist yet. This is an error, although it seems to
    // work on some systems.
    //
    // We resolve this by trying to canonicalize the parent directory, and
    // appending the requested file name onto that. If we can't canonicalize
    // the parent, we return an error.
    //
    // Also, we return errors if the path ends in "..", if there is no
    // parent directory, etc.
    let file_name = path
        .file_name()
        .ok_or_else(|| ErrorKind::IllegalDatabasePath(path.clone()))?;

    let parent = path
        .parent()
        .ok_or_else(|| ErrorKind::IllegalDatabasePath(path.clone()))?;

    let mut canonical = parent.canonicalize()?;
    canonical.push(file_name);
    Ok(canonical)
}

// Helpers for tests
#[cfg(test)]
pub mod test {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    // A helper for our tests to get their own memory Api.
    static ATOMIC_COUNTER: AtomicUsize = AtomicUsize::new(0);

    pub fn new_mem_db() -> StorageDb {
        let _ = env_logger::try_init();
        let counter = ATOMIC_COUNTER.fetch_add(1, Ordering::Relaxed);
        StorageDb::new_memory(&format!("test-api-{}", counter)).expect("should get an API")
    }
}

#[cfg(test)]
mod tests {
    use super::test::*;
    use super::*;

    // Sanity check that we can create a database.
    #[test]
    fn test_open() {
        new_mem_db();
        // XXX - should we check anything else? Seems a bit pointless, but if
        // we move the meta functions away from here then it's better than
        // nothing.
    }

    #[test]
    fn test_meta() -> Result<()> {
        let writer = new_mem_db();
        assert_eq!(get_meta::<String>(&writer, "foo")?, None);
        put_meta(&writer, "foo", &"bar".to_string())?;
        assert_eq!(get_meta(&writer, "foo")?, Some("bar".to_string()));
        delete_meta(&writer, "foo")?;
        assert_eq!(get_meta::<String>(&writer, "foo")?, None);
        Ok(())
    }
}
