use crate::actions::ActionSequence;
use crate::capabilities::{
    BrowserCapabilities, Capabilities, CapabilitiesMatching, LegacyNewSessionParameters,
    SpecNewSessionParameters,
};
use crate::common::{Date, FrameId, LocatorStrategy, WebElement, MAX_SAFE_INTEGER};
use crate::error::{ErrorStatus, WebDriverError, WebDriverResult};
use crate::httpapi::{Route, VoidWebDriverExtensionRoute, WebDriverExtensionRoute};
use crate::Parameters;
use serde::de::{self, Deserialize, Deserializer};
use serde_json::{self, Value};

#[derive(Debug, PartialEq)]
pub enum WebDriverCommand<T: WebDriverExtensionCommand> {
    NewSession(NewSessionParameters),
    DeleteSession,
    Get(GetParameters),
    GetCurrentUrl,
    GoBack,
    GoForward,
    Refresh,
    GetTitle,
    GetPageSource,
    GetWindowHandle,
    GetWindowHandles,
    NewWindow(NewWindowParameters),
    CloseWindow,
    GetWindowRect,
    SetWindowRect(WindowRectParameters),
    MinimizeWindow,
    MaximizeWindow,
    FullscreenWindow,
    SwitchToWindow(SwitchToWindowParameters),
    SwitchToFrame(SwitchToFrameParameters),
    SwitchToParentFrame,
    FindElement(LocatorParameters),
    FindElements(LocatorParameters),
    FindElementElement(WebElement, LocatorParameters),
    FindElementElements(WebElement, LocatorParameters),
    GetActiveElement,
    IsDisplayed(WebElement),
    IsSelected(WebElement),
    GetElementAttribute(WebElement, String),
    GetElementProperty(WebElement, String),
    GetCSSValue(WebElement, String),
    GetElementText(WebElement),
    GetElementTagName(WebElement),
    GetElementRect(WebElement),
    IsEnabled(WebElement),
    ExecuteScript(JavascriptCommandParameters),
    ExecuteAsyncScript(JavascriptCommandParameters),
    GetCookies,
    GetNamedCookie(String),
    AddCookie(AddCookieParameters),
    DeleteCookies,
    DeleteCookie(String),
    GetTimeouts,
    SetTimeouts(TimeoutsParameters),
    ElementClick(WebElement),
    ElementClear(WebElement),
    ElementSendKeys(WebElement, SendKeysParameters),
    PerformActions(ActionsParameters),
    ReleaseActions,
    DismissAlert,
    AcceptAlert,
    GetAlertText,
    SendAlertText(SendKeysParameters),
    TakeScreenshot,
    TakeElementScreenshot(WebElement),
    Status,
    Extension(T),
}

pub trait WebDriverExtensionCommand: Clone + Send + PartialEq {
    fn parameters_json(&self) -> Option<Value>;
}

#[derive(Clone, Debug, PartialEq)]
pub struct VoidWebDriverExtensionCommand;

impl WebDriverExtensionCommand for VoidWebDriverExtensionCommand {
    fn parameters_json(&self) -> Option<Value> {
        panic!("No extensions implemented");
    }
}

#[derive(Debug, PartialEq)]
pub struct WebDriverMessage<U: WebDriverExtensionRoute = VoidWebDriverExtensionRoute> {
    pub session_id: Option<String>,
    pub command: WebDriverCommand<U::Command>,
}

impl<U: WebDriverExtensionRoute> WebDriverMessage<U> {
    pub fn new(
        session_id: Option<String>,
        command: WebDriverCommand<U::Command>,
    ) -> WebDriverMessage<U> {
        WebDriverMessage {
            session_id,
            command,
        }
    }

