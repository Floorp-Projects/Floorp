/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(unknown_lints)]
#![warn(rust_2018_idioms)]

#[macro_use]
pub mod error;
mod schema;
mod storage;
mod store;
mod sync;

pub use types::Timestamp;

// for the UDL
impl UniffiCustomTypeConverter for Timestamp {
    type Builtin = i64;

    fn into_custom(val: Self::Builtin) -> uniffi::Result<Self> {
        Ok(Self(val as u64))
    }

    fn from_custom(obj: Self) -> Self::Builtin {
        obj.as_millis() as i64
    }
}

uniffi::include_scaffolding!("tabs");

// Our UDL uses a `Guid` type.
use sync_guid::Guid as TabsGuid;
impl UniffiCustomTypeConverter for TabsGuid {
    type Builtin = String;

    fn into_custom(val: Self::Builtin) -> uniffi::Result<TabsGuid> {
        Ok(TabsGuid::new(val.as_str()))
    }

    fn from_custom(obj: Self) -> Self::Builtin {
        obj.into()
    }
}

pub use crate::storage::{ClientRemoteTabs, RemoteTabRecord, TabsDeviceType};
pub use crate::store::{RemoteCommandStore, TabsStore};
pub use error::{ApiResult, Error, Result, TabsApiError};
use sync15::DeviceType;

pub use crate::sync::engine::get_registered_sync_engine;

pub use crate::sync::bridge::TabsBridgedEngine;
pub use crate::sync::engine::TabsEngine;

#[derive(Clone, Eq, PartialEq, Debug)]
pub enum RemoteCommand {
    CloseTab { url: String },
    // eg, CloseInactive, ??
}

// Tabs that were requested to be closed on other clients
#[derive(Clone, Eq, PartialEq, Debug)]
pub struct PendingCommand {
    pub device_id: String,
    pub command: RemoteCommand,
    pub time_requested: Timestamp,
    pub time_sent: Option<Timestamp>,
}
