/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::common::{from_cookie, from_name, to_cookie, to_name, Cookie, Frame, Timeouts, Window};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Url {
    pub url: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Locator {
    pub using: Selector,
    pub value: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum Selector {
    #[serde(rename = "css selector")]
    Css,
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
#[serde(untagged)]
pub enum PrintPageRange {
    Integer(u64),
    Range(String),
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub struct PrintParameters {
    pub orientation: PrintOrientation,
    pub scale: f64,
    pub background: bool,
    pub page: PrintPage,
    pub margin: PrintMargins,
    pub page_ranges: Vec<PrintPageRange>,
    pub shrink_to_fit: bool,
}

impl Default for PrintParameters {
    fn default() -> Self {
        PrintParameters {
            orientation: PrintOrientation::default(),
            scale: 1.0,
            background: false,
            page: PrintPage::default(),
            margin: PrintMargins::default(),
            page_ranges: Vec::new(),
            shrink_to_fit: true,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum PrintOrientation {
    Landscape,
    Portrait,
}

impl Default for PrintOrientation {
    fn default() -> Self {
        PrintOrientation::Portrait
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PrintPage {
    pub width: f64,
    pub height: f64,
}

impl Default for PrintPage {
    fn default() -> Self {
        PrintPage {
            width: 21.59,
            height: 27.94,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PrintMargins {
    pub top: f64,
    pub bottom: f64,
    pub left: f64,
    pub right: f64,
}

impl Default for PrintMargins {
    fn default() -> Self {
        PrintMargins {
            top: 1.0,
            bottom: 1.0,
            left: 1.0,
            right: 1.0,
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub enum WebAuthnProtocol {
    #[serde(rename = "ctap1/u2f")]
    Ctap1U2f,
    #[serde(rename = "ctap2")]
    Ctap2,
    #[serde(rename = "ctap2_1")]
    Ctap2_1,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "kebab-case")]
pub enum AuthenticatorTransport {
    Usb,
    Nfc,
    Ble,
    SmartCard,
    Hybrid,
    Internal,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct AuthenticatorParameters {
    pub protocol: WebAuthnProtocol,
    pub transport: AuthenticatorTransport,
    pub has_resident_key: bool,
    pub has_user_verification: bool,
    pub is_user_consenting: bool,
    pub is_user_verified: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CredentialParameters {
    pub credential_id: String,
    pub is_resident_credential: bool,
    pub rp_id: String,
    pub private_key: String,
    pub user_handle: String,
    pub sign_count: u64,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct UserVerificationParameters {
    pub is_user_verified: bool,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ScreenshotOptions {
    pub id: Option<String>,
    pub highlights: Vec<Option<String>>,
    pub full: bool,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Script {
    pub script: String,
    pub args: Option<Vec<Value>>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum Command {
    #[serde(rename = "WebDriver:AcceptAlert")]
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
    #[serde(rename = "WebDriver:DeleteSession")]
    DeleteSession,
    #[serde(rename = "WebDriver:DismissAlert")]
    DismissAlert,
    #[serde(rename = "WebDriver:ElementClear")]
    ElementClear { id: String },
    #[serde(rename = "WebDriver:ElementClick")]
    ElementClick { id: String },
    #[serde(rename = "WebDriver:ElementSendKeys")]
    ElementSendKeys {
        id: String,
        text: String,
        value: Vec<String>,
    },
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
    #[serde(rename = "WebDriver:FindElementFromShadowRoot")]
    FindShadowRootElement {
        #[serde(rename = "shadowRoot")]
        shadow_root: String,
        using: Selector,
        value: String,
    },
    #[serde(rename = "WebDriver:FindElementsFromShadowRoot")]
    FindShadowRootElements {
        #[serde(rename = "shadowRoot")]
        shadow_root: String,
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
    #[serde(rename = "WebDriver:GetComputedLabel")]
    GetComputedLabel { id: String },
    #[serde(rename = "WebDriver:GetComputedRole")]
    GetComputedRole { id: String },
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
    GetElementRect { id: String },
    #[serde(rename = "WebDriver:GetElementTagName")]
    GetElementTagName { id: String },
    #[serde(rename = "WebDriver:GetElementText")]
    GetElementText { id: String },
    #[serde(rename = "WebDriver:GetPageSource")]
    GetPageSource,
    #[serde(rename = "WebDriver:GetShadowRoot")]
    GetShadowRoot { id: String },
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
    IsDisplayed { id: String },
    #[serde(rename = "WebDriver:IsElementEnabled")]
    IsEnabled { id: String },
    #[serde(rename = "WebDriver:IsElementSelected")]
    IsSelected { id: String },
    #[serde(rename = "WebDriver:MaximizeWindow")]
    MaximizeWindow,
    #[serde(rename = "WebDriver:MinimizeWindow")]
    MinimizeWindow,
    #[serde(rename = "WebDriver:NewWindow")]
    NewWindow(NewWindow),
    #[serde(rename = "WebDriver:Print")]
    Print(PrintParameters),
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
    SwitchToFrame(Frame),
    #[serde(rename = "WebDriver:SwitchToParentFrame")]
    SwitchToParentFrame,
    #[serde(rename = "WebDriver:SwitchToWindow")]
    SwitchToWindow(Window),
    #[serde(rename = "WebDriver:TakeScreenshot")]
    TakeElementScreenshot(ScreenshotOptions),
    #[serde(rename = "WebDriver:TakeScreenshot")]
    TakeFullScreenshot(ScreenshotOptions),
    #[serde(rename = "WebDriver:TakeScreenshot")]
    TakeScreenshot(ScreenshotOptions),
    #[serde(rename = "WebAuthn:AddVirtualAuthenticator")]
    WebAuthnAddVirtualAuthenticator(AuthenticatorParameters),
    #[serde(rename = "WebAuthn:RemoveVirtualAuthenticator")]
    WebAuthnRemoveVirtualAuthenticator,
    #[serde(rename = "WebAuthn:AddCredential")]
    WebAuthnAddCredential(CredentialParameters),
    #[serde(rename = "WebAuthn:GetCredentials")]
    WebAuthnGetCredentials,
    #[serde(rename = "WebAuthn:RemoveCredential")]
    WebAuthnRemoveCredential,
    #[serde(rename = "WebAuthn:RemoveAllCredentials")]
    WebAuthnRemoveAllCredentials,
    #[serde(rename = "WebAuthn:SetUserVerified")]
    WebAuthnSetUserVerified(UserVerificationParameters),
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::Date;
    use crate::test::{assert_ser, assert_ser_de};
    use serde_json::json;

    #[test]
    fn test_json_screenshot() {
        let data = ScreenshotOptions {
            id: None,
            highlights: vec![],
            full: false,
        };
        let json = json!({"full":false,"highlights":[],"id":null});
        assert_ser_de(&data, json);
    }

    #[test]
    fn test_json_selector_css() {
        assert_ser_de(&Selector::Css, json!("css selector"));
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
            using: Selector::Css,
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
            same_site: None,
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
    fn test_json_get_computed_label_command() {
        assert_ser_de(
            &Command::GetComputedLabel { id: "foo".into() },
            json!({"WebDriver:GetComputedLabel": {"id": "foo"}}),
        );
    }

    #[test]
    fn test_json_get_computed_role_command() {
        assert_ser_de(
            &Command::GetComputedRole { id: "foo".into() },
            json!({"WebDriver:GetComputedRole": {"id": "foo"}}),
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

    #[test]
    fn test_json_find_shadow_root_element() {
        assert_ser_de(
            &Command::FindShadowRootElement {
                shadow_root: "foo".into(),
                using: Selector::Css,
                value: "bar".into(),
            },
            json!({"WebDriver:FindElementFromShadowRoot": {"shadowRoot": "foo", "using": "css selector", "value": "bar"}}),
        );
    }

    #[test]
    fn test_json_find_shadow_root_elements() {
        assert_ser_de(
            &Command::FindShadowRootElements {
                shadow_root: "foo".into(),
                using: Selector::Css,
                value: "bar".into(),
            },
            json!({"WebDriver:FindElementsFromShadowRoot": {"shadowRoot": "foo", "using": "css selector", "value": "bar"}}),
        );
    }
}
