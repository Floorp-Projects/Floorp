use hyper::method::Method;
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use mozrunner::runner::{FirefoxRunner, FirefoxProcess, Runner, RunnerProcess};
use regex::Captures;
use rustc_serialize::base64::FromBase64;
use rustc_serialize::json;
use rustc_serialize::json::{Json, ToJson};
use std::collections::BTreeMap;
use std::env;
use std::error::Error;
use std::fs::File;
use std::io::Error as IoError;
use std::io::ErrorKind;
use std::io::prelude::*;
use std::path::PathBuf;
use std::io::Result as IoResult;
use std::net::{TcpListener, TcpStream};
use std::sync::Mutex;
use std::thread;
use std::time;
use uuid::Uuid;
use webdriver::capabilities::CapabilitiesMatching;
use webdriver::command::{WebDriverCommand, WebDriverMessage, Parameters,
                         WebDriverExtensionCommand};
use webdriver::command::WebDriverCommand::{
    NewSession, DeleteSession, Status, Get, GetCurrentUrl,
    GoBack, GoForward, Refresh, GetTitle, GetPageSource, GetWindowHandle,
    GetWindowHandles, CloseWindow, SetWindowRect, GetWindowRect,
    MinimizeWindow, MaximizeWindow, FullscreenWindow, SwitchToWindow, SwitchToFrame,
    SwitchToParentFrame, FindElement, FindElements,
    FindElementElement, FindElementElements, GetActiveElement,
    IsDisplayed, IsSelected, GetElementAttribute, GetElementProperty, GetCSSValue,
    GetElementText, GetElementTagName, GetElementRect, IsEnabled,
    ElementClick, ElementTap, ElementClear, ElementSendKeys,
    ExecuteScript, ExecuteAsyncScript, GetCookies, GetNamedCookie, AddCookie,
    DeleteCookies, DeleteCookie, GetTimeouts, SetTimeouts, DismissAlert,
    AcceptAlert, GetAlertText, SendAlertText, TakeScreenshot, TakeElementScreenshot,
    Extension, PerformActions, ReleaseActions};
use webdriver::command::{
    NewSessionParameters, GetParameters, WindowRectParameters, SwitchToWindowParameters,
    SwitchToFrameParameters, LocatorParameters, JavascriptCommandParameters,
    GetNamedCookieParameters, AddCookieParameters, TimeoutsParameters,
    ActionsParameters, TakeScreenshotParameters};
use webdriver::response::{CloseWindowResponse, Cookie, CookieResponse, CookiesResponse,
                          ElementRectResponse, NewSessionResponse, TimeoutsResponse,
                          ValueResponse, WebDriverResponse, WindowRectResponse};
use webdriver::common::{Date, ELEMENT_KEY, FrameId, FRAME_KEY, Nullable, WebElement, WINDOW_KEY};
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};
use webdriver::server::{WebDriverHandler, Session};
use webdriver::httpapi::{WebDriverExtensionRoute};

use build::BuildInfo;
use capabilities::{FirefoxCapabilities, FirefoxOptions};
use logging;
use prefs;

// localhost may be routed to the IPv6 stack on certain systems,
// and nsIServerSocket in Marionette only supports IPv4
const DEFAULT_HOST: &'static str = "127.0.0.1";

const CHROME_ELEMENT_KEY: &'static str = "chromeelement-9fc5-4b51-a3c8-01716eedeb04";
const LEGACY_ELEMENT_KEY: &'static str = "ELEMENT";

pub fn extension_routes() -> Vec<(Method, &'static str, GeckoExtensionRoute)> {
    return vec![(Method::Get, "/session/{sessionId}/moz/context", GeckoExtensionRoute::GetContext),
             (Method::Post, "/session/{sessionId}/moz/context", GeckoExtensionRoute::SetContext),
             (Method::Post,
              "/session/{sessionId}/moz/xbl/{elementId}/anonymous_children",
              GeckoExtensionRoute::XblAnonymousChildren),
             (Method::Post,
              "/session/{sessionId}/moz/xbl/{elementId}/anonymous_by_attribute",
              GeckoExtensionRoute::XblAnonymousByAttribute),
             (Method::Post, "/session/{sessionId}/moz/addon/install",
                GeckoExtensionRoute::InstallAddon),
             (Method::Post, "/session/{sessionId}/moz/addon/uninstall",
                GeckoExtensionRoute::UninstallAddon)];
}

#[derive(Clone, PartialEq)]
pub enum GeckoExtensionRoute {
    GetContext,
    SetContext,
    XblAnonymousChildren,
    XblAnonymousByAttribute,
    InstallAddon,
    UninstallAddon,
}

impl WebDriverExtensionRoute for GeckoExtensionRoute {
    type Command = GeckoExtensionCommand;

    fn command(&self,
               captures: &Captures,
               body_data: &Json)
               -> WebDriverResult<WebDriverCommand<GeckoExtensionCommand>> {
        let command = match self {
            &GeckoExtensionRoute::GetContext => GeckoExtensionCommand::GetContext,
            &GeckoExtensionRoute::SetContext => {
                let parameters: GeckoContextParameters = try!(Parameters::from_json(&body_data));
                GeckoExtensionCommand::SetContext(parameters)
            }
            &GeckoExtensionRoute::XblAnonymousChildren => {
                let element_id = try!(captures.name("elementId")
                    .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                               "Missing elementId parameter")));
                GeckoExtensionCommand::XblAnonymousChildren(element_id.as_str().into())
            }
            &GeckoExtensionRoute::XblAnonymousByAttribute => {
                let element_id = try!(captures.name("elementId")
                    .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                               "Missing elementId parameter")));
                let parameters: AttributeParameters = try!(Parameters::from_json(&body_data));
                GeckoExtensionCommand::XblAnonymousByAttribute(element_id.as_str().into(),
                                                               parameters)
            }
            &GeckoExtensionRoute::InstallAddon => {
                let parameters: AddonInstallParameters = try!(Parameters::from_json(&body_data));
                GeckoExtensionCommand::InstallAddon(parameters)
            }
            &GeckoExtensionRoute::UninstallAddon => {
                let parameters: AddonUninstallParameters = try!(Parameters::from_json(&body_data));
                GeckoExtensionCommand::UninstallAddon(parameters)
            }
        };
        Ok(WebDriverCommand::Extension(command))
    }
}

#[derive(Clone, PartialEq)]
pub enum GeckoExtensionCommand {
    GetContext,
    SetContext(GeckoContextParameters),
    XblAnonymousChildren(WebElement),
    XblAnonymousByAttribute(WebElement, AttributeParameters),
    InstallAddon(AddonInstallParameters),
    UninstallAddon(AddonUninstallParameters)
}

