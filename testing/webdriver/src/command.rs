/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::actions::ActionSequence;
use crate::capabilities::{
    BrowserCapabilities, Capabilities, CapabilitiesMatching, LegacyNewSessionParameters,
    SpecNewSessionParameters,
};
use crate::common::{
    CredentialParameters, Date, FrameId, LocatorStrategy, ShadowRoot, WebElement, MAX_SAFE_INTEGER,
};
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
    FindShadowRootElement(ShadowRoot, LocatorParameters),
    FindShadowRootElements(ShadowRoot, LocatorParameters),
    GetActiveElement,
    GetComputedLabel(WebElement),
    GetComputedRole(WebElement),
    GetShadowRoot(WebElement),
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
    Print(PrintParameters),
    Status,
    Extension(T),
    WebAuthnAddVirtualAuthenticator(AuthenticatorParameters),
    WebAuthnRemoveVirtualAuthenticator,
    WebAuthnAddCredential(CredentialParameters),
    WebAuthnGetCredentials,
    WebAuthnRemoveCredential,
    WebAuthnRemoveAllCredentials,
    WebAuthnSetUserVerified(UserVerificationParameters),
}

pub trait WebDriverExtensionCommand: Clone + Send {
    fn parameters_json(&self) -> Option<Value>;
}

