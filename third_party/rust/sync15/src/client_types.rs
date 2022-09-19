/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module has to be here because of some hard-to-avoid hacks done for the
//! tabs engine... See issue #2590

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Argument to Store::prepare_for_sync. See comment there for more info. Only
/// really intended to be used by tabs engine.
#[derive(Clone, Debug)]
pub struct ClientData {
    pub local_client_id: String,
    pub recent_clients: HashMap<String, RemoteClient>,
}

/// Information about a remote client in the clients collection.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct RemoteClient {
    pub fxa_device_id: Option<String>,
    pub device_name: String,
    pub device_type: Option<DeviceType>,
}

/// Enumeration for the different types of device.
///
/// Firefox Accounts and the broader Sync universe separates devices into broad categories for
/// display purposes, such as distinguishing a desktop PC from a mobile phone.

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum DeviceType {
    #[serde(rename = "desktop")]
    Desktop,
    #[serde(rename = "mobile")]
    Mobile,
    #[serde(rename = "tablet")]
    Tablet,
    #[serde(rename = "vr")]
    VR,
    #[serde(rename = "tv")]
    TV,
    // Unknown is a bit odd - it should never be set (ie, it's never serialized)
    // and exists really just so we can avoid using an Option<>.
    #[serde(other)]
    #[serde(skip_serializing)] // Don't you dare trying.
    Unknown,
}

#[cfg(test)]
mod device_type_tests {
    use super::*;

    #[test]
    fn test_serde_ser() {
        assert_eq!(
            serde_json::to_string(&DeviceType::Desktop).unwrap(),
            "\"desktop\""
        );
        assert_eq!(
            serde_json::to_string(&DeviceType::Mobile).unwrap(),
            "\"mobile\""
        );
        assert_eq!(
            serde_json::to_string(&DeviceType::Tablet).unwrap(),
            "\"tablet\""
        );
        assert_eq!(serde_json::to_string(&DeviceType::VR).unwrap(), "\"vr\"");
        assert_eq!(serde_json::to_string(&DeviceType::TV).unwrap(), "\"tv\"");
        assert!(serde_json::to_string(&DeviceType::Unknown).is_err());
    }

    #[test]
    fn test_serde_de() {
        assert!(matches!(
            serde_json::from_str::<DeviceType>("\"desktop\"").unwrap(),
            DeviceType::Desktop
        ));
        assert!(matches!(
            serde_json::from_str::<DeviceType>("\"mobile\"").unwrap(),
            DeviceType::Mobile
        ));
        assert!(matches!(
            serde_json::from_str::<DeviceType>("\"tablet\"").unwrap(),
            DeviceType::Tablet
        ));
        assert!(matches!(
            serde_json::from_str::<DeviceType>("\"vr\"").unwrap(),
            DeviceType::VR
        ));
        assert!(matches!(
            serde_json::from_str::<DeviceType>("\"tv\"").unwrap(),
            DeviceType::TV
        ));
        assert!(matches!(
            serde_json::from_str::<DeviceType>("\"something-else\"").unwrap(),
            DeviceType::Unknown,
        ));
    }
}