    pub fn from_http(
        match_type: Route<U>,
        params: &Parameters,
        raw_body: &str,
        requires_body: bool,
    ) -> WebDriverResult<WebDriverMessage<U>> {
        let session_id = WebDriverMessage::<U>::get_session_id(params);
        let body_data = WebDriverMessage::<U>::decode_body(raw_body, requires_body)?;
        let command = match match_type {
            Route::NewSession => WebDriverCommand::NewSession(serde_json::from_str(raw_body)?),
            Route::DeleteSession => WebDriverCommand::DeleteSession,
            Route::Get => WebDriverCommand::Get(serde_json::from_str(raw_body)?),
            Route::GetCurrentUrl => WebDriverCommand::GetCurrentUrl,
            Route::GoBack => WebDriverCommand::GoBack,
            Route::GoForward => WebDriverCommand::GoForward,
            Route::Refresh => WebDriverCommand::Refresh,
            Route::GetTitle => WebDriverCommand::GetTitle,
            Route::GetPageSource => WebDriverCommand::GetPageSource,
            Route::GetWindowHandle => WebDriverCommand::GetWindowHandle,
            Route::GetWindowHandles => WebDriverCommand::GetWindowHandles,
            Route::NewWindow => WebDriverCommand::NewWindow(serde_json::from_str(raw_body)?),
            Route::CloseWindow => WebDriverCommand::CloseWindow,
            Route::GetTimeouts => WebDriverCommand::GetTimeouts,
            Route::SetTimeouts => WebDriverCommand::SetTimeouts(serde_json::from_str(raw_body)?),
            Route::GetWindowRect | Route::GetWindowPosition | Route::GetWindowSize => {
                WebDriverCommand::GetWindowRect
            }
            Route::SetWindowRect | Route::SetWindowPosition | Route::SetWindowSize => {
                WebDriverCommand::SetWindowRect(serde_json::from_str(raw_body)?)
            }
            Route::MinimizeWindow => WebDriverCommand::MinimizeWindow,
            Route::MaximizeWindow => WebDriverCommand::MaximizeWindow,
            Route::FullscreenWindow => WebDriverCommand::FullscreenWindow,
            Route::SwitchToWindow => {
                WebDriverCommand::SwitchToWindow(serde_json::from_str(raw_body)?)
            }
            Route::SwitchToFrame => {
                WebDriverCommand::SwitchToFrame(serde_json::from_str(raw_body)?)
            }
            Route::SwitchToParentFrame => WebDriverCommand::SwitchToParentFrame,
            Route::FindElement => WebDriverCommand::FindElement(serde_json::from_str(raw_body)?),
            Route::FindElements => WebDriverCommand::FindElements(serde_json::from_str(raw_body)?),
            Route::FindElementElement => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::FindElementElement(element, serde_json::from_str(raw_body)?)
            }
            Route::FindElementElements => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::FindElementElements(element, serde_json::from_str(raw_body)?)
            }
            Route::GetActiveElement => WebDriverCommand::GetActiveElement,
            Route::IsDisplayed => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::IsDisplayed(element)
            }
            Route::IsSelected => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::IsSelected(element)
            }
            Route::GetElementAttribute => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                let attr = try_opt!(
                    params.get("name"),
                    ErrorStatus::InvalidArgument,
                    "Missing name parameter"
                )
                .as_str();
                WebDriverCommand::GetElementAttribute(element, attr.into())
            }
            Route::GetElementProperty => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                let property = try_opt!(
                    params.get("name"),
                    ErrorStatus::InvalidArgument,
                    "Missing name parameter"
                )
                .as_str();
                WebDriverCommand::GetElementProperty(element, property.into())
            }
            Route::GetCSSValue => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                let property = try_opt!(
                    params.get("propertyName"),
                    ErrorStatus::InvalidArgument,
                    "Missing propertyName parameter"
                )
                .as_str();
                WebDriverCommand::GetCSSValue(element, property.into())
            }
            Route::GetElementText => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::GetElementText(element)
            }
            Route::GetElementTagName => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::GetElementTagName(element)
            }
            Route::GetElementRect => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::GetElementRect(element)
            }
            Route::IsEnabled => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::IsEnabled(element)
            }
            Route::ElementClick => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::ElementClick(element)
            }
            Route::ElementClear => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::ElementClear(element)
            }
            Route::ElementSendKeys => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::ElementSendKeys(element, serde_json::from_str(raw_body)?)
            }
            Route::ExecuteScript => {
                WebDriverCommand::ExecuteScript(serde_json::from_str(raw_body)?)
            }
            Route::ExecuteAsyncScript => {
                WebDriverCommand::ExecuteAsyncScript(serde_json::from_str(raw_body)?)
            }
            Route::GetCookies => WebDriverCommand::GetCookies,
            Route::GetNamedCookie => {
                let name = try_opt!(
                    params.get("name"),
                    ErrorStatus::InvalidArgument,
                    "Missing 'name' parameter"
                )
                .as_str()
                .into();
                WebDriverCommand::GetNamedCookie(name)
            }
            Route::AddCookie => WebDriverCommand::AddCookie(serde_json::from_str(raw_body)?),
            Route::DeleteCookies => WebDriverCommand::DeleteCookies,
            Route::DeleteCookie => {
                let name = try_opt!(
                    params.get("name"),
                    ErrorStatus::InvalidArgument,
                    "Missing name parameter"
                )
                .as_str()
                .into();
                WebDriverCommand::DeleteCookie(name)
            }
            Route::PerformActions => {
                WebDriverCommand::PerformActions(serde_json::from_str(raw_body)?)
            }
            Route::ReleaseActions => WebDriverCommand::ReleaseActions,
            Route::DismissAlert => WebDriverCommand::DismissAlert,
            Route::AcceptAlert => WebDriverCommand::AcceptAlert,
            Route::GetAlertText => WebDriverCommand::GetAlertText,
            Route::SendAlertText => {
                WebDriverCommand::SendAlertText(serde_json::from_str(raw_body)?)
            }
            Route::TakeScreenshot => WebDriverCommand::TakeScreenshot,
            Route::TakeElementScreenshot => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::TakeElementScreenshot(element)
            }
            Route::Status => WebDriverCommand::Status,
            Route::Extension(ref extension) => extension.command(params, &body_data)?,
        };
        Ok(WebDriverMessage::new(session_id, command))
    }

    fn get_session_id(params: &Parameters) -> Option<String> {
        params.get("sessionId").cloned()
    }

    fn decode_body(body: &str, requires_body: bool) -> WebDriverResult<Value> {
        if requires_body {
            match serde_json::from_str(body) {
                Ok(x @ Value::Object(_)) => Ok(x),
                Ok(_) => Err(WebDriverError::new(
                    ErrorStatus::InvalidArgument,
                    "Body was not a JSON Object",
                )),
                Err(e) => {
                    if e.is_io() {
                        Err(WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            format!("I/O error whilst decoding body: {}", e),
                        ))
                    } else {
                        let msg = format!("Failed to decode request as JSON: {}", body);
                        let stack = format!("Syntax error at :{}:{}", e.line(), e.column());
                        Err(WebDriverError::new_with_stack(
                            ErrorStatus::InvalidArgument,
                            msg,
                            stack,
                        ))
                    }
                }
            }
        } else {
            Ok(Value::Null)
        }
    }
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct ActionsParameters {
    pub actions: Vec<ActionSequence>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(remote = "Self")]
