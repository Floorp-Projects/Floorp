use crate::command::{
    AddonInstallParameters, AddonUninstallParameters, GeckoContextParameters,
    GeckoExtensionCommand, GeckoExtensionRoute, XblLocatorParameters, CHROME_ELEMENT_KEY,
    LEGACY_ELEMENT_KEY,
};
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use mozrunner::runner::{FirefoxProcess, FirefoxRunner, Runner, RunnerProcess};
use serde::de::{self, Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use serde_json::{self, Map, Value};
use std::error::Error;
use std::io::prelude::*;
use std::io::Error as IoError;
use std::io::ErrorKind;
use std::io::Result as IoResult;
use std::net::{TcpListener, TcpStream};
use std::path::PathBuf;
use std::sync::Mutex;
use std::thread;
use std::time;
use webdriver::capabilities::CapabilitiesMatching;
use webdriver::command::WebDriverCommand::{AcceptAlert, AddCookie, NewWindow, CloseWindow,
                                           DeleteCookie, DeleteCookies, DeleteSession,
                                           DismissAlert, ElementClear, ElementClick,
                                           ElementSendKeys, ExecuteAsyncScript, ExecuteScript,
                                           Extension, FindElement, FindElementElement,
                                           FindElementElements, FindElements, FullscreenWindow,
                                           Get, GetActiveElement, GetAlertText, GetCSSValue,
                                           GetCookies, GetCurrentUrl, GetElementAttribute,
                                           GetElementProperty, GetElementRect, GetElementTagName,
                                           GetElementText, GetNamedCookie, GetPageSource,
                                           GetTimeouts, GetTitle, GetWindowHandle,
                                           GetWindowHandles, GetWindowRect, GoBack, GoForward,
                                           IsDisplayed, IsEnabled, IsSelected, MaximizeWindow,
                                           MinimizeWindow, NewSession, PerformActions, Refresh,
                                           ReleaseActions, SendAlertText, SetTimeouts,
                                           SetWindowRect, Status, SwitchToFrame,
                                           SwitchToParentFrame, SwitchToWindow,
                                           TakeElementScreenshot, TakeScreenshot};
use webdriver::command::{ActionsParameters, AddCookieParameters, GetNamedCookieParameters,
                         GetParameters, JavascriptCommandParameters, LocatorParameters,
                         NewSessionParameters, SwitchToFrameParameters, SwitchToWindowParameters,
                         TimeoutsParameters, WindowRectParameters, NewWindowParameters};
use webdriver::command::{WebDriverCommand, WebDriverMessage};
use webdriver::common::{Cookie, FrameId, WebElement, ELEMENT_KEY, FRAME_KEY, WINDOW_KEY};
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};
use webdriver::response::{NewWindowResponse, CloseWindowResponse, CookieResponse, CookiesResponse,
                          ElementRectResponse, NewSessionResponse, TimeoutsResponse,
                          ValueResponse, WebDriverResponse, WindowRectResponse};
use webdriver::server::{Session, WebDriverHandler};

use crate::build::BuildInfo;
use crate::capabilities::{FirefoxCapabilities, FirefoxOptions};
use crate::logging;
use crate::prefs;

#[derive(Debug, PartialEq, Deserialize)]
pub struct MarionetteHandshake {
    #[serde(rename = "marionetteProtocol")]
    protocol: u16,
    #[serde(rename = "applicationType")]
    application_type: String,
}

#[derive(Default)]
pub struct MarionetteSettings {
    pub host: String,
    pub port: Option<u16>,
    pub binary: Option<PathBuf>,
    pub connect_existing: bool,

    /// Brings up the Browser Toolbox when starting Firefox,
    /// letting you debug internals.
    pub jsdebugger: bool,
}

#[derive(Default)]
pub struct MarionetteHandler {
    pub connection: Mutex<Option<MarionetteConnection>>,
    pub settings: MarionetteSettings,
    pub browser: Option<FirefoxProcess>,
}

impl MarionetteHandler {
    pub fn new(settings: MarionetteSettings) -> MarionetteHandler {
        MarionetteHandler {
            connection: Mutex::new(None),
            settings,
            browser: None,
        }
    }

    pub fn create_connection(
        &mut self,
        session_id: &Option<String>,
        new_session_parameters: &NewSessionParameters,
    ) -> WebDriverResult<Map<String, Value>> {
        let (options, capabilities) = {
            let mut fx_capabilities = FirefoxCapabilities::new(self.settings.binary.as_ref());
            let mut capabilities = new_session_parameters
                .match_browser(&mut fx_capabilities)?
                .ok_or(WebDriverError::new(
                    ErrorStatus::SessionNotCreated,
                    "Unable to find a matching set of capabilities",
                ))?;

            let options = FirefoxOptions::from_capabilities(
                fx_capabilities.chosen_binary,
                &mut capabilities,
            )?;
            (options, capabilities)
        };

        if let Some(l) = options.log.level {
            logging::set_max_level(l);
        }

        let host = self.settings.host.to_owned();
        let port = self.settings.port.unwrap_or(get_free_port(&host)?);
        if !self.settings.connect_existing {
            self.start_browser(port, options)?;
        }

        let mut connection = MarionetteConnection::new(host, port, session_id.clone());
        connection.connect(&mut self.browser).or_else(|e| {
            if let Some(ref mut runner) = self.browser {
                runner.kill()?;
            }
            Err(e)
        })?;
        self.connection = Mutex::new(Some(connection));
        Ok(capabilities)
    }

