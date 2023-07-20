/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{CollectionRequest, IncomingChangeset, OutgoingChangeset};
use crate::client_types::ClientData;
use crate::{telemetry, CollectionName, Guid, ServerTimestamp};
use anyhow::Result;
use std::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CollSyncIds {
    pub global: Guid,
    pub coll: Guid,
}

/// Defines how an engine is associated with a particular set of records
/// on a sync storage server. It's either disconnected, or believes it is
/// connected with a specific set of GUIDs. If the server and the engine don't
/// agree on the exact GUIDs, the engine will assume something radical happened
/// so it can't believe anything it thinks it knows about the state of the
/// server (ie, it will "reset" then do a full reconcile)
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum EngineSyncAssociation {
    /// This store is disconnected (although it may be connected in the future).
    Disconnected,
    /// Sync is connected, and has the following sync IDs.
    Connected(CollSyncIds),
}

/// The concrete `SyncEngine` implementations
#[derive(Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum SyncEngineId {
    // Note that we've derived PartialOrd etc, which uses lexicographic ordering
    // of the variants. We leverage that such that the higher priority engines
    // are listed first.
    // This order matches desktop.
    Passwords,
    Tabs,
    Bookmarks,
    Addresses,
    CreditCards,
    History,
}

impl SyncEngineId {
    // Iterate over all possible engines. Note that we've made a policy decision
    // that this should enumerate in "order" as defined by PartialCmp, and tests
    // enforce this.
    pub fn iter() -> impl Iterator<Item = SyncEngineId> {
        [
            Self::Passwords,
            Self::Tabs,
            Self::Bookmarks,
            Self::Addresses,
            Self::CreditCards,
            Self::History,
        ]
        .into_iter()
    }

    // Get the string identifier for this engine.  This must match the strings in SyncEngineSelection.
    pub fn name(&self) -> &'static str {
        match self {
            Self::Passwords => "passwords",
            Self::History => "history",
            Self::Bookmarks => "bookmarks",
            Self::Tabs => "tabs",
            Self::Addresses => "addresses",
            Self::CreditCards => "creditcards",
        }
    }
}

impl fmt::Display for SyncEngineId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name())
    }
}

impl TryFrom<&str> for SyncEngineId {
    type Error = String;

    fn try_from(value: &str) -> std::result::Result<Self, Self::Error> {
        match value {
            "passwords" => Ok(Self::Passwords),
            "history" => Ok(Self::History),
            "bookmarks" => Ok(Self::Bookmarks),
            "tabs" => Ok(Self::Tabs),
            "addresses" => Ok(Self::Addresses),
            "creditcards" => Ok(Self::CreditCards),
            _ => Err(value.into()),
        }
    }
}

/// A "sync engine" is a thing that knows how to sync. It's often implemented
/// by a "store" (which is the generic term responsible for all storage
/// associated with a component, including storage required for sync.)
///
/// Low-level engine functionality. Engines that need custom reconciliation
/// logic should use this.
///
/// Different engines will produce errors of different types.  To accommodate
/// this, we force them all to return anyhow::Error.
pub trait SyncEngine {
    fn collection_name(&self) -> CollectionName;

    /// Prepares the engine for syncing. The tabs engine currently uses this to
    /// store the current list of clients, which it uses to look up device names
    /// and types.
    ///
    /// Note that this method is only called by `sync_multiple`, and only if a
    /// command processor is registered. In particular, `prepare_for_sync` will
    /// not be called if the store is synced using `sync::synchronize` or
    /// `sync_multiple::sync_multiple`. It _will_ be called if the store is
    /// synced via the Sync Manager.
    ///
    /// TODO(issue #2590): This is pretty cludgey and will be hard to extend for
    /// any case other than the tabs case. We should find another way to support
    /// tabs...
    fn prepare_for_sync(&self, _get_client_data: &dyn Fn() -> ClientData) -> Result<()> {
        Ok(())
    }