pub struct AddCookieParameters {
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

impl<'de> Deserialize<'de> for AddCookieParameters {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Deserialize)]
        struct Wrapper {
            #[serde(with = "AddCookieParameters")]
            cookie: AddCookieParameters,
        }

        Wrapper::deserialize(deserializer).map(|wrapper| wrapper.cookie)
    }
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct GetParameters {
    pub url: String,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct GetNamedCookieParameters {
    pub name: Option<String>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct JavascriptCommandParameters {
    pub script: String,
    pub args: Option<Vec<Value>>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct LocatorParameters {
    pub using: LocatorStrategy,
    pub value: String,
}

/// Wrapper around the two supported variants of new session paramters.
///
/// The Spec variant is used for storing spec-compliant parameters whereas
/// the legacy variant is used to store `desiredCapabilities`/`requiredCapabilities`
/// parameters, and is intended to minimise breakage as we transition users to
/// the spec design.
#[derive(Debug, PartialEq)]
pub enum NewSessionParameters {
    Spec(SpecNewSessionParameters),
    Legacy(LegacyNewSessionParameters),
}

impl<'de> Deserialize<'de> for NewSessionParameters {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let value = serde_json::Value::deserialize(deserializer)?;
        if let Some(caps) = value.get("capabilities") {
            if !caps.is_object() {
                return Err(de::Error::custom("capabilities must be objects"));
            }
            let caps = SpecNewSessionParameters::deserialize(caps).map_err(de::Error::custom)?;
            return Ok(NewSessionParameters::Spec(caps));
        }

        warn!("You are using deprecated legacy session negotiation patterns (desiredCapabilities/requiredCapabilities), see https://developer.mozilla.org/en-US/docs/Web/WebDriver/Capabilities#Legacy");
        let legacy = LegacyNewSessionParameters::deserialize(value).map_err(de::Error::custom)?;
        Ok(NewSessionParameters::Legacy(legacy))
    }
}

impl CapabilitiesMatching for NewSessionParameters {
    fn match_browser<T: BrowserCapabilities>(
        &self,
        browser_capabilities: &mut T,
    ) -> WebDriverResult<Option<Capabilities>> {
        match self {
            NewSessionParameters::Spec(x) => x.match_browser(browser_capabilities),
            NewSessionParameters::Legacy(x) => x.match_browser(browser_capabilities),
        }
    }
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct NewWindowParameters {
    #[serde(rename = "type")]
    pub type_hint: Option<String>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct SendKeysParameters {
    pub text: String,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct SwitchToFrameParameters {
    pub id: Option<FrameId>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct SwitchToWindowParameters {
    pub handle: String,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct TakeScreenshotParameters {
    pub element: Option<WebElement>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct TimeoutsParameters {
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_u64"
    )]
    pub implicit: Option<u64>,
    #[serde(
        default,
        rename = "pageLoad",
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_u64"
    )]
    pub page_load: Option<u64>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_nullable_u64"
    )]
    pub script: Option<Option<u64>>,
}

fn deserialize_to_nullable_u64<'de, D>(deserializer: D) -> Result<Option<Option<u64>>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt = Option::deserialize(deserializer)?.map(|value: f64| value);
    let value = match opt {
        Some(n) => {
            if n < 0.0 || n.fract() != 0.0 {
                return Err(de::Error::custom(format!(
                    "{} is not a positive Integer",
                    n
                )));
            }
            if (n as u64) > MAX_SAFE_INTEGER {
                return Err(de::Error::custom(format!(
                    "{} is greater than maximum safe integer",
                    n
                )));
            }
            Some(Some(n as u64))
        }
        None => Some(None),
    };

    Ok(value)
}

fn deserialize_to_u64<'de, D>(deserializer: D) -> Result<Option<u64>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt = Option::deserialize(deserializer)?.map(|value: f64| value);
    let value = match opt {
        Some(n) => {
            if n < 0.0 || n.fract() != 0.0 {
                return Err(de::Error::custom(format!(
                    "{} is not a positive Integer",
                    n
                )));
            }
            if (n as u64) > MAX_SAFE_INTEGER {
                return Err(de::Error::custom(format!(
                    "{} is greater than maximum safe integer",
                    n
                )));
            }
            Some(n as u64)
        }
        None => return Err(de::Error::custom("null is not a positive integer")),
    };

    Ok(value)
}

