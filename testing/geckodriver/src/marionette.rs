/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::browser::{Browser, LocalBrowser, RemoteBrowser};
use crate::build;
use crate::capabilities::{FirefoxCapabilities, FirefoxOptions, ProfileType};
use crate::command::{
    AddonInstallParameters, AddonUninstallParameters, GeckoContextParameters,
    GeckoExtensionCommand, GeckoExtensionRoute,
};
use crate::logging;
use marionette_rs::common::{
    Cookie as MarionetteCookie, Date as MarionetteDate, Frame as MarionetteFrame,
    Timeouts as MarionetteTimeouts, WebElement as MarionetteWebElement, Window,
};
use marionette_rs::marionette::AppStatus;
use marionette_rs::message::{Command, Message, MessageId, Request};
use marionette_rs::webdriver::{
    AuthenticatorParameters as MarionetteAuthenticatorParameters,
    AuthenticatorTransport as MarionetteAuthenticatorTransport,
    Command as MarionetteWebDriverCommand, CredentialParameters as MarionetteCredentialParameters,
    Keys as MarionetteKeys, Locator as MarionetteLocator, NewWindow as MarionetteNewWindow,
    PrintMargins as MarionettePrintMargins, PrintOrientation as MarionettePrintOrientation,
    PrintPage as MarionettePrintPage, PrintPageRange as MarionettePrintPageRange,
    PrintParameters as MarionettePrintParameters, ScreenshotOptions, Script as MarionetteScript,
    Selector as MarionetteSelector, Url as MarionetteUrl,
    UserVerificationParameters as MarionetteUserVerificationParameters,
    WebAuthnProtocol as MarionetteWebAuthnProtocol, WindowRect as MarionetteWindowRect,
};
use mozdevice::AndroidStorageInput;
use serde::de::{self, Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use serde_json::{self, Map, Value};
use std::io::prelude::*;
use std::io::Error as IoError;
use std::io::ErrorKind;
use std::io::Result as IoResult;
use std::net::{Shutdown, TcpListener, TcpStream};
use std::path::PathBuf;
use std::sync::Mutex;
use std::thread;
use std::time;
use url::{Host, Url};
use webdriver::capabilities::BrowserCapabilities;
use webdriver::command::WebDriverCommand::{
    AcceptAlert, AddCookie, CloseWindow, DeleteCookie, DeleteCookies, DeleteSession, DismissAlert,
    ElementClear, ElementClick, ElementSendKeys, ExecuteAsyncScript, ExecuteScript, Extension,
    FindElement, FindElementElement, FindElementElements, FindElements, FindShadowRootElement,
    FindShadowRootElements, FullscreenWindow, Get, GetActiveElement, GetAlertText, GetCSSValue,
    GetComputedLabel, GetComputedRole, GetCookies, GetCurrentUrl, GetElementAttribute,
    GetElementProperty, GetElementRect, GetElementTagName, GetElementText, GetNamedCookie,
    GetPageSource, GetShadowRoot, GetTimeouts, GetTitle, GetWindowHandle, GetWindowHandles,
    GetWindowRect, GoBack, GoForward, IsDisplayed, IsEnabled, IsSelected, MaximizeWindow,
    MinimizeWindow, NewSession, NewWindow, PerformActions, Print, Refresh, ReleaseActions,
    SendAlertText, SetTimeouts, SetWindowRect, Status, SwitchToFrame, SwitchToParentFrame,
    SwitchToWindow, TakeElementScreenshot, TakeScreenshot, WebAuthnAddCredential,
    WebAuthnAddVirtualAuthenticator, WebAuthnGetCredentials, WebAuthnRemoveAllCredentials,
    WebAuthnRemoveCredential, WebAuthnRemoveVirtualAuthenticator, WebAuthnSetUserVerified,
};
use webdriver::command::{
    ActionsParameters, AddCookieParameters, AuthenticatorParameters, AuthenticatorTransport,
    GetNamedCookieParameters, GetParameters, JavascriptCommandParameters, LocatorParameters,
    NewSessionParameters, NewWindowParameters, PrintMargins, PrintOrientation, PrintPage,
    PrintPageRange, PrintParameters, SendKeysParameters, SwitchToFrameParameters,
    SwitchToWindowParameters, TimeoutsParameters, UserVerificationParameters, WebAuthnProtocol,
    WindowRectParameters,
};
use webdriver::command::{WebDriverCommand, WebDriverMessage};
use webdriver::common::{
    Cookie, CredentialParameters, Date, FrameId, LocatorStrategy, ShadowRoot, WebElement,
    ELEMENT_KEY, FRAME_KEY, SHADOW_KEY, WINDOW_KEY,
};
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};
use webdriver::response::{
    CloseWindowResponse, CookieResponse, CookiesResponse, ElementRectResponse, NewSessionResponse,
    NewWindowResponse, TimeoutsResponse, ValueResponse, WebDriverResponse, WindowRectResponse,
};
use webdriver::server::{Session, WebDriverHandler};
use webdriver::{capabilities::CapabilitiesMatching, server::SessionTeardownKind};

#[derive(Debug, PartialEq, Deserialize)]
struct MarionetteHandshake {
    #[serde(rename = "marionetteProtocol")]
    protocol: u16,
    #[serde(rename = "applicationType")]
    application_type: String,
}

#[derive(Default)]
pub(crate) struct MarionetteSettings {
    pub(crate) binary: Option<PathBuf>,
    pub(crate) profile_root: Option<PathBuf>,
    pub(crate) connect_existing: bool,
    pub(crate) host: String,
    pub(crate) port: Option<u16>,
    pub(crate) websocket_port: u16,
    pub(crate) allow_hosts: Vec<Host>,
    pub(crate) allow_origins: Vec<Url>,

    /// Brings up the Browser Toolbox when starting Firefox,
    /// letting you debug internals.
    pub(crate) jsdebugger: bool,

    pub(crate) android_storage: AndroidStorageInput,
}

#[derive(Default)]
pub(crate) struct MarionetteHandler {
    connection: Mutex<Option<MarionetteConnection>>,
    settings: MarionetteSettings,
}

impl MarionetteHandler {
    pub(crate) fn new(settings: MarionetteSettings) -> MarionetteHandler {
        MarionetteHandler {
            connection: Mutex::new(None),
            settings,
        }
    }

