use hyper::method::Method;
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use mozrunner::runner::{Runner, FirefoxRunner, RunnerError};
use regex::Captures;
use rustc_serialize::base64::FromBase64;
use rustc_serialize::json::{Json, ToJson};
use rustc_serialize::json;
use std::collections::BTreeMap;
use std::error::Error;
use std::fs;
use std::io::BufWriter;
use std::io::Cursor;
use std::io::Error as IoError;
use std::io::ErrorKind;
use std::io::Result as IoResult;
use std::io::prelude::*;
use std::io;
use std::net::TcpStream;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::thread::sleep;
use std::time::Duration;
use webdriver::command::{WebDriverCommand, WebDriverMessage, Parameters,
                         WebDriverExtensionCommand};
use webdriver::command::WebDriverCommand::{
    NewSession, DeleteSession, Get, GetCurrentUrl,
    GoBack, GoForward, Refresh, GetTitle, GetPageSource, GetWindowHandle,
    GetWindowHandles, Close, SetWindowSize,
    GetWindowSize, MaximizeWindow, SwitchToWindow, SwitchToFrame,
    SwitchToParentFrame, FindElement, FindElements,
    FindElementElement, FindElementElements, GetActiveElement,
    IsDisplayed, IsSelected, GetElementAttribute, GetCSSValue,
    GetElementText, GetElementTagName, GetElementRect, IsEnabled,
    ElementClick, ElementTap, ElementClear, ElementSendKeys,
    ExecuteScript, ExecuteAsyncScript, GetCookies, GetCookie, AddCookie,
    DeleteCookies, DeleteCookie, SetTimeouts, DismissAlert,
    AcceptAlert, GetAlertText, SendAlertText, TakeScreenshot, Extension};
use webdriver::command::{
    NewSessionParameters, GetParameters, WindowSizeParameters, SwitchToWindowParameters,
    SwitchToFrameParameters, LocatorParameters, JavascriptCommandParameters,
    GetCookieParameters, AddCookieParameters, TimeoutsParameters,
    TakeScreenshotParameters};
use webdriver::response::{
    WebDriverResponse, NewSessionResponse, ValueResponse, WindowSizeResponse,
    ElementRectResponse, CookieResponse, Cookie};
use webdriver::common::{
    Date, Nullable, WebElement, FrameId, ELEMENT_KEY};
use webdriver::error::{
    WebDriverResult, WebDriverError, ErrorStatus};
use webdriver::server::{WebDriverHandler, Session};
use webdriver::httpapi::{WebDriverExtensionRoute};
use zip;