/// A top-level browsing context’s window rect is a dictionary of the
/// [`screenX`], [`screenY`], `width`, and `height` attributes of the
/// `WindowProxy`.
///
/// In some user agents the operating system’s window dimensions, including
/// decorations, are provided by the proprietary `window.outerWidth` and
/// `window.outerHeight` DOM properties.
///
/// [`screenX`]: https://w3c.github.io/webdriver/webdriver-spec.html#dfn-screenx
/// [`screenY`]: https://w3c.github.io/webdriver/webdriver-spec.html#dfn-screeny
#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct WindowRectParameters {
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_i32"
    )]
    pub x: Option<i32>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_i32"
    )]
    pub y: Option<i32>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_positive_i32"
    )]
    pub width: Option<i32>,
    #[serde(
        default,
        skip_serializing_if = "Option::is_none",
        deserialize_with = "deserialize_to_positive_i32"
    )]
    pub height: Option<i32>,
}

fn deserialize_to_i32<'de, D>(deserializer: D) -> Result<Option<i32>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt = Option::deserialize(deserializer)?.map(|value: f64| value as i64);
    let value = match opt {
        Some(n) => {
            if n < i32::min_value() as i64 || n > i32::max_value() as i64 {
                return Err(de::Error::custom(format!("'{}' is larger than i32", n)));
            }
            Some(n as i32)
        }
        None => None,
    };

    Ok(value)
}

