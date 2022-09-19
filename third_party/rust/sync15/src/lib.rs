/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(unknown_lints, clippy::implicit_hasher)]
#![warn(rust_2018_idioms)]

#[cfg(feature = "crypto")]
mod bso_record;
#[cfg(feature = "sync-client")]
pub mod client;
// Types to describe client records
mod client_types;
// Note that `clients_engine` should probably be in `sync_client`, but let's not make
// things too nested at this stage...
#[cfg(feature = "sync-client")]
pub mod clients_engine;
#[cfg(feature = "sync-engine")]
pub mod engine;
mod error;
#[cfg(feature = "crypto")]
mod key_bundle;
mod payload;
mod record_types;
mod server_timestamp;
pub mod telemetry;

pub use crate::client_types::{ClientData, DeviceType, RemoteClient};
pub use crate::error::{Error, Result};
#[cfg(feature = "crypto")]
pub use bso_record::{BsoRecord, CleartextBso, EncryptedBso, EncryptedPayload};
#[cfg(feature = "crypto")]
pub use key_bundle::KeyBundle;
pub use payload::Payload;
pub use server_timestamp::ServerTimestamp;
pub use sync_guid::Guid;

// For skip_serializing_if
fn skip_if_default<T: PartialEq + Default>(v: &T) -> bool {
    *v == T::default()
}
