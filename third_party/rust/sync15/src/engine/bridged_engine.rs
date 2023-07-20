/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;

use crate::bso::{IncomingBso, OutgoingBso};
use crate::Guid;

/// A BridgedEngine acts as a bridge between application-services, rust
/// implemented sync engines and sync engines as defined by Desktop Firefox.
///
/// [Desktop Firefox has an abstract implementation of a Sync
/// Engine](https://searchfox.org/mozilla-central/source/services/sync/modules/engines.js)
/// with a number of functions each engine is expected to override. Engines
/// implemented in Rust use a different shape (specifically, the
/// [SyncEngine](crate::SyncEngine) trait), so this BridgedEngine trait adapts
/// between the 2.
pub trait BridgedEngine: Send + Sync {
    /// Returns the last sync time, in milliseconds, for this engine's
    /// collection. This is called before each sync, to determine the lower
    /// bound for new records to fetch from the server.
    fn last_sync(&self) -> Result<i64>;

    /// Sets the last sync time, in milliseconds. This is called throughout
    /// the sync, to fast-forward the stored last sync time to match the
    /// timestamp on the uploaded records.
    fn set_last_sync(&self, last_sync_millis: i64) -> Result<()>;

    /// Returns the sync ID for this engine's collection. This is only used in
    /// tests.
    fn sync_id(&self) -> Result<Option<String>>;

    /// Resets the sync ID for this engine's collection, returning the new ID.
    /// As a side effect, implementations should reset all local Sync state,
    /// as in `reset`.
    fn reset_sync_id(&self) -> Result<String>;

    /// Ensures that the locally stored sync ID for this engine's collection
    /// matches the `new_sync_id` from the server. If the two don't match,
    /// implementations should reset all local Sync state, as in `reset`.
    /// This method returns the assigned sync ID, which can be either the
    /// `new_sync_id`, or a different one if the engine wants to force other
    /// devices to reset their Sync state for this collection the next time they
    /// sync.
    fn ensure_current_sync_id(&self, new_sync_id: &str) -> Result<String>;

    /// Tells the tabs engine about recent FxA devices. A bit of a leaky abstration as it only
    /// makes sense for tabs.
    /// The arg is a json serialized `ClientData` struct.
    fn prepare_for_sync(&self, _client_data: &str) -> Result<()> {
        Ok(())
    }

    /// Indicates that the engine is about to start syncing. This is called
    /// once per sync, and always before `store_incoming`.
    fn sync_started(&self) -> Result<()>;

    /// Stages a batch of incoming Sync records. This is called multiple
    /// times per sync, once for each batch. Implementations can use the
    /// signal to check if the operation was aborted, and cancel any
    /// pending work.
    fn store_incoming(&self, incoming_records: Vec<IncomingBso>) -> Result<()>;

    /// Applies all staged records, reconciling changes on both sides and
    /// resolving conflicts. Returns a list of records to upload.
    fn apply(&self) -> Result<ApplyResults>;

    /// Indicates that the given record IDs were uploaded successfully to the
    /// server. This is called multiple times per sync, once for each batch
    /// upload.
    fn set_uploaded(&self, server_modified_millis: i64, ids: &[Guid]) -> Result<()>;

    /// Indicates that all records have been uploaded. At this point, any record
    /// IDs marked for upload that haven't been passed to `set_uploaded`, can be
    /// assumed to have failed: for example, because the server rejected a record
    /// with an invalid TTL or sort index.
    fn sync_finished(&self) -> Result<()>;

    /// Resets all local Sync state, including any change flags, mirrors, and
    /// the last sync time, such that the next sync is treated as a first sync
    /// with all new local data. Does not erase any local user data.
    fn reset(&self) -> Result<()>;

    /// Erases all local user data for this collection, and any Sync metadata.
    /// This method is destructive, and unused for most collections.
    fn wipe(&self) -> Result<()>;
}

// TODO: We should replace this with OutgoingChangeset to reduce the number
// of types engines need to deal with.
#[derive(Debug, Default)]
pub struct ApplyResults {
    /// List of records
    pub records: Vec<OutgoingBso>,
    /// The number of incoming records whose contents were merged because they
    /// changed on both sides. None indicates we aren't reporting this
    /// information.
    pub num_reconciled: Option<usize>,
}

impl ApplyResults {
    pub fn new(records: Vec<OutgoingBso>, num_reconciled: impl Into<Option<usize>>) -> Self {
        Self {
            records,
            num_reconciled: num_reconciled.into(),
        }
    }
}

// Shorthand for engines that don't care.
impl From<Vec<OutgoingBso>> for ApplyResults {
    fn from(records: Vec<OutgoingBso>) -> Self {
        Self {
            records,
            num_reconciled: None,
        }
    }
}
