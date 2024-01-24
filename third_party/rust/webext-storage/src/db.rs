/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::*;
use crate::schema;
use interrupt_support::{SqlInterruptHandle, SqlInterruptScope};
use parking_lot::Mutex;
use rusqlite::types::{FromSql, ToSql};
use rusqlite::Connection;
use rusqlite::OpenFlags;
use sql_support::open_database::open_database_with_flags;
use sql_support::ConnExt;
use std::ops::{Deref, DerefMut};
use std::path::{Path, PathBuf};
use std::sync::Arc;
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
    interrupt_handle: Arc<SqlInterruptHandle>,
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

        let conn = open_database_with_flags(db_path, flags, &schema::WebExtMigrationLogin)?;
        Ok(Self {
            interrupt_handle: Arc::new(SqlInterruptHandle::new(&conn)),
            writer: conn,
        })
    }

    pub fn interrupt_handle(&self) -> Arc<SqlInterruptHandle> {
        Arc::clone(&self.interrupt_handle)
    }

    #[allow(dead_code)]
    pub fn begin_interrupt_scope(&self) -> Result<SqlInterruptScope> {
        Ok(self.interrupt_handle.begin_interrupt_scope()?)
    }

    /// Closes the database connection. If there are any unfinalized prepared
    /// statements on the connection, `close` will fail and the `StorageDb` will
    /// remain open and the connection will be leaked - we used to return the
    /// underlying connection so the caller can retry but (a) that's very tricky
    /// in an Arc<Mutex<>> world and (b) we never actually took advantage of
    /// that retry capability.
    pub fn close(self) -> Result<()> {
        self.writer.close().map_err(|(writer, err)| {
            // In rusqlite 0.28.0 and earlier, if we just let `writer` drop,
            // the close would panic on failure.
            // Later rusqlite versions will not panic, but this behavior doesn't
            // hurt there.
            std::mem::forget(writer);
            err.into()
        })
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

// We almost exclusively use this ThreadSafeStorageDb
pub struct ThreadSafeStorageDb {
    db: Mutex<StorageDb>,
    // This "outer" interrupt_handle not protected by the mutex means
    // consumers can interrupt us when the mutex is held - which it always will
    // be if we are doing anything interruptable!
    interrupt_handle: Arc<SqlInterruptHandle>,
}

impl ThreadSafeStorageDb {
    pub fn new(db: StorageDb) -> Self {
        Self {
            interrupt_handle: db.interrupt_handle(),
            db: Mutex::new(db),
        }
    }

    pub fn interrupt_handle(&self) -> Arc<SqlInterruptHandle> {
        Arc::clone(&self.interrupt_handle)
    }

    pub fn begin_interrupt_scope(&self) -> Result<SqlInterruptScope> {
        Ok(self.interrupt_handle.begin_interrupt_scope()?)
    }

    pub fn into_inner(self) -> StorageDb {
        self.db.into_inner()
    }
}

// Deref to a Mutex<StorageDb>, which is how we will use ThreadSafeStorageDb most of the time
impl Deref for ThreadSafeStorageDb {
    type Target = Mutex<StorageDb>;

    #[inline]
    fn deref(&self) -> &Mutex<StorageDb> {
        &self.db
    }
}

// Also implement AsRef<SqlInterruptHandle> so that we can interrupt this at shutdown
impl AsRef<SqlInterruptHandle> for ThreadSafeStorageDb {
    fn as_ref(&self) -> &SqlInterruptHandle {
        &self.interrupt_handle
    }
}

pub(crate) mod sql_fns {
    use rusqlite::{functions::Context, Result};
    use sync_guid::Guid as SyncGuid;

    #[inline(never)]
    pub fn generate_guid(_ctx: &Context<'_>) -> Result<SyncGuid> {
        Ok(SyncGuid::random())
    }
}

// These should be somewhere else...
pub fn put_meta(db: &Connection, key: &str, value: &dyn ToSql) -> Result<()> {
    db.conn().execute_cached(
        "REPLACE INTO meta (key, value) VALUES (:key, :value)",
        rusqlite::named_params! { ":key": key, ":value": value },
    )?;
    Ok(())
}

pub fn get_meta<T: FromSql>(db: &Connection, key: &str) -> Result<Option<T>> {
    let res = db.conn().try_query_one(
        "SELECT value FROM meta WHERE key = :key",
        &[(":key", &key)],
        true,
    )?;
    Ok(res)
}

pub fn delete_meta(db: &Connection, key: &str) -> Result<()> {
    db.conn()
        .execute_cached("DELETE FROM meta WHERE key = :key", &[(":key", &key)])?;
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
            Err(Error::IllegalDatabasePath(p.as_ref().to_owned()))
        }
    } else {
        let p = p.as_ref();
        let u = Url::from_file_path(p).map_err(|_| Error::IllegalDatabasePath(p.to_owned()))?;
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
        .ok_or_else(|| Error::IllegalDatabasePath(path.clone()))?;

    let parent = path
        .parent()
        .ok_or_else(|| Error::IllegalDatabasePath(path.clone()))?;

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

    pub fn new_mem_thread_safe_storage_db() -> Arc<ThreadSafeStorageDb> {
        Arc::new(ThreadSafeStorageDb::new(new_mem_db()))
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