    fn start_browser(&mut self, port: u16, options: FirefoxOptions) -> WebDriverResult<()> {
        let binary = options.binary.ok_or(WebDriverError::new(
            ErrorStatus::SessionNotCreated,
            "Expected browser binary location, but unable to find \
             binary in default location, no \
             'moz:firefoxOptions.binary' capability provided, and \
             no binary flag set on the command line",
        ))?;

        let is_custom_profile = options.profile.is_some();

        let mut profile = match options.profile {
            Some(x) => x,
            None => Profile::new(None)?,
        };

        self.set_prefs(port, &mut profile, is_custom_profile, options.prefs)
            .map_err(|e| {
                WebDriverError::new(
                    ErrorStatus::SessionNotCreated,
                    format!("Failed to set preferences: {}", e),
                )
            })?;

        let mut runner = FirefoxRunner::new(&binary, profile);

        // https://developer.mozilla.org/docs/Environment_variables_affecting_crash_reporting
        runner
            .env("MOZ_CRASHREPORTER", "1")
            .env("MOZ_CRASHREPORTER_NO_REPORT", "1")
            .env("MOZ_CRASHREPORTER_SHUTDOWN", "1");

        // double-dashed flags are not accepted on Windows systems
        runner.arg("-marionette");
        if self.settings.jsdebugger {
            runner.arg("-jsdebugger");
        }
        if let Some(args) = options.args.as_ref() {
            runner.args(args);
        }

        let browser_proc = runner.start().map_err(|e| {
            WebDriverError::new(
                ErrorStatus::SessionNotCreated,
                format!("Failed to start browser {}: {}", binary.display(), e),
            )
        })?;
        self.browser = Some(browser_proc);

        Ok(())
    }

    pub fn set_prefs(
        &self,
        port: u16,
        profile: &mut Profile,
        custom_profile: bool,
        extra_prefs: Vec<(String, Pref)>,
    ) -> WebDriverResult<()> {
        let prefs = profile.user_prefs().map_err(|_| {
            WebDriverError::new(
                ErrorStatus::UnknownError,
                "Unable to read profile preferences file",
            )
        })?;

        for &(ref name, ref value) in prefs::DEFAULT.iter() {
            if !custom_profile || !prefs.contains_key(name) {
                prefs.insert((*name).clone(), (*value).clone());
            }
        }

        prefs.insert_slice(&extra_prefs[..]);

        if self.settings.jsdebugger {
            prefs.insert("devtools.browsertoolbox.panel", Pref::new("jsdebugger"));
            prefs.insert("devtools.debugger.remote-enabled", Pref::new(true));
            prefs.insert("devtools.chrome.enabled", Pref::new(true));
            prefs.insert("devtools.debugger.prompt-connection", Pref::new(false));
            prefs.insert("marionette.debugging.clicktostart", Pref::new(true));
        }

        prefs.insert("marionette.log.level", logging::max_level().into());
        prefs.insert("marionette.port", Pref::new(port));

        prefs.write().map_err(|e| {
            WebDriverError::new(
                ErrorStatus::UnknownError,
                format!("Unable to write Firefox profile: {}", e),
            )
        })
    }
}

impl WebDriverHandler<GeckoExtensionRoute> for MarionetteHandler {
    fn handle_command(
        &mut self,
        _: &Option<Session>,
        msg: WebDriverMessage<GeckoExtensionRoute>,
    ) -> WebDriverResult<WebDriverResponse> {
        let mut resolved_capabilities = None;
        {
            let mut capabilities_options = None;
            // First handle the status message which doesn't actually require a marionette
            // connection or message
            if msg.command == Status {
                let (ready, message) = self.connection
                    .lock()
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
                Ok(ref connection) => {
                    if connection.is_none() {
                        match msg.command {
                            NewSession(ref capabilities) => {
                                capabilities_options = Some(capabilities);
                            }
                            _ => {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidSessionId,
                                    "Tried to run command without establishing a connection",
                                ));
                            }
                        }
                    }
                }
                Err(_) => {
                    return Err(WebDriverError::new(
                        ErrorStatus::UnknownError,
                        "Failed to aquire Marionette connection",
                    ))
                }
            }
            if let Some(capabilities) = capabilities_options {
                resolved_capabilities =
                    Some(self.create_connection(&msg.session_id, &capabilities)?);
            }
        }

        match self.connection.lock() {
            Ok(ref mut connection) => {
                match connection.as_mut() {
                    Some(conn) => {
                        conn.send_command(resolved_capabilities, &msg)
                            .map_err(|mut err| {
                                // Shutdown the browser if no session can
                                // be established due to errors.
                                if let NewSession(_) = msg.command {
                                    err.delete_session = true;
                                }
                                err
                            })
                    }
                    None => panic!("Connection missing"),
                }
            }
            Err(_) => Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                "Failed to aquire Marionette connection",
            )),
        }
    }

    fn delete_session(&mut self, session: &Option<Session>) {
        if let Some(ref s) = *session {
            let delete_session = WebDriverMessage {
                session_id: Some(s.id.clone()),
                command: WebDriverCommand::DeleteSession,
            };
            let _ = self.handle_command(session, delete_session);
        }

        if let Ok(ref mut connection) = self.connection.lock() {
            if let Some(conn) = connection.as_mut() {
                conn.close();
            }
        }

        if let Some(ref mut runner) = self.browser {
            // TODO(https://bugzil.la/1443922):
            // Use toolkit.asyncshutdown.crash_timout pref
            match runner.wait(time::Duration::from_secs(70)) {
                Ok(x) => debug!("Browser process stopped: {}", x),
                Err(e) => error!("Failed to stop browser process: {}", e),
            }
        }

        self.connection = Mutex::new(None);
        self.browser = None;
    }
}

