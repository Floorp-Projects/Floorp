use hyper::method::Method;
use logging;
use logging::LogLevel;
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use mozrunner::runner::{Runner, FirefoxRunner};
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
use std::thread::sleep;
use std::time::Duration;
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
                          NewSessionResponse, RectResponse, TimeoutsResponse, ValueResponse,
                          WebDriverResponse};
use webdriver::common::{Date, ELEMENT_KEY, FrameId, Nullable, WebElement};
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};
use webdriver::server::{WebDriverHandler, Session};
use webdriver::httpapi::{WebDriverExtensionRoute};

use capabilities::{FirefoxCapabilities, FirefoxOptions};
use prefs;

const DEFAULT_HOST: &'static str = "localhost";

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
    pub level: Option<LogLevel>,
}

#[derive(Default)]
pub struct MarionetteSettings {
    pub port: Option<u16>,
    pub binary: Option<PathBuf>,
    pub connect_existing: bool,

    /// Optionally increase Marionette's verbosity by providing a log
    /// level. The Gecko default is LogLevel::Info for optimised
    /// builds and LogLevel::Debug for debug builds.
    pub log_level: Option<LogLevel>,
}

pub struct MarionetteHandler {
    connection: Mutex<Option<MarionetteConnection>>,
    settings: MarionetteSettings,
    browser: Option<FirefoxRunner>,
    current_log_level: Option<LogLevel>,
}