impl WebDriverExtensionCommand for GeckoExtensionCommand {
    fn parameters_json(&self) -> Option<Json> {
        match self {
            &GeckoExtensionCommand::GetContext => None,
            &GeckoExtensionCommand::SetContext(ref x) => Some(x.to_json()),
            &GeckoExtensionCommand::XblAnonymousChildren(_) => None,
            &GeckoExtensionCommand::XblAnonymousByAttribute(_, ref x) => Some(x.to_json()),
            &GeckoExtensionCommand::InstallAddon(ref x) => Some(x.to_json()),
            &GeckoExtensionCommand::UninstallAddon(ref x) => Some(x.to_json()),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
enum GeckoContext {
    Content,
    Chrome,
}

impl ToJson for GeckoContext {
    fn to_json(&self) -> Json {
        match self {
            &GeckoContext::Content => Json::String("content".to_owned()),
            &GeckoContext::Chrome => Json::String("chrome".to_owned()),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct GeckoContextParameters {
    context: GeckoContext
}

impl Parameters for GeckoContextParameters {
    fn from_json(body: &Json) -> WebDriverResult<GeckoContextParameters> {
        let data = try!(body.as_object().ok_or(
            WebDriverError::new(ErrorStatus::InvalidArgument,
                                "Message body was not an object")));
        let context_value = try!(data.get("context").ok_or(
            WebDriverError::new(ErrorStatus::InvalidArgument,
                                "Missing context key")));
        let value = try!(context_value.as_string().ok_or(
            WebDriverError::new(
                ErrorStatus::InvalidArgument,
                "context was not a string")));
        let context = try!(match value {
            "chrome" => Ok(GeckoContext::Chrome),
            "content" => Ok(GeckoContext::Content),
            _ => Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                         format!("{} is not a valid context",
                                                 value)))
        });
        Ok(GeckoContextParameters {
            context: context
        })
    }
}

impl ToMarionette for GeckoContextParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        data.insert("value".to_owned(), self.context.to_json());
        Ok(data)
    }
}

impl ToJson for GeckoContextParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("context".to_owned(), self.context.to_json());
        Json::Object(data)
    }
}


#[derive(Clone, Debug, PartialEq)]
pub struct AttributeParameters {
    name: String,
    value: String
}

impl Parameters for AttributeParameters {
    fn from_json(body: &Json) -> WebDriverResult<AttributeParameters> {
        let data = try!(body.as_object().ok_or(
            WebDriverError::new(ErrorStatus::InvalidArgument,
                                "Message body was not an object")));
        let name = try!(try!(data.get("name").ok_or(
            WebDriverError::new(ErrorStatus::InvalidArgument,
                                "Missing 'name' parameter"))).as_string().
                            ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                      "'name' parameter is not a string")));
        let value = try!(try!(data.get("value").ok_or(
            WebDriverError::new(ErrorStatus::InvalidArgument,
                                "Missing 'value' parameter"))).as_string().
                            ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                      "'value' parameter is not a string")));
        Ok(AttributeParameters {
            name: name.to_owned(),
            value: value.to_owned(),
        })
    }
}

impl ToJson for AttributeParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("name".to_owned(), self.name.to_json());
        data.insert("value".to_owned(), self.value.to_json());
        Json::Object(data)
    }
}

impl ToMarionette for AttributeParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        data.insert("using".to_owned(), "anon attribute".to_json());
        let mut value = BTreeMap::new();
        value.insert(self.name.to_owned(), self.value.to_json());
        data.insert("value".to_owned(), Json::Object(value));
        Ok(data)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct AddonInstallParameters {
    pub path: String,
    pub temporary: bool
}

impl Parameters for AddonInstallParameters {
    fn from_json(body: &Json) -> WebDriverResult<AddonInstallParameters> {
        let data = try!(body.as_object().ok_or(
            WebDriverError::new(ErrorStatus::InvalidArgument,
                                "Message body was not an object")));

        let base64 = match data.get("addon") {
            Some(x) => {
                let s = try_opt!(x.as_string(),
                                 ErrorStatus::InvalidArgument,
                                 "'addon' is not a string").to_string();

                let addon_path = env::temp_dir().as_path()
                    .join(format!("addon-{}.xpi", Uuid::new_v4()));
                let mut addon_file = try!(File::create(&addon_path));
                let addon_buf = try!(s.from_base64());
                try!(addon_file.write(addon_buf.as_slice()));

                Some(try_opt!(addon_path.to_str(),
                              ErrorStatus::UnknownError,
                              "could not write addon to file").to_string())
            },
            None => None,
        };
        let path = match data.get("path") {
            Some(x) => Some(try_opt!(x.as_string(),
                                     ErrorStatus::InvalidArgument,
                                     "'path' is not a string").to_string()),
            None => None,
        };
        if (base64.is_none() && path.is_none()) || (base64.is_some() && path.is_some()) {
            return Err(WebDriverError::new(
                ErrorStatus::InvalidArgument,
                "Must specify exactly one of 'path' and 'addon'"));
        }

        let temporary = match data.get("temporary") {
            Some(x) => try_opt!(x.as_boolean(),
                                ErrorStatus::InvalidArgument,
                                "Failed to convert 'temporary' to boolean"),
            None => false
        };

        return Ok(AddonInstallParameters {
            path: base64.or(path).unwrap(),
            temporary: temporary,
        })
    }
}

impl ToJson for AddonInstallParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("path".to_string(), self.path.to_json());
        data.insert("temporary".to_string(), self.temporary.to_json());
        Json::Object(data)
    }
}

impl ToMarionette for AddonInstallParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        data.insert("path".to_string(), self.path.to_json());
        data.insert("temporary".to_string(), self.temporary.to_json());
        Ok(data)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct AddonUninstallParameters {
    pub id: String
}

impl Parameters for AddonUninstallParameters {
    fn from_json(body: &Json) -> WebDriverResult<AddonUninstallParameters> {
        let data = try!(body.as_object().ok_or(
            WebDriverError::new(ErrorStatus::InvalidArgument,
                                "Message body was not an object")));

        let id = try_opt!(
            try_opt!(data.get("id"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'id' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'id' is not a string").to_string();

        return Ok(AddonUninstallParameters {id: id})
    }
}

impl ToJson for AddonUninstallParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        Json::Object(data)
    }
}

impl ToMarionette for AddonUninstallParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        Ok(data)
    }
}

#[derive(Default)]
pub struct LogOptions {
    pub level: Option<logging::Level>,
}