pub struct MarionetteSession {
    pub session_id: String,
    protocol: Option<u16>,
    application_type: Option<String>,
    command_id: u64,
}

impl MarionetteSession {
    pub fn new(session_id: Option<String>) -> MarionetteSession {
        let initital_id = session_id.unwrap_or("".to_string());
        MarionetteSession {
            session_id: initital_id,
            protocol: None,
            application_type: None,
            command_id: 0,
        }
    }

    pub fn update(
        &mut self,
        msg: &WebDriverMessage<GeckoExtensionRoute>,
        resp: &MarionetteResponse,
    ) -> WebDriverResult<()> {
        match msg.command {
            NewSession(_) => {
                let session_id = try_opt!(
                    try_opt!(
                        resp.result.get("sessionId"),
                        ErrorStatus::SessionNotCreated,
                        "Unable to get session id"
                    ).as_str(),
                    ErrorStatus::SessionNotCreated,
                    "Unable to convert session id to string"
                );
                self.session_id = session_id.to_string().clone();
            }
            _ => {}
        }
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

        let chrome_element = data.get(CHROME_ELEMENT_KEY);
        let element = data.get(ELEMENT_KEY);
        let frame = data.get(FRAME_KEY);
        let legacy_element = data.get(LEGACY_ELEMENT_KEY);
        let window = data.get(WINDOW_KEY);

        let value = try_opt!(
            element
                .or(legacy_element)
                .or(chrome_element)
                .or(frame)
                .or(window),
            ErrorStatus::UnknownError,
            "Failed to extract web element from Marionette response"
        );
        let id = try_opt!(
            value.as_str(),
            ErrorStatus::UnknownError,
            "Failed to convert web element reference value to string"
        ).to_string();
        Ok(WebElement::new(id))
    }

    pub fn next_command_id(&mut self) -> u64 {
        self.command_id = self.command_id + 1;
        self.command_id
    }

