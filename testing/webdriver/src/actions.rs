/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::common::{WebElement, ELEMENT_KEY};
use serde::de::{self, Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use serde_json::Value;
use std::default::Default;
use std::f64;
use unicode_segmentation::UnicodeSegmentation;

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct ActionSequence {
    pub id: String,
    #[serde(flatten)]
    pub actions: ActionsType,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum ActionsType {
    #[serde(rename = "none")]
    Null { actions: Vec<NullActionItem> },
    #[serde(rename = "key")]
    Key { actions: Vec<KeyActionItem> },
    #[serde(rename = "pointer")]
    Pointer {
        #[serde(default)]
        parameters: PointerActionParameters,
        actions: Vec<PointerActionItem>,
    },
    #[serde(rename = "wheel")]
    Wheel { actions: Vec<WheelActionItem> },
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum NullActionItem {
    General(GeneralAction),
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum GeneralAction {
    #[serde(rename = "pause")]
    Pause(PauseAction),
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct PauseAction {
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub duration: Option<u64>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum KeyActionItem {
    General(GeneralAction),
    Key(KeyAction),
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum KeyAction {
    #[serde(rename = "keyDown")]
    Down(KeyDownAction),
    #[serde(rename = "keyUp")]
    Up(KeyUpAction),
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct KeyDownAction {
    #[serde(deserialize_with = "deserialize_key_action_value")]
    pub value: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct KeyUpAction {
    #[serde(deserialize_with = "deserialize_key_action_value")]
    pub value: String,
}

fn deserialize_key_action_value<'de, D>(deserializer: D) -> Result<String, D::Error>
where
    D: Deserializer<'de>,
{
    String::deserialize(deserializer).map(|value| {
        // Only a single Unicode grapheme cluster is allowed
        if value.graphemes(true).count() != 1 {
            return Err(de::Error::custom(format!(
                "'{}' should only contain a single Unicode code point",
                value
            )));
        }

        Ok(value)
    })?
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum PointerType {
    Mouse,
    Pen,
    Touch,
}

impl Default for PointerType {
    fn default() -> PointerType {
        PointerType::Mouse
    }
}

#[derive(Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct PointerActionParameters {
    #[serde(rename = "pointerType")]
    pub pointer_type: PointerType,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum PointerActionItem {
    General(GeneralAction),
    Pointer(PointerAction),
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum PointerAction {
    #[serde(rename = "pointerCancel")]
    Cancel,
    #[serde(rename = "pointerDown")]
    Down(PointerDownAction),
    #[serde(rename = "pointerMove")]
    Move(PointerMoveAction),
    #[serde(rename = "pointerUp")]
    Up(PointerUpAction),
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct PointerDownAction {
    pub button: u64,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub width: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub height: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_pressure"
    )]
    pub pressure: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tangential_pressure"
    )]
    pub tangentialPressure: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tilt"
    )]
    pub tiltX: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tilt"
    )]
    pub tiltY: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_twist"
    )]
    pub twist: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_altitude_angle"
    )]
    pub altitudeAngle: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_azimuth_angle"
    )]
    pub azimuthAngle: Option<f64>,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct PointerMoveAction {
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub duration: Option<u64>,
    #[serde(default)]
    pub origin: PointerOrigin,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_i64"
    )]
    pub x: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_i64"
    )]
    pub y: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub width: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub height: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_pressure"
    )]
    pub pressure: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tangential_pressure"
    )]
    pub tangentialPressure: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tilt"
    )]
    pub tiltX: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tilt"
    )]
    pub tiltY: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_twist"
    )]
    pub twist: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_altitude_angle"
    )]
    pub altitudeAngle: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_azimuth_angle"
    )]
    pub azimuthAngle: Option<f64>,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct PointerUpAction {
    pub button: u64,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub width: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub height: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_pressure"
    )]
    pub pressure: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tangential_pressure"
    )]
    pub tangentialPressure: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tilt"
    )]
    pub tiltX: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_tilt"
    )]
    pub tiltY: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_twist"
    )]
    pub twist: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_altitude_angle"
    )]
    pub altitudeAngle: Option<f64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_azimuth_angle"
    )]
    pub azimuthAngle: Option<f64>,
}

