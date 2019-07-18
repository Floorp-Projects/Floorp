use serde::{Deserialize, Deserializer, Serialize, Serializer};
use serde_json::Value;

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WebElement {
    #[serde(rename = "element-6066-11e4-a52e-4f735466cecf")]
    element: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Timeouts {
    implicit: u64,
    #[serde(rename = "pageLoad", alias = "page load")]
    page_load: u64,
    script: Option<u64>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum MarionetteResult {
    #[serde(deserialize_with = "from_value", serialize_with = "to_value")]
    String(String),
    Timeouts(Timeouts),
    #[serde(deserialize_with = "from_value", serialize_with = "to_value")]
    WebElement(WebElement),
    #[serde(deserialize_with = "from_value", serialize_with = "to_empty_value")]
    Null,
}

fn to_value<T, S>(data: T, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
    T: Serialize,
{
    #[derive(Serialize)]
    struct Wrapper<T> {
        value: T,
    }

    Wrapper { value: data }.serialize(serializer)
}

fn to_empty_value<S>(serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    #[derive(Serialize)]
    struct Wrapper {
        value: Value,
    }

    Wrapper { value: Value::Null }.serialize(serializer)
}

fn from_value<'de, D, T>(deserializer: D) -> Result<T, D::Error>
where
    D: Deserializer<'de>,
    T: serde::de::DeserializeOwned,
    T: std::fmt::Debug,
{
    #[derive(Debug, Deserialize)]
    struct Wrapper<T> {
        value: T,
    }

    let w = Wrapper::deserialize(deserializer)?;
    Ok(w.value)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::{assert_de, assert_ser_de, ELEMENT_KEY};
    use serde_json::json;

    #[test]
    fn test_web_element() {
        let data = WebElement {
            element: "foo".into(),
        };
        assert_ser_de(&data, json!({"element-6066-11e4-a52e-4f735466cecf": "foo"}));
    }

    #[test]
    fn test_timeouts() {
        let data = Timeouts {
            implicit: 1000,
            page_load: 200000,
            script: Some(60000),
        };
        assert_ser_de(
            &data,
            json!({"implicit":1000,"pageLoad":200000,"script":60000}),
        );
        assert_de(
            &data,
            json!({"implicit":1000,"page load":200000,"script":60000}),
        );
    }

    #[test]
    fn test_web_element_response() {
        let data = WebElement {
            element: "foo".into(),
        };
        assert_ser_de(
            &MarionetteResult::WebElement(data),
            json!({"value": {ELEMENT_KEY: "foo"}}),
        );
    }

    #[test]
    fn test_timeouts_response() {
        let data = Timeouts {
            implicit: 1000,
            page_load: 200000,
            script: Some(60000),
        };
        assert_ser_de(
            &MarionetteResult::Timeouts(data),
            json!({"implicit":1000,"pageLoad":200000,"script":60000}),
        );
    }

    #[test]
    fn test_string_response() {
        assert_ser_de(
            &MarionetteResult::String("foo".into()),
            json!({"value": "foo"}),
        );
    }

    #[test]
    fn test_null_response() {
        assert_ser_de(&MarionetteResult::Null, json!({ "value": null }));
    }
}
