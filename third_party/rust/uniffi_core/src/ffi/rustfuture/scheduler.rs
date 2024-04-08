/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::mem;

use super::{RustFutureContinuationCallback, RustFuturePoll};

/// Schedules a [crate::RustFuture] by managing the continuation data
///
/// This struct manages the continuation callback and data that comes from the foreign side.  It
/// is responsible for calling the continuation callback when the future is ready to be woken up.
///
/// The basic guarantees are:
///
/// * Each callback will be invoked exactly once, with its associated data.
/// * If `wake()` is called, the callback will be invoked to wake up the future -- either
///   immediately or the next time we get a callback.
/// * If `cancel()` is called, the same will happen and the schedule will stay in the cancelled
///   state, invoking any future callbacks as soon as they're stored.

#[derive(Debug)]
pub(super) enum Scheduler {
    /// No continuations set, neither wake() nor cancel() called.
    Empty,
    /// `wake()` was called when there was no continuation set.  The next time `store` is called,
    /// the continuation should be immediately invoked with `RustFuturePoll::MaybeReady`
    Waked,
    /// The future has been cancelled, any future `store` calls should immediately result in the
    /// continuation being called with `RustFuturePoll::Ready`.
    Cancelled,
    /// Continuation set, the next time `wake()`  is called is called, we should invoke it.
    Set(RustFutureContinuationCallback, u64),
}

impl Scheduler {
    pub(super) fn new() -> Self {
        Self::Empty
    }

    /// Store new continuation data if we are in the `Empty` state.  If we are in the `Waked` or
    /// `Cancelled` state, call the continuation immediately with the data.
    pub(super) fn store(&mut self, callback: RustFutureContinuationCallback, data: u64) {
        match self {
            Self::Empty => *self = Self::Set(callback, data),
            Self::Set(old_callback, old_data) => {
                log::error!(
                    "store: observed `Self::Set` state.  Is poll() being called from multiple threads at once?"
                );
                old_callback(*old_data, RustFuturePoll::Ready);
                *self = Self::Set(callback, data);
            }
            Self::Waked => {
                *self = Self::Empty;
                callback(data, RustFuturePoll::MaybeReady);
            }
            Self::Cancelled => {
                callback(data, RustFuturePoll::Ready);
            }
        }
    }

    pub(super) fn wake(&mut self) {
        match self {
            // If we had a continuation set, then call it and transition to the `Empty` state.
            Self::Set(callback, old_data) => {
                let old_data = *old_data;
                let callback = *callback;
                *self = Self::Empty;
                callback(old_data, RustFuturePoll::MaybeReady);
            }
            // If we were in the `Empty` state, then transition to `Waked`.  The next time `store`
            // is called, we will immediately call the continuation.
            Self::Empty => *self = Self::Waked,
            // This is a no-op if we were in the `Cancelled` or `Waked` state.
            _ => (),
        }
    }

    pub(super) fn cancel(&mut self) {
        if let Self::Set(callback, old_data) = mem::replace(self, Self::Cancelled) {
            callback(old_data, RustFuturePoll::Ready);
        }
    }

    pub(super) fn is_cancelled(&self) -> bool {
        matches!(self, Self::Cancelled)
    }
}

// The `*const ()` data pointer references an object on the foreign side.
// This object must be `Sync` in Rust terminology -- it must be safe for us to pass the pointer to the continuation callback from any thread.
// If the foreign side upholds their side of the contract, then `Scheduler` is Send + Sync.

unsafe impl Send for Scheduler {}
unsafe impl Sync for Scheduler {}