lazy_static! {
    pub static ref E10S_PREFERENCES: [(&'static str, Pref); 1] = [
        ("browser.tabs.remote.autostart", Pref::new(true)),
    ];

    pub static ref NON_E10S_PREFERENCES: [(&'static str, Pref); 2] = [
        ("browser.tabs.remote.autostart", Pref::new(false)),
        ("browser.tabs.remote.autostart.2", Pref::new(false))
    ];

    pub static ref FIREFOX_DEFAULT_PREFERENCES: [(&'static str, Pref); 44] = [
        ("app.update.auto", Pref::new(false)),
        ("app.update.enabled", Pref::new(false)),
        ("browser.displayedE10SPrompt.1", Pref::new(5)),
        ("browser.displayedE10SPrompt.2", Pref::new(5)),
        ("browser.displayedE10SPrompt.3", Pref::new(5)),
        ("browser.displayedE10SPrompt.4", Pref::new(5)),
        ("browser.displayedE10SPrompt", Pref::new(5)),
        ("browser.dom.window.dump.enabled", Pref::new(true)),
        ("browser.EULA.3.accepted", Pref::new(true)),
        ("browser.EULA.override", Pref::new(true)),
        ("browser.offline", Pref::new(false)),
        ("browser.safebrowsing.enabled", Pref::new(false)),
        ("browser.safebrowsing.malware.enabled", Pref::new(false)),
        ("browser.search.update", Pref::new(false)),
        ("browser.sessionstore.resume_from_crash", Pref::new(false)),
        ("browser.shell.checkDefaultBrowser", Pref::new(false)),
        ("browser.startup.page", Pref::new(0)),
        ("browser.tabs.warnOnOpen", Pref::new(false)),
        ("datareporting.healthreport.logging.consoleEnabled", Pref::new(false)),
        ("datareporting.healthreport.service.enabled", Pref::new(false)),
        ("datareporting.healthreport.service.firstRun", Pref::new(false)),
        ("datareporting.healthreport.uploadEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionPolicyAccepted", Pref::new(false)),
        ("devtools.errorconsole.enabled", Pref::new(true)),
        ("dom.ipc.reportProcessHangs", Pref::new(false)),
        ("focusmanager.testmode", Pref::new(true)),
        ("security.fileuri.origin_policy", Pref::new(3)),
        ("security.fileuri.strict_origin_policy", Pref::new(false)),
        ("security.warn_entering_secure", Pref::new(false)),
        ("security.warn_entering_secure.show_once", Pref::new(false)),
        ("security.warn_entering_weak", Pref::new(false)),
        ("security.warn_entering_weak.show_once", Pref::new(false)),
        ("security.warn_leaving_secure", Pref::new(false)),
        ("security.warn_leaving_secure.show_once", Pref::new(false)),
        ("security.warn_submit_insecure", Pref::new(false)),
        ("security.warn_viewing_mixed", Pref::new(false)),
        ("security.warn_viewing_mixed.show_once", Pref::new(false)),
        ("signon.rememberSignons", Pref::new(false)),
        ("startup.homepage_welcome_url", Pref::new("about:blank")),
        ("toolkit.networkmanager.disable", Pref::new(true)),
        ("toolkit.telemetry.enabled", Pref::new(false)),
        ("toolkit.telemetry.prompted", Pref::new(2)),
        ("toolkit.telemetry.rejected", Pref::new(true)),
    ];

    pub static ref FIREFOX_REQUIRED_PREFERENCES: [(&'static str, Pref); 5] = [
        ("browser.tabs.warnOnClose", Pref::new(false)),
        ("browser.warnOnQuit", Pref::new(false)),
        // until bug 1238095 is fixed, we have to allow CPOWs
        ("dom.ipc.cpows.forbid-unsafe-from-browser", Pref::new(false)),
        ("marionette.defaultPrefs.enabled", Pref::new(true)),
        ("marionette.logging", Pref::new(true)),
    ];
}

pub fn extension_routes() -> Vec<(Method, &'static str, GeckoExtensionRoute)> {
    return vec![(Method::Get, "/session/{sessionId}/moz/context", GeckoExtensionRoute::GetContext),
                (Method::Post, "/session/{sessionId}/moz/context", GeckoExtensionRoute::SetContext)]
}

#[derive(Clone, Copy, PartialEq)]
pub enum GeckoExtensionRoute {
    GetContext,
    SetContext
}

impl WebDriverExtensionRoute for GeckoExtensionRoute {
    type Command = GeckoExtensionCommand;

    fn command(&self,
               _captures: &Captures,
               body_data: &Json) -> WebDriverResult<WebDriverCommand<GeckoExtensionCommand>> {
        let command = match self {
            &GeckoExtensionRoute::GetContext => {
                GeckoExtensionCommand::GetContext
            }
            &GeckoExtensionRoute::SetContext => {
                let parameters: GeckoContextParameters = try!(Parameters::from_json(&body_data));
                GeckoExtensionCommand::SetContext(parameters)
            }
        };
        Ok(WebDriverCommand::Extension(command))
    }
}

#[derive(Clone, PartialEq)]
pub enum GeckoExtensionCommand {
    GetContext,
    SetContext(GeckoContextParameters),
}

impl WebDriverExtensionCommand for GeckoExtensionCommand {
    fn parameters_json(&self) -> Option<Json> {
        match self {
            &GeckoExtensionCommand::GetContext => None,
            &GeckoExtensionCommand::SetContext(ref x) => Some(x.to_json()),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
enum GeckoContext {
    Content,
    Chrome
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

impl ToJson for GeckoContextParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("context".to_owned(), self.context.to_json());
        Json::Object(data)
    }
}

impl ToMarionette for GeckoContextParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        data.insert("value".to_owned(), self.context.to_json());
        Ok(data)
    }
}

pub enum BrowserLauncher {
    None,
    BinaryLauncher(PathBuf)
}

pub struct MarionetteSettings {
    port: u16,
    launcher: BrowserLauncher,
    e10s: bool
}

impl MarionetteSettings {
    pub fn new(port: u16, launcher: BrowserLauncher, e10s: bool) -> MarionetteSettings {
        MarionetteSettings {
            port: port,
            launcher: launcher,
            e10s: e10s
        }
    }
}

pub struct MarionetteHandler {
    connection: Mutex<Option<MarionetteConnection>>,
    launcher: BrowserLauncher,
    browser: Option<FirefoxRunner>,
    port: u16,
    e10s: bool,
}

impl MarionetteHandler {
    pub fn new(settings: MarionetteSettings) -> MarionetteHandler {
        MarionetteHandler {
            connection: Mutex::new(None),
            launcher: settings.launcher,
            browser: None,
            port: settings.port,
            e10s: settings.e10s
        }
    }

    fn create_connection(&mut self, session_id: &Option<String>,
                         capabilities: &NewSessionParameters) -> WebDriverResult<()> {
        let profile = try!(self.load_profile(capabilities));
        let args = try!(self.load_browser_args(capabilities));
        match self.start_browser(profile, args) {
            Err(e) => {
                return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                               e.description().to_owned()));
            },
            Ok(_) => {}
        }
        debug!("Creating connection");
        let mut connection = MarionetteConnection::new(self.port, session_id.clone());
        debug!("Starting marionette connection");
        try!(connection.connect());
        debug!("Marionette connection started");
        self.connection = Mutex::new(Some(connection));
        Ok(())
    }

    fn start_browser(&mut self, profile: Option<Profile>, args: Option<Vec<String>>) -> Result<(), RunnerError> {
        let custom_profile = profile.is_some();

        match self.launcher {
            BrowserLauncher::BinaryLauncher(ref binary) => {
                let mut runner = try!(FirefoxRunner::new(&binary, profile));
                if let Some(cmd_args) = args {
                    runner.args().extend(cmd_args);
                };
                try!(self.set_prefs(&mut runner.profile, custom_profile));
                try!(runner.start());

                self.browser = Some(runner);

                debug!("Browser started");
            },
            BrowserLauncher::None => {}
        }

        Ok(())
    }

    pub fn set_prefs(&self, profile: &mut Profile, custom_profile: bool)
                 -> Result<(), RunnerError> {
        let prefs = try!(profile.user_prefs());
        prefs.insert("marionette.defaultPrefs.port",
                     Pref::new(self.port as i64));

        prefs.insert_slice(&FIREFOX_REQUIRED_PREFERENCES[..]);
        if !custom_profile {
            prefs.insert_slice(&FIREFOX_DEFAULT_PREFERENCES[..]);
            if self.e10s {
                prefs.insert_slice(&E10S_PREFERENCES[..]);
            } else {
                prefs.insert_slice(&NON_E10S_PREFERENCES[..]);
            }
        };
        try!(prefs.write());
        Ok(())
    }

    pub fn load_profile(&self, capabilities: &NewSessionParameters) -> WebDriverResult<Option<Profile>> {
        let profile_opt = capabilities.get("firefox_profile");
        if profile_opt.is_none() {
            return Ok(None);
        }
        debug!("Using custom profile");
        let profile_json = profile_opt.unwrap();
        let profile_base64 = try!(profile_json.as_string().ok_or(
            WebDriverError::new(
                ErrorStatus::UnknownError,
                "Profile was not a string")));
        let profile_zip = &*try!(profile_base64.from_base64());
        // Create an emtpy profile directory
        let profile = try!(Profile::new(None));
        try!(unzip_buffer(profile_zip,
                          profile.temp_dir.as_ref().expect("Profile doesn't have a path").path()));
        // TODO - Stop mozprofile erroring if user.js already exists
        Ok(Some(profile))
    }

    pub fn load_browser_args(&self, capabilities: &NewSessionParameters) -> WebDriverResult<Option<Vec<String>>> {
        if let Some(args_json) = capabilities.get("firefox_args") {
            let args_array = try!(args_json.as_array()
                                  .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                             "Arguments was not an array")));
            let args = try!(args_array
                            .iter()
                            .map(|x| x.as_string().map(|x| x.to_owned()))
                            .collect::<Option<Vec<String>>>()
                            .ok_or(WebDriverError::new(
                                ErrorStatus::UnknownError,
                                "Arguments entries were not all strings")));
            Ok(Some(args))
        } else {
            Ok(None)
        }
    }
}

