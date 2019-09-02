use serde::de::{self, SeqAccess, Unexpected, Visitor};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use serde_json::{Map, Value};
use serde_repr::{Deserialize_repr, Serialize_repr};
use std::fmt;

use crate::error::MarionetteError;
use crate::marionette;
use crate::result::MarionetteResult;
use crate::webdriver;

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum Command {
    WebDriver(webdriver::Command),
    Marionette(marionette::Command),
}

impl Command {
    pub fn name(&self) -> String {
        let (command_name, _) = self.first_entry();
        command_name
    }

    fn params(&self) -> Value {
        let (_, params) = self.first_entry();
        params
    }

    fn first_entry(&self) -> (String, serde_json::Value) {
        match serde_json::to_value(&self).unwrap() {
            Value::String(cmd) => (cmd, Value::Object(Map::new())),
            Value::Object(items) => {
                let mut iter = items.iter();
                let (cmd, params) = iter.next().unwrap();
                (cmd.to_string(), params.clone())
            }
            _ => unreachable!(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize_repr, Deserialize_repr)]
#[repr(u8)]
enum MessageDirection {
    Incoming = 0,
    Outgoing = 1,
}

pub type MessageId = u32;

#[derive(Debug, Clone, PartialEq)]
pub struct Request(pub MessageId, pub Command);

impl Request {
    pub fn id(&self) -> MessageId {
        self.0
    }

    pub fn command(&self) -> &Command {
        &self.1
    }

    pub fn params(&self) -> Value {
        self.command().params()
    }
}

impl Serialize for Request {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        (
            MessageDirection::Incoming,
            self.id(),
            self.command().name(),
            self.params(),
        )
            .serialize(serializer)
    }
}

#[derive(Debug, PartialEq)]
pub enum Response {
    Result {
        id: MessageId,
        result: MarionetteResult,
    },
    Error {
        id: MessageId,
        error: MarionetteError,
    },
}

impl Serialize for Response {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            Response::Result { id, result } => {
                (MessageDirection::Outgoing, id, Value::Null, &result).serialize(serializer)
            }
            Response::Error { id, error } => {
                (MessageDirection::Outgoing, id, &error, Value::Null).serialize(serializer)
            }
        }
    }
}

#[derive(Debug, PartialEq, Serialize)]
#[serde(untagged)]
pub enum Message {
    Incoming(Request),
    Outgoing(Response),
}

struct MessageVisitor;

impl<'de> Visitor<'de> for MessageVisitor {
    type Value = Message;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("four-element array")
    }

    fn visit_seq<A: SeqAccess<'de>>(self, mut seq: A) -> Result<Self::Value, A::Error> {
        let direction = seq
            .next_element::<MessageDirection>()?
            .ok_or_else(|| de::Error::invalid_length(0, &self))?;
        let id: MessageId = seq
            .next_element()?
            .ok_or_else(|| de::Error::invalid_length(1, &self))?;

        let msg = match direction {
            MessageDirection::Incoming => {
                let name: String = seq
                    .next_element()?
                    .ok_or_else(|| de::Error::invalid_length(2, &self))?;
                let params: Value = seq
                    .next_element()?
                    .ok_or_else(|| de::Error::invalid_length(3, &self))?;

                let command = match params {
                    Value::Object(ref items) if !items.is_empty() => {
                        let command_to_params = {
                            let mut m = Map::new();
                            m.insert(name, params);
                            Value::Object(m)
                        };
                        serde_json::from_value(command_to_params).map_err(de::Error::custom)
                    }
                    Value::Object(_) | Value::Null => {
                        serde_json::from_value(Value::String(name)).map_err(de::Error::custom)
                    }
                    x => Err(de::Error::custom(format!("unknown params type: {}", x))),
                }?;
                Message::Incoming(Request(id, command))
            }

            MessageDirection::Outgoing => {
                let maybe_error: Option<MarionetteError> = seq
                    .next_element()?
                    .ok_or_else(|| de::Error::invalid_length(2, &self))?;

                let response = if let Some(error) = maybe_error {
                    seq.next_element::<Value>()?
                        .ok_or_else(|| de::Error::invalid_length(3, &self))?
                        .as_null()
                        .ok_or_else(|| de::Error::invalid_type(Unexpected::Unit, &self))?;
                    Response::Error { id, error }
                } else {
                    let result: MarionetteResult = seq
                        .next_element()?
                        .ok_or_else(|| de::Error::invalid_length(3, &self))?;
                    Response::Result { id, result }
                };

                Message::Outgoing(response)
            }
        };

        Ok(msg)
    }
}