fn deserialize_to_positive_i32<'de, D>(deserializer: D) -> Result<Option<i32>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt = Option::deserialize(deserializer)?.map(|value: f64| value as i64);
    let value = match opt {
        Some(n) => {
            if n < 0 || n > i32::max_value() as i64 {
                return Err(de::Error::custom(format!("'{}' is outside of i32", n)));
            }
            Some(n as i32)
        }
        None => None,
    };

    Ok(value)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::capabilities::SpecNewSessionParameters;
    use crate::test::check_deserialize;
    use serde_json;

    #[test]
    fn test_json_actions_parameters_missing_actions_field() {
        let json = r#"{}"#;
        assert!(serde_json::from_str::<ActionsParameters>(&json).is_err());
    }

    #[test]
    fn test_json_actions_parameters_invalid() {
        let json = r#"{"actions":null}"#;
        assert!(serde_json::from_str::<ActionsParameters>(&json).is_err());
    }

    #[test]
    fn test_json_action_parameters_empty_list() {
        let json = r#"{"actions":[]}"#;
        let data = ActionsParameters { actions: vec![] };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_action_parameters_with_unknown_field() {
        let json = r#"{"actions":[],"foo":"bar"}"#;
        let data = ActionsParameters { actions: vec![] };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_add_cookie_parameters_with_values() {
        let json = r#"{"cookie":{
            "name":"foo",
            "value":"bar",
            "path":"/",
            "domain":"foo.bar",
            "expiry":123,
            "secure":true,
            "httpOnly":false
        }}"#;
        let data = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: Some("/".into()),
            domain: Some("foo.bar".into()),
            expiry: Some(Date(123)),
            secure: true,
            httpOnly: false,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_add_cookie_parameters_with_optional_null_fields() {
        let json = r#"{"cookie":{
            "name":"foo",
            "value":"bar",
            "path":null,
            "domain":null,
            "expiry":null,
            "secure":true,
            "httpOnly":false
        }}"#;
        let data = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: None,
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_add_cookie_parameters_without_optional_fields() {
        let json = r#"{"cookie":{
            "name":"foo",
            "value":"bar",
            "secure":true,
            "httpOnly":false
        }}"#;
        let data = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: None,
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_add_cookie_parameters_with_invalid_cookie_field() {
        let json = r#"{"name":"foo"}"#;

        assert!(serde_json::from_str::<AddCookieParameters>(&json).is_err());
    }

    #[test]
    fn test_json_add_cookie_parameters_with_unknown_field() {
        let json = r#"{"cookie":{
            "name":"foo",
            "value":"bar",
            "secure":true,
            "httpOnly":false,
            "foo":"bar"
        },"foo":"bar"}"#;
        let data = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: None,
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_get_parameters_with_url() {
        let json = r#"{"url":"foo.bar"}"#;
        let data = GetParameters {
            url: "foo.bar".into(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_get_parameters_with_invalid_url_value() {
        let json = r#"{"url":3}"#;

        assert!(serde_json::from_str::<GetParameters>(&json).is_err());
    }

    #[test]
    fn test_json_get_parameters_with_invalid_url_field() {
        let json = r#"{"foo":"bar"}"#;

        assert!(serde_json::from_str::<GetParameters>(&json).is_err());
    }

    #[test]
    fn test_json_get_parameters_with_unknown_field() {
        let json = r#"{"url":"foo.bar","foo":"bar"}"#;
        let data = GetParameters {
            url: "foo.bar".into(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_value() {
        let json = r#"{"name":"foo"}"#;
        let data = GetNamedCookieParameters {
            name: Some("foo".into()),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_optional_null_field() {
        let json = r#"{"name":null}"#;
        let data = GetNamedCookieParameters { name: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_get_named_cookie_parameters_without_optional_null_field() {
        let json = r#"{}"#;
        let data = GetNamedCookieParameters { name: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_invalid_name_field() {
        let json = r#"{"name":3"#;

        assert!(serde_json::from_str::<GetNamedCookieParameters>(&json).is_err());
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_unknown_field() {
        let json = r#"{"name":"foo","foo":"bar"}"#;
        let data = GetNamedCookieParameters {
            name: Some("foo".into()),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_javascript_command_parameters_with_values() {
        let json = r#"{"script":"foo","args":["1",2]}"#;
        let data = JavascriptCommandParameters {
            script: "foo".into(),
            args: Some(vec!["1".into(), 2.into()]),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_javascript_command_parameters_with_optional_null_field() {
        let json = r#"{"script":"foo","args":null}"#;
        let data = JavascriptCommandParameters {
            script: "foo".into(),
            args: None,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_javascript_command_parameters_without_optional_null_field() {
        let json = r#"{"script":"foo"}"#;
        let data = JavascriptCommandParameters {
            script: "foo".into(),
            args: None,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_javascript_command_parameters_invalid_script_field() {
        let json = r#"{"script":null}"#;

        assert!(serde_json::from_str::<JavascriptCommandParameters>(&json).is_err());
    }

    #[test]
    fn test_json_javascript_command_parameters_invalid_args_field() {
        let json = r#"{"script":null,"args":"1"}"#;

        assert!(serde_json::from_str::<JavascriptCommandParameters>(&json).is_err());
    }

    #[test]
    fn test_json_javascript_command_parameters_missing_script_field() {
        let json = r#"{"args":null}"#;

        assert!(serde_json::from_str::<JavascriptCommandParameters>(&json).is_err());
    }

    #[test]
    fn test_json_javascript_command_parameters_with_unknown_field() {
        let json = r#"{"script":"foo","foo":"bar"}"#;
        let data = JavascriptCommandParameters {
            script: "foo".into(),
            args: None,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_locator_parameters_with_values() {
        let json = r#"{"using":"xpath","value":"bar"}"#;
        let data = LocatorParameters {
            using: LocatorStrategy::XPath,
            value: "bar".into(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_locator_parameters_invalid_using_field() {
        let json = r#"{"using":"foo","value":"bar"}"#;

        assert!(serde_json::from_str::<LocatorParameters>(&json).is_err());
    }

    #[test]
    fn test_json_locator_parameters_invalid_value_field() {
        let json = r#"{"using":"xpath","value":3}"#;

        assert!(serde_json::from_str::<LocatorParameters>(&json).is_err());
    }

    #[test]
    fn test_json_locator_parameters_missing_using_field() {
        let json = r#"{"value":"bar"}"#;

        assert!(serde_json::from_str::<LocatorParameters>(&json).is_err());
    }

    #[test]
    fn test_json_locator_parameters_missing_value_field() {
        let json = r#"{"using":"xpath"}"#;

        assert!(serde_json::from_str::<LocatorParameters>(&json).is_err());
    }

    #[test]
    fn test_json_locator_parameters_with_unknown_field() {
        let json = r#"{"using":"xpath","value":"bar","foo":"bar"}"#;
        let data = LocatorParameters {
            using: LocatorStrategy::XPath,
            value: "bar".into(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_session_parameters_spec() {
        let json = r#"{"capabilities":{"alwaysMatch":{},"firstMatch":[{}]}}"#;
        let data = NewSessionParameters::Spec(SpecNewSessionParameters {
            alwaysMatch: Capabilities::new(),
            firstMatch: vec![Capabilities::new()],
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_session_parameters_capabilities_null() {
        let json = r#"{"capabilities":null}"#;

        assert!(serde_json::from_str::<NewSessionParameters>(&json).is_err());
    }

    #[test]
    fn test_json_new_session_parameters_legacy() {
        let json = r#"{"desired":{},"required":{}}"#;
        let data = NewSessionParameters::Legacy(LegacyNewSessionParameters {
            desired: Capabilities::new(),
            required: Capabilities::new(),
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_session_parameters_spec_and_legacy() {
        let json = r#"{
            "capabilities":{
                "alwaysMatch":{},
                "firstMatch":[{}]
            },
            "desired":{},
            "required":{}
        }"#;
        let data = NewSessionParameters::Spec(SpecNewSessionParameters {
            alwaysMatch: Capabilities::new(),
            firstMatch: vec![Capabilities::new()],
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_session_parameters_with_unknown_field() {
        let json = r#"{
            "capabilities":{
                "alwaysMatch":{},
                "firstMatch":[{}]
            },
            "foo":"bar"}"#;
        let data = NewSessionParameters::Spec(SpecNewSessionParameters {
            alwaysMatch: Capabilities::new(),
            firstMatch: vec![Capabilities::new()],
        });

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_window_parameters_without_type() {
        let json = r#"{}"#;
        let data = NewWindowParameters { type_hint: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_window_parameters_with_optional_null_type() {
        let json = r#"{"type":null}"#;
        let data = NewWindowParameters { type_hint: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_window_parameters_with_supported_type() {
        let json = r#"{"type":"tab"}"#;
        let data = NewWindowParameters {
            type_hint: Some("tab".into()),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_window_parameters_with_unknown_type() {
        let json = r#"{"type":"foo"}"#;
        let data = NewWindowParameters {
            type_hint: Some("foo".into()),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_new_window_parameters_with_invalid_type() {
        let json = r#"{"type":3}"#;

        assert!(serde_json::from_str::<NewWindowParameters>(&json).is_err());
    }

    #[test]
    fn test_json_new_window_parameters_with_unknown_field() {
        let json = r#"{"type":"tab","foo":"bar"}"#;
        let data = NewWindowParameters {
            type_hint: Some("tab".into()),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_send_keys_parameters_with_value() {
        let json = r#"{"text":"foo"}"#;
        let data = SendKeysParameters { text: "foo".into() };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_send_keys_parameters_invalid_text_field() {
        let json = r#"{"text":3}"#;

        assert!(serde_json::from_str::<SendKeysParameters>(&json).is_err());
    }

    #[test]
    fn test_json_send_keys_parameters_missing_text_field() {
        let json = r#"{}"#;

        assert!(serde_json::from_str::<SendKeysParameters>(&json).is_err());
    }

    #[test]
    fn test_json_send_keys_parameters_with_unknown_field() {
        let json = r#"{"text":"foo","foo":"bar"}"#;
        let data = SendKeysParameters { text: "foo".into() };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_value() {
        let json = r#"{"id":3}"#;
        let data = SwitchToFrameParameters {
            id: Some(FrameId::Short(3)),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_optional_null_field() {
        let json = r#"{"id":null}"#;
        let data = SwitchToFrameParameters { id: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_switch_to_frame_parameters_without_optional_null_field() {
        let json = r#"{}"#;
        let data = SwitchToFrameParameters { id: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_invalid_id_field() {
        let json = r#"{"id":"3"}"#;

        assert!(serde_json::from_str::<SwitchToFrameParameters>(&json).is_err());
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_unknown_field() {
        let json = r#"{"id":3,"foo":"bar"}"#;
        let data = SwitchToFrameParameters {
            id: Some(FrameId::Short(3)),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_switch_to_window_parameters_with_value() {
        let json = r#"{"handle":"foo"}"#;
        let data = SwitchToWindowParameters {
            handle: "foo".into(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_switch_to_window_parameters_invalid_handle_field() {
        let json = r#"{"handle":3}"#;

        assert!(serde_json::from_str::<SwitchToWindowParameters>(&json).is_err());
    }

    #[test]
    fn test_json_switch_to_window_parameters_missing_handle_field() {
        let json = r#"{}"#;

        assert!(serde_json::from_str::<SwitchToWindowParameters>(&json).is_err());
    }

    #[test]
    fn test_json_switch_to_window_parameters_with_unknown_field() {
        let json = r#"{"handle":"foo","foo":"bar"}"#;
        let data = SwitchToWindowParameters {
            handle: "foo".into(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_element() {
        let json = r#"{"element":{"element-6066-11e4-a52e-4f735466cecf":"elem"}}"#;
        let data = TakeScreenshotParameters {
            element: Some(WebElement("elem".into())),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_optional_null_field() {
        let json = r#"{"element":null}"#;
        let data = TakeScreenshotParameters { element: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_take_screenshot_parameters_without_optional_null_field() {
        let json = r#"{}"#;
        let data = TakeScreenshotParameters { element: None };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_invalid_element_field() {
        let json = r#"{"element":"foo"}"#;
        assert!(serde_json::from_str::<TakeScreenshotParameters>(&json).is_err());
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_unknown_field() {
        let json = r#"{"element":{"element-6066-11e4-a52e-4f735466cecf":"elem"},"foo":"bar"}"#;
        let data = TakeScreenshotParameters {
            element: Some(WebElement("elem".into())),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_timeout_parameters_with_values() {
        let json = r#"{"implicit":0,"pageLoad":2.0,"script":9007199254740991}"#;
        let data = TimeoutsParameters {
            implicit: Some(0u64),
            page_load: Some(2u64),
            script: Some(Some(9007199254740991u64)),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_timeout_parameters_with_invalid_values() {
        let json = r#"{"implicit":-1,"pageLoad":2.5,"script":9007199254740992}"#;
        assert!(serde_json::from_str::<TimeoutsParameters>(&json).is_err());
    }

    #[test]
    fn test_json_timeout_parameters_with_only_null_script_timeout() {
        let json = r#"{"script":null}"#;
        let data = TimeoutsParameters {
            implicit: None,
            page_load: None,
            script: Some(None),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_timeout_parameters_with_only_null_implicit_timeout() {
        let json = r#"{"implicit":null}"#;

        assert!(serde_json::from_str::<TimeoutsParameters>(&json).is_err());
    }

    #[test]
    fn test_json_timeout_parameters_with_only_null_pageload_timeout() {
        let json = r#"{"pageLoad":null}"#;

        assert!(serde_json::from_str::<TimeoutsParameters>(&json).is_err());
    }

    #[test]
    fn test_json_timeout_parameters_without_optional_null_field() {
        let json = r#"{}"#;
        let data = TimeoutsParameters {
            implicit: None,
            page_load: None,
            script: None,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_timeout_parameters_with_unknown_field() {
        let json = r#"{"script":60000,"foo":"bar"}"#;
        let data = TimeoutsParameters {
            implicit: None,
            page_load: None,
            script: Some(Some(60000)),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_window_rect_parameters_with_values() {
        let json = r#"{"x":0,"y":1,"width":2,"height":3}"#;
        let data = WindowRectParameters {
            x: Some(0i32),
            y: Some(1i32),
            width: Some(2i32),
            height: Some(3i32),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_window_rect_parameters_with_optional_null_fields() {
        let json = r#"{"x":null,"y": null,"width":null,"height":null}"#;
        let data = WindowRectParameters {
            x: None,
            y: None,
            width: None,
            height: None,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_window_rect_parameters_without_optional_fields() {
        let json = r#"{}"#;
        let data = WindowRectParameters {
            x: None,
            y: None,
            width: None,
            height: None,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_window_rect_parameters_invalid_values_float() {
        let json = r#"{"x":1.1,"y":2.2,"width":3.3,"height":4.4}"#;
        let data = WindowRectParameters {
            x: Some(1),
            y: Some(2),
            width: Some(3),
            height: Some(4),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_window_rect_parameters_with_unknown_field() {
        let json = r#"{"x":1.1,"y":2.2,"foo":"bar"}"#;
        let data = WindowRectParameters {
            x: Some(1),
            y: Some(2),
            width: None,
            height: None,
        };

        check_deserialize(&json, &data);
    }
}