#[derive(Default)]
pub struct MarionetteSettings {
    pub port: Option<u16>,
    pub binary: Option<PathBuf>,
    pub connect_existing: bool,

    /// Brings up the Browser Toolbox when starting Firefox,
    /// letting you debug internals.
    pub jsdebugger: bool,
}

pub struct MarionetteHandler {
    connection: Mutex<Option<MarionetteConnection>>,
    settings: MarionetteSettings,
    browser: Option<FirefoxProcess>,
}

impl MarionetteHandler {
    pub fn new(settings: MarionetteSettings) -> MarionetteHandler {
        MarionetteHandler {
            connection: Mutex::new(None),
            settings,
            browser: None,
        }
    }

    fn create_connection(&mut self,
                         session_id: &Option<String>,
                         new_session_parameters: &NewSessionParameters)
                         -> WebDriverResult<BTreeMap<String, Json>> {
        let (options, capabilities) = {
            let mut fx_capabilities = FirefoxCapabilities::new(self.settings.binary.as_ref());
            let mut capabilities = try!(
                try!(new_session_parameters
                    .match_browser(&mut fx_capabilities))
                    .ok_or(WebDriverError::new(
                        ErrorStatus::SessionNotCreated,
                        "Unable to find a matching set of capabilities")));

            let options = try!(FirefoxOptions::from_capabilities(fx_capabilities.chosen_binary,
                                                                 &mut capabilities));
            (options, capabilities)
        };

        if let Some(l) = options.log.level {
            logging::set_max_level(l);
        }

        let port = self.settings.port.unwrap_or(get_free_port()?);
        if !self.settings.connect_existing {
            try!(self.start_browser(port, options));
        }

        let mut connection = MarionetteConnection::new(port, session_id.clone());
        try!(connection.connect(&mut self.browser));
        self.connection = Mutex::new(Some(connection));

        Ok(capabilities)
    }

    fn start_browser(&mut self, port: u16, options: FirefoxOptions) -> WebDriverResult<()> {
        let binary = options.binary
            .ok_or(WebDriverError::new(ErrorStatus::SessionNotCreated,
                                       "Expected browser binary location, but unable to find \
                                        binary in default location, no \
                                        'moz:firefoxOptions.binary' capability provided, and \
                                        no binary flag set on the command line"))?;

        let is_custom_profile = options.profile.is_some();

        let mut profile = match options.profile {
            Some(x) => x,
            None => Profile::new(None)?
        };

        self.set_prefs(port, &mut profile, is_custom_profile, options.prefs)
            .map_err(|e| {
                WebDriverError::new(ErrorStatus::SessionNotCreated,
                                    format!("Failed to set preferences: {}", e))
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

        let browser_proc = runner.start()
            .map_err(|e| {
                WebDriverError::new(ErrorStatus::SessionNotCreated,
                                    format!("Failed to start browser {}: {}",
                                            binary.display(), e))
            })?;
        self.browser = Some(browser_proc);

        Ok(())
    }

    pub fn set_prefs(&self, port: u16, profile: &mut Profile, custom_profile: bool,
                     extra_prefs: Vec<(String, Pref)>)
                 -> WebDriverResult<()> {
        let prefs = try!(profile.user_prefs()
                         .map_err(|_| WebDriverError::new(ErrorStatus::UnknownError,
                                                          "Unable to read profile preferences file")));

        for &(ref name, ref value) in prefs::DEFAULT.iter() {
            if !custom_profile || !prefs.contains_key(name) {
                prefs.insert((*name).clone(), (*value).clone());
            }
        }

        prefs.insert_slice(&extra_prefs[..]);

        if self.settings.jsdebugger {
            prefs.insert("devtools.browsertoolbox.panel", Pref::new("jsdebugger".to_owned()));
            prefs.insert("devtools.debugger.remote-enabled", Pref::new(true));
            prefs.insert("devtools.chrome.enabled", Pref::new(true));
            prefs.insert("devtools.debugger.prompt-connection", Pref::new(false));
            prefs.insert("marionette.debugging.clicktostart", Pref::new(true));
        }

        prefs.insert("marionette.log.level", Pref::new(logging::max_level().to_string()));
        prefs.insert("marionette.port", Pref::new(port as i64));

        prefs.write().map_err(|_| WebDriverError::new(ErrorStatus::UnknownError,
                                                      "Unable to write Firefox profile"))
    }
}

impl WebDriverHandler<GeckoExtensionRoute> for MarionetteHandler {
    fn handle_command(&mut self, _: &Option<Session>,
                      msg: WebDriverMessage<GeckoExtensionRoute>) -> WebDriverResult<WebDriverResponse> {
        let mut resolved_capabilities = None;
        {
            let mut capabilities_options = None;
            // First handle the status message which doesn't actually require a marionette
            // connection or message
            if msg.command == Status {
                let (ready, message) = self.connection.lock()
                    .map(|ref connection| connection
                         .as_ref()
                         .map(|_| (false, "Session already started"))
                         .unwrap_or((true, "")))
                    .unwrap_or((false, "geckodriver internal error"));
                let mut value = BTreeMap::new();
                value.insert("ready".to_string(), Json::Boolean(ready));
                value.insert("message".to_string(), Json::String(message.into()));
                return Ok(WebDriverResponse::Generic(ValueResponse::new(Json::Object(value))));
            }
            match self.connection.lock() {
                Ok(ref connection) => {
                    if connection.is_none() {
                        match msg.command {
                            NewSession(ref capabilities) => {
                                capabilities_options = Some(capabilities);
                            },
                            _ => {
                                return Err(WebDriverError::new(
                                    ErrorStatus::SessionNotCreated,
                                    "Tried to run command without establishing a connection"));
                            }
                        }
                    }
                },
                Err(_) => {
                    return Err(WebDriverError::new(
                        ErrorStatus::UnknownError,
                        "Failed to aquire Marionette connection"))
                }
            }
            if let Some(capabilities) = capabilities_options {
                resolved_capabilities = Some(try!(
                    self.create_connection(&msg.session_id, &capabilities)));
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
                                err})
                    },
                    None => panic!("Connection missing")
                }
            },
            Err(_) => {
                Err(WebDriverError::new(
                    ErrorStatus::UnknownError,
                    "Failed to aquire Marionette connection"))
            }
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
    protocol: Option<String>,
    application_type: Option<String>,
    command_id: u64
}

impl MarionetteSession {
    pub fn new(session_id: Option<String>) -> MarionetteSession {
        let initital_id = session_id.unwrap_or("".to_string());
        MarionetteSession {
            session_id: initital_id,
            protocol: None,
            application_type: None,
            command_id: 0
        }
    }