#[derive(Clone, Debug)]
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
            Route::FindShadowRootElement => {
                let shadow_id = try_opt!(
                    params.get("shadowId"),
                    ErrorStatus::InvalidArgument,
                    "Missing shadowId parameter"
                );
                let shadow_root = ShadowRoot(shadow_id.as_str().into());
                WebDriverCommand::FindShadowRootElement(
                    shadow_root,
                    serde_json::from_str(raw_body)?,
                )
            }
            Route::FindShadowRootElements => {
                let shadow_id = try_opt!(
                    params.get("shadowId"),
                    ErrorStatus::InvalidArgument,
                    "Missing shadowId parameter"
                );
                let shadow_root = ShadowRoot(shadow_id.as_str().into());
                WebDriverCommand::FindShadowRootElements(
                    shadow_root,
                    serde_json::from_str(raw_body)?,
                )
            }
            Route::GetActiveElement => WebDriverCommand::GetActiveElement,
            Route::GetShadowRoot => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::GetShadowRoot(element)
            }
            Route::GetComputedLabel => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::GetComputedLabel(element)
            }
            Route::GetComputedRole => {
                let element_id = try_opt!(
                    params.get("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement(element_id.as_str().into());
                WebDriverCommand::GetComputedRole(element)
            }
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
            Route::Print => WebDriverCommand::Print(serde_json::from_str(raw_body)?),
            Route::Status => WebDriverCommand::Status,
            Route::Extension(ref extension) => extension.command(params, &body_data)?,
            Route::WebAuthnAddVirtualAuthenticator => {
                WebDriverCommand::WebAuthnAddVirtualAuthenticator(serde_json::from_str(raw_body)?)
            }
            Route::WebAuthnRemoveVirtualAuthenticator => {
                WebDriverCommand::WebAuthnRemoveVirtualAuthenticator
            }
            Route::WebAuthnAddCredential => {
                WebDriverCommand::WebAuthnAddCredential(serde_json::from_str(raw_body)?)
            }
            Route::WebAuthnGetCredentials => WebDriverCommand::WebAuthnGetCredentials,
            Route::WebAuthnRemoveCredential => WebDriverCommand::WebAuthnRemoveCredential,
            Route::WebAuthnRemoveAllCredentials => WebDriverCommand::WebAuthnRemoveAllCredentials,
            Route::WebAuthnSetUserVerified => {
                WebDriverCommand::WebAuthnSetUserVerified(serde_json::from_str(raw_body)?)
            }
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
    pub sameSite: Option<String>,
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
    #[serde(deserialize_with = "deserialize_to_print_scale_f64")]
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

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum PrintOrientation {
    Landscape,
    #[default]
    Portrait,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct PrintPage {
    #[serde(deserialize_with = "deserialize_to_positive_f64")]
    pub width: f64,
    #[serde(deserialize_with = "deserialize_to_positive_f64")]
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
#[serde(default)]
pub struct PrintMargins {
    #[serde(deserialize_with = "deserialize_to_positive_f64")]
    pub top: f64,
    #[serde(deserialize_with = "deserialize_to_positive_f64")]
    pub bottom: f64,
    #[serde(deserialize_with = "deserialize_to_positive_f64")]
    pub left: f64,
    #[serde(deserialize_with = "deserialize_to_positive_f64")]
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

fn default_as_true() -> bool {
    true
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct AuthenticatorParameters {
    pub protocol: WebAuthnProtocol,
    pub transport: AuthenticatorTransport,
    #[serde(default)]
    pub has_resident_key: bool,
    #[serde(default)]
    pub has_user_verification: bool,
    #[serde(default = "default_as_true")]
    pub is_user_consenting: bool,
    #[serde(default)]
    pub is_user_verified: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct UserVerificationParameters {
    #[serde(rename = "isUserVerified")]
    pub is_user_verified: bool,
}

fn deserialize_to_positive_f64<'de, D>(deserializer: D) -> Result<f64, D::Error>
where
    D: Deserializer<'de>,
{
    let val = f64::deserialize(deserializer)?;
    if val < 0.0 {
        return Err(de::Error::custom(format!("{} is negative", val)));
    };
    Ok(val)
}

fn deserialize_to_print_scale_f64<'de, D>(deserializer: D) -> Result<f64, D::Error>
where
    D: Deserializer<'de>,
{
    let val = f64::deserialize(deserializer)?;
    if !(0.1..=2.0).contains(&val) {
        return Err(de::Error::custom(format!("{} is outside range 0.1-2", val)));
    };
    Ok(val)
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct SendKeysParameters {
    pub text: String,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct SwitchToFrameParameters {
    pub id: FrameId,
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
    #[allow(clippy::option_option)]
    pub script: Option<Option<u64>>,
}

#[allow(clippy::option_option)]
fn deserialize_to_nullable_u64<'de, D>(deserializer: D) -> Result<Option<Option<u64>>, D::Error>
where
    D: Deserializer<'de>,
{
    let opt: Option<f64> = Option::deserialize(deserializer)?;
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
    let opt: Option<f64> = Option::deserialize(deserializer)?;
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
            if n < i64::from(i32::min_value()) || n > i64::from(i32::max_value()) {
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
            if n < 0 || n > i64::from(i32::max_value()) {
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
    use crate::common::ELEMENT_KEY;
    use crate::test::assert_de;
    use serde_json::{self, json};

    #[test]
    fn test_json_actions_parameters_missing_actions_field() {
        assert!(serde_json::from_value::<ActionsParameters>(json!({})).is_err());
    }

    #[test]
    fn test_json_actions_parameters_invalid() {
        assert!(serde_json::from_value::<ActionsParameters>(json!({ "actions": null })).is_err());
    }

    #[test]
    fn test_json_action_parameters_empty_list() {
        assert_de(
            &ActionsParameters { actions: vec![] },
            json!({"actions": []}),
        );
    }

    #[test]
    fn test_json_action_parameters_with_unknown_field() {
        assert_de(
            &ActionsParameters { actions: vec![] },
            json!({"actions": [], "foo": "bar"}),
        );
    }

    #[test]
    fn test_json_add_cookie_parameters_with_values() {
        let json = json!({"cookie": {
            "name": "foo",
            "value": "bar",
            "path": "/",
            "domain": "foo.bar",
            "expiry": 123,
            "secure": true,
            "httpOnly": false,
            "sameSite": "Lax",
        }});
        let cookie = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: Some("/".into()),
            domain: Some("foo.bar".into()),
            expiry: Some(Date(123)),
            secure: true,
            httpOnly: false,
            sameSite: Some("Lax".into()),
        };

        assert_de(&cookie, json);
    }

    #[test]
    fn test_json_add_cookie_parameters_with_optional_null_fields() {
        let json = json!({"cookie": {
            "name": "foo",
            "value": "bar",
            "path": null,
            "domain": null,
            "expiry": null,
            "secure": true,
            "httpOnly": false,
            "sameSite": null,
        }});
        let cookie = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: None,
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
            sameSite: None,
        };

        assert_de(&cookie, json);
    }

    #[test]
    fn test_json_add_cookie_parameters_without_optional_fields() {
        let json = json!({"cookie": {
            "name": "foo",
            "value": "bar",
            "secure": true,
            "httpOnly": false,
        }});
        let cookie = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: None,
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
            sameSite: None,
        };

        assert_de(&cookie, json);
    }

    #[test]
    fn test_json_add_cookie_parameters_with_invalid_cookie_field() {
        assert!(serde_json::from_value::<AddCookieParameters>(json!({"name": "foo"})).is_err());
    }

    #[test]
    fn test_json_add_cookie_parameters_with_unknown_field() {
        let json = json!({"cookie": {
            "name": "foo",
            "value": "bar",
            "secure": true,
            "httpOnly": false,
            "foo": "bar",
        }, "baz": "bah"});
        let cookie = AddCookieParameters {
            name: "foo".into(),
            value: "bar".into(),
            path: None,
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
            sameSite: None,
        };

        assert_de(&cookie, json);
    }

    #[test]
    fn test_json_get_parameters_with_url() {
        assert_de(
            &GetParameters {
                url: "foo.bar".into(),
            },
            json!({"url": "foo.bar"}),
        );
    }

    #[test]
    fn test_json_get_parameters_with_invalid_url_value() {
        assert!(serde_json::from_value::<GetParameters>(json!({"url": 3})).is_err());
    }

    #[test]
    fn test_json_get_parameters_with_invalid_url_field() {
        assert!(serde_json::from_value::<GetParameters>(json!({"foo": "bar"})).is_err());
    }

    #[test]
    fn test_json_get_parameters_with_unknown_field() {
        assert_de(
            &GetParameters {
                url: "foo.bar".into(),
            },
            json!({"url": "foo.bar", "foo": "bar"}),
        );
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_value() {
        assert_de(
            &GetNamedCookieParameters {
                name: Some("foo".into()),
            },
            json!({"name": "foo"}),
        );
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_optional_null_field() {
        assert_de(
            &GetNamedCookieParameters { name: None },
            json!({ "name": null }),
        );
    }

    #[test]
    fn test_json_get_named_cookie_parameters_without_optional_null_field() {
        assert_de(&GetNamedCookieParameters { name: None }, json!({}));
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_invalid_name_field() {
        assert!(serde_json::from_value::<GetNamedCookieParameters>(json!({"name": 3})).is_err());
    }

    #[test]
    fn test_json_get_named_cookie_parameters_with_unknown_field() {
        assert_de(
            &GetNamedCookieParameters {
                name: Some("foo".into()),
            },
            json!({"name": "foo", "foo": "bar"}),
        );
    }

    #[test]
    fn test_json_javascript_command_parameters_with_values() {
        let json = json!({
            "script": "foo",
            "args": ["1", 2],
        });
        let execute_script = JavascriptCommandParameters {
            script: "foo".into(),
            args: Some(vec!["1".into(), 2.into()]),
        };

        assert_de(&execute_script, json);
    }

    #[test]
    fn test_json_javascript_command_parameters_with_optional_null_field() {
        let json = json!({
            "script": "foo",
            "args": null,
        });
        let execute_script = JavascriptCommandParameters {
            script: "foo".into(),
            args: None,
        };

        assert_de(&execute_script, json);
    }

    #[test]
    fn test_json_javascript_command_parameters_without_optional_null_field() {
        let execute_script = JavascriptCommandParameters {
            script: "foo".into(),
            args: None,
        };
        assert_de(&execute_script, json!({"script": "foo"}));
    }

    #[test]
    fn test_json_javascript_command_parameters_invalid_script_field() {
        let json = json!({ "script": null });
        assert!(serde_json::from_value::<JavascriptCommandParameters>(json).is_err());
    }

    #[test]
    fn test_json_javascript_command_parameters_invalid_args_field() {
        let json = json!({
            "script": null,
            "args": "1",
        });
        assert!(serde_json::from_value::<JavascriptCommandParameters>(json).is_err());
    }

    #[test]
    fn test_json_javascript_command_parameters_missing_script_field() {
        let json = json!({ "args": null });
        assert!(serde_json::from_value::<JavascriptCommandParameters>(json).is_err());
    }

    #[test]
    fn test_json_javascript_command_parameters_with_unknown_field() {
        let json = json!({
            "script": "foo",
            "foo": "bar",
        });
        let execute_script = JavascriptCommandParameters {
            script: "foo".into(),
            args: None,
        };

        assert_de(&execute_script, json);
    }

    #[test]
    fn test_json_locator_parameters_with_values() {
        let json = json!({
            "using": "xpath",
            "value": "bar",
        });
        let locator = LocatorParameters {
            using: LocatorStrategy::XPath,
            value: "bar".into(),
        };

        assert_de(&locator, json);
    }

    #[test]
    fn test_json_locator_parameters_invalid_using_field() {
        let json = json!({
            "using": "foo",
            "value": "bar",
        });
        assert!(serde_json::from_value::<LocatorParameters>(json).is_err());
    }

    #[test]
    fn test_json_locator_parameters_invalid_value_field() {
        let json = json!({
            "using": "xpath",
            "value": 3,
        });
        assert!(serde_json::from_value::<LocatorParameters>(json).is_err());
    }

    #[test]
    fn test_json_locator_parameters_missing_using_field() {
        assert!(serde_json::from_value::<LocatorParameters>(json!({"value": "bar"})).is_err());
    }

    #[test]
    fn test_json_locator_parameters_missing_value_field() {
        assert!(serde_json::from_value::<LocatorParameters>(json!({"using": "xpath"})).is_err());
    }

    #[test]
    fn test_json_locator_parameters_with_unknown_field() {
        let json = json!({
            "using": "xpath",
            "value": "bar",
            "foo": "bar",
        });
        let locator = LocatorParameters {
            using: LocatorStrategy::XPath,
            value: "bar".into(),
        };

        assert_de(&locator, json);
    }

    #[test]
    fn test_json_new_session_parameters_spec() {
        let json = json!({"capabilities": {
            "alwaysMatch": {},
            "firstMatch": [{}],
        }});
        let caps = NewSessionParameters::Spec(SpecNewSessionParameters {
            alwaysMatch: Capabilities::new(),
            firstMatch: vec![Capabilities::new()],
        });

        assert_de(&caps, json);
    }

    #[test]
    fn test_json_new_session_parameters_capabilities_null() {
        let json = json!({ "capabilities": null });
        assert!(serde_json::from_value::<NewSessionParameters>(json).is_err());
    }

    #[test]
    fn test_json_new_session_parameters_legacy() {
        let json = json!({
            "desiredCapabilities": {},
            "requiredCapabilities": {},
        });
        let caps = NewSessionParameters::Legacy(LegacyNewSessionParameters {
            desired: Capabilities::new(),
            required: Capabilities::new(),
        });

        assert_de(&caps, json);
    }

    #[test]
    fn test_json_new_session_parameters_spec_and_legacy() {
        let json = json!({
            "capabilities": {
                "alwaysMatch": {},
                "firstMatch": [{}],
            },
            "desiredCapabilities": {},
            "requiredCapabilities": {},
        });
        let caps = NewSessionParameters::Spec(SpecNewSessionParameters {
            alwaysMatch: Capabilities::new(),
            firstMatch: vec![Capabilities::new()],
        });

        assert_de(&caps, json);
    }

    #[test]
    fn test_json_new_session_parameters_with_unknown_field() {
        let json = json!({
            "capabilities": {
                "alwaysMatch": {},
                "firstMatch": [{}]
            },
            "foo": "bar",
        });
        let caps = NewSessionParameters::Spec(SpecNewSessionParameters {
            alwaysMatch: Capabilities::new(),
            firstMatch: vec![Capabilities::new()],
        });

        assert_de(&caps, json);
    }

    #[test]
    fn test_json_new_window_parameters_without_type() {
        assert_de(&NewWindowParameters { type_hint: None }, json!({}));
    }

    #[test]
    fn test_json_new_window_parameters_with_optional_null_type() {
        assert_de(
            &NewWindowParameters { type_hint: None },
            json!({ "type": null }),
        );
    }

    #[test]
    fn test_json_new_window_parameters_with_supported_type() {
        assert_de(
            &NewWindowParameters {
                type_hint: Some("tab".into()),
            },
            json!({"type": "tab"}),
        );
    }

    #[test]
    fn test_json_new_window_parameters_with_unknown_type() {
        assert_de(
            &NewWindowParameters {
                type_hint: Some("foo".into()),
            },
            json!({"type": "foo"}),
        );
    }

    #[test]
    fn test_json_new_window_parameters_with_invalid_type() {
        assert!(serde_json::from_value::<NewWindowParameters>(json!({"type": 3})).is_err());
    }

    #[test]
    fn test_json_new_window_parameters_with_unknown_field() {
        let json = json!({
            "type": "tab",
            "foo": "bar",
        });
        let new_window = NewWindowParameters {
            type_hint: Some("tab".into()),
        };

        assert_de(&new_window, json);
    }

    #[test]
    fn test_json_print_defaults() {
        let params = PrintParameters::default();
        assert_de(&params, json!({}));
    }

    #[test]
    fn test_json_print() {
        let params = PrintParameters {
            orientation: PrintOrientation::Landscape,
            page: PrintPage {
                width: 10.0,
                ..Default::default()
            },
            margin: PrintMargins {
                top: 10.0,
                ..Default::default()
            },
            scale: 1.5,
            ..Default::default()
        };
        assert_de(
            &params,
            json!({"orientation": "landscape", "page": {"width": 10}, "margin": {"top": 10}, "scale": 1.5}),
        );
    }

    #[test]
    fn test_json_scale_invalid() {
        assert!(serde_json::from_value::<PrintParameters>(json!({"scale": 3})).is_err());
    }

    #[test]
    fn test_json_authenticator() {
        let params = AuthenticatorParameters {
            protocol: WebAuthnProtocol::Ctap1U2f,
            transport: AuthenticatorTransport::Usb,
            has_resident_key: false,
            has_user_verification: false,
            is_user_consenting: false,
            is_user_verified: false,
        };
        assert_de(
            &params,
            json!({"protocol": "ctap1/u2f", "transport": "usb", "hasResidentKey": false, "hasUserVerification": false, "isUserConsenting": false, "isUserVerified": false}),
        );
    }

    #[test]
    fn test_json_credential() {
        use base64::{engine::general_purpose::URL_SAFE, Engine};

        let encoded_string = URL_SAFE.encode(b"hello internet~");
        let params = CredentialParameters {
            credential_id: r"c3VwZXIgcmVhZGVy".to_string(),
            is_resident_credential: true,
            rp_id: "valid.rpid".to_string(),
            private_key: encoded_string.clone(),
            user_handle: encoded_string.clone(),
            sign_count: 0,
        };
        assert_de(
            &params,
            json!({"credentialId": r"c3VwZXIgcmVhZGVy", "isResidentCredential": true, "rpId": "valid.rpid", "privateKey": encoded_string, "userHandle": encoded_string, "signCount": 0}),
        );
    }

    #[test]
    fn test_json_user_verification() {
        let params = UserVerificationParameters {
            is_user_verified: false,
        };
        assert_de(&params, json!({"isUserVerified": false}));
    }

    #[test]
    fn test_json_send_keys_parameters_with_value() {
        assert_de(
            &SendKeysParameters { text: "foo".into() },
            json!({"text": "foo"}),
        );
    }

    #[test]
    fn test_json_send_keys_parameters_invalid_text_field() {
        assert!(serde_json::from_value::<SendKeysParameters>(json!({"text": 3})).is_err());
    }

    #[test]
    fn test_json_send_keys_parameters_missing_text_field() {
        assert!(serde_json::from_value::<SendKeysParameters>(json!({})).is_err());
    }

    #[test]
    fn test_json_send_keys_parameters_with_unknown_field() {
        let json = json!({
            "text": "foo",
            "foo": "bar",
        });
        let send_keys = SendKeysParameters { text: "foo".into() };

        assert_de(&send_keys, json);
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_number() {
        assert_de(
            &SwitchToFrameParameters {
                id: FrameId::Short(3),
            },
            json!({"id": 3}),
        );
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_null() {
        assert_de(
            &SwitchToFrameParameters { id: FrameId::Top },
            json!({"id": null}),
        );
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_web_element() {
        assert_de(
            &SwitchToFrameParameters {
                id: FrameId::Element(WebElement("foo".to_string())),
            },
            json!({"id": {"element-6066-11e4-a52e-4f735466cecf": "foo"}}),
        );
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_missing_id() {
        assert!(serde_json::from_value::<SwitchToFrameParameters>(json!({})).is_err())
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_invalid_id_field() {
        assert!(serde_json::from_value::<SwitchToFrameParameters>(json!({"id": "3"})).is_err());
    }

    #[test]
    fn test_json_switch_to_frame_parameters_with_unknown_field() {
        let json = json!({
            "id":3,
            "foo": "bar",
        });
        let switch_to_frame = SwitchToFrameParameters {
            id: FrameId::Short(3),
        };

        assert_de(&switch_to_frame, json);
    }

    #[test]
    fn test_json_switch_to_window_parameters_with_value() {
        assert_de(
            &SwitchToWindowParameters {
                handle: "foo".into(),
            },
            json!({"handle": "foo"}),
        );
    }

    #[test]
    fn test_json_switch_to_window_parameters_invalid_handle_field() {
        assert!(serde_json::from_value::<SwitchToWindowParameters>(json!({"handle": 3})).is_err());
    }

    #[test]
    fn test_json_switch_to_window_parameters_missing_handle_field() {
        assert!(serde_json::from_value::<SwitchToWindowParameters>(json!({})).is_err());
    }

    #[test]
    fn test_json_switch_to_window_parameters_with_unknown_field() {
        let json = json!({
            "handle": "foo",
            "foo": "bar",
        });
        let switch_to_window = SwitchToWindowParameters {
            handle: "foo".into(),
        };

        assert_de(&switch_to_window, json);
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_element() {
        assert_de(
            &TakeScreenshotParameters {
                element: Some(WebElement("elem".into())),
            },
            json!({"element": {ELEMENT_KEY: "elem"}}),
        );
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_optional_null_field() {
        assert_de(
            &TakeScreenshotParameters { element: None },
            json!({ "element": null }),
        );
    }

    #[test]
    fn test_json_take_screenshot_parameters_without_optional_null_field() {
        assert_de(&TakeScreenshotParameters { element: None }, json!({}));
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_invalid_element_field() {
        assert!(
            serde_json::from_value::<TakeScreenshotParameters>(json!({"element": "foo"})).is_err()
        );
    }

    #[test]
    fn test_json_take_screenshot_parameters_with_unknown_field() {
        let json = json!({
            "element": {ELEMENT_KEY: "elem"},
            "foo": "bar",
        });
        let take_screenshot = TakeScreenshotParameters {
            element: Some(WebElement("elem".into())),
        };

        assert_de(&take_screenshot, json);
    }

    #[test]
    fn test_json_timeout_parameters_with_only_null_script_timeout() {
        let timeouts = TimeoutsParameters {
            implicit: None,
            page_load: None,
            script: Some(None),
        };
        assert_de(&timeouts, json!({ "script": null }));
    }

    #[test]
    fn test_json_timeout_parameters_with_only_null_implicit_timeout() {
        assert!(serde_json::from_value::<TimeoutsParameters>(json!({ "implicit": null })).is_err());
    }

    #[test]
    fn test_json_timeout_parameters_with_only_null_pageload_timeout() {
        assert!(serde_json::from_value::<TimeoutsParameters>(json!({ "pageLoad": null })).is_err());
    }

    #[test]
    fn test_json_timeout_parameters_without_optional_null_field() {
        let timeouts = TimeoutsParameters {
            implicit: None,
            page_load: None,
            script: None,
        };
        assert_de(&timeouts, json!({}));
    }

    #[test]
    fn test_json_timeout_parameters_with_unknown_field() {
        let json = json!({
            "script": 60000,
            "foo": "bar",
        });
        let timeouts = TimeoutsParameters {
            implicit: None,
            page_load: None,
            script: Some(Some(60000)),
        };

        assert_de(&timeouts, json);
    }

    #[test]
    fn test_json_window_rect_parameters_with_values() {
        let json = json!({
            "x": 0,
            "y": 1,
            "width": 2,
            "height": 3,
        });
        let rect = WindowRectParameters {
            x: Some(0i32),
            y: Some(1i32),
            width: Some(2i32),
            height: Some(3i32),
        };

        assert_de(&rect, json);
    }

    #[test]
    fn test_json_window_rect_parameters_with_optional_null_fields() {
        let json = json!({
            "x": null,
            "y": null,
            "width": null,
            "height": null,
        });
        let rect = WindowRectParameters {
            x: None,
            y: None,
            width: None,
            height: None,
        };

        assert_de(&rect, json);
    }

    #[test]
    fn test_json_window_rect_parameters_without_optional_fields() {
        let rect = WindowRectParameters {
            x: None,
            y: None,
            width: None,
            height: None,
        };
        assert_de(&rect, json!({}));
    }

    #[test]
    fn test_json_window_rect_parameters_invalid_values_float() {
        let json = json!({
            "x": 1.1,
            "y": 2.2,
            "width": 3.3,
            "height": 4.4,
        });
        let rect = WindowRectParameters {
            x: Some(1),
            y: Some(2),
            width: Some(3),
            height: Some(4),
        };

        assert_de(&rect, json);
    }

    #[test]
    fn test_json_window_rect_parameters_with_unknown_field() {
        let json = json!({
            "x": 1.1,
            "y": 2.2,
            "foo": "bar",
        });
        let rect = WindowRectParameters {
            x: Some(1),
            y: Some(2),
            width: None,
            height: None,
        };

        assert_de(&rect, json);
    }
}
