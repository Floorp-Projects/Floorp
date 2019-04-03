/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A Rust wrapper for mozStorage.
//!
//! mozStorage wraps the SQLite C API with support for XPCOM data structures,
//! asynchronous statement execution, cleanup on shutdown, and connection
//! cloning that propagates attached databases, pragmas, functions, and
//! temporary entities. It also collects timing and memory usage stats for
//! telemetry, and supports detailed statement logging. Additionally, mozStorage
//! makes it possible to use the same connection handle from JS and native
//! (C++ and Rust) code.
//!
//! Most mozStorage objects, like connections, statements, result rows,
//! and variants, are thread-safe. Each connection manages a background
//! thread that can be used to execute statements asynchronously, without
//! blocking the main thread.
//!
//! This crate provides a thin wrapper to make mozStorage easier to use
//! from Rust. It only wraps the synchronous API, so you can either manage
//! the entire connection from a background thread, or use the `moz_task`
//! crate to dispatch tasks to the connection's async thread. Executing
//! synchronous statements on the main thread is not supported, and will
//! assert in debug builds.

#![allow(non_snake_case)]

use std::{error, fmt, ops::Deref, result};

use nserror::{nsresult, NS_ERROR_NO_INTERFACE};
use nsstring::nsCString;
use storage_variant::VariantType;
use xpcom::{
    getter_addrefs,
    interfaces::{
        mozIStorageAsyncConnection, mozIStorageConnection, mozIStorageStatement, nsIEventTarget,
        nsIThread,
    },
    RefPtr, XpCom,
};

const SQLITE_OK: i32 = 0;

pub type Result<T> = result::Result<T, Error>;

/// `Conn` wraps a `mozIStorageConnection`.
#[derive(Clone)]
pub struct Conn {
    handle: RefPtr<mozIStorageConnection>,
}

// This is safe as long as our `mozIStorageConnection` is an instance of
// `mozilla::storage::Connection`, which is atomically reference counted.
unsafe impl Send for Conn {}
unsafe impl Sync for Conn {}

impl Conn {
    /// Wraps a `mozIStorageConnection` in a `Conn`.
    #[inline]
    pub fn wrap(connection: RefPtr<mozIStorageConnection>) -> Conn {
        Conn { handle: connection }
    }

    /// Returns the wrapped `mozIStorageConnection`.
    #[inline]
    pub fn connection(&self) -> &mozIStorageConnection {
        &self.handle
    }

    /// Returns the async thread for this connection. This can be used
    /// with `moz_task` to run synchronous statements on the storage
    /// thread, without blocking the main thread.
    pub fn thread(&self) -> Result<RefPtr<nsIThread>> {
        let target = self.handle.get_interface::<nsIEventTarget>();
        target
            .and_then(|t| t.query_interface::<nsIThread>())
            .ok_or(Error::NoThread)
    }

    /// Prepares a SQL statement. `query` should only contain one SQL statement.
    /// If `query` contains multiple statements, only the first will be prepared,
    /// and the rest will be ignored.
    pub fn prepare<Q: AsRef<str>>(&self, query: Q) -> Result<Statement> {
        let statement = self.call_and_wrap_error(|| {
            getter_addrefs(|p| unsafe {
                self.handle
                    .CreateStatement(&*nsCString::from(query.as_ref()), p)
            })
        })?;
        Ok(Statement {
            conn: self,
            handle: statement,
        })
    }

    /// Executes a SQL statement. `query` may contain one or more
    /// semicolon-separated SQL statements.
    pub fn exec<Q: AsRef<str>>(&self, query: Q) -> Result<()> {
        self.call_and_wrap_error(|| {
            unsafe {
                self.handle
                    .ExecuteSimpleSQL(&*nsCString::from(query.as_ref()))
            }
            .to_result()
        })
    }

    /// Opens a transaction with the default transaction behavior for this
    /// connection. The transaction should be committed when done. Uncommitted
    /// `Transaction`s will automatically roll back when they go out of scope.
    pub fn transaction(&mut self) -> Result<Transaction> {
        let behavior = self.get_default_transaction_behavior();
        Transaction::new(self, behavior)
    }

    /// Opens a transaction with the requested behavior.
    pub fn transaction_with_behavior(
        &mut self,
        behavior: TransactionBehavior,
    ) -> Result<Transaction> {
        Transaction::new(self, behavior)
    }

