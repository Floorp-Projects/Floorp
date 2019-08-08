use serde::de;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use serde_json::Value;

use crate::common::{Cookie, Timeouts, WebElement};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct NewWindow {
    handle: String,
    #[serde(rename = "type")]
    type_hint: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WindowRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ElementRect {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum MarionetteResult {
    #[serde(deserialize_with = "from_value", serialize_with = "to_empty_value")]
    Null,
    NewWindow(NewWindow),
    WindowRect(WindowRect),
    ElementRect(ElementRect),
    #[serde(deserialize_with = "from_value", serialize_with = "to_value")]
    String(String),
    Strings(Vec<String>),
    #[serde(deserialize_with = "from_value", serialize_with = "to_value")]
    WebElement(WebElement),
    WebElements(Vec<WebElement>),
    Cookies(Vec<Cookie>),
    Timeouts(Timeouts),
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

    let v = Value::deserialize(deserializer)?;
    if v.is_object() {
        let w = serde_json::from_value::<Wrapper<T>>(v).map_err(de::Error::custom)?;
        Ok(w.value)
    } else {
        Err(de::Error::custom("Cannot be deserialized to struct"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::{assert_de, assert_ser_de, ELEMENT_KEY};
    use serde_json::json;

    #[test]
    fn test_cookies_response() {
        let mut data = Vec::new();
        data.push(Cookie {
            name: "foo".into(),
            value: "bar".into(),
            path: Some("/common".into()),
            domain: Some("web-platform.test".into()),
            secure: false,
            http_only: false,
            expiry: None,
        });
        assert_ser_de(
            &MarionetteResult::Cookies(data),
            json!([{"name":"foo","value":"bar","path":"/common","domain":"web-platform.test","secure":false,"httpOnly":false}]),
        );
    }

    #[test]
    fn test_new_window_response() {
        let data = NewWindow {
            handle: "6442450945".into(),
            type_hint: "tab".into(),
        };
        let json = json!({"handle": "6442450945", "type": "tab"});
        assert_ser_de(&MarionetteResult::NewWindow(data), json);
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
    fn test_web_elements_response() {
        let data = vec![
            WebElement {
                element: "foo".into(),
            },
            WebElement {
                element: "bar".into(),
            },
        ];
        assert_ser_de(
            &MarionetteResult::WebElements(data),
            json!([{ELEMENT_KEY: "foo"}, {ELEMENT_KEY: "bar"}]),
        );
    }

    #[test]
    fn test_timeouts_response() {
        let data = Timeouts {
            implicit: Some(1000),
            page_load: Some(200000),
            script: Some(Some(60000)),
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
    fn test_strings_response() {
        assert_ser_de(
            &MarionetteResult::Strings(vec!["2147483649".to_string()]),
            json!(["2147483649"]),
        );
    }

    #[test]
    fn test_null_response() {
        assert_ser_de(&MarionetteResult::Null, json!({ "value": null }));
    }

    #[test]
    fn test_window_rect_response() {
        let data = WindowRect {
            x: 100,
            y: 100,
            width: 800,
            height: 600,
        };
        let json = json!({"x": 100, "y": 100, "width": 800, "height": 600});
        assert_ser_de(&MarionetteResult::WindowRect(data), json);
    }

    #[test]
    fn test_element_rect_response() {
        let data = ElementRect {
            x: 8.0,
            y: 8.0,
            width: 148.6666717529297,
            height: 22.0,
        };
        let json = json!({"x": 8, "y": 8, "width": 148.6666717529297, "height": 22});
        assert_de(&MarionetteResult::ElementRect(data), json);
    }
}
