use serde::ser::{Serialize, Serializer};

pub static ELEMENT_KEY: &'static str = "element-6066-11e4-a52e-4f735466cecf";
pub static FRAME_KEY: &'static str = "frame-075b-4da1-b6ba-e579c2d3230a";
pub static WINDOW_KEY: &'static str = "window-fcc6-11e5-b4f8-330a88ab9d7f";

pub static MAX_SAFE_INTEGER: u64 = 9_007_199_254_740_991;

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Cookie {
    pub name: String,
    pub value: String,
    pub path: Option<String>,
    pub domain: Option<String>,
    #[serde(default)]
    pub secure: bool,
    #[serde(default)]
    pub httpOnly: bool,
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
        rename = "element-6066-11e4-a52e-4f735466cecf", serialize_with = "serialize_webelement_id"
    )]
    Element(WebElement),
}

// TODO(Henrik): Remove when ToMarionette trait has been fixed (Bug 1481776)
fn serialize_webelement_id<S>(element: &WebElement, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    element.id.serialize(serializer)
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

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WebElement {
    #[serde(rename = "element-6066-11e4-a52e-4f735466cecf")]
    pub id: String,
}

impl WebElement {
    pub fn new(id: String) -> WebElement {
        WebElement { id }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json;
    use test::{check_serialize, check_serialize_deserialize};

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
        let json = r#""elem""#;
        let data = FrameId::Element(WebElement::new("elem".into()));

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
        let data = WebElement::new("elem".into());

        check_serialize_deserialize(&json, &data);
    }

    #[test]
    fn test_json_webelement_invalid() {
        let data = r#"{"elem-6066-11e4-a52e-4f735466cecf":"elem"}"#;
        assert!(serde_json::from_str::<WebElement>(&data).is_err());
    }
}
