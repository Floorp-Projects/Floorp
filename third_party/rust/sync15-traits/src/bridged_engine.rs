/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{error::Error, fmt};

use serde::{Deserialize, Serialize};

use super::{Guid, Payload, ServerTimestamp};

/// A BridgedEngine acts as a bridge between application-services, rust
/// implemented sync engines and sync engines as defined by Desktop Firefox.
///
/// [Desktop Firefox has an abstract implementation of a Sync
/// Engine](https://searchfox.org/mozilla-central/source/services/sync/modules/engines.js)
/// with a number of functions each engine is expected to override. Engines
/// implemented in Rust use a different shape (specifically, the
/// [SyncEngine](crate::SyncEngine) trait), so this BridgedEngine trait adapts
/// between the 2.
pub trait BridgedEngine {
    /// The type returned for errors.
    type Error;

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

    /// Indicates that the engine is about to start syncing. This is called
    /// once per sync, and always before `store_incoming`.
    fn sync_started(&self) -> Result<(), Self::Error>;

    /// Stages a batch of incoming Sync records. This is called multiple
    /// times per sync, once for each batch. Implementations can use the
    /// signal to check if the operation was aborted, and cancel any
    /// pending work.
    fn store_incoming(&self, incoming_payloads: &[IncomingEnvelope]) -> Result<(), Self::Error>;

    /// Applies all staged records, reconciling changes on both sides and
    /// resolving conflicts. Returns a list of records to upload.
    fn apply(&self) -> Result<ApplyResults, Self::Error>;

    /// Indicates that the given record IDs were uploaded successfully to the
    /// server. This is called multiple times per sync, once for each batch
    /// upload.
    fn set_uploaded(&self, server_modified_millis: i64, ids: &[Guid]) -> Result<(), Self::Error>;

    /// Indicates that all records have been uploaded. At this point, any record
    /// IDs marked for upload that haven't been passed to `set_uploaded`, can be
    /// assumed to have failed: for example, because the server rejected a record
    /// with an invalid TTL or sort index.
    fn sync_finished(&self) -> Result<(), Self::Error>;

    /// Resets all local Sync state, including any change flags, mirrors, and
    /// the last sync time, such that the next sync is treated as a first sync
    /// with all new local data. Does not erase any local user data.
    fn reset(&self) -> Result<(), Self::Error>;

    /// Erases all local user data for this collection, and any Sync metadata.
    /// This method is destructive, and unused for most collections.
    fn wipe(&self) -> Result<(), Self::Error>;
}

#[derive(Clone, Debug, Default)]
pub struct ApplyResults {
    /// List of records
    pub envelopes: Vec<OutgoingEnvelope>,
    /// The number of incoming records whose contents were merged because they
    /// changed on both sides. None indicates we aren't reporting this
    /// information.
    pub num_reconciled: Option<usize>,
}

impl ApplyResults {
    pub fn new(envelopes: Vec<OutgoingEnvelope>, num_reconciled: impl Into<Option<usize>>) -> Self {
        Self {
            envelopes,
            num_reconciled: num_reconciled.into(),
        }
    }
}

// Shorthand for engines that don't care.
impl From<Vec<OutgoingEnvelope>> for ApplyResults {
    fn from(envelopes: Vec<OutgoingEnvelope>) -> Self {
        Self {
            envelopes,
            num_reconciled: None,
        }
    }
}

/// An envelope for an incoming item, passed to `BridgedEngine::store_incoming`.
/// Envelopes are a halfway point between BSOs, the format used for all items on
/// the Sync server, and records, which are specific to each engine.
///
/// Specifically, the "envelope" has all the metadata plus the JSON payload
/// as clear-text - the analogy is that it's got all the info needed to get the
/// data from the server to the engine without knowing what the contents holds,
/// and without the engine needing to know how to decrypt.
///
/// A BSO is a JSON object with metadata fields (`id`, `modifed`, `sortindex`),
/// and a BSO payload that is itself a JSON string. For encrypted records, the
/// BSO payload has a ciphertext, which must be decrypted to yield a cleartext.
/// The payload is a cleartext JSON string (that's three levels of JSON wrapping, if
/// you're keeping score: the BSO itself, BSO payload, and our sub-payload) with the
/// actual content payload.
///
/// An envelope combines the metadata fields from the BSO, and the cleartext
/// BSO payload.
#[derive(Clone, Debug, Deserialize)]
pub struct IncomingEnvelope {
    pub id: Guid,
    pub modified: ServerTimestamp,
    #[serde(default)]
    pub sortindex: Option<i32>,
    #[serde(default)]
    pub ttl: Option<u32>,
    // Don't provide access to the cleartext payload directly. We want all
    // callers to use `payload()` to convert/validate the string.
    payload: String,
}

impl IncomingEnvelope {
    /// Parses and returns the record payload from this envelope. Returns an
    /// error if the envelope's payload isn't valid.
    pub fn payload(&self) -> Result<Payload, PayloadError> {
        let payload: Payload = serde_json::from_str(&self.payload)?;
        if payload.id != self.id {
            return Err(PayloadError::MismatchedId {
                envelope: self.id.clone(),
                payload: payload.id,
            });
        }
        // Remove auto field data from payload and replace with real data
        Ok(payload
            .with_auto_field("ttl", self.ttl)
            .with_auto_field("sortindex", self.sortindex))
    }
}

/// An envelope for an outgoing item, returned from `BridgedEngine::apply`. This
/// is conceptually identical to [IncomingEnvelope], but omits fields that are
/// only set by the server, like `modified`.
#[derive(Clone, Debug, Serialize)]
pub struct OutgoingEnvelope {
    id: Guid,
    payload: String,
    sortindex: Option<i32>,
    ttl: Option<u32>,
}

impl From<Payload> for OutgoingEnvelope {
    fn from(mut payload: Payload) -> Self {
        let id = payload.id.clone();
        // Remove auto field data from OutgoingEnvelope payload
        let ttl = payload.take_auto_field("ttl");
        let sortindex = payload.take_auto_field("sortindex");
        OutgoingEnvelope {
            id,
            payload: payload.into_json_string(),
            sortindex,
            ttl,
        }
    }
}

/// An error that indicates a payload is invalid.
#[derive(Debug)]
pub enum PayloadError {
    /// The payload contains invalid JSON.
    Invalid(serde_json::Error),
    /// The ID of the BSO in the envelope doesn't match the ID in the payload.
    MismatchedId { envelope: Guid, payload: Guid },
}

impl Error for PayloadError {}

impl From<serde_json::Error> for PayloadError {
    fn from(err: serde_json::Error) -> PayloadError {
        PayloadError::Invalid(err)
    }
}

impl fmt::Display for PayloadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PayloadError::Invalid(err) => err.fmt(f),
            PayloadError::MismatchedId { envelope, payload } => write!(
                f,
                "ID `{}` in envelope doesn't match `{}` in payload",
                envelope, payload
            ),
        }
    }
}