    fn create_connection(
        &self,
        session_id: Option<String>,
        new_session_parameters: &NewSessionParameters,
    ) -> WebDriverResult<MarionetteConnection> {
        let mut fx_capabilities = FirefoxCapabilities::new(self.settings.binary.as_ref());
        let (capabilities, options) = {
            let mut capabilities = new_session_parameters
                .match_browser(&mut fx_capabilities)?
                .ok_or_else(|| {
                    WebDriverError::new(
                        ErrorStatus::SessionNotCreated,
                        "Unable to find a matching set of capabilities",
                    )
                })?;

            let options = FirefoxOptions::from_capabilities(
                fx_capabilities.chosen_binary.clone(),
                &self.settings,
                &mut capabilities,
            )?;
            (capabilities, options)
        };

        if let Some(l) = options.log.level {
            logging::set_max_level(l);
        }

        let marionette_host = self.settings.host.to_owned();
        let marionette_port = match self.settings.port {
            Some(port) => port,
            None => {
                // If we're launching Firefox Desktop version 95 or later, and there's no port
                // specified, we can pass 0 as the port and later read it back from
                // the profile.
                let can_use_profile: bool = options.android.is_none()
                    && options.profile != ProfileType::Named
                    && !self.settings.connect_existing
                    && fx_capabilities
                        .browser_version(&capabilities)
                        .map(|opt_v| {
                            opt_v
                                .map(|v| {
                                    fx_capabilities
                                        .compare_browser_version(&v, ">=95")
                                        .unwrap_or(false)
                                })
                                .unwrap_or(false)
                        })
                        .unwrap_or(false);
                if can_use_profile {
                    0
                } else {
                    get_free_port(&marionette_host)?
                }
            }
        };

        let websocket_port = if options.use_websocket {
            Some(self.settings.websocket_port)
        } else {
            None
        };

        let browser = if options.android.is_some() {
            // TODO: support connecting to running Apps.  There's no real obstruction here,
            // just some details about port forwarding to work through.  We can't follow
            // `chromedriver` here since it uses an abstract socket rather than a TCP socket:
            // see bug 1240830 for thoughts on doing that for Marionette.
            if self.settings.connect_existing {
                return Err(WebDriverError::new(
                    ErrorStatus::SessionNotCreated,
                    "Cannot connect to an existing Android App yet",
                ));
            }
            Browser::Remote(RemoteBrowser::new(
                options,
                marionette_port,
                websocket_port,
                self.settings.profile_root.as_deref(),
            )?)
        } else if !self.settings.connect_existing {
            Browser::Local(LocalBrowser::new(
                options,
                marionette_port,
                self.settings.jsdebugger,
                self.settings.profile_root.as_deref(),
            )?)
        } else {
            Browser::Existing(marionette_port)
        };
        let session = MarionetteSession::new(session_id, capabilities);
        MarionetteConnection::new(marionette_host, browser, session)
    }

    fn close_connection(&mut self, wait_for_shutdown: bool) {
        if let Ok(connection) = self.connection.get_mut() {
            if let Some(conn) = connection.take() {
                if let Err(e) = conn.close(wait_for_shutdown) {
                    error!("Failed to close browser connection: {}", e)
                }
            }
        }
    }
}

impl WebDriverHandler<GeckoExtensionRoute> for MarionetteHandler {
    fn handle_command(
        &mut self,
        _: &Option<Session>,
        msg: WebDriverMessage<GeckoExtensionRoute>,
    ) -> WebDriverResult<WebDriverResponse> {
        // First handle the status message which doesn't actually require a marionette
        // connection or message
        if let Status = msg.command {
            let (ready, message) = self
                .connection
                .get_mut()
                .map(|ref connection| {
                    connection
                        .as_ref()
                        .map(|_| (false, "Session already started"))
                        .unwrap_or((true, ""))
                })
                .unwrap_or((false, "geckodriver internal error"));
            let mut value = Map::new();
            value.insert("ready".to_string(), Value::Bool(ready));
            value.insert("message".to_string(), Value::String(message.into()));
            return Ok(WebDriverResponse::Generic(ValueResponse(Value::Object(
                value,
            ))));
        }

        match self.connection.lock() {
            Ok(mut connection) => {
                if connection.is_none() {
                    if let NewSession(ref capabilities) = msg.command {
                        let conn = self.create_connection(msg.session_id.clone(), capabilities)?;
                        *connection = Some(conn);
                    } else {
                        return Err(WebDriverError::new(
                            ErrorStatus::InvalidSessionId,
                            "Tried to run command without establishing a connection",
                        ));
                    }
                }
                let conn = connection.as_mut().expect("Missing connection");
                conn.send_command(&msg).map_err(|mut err| {
                    // Shutdown the browser if no session can
                    // be established due to errors.
                    if let NewSession(_) = msg.command {
                        err.delete_session = true;
                    }
                    err
                })
            }
            Err(_) => Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                "Failed to aquire Marionette connection",
            )),
        }
    }

    fn teardown_session(&mut self, kind: SessionTeardownKind) {
        let wait_for_shutdown = match kind {
            SessionTeardownKind::Deleted => true,
            SessionTeardownKind::NotDeleted => false,
        };
        self.close_connection(wait_for_shutdown);
    }
}

impl Drop for MarionetteHandler {
    fn drop(&mut self) {
        self.close_connection(false);
    }
}

struct MarionetteSession {
    session_id: String,
    capabilities: Map<String, Value>,
    command_id: MessageId,
}

impl MarionetteSession {
    fn new(session_id: Option<String>, capabilities: Map<String, Value>) -> MarionetteSession {
        let initital_id = session_id.unwrap_or_default();
        MarionetteSession {
            session_id: initital_id,
            capabilities,
            command_id: 0,
        }
    }

    fn update(
        &mut self,
        msg: &WebDriverMessage<GeckoExtensionRoute>,
        resp: &MarionetteResponse,
    ) -> WebDriverResult<()> {
        if let NewSession(_) = msg.command {
            let session_id = try_opt!(
                try_opt!(
                    resp.result.get("sessionId"),
                    ErrorStatus::SessionNotCreated,
                    "Unable to get session id"
                )
                .as_str(),
                ErrorStatus::SessionNotCreated,
                "Unable to convert session id to string"
            );
            self.session_id = session_id.to_string();
        };
        Ok(())
    }

    /// Converts a Marionette JSON response into a `WebElement`.
    ///
    /// Note that it currently coerces all chrome elements, web frames, and web
    /// windows also into web elements.  This will change at a later point.
    fn to_web_element(&self, json_data: &Value) -> WebDriverResult<WebElement> {
        let data = try_opt!(
            json_data.as_object(),
            ErrorStatus::UnknownError,
            "Failed to convert data to an object"
        );

        let element = data.get(ELEMENT_KEY);
        let frame = data.get(FRAME_KEY);
        let window = data.get(WINDOW_KEY);

        let value = try_opt!(
            element.or(frame).or(window),
            ErrorStatus::UnknownError,
            "Failed to extract web element from Marionette response"
        );
        let id = try_opt!(
            value.as_str(),
            ErrorStatus::UnknownError,
            "Failed to convert web element reference value to string"
        )
        .to_string();
        Ok(WebElement(id))
    }

