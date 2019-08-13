use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::common::{from_cookie, from_name, to_cookie, to_name, Cookie, Frame, Timeouts};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Url {
    pub url: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct LegacyWebElement {
    pub id: String,
}

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
pub struct NewWindow {
    #[serde(rename = "type", skip_serializing_if = "Option::is_none")]
    pub type_hint: Option<String>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WindowRect {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub x: Option<i32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub y: Option<i32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub width: Option<i32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub height: Option<i32>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Keys {
    pub text: String,
    pub value: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Script {
    pub script: String,
    pub args: Option<Vec<Value>>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SwitchToFrame {
    pub id: Option<Frame>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum Command {
    // Needs to be updated to "WebDriver:AcceptAlert" for Firefox 63
    #[serde(rename = "WebDriver:AcceptDialog")]
    AcceptAlert,
    #[serde(
        rename = "WebDriver:AddCookie",
        serialize_with = "to_cookie",
        deserialize_with = "from_cookie"
    )]
    AddCookie(Cookie),
    #[serde(rename = "WebDriver:CloseWindow")]
    CloseWindow,
    #[serde(
        rename = "WebDriver:DeleteCookie",
        serialize_with = "to_name",
        deserialize_with = "from_name"
    )]
    DeleteCookie(String),
    #[serde(rename = "WebDriver:DeleteAllCookies")]
    DeleteCookies,
    #[serde(rename = "WebDriver:DismissAlert")]
    DismissAlert,
    #[serde(rename = "WebDriver:ElementClear")]
    ElementClear(LegacyWebElement),
    #[serde(rename = "WebDriver:ElementClick")]
    ElementClick(LegacyWebElement),
    #[serde(rename = "WebDriver:ExecuteAsyncScript")]
    ExecuteAsyncScript(Script),
    #[serde(rename = "WebDriver:ExecuteScript")]
    ExecuteScript(Script),
    #[serde(rename = "WebDriver:FindElement")]
    FindElement(Locator),
    #[serde(rename = "WebDriver:FindElements")]
    FindElements(Locator),
    #[serde(rename = "WebDriver:FindElement")]
    FindElementElement {
        element: String,
        using: Selector,
        value: String,
    },
    #[serde(rename = "WebDriver:FindElements")]
    FindElementElements {
        element: String,
        using: Selector,
        value: String,
    },
    #[serde(rename = "WebDriver:FullscreenWindow")]
    FullscreenWindow,
    #[serde(rename = "WebDriver:Navigate")]
    Get(Url),
    #[serde(rename = "WebDriver:GetActiveElement")]
    GetActiveElement,
    #[serde(rename = "WebDriver:GetAlertText")]
    GetAlertText,
    #[serde(rename = "WebDriver:GetCookies")]
    GetCookies,
    #[serde(rename = "WebDriver:GetElementCSSValue")]
    GetCSSValue {
        id: String,
        #[serde(rename = "propertyName")]
        property: String,
    },
    #[serde(rename = "WebDriver:GetCurrentURL")]
    GetCurrentUrl,
    #[serde(rename = "WebDriver:GetElementAttribute")]
    GetElementAttribute { id: String, name: String },
    #[serde(rename = "WebDriver:GetElementProperty")]
    GetElementProperty { id: String, name: String },
    #[serde(rename = "WebDriver:GetElementRect")]
    GetElementRect(LegacyWebElement),
    #[serde(rename = "WebDriver:GetElementTagName")]
    GetElementTagName(LegacyWebElement),
    #[serde(rename = "WebDriver:GetElementText")]
    GetElementText(LegacyWebElement),
    #[serde(rename = "WebDriver:GetPageSource")]
    GetPageSource,
    #[serde(rename = "WebDriver:GetTimeouts")]
    GetTimeouts,
    #[serde(rename = "WebDriver:GetTitle")]
    GetTitle,
    #[serde(rename = "WebDriver:GetWindowHandle")]
    GetWindowHandle,
    #[serde(rename = "WebDriver:GetWindowHandles")]
    GetWindowHandles,
    #[serde(rename = "WebDriver:GetWindowRect")]
    GetWindowRect,
    #[serde(rename = "WebDriver:Back")]
    GoBack,
    #[serde(rename = "WebDriver:Forward")]
    GoForward,
    #[serde(rename = "WebDriver:IsElementDisplayed")]
    IsDisplayed(LegacyWebElement),
    #[serde(rename = "WebDriver:IsElementEnabled")]
    IsEnabled(LegacyWebElement),
    #[serde(rename = "WebDriver:IsElementSelected")]
    IsSelected(LegacyWebElement),
    #[serde(rename = "WebDriver:MaximizeWindow")]
    MaximizeWindow,
    #[serde(rename = "WebDriver:MinimizeWindow")]
    MinimizeWindow,
    #[serde(rename = "WebDriver:NewWindow")]
    NewWindow(NewWindow),
    #[serde(rename = "WebDriver:Refresh")]
    Refresh,
    #[serde(rename = "WebDriver:ReleaseActions")]
    ReleaseActions,
    #[serde(rename = "WebDriver:SendAlertText")]
    SendAlertText(Keys),
    #[serde(rename = "WebDriver:SetTimeouts")]
    SetTimeouts(Timeouts),
    #[serde(rename = "WebDriver:SetWindowRect")]
    SetWindowRect(WindowRect),
    #[serde(rename = "WebDriver:SwitchToFrame")]
    SwitchToFrame(SwitchToFrame),
    #[serde(rename = "WebDriver:SwitchToParentFrame")]
    SwitchToParentFrame,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::Date;
    use crate::test::{assert_ser, assert_ser_de};
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
    fn test_json_keys() {
        let data = Keys {
            text: "Foo".into(),
            value: vec!["F".into(), "o".into(), "o".into()],
        };
        let json = json!({"text": "Foo", "value": ["F", "o", "o"]});
        assert_ser_de(&data, json);
    }

    #[test]
    fn test_json_new_window() {
        let data = NewWindow {
            type_hint: Some("foo".into()),
        };
        assert_ser_de(&data, json!({ "type": "foo" }));
    }

    #[test]
    fn test_json_window_rect() {
        let data = WindowRect {
            x: Some(123),
            y: None,
            width: None,
            height: None,
        };
        assert_ser_de(&data, json!({"x": 123}));
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
    fn test_command_with_wrapper_params() {
        let cookie = Cookie {
            name: "hello".into(),
            value: "world".into(),
            path: None,
            domain: None,
            secure: false,
            http_only: false,
            expiry: Some(Date(1564488092)),
        };
        let json = json!({"WebDriver:AddCookie": {"cookie": {"name": "hello", "value": "world", "secure": false, "httpOnly": false, "expiry": 1564488092}}});
        assert_ser_de(&Command::AddCookie(cookie), json);
    }

    #[test]
    fn test_empty_commands() {
        assert_ser_de(&Command::GetTimeouts, json!("WebDriver:GetTimeouts"));
    }

    #[test]
    fn test_json_command_invalid() {
        assert!(serde_json::from_value::<Command>(json!("foo")).is_err());
    }

    #[test]
    fn test_json_delete_cookie_command() {
        let json = json!({"WebDriver:DeleteCookie": {"name": "foo"}});
        assert_ser_de(&Command::DeleteCookie("foo".into()), json);
    }

    #[test]
    fn test_json_new_window_command() {
        let data = NewWindow {
            type_hint: Some("foo".into()),
        };
        let json = json!({"WebDriver:NewWindow": {"type": "foo"}});
        assert_ser_de(&Command::NewWindow(data), json);
    }

    #[test]
    fn test_json_new_window_command_with_none_value() {
        let data = NewWindow { type_hint: None };
        let json = json!({"WebDriver:NewWindow": {}});
        assert_ser_de(&Command::NewWindow(data), json);
    }

    #[test]
    fn test_json_command_as_struct() {
        assert_ser(
            &Command::FindElementElement {
                element: "foo".into(),
                using: Selector::XPath,
                value: "bar".into(),
            },
            json!({"WebDriver:FindElement": {"element": "foo", "using": "xpath", "value": "bar" }}),
        );
    }

    #[test]
    fn test_json_get_css_value() {
        assert_ser_de(
            &Command::GetCSSValue {
                id: "foo".into(),
                property: "bar".into(),
            },
            json!({"WebDriver:GetElementCSSValue": {"id": "foo", "propertyName": "bar"}}),
        );
    }
}