    pub fn response(
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
            | IsEnabled(_)
            | ExecuteScript(_)
            | ExecuteAsyncScript(_)
            | GetAlertText
            | TakeScreenshot
            | TakeElementScreenshot(_) => WebDriverResponse::Generic(resp.to_value_response(true)?),
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
                // Check for the spec-compliant "pageLoad", but also for "page load",
                // which was sent by Firefox 52 and earlier.
                let page_load = try_opt!(
                    try_opt!(
                        resp.result.get("pageLoad").or(resp.result.get("page load")),
                        ErrorStatus::UnknownError,
                        "Missing field: pageLoad"
                    ).as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret page load duration as u64"
                );
                let implicit = try_opt!(
                    try_opt!(
                        resp.result.get("implicit"),
                        ErrorStatus::UnknownError,
                        "Missing field: implicit"
                    ).as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret implicit search duration as u64"
                );

                WebDriverResponse::Timeouts(TimeoutsResponse {
                    script: script,
                    page_load: page_load,
                    implicit: implicit,
                })
            }
            Status => panic!("Got status command that should already have been handled"),
            GetWindowHandles => WebDriverResponse::Generic(resp.to_value_response(false)?),
            NewWindow(_) => {
                let handle: String = try_opt!(
                    try_opt!(
                        resp.result.get("handle"),
                        ErrorStatus::UnknownError,
                        "Failed to find handle field"
                    ).as_str(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret handle as string"
                ).into();
                let typ: String = try_opt!(
                    try_opt!(
                        resp.result.get("type"),
                        ErrorStatus::UnknownError,
                        "Failed to find type field"
                    ).as_str(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret type as string"
                ).into();

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
                        ).to_owned())
                    }).collect::<Result<Vec<_>, _>>()?;
                WebDriverResponse::CloseWindow(CloseWindowResponse(handles))
            }
            GetElementRect(_) => {
                let x = try_opt!(
                    try_opt!(
                        resp.result.get("x"),
                        ErrorStatus::UnknownError,
                        "Failed to find x field"
                    ).as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as float"
                );

                let y = try_opt!(
                    try_opt!(
                        resp.result.get("y"),
                        ErrorStatus::UnknownError,
                        "Failed to find y field"
                    ).as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret y as float"
                );

                let width = try_opt!(
                    try_opt!(
                        resp.result.get("width"),
                        ErrorStatus::UnknownError,
                        "Failed to find width field"
                    ).as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as float"
                );

                let height = try_opt!(
                    try_opt!(
                        resp.result.get("height"),
                        ErrorStatus::UnknownError,
                        "Failed to find height field"
                    ).as_f64(),
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
                    ).as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as positive integer"
                );

                let height = try_opt!(
                    try_opt!(
                        resp.result.get("height"),
                        ErrorStatus::UnknownError,
                        "Failed to find heigenht field"
                    ).as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret height as positive integer"
                );

                let x = try_opt!(
                    try_opt!(
                        resp.result.get("x"),
                        ErrorStatus::UnknownError,
                        "Failed to find x field"
                    ).as_i64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as integer"
                );

                let y = try_opt!(
                    try_opt!(
                        resp.result.get("y"),
                        ErrorStatus::UnknownError,
                        "Failed to find y field"
                    ).as_i64(),
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
            FindElement(_) | FindElementElement(_, _) => {
                let element = self.to_web_element(try_opt!(
                    resp.result.get("value"),
                    ErrorStatus::UnknownError,
                    "Failed to find value field"
                ))?;
                WebDriverResponse::Generic(ValueResponse(serde_json::to_value(element)?))
            }
            FindElements(_) | FindElementElements(_, _) => {
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
                    ).as_str(),
                    ErrorStatus::InvalidSessionId,
                    "sessionId is not a string"
                );

                let mut capabilities = try_opt!(
                    try_opt!(
                        resp.result.get("capabilities"),
                        ErrorStatus::UnknownError,
                        "Failed to find capabilities field"
                    ).as_object(),
                    ErrorStatus::UnknownError,
                    "capabilities field is not an object"
                ).clone();

                capabilities.insert("moz:geckodriverVersion".into(), BuildInfo.into());

                WebDriverResponse::NewSession(NewSessionResponse::new(
                    session_id.to_string(),
                    Value::Object(capabilities.clone()),
                ))
            }
            DeleteSession => WebDriverResponse::DeleteSession,
            Extension(ref extension) => match extension {
                GetContext => WebDriverResponse::Generic(resp.to_value_response(true)?),
                SetContext(_) => WebDriverResponse::Void,
                XblAnonymousChildren(_) => {
                    let els_vec = try_opt!(
                        resp.result.as_array(),
                        ErrorStatus::UnknownError,
                        "Failed to interpret body as array"
                    );
                    let els = els_vec
                        .iter()
                        .map(|x| self.to_web_element(x))
                        .collect::<Result<Vec<_>, _>>()?;

                    WebDriverResponse::Generic(ValueResponse(serde_json::to_value(els)?))
                }
                XblAnonymousByAttribute(_, _) => {
                    let el = self.to_web_element(try_opt!(
                        resp.result.get("value"),
                        ErrorStatus::UnknownError,
                        "Failed to find value field"
                    ))?;
                    WebDriverResponse::Generic(ValueResponse(serde_json::to_value(el)?))
                }
                InstallAddon(_) => WebDriverResponse::Generic(resp.to_value_response(true)?),
                UninstallAddon(_) => WebDriverResponse::Void,
                TakeFullScreenshot => WebDriverResponse::Generic(resp.to_value_response(true)?),
            },
        })
    }
}

#[derive(Debug, PartialEq)]
pub struct MarionetteCommand {
    pub id: u64,
    pub name: String,
    pub params: Map<String, Value>,
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
    fn new(id: u64, name: String, params: Map<String, Value>) -> MarionetteCommand {
        MarionetteCommand {
            id: id,
            name: name,
            params: params,
        }
    }