fn unzip_buffer(buf: &[u8], dest_dir: &Path) -> WebDriverResult<()> {
    let reader = Cursor::new(buf);
    let mut zip = try!(zip::ZipArchive::new(reader).map_err(|_| {
        WebDriverError::new(
            ErrorStatus::UnknownError,
            "Failed to unzip profile")
    }));
    for i in 0..zip.len() {
        let mut file = try!(zip.by_index(i).map_err(|_| {
            WebDriverError::new(
                ErrorStatus::UnknownError,
                "Processing zip file failed")
        }));
        let unzip_path = {
            let rel_path = Path::new(file.name());
            let unzip_path = dest_dir.join(rel_path);
            if let Some(dir) = unzip_path.parent() {
                if !dir.exists() {
                    try!(fs::create_dir_all(dir));
                }
            }
            unzip_path
        };
        let dest = try!(fs::File::create(unzip_path));
        let mut writer = BufWriter::new(dest);
        try!(io::copy(&mut file, &mut writer));
    }
    Ok(())
}

impl WebDriverHandler<GeckoExtensionRoute> for MarionetteHandler {
    fn handle_command(&mut self, _: &Option<Session>, msg: &WebDriverMessage<GeckoExtensionRoute>) -> WebDriverResult<WebDriverResponse> {
        let mut new_capabilities = None;
        match self.connection.lock() {
            Ok(ref mut connection) => {
                if connection.is_none() {
                    match msg.command {
                        NewSession(ref capabilities) => {
                            debug!("Got NewSession command");
                            new_capabilities = Some(capabilities)
                        },
                        _ => {
                            return Err(WebDriverError::new(
                                ErrorStatus::UnknownError,
                                "Tried to run command without establishing a connection"));
                        }
                    }
                }
            },
            Err(_) => {
                return Err(WebDriverError::new(
                    ErrorStatus::UnknownError,
                    "Failed to aquire marionette connection"))
            }
        }
        if let Some(capabilities) = new_capabilities {
            try!(self.create_connection(&msg.session_id, &capabilities));
        }
        match self.connection.lock() {
            Ok(ref mut connection) => {
                match connection.as_mut() {
                    Some(conn) => conn.send_command(msg),
                    None => panic!()
                }
            },
            Err(_) => {
                Err(WebDriverError::new(
                    ErrorStatus::UnknownError,
                    "Failed to aquire marionette connection"))
            }
        }
    }