impl MarionetteHandler {
    pub fn new(settings: MarionetteSettings) -> MarionetteHandler {
        MarionetteHandler {
            connection: Mutex::new(None),
            settings: settings,
            browser: None,
            current_log_level: None,
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

        self.current_log_level = options.log.level.clone().or(self.settings.log_level.clone());
        logging::init(&self.current_log_level);

        let port = self.settings.port.unwrap_or(try!(get_free_port()));
        if !self.settings.connect_existing {
            try!(self.start_browser(port, options));
        }

        let mut connection = MarionetteConnection::new(port, session_id.clone());
        try!(connection.connect());
        self.connection = Mutex::new(Some(connection));

        Ok(capabilities)
    }

    fn start_browser(&mut self, port: u16, mut options: FirefoxOptions) -> WebDriverResult<()> {
        let binary = try!(options.binary
            .ok_or(WebDriverError::new(ErrorStatus::SessionNotCreated,
                                       "Expected browser binary location, but unable to find \
                                        binary in default location, no \
                                        'moz:firefoxOptions.binary' capability provided, and \
                                        no binary flag set on the command line")));

        let custom_profile = options.profile.is_some();

        let mut runner = try!(FirefoxRunner::new(&binary, options.profile.take())
                              .map_err(|e| WebDriverError::new(ErrorStatus::SessionNotCreated,
                                                               e.description().to_owned())));

        // double-dashed flags are not accepted on Windows systems
        runner.args().push("-marionette".to_owned());

        if let Some(args) = options.args.take() {
            runner.args().extend(args);
        };

        try!(self.set_prefs(port, &mut runner.profile, custom_profile, options.prefs)
            .map_err(|e| {
                WebDriverError::new(ErrorStatus::SessionNotCreated,
                                    format!("Failed to set preferences: {}", e))
            }));

        info!("Starting browser {} with args {:?}", binary.display(), runner.args());
        try!(runner.start()
            .map_err(|e| {
                WebDriverError::new(ErrorStatus::SessionNotCreated,
                                    format!("Failed to start browser {}: {}",
                                            binary.display(), e))
            }));
        self.browser = Some(runner);

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

        // fallbacks can be removed when Firefox 54 becomes stable
        if let Some(ref level) = self.current_log_level {
            prefs.insert("marionette.log.level", Pref::new(level.to_string()));
            prefs.insert("marionette.logging", Pref::new(level.to_string()));  // fallback
        };
        prefs.insert("marionette.port", Pref::new(port as i64));
        prefs.insert("marionette.defaultPrefs.port", Pref::new(port as i64));  // fallback

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
                    Some(conn) => conn.send_command(resolved_capabilities, &msg),
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

    fn delete_session(&mut self, _: &Option<Session>) {
        if let Ok(connection) = self.connection.lock() {
            if let Some(ref conn) = *connection {
                conn.close();
            }
        }
        if let Some(ref mut runner) = self.browser {
            debug!("Stopping browser process");
            if runner.stop().is_err() {
                error!("Failed to kill browser process");
            };
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

    fn to_web_element(&self, json_data: &Json) -> WebDriverResult<WebElement> {
        let data = try_opt!(json_data.as_object(),
                            ErrorStatus::UnknownError,
                            "Failed to convert data to an object");
        let id = try_opt!(
            try_opt!(
                match data.get("ELEMENT") {
                    Some(id) => Some(id),
                    None => {
                        match data.get(ELEMENT_KEY) {
                            Some(id) => Some(id),
                            None => None
                        }
                    }
                },
                ErrorStatus::UnknownError,
                "Failed to extract Web Element from response").as_string(),
            ErrorStatus::UnknownError,
            "Failed to convert id value to string"
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
            let status = self.error_from_string(&error.status);

            return Err(WebDriverError::new(status, error.message));
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

                WebDriverResponse::ElementRect(RectResponse::new(x, y, width, height))
            },
            FullscreenWindow | MinimizeWindow | MaximizeWindow | GetWindowRect |
            SetWindowRect(_) => {
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
                    "Failed to interpret height as float");

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

                WebDriverResponse::WindowRect(RectResponse::new(x, y, width, height))
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
                let mut session_id = try_opt!(
                    try_opt!(resp.result.find("sessionId"),
                             ErrorStatus::InvalidSessionId,
                             "Failed to find sessionId field").as_string(),
                    ErrorStatus::InvalidSessionId,
                    "sessionId was not a string");

                if session_id.starts_with("{") && session_id.ends_with("}") {
                    session_id = &session_id[1..session_id.len()-1];
                }

                let capabilities = try_opt!(
                    try_opt!(resp.result.find("capabilities"),
                             ErrorStatus::UnknownError,
                             "Failed to find capabilities field").as_object(),
                    ErrorStatus::UnknownError,
                    "capabiltites field was not an Object");

                WebDriverResponse::NewSession(NewSessionResponse::new(
                    session_id.to_string(), Json::Object(capabilities.clone())))
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
                                        Ok(Date::new((try_opt!(
                                            x.as_u64(),
                                            ErrorStatus::UnknownError,
                                            "Cookie expiry must be a positive integer"))))
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

    pub fn error_from_string(&self, error_code: &str) -> ErrorStatus {
        match error_code {
            "element click intercepted" => ErrorStatus::ElementClickIntercepted,
            "element not interactable" | "element not visible" => ErrorStatus::ElementNotInteractable,
            "element not selectable" => ErrorStatus::ElementNotSelectable,
            "insecure certificate" => ErrorStatus::InsecureCertificate,
            "invalid argument" => ErrorStatus::InvalidArgument,
            "invalid cookie domain" => ErrorStatus::InvalidCookieDomain,
            "invalid coordinates" | "invalid element coordinates" => ErrorStatus::InvalidCoordinates,
            "invalid element state" => ErrorStatus::InvalidElementState,
            "invalid selector" => ErrorStatus::InvalidSelector,
            "invalid session id" => ErrorStatus::InvalidSessionId,
            "javascript error" => ErrorStatus::JavascriptError,
            "move target out of bounds" => ErrorStatus::MoveTargetOutOfBounds,
            "no such alert" => ErrorStatus::NoSuchAlert,
            "no such element" => ErrorStatus::NoSuchElement,
            "no such frame" => ErrorStatus::NoSuchFrame,
            "no such window" => ErrorStatus::NoSuchWindow,
            "script timeout" => ErrorStatus::ScriptTimeout,
            "session not created" => ErrorStatus::SessionNotCreated,
            "stale element reference" => ErrorStatus::StaleElementReference,
            "timeout" => ErrorStatus::Timeout,
            "unable to capture screen" => ErrorStatus::UnableToCaptureScreen,
            "unable to set cookie" => ErrorStatus::UnableToSetCookie,
            "unexpected alert open" => ErrorStatus::UnexpectedAlertOpen,
            "unknown command" => ErrorStatus::UnknownCommand,
            "unknown error" => ErrorStatus::UnknownError,
            "unsupported operation" => ErrorStatus::UnsupportedOperation,
            _ => ErrorStatus::UnknownError,
        }
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
            NewSession(_) => {
                let caps = capabilities.expect("Tried to create new session without processing capabilities");

                let mut data = BTreeMap::new();
                for (k, v) in caps.iter() {
                    data.insert(k.to_string(), v.to_json());
                }

                // duplicate in capabilities.desiredCapabilities for legacy compat
                let mut legacy_caps = BTreeMap::new();
                legacy_caps.insert("desiredCapabilities".to_string(), caps.to_json());
                data.insert("capabilities".to_string(), legacy_caps.to_json());

                (Some("newSession"), Some(Ok(data)))
            },
            DeleteSession => {
                let mut body = BTreeMap::new();
                body.insert("flags".to_owned(), vec!["eForceQuit".to_json()].to_json());
                (Some("quit"), Some(Ok(body)))
            },
            Status => panic!("Got status command that should already have been handled"),
            Get(ref x) => (Some("get"), Some(x.to_marionette())),
            GetCurrentUrl => (Some("getCurrentUrl"), None),
            GoBack => (Some("goBack"), None),
            GoForward => (Some("goForward"), None),
            Refresh => (Some("refresh"), None),
            GetTitle => (Some("getTitle"), None),
            GetPageSource => (Some("getPageSource"), None),
            GetWindowHandle => (Some("getWindowHandle"), None),
            GetWindowHandles => (Some("getWindowHandles"), None),
            CloseWindow => (Some("close"), None),
            GetTimeouts => (Some("getTimeouts"), None),
            SetTimeouts(ref x) => (Some("setTimeouts"), Some(x.to_marionette())),
            SetWindowRect(ref x) => (Some("setWindowRect"), Some(x.to_marionette())),
            GetWindowRect => (Some("getWindowRect"), None),
            MinimizeWindow => (Some("WebDriver:MinimizeWindow"), None),
            MaximizeWindow => (Some("maximizeWindow"), None),
            FullscreenWindow => (Some("fullscreenWindow"), None),
            SwitchToWindow(ref x) => (Some("switchToWindow"), Some(x.to_marionette())),
            SwitchToFrame(ref x) => (Some("switchToFrame"), Some(x.to_marionette())),
            SwitchToParentFrame => (Some("switchToParentFrame"), None),
            FindElement(ref x) => (Some("findElement"), Some(x.to_marionette())),
            FindElements(ref x) => (Some("findElements"), Some(x.to_marionette())),
            FindElementElement(ref e, ref x) => {
                let mut data = try!(x.to_marionette());
                data.insert("element".to_string(), e.id.to_json());
                (Some("findElement"), Some(Ok(data)))
            },
            FindElementElements(ref e, ref x) => {
                let mut data = try!(x.to_marionette());
                data.insert("element".to_string(), e.id.to_json());
                (Some("findElements"), Some(Ok(data)))
            },
            GetActiveElement => (Some("getActiveElement"), None),
            IsDisplayed(ref x) => (Some("isElementDisplayed"), Some(x.to_marionette())),
            IsSelected(ref x) => (Some("isElementSelected"), Some(x.to_marionette())),
            GetElementAttribute(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("name".to_string(), x.to_json());
                (Some("getElementAttribute"), Some(Ok(data)))
            },
            GetElementProperty(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("name".to_string(), x.to_json());
                (Some("getElementProperty"), Some(Ok(data)))
            },
            GetCSSValue(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("propertyName".to_string(), x.to_json());
                (Some("getElementValueOfCssProperty"), Some(Ok(data)))
            },
            GetElementText(ref x) => (Some("getElementText"), Some(x.to_marionette())),
            GetElementTagName(ref x) => (Some("getElementTagName"), Some(x.to_marionette())),
            GetElementRect(ref x) => (Some("getElementRect"), Some(x.to_marionette())),
            IsEnabled(ref x) => (Some("isElementEnabled"), Some(x.to_marionette())),
            PerformActions(ref x) => (Some("performActions"), Some(x.to_marionette())),
            ReleaseActions => (Some("releaseActions"), None),
            ElementClick(ref x) => (Some("clickElement"), Some(x.to_marionette())),
            ElementTap(ref x) => (Some("singleTap"), Some(x.to_marionette())),
            ElementClear(ref x) => (Some("clearElement"), Some(x.to_marionette())),
            ElementSendKeys(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("text".to_string(), x.text.to_json());
                data.insert("value".to_string(),
                            x.text
                                .chars()
                                .map(|x| x.to_string())
                                .collect::<Vec<String>>()
                                .to_json());
                (Some("sendKeysToElement"), Some(Ok(data)))
            },
            ExecuteScript(ref x) => (Some("executeScript"), Some(x.to_marionette())),
            ExecuteAsyncScript(ref x) => (Some("executeAsyncScript"), Some(x.to_marionette())),
            GetCookies | GetNamedCookie(_) => (Some("getCookies"), None),
            DeleteCookies => (Some("deleteAllCookies"), None),
            DeleteCookie(ref x) => {
                let mut data = BTreeMap::new();
                data.insert("name".to_string(), x.to_json());
                (Some("deleteCookie"), Some(Ok(data)))
            },
            AddCookie(ref x) => (Some("addCookie"), Some(x.to_marionette())),
            DismissAlert => (Some("dismissDialog"), None),
            AcceptAlert => (Some("acceptDialog"), None),
            GetAlertText => (Some("getTextFromDialog"), None),
            SendAlertText(ref x) => {
                let mut data = BTreeMap::new();
                data.insert("text".to_string(), x.text.to_json());
                data.insert("value".to_string(),
                            x.text
                                .chars()
                                .map(|x| x.to_string())
                                .collect::<Vec<String>>()
                                .to_json());
                (Some("sendKeysToDialog"), Some(Ok(data)))
            },
            TakeScreenshot => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), Json::Null);
                data.insert("highlights".to_string(), Json::Array(vec![]));
                data.insert("full".to_string(), Json::Boolean(false));
                (Some("takeScreenshot"), Some(Ok(data)))
            },
            TakeElementScreenshot(ref e) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("highlights".to_string(), Json::Array(vec![]));
                data.insert("full".to_string(), Json::Boolean(false));
                (Some("takeScreenshot"), Some(Ok(data)))
            },
            Extension(ref extension) => {
                match extension {
                    &GeckoExtensionCommand::GetContext => (Some("getContext"), None),
                    &GeckoExtensionCommand::SetContext(ref x) => {
                        (Some("setContext"), Some(x.to_marionette()))
                    },
                    &GeckoExtensionCommand::XblAnonymousChildren(ref e) => {
                        let mut data = BTreeMap::new();
                        data.insert("using".to_owned(), "anon".to_json());
                        data.insert("value".to_owned(), Json::Null);
                        data.insert("element".to_string(), e.id.to_json());
                        (Some("findElements"), Some(Ok(data)))
                    },
                    &GeckoExtensionCommand::XblAnonymousByAttribute(ref e, ref x) => {
                        let mut data = try!(x.to_marionette());
                        data.insert("element".to_string(), e.id.to_json());
                        (Some("findElement"), Some(Ok(data)))
                    },
                    &GeckoExtensionCommand::InstallAddon(ref x) => {
                        (Some("addon:install"), Some(x.to_marionette()))
                    },
                    &GeckoExtensionCommand::UninstallAddon(ref x) => {
                        (Some("addon:uninstall"), Some(x.to_marionette()))
                    }
                }
            }
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
    pub status: String,
    pub message: String,
    pub stacktrace: Option<String>
}

impl MarionetteError {
    fn new(status: String, message: String, stacktrace: Option<String>) -> MarionetteError {
        MarionetteError {
            status: status,
            message: message,
            stacktrace: stacktrace
        }
    }

    fn from_json(data: &Json) -> WebDriverResult<MarionetteError> {
        if !data.is_object() {
            return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                           "Expected an error object"));
        }
        let status = try_opt!(
            try_opt!(data.find("error"),
                     ErrorStatus::UnknownError,
                     "Error value has no status").as_string(),
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
        Ok(MarionetteError::new(status, message, stacktrace))
    }
}

impl ToJson for MarionetteError {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("status".into(), self.status.to_json());
        data.insert("message".into(), self.message.to_json());
        data.insert("stacktrace".into(), self.stacktrace.to_json());
        Json::Object(data)
    }
}