    fn from_webdriver_message(
        id: u64,
        capabilities: Option<Map<String, Value>>,
        msg: &WebDriverMessage<GeckoExtensionRoute>,
    ) -> WebDriverResult<MarionetteCommand> {
        use self::GeckoExtensionCommand::*;

        let (opt_name, opt_parameters) = match msg.command {
            Status => panic!("Got status command that should already have been handled"),
            AcceptAlert => {
                // Needs to be updated to "WebDriver:AcceptAlert" for Firefox 63
                (Some("WebDriver:AcceptDialog"), None)
            }
            AddCookie(ref x) => (Some("WebDriver:AddCookie"), Some(x.to_marionette())),
            NewWindow(ref x) => (Some("WebDriver:NewWindow"), Some(x.to_marionette())),
            CloseWindow => (Some("WebDriver:CloseWindow"), None),
            DeleteCookie(ref x) => {
                let mut data = Map::new();
                data.insert("name".to_string(), Value::String(x.clone()));
                (Some("WebDriver:DeleteCookie"), Some(Ok(data)))
            }
            DeleteCookies => (Some("WebDriver:DeleteAllCookies"), None),
            DeleteSession => {
                let mut body = Map::new();
                body.insert(
                    "flags".to_owned(),
                    serde_json::to_value(vec!["eForceQuit".to_string()])?,
                );
                (Some("Marionette:Quit"), Some(Ok(body)))
            }
            DismissAlert => (Some("WebDriver:DismissAlert"), None),
            ElementClear(ref x) => (Some("WebDriver:ElementClear"), Some(x.to_marionette())),
            ElementClick(ref x) => (Some("WebDriver:ElementClick"), Some(x.to_marionette())),
            ElementSendKeys(ref e, ref x) => {
                let mut data = Map::new();
                data.insert("id".to_string(), Value::String(e.id.clone()));
                data.insert("text".to_string(), Value::String(x.text.clone()));
                data.insert(
                    "value".to_string(),
                    serde_json::to_value(
                        x.text
                            .chars()
                            .map(|x| x.to_string())
                            .collect::<Vec<String>>(),
                    )?,
                );
                (Some("WebDriver:ElementSendKeys"), Some(Ok(data)))
            }
            ExecuteAsyncScript(ref x) => (
                Some("WebDriver:ExecuteAsyncScript"),
                Some(x.to_marionette()),
            ),
            ExecuteScript(ref x) => (Some("WebDriver:ExecuteScript"), Some(x.to_marionette())),
            FindElement(ref x) => (Some("WebDriver:FindElement"), Some(x.to_marionette())),
            FindElementElement(ref e, ref x) => {
                let mut data = x.to_marionette()?;
                data.insert("element".to_string(), Value::String(e.id.clone()));
                (Some("WebDriver:FindElement"), Some(Ok(data)))
            }
            FindElements(ref x) => (Some("WebDriver:FindElements"), Some(x.to_marionette())),
            FindElementElements(ref e, ref x) => {
                let mut data = x.to_marionette()?;
                data.insert("element".to_string(), Value::String(e.id.clone()));
                (Some("WebDriver:FindElements"), Some(Ok(data)))
            }
            FullscreenWindow => (Some("WebDriver:FullscreenWindow"), None),
            Get(ref x) => (Some("WebDriver:Navigate"), Some(x.to_marionette())),
            GetAlertText => (Some("WebDriver:GetAlertText"), None),
            GetActiveElement => (Some("WebDriver:GetActiveElement"), None),
            GetCookies | GetNamedCookie(_) => (Some("WebDriver:GetCookies"), None),
            GetCurrentUrl => (Some("WebDriver:GetCurrentURL"), None),
            GetCSSValue(ref e, ref x) => {
                let mut data = Map::new();
                data.insert("id".to_string(), Value::String(e.id.clone()));
                data.insert("propertyName".to_string(), Value::String(x.clone()));
                (Some("WebDriver:GetElementCSSValue"), Some(Ok(data)))
            }
            GetElementAttribute(ref e, ref x) => {
                let mut data = Map::new();
                data.insert("id".to_string(), Value::String(e.id.clone()));
                data.insert("name".to_string(), Value::String(x.clone()));
                (Some("WebDriver:GetElementAttribute"), Some(Ok(data)))
            }
            GetElementProperty(ref e, ref x) => {
                let mut data = Map::new();
                data.insert("id".to_string(), Value::String(e.id.clone()));
                data.insert("name".to_string(), Value::String(x.clone()));
                (Some("WebDriver:GetElementProperty"), Some(Ok(data)))
            }
            GetElementRect(ref x) => (Some("WebDriver:GetElementRect"), Some(x.to_marionette())),
            GetElementTagName(ref x) => {
                (Some("WebDriver:GetElementTagName"), Some(x.to_marionette()))
            }
            GetElementText(ref x) => (Some("WebDriver:GetElementText"), Some(x.to_marionette())),
            GetPageSource => (Some("WebDriver:GetPageSource"), None),
            GetTimeouts => (Some("WebDriver:GetTimeouts"), None),
            GetTitle => (Some("WebDriver:GetTitle"), None),
            GetWindowHandle => (Some("WebDriver:GetWindowHandle"), None),
            GetWindowHandles => (Some("WebDriver:GetWindowHandles"), None),
            GetWindowRect => (Some("WebDriver:GetWindowRect"), None),
            GoBack => (Some("WebDriver:Back"), None),
            GoForward => (Some("WebDriver:Forward"), None),
            IsDisplayed(ref x) => (
                Some("WebDriver:IsElementDisplayed"),
                Some(x.to_marionette()),
            ),
            IsEnabled(ref x) => (Some("WebDriver:IsElementEnabled"), Some(x.to_marionette())),
            IsSelected(ref x) => (Some("WebDriver:IsElementSelected"), Some(x.to_marionette())),
            MaximizeWindow => (Some("WebDriver:MaximizeWindow"), None),
            MinimizeWindow => (Some("WebDriver:MinimizeWindow"), None),
            NewSession(_) => {
                let caps = capabilities
                    .expect("Tried to create new session without processing capabilities");

                let mut data = Map::new();
                for (k, v) in caps.iter() {
                    data.insert(k.to_string(), serde_json::to_value(v)?);
                }

                (Some("WebDriver:NewSession"), Some(Ok(data)))
            }
            PerformActions(ref x) => (Some("WebDriver:PerformActions"), Some(x.to_marionette())),
            Refresh => (Some("WebDriver:Refresh"), None),
            ReleaseActions => (Some("WebDriver:ReleaseActions"), None),
            SendAlertText(ref x) => {
                let mut data = Map::new();
                data.insert("text".to_string(), Value::String(x.text.clone()));
                data.insert(
                    "value".to_string(),
                    serde_json::to_value(
                        x.text
                            .chars()
                            .map(|x| x.to_string())
                            .collect::<Vec<String>>(),
                    )?,
                );
                (Some("WebDriver:SendAlertText"), Some(Ok(data)))
            }
            SetTimeouts(ref x) => (Some("WebDriver:SetTimeouts"), Some(x.to_marionette())),
            SetWindowRect(ref x) => (Some("WebDriver:SetWindowRect"), Some(x.to_marionette())),
            SwitchToFrame(ref x) => (Some("WebDriver:SwitchToFrame"), Some(x.to_marionette())),
            SwitchToParentFrame => (Some("WebDriver:SwitchToParentFrame"), None),
            SwitchToWindow(ref x) => (Some("WebDriver:SwitchToWindow"), Some(x.to_marionette())),
            TakeElementScreenshot(ref e) => {
                let mut data = Map::new();
                data.insert("id".to_string(), Value::String(e.id.clone()));
                data.insert("highlights".to_string(), Value::Array(vec![]));
                data.insert("full".to_string(), Value::Bool(false));
                (Some("WebDriver:TakeScreenshot"), Some(Ok(data)))
            }
            TakeScreenshot => {
                let mut data = Map::new();
                data.insert("id".to_string(), Value::Null);
                data.insert("highlights".to_string(), Value::Array(vec![]));
                data.insert("full".to_string(), Value::Bool(false));
                (Some("WebDriver:TakeScreenshot"), Some(Ok(data)))
            }
            Extension(ref extension) => match extension {
                GetContext => (Some("Marionette:GetContext"), None),
                InstallAddon(x) => {
                    (Some("Addon:Install"), Some(x.to_marionette()))
                }
                SetContext(x) => {
                    (Some("Marionette:SetContext"), Some(x.to_marionette()))
                }
                UninstallAddon(x) => {
                    (Some("Addon:Uninstall"), Some(x.to_marionette()))
                }
                XblAnonymousByAttribute(e, x) => {
                    let mut data = x.to_marionette()?;
                    data.insert("element".to_string(), Value::String(e.id.clone()));
                    (Some("WebDriver:FindElement"), Some(Ok(data)))
                }
                XblAnonymousChildren(e) => {
                    let mut data = Map::new();
                    data.insert("using".to_owned(), serde_json::to_value("anon")?);
                    data.insert("value".to_owned(), Value::Null);
                    data.insert("element".to_string(), serde_json::to_value(e.id.clone())?);
                    (Some("WebDriver:FindElements"), Some(Ok(data)))
                }
                TakeFullScreenshot => {
                    let mut data = Map::new();
                    data.insert("id".to_string(), Value::Null);
                    data.insert("highlights".to_string(), Value::Array(vec![]));
                    data.insert("full".to_string(), Value::Bool(true));
                    (Some("WebDriver:TakeScreenshot"), Some(Ok(data)))
                }
            },
        };

        let name = try_opt!(
            opt_name,
            ErrorStatus::UnsupportedOperation,
            "Operation not supported"
        );
        let parameters = opt_parameters.unwrap_or(Ok(Map::new()))?;

        Ok(MarionetteCommand::new(id, name.into(), parameters))
    }
}