    /// Converts a Marionette JSON response into a `ShadowRoot`.
    fn to_shadow_root(&self, json_data: &Value) -> WebDriverResult<ShadowRoot> {
        let data = try_opt!(
            json_data.as_object(),
            ErrorStatus::UnknownError,
            "Failed to convert data to an object"
        );

        let shadow_root = data.get(SHADOW_KEY);

        let value = try_opt!(
            shadow_root,
            ErrorStatus::UnknownError,
            "Failed to extract shadow root from Marionette response"
        );
        let id = try_opt!(
            value.as_str(),
            ErrorStatus::UnknownError,
            "Failed to convert shadow root reference value to string"
        )
        .to_string();
        Ok(ShadowRoot(id))
    }

    fn next_command_id(&mut self) -> MessageId {
        self.command_id += 1;
        self.command_id
    }

    fn response(
        &mut self,
        msg: &WebDriverMessage<GeckoExtensionRoute>,
        resp: MarionetteResponse,
    ) -> WebDriverResult<WebDriverResponse> {
        use self::GeckoExtensionCommand::*;

        if resp.id != self.command_id {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                format!(
                    "Marionette responses arrived out of sequence, expected {}, got {}",
                    self.command_id, resp.id
                ),
            ));
        }

        if let Some(error) = resp.error {
            return Err(error.into());
        }

        self.update(msg, &resp)?;

        Ok(match msg.command {
            // Everything that doesn't have a response value
            Get(_)
            | GoBack
            | GoForward
            | Refresh
            | SetTimeouts(_)
            | SwitchToWindow(_)
            | SwitchToFrame(_)
            | SwitchToParentFrame
            | AddCookie(_)
            | DeleteCookies
            | DeleteCookie(_)
            | DismissAlert
            | AcceptAlert
            | SendAlertText(_)
            | ElementClick(_)
            | ElementClear(_)
            | ElementSendKeys(_, _)
            | PerformActions(_)
            | ReleaseActions => WebDriverResponse::Void,
            // Things that simply return the contents of the marionette "value" property
            GetCurrentUrl
            | GetTitle
            | GetPageSource
            | GetWindowHandle
            | IsDisplayed(_)
            | IsSelected(_)
            | GetElementAttribute(_, _)
            | GetElementProperty(_, _)
            | GetCSSValue(_, _)
            | GetElementText(_)
            | GetElementTagName(_)
            | GetComputedLabel(_)
            | GetComputedRole(_)
            | IsEnabled(_)
            | ExecuteScript(_)
            | ExecuteAsyncScript(_)
            | GetAlertText
            | TakeScreenshot
            | Print(_)
            | TakeElementScreenshot(_)
            | WebAuthnAddVirtualAuthenticator(_)
            | WebAuthnRemoveVirtualAuthenticator
            | WebAuthnAddCredential(_)
            | WebAuthnGetCredentials
            | WebAuthnRemoveCredential
            | WebAuthnRemoveAllCredentials
            | WebAuthnSetUserVerified(_) => {
                WebDriverResponse::Generic(resp.into_value_response(true)?)
            }
            GetTimeouts => {
                let script = match try_opt!(
                    resp.result.get("script"),
                    ErrorStatus::UnknownError,
                    "Missing field: script"
                ) {
                    Value::Null => None,
                    n => try_opt!(
                        Some(n.as_u64()),
                        ErrorStatus::UnknownError,
                        "Failed to interpret script timeout duration as u64"
                    ),
                };
                let page_load = try_opt!(
                    try_opt!(
                        resp.result.get("pageLoad"),
                        ErrorStatus::UnknownError,
                        "Missing field: pageLoad"
                    )
                    .as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret page load duration as u64"
                );
                let implicit = try_opt!(
                    try_opt!(
                        resp.result.get("implicit"),
                        ErrorStatus::UnknownError,
                        "Missing field: implicit"
                    )
                    .as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret implicit search duration as u64"
                );

                WebDriverResponse::Timeouts(TimeoutsResponse {
                    script,
                    page_load,
                    implicit,
                })
            }
            Status => panic!("Got status command that should already have been handled"),
            GetWindowHandles => WebDriverResponse::Generic(resp.into_value_response(false)?),
            NewWindow(_) => {
                let handle: String = try_opt!(
                    try_opt!(
                        resp.result.get("handle"),
                        ErrorStatus::UnknownError,
                        "Failed to find handle field"
                    )
                    .as_str(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret handle as string"
                )
                .into();
                let typ: String = try_opt!(
                    try_opt!(
                        resp.result.get("type"),
                        ErrorStatus::UnknownError,
                        "Failed to find type field"
                    )
                    .as_str(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret type as string"
                )
                .into();

                WebDriverResponse::NewWindow(NewWindowResponse { handle, typ })
            }
            CloseWindow => {
                let data = try_opt!(
                    resp.result.as_array(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret value as array"
                );
                let handles = data
                    .iter()
                    .map(|x| {
                        Ok(try_opt!(
                            x.as_str(),
                            ErrorStatus::UnknownError,
                            "Failed to interpret window handle as string"
                        )
                        .to_owned())
                    })
                    .collect::<Result<Vec<_>, _>>()?;
                WebDriverResponse::CloseWindow(CloseWindowResponse(handles))
            }
            GetElementRect(_) => {
                let x = try_opt!(
                    try_opt!(
                        resp.result.get("x"),
                        ErrorStatus::UnknownError,
                        "Failed to find x field"
                    )
                    .as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as float"
                );

                let y = try_opt!(
                    try_opt!(
                        resp.result.get("y"),
                        ErrorStatus::UnknownError,
                        "Failed to find y field"
                    )
                    .as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret y as float"
                );

                let width = try_opt!(
                    try_opt!(
                        resp.result.get("width"),
                        ErrorStatus::UnknownError,
                        "Failed to find width field"
                    )
                    .as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as float"
                );

                let height = try_opt!(
                    try_opt!(
                        resp.result.get("height"),
                        ErrorStatus::UnknownError,
                        "Failed to find height field"
                    )
                    .as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as float"
                );

                let rect = ElementRectResponse {
                    x,
                    y,
                    width,
                    height,
                };
                WebDriverResponse::ElementRect(rect)
            }
            FullscreenWindow | MinimizeWindow | MaximizeWindow | GetWindowRect
            | SetWindowRect(_) => {
                let width = try_opt!(
                    try_opt!(
                        resp.result.get("width"),
                        ErrorStatus::UnknownError,
                        "Failed to find width field"
                    )
                    .as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as positive integer"
                );

                let height = try_opt!(
                    try_opt!(
                        resp.result.get("height"),
                        ErrorStatus::UnknownError,
                        "Failed to find heigenht field"
                    )
                    .as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret height as positive integer"
                );

                let x = try_opt!(
                    try_opt!(
                        resp.result.get("x"),
                        ErrorStatus::UnknownError,
                        "Failed to find x field"
                    )
                    .as_i64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as integer"
                );

                let y = try_opt!(
                    try_opt!(
                        resp.result.get("y"),
                        ErrorStatus::UnknownError,
                        "Failed to find y field"
                    )
                    .as_i64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret y as integer"
                );

                let rect = WindowRectResponse {
                    x: x as i32,
                    y: y as i32,
                    width: width as i32,
                    height: height as i32,
                };
                WebDriverResponse::WindowRect(rect)
            }
            GetCookies => {
                let cookies: Vec<Cookie> = serde_json::from_value(resp.result)?;
                WebDriverResponse::Cookies(CookiesResponse(cookies))
            }
            GetNamedCookie(ref name) => {
                let mut cookies: Vec<Cookie> = serde_json::from_value(resp.result)?;
                cookies.retain(|x| x.name == *name);
                let cookie = try_opt!(
                    cookies.pop(),
                    ErrorStatus::NoSuchCookie,
                    format!("No cookie with name {}", name)
                );
                WebDriverResponse::Cookie(CookieResponse(cookie))
            }
            FindElement(_) | FindElementElement(_, _) | FindShadowRootElement(_, _) => {
                let element = self.to_web_element(try_opt!(
                    resp.result.get("value"),
                    ErrorStatus::UnknownError,
                    "Failed to find value field"
                ))?;
                WebDriverResponse::Generic(ValueResponse(serde_json::to_value(element)?))
            }
            FindElements(_) | FindElementElements(_, _) | FindShadowRootElements(_, _) => {
                let element_vec = try_opt!(
                    resp.result.as_array(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret value as array"
                );
                let elements = element_vec
                    .iter()
                    .map(|x| self.to_web_element(x))
                    .collect::<Result<Vec<_>, _>>()?;

                // TODO(Henrik): How to remove unwrap?
                WebDriverResponse::Generic(ValueResponse(Value::Array(
                    elements
                        .iter()
                        .map(|x| serde_json::to_value(x).unwrap())
                        .collect(),
                )))
            }
            GetShadowRoot(_) => {
                let shadow_root = self.to_shadow_root(try_opt!(
                    resp.result.get("value"),
                    ErrorStatus::UnknownError,
                    "Failed to find value field"
                ))?;
                WebDriverResponse::Generic(ValueResponse(serde_json::to_value(shadow_root)?))
            }
            GetActiveElement => {
                let element = self.to_web_element(try_opt!(
                    resp.result.get("value"),
                    ErrorStatus::UnknownError,
                    "Failed to find value field"
                ))?;
                WebDriverResponse::Generic(ValueResponse(serde_json::to_value(element)?))
            }
            NewSession(_) => {
                let session_id = try_opt!(
                    try_opt!(
                        resp.result.get("sessionId"),
                        ErrorStatus::InvalidSessionId,
                        "Failed to find sessionId field"
                    )
                    .as_str(),
                    ErrorStatus::InvalidSessionId,
                    "sessionId is not a string"
                );

                let mut capabilities = try_opt!(
                    try_opt!(
                        resp.result.get("capabilities"),
                        ErrorStatus::UnknownError,
                        "Failed to find capabilities field"
                    )
                    .as_object(),
                    ErrorStatus::UnknownError,
                    "capabilities field is not an object"
                )
                .clone();

                capabilities.insert("moz:geckodriverVersion".into(), build::build_info().into());

                WebDriverResponse::NewSession(NewSessionResponse::new(
                    session_id.to_string(),
                    Value::Object(capabilities),
                ))
            }
            DeleteSession => WebDriverResponse::DeleteSession,
            Extension(ref extension) => match extension {
                GetContext => WebDriverResponse::Generic(resp.into_value_response(true)?),
                SetContext(_) => WebDriverResponse::Void,
                InstallAddon(_) => WebDriverResponse::Generic(resp.into_value_response(true)?),
                UninstallAddon(_) => WebDriverResponse::Void,
                TakeFullScreenshot => WebDriverResponse::Generic(resp.into_value_response(true)?),
            },
        })
    }
}

fn try_convert_to_marionette_message(
    msg: &WebDriverMessage<GeckoExtensionRoute>,
    browser: &Browser,
) -> WebDriverResult<Option<Command>> {
    use self::GeckoExtensionCommand::*;
    use self::WebDriverCommand::*;

    Ok(match msg.command {
        AcceptAlert => Some(Command::WebDriver(MarionetteWebDriverCommand::AcceptAlert)),
        AddCookie(ref x) => Some(Command::WebDriver(MarionetteWebDriverCommand::AddCookie(
            x.to_marionette()?,
        ))),
        CloseWindow => Some(Command::WebDriver(MarionetteWebDriverCommand::CloseWindow)),
        DeleteCookie(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::DeleteCookie(x.clone()),
        )),
        DeleteCookies => Some(Command::WebDriver(
            MarionetteWebDriverCommand::DeleteCookies,
        )),
        DeleteSession => match browser {
            Browser::Local(_) | Browser::Remote(_) => Some(Command::Marionette(
                marionette_rs::marionette::Command::DeleteSession {
                    flags: vec![AppStatus::eForceQuit],
                },
            )),
            Browser::Existing(_) => Some(Command::WebDriver(
                MarionetteWebDriverCommand::DeleteSession,
            )),
        },
        DismissAlert => Some(Command::WebDriver(MarionetteWebDriverCommand::DismissAlert)),
        ElementClear(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::ElementClear {
                id: e.clone().to_string(),
            },
        )),
        ElementClick(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::ElementClick {
                id: e.clone().to_string(),
            },
        )),
        ElementSendKeys(ref e, ref x) => {
            let keys = x.to_marionette()?;
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::ElementSendKeys {
                    id: e.clone().to_string(),
                    text: keys.text.clone(),
                    value: keys.value,
                },
            ))
        }
        ExecuteAsyncScript(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::ExecuteAsyncScript(x.to_marionette()?),
        )),
        ExecuteScript(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::ExecuteScript(x.to_marionette()?),
        )),
        FindElement(ref x) => Some(Command::WebDriver(MarionetteWebDriverCommand::FindElement(
            x.to_marionette()?,
        ))),
        FindElements(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::FindElements(x.to_marionette()?),
        )),
        FindElementElement(ref e, ref x) => {
            let locator = x.to_marionette()?;
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::FindElementElement {
                    element: e.clone().to_string(),
                    using: locator.using.clone(),
                    value: locator.value,
                },
            ))
        }
        FindElementElements(ref e, ref x) => {
            let locator = x.to_marionette()?;
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::FindElementElements {
                    element: e.clone().to_string(),
                    using: locator.using.clone(),
                    value: locator.value,
                },
            ))
        }
        FindShadowRootElement(ref s, ref x) => {
            let locator = x.to_marionette()?;
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::FindShadowRootElement {
                    shadow_root: s.clone().to_string(),
                    using: locator.using.clone(),
                    value: locator.value,
                },
            ))
        }
        FindShadowRootElements(ref s, ref x) => {
            let locator = x.to_marionette()?;
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::FindShadowRootElements {
                    shadow_root: s.clone().to_string(),
                    using: locator.using.clone(),
                    value: locator.value,
                },
            ))
        }
        FullscreenWindow => Some(Command::WebDriver(
            MarionetteWebDriverCommand::FullscreenWindow,
        )),
        Get(ref x) => Some(Command::WebDriver(MarionetteWebDriverCommand::Get(
            x.to_marionette()?,
        ))),
        GetActiveElement => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetActiveElement,
        )),
        GetAlertText => Some(Command::WebDriver(MarionetteWebDriverCommand::GetAlertText)),
        GetComputedLabel(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetComputedLabel {
                id: e.clone().to_string(),
            },
        )),
        GetComputedRole(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetComputedRole {
                id: e.clone().to_string(),
            },
        )),
        GetCookies | GetNamedCookie(_) => {
            Some(Command::WebDriver(MarionetteWebDriverCommand::GetCookies))
        }
        GetCSSValue(ref e, ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetCSSValue {
                id: e.clone().to_string(),
                property: x.clone(),
            },
        )),
        GetCurrentUrl => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetCurrentUrl,
        )),
        GetElementAttribute(ref e, ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetElementAttribute {
                id: e.clone().to_string(),
                name: x.clone(),
            },
        )),
        GetElementProperty(ref e, ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetElementProperty {
                id: e.clone().to_string(),
                name: x.clone(),
            },
        )),
        GetElementRect(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetElementRect {
                id: e.clone().to_string(),
            },
        )),
        GetElementTagName(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetElementTagName {
                id: e.clone().to_string(),
            },
        )),
        GetElementText(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetElementText {
                id: e.clone().to_string(),
            },
        )),
        GetPageSource => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetPageSource,
        )),
        GetShadowRoot(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetShadowRoot {
                id: e.clone().to_string(),
            },
        )),
        GetTitle => Some(Command::WebDriver(MarionetteWebDriverCommand::GetTitle)),
        GetWindowHandle => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetWindowHandle,
        )),
        GetWindowHandles => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetWindowHandles,
        )),
        GetWindowRect => Some(Command::WebDriver(
            MarionetteWebDriverCommand::GetWindowRect,
        )),
        GetTimeouts => Some(Command::WebDriver(MarionetteWebDriverCommand::GetTimeouts)),
        GoBack => Some(Command::WebDriver(MarionetteWebDriverCommand::GoBack)),
        GoForward => Some(Command::WebDriver(MarionetteWebDriverCommand::GoForward)),
        IsDisplayed(ref e) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::IsDisplayed {
                id: e.clone().to_string(),
            },
        )),
        IsEnabled(ref e) => Some(Command::WebDriver(MarionetteWebDriverCommand::IsEnabled {
            id: e.clone().to_string(),
        })),
        IsSelected(ref e) => Some(Command::WebDriver(MarionetteWebDriverCommand::IsSelected {
            id: e.clone().to_string(),
        })),
        MaximizeWindow => Some(Command::WebDriver(
            MarionetteWebDriverCommand::MaximizeWindow,
        )),
        MinimizeWindow => Some(Command::WebDriver(
            MarionetteWebDriverCommand::MinimizeWindow,
        )),
        NewWindow(ref x) => Some(Command::WebDriver(MarionetteWebDriverCommand::NewWindow(
            x.to_marionette()?,
        ))),
        Print(ref x) => Some(Command::WebDriver(MarionetteWebDriverCommand::Print(
            x.to_marionette()?,
        ))),
        WebAuthnAddVirtualAuthenticator(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::WebAuthnAddVirtualAuthenticator(x.to_marionette()?),
        )),
        WebAuthnRemoveVirtualAuthenticator => Some(Command::WebDriver(
            MarionetteWebDriverCommand::WebAuthnRemoveVirtualAuthenticator,
        )),
        WebAuthnAddCredential(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::WebAuthnAddCredential(x.to_marionette()?),
        )),
        WebAuthnGetCredentials => Some(Command::WebDriver(
            MarionetteWebDriverCommand::WebAuthnGetCredentials,
        )),
        WebAuthnRemoveCredential => Some(Command::WebDriver(
            MarionetteWebDriverCommand::WebAuthnRemoveCredential,
        )),
        WebAuthnRemoveAllCredentials => Some(Command::WebDriver(
            MarionetteWebDriverCommand::WebAuthnRemoveAllCredentials,
        )),
        WebAuthnSetUserVerified(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::WebAuthnSetUserVerified(x.to_marionette()?),
        )),
        Refresh => Some(Command::WebDriver(MarionetteWebDriverCommand::Refresh)),
        ReleaseActions => Some(Command::WebDriver(
            MarionetteWebDriverCommand::ReleaseActions,
        )),
        SendAlertText(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::SendAlertText(x.to_marionette()?),
        )),
        SetTimeouts(ref x) => Some(Command::WebDriver(MarionetteWebDriverCommand::SetTimeouts(
            x.to_marionette()?,
        ))),
        SetWindowRect(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::SetWindowRect(x.to_marionette()?),
        )),
        SwitchToFrame(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::SwitchToFrame(x.to_marionette()?),
        )),
        SwitchToParentFrame => Some(Command::WebDriver(
            MarionetteWebDriverCommand::SwitchToParentFrame,
        )),
        SwitchToWindow(ref x) => Some(Command::WebDriver(
            MarionetteWebDriverCommand::SwitchToWindow(x.to_marionette()?),
        )),
        TakeElementScreenshot(ref e) => {
            let screenshot = ScreenshotOptions {
                id: Some(e.clone().to_string()),
                highlights: vec![],
                full: false,
            };
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::TakeElementScreenshot(screenshot),
            ))
        }
        TakeScreenshot => {
            let screenshot = ScreenshotOptions {
                id: None,
                highlights: vec![],
                full: false,
            };
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::TakeScreenshot(screenshot),
            ))
        }
        Extension(TakeFullScreenshot) => {
            let screenshot = ScreenshotOptions {
                id: None,
                highlights: vec![],
                full: true,
            };
            Some(Command::WebDriver(
                MarionetteWebDriverCommand::TakeFullScreenshot(screenshot),
            ))
        }
        _ => None,
    })
}

