/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # sync15-traits
//!
//! The sync15-traits support component is a home for the core types and traits
//! used by Sync. It's extracted into its own crate because these types are used
//! in so many different contexts, from the [Sync Manager](../sync_manager/index.html)
//! to desktop Firefox.
//!
//! Because this is a "support" component, there aren't any instructions for how
//! to use this component, but you will find documentation for most of the types
//! and traits implemented here.

#![warn(rust_2018_idioms)]
pub mod bridged_engine;
#[cfg(feature = "crypto")]
mod bso_record;
mod changeset;
pub mod client;
mod engine;
mod error;
#[cfg(feature = "crypto")]
mod key_bundle;
mod payload;
pub mod request;
mod server_timestamp;
pub mod telemetry;

pub use bridged_engine::{ApplyResults, BridgedEngine, IncomingEnvelope, OutgoingEnvelope};
#[cfg(feature = "crypto")]
pub use bso_record::{BsoRecord, CleartextBso, EncryptedBso, EncryptedPayload};
pub use changeset::{IncomingChangeset, OutgoingChangeset, RecordChangeset};
pub use client::DeviceType;
pub use engine::{CollSyncIds, EngineSyncAssociation, SyncEngine, SyncEngineId};
pub use error::SyncTraitsError;
#[cfg(feature = "crypto")]
pub use key_bundle::KeyBundle;
pub use payload::Payload;
pub use request::{CollectionRequest, RequestOrder};
pub use server_timestamp::ServerTimestamp;
pub use sync_guid::Guid;

// For skip_serializing_if
pub(crate) fn skip_if_default<T: PartialEq + Default>(v: &T) -> bool {
    *v == T::default()
}
