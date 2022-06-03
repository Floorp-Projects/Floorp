/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{in_shutdown, Interrupted, Interruptee};
use rusqlite::{Connection, InterruptHandle};
use std::fmt;
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    Arc,
};

/// Interrupt operations that use SQL
///
/// Typical usage of this type:
///   - Components typically create a wrapper class around an `rusqlite::Connection`
///     (`PlacesConnection`, `LoginStore`, etc.)
///   - The wrapper stores an `Arc<SqlInterruptHandle>`
///   - The wrapper has a method that clones and returns that `Arc`.  This allows passing the interrupt
///     handle to a different thread in order to interrupt a particular operation.
///   - The wrapper calls `begin_interrupt_scope()` at the start of each operation.  The code that
///     performs the operation periodically calls `err_if_interrupted()`.
///   - Finally, the wrapper class implements `AsRef<SqlInterruptHandle>` and calls
///     `register_interrupt()`.  This causes all operations to be interrupted when we enter
///     shutdown mode.
pub struct SqlInterruptHandle {
    db_handle: InterruptHandle,
    // Counter that we increment on each interrupt() call.
    // We use Ordering::Relaxed to read/write to this variable.  This is safe because we're
    // basically using it as a flag and don't need stronger synchronization guarentees.
    interrupt_counter: Arc<AtomicUsize>,
}

impl SqlInterruptHandle {
    #[inline]
    pub fn new(conn: &Connection) -> Self {
        Self {
            db_handle: conn.get_interrupt_handle(),
            interrupt_counter: Arc::new(AtomicUsize::new(0)),
        }
    }

    /// Begin an interrupt scope that will be interrupted by this handle
    ///
    /// Returns Err(Interrupted) if we're in shutdown mode
    #[inline]
    pub fn begin_interrupt_scope(&self) -> Result<SqlInterruptScope, Interrupted> {
        if in_shutdown() {
            Err(Interrupted)
        } else {
            Ok(SqlInterruptScope::new(Arc::clone(&self.interrupt_counter)))
        }
    }

    /// Interrupt all interrupt scopes created by this handle
    #[inline]
    pub fn interrupt(&self) {
        self.interrupt_counter.fetch_add(1, Ordering::Relaxed);
        self.db_handle.interrupt();
    }
}

impl fmt::Debug for SqlInterruptHandle {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("SqlInterruptHandle")
            .field(
                "interrupt_counter",
                &self.interrupt_counter.load(Ordering::Relaxed),
            )
            .finish()
    }
}

/// Check if an operation has been interrupted
///
/// This is used by the rust code to check if an operation should fail because it was interrupted.
/// It handles the case where we get interrupted outside of an SQL query.
#[derive(Debug)]
pub struct SqlInterruptScope {
    start_value: usize,
    interrupt_counter: Arc<AtomicUsize>,
}

impl SqlInterruptScope {
    fn new(interrupt_counter: Arc<AtomicUsize>) -> Self {
        let start_value = interrupt_counter.load(Ordering::Relaxed);
        Self {
            start_value,
            interrupt_counter,
        }
    }

    // Create an `SqlInterruptScope` that's never interrupted.
    //
    // This should only be used for testing purposes.
    pub fn dummy() -> Self {
        Self::new(Arc::new(AtomicUsize::new(0)))
    }

    /// Check if scope has been interrupted
    #[inline]
    pub fn was_interrupted(&self) -> bool {
        self.interrupt_counter.load(Ordering::Relaxed) != self.start_value
    }

    /// Return Err(Interrupted) if we were interrupted
    #[inline]
    pub fn err_if_interrupted(&self) -> Result<(), Interrupted> {
        if self.was_interrupted() {
            Err(Interrupted)
        } else {
            Ok(())
        }
    }
}

impl Interruptee for SqlInterruptScope {
    #[inline]
    fn was_interrupted(&self) -> bool {
        self.was_interrupted()
    }
}
