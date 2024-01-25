/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::ser::SerializeMap;
use serde::{de, Deserialize, Deserializer, Serialize, Serializer};
use serde_json::Value;

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct BoolValue {
    value: bool,
}

impl BoolValue {
    pub fn new(val: bool) -> Self {
        BoolValue { value: val }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Cookie {
    pub name: String,
    pub value: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub path: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub domain: Option<String>,
    #[serde(default)]
    pub secure: bool,
    #[serde(default, rename = "httpOnly")]
    pub http_only: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub expiry: Option<Date>,
    #[serde(skip_serializing_if = "Option::is_none", rename = "sameSite")]
    pub same_site: Option<String>,
}

pub fn to_cookie<T, S>(data: T, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
    T: Serialize,
{
    #[derive(Serialize)]
    struct Wrapper<T> {
        cookie: T,
    }

    Wrapper { cookie: data }.serialize(serializer)
}

pub fn from_cookie<'de, D, T>(deserializer: D) -> Result<T, D::Error>
where
    D: Deserializer<'de>,
    T: serde::de::DeserializeOwned,
    T: std::fmt::Debug,
{
    #[derive(Debug, Deserialize)]
    struct Wrapper<T> {
        cookie: T,
    }

    let w = Wrapper::deserialize(deserializer)?;
    Ok(w.cookie)
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Date(pub u64);

#[derive(Clone, Debug, PartialEq)]
pub enum Frame {
    Index(u16),
    Element(String),
    Top,
}

impl Serialize for Frame {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map = serializer.serialize_map(Some(1))?;
        match self {
            Frame::Index(nth) => map.serialize_entry("id", nth)?,
            Frame::Element(el) => map.serialize_entry("element", el)?,
            Frame::Top => map.serialize_entry("id", &Value::Null)?,
        }
        map.end()
    }
}

impl<'de> Deserialize<'de> for Frame {
    fn deserialize<D>(deserializer: D) -> Result<Frame, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Debug, Deserialize)]
        #[serde(rename_all = "lowercase")]
        struct JsonFrame {
            id: Option<u16>,
            element: Option<String>,
        }

        let json = JsonFrame::deserialize(deserializer)?;
        match (json.id, json.element) {
            (Some(_id), Some(_element)) => Err(de::Error::custom("conflicting frame identifiers")),
            (Some(id), None) => Ok(Frame::Index(id)),
            (None, Some(element)) => Ok(Frame::Element(element)),
            (None, None) => Ok(Frame::Top),
        }
    }
}

// TODO(nupur): Bug 1567165 - Make WebElement in Marionette a unit struct
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WebElement {
    #[serde(rename = "element-6066-11e4-a52e-4f735466cecf")]
    pub element: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Timeouts {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub implicit: Option<u64>,
    #[serde(default, rename = "pageLoad", skip_serializing_if = "Option::is_none")]
    pub page_load: Option<u64>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    #[allow(clippy::option_option)]
    pub script: Option<Option<u64>>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Window {
    pub handle: String,
}

pub fn to_name<T, S>(data: T, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
    T: Serialize,
{
    #[derive(Serialize)]
    struct Wrapper<T> {
        name: T,
    }

    Wrapper { name: data }.serialize(serializer)
}

pub fn from_name<'de, D, T>(deserializer: D) -> Result<T, D::Error>
where
    D: Deserializer<'de>,
    T: serde::de::DeserializeOwned,
    T: std::fmt::Debug,
{
    #[derive(Debug, Deserialize)]
    struct Wrapper<T> {
        name: T,
    }

    let w = Wrapper::deserialize(deserializer)?;
    Ok(w.name)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::{assert_de, assert_ser, assert_ser_de, ELEMENT_KEY};
    use serde_json::json;

    #[test]
    fn test_cookie_default_values() {
        let data = Cookie {
            name: "hello".into(),
            value: "world".into(),
            path: None,
            domain: None,
            secure: false,
            http_only: false,
            expiry: None,
            same_site: None,
        };
        assert_de(&data, json!({"name":"hello", "value":"world"}));
    }

    #[test]
    fn test_json_frame_index() {
        assert_ser_de(&Frame::Index(1234), json!({"id": 1234}));
    }

    #[test]
    fn test_json_frame_element() {
        assert_ser_de(&Frame::Element("elem".into()), json!({"element": "elem"}));
    }

    #[test]
    fn test_json_frame_parent() {
        assert_ser_de(&Frame::Top, json!({ "id": null }));
    }

    #[test]
    fn test_web_element() {
        let data = WebElement {
            element: "foo".into(),
        };
        assert_ser_de(&data, json!({ELEMENT_KEY: "foo"}));
    }

    #[test]
    fn test_timeouts_with_all_params() {
        let data = Timeouts {
            implicit: Some(1000),
            page_load: Some(200000),
            script: Some(Some(60000)),
        };
        assert_ser_de(
            &data,
            json!({"implicit":1000,"pageLoad":200000,"script":60000}),
        );
    }

    #[test]
    fn test_timeouts_with_missing_params() {
        let data = Timeouts {
            implicit: Some(1000),
            page_load: None,
            script: None,
        };
        assert_ser_de(&data, json!({"implicit":1000}));
    }

    #[test]
    fn test_timeouts_setting_script_none() {
        let data = Timeouts {
            implicit: Some(1000),
            page_load: None,
            script: Some(None),
        };
        assert_ser(&data, json!({"implicit":1000, "script":null}));
    }
}