#[derive(Clone, Debug, PartialEq, Serialize)]
pub enum PointerOrigin {
    #[serde(
        rename = "element-6066-11e4-a52e-4f735466cecf",
        serialize_with = "serialize_webelement_id"
    )]
    Element(WebElement),
    #[serde(rename = "pointer")]
    Pointer,
    #[serde(rename = "viewport")]
    Viewport,
}

impl Default for PointerOrigin {
    fn default() -> PointerOrigin {
        PointerOrigin::Viewport
    }
}

// TODO: The custom deserializer can be removed once the support of the legacy
// ELEMENT key has been removed from Selenium bindings
// See: https://github.com/SeleniumHQ/selenium/issues/6393
impl<'de> Deserialize<'de> for PointerOrigin {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let value = Value::deserialize(deserializer)?;
        if let Some(web_element) = value.get(ELEMENT_KEY) {
            String::deserialize(web_element)
                .map(|id| PointerOrigin::Element(WebElement(id)))
                .map_err(de::Error::custom)
        } else if value == "pointer" {
            Ok(PointerOrigin::Pointer)
        } else if value == "viewport" {
            Ok(PointerOrigin::Viewport)
        } else {
            Err(de::Error::custom(format!(
                "unknown value `{}`, expected `pointer`, `viewport`, or `element-6066-11e4-a52e-4f735466cecf`",
                value
            )))
        }
    }
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum WheelActionItem {
    General(GeneralAction),
    Wheel(WheelAction),
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum WheelAction {
    #[serde(rename = "scroll")]
    Scroll(WheelScrollAction),
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WheelScrollAction {
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_u64"
    )]
    pub duration: Option<u64>,
    #[serde(default)]
    pub origin: PointerOrigin,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_i64"
    )]
    pub x: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_i64"
    )]
    pub y: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_i64"
    )]
    pub deltaX: Option<i64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_option_i64"
    )]
    pub deltaY: Option<i64>,
}

fn serialize_webelement_id<S>(element: &WebElement, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    element.to_string().serialize(serializer)
}

fn deserialize_to_option_i64<'de, D>(deserializer: D) -> Result<Option<i64>, D::Error>
where
    D: Deserializer<'de>,
{
    Option::deserialize(deserializer)?
        .ok_or_else(|| de::Error::custom("invalid type: null, expected i64"))
}

fn deserialize_to_option_u64<'de, D>(deserializer: D) -> Result<Option<u64>, D::Error>
where
    D: Deserializer<'de>,
{
    Option::deserialize(deserializer)?
        .ok_or_else(|| de::Error::custom("invalid type: null, expected i64"))
}

fn deserialize_to_option_f64<'de, D>(deserializer: D) -> Result<Option<f64>, D::Error>
where
    D: Deserializer<'de>,
{
    Option::deserialize(deserializer)?
        .ok_or_else(|| de::Error::custom("invalid type: null, expected f64"))
}

fn deserialize_to_pressure<'de, D>(deserializer: D) -> Result<Option<f64>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt_value = deserialize_to_option_f64(deserializer)?;
    if let Some(value) = opt_value {
        if !(0f64..=1.0).contains(&value) {
            return Err(de::Error::custom(format!("{} is outside range 0-1", value)));
        }
    };
    Ok(opt_value)
}

fn deserialize_to_tangential_pressure<'de, D>(deserializer: D) -> Result<Option<f64>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt_value = deserialize_to_option_f64(deserializer)?;
    if let Some(value) = opt_value {
        if !(-1.0..=1.0).contains(&value) {
            return Err(de::Error::custom(format!(
                "{} is outside range -1-1",
                value
            )));
        }
    };
    Ok(opt_value)
}

