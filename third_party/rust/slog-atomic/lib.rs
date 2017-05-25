//! Slog atomic switching drain
//!
//! `AtomicSwitch` allows swapping drain that it wraps atomically, race-free, in
//! runtime. This can be useful eg. for turning on debug logging
//! in production.
//!
//! See [`slog` `signal.rs`
//! example](https://github.com/dpc/slog-rs/blob/master/examples/signal.rs)
#![warn(missing_docs)]

#[macro_use]
extern crate slog;
extern crate crossbeam;

use slog::*;
use std::sync::Arc;
use crossbeam::sync::ArcCell;

/// Handle to `AtomicSwitch` that controls it.
pub struct AtomicSwitchCtrl<E>(Arc<ArcCell<Box<Drain<Error=E>+Send+Sync>>>);

/// Drain wrapping another drain, allowing atomic substitution in runtime
pub struct AtomicSwitch<E>(Arc<ArcCell<Box<Drain<Error=E>+Send+Sync>>>);

impl<E> AtomicSwitch<E> {
    /// Wrap `drain` in `AtomicSwitch` to allow swapping it later
    ///
    /// Use `AtomicSwitch::ctrl()` to get a handle to it
    pub fn new<D: Drain<Error=E> + 'static + Send+Sync>(drain: D) -> Self {
        AtomicSwitch::new_from_arc(Arc::new(ArcCell::new(Arc::new(Box::new(drain) as Box<Drain<Error=E>+Send+Sync>))))
    }

    /// Create new `AtomicSwitch` from an existing `Arc<...>`
    ///
    /// See `AtomicSwitch::new()`
    pub fn new_from_arc(d: Arc<ArcCell<Box<Drain<Error=E>+Send+Sync>>>) -> Self {
        AtomicSwitch(d)
    }

    /// Get a `AtomicSwitchCtrl` handle to control this `AtomicSwitch` drain
    pub fn ctrl(&self) -> AtomicSwitchCtrl<E> {
        AtomicSwitchCtrl(self.0.clone())
    }
}

impl<E> AtomicSwitchCtrl<E> {
    /// Get Arc to the currently wrapped drain
    pub fn get(&self) -> Arc<Box<Drain<Error=E>+Send+Sync>> {
        self.0.get()
    }

    /// Set the current wrapped drain
    pub fn set<D: Drain<Error=E>+Send+Sync>(&self, drain: D) {
        let _ = self.0.set(Arc::new(Box::new(drain)));
    }

    /// Swap the existing drain with a new one
    pub fn swap(&self, drain: Arc<Box<Drain<Error=E>+Send+Sync>>) -> Arc<Box<Drain<Error=E>+Send+Sync>> {
        self.0.set(drain)
    }

    /// Get a `AtomicSwitch` drain controlled by this `AtomicSwitchCtrl`
    pub fn drain(&self) -> AtomicSwitch<E> {
        AtomicSwitch(self.0.clone())
    }
}

impl<E> Drain for AtomicSwitch<E> {
    type Error = E;
    fn log(&self, info: &Record, logger_values: &OwnedKeyValueList) -> std::result::Result<(), E> {
        self.0.get().log(info, logger_values)
    }
}
