use common::{WebElement, ELEMENT_KEY};
use serde::de::{self, Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use serde_json::Value;
use std::default::Default;
use unicode_segmentation::UnicodeSegmentation;

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct ActionSequence {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub id: Option<String>,
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
        if value.graphemes(true).collect::<Vec<&str>>().len() != 1 {
            return Err(de::Error::custom(format!(
                "'{}' should only contain a single Unicode code point",
                value
            )));
        }

        Ok(value)
    })?
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
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

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PointerDownAction {
    pub button: u64,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
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
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PointerUpAction {
    pub button: u64,
}

#[derive(Clone, Debug, PartialEq, Serialize)]
pub enum PointerOrigin {
    #[serde(
        rename = "element-6066-11e4-a52e-4f735466cecf", serialize_with = "serialize_webelement_id"
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
                .map(|id| PointerOrigin::Element(WebElement { id }))
                .map_err(de::Error::custom)
        } else if value == "pointer" {
            Ok(PointerOrigin::Pointer)
        } else if value == "viewport" {
            Ok(PointerOrigin::Viewport)
        } else {
            Err(de::Error::custom(format!(
                "unknown value `{}`, expected `pointer`, `viewport`, or `element-6066-11e4-a52e-4f735466cecf`",
                value.to_string()
            )))
        }
    }
}