    fn get_default_transaction_behavior(&self) -> TransactionBehavior {
        let mut typ = 0i32;
        let rv = unsafe { self.handle.GetDefaultTransactionType(&mut typ) };
        if rv.failed() {
            return TransactionBehavior::Deferred;
        }
        match typ as i64 {
            mozIStorageAsyncConnection::TRANSACTION_IMMEDIATE => TransactionBehavior::Immediate,
            mozIStorageAsyncConnection::TRANSACTION_EXCLUSIVE => TransactionBehavior::Exclusive,
            _ => TransactionBehavior::Deferred,
        }
    }

    /// Invokes a storage operation and returns the last SQLite error if the
    /// operation fails. This lets `Conn::{prepare, exec}` and
    /// `Statement::{step, execute}` return more detailed errors, as the
    /// `nsresult` codes that mozStorage uses are often too generic. For
    /// example, `NS_ERROR_FAILURE` might be anything from a SQL syntax error
    /// to an invalid column name in a trigger.
    ///
    /// Note that the last error may not be accurate if the underlying
    /// `mozIStorageConnection` is used concurrently from multiple threads.
    /// Multithreaded callers that share a connection should serialize their
    /// uses.
    fn call_and_wrap_error<T>(
        &self,
        func: impl FnOnce() -> result::Result<T, nsresult>,
    ) -> Result<T> {
        func().or_else(|rv| -> Result<T> {
            let mut code = 0i32;
            unsafe { self.handle.GetLastError(&mut code) }.to_result()?;
            Err(if code != SQLITE_OK {
                let mut message = nsCString::new();
                unsafe { self.handle.GetLastErrorString(&mut *message) }.to_result()?;
                Error::Database(rv, code, String::from_utf8_lossy(&message).into_owned())
            } else {
                rv.into()
            })
        })
    }
}

pub enum TransactionBehavior {
    Deferred,
    Immediate,
    Exclusive,
}

pub struct Transaction<'c> {
    conn: &'c mut Conn,
    active: bool,
}

impl<'c> Transaction<'c> {
    /// Opens a transaction on `conn` with the given `behavior`.
    fn new(conn: &'c mut Conn, behavior: TransactionBehavior) -> Result<Transaction<'c>> {
        conn.exec(match behavior {
            TransactionBehavior::Deferred => "BEGIN DEFERRED",
            TransactionBehavior::Immediate => "BEGIN IMMEDIATE",
            TransactionBehavior::Exclusive => "BEGIN EXCLUSIVE",
        })?;
        Ok(Transaction { conn, active: true })
    }

    /// Commits the transaction.
    pub fn commit(mut self) -> Result<()> {
        if self.active {
            self.conn.exec("COMMIT")?;
            self.active = false;
        }
        Ok(())
    }

    /// Rolls the transaction back.
    pub fn rollback(mut self) -> Result<()> {
        self.abort()
    }

    fn abort(&mut self) -> Result<()> {
        if self.active {
            self.conn.exec("ROLLBACK")?;
            self.active = false;
        }
        Ok(())
    }
}

impl<'c> Deref for Transaction<'c> {
    type Target = Conn;

    fn deref(&self) -> &Conn {
        self.conn
    }
}

impl<'c> Drop for Transaction<'c> {
    fn drop(&mut self) {
        let _ = self.abort();
    }
}

pub struct Statement<'c> {
    conn: &'c Conn,
    handle: RefPtr<mozIStorageStatement>,
}

impl<'c> Statement<'c> {
    /// Binds a parameter at the given `index` to the prepared statement.
    /// `value` is any type that can be converted into a `Variant`.
    pub fn bind_by_index<V: VariantType>(&mut self, index: u32, value: V) -> Result<()> {
        let variant = value.into_variant();
        unsafe { self.handle.BindByIndex(index as u32, variant.coerce()) }
            .to_result()
            .map_err(|rv| Error::BindByIndex(rv, index))
    }

    /// Binds a parameter with the given `name` to the prepared statement.
    pub fn bind_by_name<N: AsRef<str>, V: VariantType>(&mut self, name: N, value: V) -> Result<()> {
        let variant = value.into_variant();
        unsafe {
            self.handle
                .BindByName(&*nsCString::from(name.as_ref()), variant.coerce())
        }
        .to_result()
        .map_err(|rv| Error::BindByName(rv, name.as_ref().into()))
    }

