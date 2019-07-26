use serde::ser::{Serialize, Serializer};
use serde::{Deserialize, Deserializer};
use std::collections::HashMap;

pub static ELEMENT_KEY: &'static str = "element-6066-11e4-a52e-4f735466cecf";
pub static FRAME_KEY: &'static str = "frame-075b-4da1-b6ba-e579c2d3230a";
pub static WINDOW_KEY: &'static str = "window-fcc6-11e5-b4f8-330a88ab9d7f";

pub static MAX_SAFE_INTEGER: u64 = 9_007_199_254_740_991;

pub type Parameters = HashMap<String, String>;

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Cookie {
    pub name: String,
    pub value: String,
    pub path: Option<String>,
    pub domain: Option<String>,
    #[serde(default)]
    pub secure: bool,
    #[serde(rename = "httpOnly")]
    pub http_only: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub expiry: Option<Date>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Date(pub u64);

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum FrameId {
    Short(u16),
    #[serde(
        rename = "element-6066-11e4-a52e-4f735466cecf",
        serialize_with = "serialize_webelement_id"
    )]
    Element(WebElement),
}

// TODO(Henrik): Remove when ToMarionette trait has been fixed (Bug 1481776)
fn serialize_webelement_id<S>(element: &WebElement, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    element.serialize(serializer)
}

#[derive(Copy, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum LocatorStrategy {
    #[serde(rename = "css selector")]
    CSSSelector,
    #[serde(rename = "link text")]
    LinkText,
    #[serde(rename = "partial link text")]
    PartialLinkText,
    #[serde(rename = "tag name")]
    TagName,
    #[serde(rename = "xpath")]
    XPath,
}

#[derive(Clone, Debug, PartialEq)]
pub struct WebElement(pub String);

// private
#[derive(Serialize, Deserialize)]
struct WebElementObject {
    #[serde(rename = "element-6066-11e4-a52e-4f735466cecf")]
    id: String,
}

impl Serialize for WebElement {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        WebElementObject { id: self.0.clone() }.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for WebElement {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(|WebElementObject { id }| WebElement(id))
    }
}

impl WebElement {
    pub fn to_string(&self) -> String {
        self.0.clone()
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct WebFrame(pub String);

// private
#[derive(Serialize, Deserialize)]
struct WebFrameObject {
    #[serde(rename = "frame-075b-4da1-b6ba-e579c2d3230a")]
    id: String,
}

impl Serialize for WebFrame {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        WebFrameObject { id: self.0.clone() }.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for WebFrame {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(|WebFrameObject { id }| WebFrame(id))
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct WebWindow(pub String);

// private
#[derive(Serialize, Deserialize)]
struct WebWindowObject {
    #[serde(rename = "window-fcc6-11e5-b4f8-330a88ab9d7f")]
    id: String,
}

impl Serialize for WebWindow {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        WebWindowObject { id: self.0.clone() }.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for WebWindow {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(|WebWindowObject { id }| WebWindow(id))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::{check_serialize, check_serialize_deserialize};
    use serde_json;

    #[test]
    fn test_json_date() {
        let json = r#"1234"#;
        let data = Date(1234);

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_date_invalid() {
        let json = r#""2018-01-01""#;
        assert!(serde_json::from_str::<Date>(&json).is_err());
    }

    #[test]
    fn test_json_frame_id_short() {
        let json = r#"1234"#;
        let data = FrameId::Short(1234);

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_frame_id_webelement() {
        let json = r#"{"element-6066-11e4-a52e-4f735466cecf":"elem"}"#;
        let data = FrameId::Element(WebElement("elem".into()));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_frame_id_invalid() {
        let json = r#"true"#;
        assert!(serde_json::from_str::<FrameId>(&json).is_err());
    }

    #[test]
    fn test_json_locator_strategy_css_selector() {
        let json = r#""css selector""#;
        let data = LocatorStrategy::CSSSelector;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_locator_strategy_link_text() {
        let json = r#""link text""#;
        let data = LocatorStrategy::LinkText;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_locator_strategy_partial_link_text() {
        let json = r#""partial link text""#;
        let data = LocatorStrategy::PartialLinkText;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_locator_strategy_tag_name() {
        let json = r#""tag name""#;
        let data = LocatorStrategy::TagName;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_locator_strategy_xpath() {
        let json = r#""xpath""#;
        let data = LocatorStrategy::XPath;

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_locator_strategy_invalid() {
        let json = r#""foo""#;
        assert!(serde_json::from_str::<LocatorStrategy>(&json).is_err());
    }

    #[test]
    fn test_json_webelement() {
        let json = r#"{"element-6066-11e4-a52e-4f735466cecf":"elem"}"#;
        let data = WebElement("elem".into());

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_webelement_invalid() {
        let data = r#"{"elem-6066-11e4-a52e-4f735466cecf":"elem"}"#;
        assert!(serde_json::from_str::<WebElement>(&data).is_err());
    }

    #[test]
    fn test_json_webframe() {
        let json = r#"{"frame-075b-4da1-b6ba-e579c2d3230a":"frm"}"#;
        let data = WebFrame("frm".into());

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_webframe_invalid() {
        let data = r#"{"frm-075b-4da1-b6ba-e579c2d3230a":"frm"}"#;
        assert!(serde_json::from_str::<WebFrame>(&data).is_err());
    }

    #[test]
    fn test_json_webwindow() {
        let json = r#"{"window-fcc6-11e5-b4f8-330a88ab9d7f":"window"}"#;
        let data = WebWindow("window".into());

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_webwindow_invalid() {
        let data = r#"{""wind-fcc6-11e5-b4f8-330a88ab9d7f":"window"}"#;
        assert!(serde_json::from_str::<WebWindow>(&data).is_err());
    }
}