fn serialize_webelement_id<S>(element: &WebElement, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    element.id.serialize(serializer)
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

#[cfg(test)]
mod test {
    use super::*;
    use serde_json;
    use test::{check_deserialize, check_serialize_deserialize};

    #[test]
    fn test_json_action_sequence_null() {
        let json = r#"{
            "id":"none",
            "type":"none",
            "actions":[{
                "type":"pause","duration":1
            }]
        }"#;
        let data = ActionSequence {
            id: Some("none".into()),
            actions: ActionsType::Null {
                actions: vec![NullActionItem::General(GeneralAction::Pause(PauseAction {
                    duration: Some(1),
                }))],
            },
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_action_sequence_key() {
        let json = r#"{
            "id":"some_key",
            "type":"key",
            "actions":[
                {"type":"keyDown","value":"f"}
            ]
        }"#;
        let data = ActionSequence {
            id: Some("some_key".into()),
            actions: ActionsType::Key {
                actions: vec![KeyActionItem::Key(KeyAction::Down(KeyDownAction {
                    value: String::from("f"),
                }))],
            },
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_action_sequence_pointer() {
        let json = r#"{
            "id":"some_pointer",
            "type":"pointer",
            "parameters":{
                "pointerType":"mouse"
            },
            "actions":[
                {"type":"pointerDown","button":0},
                {"type":"pointerMove","origin":"pointer","x":10,"y":20},
                {"type":"pointerUp","button":0}
            ]
        }"#;
        let data = ActionSequence {
            id: Some("some_pointer".into()),
            actions: ActionsType::Pointer {
                parameters: PointerActionParameters {
                    pointer_type: PointerType::Mouse,
                },
                actions: vec![
                    PointerActionItem::Pointer(PointerAction::Down(PointerDownAction {
                        button: 0,
                    })),
                    PointerActionItem::Pointer(PointerAction::Move(PointerMoveAction {
                        origin: PointerOrigin::Pointer,
                        duration: None,
                        x: Some(10),
                        y: Some(20),
                    })),
                    PointerActionItem::Pointer(PointerAction::Up(PointerUpAction { button: 0 })),
                ],
            },
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_action_sequence_actions_missing() {
        let json = r#"{
            "id": "3"
        }"#;

        assert!(serde_json::from_str::<ActionSequence>(&json).is_err());
    }

    #[test]
    fn test_json_action_sequence_actions_null() {
        let json = r#"{
            "id": "3",
            "actions": null
        }"#;

        assert!(serde_json::from_str::<ActionSequence>(&json).is_err());
    }

    #[test]
    fn test_json_action_sequence_actions_invalid_type() {
        let json = r#"{
            "id": "3",
            "actions": "foo"
        }"#;

        assert!(serde_json::from_str::<ActionSequence>(&json).is_err());
    }

    #[test]
    fn test_json_actions_type_null() {
        let json = r#"{
            "type":"none",
            "actions":[{
                "type":"pause",
                "duration":1
            }]
        }"#;
        let data = ActionsType::Null {
            actions: vec![NullActionItem::General(GeneralAction::Pause(PauseAction {
                duration: Some(1),
            }))],
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_actions_type_key() {
        let json = r#"{
            "type":"key",
            "actions":[{
                "type":"keyDown",
                "value":"f"
            }]
        }"#;
        let data = ActionsType::Key {
            actions: vec![KeyActionItem::Key(KeyAction::Down(KeyDownAction {
                value: String::from("f"),
            }))],
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_actions_type_pointer() {
        let json = r#"{
            "type":"pointer",
            "parameters":{"pointerType":"mouse"},
            "actions":[
                {"type":"pointerDown","button":1}
            ]}"#;
        let data = ActionsType::Pointer {
            parameters: PointerActionParameters {
                pointer_type: PointerType::Mouse,
            },
            actions: vec![PointerActionItem::Pointer(PointerAction::Down(
                PointerDownAction { button: 1 },
            ))],
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_actions_type_pointer_with_parameters_missing() {
        let json = r#"{
            "type":"pointer",
            "actions":[
                {"type":"pointerDown","button":1}
            ]}"#;
        let data = ActionsType::Pointer {
            parameters: PointerActionParameters {
                pointer_type: PointerType::Mouse,
            },
            actions: vec![PointerActionItem::Pointer(PointerAction::Down(
                PointerDownAction { button: 1 },
            ))],
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_actions_type_pointer_with_parameters_invalid_type() {
        let json = r#"{
            "type":"pointer",
            "parameters":null,
            "actions":[
                {"type":"pointerDown","button":1}
            ]}"#;

        assert!(serde_json::from_str::<ActionsType>(&json).is_err());
    }

    #[test]
    fn test_json_actions_type_invalid() {
        let json = r#"{"actions":[{"foo":"bar"}]}"#;
        assert!(serde_json::from_str::<ActionsType>(&json).is_err());
    }

    #[test]
    fn test_json_null_action_item_general() {
        let json = r#"{"type":"pause","duration":1}"#;
        let data = NullActionItem::General(GeneralAction::Pause(PauseAction { duration: Some(1) }));

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_null_action_item_invalid_type() {
        let json = r#"{"type":"invalid"}"#;
        assert!(serde_json::from_str::<NullActionItem>(&json).is_err());
    }

    #[test]
    fn test_json_general_action_pause() {
        let json = r#"{"type":"pause","duration":1}"#;
        let data = GeneralAction::Pause(PauseAction { duration: Some(1) });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_general_action_pause_with_duration_missing() {
        let json = r#"{"type":"pause"}"#;
        let data = GeneralAction::Pause(PauseAction { duration: None });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_general_action_pause_with_duration_null() {
        let json = r#"{"type":"pause","duration":null}"#;

        assert!(serde_json::from_str::<GeneralAction>(&json).is_err());
    }

    #[test]
    fn test_json_general_action_pause_with_duration_invalid_type() {
        let json = r#"{"type":"pause","duration":"foo"}"#;

        assert!(serde_json::from_str::<GeneralAction>(&json).is_err());
    }

    #[test]
    fn test_json_general_action_pause_with_duration_negative() {
        let json = r#"{"type":"pause","duration":-30}"#;

        assert!(serde_json::from_str::<GeneralAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_item_general() {
        let json = r#"{"type":"pause","duration":1}"#;
        let data = KeyActionItem::General(GeneralAction::Pause(PauseAction { duration: Some(1) }));

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_item_key() {
        let json = r#"{"type":"keyDown","value":"f"}"#;
        let data = KeyActionItem::Key(KeyAction::Down(KeyDownAction {
            value: String::from("f"),
        }));

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_item_invalid_type() {
        let json = r#"{"type":"invalid"}"#;
        assert!(serde_json::from_str::<KeyActionItem>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_missing_subtype() {
        let json = r#"{"value":"f"}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_wrong_subtype() {
        let json = r#"{"type":"pause","value":"f"}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_down() {
        let json = r#"{"type":"keyDown","value":"f"}"#;
        let data = KeyAction::Down(KeyDownAction {
            value: "f".to_owned(),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_down_with_value_unicode() {
        let json = r#"{"type":"keyDown","value":"à"}"#;
        let data = KeyAction::Down(KeyDownAction {
            value: "à".to_owned(),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_down_with_value_unicode_encoded() {
        let json = r#"{"type":"keyDown","value":"\u00E0"}"#;
        let data = KeyAction::Down(KeyDownAction {
            value: "à".to_owned(),
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_down_with_value_missing() {
        let json = r#"{"type":"keyDown"}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_down_with_value_null() {
        let json = r#"{"type":"keyDown","value":null}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_down_with_value_invalid_type() {
        let json = r#"{"type":"keyDown,"value":["f","o","o"]}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_down_with_multiple_code_points() {
        let json = r#"{"type":"keyDown","value":"fo"}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_up() {
        let json = r#"{"type":"keyUp","value":"f"}"#;
        let data = KeyAction::Up(KeyUpAction {
            value: "f".to_owned(),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_up_with_value_unicode() {
        let json = r#"{"type":"keyUp","value":"à"}"#;
        let data = KeyAction::Up(KeyUpAction {
            value: "à".to_owned(),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_up_with_value_unicode_encoded() {
        let json = r#"{"type":"keyUp","value":"\u00E0"}"#;
        let data = KeyAction::Up(KeyUpAction {
            value: "à".to_owned(),
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_key_action_up_with_value_missing() {
        let json = r#"{"type":"keyUp"}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_up_with_value_null() {
        let json = r#"{"type":"keyUp,"value":null}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_up_with_value_invalid_type() {
        let json = r#"{"type":"keyUp,"value":["f","o","o"]}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_key_action_up_with_multiple_code_points() {
        let json = r#"{"type":"keyUp","value":"fo"}"#;

        assert!(serde_json::from_str::<KeyAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_item_general() {
        let json = r#"{"type":"pause","duration":1}"#;
        let data = PointerActionItem::General(GeneralAction::Pause(PauseAction { duration: Some(1) }));

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_item_pointer() {
        let json = r#"{"type":"pointerCancel"}"#;
        let data = PointerActionItem::Pointer(PointerAction::Cancel);

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_item_invalid() {
        let json = r#"{"type":"invalid"}"#;

        assert!(serde_json::from_str::<PointerActionItem>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_parameters_mouse() {
        let json = r#"{"pointerType":"mouse"}"#;
        let data = PointerActionParameters {
            pointer_type: PointerType::Mouse,
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_parameters_pen() {
        let json = r#"{"pointerType":"pen"}"#;
        let data = PointerActionParameters {
            pointer_type: PointerType::Pen,
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_parameters_touch() {
        let json = r#"{"pointerType":"touch"}"#;
        let data = PointerActionParameters {
            pointer_type: PointerType::Touch,
        };

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_item_invalid_type() {
        let json = r#"{"type":"pointerInvalid"}"#;
        assert!(serde_json::from_str::<PointerActionItem>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_with_subtype_missing() {
        let json = r#"{"button":1}"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_with_subtype_invalid() {
        let json = r#"{"type":"invalid"}"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_with_subtype_wrong() {
        let json = r#"{"type":"pointerMove",button":1}"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_cancel() {
        let json = r#"{"type":"pointerCancel"}"#;
        let data = PointerAction::Cancel;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_down() {
        let json = r#"{"type":"pointerDown","button":1}"#;
        let data = PointerAction::Down(PointerDownAction { button: 1 });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_down_with_button_missing() {
        let json = r#"{"type":"pointerDown"}"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_down_with_button_null() {
        let json = r#"{
            "type":"pointerDown",
            "button":null
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_down_with_button_invalid_type() {
        let json = r#"{
            "type":"pointerDown",
            "button":"foo",
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_down_with_button_negative() {
        let json = r#"{
            "type":"pointerDown",
            "button":-30
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":"viewport",
            "x":5,
            "y":10
        }"#;
        let data = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: Some(10),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_move_missing_subtype() {
        let json = r#"{
            "duration":100,
            "origin":"viewport",
            "x":5,
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_wrong_subtype() {
        let json = r#"{
            "type":"pointerUp",
            "duration":100,
            "origin":"viewport",
            "x":5,
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_missing() {
        let json = r#"{
            "type":"pointerMove",
            "origin":"viewport",
            "x":5,
            "y":10
        }"#;
        let data = PointerAction::Move(PointerMoveAction {
            duration: None,
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: Some(10),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_null() {
        let json = r#"{
            "type":"pointerMove",
            "duration":null,
            "origin":"viewport",
            "x":5,
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_invalid_type() {
        let json = r#"{
            "type":"pointerMove",
            "duration":"invalid",
            "origin":"viewport",
            "x":5,
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_duration_negative() {
        let json = r#"{
            "type":"pointerMove",
            "duration":-30,
            "origin":"viewport",
            "x":5,
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_missing() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "x":5,
            "y":10
        }"#;
        let data = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: Some(10),
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_webelement() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":{
                "element-6066-11e4-a52e-4f735466cecf":"elem"
            },
            "x":5,
            "y":10
        }"#;
        let data = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Element(WebElement { id: "elem".into() }),
            x: Some(5),
            y: Some(10),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_webelement_and_legacy_element() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":{
                "ELEMENT":"elem",
                "element-6066-11e4-a52e-4f735466cecf":"elem"
            },
            "x":5,
            "y":10
        }"#;
        let data = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Element(WebElement { id: "elem".into() }),
            x: Some(5),
            y: Some(10),
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_move_with_origin_only_legacy_element() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":{
                "ELEMENT":"elem"
            },
            "x":5,
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerOrigin>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_x_missing() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":"viewport",
            "y":10
        }"#;
        let data = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: None,
            y: Some(10),
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_move_with_x_null() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":"viewport",
            "x": null,
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_x_invalid_type() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":"viewport",
            "x": "invalid",
            "y":10
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_y_missing() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":"viewport",
            "x":5
        }"#;
        let data = PointerAction::Move(PointerMoveAction {
            duration: Some(100),
            origin: PointerOrigin::Viewport,
            x: Some(5),
            y: None,
        });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_move_with_y_null() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":"viewport",
            "x":5,
            "y":null
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_move_with_y_invalid_type() {
        let json = r#"{
            "type":"pointerMove",
            "duration":100,
            "origin":"viewport",
            "x":5,
            "y":"invalid"
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_up() {
        let json = r#"{
            "type":"pointerUp",
            "button":1
        }"#;
        let data = PointerAction::Up(PointerUpAction { button: 1 });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_action_up_with_button_missing() {
        let json = r#"{
            "type":"pointerUp"
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_up_with_button_null() {
        let json = r#"{
            "type":"pointerUp",
            "button":null
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_up_with_button_invalid_type() {
        let json = r#"{
            "type":"pointerUp",
            "button":"foo",
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_action_up_with_button_negative() {
        let json = r#"{
            "type":"pointerUp",
            "button":-30
        }"#;

        assert!(serde_json::from_str::<PointerAction>(&json).is_err());
    }

    #[test]
    fn test_json_pointer_origin_pointer() {
        let json = r#""pointer""#;
        let data = PointerOrigin::Pointer;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_origin_viewport() {
        let json = r#""viewport""#;
        let data = PointerOrigin::Viewport;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_origin_web_element() {
        let json = r#"{"element-6066-11e4-a52e-4f735466cecf":"elem"}"#;
        let data = PointerOrigin::Element(WebElement { id: "elem".into() });

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_origin_invalid_type() {
        let data = r#""invalid""#;
        assert!(serde_json::from_str::<PointerOrigin>(&data).is_err());
    }

    #[test]
    fn test_json_pointer_type_mouse() {
        let json = r#""mouse""#;
        let data = PointerType::Mouse;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_type_pen() {
        let json = r#""pen""#;
        let data = PointerType::Pen;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_type_touch() {
        let json = r#""touch""#;
        let data = PointerType::Touch;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_pointer_type_invalid_type() {
        let json = r#""invalid""#;
        assert!(serde_json::from_str::<PointerType>(&json).is_err());
    }
}