#[derive(Debug, PartialEq)]
pub struct MarionetteResponse {
    pub id: u64,
    pub error: Option<MarionetteError>,
    pub result: Value,
}

impl<'de> Deserialize<'de> for MarionetteResponse {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Deserialize)]
        struct ResponseWrapper {
            msg_type: u64,
            id: u64,
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
    fn to_value_response(self, value_required: bool) -> WebDriverResult<ValueResponse> {
        let value: &Value = match value_required {
            true => try_opt!(
                self.result.get("value"),
                ErrorStatus::UnknownError,
                "Failed to find value field"
            ),
            false => &self.result,
        };

        Ok(ValueResponse(value.clone()))
    }
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct MarionetteError {
    #[serde(rename = "error")]
    pub code: String,
    pub message: String,
    pub stacktrace: Option<String>,
}

impl Into<WebDriverError> for MarionetteError {
    fn into(self) -> WebDriverError {
        let status = ErrorStatus::from(self.code);
        let message = self.message;

        if let Some(stack) = self.stacktrace {
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

pub struct MarionetteConnection {
    host: String,
    port: u16,
    stream: Option<TcpStream>,
    pub session: MarionetteSession,
}

impl MarionetteConnection {
    pub fn new(host: String, port: u16, session_id: Option<String>) -> MarionetteConnection {
        let session = MarionetteSession::new(session_id);
        MarionetteConnection {
            host,
            port,
            stream: None,
            session,
        }
    }

    pub fn connect(&mut self, browser: &mut Option<FirefoxProcess>) -> WebDriverResult<()> {
        let timeout = time::Duration::from_secs(60);
        let poll_interval = time::Duration::from_millis(100);
        let now = time::Instant::now();

        debug!(
            "Waiting {}s to connect to browser on {}:{}",
            timeout.as_secs(),
            self.host,
            self.port
        );
        loop {
            // immediately abort connection attempts if process disappears
            if let &mut Some(ref mut runner) = browser {
                let exit_status = match runner.try_wait() {
                    Ok(Some(status)) => Some(
                        status
                            .code()
                            .map(|c| c.to_string())
                            .unwrap_or("signal".into()),
                    ),
                    Ok(None) => None,
                    Err(_) => Some("{unknown}".into()),
                };
                if let Some(s) = exit_status {
                    return Err(WebDriverError::new(
                        ErrorStatus::UnknownError,
                        format!("Process unexpectedly closed with status {}", s),
                    ));
                }
            }

            match TcpStream::connect((&self.host[..], self.port)) {
                Ok(stream) => {
                    self.stream = Some(stream);
                    break;
                }
                Err(e) => {
                    if now.elapsed() < timeout {
                        thread::sleep(poll_interval);
                    } else {
                        return Err(WebDriverError::new(
                            ErrorStatus::UnknownError,
                            e.description().to_owned(),
                        ));
                    }
                }
            }
        }

        debug!(
            "Connection established on {}:{}. Waiting for Marionette handshake",
            self.host, self.port,
        );

        let data = self.handshake()?;
        self.session.application_type = Some(data.application_type);
        self.session.protocol = Some(data.protocol);

        debug!("Connected to Marionette");
        Ok(())
    }

    fn handshake(&mut self) -> WebDriverResult<MarionetteHandshake> {
        let resp = (match self.stream.as_mut().unwrap().read_timeout() {
            Ok(timeout) => {
                // If platform supports changing the read timeout of the stream,
                // use a short one only for the handshake with Marionette.
                self.stream
                    .as_mut()
                    .unwrap()
                    .set_read_timeout(Some(time::Duration::from_secs(10)))
                    .ok();
                let data = self.read_resp();
                self.stream.as_mut().unwrap().set_read_timeout(timeout).ok();

                data
            }
            _ => self.read_resp(),
        }).or_else(|e| {
            Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                format!("Socket timeout reading Marionette handshake data: {}", e),
            ))
        })?;

        let data = serde_json::from_str::<MarionetteHandshake>(&resp)?;

        if data.application_type != "gecko" {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                format!(
                    "Unrecognized application type {}",
                    data.application_type
                ),
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

    pub fn close(&self) {}

    fn encode_msg(&self, msg: MarionetteCommand) -> WebDriverResult<String> {
        let data = serde_json::to_string(&msg)?;

        Ok(format!("{}:{}", data.len(), data))
    }

    pub fn send_command(
        &mut self,
        capabilities: Option<Map<String, Value>>,
        msg: &WebDriverMessage<GeckoExtensionRoute>,
    ) -> WebDriverResult<WebDriverResponse> {
        let id = self.session.next_command_id();
        let command = MarionetteCommand::from_webdriver_message(id, capabilities, msg)?;
        let resp_data = self.send(command)?;
        let data: MarionetteResponse = serde_json::from_str(&resp_data)?;

        self.session.response(msg, data)
    }

    fn send(&mut self, msg: MarionetteCommand) -> WebDriverResult<String> {
        let data = self.encode_msg(msg)?;

        match self.stream {
            Some(ref mut stream) => {
                if stream.write(&*data.as_bytes()).is_err() {
                    let mut err = WebDriverError::new(
                        ErrorStatus::UnknownError,
                        "Failed to write response to stream",
                    );
                    err.delete_session = true;
                    return Err(err);
                }
            }
            None => {
                let mut err = WebDriverError::new(
                    ErrorStatus::UnknownError,
                    "Tried to write before opening stream",
                );
                err.delete_session = true;
                return Err(err);
            }
        }

        match self.read_resp() {
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

    fn read_resp(&mut self) -> IoResult<String> {
        let mut bytes = 0usize;

        // TODO(jgraham): Check before we unwrap?
        let stream = self.stream.as_mut().unwrap();
        loop {
            let buf = &mut [0 as u8];
            let num_read = stream.read(buf)?;
            let byte = match num_read {
                0 => {
                    return Err(IoError::new(
                        ErrorKind::Other,
                        "EOF reading marionette message",
                    ))
                }
                1 => buf[0] as char,
                _ => panic!("Expected one byte got more"),
            };
            match byte {
                '0'...'9' => {
                    bytes = bytes * 10;
                    bytes += byte as usize - '0' as usize;
                }
                ':' => break,
                _ => {}
            }
        }

        let buf = &mut [0 as u8; 8192];
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

trait ToMarionette {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>>;
}

impl ToMarionette for AddonInstallParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert("path".to_string(), serde_json::to_value(&self.path)?);
        if self.temporary.is_some() {
            data.insert("temporary".to_string(), serde_json::to_value(&self.temporary)?);
        }
        Ok(data)
    }
}

impl ToMarionette for AddonUninstallParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert("id".to_string(), Value::String(self.id.clone()));
        Ok(data)
    }
}

impl ToMarionette for GeckoContextParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert(
            "value".to_owned(),
            serde_json::to_value(self.context.clone())?,
        );
        Ok(data)
    }
}

impl ToMarionette for XblLocatorParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut value = Map::new();
        value.insert(self.name.to_owned(), Value::String(self.value.clone()));

        let mut data = Map::new();
        data.insert(
            "using".to_owned(),
            Value::String("anon attribute".to_string()),
        );
        data.insert("value".to_owned(), Value::Object(value));
        Ok(data)
    }
}