    /// Executes the statement and returns the next row of data.
    pub fn step<'s>(&'s mut self) -> Result<Option<Step<'c, 's>>> {
        let has_more = self.conn.call_and_wrap_error(|| {
            let mut has_more = false;
            unsafe { self.handle.ExecuteStep(&mut has_more) }.to_result()?;
            Ok(has_more)
        })?;
        Ok(if has_more { Some(Step(self)) } else { None })
    }

    /// Executes the statement once, discards any data, and resets the
    /// statement.
    pub fn execute(&mut self) -> Result<()> {
        self.conn
            .call_and_wrap_error(|| unsafe { self.handle.Execute() }.to_result())
    }

    /// Resets the prepared statement so that it's ready to be executed
    /// again, and clears any bound parameters.
    pub fn reset(&mut self) -> Result<()> {
        unsafe { self.handle.Reset() }.to_result()?;
        Ok(())
    }

    fn get_column_index<N: AsRef<str>>(&self, name: N) -> Result<u32> {
        let mut index = 0u32;
        let rv = unsafe {
            self.handle
                .GetColumnIndex(&*nsCString::from(name.as_ref()), &mut index)
        };
        if rv.succeeded() {
            Ok(index)
        } else {
            Err(Error::InvalidColumn(rv, name.as_ref().into()))
        }
    }

    fn get_column_value<T: VariantType>(&self, index: u32) -> Result<T> {
        let variant = getter_addrefs(|p| unsafe { self.handle.GetVariant(index, p) })?;
        let value = T::from_variant(variant.coerce())?;
        Ok(value)
    }
}

impl<'c> Drop for Statement<'c> {
    fn drop(&mut self) {
        unsafe { self.handle.Finalize() };
    }
}

/// A step is the next row in the result set for a statement.
pub struct Step<'c, 's>(&'s mut Statement<'c>);

impl<'c, 's> Step<'c, 's> {
    /// Returns the value of the column at `index` for the current row.
    pub fn get_by_index<T: VariantType>(&self, index: u32) -> Result<T> {
        self.0.get_column_value(index)
    }

    /// A convenience wrapper that returns the default value for the column
    /// at `index` if `NULL`.
    pub fn get_by_index_or_default<T: VariantType + Default>(&self, index: u32) -> T {
        self.get_by_index(index).unwrap_or_default()
    }

    /// Returns the value of the column specified by `name` for the current row.
    pub fn get_by_name<N: AsRef<str>, T: VariantType>(&self, name: N) -> Result<T> {
        let index = self.0.get_column_index(name)?;
        self.0.get_column_value(index)
    }

    /// Returns the default value for the column with the given `name`, or the
    /// default if the column is `NULL`.
    pub fn get_by_name_or_default<N: AsRef<str>, T: VariantType + Default>(&self, name: N) -> T {
        self.get_by_name(name).unwrap_or_default()
    }
}

/// Storage errors.
#[derive(Debug)]
pub enum Error {
    /// A connection doesn't have a usable async thread. The connection might be
    /// closed, or the thread manager may have shut down.
    NoThread,

    /// A database operation failed. The error includes a SQLite result code,
    /// and an explanation string.
    Database(nsresult, i32, String),

    /// A parameter couldn't be bound at this index, likely because the index
    /// is out of range.
    BindByIndex(nsresult, u32),

    /// A parameter couldn't be bound to this name, likely because the statement
    /// doesn't have a matching `:`-prefixed parameter with the name.
    BindByName(nsresult, String),

    /// A column with this name doesn't exist.
    InvalidColumn(nsresult, String),

    /// A storage operation failed for other reasons.
    Other(nsresult),
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        None
    }
}

impl From<nsresult> for Error {
    fn from(rv: nsresult) -> Error {
        Error::Other(rv)
    }
}

impl From<Error> for nsresult {
    fn from(err: Error) -> nsresult {
        match err {
            Error::NoThread => NS_ERROR_NO_INTERFACE,
            Error::Database(rv, _, _)
            | Error::BindByIndex(rv, _)
            | Error::BindByName(rv, _)
            | Error::InvalidColumn(rv, _)
            | Error::Other(rv) => rv,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::NoThread => f.write_str("Async thread unavailable for storage connection"),
            Error::Database(_, code, message) => {
                if message.is_empty() {
                    write!(f, "Database operation failed with code {}", code)
                } else {
                    write!(
                        f,
                        "Database operation failed with code {} ({})",
                        code, message
                    )
                }
            }
            Error::BindByIndex(_, index) => write!(f, "Can't bind parameter at {}", index),
            Error::BindByName(_, name) => write!(f, "Can't bind named parameter {}", name),
            Error::InvalidColumn(_, name) => write!(f, "Column {} doesn't exist", name),
            Error::Other(rv) => write!(f, "Storage operation failed with {}", rv.error_name()),
        }
    }
}