    pub fn update(&mut self, msg: &WebDriverMessage<GeckoExtensionRoute>,
                  resp: &MarionetteResponse) -> WebDriverResult<()> {
        match msg.command {
            NewSession(_) => {
                let session_id = try_opt!(
                    try_opt!(resp.result.find("sessionId"),
                             ErrorStatus::SessionNotCreated,
                             "Unable to get session id").as_string(),
                        ErrorStatus::SessionNotCreated,
                        "Unable to convert session id to string");
                self.session_id = session_id.to_string().clone();
            },
            _ => {}
        }
        Ok(())
    }

    /// Converts a Marionette JSON response into a `WebElement`.
    ///
    /// Note that it currently coerces all chrome elements, web frames, and web
    /// windows also into web elements.  This will change at a later point.
    fn to_web_element(&self, json_data: &Json) -> WebDriverResult<WebElement> {
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
            value.as_string(),
            ErrorStatus::UnknownError,
            "Failed to convert web element reference value to string"
        ).to_string();
        Ok(WebElement::new(id))
    }

    pub fn next_command_id(&mut self) -> u64 {
        self.command_id = self.command_id + 1;
        self.command_id
    }

    pub fn response(&mut self, msg: &WebDriverMessage<GeckoExtensionRoute>,
                    resp: MarionetteResponse) -> WebDriverResult<WebDriverResponse> {

        if resp.id != self.command_id {
            return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                           format!("Marionette responses arrived out of sequence, expected {}, got {}",
                                                   self.command_id, resp.id)));
        }

        if let Some(error) = resp.error {
            return Err(error.into());
        }

        try!(self.update(msg, &resp));

        Ok(match msg.command {
            // Everything that doesn't have a response value
            Get(_) | GoBack | GoForward | Refresh | SetTimeouts(_) |
            SwitchToWindow(_) | SwitchToFrame(_) |
            SwitchToParentFrame | AddCookie(_) | DeleteCookies | DeleteCookie(_) |
            DismissAlert | AcceptAlert | SendAlertText(_) | ElementClick(_) |
            ElementTap(_) | ElementClear(_) | ElementSendKeys(_, _) |
            PerformActions(_) | ReleaseActions => {
                WebDriverResponse::Void
            },
            // Things that simply return the contents of the marionette "value" property
            GetCurrentUrl | GetTitle | GetPageSource | GetWindowHandle | IsDisplayed(_) |
            IsSelected(_) | GetElementAttribute(_, _) | GetElementProperty(_, _) |
            GetCSSValue(_, _) | GetElementText(_) |
            GetElementTagName(_) | IsEnabled(_) | ExecuteScript(_) | ExecuteAsyncScript(_) |
            GetAlertText | TakeScreenshot | TakeElementScreenshot(_) => {
                let value = try_opt!(resp.result.find("value"),
                                     ErrorStatus::UnknownError,
                                     "Failed to find value field");
                //TODO: Convert webelement keys
                WebDriverResponse::Generic(ValueResponse::new(value.clone()))
            },
            GetTimeouts => {
                let script = try_opt!(try_opt!(resp.result
                                                   .find("script"),
                                               ErrorStatus::UnknownError,
                                               "Missing field: script")
                                          .as_u64(),
                                      ErrorStatus::UnknownError,
                                      "Failed to interpret script timeout duration as u64");
                // Check for the spec-compliant "pageLoad", but also for "page load",
                // which was sent by Firefox 52 and earlier.
                let page_load = try_opt!(try_opt!(resp.result.find("pageLoad")
                                                      .or(resp.result.find("page load")),
                                                  ErrorStatus::UnknownError,
                                                  "Missing field: pageLoad")
                                             .as_u64(),
                                         ErrorStatus::UnknownError,
                                         "Failed to interpret page load duration as u64");
                let implicit = try_opt!(try_opt!(resp.result
                                                     .find("implicit"),
                                                 ErrorStatus::UnknownError,
                                                 "Missing field: implicit")
                                            .as_u64(),
                                        ErrorStatus::UnknownError,
                                        "Failed to interpret implicit search duration as u64");

                WebDriverResponse::Timeouts(TimeoutsResponse {
                    script: script,
                    pageLoad: page_load,
                    implicit: implicit,
                })
            },
            Status => panic!("Got status command that should already have been handled"),
            GetWindowHandles => {
                WebDriverResponse::Generic(ValueResponse::new(resp.result.clone()))
            },
            CloseWindow => {
                let data = try_opt!(resp.result.as_array(),
                                    ErrorStatus::UnknownError,
                                    "Failed to interpret value as array");
                let handles = try!(data.iter()
                                       .map(|x| {
                                                Ok(try_opt!(x.as_string(),
                                                            ErrorStatus::UnknownError,
                                                            "Failed to interpret window handle as string")
                                                           .to_owned())
                                            })
                                       .collect());
                WebDriverResponse::CloseWindow(CloseWindowResponse { window_handles: handles })
            },
            GetElementRect(_) => {
                let x = try_opt!(
                    try_opt!(resp.result.find("x"),
                             ErrorStatus::UnknownError,
                             "Failed to find x field").as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as float");

                let y = try_opt!(
                    try_opt!(resp.result.find("y"),
                             ErrorStatus::UnknownError,
                             "Failed to find y field").as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret y as float");

                let width = try_opt!(
                    try_opt!(resp.result.find("width"),
                             ErrorStatus::UnknownError,
                             "Failed to find width field").as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as float");

                let height = try_opt!(
                    try_opt!(resp.result.find("height"),
                             ErrorStatus::UnknownError,
                             "Failed to find height field").as_f64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as float");

                let rect = ElementRectResponse { x, y, width, height };
                WebDriverResponse::ElementRect(rect)
            },
            FullscreenWindow | MinimizeWindow | MaximizeWindow | GetWindowRect |
            SetWindowRect(_) => {
                let width = try_opt!(
                    try_opt!(resp.result.find("width"),
                             ErrorStatus::UnknownError,
                             "Failed to find width field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as positive integer");

                let height = try_opt!(
                    try_opt!(resp.result.find("height"),
                             ErrorStatus::UnknownError,
                             "Failed to find heigenht field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret height as positive integer");

                let x = try_opt!(
                    try_opt!(resp.result.find("x"),
                             ErrorStatus::UnknownError,
                             "Failed to find x field").as_i64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as integer");

                let y = try_opt!(
                    try_opt!(resp.result.find("y"),
                             ErrorStatus::UnknownError,
                             "Failed to find y field").as_i64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret y as integer");

                let rect = WindowRectResponse {
                    x: x as i32,
                    y: y as i32,
                    width: width as i32,
                    height: height as i32,
                };
                WebDriverResponse::WindowRect(rect)
            },
            GetCookies => {
                let cookies = try!(self.process_cookies(&resp.result));
                WebDriverResponse::Cookies(CookiesResponse { value: cookies })
            },
            GetNamedCookie(ref name) => {
                let mut cookies = try!(self.process_cookies(&resp.result));
                cookies.retain(|x| x.name == *name);
                let cookie = try_opt!(cookies.pop(),
                                      ErrorStatus::NoSuchCookie,
                                      format!("No cookie with name {}", name));
                WebDriverResponse::Cookie(CookieResponse { value: cookie })
            }
            FindElement(_) | FindElementElement(_, _) => {
                let element = try!(self.to_web_element(
                    try_opt!(resp.result.find("value"),
                             ErrorStatus::UnknownError,
                             "Failed to find value field")));
                WebDriverResponse::Generic(ValueResponse::new(element.to_json()))
            },
            FindElements(_) | FindElementElements(_, _) => {
                let element_vec = try_opt!(resp.result.as_array(),
                                           ErrorStatus::UnknownError,
                                           "Failed to interpret value as array");
                let elements = try!(element_vec.iter().map(
                    |x| {
                        self.to_web_element(x)
                    }).collect::<Result<Vec<_>, _>>());
                WebDriverResponse::Generic(ValueResponse::new(
                    Json::Array(elements.iter().map(|x| {x.to_json()}).collect())))
            },
            GetActiveElement => {
                let element = try!(self.to_web_element(
                    try_opt!(resp.result.find("value"),
                             ErrorStatus::UnknownError,
                             "Failed to find value field")));
                WebDriverResponse::Generic(ValueResponse::new(element.to_json()))
            },
            NewSession(_) => {
                let session_id = try_opt!(
                    try_opt!(resp.result.find("sessionId"),
                             ErrorStatus::InvalidSessionId,
                             "Failed to find sessionId field").as_string(),
                    ErrorStatus::InvalidSessionId,
                    "sessionId is not a string");

                let mut capabilities = try_opt!(
                    try_opt!(resp.result.find("capabilities"),
                             ErrorStatus::UnknownError,
                             "Failed to find capabilities field").as_object(),
                    ErrorStatus::UnknownError,
                    "capabilities field is not an object").clone();

                capabilities.insert("moz:geckodriverVersion".into(), BuildInfo.into());

                WebDriverResponse::NewSession(NewSessionResponse::new(
                    session_id.to_string(), Json::Object(capabilities)))
            },
            DeleteSession => {
                WebDriverResponse::DeleteSession
            },
            Extension(ref extension) => {
                match extension {
                    &GeckoExtensionCommand::GetContext => {
                        let value = try_opt!(resp.result.find("value"),
                                             ErrorStatus::UnknownError,
                                             "Failed to find value field");
                        WebDriverResponse::Generic(ValueResponse::new(value.clone()))
                    },
                    &GeckoExtensionCommand::SetContext(_) => WebDriverResponse::Void,
                    &GeckoExtensionCommand::XblAnonymousChildren(_) => {
                        let els_vec = try_opt!(resp.result.as_array(),
                            ErrorStatus::UnknownError, "Failed to interpret body as array");
                        let els = try!(els_vec.iter().map(|x| self.to_web_element(x))
                            .collect::<Result<Vec<_>, _>>());
                        WebDriverResponse::Generic(ValueResponse::new(
                            Json::Array(els.iter().map(|el| el.to_json()).collect())))
                    },
                    &GeckoExtensionCommand::XblAnonymousByAttribute(_, _) => {
                        let el = try!(self.to_web_element(try_opt!(resp.result.find("value"),
                            ErrorStatus::UnknownError, "Failed to find value field")));
                        WebDriverResponse::Generic(ValueResponse::new(el.to_json()))
                    },
                    &GeckoExtensionCommand::InstallAddon(_) => {
                        let value = try_opt!(resp.result.find("value"),
                                             ErrorStatus::UnknownError,
                                             "Failed to find value field");
                        WebDriverResponse::Generic(ValueResponse::new(value.clone()))
                    },
                    &GeckoExtensionCommand::UninstallAddon(_) => WebDriverResponse::Void
                }
            }
        })
    }

    fn process_cookies(&self, json_data: &Json) -> WebDriverResult<Vec<Cookie>> {
        let value = try_opt!(json_data.as_array(),
                             ErrorStatus::UnknownError,
                             "Failed to interpret value as array");
        value.iter().map(|x| {
            let name = try_opt!(
                try_opt!(x.find("name"),
                         ErrorStatus::UnknownError,
                         "Cookie must have a name field").as_string(),
                ErrorStatus::UnknownError,
                "Cookie must have string name").to_string();
            let value = try_opt!(
                try_opt!(x.find("value"),
                         ErrorStatus::UnknownError,
                         "Cookie must have a value field").as_string(),
                ErrorStatus::UnknownError,
                "Cookie must have a string value").to_string();
            let path = try!(
                Nullable::from_json(x.find("path").unwrap_or(&Json::Null),
                                    |x| {
                                        Ok((try_opt!(x.as_string(),
                                                     ErrorStatus::UnknownError,
                                                     "Cookie path must be string")).to_string())
                                    }));
            let domain = try!(
                Nullable::from_json(x.find("domain").unwrap_or(&Json::Null),
                                    |x| {
                                        Ok((try_opt!(x.as_string(),
                                                     ErrorStatus::UnknownError,
                                                     "Cookie domain must be string")).to_string())
                                    }));
            let expiry = try!(
                Nullable::from_json(x.find("expiry").unwrap_or(&Json::Null),
                                    |x| {
                                        Ok(Date::new(try_opt!(
                                            x.as_u64(),
                                            ErrorStatus::UnknownError,
                                            "Cookie expiry must be a positive integer")))
                                    }));
            let secure = try_opt!(
                x.find("secure").map_or(Some(false), |x| x.as_boolean()),
                ErrorStatus::UnknownError,
                "Cookie secure flag must be boolean");
            let http_only = try_opt!(
                x.find("httpOnly").map_or(Some(false), |x| x.as_boolean()),
                ErrorStatus::UnknownError,
                "Cookie httpOnly flag must be boolean");

            let new_cookie = Cookie {
                name: name,
                value: value,
                path: path,
                domain: domain,
                expiry: expiry,
                secure: secure,
                httpOnly: http_only,
            };
            Ok(new_cookie)
        }).collect::<Result<Vec<_>, _>>()
    }
}

pub struct MarionetteCommand {
    pub id: u64,
    pub name: String,
    pub params: BTreeMap<String, Json>
}

impl MarionetteCommand {
    fn new(id: u64, name: String, params: BTreeMap<String, Json>) -> MarionetteCommand {
        MarionetteCommand {
            id: id,
            name: name,
            params: params,
        }
    }

    fn from_webdriver_message(id: u64,
                              capabilities: Option<BTreeMap<String, Json>>,
                              msg: &WebDriverMessage<GeckoExtensionRoute>)
                              -> WebDriverResult<MarionetteCommand> {
        let (opt_name, opt_parameters) = match msg.command {
            Status => panic!("Got status command that should already have been handled"),
            AcceptAlert => {
                // Needs to be updated to "WebDriver:AcceptAlert" for Firefox 63
                (Some("WebDriver:AcceptDialog"), None)
            }
            AddCookie(ref x) => (Some("WebDriver:AddCookie"), Some(x.to_marionette())),
            CloseWindow => (Some("WebDriver:CloseWindow"), None),
            DeleteCookie(ref x) => {
                let mut data = BTreeMap::new();
                data.insert("name".to_string(), x.to_json());
                (Some("WebDriver:DeleteCookie"), Some(Ok(data)))
            }
            DeleteCookies => (Some("WebDriver:DeleteAllCookies"), None),
            DeleteSession => {
                let mut body = BTreeMap::new();
                body.insert("flags".to_owned(), vec!["eForceQuit".to_json()].to_json());
                (Some("Marionette:Quit"), Some(Ok(body)))
            }
            DismissAlert => (Some("WebDriver:DismissAlert"), None),
            ElementClear(ref x) => (Some("WebDriver:ElementClear"), Some(x.to_marionette())),
            ElementClick(ref x) => (Some("WebDriver:ElementClick"), Some(x.to_marionette())),
            ElementSendKeys(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("text".to_string(), x.text.to_json());
                data.insert(
                    "value".to_string(),
                    x.text
                        .chars()
                        .map(|x| x.to_string())
                        .collect::<Vec<String>>()
                        .to_json(),
                );
                (Some("WebDriver:ElementSendKeys"), Some(Ok(data)))
            }
            ElementTap(ref x) => (Some("singleTap"), Some(x.to_marionette())),
            ExecuteAsyncScript(ref x) => (
                Some("WebDriver:ExecuteAsyncScript"),
                Some(x.to_marionette()),
            ),
            ExecuteScript(ref x) => (Some("WebDriver:ExecuteScript"), Some(x.to_marionette())),
            FindElement(ref x) => (Some("WebDriver:FindElement"), Some(x.to_marionette())),
            FindElementElement(ref e, ref x) => {
                let mut data = try!(x.to_marionette());
                data.insert("element".to_string(), e.id.to_json());
                (Some("WebDriver:FindElement"), Some(Ok(data)))
            }
            FindElements(ref x) => (Some("WebDriver:FindElements"), Some(x.to_marionette())),
            FindElementElements(ref e, ref x) => {
                let mut data = try!(x.to_marionette());
                data.insert("element".to_string(), e.id.to_json());
                (Some("WebDriver:FindElements"), Some(Ok(data)))
            }
            FullscreenWindow => (Some("WebDriver:FullscreenWindow"), None),
            Get(ref x) => (Some("WebDriver:Navigate"), Some(x.to_marionette())),
            GetAlertText => (Some("WebDriver:GetAlertText"), None),
            GetActiveElement => (Some("WebDriver:GetActiveElement"), None),
            GetCookies | GetNamedCookie(_) => (Some("WebDriver:GetCookies"), None),
            GetCurrentUrl => (Some("WebDriver:GetCurrentURL"), None),
            GetCSSValue(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("propertyName".to_string(), x.to_json());
                (Some("WebDriver:GetElementCSSValue"), Some(Ok(data)))
            }
            GetElementAttribute(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("name".to_string(), x.to_json());
                (Some("WebDriver:GetElementAttribute"), Some(Ok(data)))
            }
            GetElementProperty(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("name".to_string(), x.to_json());
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

                let mut data = BTreeMap::new();
                for (k, v) in caps.iter() {
                    data.insert(k.to_string(), v.to_json());
                }

                // duplicate in capabilities.desiredCapabilities for legacy compat
                let mut legacy_caps = BTreeMap::new();
                legacy_caps.insert("desiredCapabilities".to_string(), caps.to_json());
                data.insert("capabilities".to_string(), legacy_caps.to_json());

                (Some("WebDriver:NewSession"), Some(Ok(data)))
            }
            PerformActions(ref x) => (Some("WebDriver:PerformActions"), Some(x.to_marionette())),
            Refresh => (Some("WebDriver:Refresh"), None),
            ReleaseActions => (Some("WebDriver:ReleaseActions"), None),
            SendAlertText(ref x) => {
                let mut data = BTreeMap::new();
                data.insert("text".to_string(), x.text.to_json());
                data.insert(
                    "value".to_string(),
                    x.text
                        .chars()
                        .map(|x| x.to_string())
                        .collect::<Vec<String>>()
                        .to_json(),
                );
                (Some("WebDriver:SendAlertText"), Some(Ok(data)))
            }
            SetTimeouts(ref x) => (Some("WebDriver:SetTimeouts"), Some(x.to_marionette())),
            SetWindowRect(ref x) => (Some("WebDriver:SetWindowRect"), Some(x.to_marionette())),
            SwitchToFrame(ref x) => (Some("WebDriver:SwitchToFrame"), Some(x.to_marionette())),
            SwitchToParentFrame => (Some("WebDriver:SwitchToParentFrame"), None),
            SwitchToWindow(ref x) => (Some("WebDriver:SwitchToWindow"), Some(x.to_marionette())),
            TakeElementScreenshot(ref e) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("highlights".to_string(), Json::Array(vec![]));
                data.insert("full".to_string(), Json::Boolean(false));
                (Some("WebDriver:TakeScreenshot"), Some(Ok(data)))
            }
            TakeScreenshot => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), Json::Null);
                data.insert("highlights".to_string(), Json::Array(vec![]));
                data.insert("full".to_string(), Json::Boolean(false));
                (Some("WebDriver:TakeScreenshot"), Some(Ok(data)))
            }
            Extension(ref extension) => match extension {
                &GeckoExtensionCommand::GetContext => (Some("Marionette:GetContext"), None),
                &GeckoExtensionCommand::InstallAddon(ref x) => {
                    (Some("Addon:Install"), Some(x.to_marionette()))
                }
                &GeckoExtensionCommand::SetContext(ref x) => {
                    (Some("Marionette:SetContext"), Some(x.to_marionette()))
                }
                &GeckoExtensionCommand::UninstallAddon(ref x) => {
                    (Some("Addon:Uninstall"), Some(x.to_marionette()))
                }
                &GeckoExtensionCommand::XblAnonymousByAttribute(ref e, ref x) => {
                    let mut data = try!(x.to_marionette());
                    data.insert("element".to_string(), e.id.to_json());
                    (Some("WebDriver:FindElement"), Some(Ok(data)))
                }
                &GeckoExtensionCommand::XblAnonymousChildren(ref e) => {
                    let mut data = BTreeMap::new();
                    data.insert("using".to_owned(), "anon".to_json());
                    data.insert("value".to_owned(), Json::Null);
                    data.insert("element".to_string(), e.id.to_json());
                    (Some("WebDriver:FindElements"), Some(Ok(data)))
                }
            },
        };

        let name = try_opt!(opt_name,
                            ErrorStatus::UnsupportedOperation,
                            "Operation not supported");
        let parameters = try!(opt_parameters.unwrap_or(Ok(BTreeMap::new())));

        Ok(MarionetteCommand::new(id, name.into(), parameters))
    }
}

impl ToJson for MarionetteCommand {
    fn to_json(&self) -> Json {
        Json::Array(vec![Json::U64(0), self.id.to_json(), self.name.to_json(),
                         self.params.to_json()])
    }
}

pub struct MarionetteResponse {
    pub id: u64,
    pub error: Option<MarionetteError>,
    pub result: Json,
}

impl MarionetteResponse {
    fn from_json(data: &Json) -> WebDriverResult<MarionetteResponse> {
        let data_array = try_opt!(data.as_array(),
                                  ErrorStatus::UnknownError,
                                  "Expected a json array");

        if data_array.len() != 4 {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                "Expected an array of length 4"));
        }

        if data_array[0].as_u64() != Some(1) {
            return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                           "Expected 1 in first element of response"));
        };
        let id = try_opt!(data[1].as_u64(),
                          ErrorStatus::UnknownError,
                          "Expected an integer id");
        let error = if data[2].is_object() {
            Some(try!(MarionetteError::from_json(&data[2])))
        } else if data[2].is_null() {
            None
        } else {
            return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                           "Expected object or null error"));
        };

        let result = if data[3].is_null() || data[3].is_object() || data[3].is_array() {
            data[3].clone()
        } else {
            return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                           "Expected object params"));
        };

        Ok(MarionetteResponse {id: id,
                               error: error,
                               result: result})
    }
}