impl ToMarionette for ActionsParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        ).clone())
    }
}

impl ToMarionette for AddCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut cookie = Map::new();
        cookie.insert("name".to_string(), serde_json::to_value(&self.name)?);
        cookie.insert("value".to_string(), serde_json::to_value(&self.value)?);
        if self.path.is_some() {
            cookie.insert("path".to_string(), serde_json::to_value(&self.path)?);
        }
        if self.domain.is_some() {
            cookie.insert("domain".to_string(), serde_json::to_value(&self.domain)?);
        }
        if self.expiry.is_some() {
            cookie.insert("expiry".to_string(), serde_json::to_value(&self.expiry)?);
        }
        cookie.insert("secure".to_string(), serde_json::to_value(self.secure)?);
        cookie.insert("httpOnly".to_string(), serde_json::to_value(self.httpOnly)?);

        let mut data = Map::new();
        data.insert("cookie".to_string(), serde_json::to_value(cookie)?);
        Ok(data)
    }
}

impl ToMarionette for FrameId {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        match *self {
            FrameId::Short(x) => data.insert("id".to_string(), serde_json::to_value(x)?),
            FrameId::Element(ref x) => data.insert(
                "element".to_string(),
                Value::Object(x.to_marionette()?),
            ),
        };
        Ok(data)
    }
}

