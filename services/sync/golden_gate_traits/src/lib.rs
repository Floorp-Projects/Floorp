/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! These types should eventually move to the `sync15-traits` crate in
//! Application Services. They're defined in a separate crate in m-c now so
//! that Golden Gate doesn't rely on their internals.

use std::{error::Error, fmt, sync::Mutex, sync::MutexGuard, sync::PoisonError};

/// A bridged Sync engine implements all the methods needed to support
/// Desktop Sync.
pub trait BridgedEngine {
    /// The type returned for errors.
    type Error;

    /// Initializes the engine. This is called once, when the engine is first
    /// created, and guaranteed to be called before any of the other methods.
    /// The default implementation does nothing.
    fn initialize(&self) -> Result<(), Self::Error> {
        Ok(())
    }

    /// Returns the last sync time, in milliseconds, for this engine's
    /// collection. This is called before each sync, to determine the lower
    /// bound for new records to fetch from the server.
    fn last_sync(&self) -> Result<i64, Self::Error>;

    /// Sets the last sync time, in milliseconds. This is called throughout
    /// the sync, to fast-forward the stored last sync time to match the
    /// timestamp on the uploaded records.
    fn set_last_sync(&self, last_sync_millis: i64) -> Result<(), Self::Error>;

    /// Returns the sync ID for this engine's collection. This is only used in
    /// tests.
    fn sync_id(&self) -> Result<Option<String>, Self::Error>;

    /// Resets the sync ID for this engine's collection, returning the new ID.
    /// As a side effect, implementations should reset all local Sync state,
    /// as in `reset`.
    fn reset_sync_id(&self) -> Result<String, Self::Error>;

    /// Ensures that the locally stored sync ID for this engine's collection
    /// matches the `new_sync_id` from the server. If the two don't match,
    /// implementations should reset all local Sync state, as in `reset`.
    /// This method returns the assigned sync ID, which can be either the
    /// `new_sync_id`, or a different one if the engine wants to force other
    /// devices to reset their Sync state for this collection the next time they
    /// sync.
    fn ensure_current_sync_id(&self, new_sync_id: &str) -> Result<String, Self::Error>;

    /// Stages a batch of incoming Sync records. This is called multiple
    /// times per sync, once for each batch. Implementations can use the
    /// signal to check if the operation was aborted, and cancel any
    /// pending work.
    fn store_incoming(
        &self,
        incoming_cleartexts: &[String],
        signal: &dyn Interruptee,
    ) -> Result<(), Self::Error>;

    /// Applies all staged records, reconciling changes on both sides and
    /// resolving conflicts. Returns a list of records to upload.
    fn apply(&self, signal: &dyn Interruptee) -> Result<Vec<String>, Self::Error>;

    /// Indicates that the given record IDs were uploaded successfully to the
    /// server. This is called multiple times per sync, once for each batch
    /// upload.
    fn set_uploaded(
        &self,
        server_modified_millis: i64,
        ids: &[String],
        signal: &dyn Interruptee,
    ) -> Result<(), Self::Error>;

    /// Indicates that all records have been uploaded. At this point, any record
    /// IDs marked for upload that haven't been passed to `set_uploaded`, can be
    /// assumed to have failed: for example, because the server rejected a record
    /// with an invalid TTL or sort index.
    fn sync_finished(&self, signal: &dyn Interruptee) -> Result<(), Self::Error>;

    /// Resets all local Sync state, including any change flags, mirrors, and
    /// the last sync time, such that the next sync is treated as a first sync
    /// with all new local data. Does not erase any local user data.
    fn reset(&self) -> Result<(), Self::Error>;

    /// Erases all local user data for this collection, and any Sync metadata.
    /// This method is destructive, and unused for most collections.
    fn wipe(&self) -> Result<(), Self::Error>;

    /// Tears down the engine. The opposite of `initialize`, `finalize` is
    /// called when an engine is disabled, or otherwise no longer needed. The
    /// default implementation does nothing.
    fn finalize(&self) -> Result<(), Self::Error> {
        Ok(())
    }
}

/// An interruptee is an abort signal used to interrupt a running task.
/// Implementations can store an interrupted flag, usually as an atomic
/// integer or Boolean, set the flag on abort, and have
/// `Interruptee::was_interrupted` return the flag's value.
///
/// Although it's not required, in practice, an `Interruptee` should be
/// `Send + Sync`, so that a task running on a background task queue can be
/// interrupted from the main thread.
pub trait Interruptee {
    /// Indicates if the caller signaled to interrupt.
    fn was_interrupted(&self) -> bool;

    /// Returns an error if the caller signaled to abort. This helper makes it
    /// easier to use the signal with the `?` operator.
    fn err_if_interrupted(&self) -> Result<(), Interrupted> {
        if self.was_interrupted() {
            Err(Interrupted)
        } else {
            Ok(())
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct Interrupted;

impl Error for Interrupted {}

impl fmt::Display for Interrupted {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        "The operation was interrupted".fmt(f)
    }
}

/// A blanket implementation of `BridgedEngine` for any `Mutex<BridgedEngine>`.
/// This is provided for convenience, since we expect most bridges to hold
/// their engines in an `Arc<Mutex<impl BridgedEngine>>`.
impl<E> BridgedEngine for Mutex<E>
where
    E: BridgedEngine,
    E::Error: for<'a> From<PoisonError<MutexGuard<'a, E>>>,
{
    type Error = E::Error;

    fn initialize(&self) -> Result<(), Self::Error> {
        self.lock()?.initialize()
    }

    fn last_sync(&self) -> Result<i64, Self::Error> {
        self.lock()?.last_sync()
    }

    fn set_last_sync(&self, millis: i64) -> Result<(), Self::Error> {
        self.lock()?.set_last_sync(millis)
    }

    fn store_incoming(
        &self,
        incoming_cleartexts: &[String],
        signal: &dyn Interruptee,
    ) -> Result<(), Self::Error> {
        self.lock()?.store_incoming(incoming_cleartexts, signal)
    }

    fn apply(&self, signal: &dyn Interruptee) -> Result<Vec<String>, Self::Error> {
        self.lock()?.apply(signal)
    }

    fn set_uploaded(
        &self,
        server_modified_millis: i64,
        ids: &[String],
        signal: &dyn Interruptee,
    ) -> Result<(), Self::Error> {
        self.lock()?
            .set_uploaded(server_modified_millis, ids, signal)
    }

    fn sync_finished(&self, signal: &dyn Interruptee) -> Result<(), Self::Error> {
        self.lock()?.sync_finished(signal)
    }

    fn reset(&self) -> Result<(), Self::Error> {
        self.lock()?.reset()
    }

    fn wipe(&self) -> Result<(), Self::Error> {
        self.lock()?.wipe()
    }

    fn finalize(&self) -> Result<(), Self::Error> {
        self.lock()?.finalize()
    }

    fn sync_id(&self) -> Result<Option<String>, Self::Error> {
        self.lock()?.sync_id()
    }

    fn reset_sync_id(&self) -> Result<String, Self::Error> {
        self.lock()?.reset_sync_id()
    }

    fn ensure_current_sync_id(&self, new_sync_id: &str) -> Result<String, Self::Error> {
        self.lock()?.ensure_current_sync_id(new_sync_id)
    }
}