impl ToJson for MarionetteResponse {
    fn to_json(&self) -> Json {
        Json::Array(vec![Json::U64(1), self.id.to_json(), self.error.to_json(),
                         self.result.clone()])
    }
}

#[derive(RustcEncodable, RustcDecodable)]
pub struct MarionetteError {
    pub code: String,
    pub message: String,
    pub stacktrace: Option<String>
}

impl MarionetteError {
    fn from_json(data: &Json) -> WebDriverResult<MarionetteError> {
        if !data.is_object() {
            return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                           "Expected an error object"));
        }

        let code = try_opt!(
            try_opt!(data.find("error"),
                     ErrorStatus::UnknownError,
                     "Error value has no error code").as_string(),
            ErrorStatus::UnknownError,
            "Error status was not a string").into();
        let message = try_opt!(
            try_opt!(data.find("message"),
                     ErrorStatus::UnknownError,
                     "Error value has no message").as_string(),
            ErrorStatus::UnknownError,
            "Error message was not a string").into();
        let stacktrace = match data.find("stacktrace") {
            None | Some(&Json::Null) => None,
            Some(x) => Some(try_opt!(x.as_string(),
                                     ErrorStatus::UnknownError,
                                     "Error message was not a string").into()),
        };

        Ok(MarionetteError { code, message, stacktrace })
    }
}

