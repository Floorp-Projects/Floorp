use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Locator {
    pub using: Selector,
    pub value: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum Selector {
    #[serde(rename = "css selector")]
    CSS,
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
pub enum Command {
    #[serde(rename = "WebDriver:FindElement")]
    FindElement(Locator),
    #[serde(rename = "WebDriver:GetTimeouts")]
    GetTimeouts,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::assert_ser_de;
    use serde_json::json;

    #[test]
    fn test_json_selector_css() {
        assert_ser_de(&Selector::CSS, json!("css selector"));
    }

    #[test]
    fn test_json_selector_link_text() {
        assert_ser_de(&Selector::LinkText, json!("link text"));
    }

    #[test]
    fn test_json_selector_partial_link_text() {
        assert_ser_de(&Selector::PartialLinkText, json!("partial link text"));
    }

    #[test]
    fn test_json_selector_tag_name() {
        assert_ser_de(&Selector::TagName, json!("tag name"));
    }

    #[test]
    fn test_json_selector_xpath() {
        assert_ser_de(&Selector::XPath, json!("xpath"));
    }

    #[test]
    fn test_json_selector_invalid() {
        assert!(serde_json::from_value::<Selector>(json!("foo")).is_err());
    }

    #[test]
    fn test_json_locator() {
        let json = json!({
            "using": "partial link text",
            "value": "link text",
        });
        let data = Locator {
            using: Selector::PartialLinkText,
            value: "link text".into(),
        };

        assert_ser_de(&data, json);
    }

    #[test]
    fn test_command_with_params() {
        let locator = Locator {
            using: Selector::CSS,
            value: "value".into(),
        };
        let json = json!({"WebDriver:FindElement": {"using": "css selector", "value": "value"}});
        assert_ser_de(&Command::FindElement(locator), json);
    }

    #[test]
    fn test_empty_commands() {
        assert_ser_de(&Command::GetTimeouts, json!("WebDriver:GetTimeouts"));
    }

    #[test]
    fn test_json_command_invalid() {
        assert!(serde_json::from_value::<Command>(json!("foo")).is_err());
    }
}