#[derive(Debug, PartialEq)]
struct MarionetteCommand {
    id: MessageId,
    name: String,
    params: Map<String, Value>,
}

impl Serialize for MarionetteCommand {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let data = (&0, &self.id, &self.name, &self.params);
        data.serialize(serializer)
    }
}

impl MarionetteCommand {
    fn new(id: MessageId, name: String, params: Map<String, Value>) -> MarionetteCommand {
        MarionetteCommand { id, name, params }
    }

    fn encode_msg<T>(msg: T) -> WebDriverResult<String>
    where
        T: serde::Serialize,
    {
        let data = serde_json::to_string(&msg)?;

        Ok(format!("{}:{}", data.len(), data))
    }

    fn from_webdriver_message(
        id: MessageId,
        capabilities: &Map<String, Value>,
        browser: &Browser,
        msg: &WebDriverMessage<GeckoExtensionRoute>,
    ) -> WebDriverResult<String> {
        use self::GeckoExtensionCommand::*;

        if let Some(cmd) = try_convert_to_marionette_message(msg, browser)? {
            let req = Message::Incoming(Request(id, cmd));
            MarionetteCommand::encode_msg(req)
        } else {
            let (opt_name, opt_parameters) = match msg.command {
                Status => panic!("Got status command that should already have been handled"),
                NewSession(_) => {
                    let mut data = Map::new();
                    for (k, v) in capabilities.iter() {
                        data.insert(k.to_string(), serde_json::to_value(v)?);
                    }

                    (Some("WebDriver:NewSession"), Some(Ok(data)))
                }
                PerformActions(ref x) => {
                    (Some("WebDriver:PerformActions"), Some(x.to_marionette()))
                }
                Extension(ref extension) => match extension {
                    GetContext => (Some("Marionette:GetContext"), None),
                    InstallAddon(x) => (Some("Addon:Install"), Some(x.to_marionette())),
                    SetContext(x) => (Some("Marionette:SetContext"), Some(x.to_marionette())),
                    UninstallAddon(x) => (Some("Addon:Uninstall"), Some(x.to_marionette())),
                    _ => (None, None),
                },
                _ => (None, None),
            };

            let name = try_opt!(
                opt_name,
                ErrorStatus::UnsupportedOperation,
                "Operation not supported"
            );
            let parameters = opt_parameters.unwrap_or_else(|| Ok(Map::new()))?;

            let req = MarionetteCommand::new(id, name.into(), parameters);
            MarionetteCommand::encode_msg(req)
        }
    }
}