impl ToJson for MarionetteError {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("error".into(), self.code.to_json());
        data.insert("message".into(), self.message.to_json());
        data.insert("stacktrace".into(), self.stacktrace.to_json());
        Json::Object(data)
    }
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

fn get_free_port() -> IoResult<u16> {
    TcpListener::bind((DEFAULT_HOST, 0))
        .and_then(|stream| stream.local_addr())
        .map(|x| x.port())
}

pub struct MarionetteConnection {
    port: u16,
    stream: Option<TcpStream>,
    pub session: MarionetteSession
}

impl MarionetteConnection {
    pub fn new(port: u16, session_id: Option<String>) -> MarionetteConnection {
        MarionetteConnection {
            port: port,
            stream: None,
            session: MarionetteSession::new(session_id),
        }
    }

    pub fn connect(&mut self, browser: &mut Option<FirefoxProcess>) -> WebDriverResult<()> {
        let timeout = time::Duration::from_secs(60);
        let poll_interval = time::Duration::from_millis(100);
        let now = time::Instant::now();

        debug!(
            "Waiting {}s to connect to browser on {}:{}",
            timeout.as_secs(),
            DEFAULT_HOST,
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

            match TcpStream::connect(&(DEFAULT_HOST, self.port)) {
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

        debug!("Connected to Marionette on {}:{}", DEFAULT_HOST, self.port);
        self.handshake()
    }

    fn handshake(&mut self) -> WebDriverResult<()> {
        let resp = try!(self.read_resp());
        let handshake_data = try!(Json::from_str(&*resp));

        let data = try_opt!(handshake_data.as_object(),
                            ErrorStatus::UnknownError,
                            "Expected a json object in handshake");

        self.session.protocol = Some(try_opt!(data.get("marionetteProtocol"),
                                              ErrorStatus::UnknownError,
                                              "Missing 'marionetteProtocol' field in handshake").to_string());

        self.session.application_type = Some(try_opt!(data.get("applicationType"),
                                              ErrorStatus::UnknownError,
                                              "Missing 'applicationType' field in handshake").to_string());

        if self.session.protocol != Some("3".into()) {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                format!("Unsupported Marionette protocol version {}, required 3",
                        self.session.protocol.as_ref().unwrap_or(&"<unknown>".into()))));
        }

        Ok(())
    }

