/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::open_database::{open_database_with_flags, ConnectionInitializer, Error};
use interrupt_support::{register_interrupt, SqlInterruptHandle, SqlInterruptScope};
use parking_lot::{MappedMutexGuard, Mutex, MutexGuard};
use rusqlite::{Connection, OpenFlags};
use std::{
    path::{Path, PathBuf},
    sync::{Arc, Weak},
};

/// Lazily-loaded database with interruption support
///
/// In addition to the [Self::interrupt] method, LazyDb also calls
/// [interrupt_support::register_interrupt] on any opened database.  This means that if
/// [interrupt_support::shutdown] is called it will interrupt this database if it's open and
/// in-use.
pub struct LazyDb<CI> {
    path: PathBuf,
    open_flags: OpenFlags,
    connection_initializer: CI,
    // Note: if you're going to lock both mutexes at once, make sure to lock the connection mutex
    // first.  Otherwise, you risk creating a deadlock where two threads each hold one of the locks
    // and is waiting for the other.
    connection: Mutex<Option<Connection>>,
    // It's important to use a separate mutex for the interrupt handle, since the whole point is to
    // interrupt while another thread holds the connection mutex.  Since the only mutation is
    // setting/unsetting the Arc, maybe this could be sped up by using something like
    // `arc_swap::ArcSwap`, but that seems like overkill for our purposes. This mutex should rarely
    // be contested and interrupt operations execute quickly.
    interrupt_handle: Mutex<Option<Arc<SqlInterruptHandle>>>,
}

impl<CI: ConnectionInitializer> LazyDb<CI> {
    /// Create a new LazyDb
    ///
    /// This does not open the connection and is non-blocking
    pub fn new(path: &Path, open_flags: OpenFlags, connection_initializer: CI) -> Self {
        Self {
            path: path.to_owned(),
            open_flags,
            connection_initializer,
            connection: Mutex::new(None),
            interrupt_handle: Mutex::new(None),
        }
    }

    /// Lock the database mutex and get a connection and interrupt scope.
    ///
    /// If the connection is closed, it will be opened.
    ///
    /// Calling `lock` again, or calling `close`, from the same thread while the mutex guard is
    /// still alive will cause a deadlock.
    pub fn lock(&self) -> Result<(MappedMutexGuard<'_, Connection>, SqlInterruptScope), Error> {
        // Call get_conn first, then get_scope to ensure we acquire the locks in the correct order
        let conn = self.get_conn()?;
        let scope = self.get_scope(&conn)?;
        Ok((conn, scope))
    }

    fn get_conn(&self) -> Result<MappedMutexGuard<'_, Connection>, Error> {
        let mut guard = self.connection.lock();
        // Open the database if it wasn't opened before.  Do this outside of the MutexGuard::map call to simplify the error handling
        if guard.is_none() {
            *guard = Some(open_database_with_flags(
                &self.path,
                self.open_flags,
                &self.connection_initializer,
            )?);
        };
        // Use MutexGuard::map to get a Connection rather than Option<Connection>.  The unwrap()
        // call can't fail because of the previous code.
        Ok(MutexGuard::map(guard, |conn_option| {
            conn_option.as_mut().unwrap()
        }))
    }

    fn get_scope(&self, conn: &Connection) -> Result<SqlInterruptScope, Error> {
        let mut handle_guard = self.interrupt_handle.lock();
        let result = match handle_guard.as_ref() {
            Some(handle) => handle.begin_interrupt_scope(),
            None => {
                let handle = Arc::new(SqlInterruptHandle::new(conn));
                register_interrupt(
                    Arc::downgrade(&handle) as Weak<dyn AsRef<SqlInterruptHandle> + Send + Sync>
                );
                handle_guard.insert(handle).begin_interrupt_scope()
            }
        };
        // If we see an Interrupted error when beginning the scope, it means that we're in shutdown
        // mode.
        result.map_err(|_| Error::Shutdown)
    }

    /// Close the database if it's open
    ///
    /// Pass interrupt=true to interrupt any in-progress queries before closing the database.
    ///
    /// Do not call `close` if you already have a lock on the database in the current thread, as
    /// this will cause a deadlock.
    pub fn close(&self, interrupt: bool) {
        let mut interrupt_handle = self.interrupt_handle.lock();
        if let Some(handle) = interrupt_handle.as_ref() {
            if interrupt {
                handle.interrupt();
            }
            *interrupt_handle = None;
        }
        // Drop the interrupt handle lock to avoid holding both locks at once.
        drop(interrupt_handle);
        *self.connection.lock() = None;
    }

    /// Interrupt any in-progress queries
    pub fn interrupt(&self) {
        if let Some(handle) = self.interrupt_handle.lock().as_ref() {
            handle.interrupt();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::open_database::test_utils::TestConnectionInitializer;

    fn open_test_db() -> LazyDb<TestConnectionInitializer> {
        LazyDb::new(
            Path::new(":memory:"),
            OpenFlags::default(),
            TestConnectionInitializer::new(),
        )
    }

    #[test]
    fn test_interrupt() {
        let lazy_db = open_test_db();
        let (_, scope) = lazy_db.lock().unwrap();
        assert!(!scope.was_interrupted());
        lazy_db.interrupt();
        assert!(scope.was_interrupted());
    }

    #[test]
    fn interrupt_before_db_is_opened_should_not_fail() {
        let lazy_db = open_test_db();
        lazy_db.interrupt();
    }
}