#[derive(Debug, PartialEq)]
struct MarionetteResponse {
    id: MessageId,
    error: Option<MarionetteError>,
    result: Value,
}

impl<'de> Deserialize<'de> for MarionetteResponse {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Deserialize)]
        struct ResponseWrapper {
            msg_type: u64,
            id: MessageId,
            error: Option<MarionetteError>,
            result: Value,
        }

        let wrapper: ResponseWrapper = Deserialize::deserialize(deserializer)?;

        if wrapper.msg_type != 1 {
            return Err(de::Error::custom(
                "Expected '1' in first element of response",
            ));
        };

        Ok(MarionetteResponse {
            id: wrapper.id,
            error: wrapper.error,
            result: wrapper.result,
        })
    }
}

impl MarionetteResponse {
    fn into_value_response(self, value_required: bool) -> WebDriverResult<ValueResponse> {
        let value: &Value = if value_required {
            try_opt!(
                self.result.get("value"),
                ErrorStatus::UnknownError,
                "Failed to find value field"
            )
        } else {
            &self.result
        };

        Ok(ValueResponse(value.clone()))
    }
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
struct MarionetteError {
    #[serde(rename = "error")]
    code: String,
    message: String,
    stacktrace: Option<String>,
}

impl From<MarionetteError> for WebDriverError {
    fn from(error: MarionetteError) -> WebDriverError {
        let status = ErrorStatus::from(error.code);
        let message = error.message;

        if let Some(stack) = error.stacktrace {
            WebDriverError::new_with_stack(status, message, stack)
        } else {
            WebDriverError::new(status, message)
        }
    }
}

fn get_free_port(host: &str) -> IoResult<u16> {
    TcpListener::bind((host, 0))
        .and_then(|stream| stream.local_addr())
        .map(|x| x.port())
}

struct MarionetteConnection {
    browser: Browser,
    session: MarionetteSession,
    stream: TcpStream,
}

impl MarionetteConnection {
    fn new(
        host: String,
        mut browser: Browser,
        session: MarionetteSession,
    ) -> WebDriverResult<MarionetteConnection> {
        let stream = match MarionetteConnection::connect(&host, &mut browser) {
            Ok(stream) => stream,
            Err(e) => {
                if let Err(e) = browser.close(true) {
                    error!("Failed to stop browser: {:?}", e);
                }
                return Err(e);
            }
        };
        Ok(MarionetteConnection {
            browser,
            session,
            stream,
        })
    }