    pub fn close(&self) {
    }

    fn encode_msg(&self, msg:Json) -> String {
        let data = json::encode(&msg).unwrap();
        format!("{}:{}", data.len(), data)
    }

    pub fn send_command(&mut self,
                        capabilities: Option<BTreeMap<String, Json>>,
                        msg: &WebDriverMessage<GeckoExtensionRoute>)
                        -> WebDriverResult<WebDriverResponse> {
        let id = self.session.next_command_id();
        let command = try!(MarionetteCommand::from_webdriver_message(id, capabilities, msg));

        let resp_data = try!(self.send(command.to_json()));
        let json_data: Json = try!(Json::from_str(&*resp_data));

        self.session.response(msg, try!(MarionetteResponse::from_json(&json_data)))
    }

    fn send(&mut self, msg: Json) -> WebDriverResult<String> {
        let data = self.encode_msg(msg);

        match self.stream {
            Some(ref mut stream) => {
                if stream.write(&*data.as_bytes()).is_err() {
                    let mut err = WebDriverError::new(ErrorStatus::UnknownError,
                                                      "Failed to write response to stream");
                    err.delete_session = true;
                    return Err(err);
                }
            }
            None => {
                let mut err = WebDriverError::new(ErrorStatus::UnknownError,
                                                  "Tried to write before opening stream");
                err.delete_session = true;
                return Err(err);
            }
        }
        match self.read_resp() {
            Ok(resp) => Ok(resp),
            Err(_) => {
                let mut err = WebDriverError::new(ErrorStatus::UnknownError,
                                                  "Failed to decode response from marionette");
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
            let num_read = try!(stream.read(buf));
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
            let num_read = try!(stream.read(buf));
            if num_read == 0 {
                return Err(IoError::new(ErrorKind::Other,
                                        "EOF reading marionette message"))
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
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>>;
}

impl ToMarionette for GetParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(), ErrorStatus::UnknownError, "Expected an object").clone())
    }
}