    fn delete_session(&mut self, _: &Option<Session>) {
        debug!("delete_session");
        if let Ok(connection) = self.connection.lock() {
            if let Some(ref conn) = *connection {
                conn.close();
            }
        }
        if let Some(ref mut runner) = self.browser {
            debug!("Closing browser");
            if runner.stop().is_err() {
                error!("Failed to kill browser");
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

    pub fn response(&mut self, message: &WebDriverMessage<GeckoExtensionRoute>,
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

        try!(self.update(message, &resp));

        Ok(match message.command {
            //Everything that doesn't have a response value
            Get(_) | GoBack | GoForward | Refresh | Close | SetTimeouts(_) |
            SetWindowSize(_) | MaximizeWindow | SwitchToWindow(_) | SwitchToFrame(_) |
            SwitchToParentFrame | AddCookie(_) | DeleteCookies | DeleteCookie(_) |
            DismissAlert | AcceptAlert | SendAlertText(_) | ElementClick(_) |
            ElementTap(_) | ElementClear(_) | ElementSendKeys(_, _) => {
                WebDriverResponse::Void
            },
            //Things that simply return the contents of the marionette "value" property
            GetCurrentUrl | GetTitle | GetPageSource | GetWindowHandle | IsDisplayed(_) |
            IsSelected(_) | GetElementAttribute(_, _) | GetCSSValue(_, _) | GetElementText(_) |
            GetElementTagName(_) | IsEnabled(_) | ExecuteScript(_) | ExecuteAsyncScript(_) |
            GetAlertText | TakeScreenshot => {
                let value = try_opt!(resp.result.find("value"),
                                     ErrorStatus::UnknownError,
                                     "Failed to find value field");
                //TODO: Convert webelement keys
                WebDriverResponse::Generic(ValueResponse::new(value.clone()))
            },
            GetWindowHandles => {
                WebDriverResponse::Generic(ValueResponse::new(resp.result.clone()))
            },
            GetWindowSize => {
                let width = try_opt!(
                    try_opt!(resp.result.find("width"),
                             ErrorStatus::UnknownError,
                             "Failed to find width field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as integer");

                let height = try_opt!(
                    try_opt!(resp.result.find("height"),
                             ErrorStatus::UnknownError,
                             "Failed to find height field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as integer");

                WebDriverResponse::WindowSize(WindowSizeResponse::new(width, height))
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

                WebDriverResponse::ElementRect(ElementRectResponse::new(x, y, width, height))
            },
            GetCookies => {
                let cookies = try!(self.process_cookies(&resp.result));
                WebDriverResponse::Cookie(CookieResponse::new(cookies))
            },
            GetCookie(ref name) => {
                let mut cookies = try!(self.process_cookies(&resp.result));
                cookies.retain(|x| x.name == *name);
                WebDriverResponse::Cookie(CookieResponse::new(cookies))
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
                    }
                    &GeckoExtensionCommand::SetContext(_) => WebDriverResponse::Void
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
                         "Failed to find name field").as_string(),
                ErrorStatus::UnknownError,
                "Failed to interpret name as string").to_string();
            let value = try_opt!(
                try_opt!(x.find("value"),
                         ErrorStatus::UnknownError,
                         "Failed to find value field").as_string(),
                ErrorStatus::UnknownError,
                "Failed to interpret value as string").to_string();
            let path = try!(
                Nullable::from_json(x.find("path").unwrap_or(&Json::Null),
                                    |x| {
                                        Ok((try_opt!(x.as_string(),
                                                     ErrorStatus::UnknownError,
                                                     "Failed to interpret path as String")).to_string())
                                    }));
            let domain = try!(
                Nullable::from_json(x.find("domain").unwrap_or(&Json::Null),
                                    |x| {
                                        Ok((try_opt!(x.as_string(),
                                                     ErrorStatus::UnknownError,
                                                     "Failed to interpret domain as String")).to_string())
                                    }));
            let expiry = try!(
                Nullable::from_json(x.find("expiry").unwrap_or(&Json::Null),
                                    |x| {
                                        Ok(Date::new((try_opt!(
                                            x.as_u64(),
                                            ErrorStatus::UnknownError,
                                            "Failed to interpret domain as String"))))
                                    }));
            let secure = try_opt!(
                x.find("secure").map_or(Some(false), |x| x.as_boolean()),
                ErrorStatus::UnknownError,
                "Failed to interpret secure as boolean");
            let http_only = try_opt!(
                x.find("httpOnly").map_or(Some(false), |x| x.as_boolean()),
                ErrorStatus::UnknownError,
                "Failed to interpret httpOnly as boolean");
            Ok(Cookie::new(name, value, path, domain, expiry, secure, http_only))
        }).collect::<Result<Vec<_>, _>>()
    }

    pub fn error_from_string(&self, error_code: &str) -> ErrorStatus {
        match error_code {
            "element not selectable" => ErrorStatus::ElementNotSelectable,
            "element not visible" => ErrorStatus::ElementNotVisible,
            "invalid argument" => ErrorStatus::InvalidArgument,
            "invalid cookie domain" => ErrorStatus::InvalidCookieDomain,
            "invalid element coordinates" => ErrorStatus::InvalidElementCoordinates,
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
            "unable to set cookie" => ErrorStatus::UnableToSetCookie,
            "unexpected alert open" => ErrorStatus::UnexpectedAlertOpen,
            "unknown error" => ErrorStatus::UnknownError,
            "unknown command" => ErrorStatus::UnknownPath,
            "unsupported operation" => ErrorStatus::UnsupportedOperation,
            _ => ErrorStatus::UnknownError
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
            params: params
        }
    }

    fn from_webdriver_message(id: u64, msg: &WebDriverMessage<GeckoExtensionRoute>) -> WebDriverResult<MarionetteCommand> {
        let (opt_name, opt_parameters) = match msg.command {
            NewSession(_) => {
                let mut data = BTreeMap::new();
                data.insert("sessionId".to_string(), Json::Null);
                data.insert("capabilities".to_string(), Json::Null);
                debug!("Creating NewSession message");
                (Some("newSession"), Some(Ok(data)))
            },
            DeleteSession => (Some("deleteSession"), None),
            Get(ref x) => (Some("get"), Some(x.to_marionette())),
            GetCurrentUrl => (Some("getCurrentUrl"), None),
            GoBack => (Some("goBack"), None),
            GoForward => (Some("goForward"), None),
            Refresh => (Some("refresh"), None),
            GetTitle => (Some("getTitle"), None),
            GetPageSource => (Some("getPageSource"), None),
            GetWindowHandle => (Some("getWindowHandle"), None),
            GetWindowHandles => (Some("getWindowHandles"), None),
            Close => (Some("close"), None),
            SetTimeouts(ref x) => (Some("timeouts"), Some(x.to_marionette())),
            SetWindowSize(ref x) => (Some("setWindowSize"), Some(x.to_marionette())),
            GetWindowSize => (Some("getWindowSize"), None),
            MaximizeWindow => (Some("maximizeWindow"), None),
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
            ElementClick(ref x) => (Some("clickElement"), Some(x.to_marionette())),
            ElementTap(ref x) => (Some("singleTap"), Some(x.to_marionette())),
            ElementClear(ref x) => (Some("clearElement"), Some(x.to_marionette())),
            ElementSendKeys(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                let json_value: Vec<String> = x.value.iter().map(|x| {
                    x.to_string()
                }).collect();
                data.insert("value".to_string(), json_value.to_json());
                (Some("sendKeysToElement"), Some(Ok(data)))
            },
            ExecuteScript(ref x) => (Some("executeScript"), Some(x.to_marionette())),
            ExecuteAsyncScript(ref x) => (Some("executeAsyncScript"), Some(x.to_marionette())),
            GetCookies | GetCookie(_) => (Some("getCookies"), None),
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
                data.insert("value".to_string(), x.to_json());
                (Some("sendKeysToDialog"), Some(Ok(data)))
            },
            TakeScreenshot => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), Json::Null);
                data.insert("highlights".to_string(), Json::Array(vec![]));
                data.insert("full".to_string(), Json::Boolean(false));
                (Some("takeScreenshot"), Some(Ok(data)))
            },
            Extension(ref extension) => {
                match extension {
                    &GeckoExtensionCommand::GetContext => (Some("getContext"), None),
                    &GeckoExtensionCommand::SetContext(ref x) => {
                        (Some("setContext"), Some(x.to_marionette()))
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
    pub result: Json
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
            match TcpStream::connect(&("localhost", self.port)) {
                Ok(stream) => {
                    self.stream = Some(stream);
                    break
                },
                Err(e) => {
                    debug!("{}/{}", poll_attempt, poll_attempts);
                    if poll_attempt <= poll_attempts {
                        poll_attempt += 1;
                        sleep(Duration::from_millis(poll_interval));
                    } else {
                        return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                       e.description().to_owned()));
                    }
                }
            }
        };

        debug!("TCP stream open");

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
                                              "Missing marionetteProtocol field in handshake").to_string());

        self.session.application_type = Some(try_opt!(data.get("applicationType"),
                                              ErrorStatus::UnknownError,
                                              "Missing applicationType field in handshake").to_string());

        if self.session.protocol != Some("3".into()) {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                format!("Unsupported marionette protocol version {}, required 3",
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

    pub fn send_command(&mut self, msg: &WebDriverMessage<GeckoExtensionRoute>) -> WebDriverResult<WebDriverResponse>  {
        let command = try!(MarionetteCommand::from_webdriver_message(
            self.session.next_command_id(), msg));

        let resp_data = try!(self.send(command.to_json()));

        let json_data : Json = try!(Json::from_str(&*resp_data));

        self.session.response(msg, try!(MarionetteResponse::from_json(&json_data)))
    }

    fn send(&mut self, msg: Json) -> WebDriverResult<String> {
        let data = self.encode_msg(msg);
        debug!("Sending {}", data);
        match self.stream {
            Some(ref mut stream) => {
                if stream.write(&*data.as_bytes()).is_err() {
                    let mut err = WebDriverError::new(ErrorStatus::UnknownError,
                                                      "Failed to write response to stream");
                    err.set_delete_session();
                    return Err(err);
                }
            },
            None => {
                let mut err = WebDriverError::new(ErrorStatus::UnknownError,
                                                  "Tried to write before opening stream");
                err.set_delete_session();
                return Err(err);
            }
        }
        match self.read_resp() {
            Ok(resp) => {
                debug!("Marionette response {}", resp);
                Ok(resp)
            },
            Err(_) => {
                let mut err = WebDriverError::new(ErrorStatus::UnknownError,
                                                  "Failed to decode response from marionette");
                err.set_delete_session();
                Err(err)
            }
        }
    }

    fn read_resp(&mut self) -> IoResult<String> {
        debug!("Entering read_resp");
        let mut bytes = 0usize;
        //TODO: Check before we unwrap?
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
            debug!("Got byte {}", byte);
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
        let mut data = Vec::with_capacity(bytes);
        let mut total_read = 0;
        while total_read < bytes {
            let num_read = try!(stream.read(buf));
            if num_read == 0 {
                return Err(IoError::new(ErrorKind::Other,
                                        "EOF reading marionette message"))
            }
            total_read += num_read;
            for x in &buf[..num_read] {
                data.push(*x);
            }
        }
        debug!("Leaving read_resp");
        //Need to handle the error here
        Ok(String::from_utf8(data).unwrap())
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

impl ToMarionette for WindowSizeParameters {
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
        Ok(try_opt!(self.to_json().as_object(), ErrorStatus::UnknownError, "Expected an object").clone())
    }
}

impl ToMarionette for SwitchToFrameParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        let mut data = BTreeMap::new();
        let key = match self.id {
            FrameId::Null => None,
            FrameId::Short(_) => Some("id"),
            FrameId::Element(_) => Some("element")
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

impl ToMarionette for GetCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(), ErrorStatus::UnknownError, "Expected an object").clone())
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