    fn connect(host: &str, browser: &mut Browser) -> WebDriverResult<TcpStream> {
        let timeout = time::Duration::from_secs(60);
        let poll_interval = time::Duration::from_millis(100);
        let now = time::Instant::now();

        debug!(
            "Waiting {}s to connect to browser on {}",
            timeout.as_secs(),
            host,
        );

        loop {
            // immediately abort connection attempts if process disappears
            if let Browser::Local(browser) = browser {
                if let Some(status) = browser.check_status() {
                    return Err(WebDriverError::new(
                        ErrorStatus::UnknownError,
                        format!("Process unexpectedly closed with status {}", status),
                    ));
                }
            }

            let last_err;

            if let Some(port) = browser.marionette_port()? {
                match MarionetteConnection::try_connect(host, port) {
                    Ok(stream) => {
                        debug!("Connection to Marionette established on {}:{}.", host, port);
                        browser.update_marionette_port(port);
                        return Ok(stream);
                    }
                    Err(e) => {
                        let err_str = e.to_string();
                        last_err = Some(err_str);
                    }
                }
            } else {
                last_err = Some("Failed to read marionette port".into());
            }
            if now.elapsed() < timeout {
                trace!("Retrying in {:?}", poll_interval);
                thread::sleep(poll_interval);
            } else {
                return Err(WebDriverError::new(
                    ErrorStatus::Timeout,
                    last_err.unwrap_or_else(|| "Unknown error".into()),
                ));
            }
        }
    }