impl ToMarionette for TimeoutsParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(), ErrorStatus::UnknownError, "Expected an object").clone())
    }
}

impl ToMarionette for WindowRectParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(), ErrorStatus::UnknownError, "Expected an object").clone())
    }
}

impl ToMarionette for SwitchToWindowParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        data.insert("name".to_string(), self.handle.to_json());
        Ok(data)
    }
}

impl ToMarionette for LocatorParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(),
                    ErrorStatus::UnknownError,
                    "Expected an object")
            .clone())
    }
}

impl ToMarionette for SwitchToFrameParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        let key = match self.id {
            FrameId::Null => None,
            FrameId::Short(_) => Some("id"),
            FrameId::Element(_) => Some("element"),
        };
        if let Some(x) = key {
            data.insert(x.to_string(), self.id.to_json());
        }
        Ok(data)
    }
}

impl ToMarionette for JavascriptCommandParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = self.to_json().as_object().unwrap().clone();
        data.insert("newSandbox".to_string(), false.to_json());
        data.insert("specialPowers".to_string(), false.to_json());
        data.insert("scriptTimeout".to_string(), Json::Null);
        Ok(data)
    }
}

impl ToMarionette for ActionsParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(),
                    ErrorStatus::UnknownError,
                    "Expected an object")
            .clone())
    }
}

impl ToMarionette for GetNamedCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(),
                    ErrorStatus::UnknownError,
                    "Expected an object")
            .clone())
    }
}

impl ToMarionette for AddCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut cookie = BTreeMap::new();
        cookie.insert("name".to_string(), self.name.to_json());
        cookie.insert("value".to_string(), self.value.to_json());
        if self.path.is_value() {
            cookie.insert("path".to_string(), self.path.to_json());
        }
        if self.domain.is_value() {
            cookie.insert("domain".to_string(), self.domain.to_json());
        }
        if self.expiry.is_value() {
            cookie.insert("expiry".to_string(), self.expiry.to_json());
        }
        cookie.insert("secure".to_string(), self.secure.to_json());
        cookie.insert("httpOnly".to_string(), self.httpOnly.to_json());
        let mut data = BTreeMap::new();
        data.insert("cookie".to_string(), Json::Object(cookie));
        Ok(data)
    }
}

impl ToMarionette for TakeScreenshotParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        let element = match self.element {
            Nullable::Null => Json::Null,
            Nullable::Value(ref x) => Json::Object(try!(x.to_marionette()))
        };
        data.insert("element".to_string(), element);
        Ok(data)
    }
}

impl ToMarionette for WebElement {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        Ok(data)
    }
}

impl<T: ToJson> ToMarionette for Nullable<T> {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        //Note this is a terrible hack. We don't want Nullable<T: ToJson+ToMarionette>
        //so in cases where ToJson != ToMarionette you have to deal with the Nullable
        //explicitly. This kind of suggests that the whole design is wrong.
        Ok(try_opt!(self.to_json().as_object(), ErrorStatus::UnknownError, "Expected an object").clone())
    }
}

impl ToMarionette for FrameId {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        match *self {
            FrameId::Short(x) => data.insert("id".to_string(), x.to_json()),
            FrameId::Element(ref x) => data.insert("element".to_string(),
                                                   Json::Object(try!(x.to_marionette()))),
            FrameId::Null => None
        };
        Ok(data)
    }
}

#[cfg(test)]
mod tests {
    use marionette::{AddonInstallParameters, Parameters};
    use rustc_serialize::json::Json;
    use std::io::Read;
    use std::fs::File;
    use webdriver::error::WebDriverResult;

    #[test]
    fn test_addon_install_params_missing_path() {
        let json_data: Json = Json::from_str(r#"{"temporary": true}"#).unwrap();
        let res: WebDriverResult<AddonInstallParameters> = Parameters::from_json(&json_data);
        assert!(res.is_err());
    }

    #[test]
    fn test_addon_install_params_with_both_path_and_base64() {
        let json_data: Json = Json::from_str(
            r#"{"path": "/path/to.xpi", "addon": "aGVsbG8=", "temporary": true}"#).unwrap();
        let res: WebDriverResult<AddonInstallParameters> = Parameters::from_json(&json_data);
        assert!(res.is_err());
    }

    #[test]
    fn test_addon_install_params_with_path() {
        let json_data: Json = Json::from_str(
            r#"{"path": "/path/to.xpi", "temporary": true}"#).unwrap();
        let parameters: AddonInstallParameters = Parameters::from_json(&json_data).unwrap();
        assert_eq!(parameters.path, "/path/to.xpi");
        assert_eq!(parameters.temporary, true);
    }

    #[test]
    fn test_addon_install_params_with_base64() {
        let json_data: Json = Json::from_str(
            r#"{"addon": "aGVsbG8=", "temporary": true}"#).unwrap();
        let parameters: AddonInstallParameters = Parameters::from_json(&json_data).unwrap();

        assert_eq!(parameters.temporary, true);
        let mut file = File::open(parameters.path).unwrap();
        let mut contents = String::new();
        file.read_to_string(&mut contents).unwrap();
        assert_eq!("hello", contents);
    }
}