    /// Tells the engine what the local encryption key is for the data managed
    /// by the engine. This is only used by collections that store data
    /// encrypted locally and is unrelated to the encryption used by Sync.
    /// The intent is that for such collections, this key can be used to
    /// decrypt local data before it is re-encrypted by Sync and sent to the
    /// storage servers, and similarly, data from the storage servers will be
    /// decrypted by Sync, then encrypted by the local encryption key before
    /// being added to the local database.
    ///
    /// The expectation is that the key value is being maintained by the
    /// embedding application in some secure way suitable for the environment
    /// in which the app is running - eg, the OS "keychain". The value of the
    /// key is implementation dependent - it is expected that the engine and
    /// embedding application already have some external agreement about how
    /// to generate keys and in what form they are exchanged. Finally, there's
    /// an assumption that sync engines are short-lived and only live for a
    /// single sync - this means that sync doesn't hold on to the key for an
    /// extended period.
    ///
    /// This will panic if called by an engine that doesn't have explicit
    /// support for local encryption keys as that implies a degree of confusion
    /// which shouldn't be possible to ignore.
    fn set_local_encryption_key(&mut self, _key: &str) -> Result<()> {
        unimplemented!("This engine does not support local encryption");
    }

    /// `inbound` is a vector to support the case where
    /// `get_collection_requests` returned multiple requests. The changesets are
    /// in the same order as the requests were -- e.g. if `vec![req_a, req_b]`
    /// was returned from `get_collection_requests`, `inbound` will have the
    /// results from `req_a` as its first index, and those from `req_b` as it's
    /// second.
    fn apply_incoming(
        &self,
        inbound: Vec<IncomingChangeset>,
        telem: &mut telemetry::Engine,
    ) -> Result<OutgoingChangeset>;

    fn sync_finished(
        &self,
        new_timestamp: ServerTimestamp,
        records_synced: Vec<Guid>,
    ) -> Result<()>;

    /// The engine is responsible for building collection requests. Engines
    /// typically will store a lastModified timestamp and use that to build a
    /// request saying "give me full records since that date" - however, other
    /// engines might do something fancier. This could even later be extended to
    /// handle "backfills" etc
    ///
    /// To support more advanced use cases,  multiple requests can be returned
    /// here - either from the same or different collections. The vast majority
    /// of engines will just want to return zero or one item in their vector
    /// (zero is a valid optimization when the server timestamp is the same as
    /// the engine last saw, one when it is not)
    ///
    /// Important: In the case when more than one collection is requested, it's
    /// assumed the last one is the "canonical" one. (That is, it must be for
    /// "this" collection, its timestamp is used to represent the sync, etc).
    /// (Note that multiple collection request support is currently unused, so
    /// it might make sense to delete it - if we need it later, we may find a
    /// better shape for our use-case)
    fn get_collection_requests(
        &self,
        server_timestamp: ServerTimestamp,
    ) -> Result<Vec<CollectionRequest>>;

    /// Get persisted sync IDs. If they don't match the global state we'll be
    /// `reset()` with the new IDs.
    fn get_sync_assoc(&self) -> Result<EngineSyncAssociation>;

    /// Reset the engine (and associated store) without wiping local data,
    /// ready for a "first sync".
    /// `assoc` defines how this store is to be associated with sync.
    fn reset(&self, assoc: &EngineSyncAssociation) -> Result<()>;

    fn wipe(&self) -> Result<()>;
}

#[cfg(test)]
mod test {
    use super::*;
    use std::iter::zip;

    #[test]
    fn test_engine_priority() {
        fn sorted(mut engines: Vec<SyncEngineId>) -> Vec<SyncEngineId> {
            engines.sort();
            engines
        }
        assert_eq!(
            vec![SyncEngineId::Passwords, SyncEngineId::Tabs],
            sorted(vec![SyncEngineId::Passwords, SyncEngineId::Tabs])
        );
        assert_eq!(
            vec![SyncEngineId::Passwords, SyncEngineId::Tabs],
            sorted(vec![SyncEngineId::Tabs, SyncEngineId::Passwords])
        );
    }

    #[test]
    fn test_engine_enum_order() {
        let unsorted = SyncEngineId::iter().collect::<Vec<SyncEngineId>>();
        let mut sorted = SyncEngineId::iter().collect::<Vec<SyncEngineId>>();
        sorted.sort();

        // iterating should supply identical elements in each.
        assert!(zip(unsorted, sorted).fold(true, |acc, (a, b)| acc && (a == b)))
    }
}