impl<'de> Deserialize<'de> for Message {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_seq(MessageVisitor)
    }
}

#[cfg(test)]
mod tests {
    use serde_json::json;

    use super::*;

    use crate::common::*;
    use crate::error::{ErrorKind, MarionetteError};
    use crate::test::assert_ser_de;

    #[test]
    fn test_incoming() {
        let json =
            json!([0, 42, "WebDriver:FindElement", {"using": "css selector", "value": "value"}]);
        let find_element = webdriver::Command::FindElement(webdriver::Locator {
            using: webdriver::Selector::CSS,
            value: "value".into(),
        });
        let req = Request(42, Command::WebDriver(find_element));
        let msg = Message::Incoming(req);
        assert_ser_de(&msg, json);
    }

    #[test]
    fn test_incoming_empty_params() {
        let json = json!([0, 42, "WebDriver:GetTimeouts", {}]);
        let req = Request(42, Command::WebDriver(webdriver::Command::GetTimeouts));
        let msg = Message::Incoming(req);
        assert_ser_de(&msg, json);
    }

    #[test]
    fn test_incoming_common_params() {
        let json = json!([0, 42, "Marionette:AcceptConnections", {"value": false}]);
        let params = BoolValue::new(false);
        let req = Request(
            42,
            Command::Marionette(marionette::Command::AcceptConnections(params)),
        );
        let msg = Message::Incoming(req);
        assert_ser_de(&msg, json);
    }

    #[test]
    fn test_incoming_params_derived() {
        assert!(serde_json::from_value::<Message>(
            json!([0,42,"WebDriver:FindElement",{"using":"foo","value":"foo"}])
        )
        .is_err());
        assert!(serde_json::from_value::<Message>(
            json!([0,42,"Marionette:AcceptConnections",{"value":"foo"}])
        )
        .is_err());
    }

    #[test]
    fn test_incoming_no_params() {
        assert!(serde_json::from_value::<Message>(
            json!([0,42,"WebDriver:GetTimeouts",{"value":true}])
        )
        .is_err());
        assert!(serde_json::from_value::<Message>(
            json!([0,42,"Marionette:Context",{"value":"foo"}])
        )
        .is_err());
        assert!(serde_json::from_value::<Message>(
            json!([0,42,"Marionette:GetScreenOrientation",{"value":true}])
        )
        .is_err());
    }

    #[test]
    fn test_outgoing_result() {
        let json = json!([1, 42, null, { "value": null }]);
        let result = MarionetteResult::Null;
        let msg = Message::Outgoing(Response::Result { id: 42, result });

        assert_ser_de(&msg, json);
    }

    #[test]
    fn test_outgoing_error() {
        let json =
            json!([1, 42, {"error": "no such element", "message": "", "stacktrace": ""}, null]);
        let error = MarionetteError {
            kind: ErrorKind::NoSuchElement,
            message: "".into(),
            stack: "".into(),
        };
        let msg = Message::Outgoing(Response::Error { id: 42, error });

        assert_ser_de(&msg, json);
    }

    #[test]
    fn test_invalid_type() {
        assert!(
            serde_json::from_value::<Message>(json!([2, 42, "WebDriver:GetTimeouts", {}])).is_err()
        );
        assert!(serde_json::from_value::<Message>(json!([3, 42, "no such element", {}])).is_err());
    }

    #[test]
    fn test_missing_fields() {
        // all fields are required
        assert!(
            serde_json::from_value::<Message>(json!([2, 42, "WebDriver:GetTimeouts"])).is_err()
        );
        assert!(serde_json::from_value::<Message>(json!([2, 42])).is_err());
        assert!(serde_json::from_value::<Message>(json!([2])).is_err());
        assert!(serde_json::from_value::<Message>(json!([])).is_err());
    }

    #[test]
    fn test_unknown_command() {
        assert!(serde_json::from_value::<Message>(json!([0, 42, "hooba", {}])).is_err());
    }

    #[test]
    fn test_unknown_error() {
        assert!(serde_json::from_value::<Message>(json!([1, 42, "flooba", {}])).is_err());
    }

    #[test]
    fn test_message_id_bounds() {
        let overflow = i64::from(std::u32::MAX) + 1;
        let underflow = -1;

        fn get_timeouts(message_id: i64) -> Value {
            json!([0, message_id, "WebDriver:GetTimeouts", {}])
        }

        assert!(serde_json::from_value::<Message>(get_timeouts(overflow)).is_err());
        assert!(serde_json::from_value::<Message>(get_timeouts(underflow)).is_err());
    }
}