fn deserialize_to_tilt<'de, D>(deserializer: D) -> Result<Option<i64>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt_value = deserialize_to_option_i64(deserializer)?;
    if let Some(value) = opt_value {
        if !(-90..=90).contains(&value) {
            return Err(de::Error::custom(format!(
                "{} is outside range -90-90",
                value
            )));
        }
    };
    Ok(opt_value)
}

fn deserialize_to_twist<'de, D>(deserializer: D) -> Result<Option<u64>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt_value = deserialize_to_option_u64(deserializer)?;
    if let Some(value) = opt_value {
        if !(0..=359).contains(&value) {
            return Err(de::Error::custom(format!(
                "{} is outside range 0-359",
                value
            )));
        }
    };
    Ok(opt_value)
}

fn deserialize_to_altitude_angle<'de, D>(deserializer: D) -> Result<Option<f64>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt_value = deserialize_to_option_f64(deserializer)?;
    if let Some(value) = opt_value {
        if !(0f64..=f64::consts::FRAC_PI_2).contains(&value) {
            return Err(de::Error::custom(format!(
                "{} is outside range 0-PI/2",
                value
            )));
        }
    };
    Ok(opt_value)
}

fn deserialize_to_azimuth_angle<'de, D>(deserializer: D) -> Result<Option<f64>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt_value = deserialize_to_option_f64(deserializer)?;
    if let Some(value) = opt_value {
        if !(0f64..=f64::consts::TAU).contains(&value) {
            return Err(de::Error::custom(format!(
                "{} is outside range 0-2*PI",
                value
            )));
        }
    };
    Ok(opt_value)
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test::{assert_de, assert_ser_de};
    use serde_json::{self, json, Value};

    #[test]
    fn test_json_action_sequence_null() {
        let json = json!({
            "id": "some_key",
            "type": "none",
            "actions": [{
                "type": "pause",
                "duration": 1,
            }]
        });
        let seq = ActionSequence {
            id: "some_key".into(),
            actions: ActionsType::Null {
                actions: vec![NullActionItem::General(GeneralAction::Pause(PauseAction {
                    duration: Some(1),
                }))],
            },
        };

        assert_ser_de(&seq, json);
    }

    #[test]
    fn test_json_action_sequence_key() {
        let json = json!({
            "id": "some_key",
            "type": "key",
            "actions": [
                {"type": "keyDown", "value": "f"},
            ],
        });
        let seq = ActionSequence {
            id: "some_key".into(),
            actions: ActionsType::Key {
                actions: vec![KeyActionItem::Key(KeyAction::Down(KeyDownAction {
                    value: String::from("f"),
                }))],
            },
        };

        assert_ser_de(&seq, json);
    }

    #[test]
    fn test_json_action_sequence_pointer() {
        let json = json!({
            "id": "some_pointer",
            "type": "pointer",
            "parameters": {
                "pointerType": "mouse"
            },
            "actions": [
                {"type": "pointerDown", "button": 0},
                {"type": "pointerMove", "origin": "pointer", "x": 10, "y": 20},
                {"type": "pointerUp", "button": 0},
            ]
        });
        let seq = ActionSequence {
            id: "some_pointer".into(),
            actions: ActionsType::Pointer {
                parameters: PointerActionParameters {
                    pointer_type: PointerType::Mouse,
                },
                actions: vec![
                    PointerActionItem::Pointer(PointerAction::Down(PointerDownAction {
                        button: 0,
                        ..Default::default()
                    })),
                    PointerActionItem::Pointer(PointerAction::Move(PointerMoveAction {
                        origin: PointerOrigin::Pointer,
                        duration: None,
                        x: Some(10),
                        y: Some(20),
                        ..Default::default()
                    })),
                    PointerActionItem::Pointer(PointerAction::Up(PointerUpAction {
                        button: 0,
                        ..Default::default()
                    })),
                ],
            },
        };

        assert_ser_de(&seq, json);
    }

    #[test]
    fn test_json_action_sequence_id_missing() {
        let json = json!({
            "type": "key",
            "actions": [],
        });
        assert!(serde_json::from_value::<ActionSequence>(json).is_err());
    }

    #[test]
    fn test_json_action_sequence_id_null() {
        let json = json!({
            "id": null,
            "type": "key",
            "actions": [],
        });
        assert!(serde_json::from_value::<ActionSequence>(json).is_err());
    }

    #[test]
    fn test_json_action_sequence_actions_missing() {
        assert!(serde_json::from_value::<ActionSequence>(json!({"id": "3"})).is_err());
    }

    #[test]
    fn test_json_action_sequence_actions_null() {
        let json = json!({
            "id": "3",
            "actions": null,
        });
        assert!(serde_json::from_value::<ActionSequence>(json).is_err());
    }

    #[test]
    fn test_json_action_sequence_actions_invalid_type() {
        let json = json!({
            "id": "3",
            "actions": "foo",
        });
        assert!(serde_json::from_value::<ActionSequence>(json).is_err());
    }

    #[test]
    fn test_json_actions_type_null() {
        let json = json!({
            "type": "none",
            "actions": [{
                "type": "pause",
                "duration": 1,
            }],
        });
        let null = ActionsType::Null {
            actions: vec![NullActionItem::General(GeneralAction::Pause(PauseAction {
                duration: Some(1),
            }))],
        };

        assert_ser_de(&null, json);
    }

    #[test]
    fn test_json_actions_type_key() {
        let json = json!({
            "type": "key",
            "actions": [{
                "type": "keyDown",
                "value": "f",
            }],
        });
        let key = ActionsType::Key {
            actions: vec![KeyActionItem::Key(KeyAction::Down(KeyDownAction {
                value: String::from("f"),
            }))],
        };

        assert_ser_de(&key, json);
    }

    #[test]
    fn test_json_actions_type_pointer() {
        let json = json!({
        "type": "pointer",
        "parameters": {"pointerType": "mouse"},
        "actions": [
            {"type": "pointerDown", "button": 1},
        ]});
        let pointer = ActionsType::Pointer {
            parameters: PointerActionParameters {
                pointer_type: PointerType::Mouse,
            },
            actions: vec![PointerActionItem::Pointer(PointerAction::Down(
                PointerDownAction {
                    button: 1,
                    ..Default::default()
                },
            ))],
        };

        assert_ser_de(&pointer, json);
    }

    #[test]
    fn test_json_actions_type_pointer_with_parameters_missing() {
        let json = json!({
        "type": "pointer",
        "actions": [
            {"type": "pointerDown", "button": 1},
        ]});
        let pointer = ActionsType::Pointer {
            parameters: PointerActionParameters {
                pointer_type: PointerType::Mouse,
            },
            actions: vec![PointerActionItem::Pointer(PointerAction::Down(
                PointerDownAction {
                    button: 1,
                    ..Default::default()
                },
            ))],
        };

        assert_de(&pointer, json);
    }

    #[test]
    fn test_json_actions_type_pointer_with_parameters_invalid_type() {
        let json = json!({
        "type": "pointer",
        "parameters": null,
        "actions": [
            {"type":"pointerDown", "button": 1},
        ]});
        assert!(serde_json::from_value::<ActionsType>(json).is_err());
    }

    #[test]
    fn test_json_actions_type_invalid() {
        let json = json!({"actions": [{"foo": "bar"}]});
        assert!(serde_json::from_value::<ActionsType>(json).is_err());
    }

    #[test]
    fn test_json_null_action_item_general() {
        let pause =
            NullActionItem::General(GeneralAction::Pause(PauseAction { duration: Some(1) }));
        assert_ser_de(&pause, json!({"type": "pause", "duration": 1}));
    }

    #[test]
    fn test_json_null_action_item_invalid_type() {
        assert!(serde_json::from_value::<NullActionItem>(json!({"type": "invalid"})).is_err());
    }

    #[test]
    fn test_json_general_action_pause() {
        let pause = GeneralAction::Pause(PauseAction { duration: Some(1) });
        assert_ser_de(&pause, json!({"type": "pause", "duration": 1}));
    }

    #[test]
    fn test_json_general_action_pause_with_duration_missing() {
        let pause = GeneralAction::Pause(PauseAction { duration: None });
        assert_ser_de(&pause, json!({"type": "pause"}));
    }

    #[test]
    fn test_json_general_action_pause_with_duration_null() {
        let json = json!({"type": "pause", "duration": null});
        assert!(serde_json::from_value::<GeneralAction>(json).is_err());
    }

    #[test]
    fn test_json_general_action_pause_with_duration_invalid_type() {
        let json = json!({"type": "pause", "duration":" foo"});
        assert!(serde_json::from_value::<GeneralAction>(json).is_err());
    }

    #[test]
    fn test_json_general_action_pause_with_duration_negative() {
        let json = json!({"type": "pause", "duration": -30});
        assert!(serde_json::from_value::<GeneralAction>(json).is_err());
    }

    #[test]
    fn test_json_key_action_item_general() {
        let pause = KeyActionItem::General(GeneralAction::Pause(PauseAction { duration: Some(1) }));
        assert_ser_de(&pause, json!({"type": "pause", "duration": 1}));
    }

    #[test]
    fn test_json_key_action_item_key() {
        let key_down = KeyActionItem::Key(KeyAction::Down(KeyDownAction {
            value: String::from("f"),
        }));
        assert_ser_de(&key_down, json!({"type": "keyDown", "value": "f"}));
    }

    #[test]
    fn test_json_key_action_item_invalid_type() {
        assert!(serde_json::from_value::<KeyActionItem>(json!({"type": "invalid"})).is_err());
    }

    #[test]
    fn test_json_key_action_missing_subtype() {
        assert!(serde_json::from_value::<KeyAction>(json!({"value": "f"})).is_err());
    }

    #[test]
    fn test_json_key_action_wrong_subtype() {
        let json = json!({"type": "pause", "value": "f"});
        assert!(serde_json::from_value::<KeyAction>(json).is_err());
    }

    #[test]
    fn test_json_key_action_down() {
        let key_down = KeyAction::Down(KeyDownAction {
            value: "f".to_string(),
        });
        assert_ser_de(&key_down, json!({"type": "keyDown", "value": "f"}));
    }

    #[test]
    fn test_json_key_action_down_with_value_unicode() {
        let key_down = KeyAction::Down(KeyDownAction {
            value: "à".to_string(),
        });
        assert_ser_de(&key_down, json!({"type": "keyDown", "value": "à"}));
    }

    #[test]
    fn test_json_key_action_down_with_value_unicode_encoded() {
        let key_down = KeyAction::Down(KeyDownAction {
            value: "à".to_string(),
        });
        assert_de(&key_down, json!({"type": "keyDown", "value": "\u{00E0}"}));
    }

    #[test]
    fn test_json_key_action_down_with_value_missing() {
        assert!(serde_json::from_value::<KeyAction>(json!({"type": "keyDown"})).is_err());
    }

    #[test]
    fn test_json_key_action_down_with_value_null() {
        let json = json!({"type": "keyDown", "value": null});
        assert!(serde_json::from_value::<KeyAction>(json).is_err());
    }

    #[test]
    fn test_json_key_action_down_with_value_invalid_type() {
        let json = json!({"type": "keyDown", "value": ["f", "o", "o"]});
        assert!(serde_json::from_value::<KeyAction>(json).is_err());
    }

    #[test]
    fn test_json_key_action_down_with_multiple_code_points() {
        let json = json!({"type": "keyDown", "value": "fo"});
        assert!(serde_json::from_value::<KeyAction>(json).is_err());
    }

    #[test]
    fn test_json_key_action_up() {
        let key_up = KeyAction::Up(KeyUpAction {
            value: "f".to_string(),
        });
        assert_ser_de(&key_up, json!({"type": "keyUp", "value": "f"}));
    }

    #[test]
    fn test_json_key_action_up_with_value_unicode() {
        let key_up = KeyAction::Up(KeyUpAction {
            value: "à".to_string(),
        });
        assert_ser_de(&key_up, json!({"type":"keyUp", "value": "à"}));
    }

    #[test]
    fn test_json_key_action_up_with_value_unicode_encoded() {
        let key_up = KeyAction::Up(KeyUpAction {
            value: "à".to_string(),
        });
        assert_de(&key_up, json!({"type": "keyUp", "value": "\u{00E0}"}));
    }

    #[test]
    fn test_json_key_action_up_with_value_missing() {
        assert!(serde_json::from_value::<KeyAction>(json!({"type": "keyUp"})).is_err());
    }

    #[test]
    fn test_json_key_action_up_with_value_null() {
        let json = json!({"type": "keyUp", "value": null});
        assert!(serde_json::from_value::<KeyAction>(json).is_err());
    }

    #[test]
    fn test_json_key_action_up_with_value_invalid_type() {
        let json = json!({"type": "keyUp", "value": ["f","o","o"]});
        assert!(serde_json::from_value::<KeyAction>(json).is_err());
    }

    #[test]
    fn test_json_key_action_up_with_multiple_code_points() {
        let json = json!({"type": "keyUp", "value": "fo"});
        assert!(serde_json::from_value::<KeyAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_item_general() {
        let pause =
            PointerActionItem::General(GeneralAction::Pause(PauseAction { duration: Some(1) }));
        assert_ser_de(&pause, json!({"type": "pause", "duration": 1}));
    }

    #[test]
    fn test_json_pointer_action_item_pointer() {
        let cancel = PointerActionItem::Pointer(PointerAction::Cancel);
        assert_ser_de(&cancel, json!({"type": "pointerCancel"}));
    }

    #[test]
    fn test_json_pointer_action_item_invalid() {
        assert!(serde_json::from_value::<PointerActionItem>(json!({"type": "invalid"})).is_err());
    }

    #[test]
    fn test_json_pointer_action_parameters_mouse() {
        let mouse = PointerActionParameters {
            pointer_type: PointerType::Mouse,
        };
        assert_ser_de(&mouse, json!({"pointerType": "mouse"}));
    }

    #[test]
    fn test_json_pointer_action_parameters_pen() {
        let pen = PointerActionParameters {
            pointer_type: PointerType::Pen,
        };
        assert_ser_de(&pen, json!({"pointerType": "pen"}));
    }

    #[test]
    fn test_json_pointer_action_parameters_touch() {
        let touch = PointerActionParameters {
            pointer_type: PointerType::Touch,
        };
        assert_ser_de(&touch, json!({"pointerType": "touch"}));
    }

    #[test]
    fn test_json_pointer_action_item_invalid_type() {
        let json = json!({"type": "pointerInvalid"});
        assert!(serde_json::from_value::<PointerActionItem>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_missing_subtype() {
        assert!(serde_json::from_value::<PointerAction>(json!({"button": 1})).is_err());
    }

    #[test]
    fn test_json_pointer_action_invalid_subtype() {
        let json = json!({"type": "invalid", "button": 1});
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_cancel() {
        assert_ser_de(&PointerAction::Cancel, json!({"type": "pointerCancel"}));
    }

    #[test]
    fn test_json_pointer_action_down() {
        let pointer_down = PointerAction::Down(PointerDownAction {
            button: 1,
            ..Default::default()
        });
        assert_ser_de(&pointer_down, json!({"type": "pointerDown", "button": 1}));
    }

    #[test]
    fn test_json_pointer_action_down_with_button_missing() {
        let json = json!({"type": "pointerDown"});
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_down_with_button_null() {
        let json = json!({
            "type": "pointerDown",
            "button": null,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_down_with_button_invalid_type() {
        let json = json!({
            "type": "pointerDown",
            "button": "foo",
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_down_with_button_negative() {
        let json = json!({
            "type": "pointerDown",
            "button": -30,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": "viewport",
            "x": 5,
            "y": 10,
        });
        let pointer_move = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: Some(10),
            ..Default::default()
        });

        assert_ser_de(&pointer_move, json);
    }

    #[test]
    fn test_json_pointer_action_move_missing_subtype() {
        let json = json!({
            "duration": 100,
            "origin": "viewport",
            "x": 5,
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_wrong_subtype() {
        let json = json!({
            "type": "pointerUp",
            "duration": 100,
            "origin": "viewport",
            "x": 5,
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_missing() {
        let json = json!({
            "type": "pointerMove",
            "origin": "viewport",
            "x": 5,
            "y": 10,
        });
        let pointer_move = PointerAction::Move(PointerMoveAction {
            duration: None,
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: Some(10),
            ..Default::default()
        });

        assert_ser_de(&pointer_move, json);
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_null() {
        let json = json!({
            "type": "pointerMove",
            "duration": null,
            "origin": "viewport",
            "x": 5,
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_invalid_type() {
        let json = json!({
            "type": "pointerMove",
            "duration": "invalid",
            "origin": "viewport",
            "x": 5,
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_negative() {
        let json = json!({
            "type": "pointerMove",
            "duration": -30,
            "origin": "viewport",
            "x": 5,
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_missing() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "x": 5,
            "y": 10,
        });
        let pointer_move = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: Some(10),
            ..Default::default()
        });

        assert_de(&pointer_move, json);
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_webelement() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": {ELEMENT_KEY: "elem"},
            "x": 5,
            "y": 10,
        });
        let pointer_move = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Element(WebElement("elem".into())),
            x: Some(5),
            y: Some(10),
            ..Default::default()
        });

        assert_ser_de(&pointer_move, json);
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_webelement_and_legacy_element() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": {ELEMENT_KEY: "elem"},
            "x": 5,
            "y": 10,
        });
        let pointer_move = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Element(WebElement("elem".into())),
            x: Some(5),
            y: Some(10),
            ..Default::default()
        });

        assert_de(&pointer_move, json);
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_only_legacy_element() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": {ELEMENT_KEY: "elem"},
            "x": 5,
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerOrigin>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_x_missing() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": "viewport",
            "y": 10,
        });
        let pointer_move = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: None,
            y: Some(10),
            ..Default::default()
        });

        assert_ser_de(&pointer_move, json);
    }

    #[test]
    fn test_json_pointer_action_move_with_x_null() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": "viewport",
            "x": null,
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_x_invalid_type() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": "viewport",
            "x": "invalid",
            "y": 10,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_y_missing() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": "viewport",
            "x": 5,
        });
        let pointer_move = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: None,
            ..Default::default()
        });

        assert_ser_de(&pointer_move, json);
    }

    #[test]
    fn test_json_pointer_action_move_with_y_null() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": "viewport",
            "x": 5,
            "y": null,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_y_invalid_type() {
        let json = json!({
            "type": "pointerMove",
            "duration": 100,
            "origin": "viewport",
            "x": 5,
            "y": "invalid",
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_up() {
        let pointer_up = PointerAction::Up(PointerUpAction {
            button: 1,
            ..Default::default()
        });
        assert_ser_de(&pointer_up, json!({"type": "pointerUp", "button": 1}));
    }

    #[test]
    fn test_json_pointer_action_up_with_button_missing() {
        assert!(serde_json::from_value::<PointerAction>(json!({"type": "pointerUp"})).is_err());
    }

    #[test]
    fn test_json_pointer_action_up_with_button_null() {
        let json = json!({
            "type": "pointerUp",
            "button": null,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_up_with_button_invalid_type() {
        let json = json!({
            "type": "pointerUp",
            "button": "foo",
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_action_up_with_button_negative() {
        let json = json!({
            "type": "pointerUp",
            "button": -30,
        });
        assert!(serde_json::from_value::<PointerAction>(json).is_err());
    }

    #[test]
    fn test_json_pointer_origin_pointer() {
        assert_ser_de(&PointerOrigin::Pointer, json!("pointer"));
    }

    #[test]
    fn test_json_pointer_origin_viewport() {
        assert_ser_de(&PointerOrigin::Viewport, json!("viewport"));
    }

    #[test]
    fn test_json_pointer_origin_web_element() {
        let element = PointerOrigin::Element(WebElement("elem".into()));
        assert_ser_de(&element, json!({ELEMENT_KEY: "elem"}));
    }

    #[test]
    fn test_json_pointer_origin_invalid_type() {
        assert!(serde_json::from_value::<PointerOrigin>(json!("invalid")).is_err());
    }

    #[test]
    fn test_json_pointer_type_mouse() {
        assert_ser_de(&PointerType::Mouse, json!("mouse"));
    }

    #[test]
    fn test_json_pointer_type_pen() {
        assert_ser_de(&PointerType::Pen, json!("pen"));
    }

    #[test]
    fn test_json_pointer_type_touch() {
        assert_ser_de(&PointerType::Touch, json!("touch"));
    }

    #[test]
    fn test_json_pointer_type_invalid_type() {
        assert!(serde_json::from_value::<PointerType>(json!("invalid")).is_err());
    }

    #[test]
    fn test_pointer_properties() {
        // Ideally these would be seperate tests, but it was too much boilerplate to write
        // and adding a macro seemed like overkill.
        for actionType in ["pointerUp", "pointerDown", "pointerMove"] {
            for (prop_name, value, is_valid) in [
                ("pressure", Value::from(0), true),
                ("pressure", Value::from(0.5), true),
                ("pressure", Value::from(1), true),
                ("pressure", Value::from(1.1), false),
                ("pressure", Value::from(-0.1), false),
                ("tangentialPressure", Value::from(-1), true),
                ("tangentialPressure", Value::from(0), true),
                ("tangentialPressure", Value::from(1.0), true),
                ("tangentialPressure", Value::from(-1.1), false),
                ("tangentialPressure", Value::from(1.1), false),
                ("tiltX", Value::from(-90), true),
                ("tiltX", Value::from(0), true),
                ("tiltX", Value::from(45), true),
                ("tiltX", Value::from(90), true),
                ("tiltX", Value::from(0.5), false),
                ("tiltX", Value::from(-91), false),
                ("tiltX", Value::from(91), false),
                ("tiltY", Value::from(-90), true),
                ("tiltY", Value::from(0), true),
                ("tiltY", Value::from(45), true),
                ("tiltY", Value::from(90), true),
                ("tiltY", Value::from(0.5), false),
                ("tiltY", Value::from(-91), false),
                ("tiltY", Value::from(91), false),
                ("twist", Value::from(0), true),
                ("twist", Value::from(180), true),
                ("twist", Value::from(359), true),
                ("twist", Value::from(360), false),
                ("twist", Value::from(-1), false),
                ("twist", Value::from(23.5), false),
                ("altitudeAngle", Value::from(0), true),
                ("altitudeAngle", Value::from(f64::consts::FRAC_PI_4), true),
                ("altitudeAngle", Value::from(f64::consts::FRAC_PI_2), true),
                (
                    "altitudeAngle",
                    Value::from(f64::consts::FRAC_PI_2 + 0.1),
                    false,
                ),
                ("altitudeAngle", Value::from(-f64::consts::FRAC_PI_4), false),
                ("azimuthAngle", Value::from(0), true),
                ("azimuthAngle", Value::from(f64::consts::PI), true),
                ("azimuthAngle", Value::from(f64::consts::TAU), true),
                ("azimuthAngle", Value::from(f64::consts::TAU + 0.01), false),
                ("azimuthAngle", Value::from(-f64::consts::FRAC_PI_4), false),
            ] {
                let mut json = serde_json::Map::new();
                json.insert("type".into(), actionType.into());
                if actionType != "pointerMove" {
                    json.insert("button".into(), Value::from(0));
                }
                json.insert(prop_name.into(), value);
                println!("{:?}", json);
                let deserialized = serde_json::from_value::<PointerAction>(json.into());
                if is_valid {
                    assert!(deserialized.is_ok());
                } else {
                    assert!(deserialized.is_err());
                }
            }
        }
    }
}