    fn try_connect(host: &str, port: u16) -> WebDriverResult<TcpStream> {
        let mut stream = TcpStream::connect((host, port))?;
        MarionetteConnection::handshake(&mut stream)?;
        Ok(stream)
    }

    fn handshake(stream: &mut TcpStream) -> WebDriverResult<MarionetteHandshake> {
        let resp = (match stream.read_timeout() {
            Ok(timeout) => {
                // If platform supports changing the read timeout of the stream,
                // use a short one only for the handshake with Marionette. Don't
                // make it shorter as 1000ms to not fail on slow connections.
                stream
                    .set_read_timeout(Some(time::Duration::from_millis(1000)))
                    .ok();
                let data = MarionetteConnection::read_resp(stream);
                stream.set_read_timeout(timeout).ok();

                data
            }
            _ => MarionetteConnection::read_resp(stream),
        })
        .map_err(|e| {
            WebDriverError::new(
                ErrorStatus::UnknownError,
                format!("Socket timeout reading Marionette handshake data: {}", e),
            )
        })?;

        let data = serde_json::from_str::<MarionetteHandshake>(&resp)?;

        if data.application_type != "gecko" {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                format!("Unrecognized application type {}", data.application_type),
            ));
        }

        if data.protocol != 3 {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                format!(
                    "Unsupported Marionette protocol version {}, required 3",
                    data.protocol
                ),
            ));
        }

        Ok(data)
    }

    fn close(self, wait_for_shutdown: bool) -> WebDriverResult<()> {
        self.stream.shutdown(Shutdown::Both)?;
        self.browser.close(wait_for_shutdown)?;
        Ok(())
    }

    fn send_command(
        &mut self,
        msg: &WebDriverMessage<GeckoExtensionRoute>,
    ) -> WebDriverResult<WebDriverResponse> {
        let id = self.session.next_command_id();
        let enc_cmd = MarionetteCommand::from_webdriver_message(
            id,
            &self.session.capabilities,
            &self.browser,
            msg,
        )?;
        let resp_data = self.send(enc_cmd)?;
        let data: MarionetteResponse = serde_json::from_str(&resp_data)?;

        self.session.response(msg, data)
    }

    fn send(&mut self, data: String) -> WebDriverResult<String> {
        if self.stream.write(data.as_bytes()).is_err() {
            let mut err = WebDriverError::new(
                ErrorStatus::UnknownError,
                "Failed to write request to stream",
            );
            err.delete_session = true;
            return Err(err);
        }

        match MarionetteConnection::read_resp(&mut self.stream) {
            Ok(resp) => Ok(resp),
            Err(_) => {
                let mut err = WebDriverError::new(
                    ErrorStatus::UnknownError,
                    "Failed to decode response from marionette",
                );
                err.delete_session = true;
                Err(err)
            }
        }
    }

    fn read_resp(stream: &mut TcpStream) -> IoResult<String> {
        let mut bytes = 0usize;

        loop {
            let buf = &mut [0u8];
            let num_read = stream.read(buf)?;
            let byte = match num_read {
                0 => {
                    return Err(IoError::new(
                        ErrorKind::Other,
                        "EOF reading marionette message",
                    ))
                }
                1 => buf[0],
                _ => panic!("Expected one byte got more"),
            } as char;
            match byte {
                '0'..='9' => {
                    bytes *= 10;
                    bytes += byte as usize - '0' as usize;
                }
                ':' => break,
                _ => {}
            }
        }

        let buf = &mut [0u8; 8192];
        let mut payload = Vec::with_capacity(bytes);
        let mut total_read = 0;
        while total_read < bytes {
            let num_read = stream.read(buf)?;
            if num_read == 0 {
                return Err(IoError::new(
                    ErrorKind::Other,
                    "EOF reading marionette message",
                ));
            }
            total_read += num_read;
            for x in &buf[..num_read] {
                payload.push(*x);
            }
        }

        // TODO(jgraham): Need to handle the error here
        Ok(String::from_utf8(payload).unwrap())
    }
}

trait ToMarionette<T> {
    fn to_marionette(&self) -> WebDriverResult<T>;
}

impl ToMarionette<Map<String, Value>> for AddonInstallParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert("path".to_string(), serde_json::to_value(&self.path)?);
        if self.temporary.is_some() {
            data.insert(
                "temporary".to_string(),
                serde_json::to_value(self.temporary)?,
            );
        }
        Ok(data)
    }
}

impl ToMarionette<Map<String, Value>> for AddonUninstallParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert("id".to_string(), Value::String(self.id.clone()));
        Ok(data)
    }
}

impl ToMarionette<Map<String, Value>> for GeckoContextParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert(
            "value".to_owned(),
            serde_json::to_value(self.context.clone())?,
        );
        Ok(data)
    }
}

impl ToMarionette<MarionettePrintParameters> for PrintParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionettePrintParameters> {
        Ok(MarionettePrintParameters {
            orientation: self.orientation.to_marionette()?,
            scale: self.scale,
            background: self.background,
            page: self.page.to_marionette()?,
            margin: self.margin.to_marionette()?,
            page_ranges: self
                .page_ranges
                .iter()
                .map(|x| x.to_marionette())
                .collect::<WebDriverResult<Vec<_>>>()?,
            shrink_to_fit: self.shrink_to_fit,
        })
    }
}

impl ToMarionette<MarionettePrintOrientation> for PrintOrientation {
    fn to_marionette(&self) -> WebDriverResult<MarionettePrintOrientation> {
        Ok(match self {
            PrintOrientation::Landscape => MarionettePrintOrientation::Landscape,
            PrintOrientation::Portrait => MarionettePrintOrientation::Portrait,
        })
    }
}

impl ToMarionette<MarionettePrintPage> for PrintPage {
    fn to_marionette(&self) -> WebDriverResult<MarionettePrintPage> {
        Ok(MarionettePrintPage {
            width: self.width,
            height: self.height,
        })
    }
}

impl ToMarionette<MarionettePrintPageRange> for PrintPageRange {
    fn to_marionette(&self) -> WebDriverResult<MarionettePrintPageRange> {
        Ok(match self {
            PrintPageRange::Integer(num) => MarionettePrintPageRange::Integer(*num),
            PrintPageRange::Range(range) => MarionettePrintPageRange::Range(range.clone()),
        })
    }
}

