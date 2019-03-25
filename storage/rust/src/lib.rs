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

use std::{ops::Deref, result};

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

pub type Result<T> = result::Result<T, nsresult>;

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
            .ok_or(NS_ERROR_NO_INTERFACE)
    }

    /// Prepares a SQL statement. `query` should only contain one SQL statement.
    /// If `query` contains multiple statements, only the first will be prepared,
    /// and the rest will be ignored.
    pub fn prepare<Q: AsRef<str>>(&self, query: Q) -> Result<Statement> {
        let statement = getter_addrefs(|p| unsafe {
            self.handle
                .CreateStatement(&*nsCString::from(query.as_ref()), p)
        })?;
        Ok(Statement { handle: statement })
    }

    /// Executes a SQL statement. `query` may contain one or more
    /// semicolon-separated SQL statements.
    pub fn exec<Q: AsRef<str>>(&self, query: Q) -> Result<()> {
        unsafe {
            self.handle
                .ExecuteSimpleSQL(&*nsCString::from(query.as_ref()))
        }
        .to_result()
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

pub struct Statement {
    handle: RefPtr<mozIStorageStatement>,
}

impl Statement {
    /// Binds a parameter at the given `index` to the prepared statement.
    /// `value` is any type that can be converted into a `Variant`.
    pub fn bind_by_index<V: VariantType>(&mut self, index: u32, value: V) -> Result<()> {
        let variant = value.into_variant();
        unsafe { self.handle.BindByIndex(index as u32, variant.coerce()) }.to_result()
    }

    /// Binds a parameter with the given `name` to the prepared statement.
    pub fn bind_by_name<N: AsRef<str>, V: VariantType>(&mut self, name: N, value: V) -> Result<()> {
        let variant = value.into_variant();
        unsafe {
            self.handle
                .BindByName(&*nsCString::from(name.as_ref()), variant.coerce())
        }
        .to_result()
    }

    /// Executes the statement and returns the next row of data.
    pub fn step<'a>(&'a mut self) -> Result<Option<Step<'a>>> {
        let mut has_more = false;
        unsafe { self.handle.ExecuteStep(&mut has_more) }.to_result()?;
        Ok(if has_more { Some(Step(self)) } else { None })
    }

    /// Executes the statement once, discards any data, and resets the
    /// statement.
    pub fn execute(&mut self) -> Result<()> {
        unsafe { self.handle.Execute() }.to_result()
    }

    /// Resets the prepared statement so that it's ready to be executed
    /// again, and clears any bound parameters.
    pub fn reset(&mut self) -> Result<()> {
        unsafe { self.handle.Reset() }.to_result()
    }

    fn get_column_index<N: AsRef<str>>(&self, name: N) -> Result<u32> {
        let mut index = 0u32;
        unsafe {
            self.handle
                .GetColumnIndex(&*nsCString::from(name.as_ref()), &mut index)
        }
        .to_result()
        .map(|_| index)
    }

    fn get_variant<T: VariantType>(&self, index: u32) -> Result<T> {
        let variant = getter_addrefs(|p| unsafe { self.handle.GetVariant(index, p) })?;
        T::from_variant(variant.coerce())
    }
}

impl Drop for Statement {
    fn drop(&mut self) {
        unsafe { self.handle.Finalize() };
    }
}

/// A step is the next row in the result set for a statement.
pub struct Step<'a>(&'a mut Statement);

impl<'a> Step<'a> {
    /// Returns the value of the column at `index` for the current row.
    pub fn get_by_index<'s, T: VariantType>(&'s self, index: u32) -> Result<T> {
        self.0.get_variant(index)
    }

    /// A convenience wrapper that returns the default value for the column
    /// at `index` if `NULL`.
    pub fn get_by_index_or_default<'s, T: VariantType + Default>(&'s self, index: u32) -> T {
        self.get_by_index(index).unwrap_or_default()
    }

    /// Returns the value of the column specified by `name` for the current row.
    pub fn get_by_name<'s, N: AsRef<str>, T: VariantType>(&'s self, name: N) -> Result<T> {
        let index = self.0.get_column_index(name)?;
        self.0.get_variant(index)
    }

    /// Returns the default value for the column with the given `name`, or the
    /// default if the column is `NULL`.
    pub fn get_by_name_or_default<'s, N: AsRef<str>, T: VariantType + Default>(
        &'s self,
        name: N,
    ) -> T {
        self.get_by_name(name).unwrap_or_default()
    }
}
