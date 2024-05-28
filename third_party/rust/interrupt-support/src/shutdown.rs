/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Shutdown handling for database operations
///
/// This module allows us to enter shutdown mode, causing all `SqlInterruptScope` instances that opt-in to
/// to be permanently interrupted.  This means:
///
///   - All current scopes will be interrupted
///   - Any attempt to create a new scope will be interrupted
///
/// Here's how add shutdown support to a component:
///
///   - Database connections need to be wrapped in a type that:
///      - Implements `AsRef<SqlInterruptHandle>`.
///      - Gets wrapped in an `Arc<>`.  This is needed so the shutdown code can get a weak reference to
///        the instance.
///      - Calls `register_interrupt()` on creation
///   - Use `SqlInterruptScope::begin_interrupt_scope()` before each operation.
///     This will return an error if shutdown mode is in effect.
///     The interrupt scope should be periodically checked to handle the operation being interrupted/shutdown after it started.
///
///  See `PlacesDb::begin_interrupt_scope()` and `PlacesApi::new_connection()` for an example of
///  how this works.
use crate::Interruptee;
use parking_lot::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Weak;

use crate::SqlInterruptHandle;

// Bool that tracks if we're in shutdown mode or not.  We use Ordering::Relaxed to read/write to
// variable.  It's just a flag so we don't need stronger synchronization guarantees.
static IN_SHUTDOWN: AtomicBool = AtomicBool::new(false);

// `SqlInterruptHandle` instances to interrupt when we shutdown
lazy_static::lazy_static! {
   static ref REGISTERED_INTERRUPTS: Mutex<Vec<Weak<dyn AsRef<SqlInterruptHandle> + Send + Sync>>> = Mutex::new(Vec::new());
}

/// Initiate shutdown mode
pub fn shutdown() {
    IN_SHUTDOWN.store(true, Ordering::Relaxed);
    for weak in REGISTERED_INTERRUPTS.lock().iter() {
        if let Some(interrupt) = weak.upgrade() {
            interrupt.as_ref().as_ref().interrupt()
        }
    }
}

/// Check if we're currently in shutdown mode
pub fn in_shutdown() -> bool {
    IN_SHUTDOWN.load(Ordering::Relaxed)
}

/// Register a ShutdownInterrupt implementation
///
/// Call this function to ensure that the `SqlInterruptHandle::interrupt()` method will be called
/// at shutdown.
pub fn register_interrupt(interrupt: Weak<dyn AsRef<SqlInterruptHandle> + Send + Sync>) {
    // Try to find an existing entry that's been dropped to replace.  This keeps the vector growth
    // in check
    let mut interrupts = REGISTERED_INTERRUPTS.lock();
    for weak in interrupts.iter_mut() {
        if weak.strong_count() == 0 {
            *weak = interrupt;
            return;
        }
    }
    // No empty slots, push the new value
    interrupts.push(interrupt);
}

// Implements Interruptee by checking if we've entered shutdown mode
pub struct ShutdownInterruptee;
impl Interruptee for ShutdownInterruptee {
    #[inline]
    fn was_interrupted(&self) -> bool {
        in_shutdown()
    }
}
