/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This type is strictly owned by FxA, but is defined in this crate because of
//! some hard-to-avoid hacks done for the tabs engine... See issue #2590.
//!
//! Thus, fxa-client ends up taking a dep on this crate, which is roughly
//! the opposite of reality.

use serde::{Deserialize, Deserializer, Serialize, Serializer};

/// Enumeration for the different types of device.
///
/// Firefox Accounts and the broader Sync universe separates devices into broad categories for
/// various purposes, such as distinguishing a desktop PC from a mobile phone.
///
/// A special variant in this enum, `DeviceType::Unknown` is used to capture
/// the string values we don't recognise. It also has a custom serde serializer and deserializer
/// which implements the following semantics:
/// * deserializing a `DeviceType` which uses a string value we don't recognise or null will return
///   `DeviceType::Unknown` rather than returning an error.
/// * serializing `DeviceType::Unknown` will serialize `null`.
///
/// This has a few important implications:
/// * In general, `Option<DeviceType>` should be avoided, and a plain `DeviceType` used instead,
///   because in that case, `None` would be semantically identical to `DeviceType::Unknown` and
///   as mentioned above, `null` already deserializes as `DeviceType::Unknown`.
/// * Any unknown device types can not be round-tripped via this enum - eg, if you deserialize
///   a struct holding a `DeviceType` string value we don't recognize, then re-serialize it, the
///   original string value is lost. We don't consider this a problem because in practice, we only
///   upload records with *this* device's type, not the type of other devices, and it's reasonable
///   to assume that this module knows about all valid device types for the device type it is
///   deployed on.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, Hash)]
pub enum DeviceType {
    Desktop,
    Mobile,
    Tablet,
    VR,
    TV,
    // See docstrings above re how Unknown is serialized and deserialized.
    #[default]
    Unknown,
}

impl<'de> Deserialize<'de> for DeviceType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Ok(match String::deserialize(deserializer) {
            Ok(s) => match s.as_str() {
                "desktop" => DeviceType::Desktop,
                "mobile" => DeviceType::Mobile,
                "tablet" => DeviceType::Tablet,
                "vr" => DeviceType::VR,
                "tv" => DeviceType::TV,
                // There's a vague possibility that desktop might serialize "phone" for mobile
                // devices - https://searchfox.org/mozilla-central/rev/a156a65ced2dae5913ae35a68e9445b8ee7ca457/services/sync/modules/engines/clients.js#292
                "phone" => DeviceType::Mobile,
                // Everything else is Unknown.
                _ => DeviceType::Unknown,
            },
            // Anything other than a string is "unknown" - this isn't ideal - we really only want
            // to handle null and, eg, a number probably should be an error, but meh.
            Err(_) => DeviceType::Unknown,
        })
    }
}
impl Serialize for DeviceType {
    fn serialize<S>(&self, s: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            // It's unfortunate we need to duplicate the strings here...
            DeviceType::Desktop => s.serialize_unit_variant("DeviceType", 0, "desktop"),
            DeviceType::Mobile => s.serialize_unit_variant("DeviceType", 1, "mobile"),
            DeviceType::Tablet => s.serialize_unit_variant("DeviceType", 2, "tablet"),
            DeviceType::VR => s.serialize_unit_variant("DeviceType", 3, "vr"),
            DeviceType::TV => s.serialize_unit_variant("DeviceType", 4, "tv"),
            // This is the important bit - Unknown -> None
            DeviceType::Unknown => s.serialize_none(),
        }
    }
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
        assert_eq!(serde_json::to_string(&DeviceType::Unknown).unwrap(), "null");
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
        assert!(matches!(
            serde_json::from_str::<DeviceType>("null").unwrap(),
            DeviceType::Unknown,
        ));
        assert!(matches!(
            serde_json::from_str::<DeviceType>("99").unwrap(),
            DeviceType::Unknown,
        ));
    }
}