fn get_free_port() -> IoResult<u16> {
    TcpListener::bind(&("localhost", 0))
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
            session: MarionetteSession::new(session_id)
        }
    }

    pub fn connect(&mut self) -> WebDriverResult<()> {
        let timeout = 60 * 1000;  // ms
        let poll_interval = 100;  // ms
        let poll_attempts = timeout / poll_interval;
        let mut poll_attempt = 0;

        loop {
            match TcpStream::connect(&(DEFAULT_HOST, self.port)) {
                Ok(stream) => {
                    self.stream = Some(stream);
                    break
                },
                Err(e) => {
                    trace!("  connection attempt {}/{}", poll_attempt, poll_attempts);
                    if poll_attempt <= poll_attempts {
                        poll_attempt += 1;
                        sleep(Duration::from_millis(poll_interval));
                    } else {
                        return Err(WebDriverError::new(
                            ErrorStatus::UnknownError, e.description().to_owned()));
                    }
                }
            }
        };

        debug!("Connected to Marionette on {}:{}", DEFAULT_HOST, self.port);

        try!(self.handshake());
        Ok(())
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
        trace!("-> {}", data);

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
        let mut stream = self.stream.as_mut().unwrap();
        loop {
            let mut buf = &mut [0 as u8];
            let num_read = try!(stream.read(buf));
            let byte = match num_read {
                0 => {
                    return Err(IoError::new(ErrorKind::Other,
                                            "EOF reading marionette message"))
                },
                1 => buf[0] as char,
                _ => panic!("Expected one byte got more")
            };
            match byte {
                '0'...'9' => {
                    bytes = bytes * 10;
                    bytes += byte as usize - '0' as usize;
                },
                ':' => {
                    break
                }
                _ => {}
            }
        }

        let mut buf = &mut [0 as u8; 8192];
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
        let data = String::from_utf8(payload).unwrap();
        trace!("<- {}", data);

        Ok(data)
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