impl ToMarionette for GetNamedCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        ).clone())
    }
}

impl ToMarionette for GetParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        ).clone())
    }
}

impl ToMarionette for JavascriptCommandParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        ).clone())
    }
}

impl ToMarionette for LocatorParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        ).clone())
    }
}

impl ToMarionette for NewWindowParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        if let Some(ref x) = self.type_hint {
            data.insert("type".to_string(), serde_json::to_value(x)?);
        }
        Ok(data)
    }
}

impl ToMarionette for SwitchToFrameParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        let key = match self.id {
            None => None,
            Some(FrameId::Short(_)) => Some("id"),
            Some(FrameId::Element(_)) => Some("element"),
        };
        if let Some(x) = key {
            data.insert(x.to_string(), serde_json::to_value(&self.id)?);
        }
        Ok(data)
    }
}

impl ToMarionette for SwitchToWindowParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert(
            "name".to_string(),
            serde_json::to_value(self.handle.clone())?,
        );
        Ok(data)
    }
}

impl ToMarionette for TimeoutsParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        ).clone())
    }
}

impl ToMarionette for WebElement {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        let mut data = Map::new();
        data.insert("id".to_string(), serde_json::to_value(&self.id)?);
        Ok(data)
    }
}

impl ToMarionette for WindowRectParameters {
    fn to_marionette(&self) -> WebDriverResult<Map<String, Value>> {
        Ok(try_opt!(
            serde_json::to_value(self)?.as_object(),
            ErrorStatus::UnknownError,
            "Expected an object"
        ).clone())
    }
}

#[cfg(test)]
mod tests {
    use super::{MarionetteHandler, MarionetteSettings};
    use mozprofile::preferences::PrefValue;
    use mozprofile::profile::Profile;

    // This is not a pretty test, mostly due to the nature of
    // mozprofile's and MarionetteHandler's APIs, but we have had
    // several regressions related to marionette.log.level.
    #[test]
    fn test_marionette_log_level() {
        let mut profile = Profile::new(None).unwrap();
        let handler = MarionetteHandler::new(MarionetteSettings::default());
        handler.set_prefs(2828, &mut profile, false, vec![]).ok();
        let user_prefs = profile.user_prefs().unwrap();

        let pref = user_prefs.get("marionette.log.level").unwrap();
        let value = match pref.value {
            PrefValue::String(ref s) => s,
            _ => panic!(),
        };
        for (i, ch) in value.chars().enumerate() {
            if i == 0 {
                assert!(ch.is_uppercase());
            } else {
                assert!(ch.is_lowercase());
            }
        }
    }
}