impl ToMarionette<MarionettePrintMargins> for PrintMargins {
    fn to_marionette(&self) -> WebDriverResult<MarionettePrintMargins> {
        Ok(MarionettePrintMargins {
            top: self.top,
            bottom: self.bottom,
            left: self.left,
            right: self.right,
        })
    }
}

impl ToMarionette<MarionetteAuthenticatorParameters> for AuthenticatorParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteAuthenticatorParameters> {
        Ok(MarionetteAuthenticatorParameters {
            protocol: self.protocol.to_marionette()?,
            transport: self.transport.to_marionette()?,
            has_resident_key: self.has_resident_key,
            has_user_verification: self.has_user_verification,
            is_user_consenting: self.is_user_consenting,
            is_user_verified: self.is_user_verified,
        })
    }
}

impl ToMarionette<MarionetteAuthenticatorTransport> for AuthenticatorTransport {
    fn to_marionette(&self) -> WebDriverResult<MarionetteAuthenticatorTransport> {
        Ok(match self {
            AuthenticatorTransport::Usb => MarionetteAuthenticatorTransport::Usb,
            AuthenticatorTransport::Nfc => MarionetteAuthenticatorTransport::Nfc,
            AuthenticatorTransport::Ble => MarionetteAuthenticatorTransport::Ble,
            AuthenticatorTransport::SmartCard => MarionetteAuthenticatorTransport::SmartCard,
            AuthenticatorTransport::Hybrid => MarionetteAuthenticatorTransport::Hybrid,
            AuthenticatorTransport::Internal => MarionetteAuthenticatorTransport::Internal,
        })
    }
}

impl ToMarionette<MarionetteCredentialParameters> for CredentialParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteCredentialParameters> {
        Ok(MarionetteCredentialParameters {
            credential_id: self.credential_id.clone(),
            is_resident_credential: self.is_resident_credential,
            rp_id: self.rp_id.clone(),
            private_key: self.private_key.clone(),
            user_handle: self.user_handle.clone(),
            sign_count: self.sign_count,
        })
    }
}

impl ToMarionette<MarionetteUserVerificationParameters> for UserVerificationParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteUserVerificationParameters> {
        Ok(MarionetteUserVerificationParameters {
            is_user_verified: self.is_user_verified,
        })
    }
}

impl ToMarionette<MarionetteWebAuthnProtocol> for WebAuthnProtocol {
    fn to_marionette(&self) -> WebDriverResult<MarionetteWebAuthnProtocol> {
        Ok(match self {
            WebAuthnProtocol::Ctap1U2f => MarionetteWebAuthnProtocol::Ctap1U2f,
            WebAuthnProtocol::Ctap2 => MarionetteWebAuthnProtocol::Ctap2,
            WebAuthnProtocol::Ctap2_1 => MarionetteWebAuthnProtocol::Ctap2_1,
        })
    }
}

impl ToMarionette<Map<String, Value>> for ActionsParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        )
        .clone())
    }
}

impl ToMarionette<MarionetteCookie> for AddCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteCookie> {
        Ok(MarionetteCookie {
            name: self.name.clone(),
            value: self.value.clone(),
            path: self.path.clone(),
            domain: self.domain.clone(),
            secure: self.secure,
            http_only: self.httpOnly,
            expiry: match &self.expiry {
                Some(date) => Some(date.to_marionette()?),
                None => None,
            },
            same_site: self.sameSite.clone(),
        })
    }
}

impl ToMarionette<MarionetteDate> for Date {
    fn to_marionette(&self) -> WebDriverResult<MarionetteDate> {
        Ok(MarionetteDate(self.0))
    }
}

impl ToMarionette<Map<String, Value>> for GetNamedCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        )
        .clone())
    }
}

impl ToMarionette<MarionetteUrl> for GetParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteUrl> {
        Ok(MarionetteUrl {
            url: self.url.clone(),
        })
    }
}

impl ToMarionette<MarionetteScript> for JavascriptCommandParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteScript> {
        Ok(MarionetteScript {
            script: self.script.clone(),
            args: self.args.clone(),
        })
    }
}

impl ToMarionette<MarionetteLocator> for LocatorParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteLocator> {
        Ok(MarionetteLocator {
            using: self.using.to_marionette()?,
            value: self.value.clone(),
        })
    }
}

impl ToMarionette<MarionetteSelector> for LocatorStrategy {
    fn to_marionette(&self) -> WebDriverResult<MarionetteSelector> {
        use self::LocatorStrategy::*;
        match self {
            CSSSelector => Ok(MarionetteSelector::Css),
            LinkText => Ok(MarionetteSelector::LinkText),
            PartialLinkText => Ok(MarionetteSelector::PartialLinkText),
            TagName => Ok(MarionetteSelector::TagName),
            XPath => Ok(MarionetteSelector::XPath),
        }
    }
}

impl ToMarionette<MarionetteNewWindow> for NewWindowParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteNewWindow> {
        Ok(MarionetteNewWindow {
            type_hint: self.type_hint.clone(),
        })
    }
}

impl ToMarionette<MarionetteKeys> for SendKeysParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteKeys> {
        Ok(MarionetteKeys {
            text: self.text.clone(),
            value: self
                .text
                .chars()
                .map(|x| x.to_string())
                .collect::<Vec<String>>(),
        })
    }
}

impl ToMarionette<MarionetteFrame> for SwitchToFrameParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteFrame> {
        Ok(match &self.id {
            Some(x) => match x {
                FrameId::Short(n) => MarionetteFrame::Index(*n),
                FrameId::Element(el) => MarionetteFrame::Element(el.0.clone()),
            },
            None => MarionetteFrame::Parent,
        })
    }
}

impl ToMarionette<Window> for SwitchToWindowParameters {
    fn to_marionette(&self) -> WebDriverResult<Window> {
        Ok(Window {
            handle: self.handle.clone(),
        })
    }
}

impl ToMarionette<MarionetteTimeouts> for TimeoutsParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteTimeouts> {
        Ok(MarionetteTimeouts {
            implicit: self.implicit,
            page_load: self.page_load,
            script: self.script,
        })
    }
}

impl ToMarionette<MarionetteWebElement> for WebElement {
    fn to_marionette(&self) -> WebDriverResult<MarionetteWebElement> {
        Ok(MarionetteWebElement {
            element: self.to_string(),
        })
    }
}

impl ToMarionette<MarionetteWindowRect> for WindowRectParameters {
    fn to_marionette(&self) -> WebDriverResult<MarionetteWindowRect> {
        Ok(MarionetteWindowRect {
            x: self.x,
            y: self.y,
            width: self.width,
            height: self.height,
        })
    }
}
