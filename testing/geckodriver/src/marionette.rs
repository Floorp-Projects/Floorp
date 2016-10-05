use hyper::method::Method;
use logging;
use logging::LogLevel;
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use mozrunner::runner::{Runner, FirefoxRunner};
use mozrunner::runner::platform::firefox_default_path;
use regex::Captures;
use rustc_serialize::base64::FromBase64;
use rustc_serialize::json;
use rustc_serialize::json::{Json, ToJson};
use std::collections::BTreeMap;
use std::error::Error;
use std::fs;
use std::io;
use std::io::BufWriter;
use std::io::Cursor;
use std::io::Error as IoError;
use std::io::ErrorKind;
use std::io::prelude::*;
use std::io::Result as IoResult;
use std::net::{TcpListener, TcpStream};
use std::path::{Path, PathBuf};
use std::str::FromStr;
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
    IsDisplayed, IsSelected, GetElementAttribute, GetElementProperty, GetCSSValue,
    GetElementText, GetElementTagName, GetElementRect, IsEnabled,
    ElementClick, ElementTap, ElementClear, ElementSendKeys,
    ExecuteScript, ExecuteAsyncScript, GetCookies, GetCookie, AddCookie,
    DeleteCookies, DeleteCookie, SetTimeouts, DismissAlert,
    AcceptAlert, GetAlertText, SendAlertText, TakeScreenshot, Extension,
    SetWindowPosition, GetWindowPosition};
use webdriver::command::{
    NewSessionParameters, GetParameters, WindowSizeParameters, SwitchToWindowParameters,
    SwitchToFrameParameters, LocatorParameters, JavascriptCommandParameters,
    GetCookieParameters, AddCookieParameters, TimeoutsParameters,
    TakeScreenshotParameters, WindowPositionParameters};
use webdriver::response::{
    WebDriverResponse, NewSessionResponse, ValueResponse, WindowSizeResponse,
    WindowPositionResponse, ElementRectResponse, CookieResponse, Cookie};
use webdriver::common::{
    Date, Nullable, WebElement, FrameId, ELEMENT_KEY};
use webdriver::error::{
    WebDriverResult, WebDriverError, ErrorStatus};
use webdriver::server::{WebDriverHandler, Session};
use webdriver::httpapi::{WebDriverExtensionRoute};
use zip;

const DEFAULT_HOST: &'static str = "localhost";

lazy_static! {
    pub static ref FIREFOX_DEFAULT_PREFERENCES: [(&'static str, Pref); 50] = [
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
        ("browser.firstrun-content.dismissed", Pref::new("")),
        ("browser.offline", Pref::new(false)),
        ("browser.safebrowsing.enabled", Pref::new(false)),
        ("browser.safebrowsing.malware.enabled", Pref::new(false)),
        ("browser.search.update", Pref::new(false)),
        ("browser.sessionstore.resume_from_crash", Pref::new(false)),
        ("browser.shell.checkDefaultBrowser", Pref::new(false)),
        ("browser.startup.homepage_override.mstone", Pref::new("ignore")),
        ("browser.startup.page", Pref::new(0)),
        ("browser.tabs.warnOnOpen", Pref::new(false)),
        ("browser.usedOnWindows10.introURL", Pref::new("")),
        ("datareporting.healthreport.logging.consoleEnabled", Pref::new(false)),
        ("datareporting.healthreport.service.enabled", Pref::new(false)),
        ("datareporting.healthreport.service.firstRun", Pref::new(false)),
        ("datareporting.healthreport.uploadEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionPolicyAccepted", Pref::new(false)),
        ("devtools.errorconsole.enabled", Pref::new(true)),
        ("dom.disable_open_during_load", Pref::new(false)),
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
        ("signon.autofillForms", Pref::new(false)),
        ("signon.rememberSignons", Pref::new(false)),
        ("startup.homepage_welcome_url.additional", Pref::new("about:blank")),
        ("startup.homepage_welcome_url", Pref::new("about:blank")),
        ("toolkit.networkmanager.disable", Pref::new(true)),
        ("toolkit.telemetry.enabled", Pref::new(false)),
        ("toolkit.telemetry.prompted", Pref::new(2)),
        ("toolkit.telemetry.rejected", Pref::new(true)),
    ];

    pub static ref FIREFOX_REQUIRED_PREFERENCES: [(&'static str, Pref); 4] = [
        ("browser.tabs.warnOnClose", Pref::new(false)),
        ("browser.warnOnQuit", Pref::new(false)),
        // until bug 1238095 is fixed, we have to allow CPOWs
        ("dom.ipc.cpows.forbid-unsafe-from-browser", Pref::new(false)),
        ("marionette.defaultPrefs.enabled", Pref::new(true)),
    ];
}

pub fn extension_routes() -> Vec<(Method, &'static str, GeckoExtensionRoute)> {
    return vec![(Method::Get, "/session/{sessionId}/moz/context", GeckoExtensionRoute::GetContext),
                (Method::Post, "/session/{sessionId}/moz/context", GeckoExtensionRoute::SetContext),
                (Method::Post, "/session/{sessionId}/moz/xbl/{elementId}/anonymous_children", GeckoExtensionRoute::XblAnonymousChildren),
                (Method::Post, "/session/{sessionId}/moz/xbl/{elementId}/anonymous_by_attribute", GeckoExtensionRoute::XblAnonymousByAttribute),
]
}

#[derive(Clone, PartialEq)]
pub enum GeckoExtensionRoute {
    GetContext,
    SetContext,
    XblAnonymousChildren,
    XblAnonymousByAttribute,
}

impl WebDriverExtensionRoute for GeckoExtensionRoute {
    type Command = GeckoExtensionCommand;

    fn command(&self,
               captures: &Captures,
               body_data: &Json) -> WebDriverResult<WebDriverCommand<GeckoExtensionCommand>> {
        let command = match self {
            &GeckoExtensionRoute::GetContext => {
                GeckoExtensionCommand::GetContext
            }
            &GeckoExtensionRoute::SetContext => {
                let parameters: GeckoContextParameters = try!(Parameters::from_json(&body_data));
                GeckoExtensionCommand::SetContext(parameters)
            },
            &GeckoExtensionRoute::XblAnonymousChildren => {
                let element_id = try!(captures.name("elementId")
                                      .ok_or(WebDriverError::new(
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter")));
                GeckoExtensionCommand::XblAnonymousChildren(element_id.into())
            },
            &GeckoExtensionRoute::XblAnonymousByAttribute => {
                let element_id = try!(captures.name("elementId")
                                      .ok_or(WebDriverError::new(
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter")));
                let parameters: AttributeParameters = try!(Parameters::from_json(&body_data));
                GeckoExtensionCommand::XblAnonymousByAttribute(element_id.into(), parameters)
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
}

impl WebDriverExtensionCommand for GeckoExtensionCommand {
    fn parameters_json(&self) -> Option<Json> {
        match self {
            &GeckoExtensionCommand::GetContext => None,
            &GeckoExtensionCommand::SetContext(ref x) => Some(x.to_json()),
            &GeckoExtensionCommand::XblAnonymousChildren(_) => None,
            &GeckoExtensionCommand::XblAnonymousByAttribute(_, ref x) => Some(x.to_json()),
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

#[derive(Default)]
pub struct LogOptions {
    pub level: Option<LogLevel>,
}

#[derive(Default)]
pub struct FirefoxOptions {
    pub binary: Option<PathBuf>,
    pub profile: Option<Profile>,
    pub args: Option<Vec<String>>,
    pub log: LogOptions,
    pub prefs: Vec<(String, Pref)>,
}

impl FirefoxOptions {
    pub fn from_capabilities(capabilities: &mut NewSessionParameters) -> WebDriverResult<FirefoxOptions> {
        if let Some(options) = capabilities.consume("moz:firefoxOptions") {
            let firefox_options = try!(options
                                       .as_object()
                                       .ok_or(WebDriverError::new(
                                           ErrorStatus::InvalidArgument,
                                           "'moz:firefoxOptions' capability was not an object")));
            let binary = try!(FirefoxOptions::load_binary(&firefox_options));
            let profile = try!(FirefoxOptions::load_profile(&firefox_options));
            let args = try!(FirefoxOptions::load_args(&firefox_options));
            let log = try!(FirefoxOptions::load_log(&firefox_options));
            let prefs = try!(FirefoxOptions::load_prefs(&firefox_options));

            Ok(FirefoxOptions {
                binary: binary,
                profile: profile,
                args: args,
                log: log,
                prefs: prefs,
            })
        } else {
            Ok(Default::default())
        }
    }

    fn load_binary(options: &BTreeMap<String, Json>) -> WebDriverResult<Option<PathBuf>> {
        if let Some(path) = options.get("binary") {
            Ok(Some(PathBuf::from(try!(path
                                       .as_string()
                                       .ok_or(WebDriverError::new(
                                           ErrorStatus::InvalidArgument,
                                           "'binary' capability was not a string"))))))
        } else {
            Ok(None)
        }
    }

    fn load_profile(options: &BTreeMap<String, Json>) -> WebDriverResult<Option<Profile>> {
        if let Some(profile_json) = options.get("profile") {
            let profile_base64 = try!(profile_json
                                      .as_string()
                                      .ok_or(
                                          WebDriverError::new(ErrorStatus::UnknownError,
                                                              "Profile was not a string")));
            let profile_zip = &*try!(profile_base64.from_base64());

            // Create an emtpy profile directory
            let profile = try!(Profile::new(None));
            try!(unzip_buffer(profile_zip,
                              profile.temp_dir
                              .as_ref()
                              .expect("Profile doesn't have a path")
                              .path()));

            Ok(Some(profile))
        } else {
            Ok(None)
        }
    }

    fn load_args(options: &BTreeMap<String, Json>) -> WebDriverResult<Option<Vec<String>>> {
        if let Some(args_json) = options.get("args") {
            let args_array = try!(args_json.as_array()
                                  .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                             "Arguments were not an array")));
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

    fn load_log(options: &BTreeMap<String, Json>) -> WebDriverResult<LogOptions> {
        if let Some(json) = options.get("log") {
            let log = try!(json.as_object()
                .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument, "Log section is not an object")));

            let level = match log.get("level") {
                Some(json) => {
                    let s = try!(json.as_string()
                        .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument, "Log level is not a string")));
                    Some(try!(LogLevel::from_str(s).ok()
                        .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument, "Log level is unknown"))))
                },
                None => None,
            };

            Ok(LogOptions { level: level })

        } else {
            Ok(Default::default())
        }
    }

    pub fn load_prefs(options: &BTreeMap<String, Json>) -> WebDriverResult<Vec<(String, Pref)>> {
        if let Some(prefs_data) = options.get("prefs") {
            let prefs = try!(prefs_data
                             .as_object()
                             .ok_or(WebDriverError::new(ErrorStatus::UnknownError,"Prefs were not an object")));
            let mut rv = Vec::with_capacity(prefs.len());
            for (key, value) in prefs.iter() {
                rv.push((key.clone(), try!(pref_from_json(value))));
            };
            Ok(rv)
        } else {
            Ok(vec![])
        }
    }
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

    fn create_connection(&mut self, session_id: &Option<String>,
                         capabilities: &mut NewSessionParameters) -> WebDriverResult<()> {
        let options = try!(FirefoxOptions::from_capabilities(capabilities));

        self.current_log_level = options.log.level.clone().or(self.settings.log_level.clone());
        logging::init(&self.current_log_level);

        let port = self.settings.port.unwrap_or(try!(get_free_port()));
        if !self.settings.connect_existing {
            try!(self.start_browser(port, options));
        }

        let mut connection = MarionetteConnection::new(port, session_id.clone());
        try!(connection.connect());
        self.connection = Mutex::new(Some(connection));

        Ok(())
    }

    fn start_browser(&mut self, port: u16, mut options: FirefoxOptions) -> WebDriverResult<()> {
        let binary = try!(self.binary_path(&mut options)
                          .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                     "Expected browser binary location, \
                                                      but unable to find binary in default location, \
                                                      no 'moz:firefoxOptions.binary' capability provided, \
                                                      and no binary flag set on the command line")));

        let custom_profile = options.profile.is_some();

        let mut runner = try!(FirefoxRunner::new(&binary, options.profile.take())
                              .map_err(|e| WebDriverError::new(ErrorStatus::UnknownError,
                                                               e.description().to_owned())));
        if let Some(args) = options.args.take() {
            runner.args().extend(args);
        };

        try!(self.set_prefs(port, &mut runner.profile, custom_profile, options.prefs)
             .map_err(|e| WebDriverError::new(ErrorStatus::UnknownError,
                                              format!("Failed to set preferences:\n{}",
                                                      e.description()))));

        info!("Starting browser {}", binary.to_string_lossy());
        try!(runner.start()
             .map_err(|e| WebDriverError::new(ErrorStatus::UnknownError,
                                              format!("Failed to start browser:\n{}",
                                                      e.description()))));

        self.browser = Some(runner);

        Ok(())
    }

    fn binary_path(&self, options: &mut FirefoxOptions) -> Option<PathBuf> {
        options.binary.take()
            .or_else(|| self.settings.binary.as_ref().map(|x| x.clone()))
            .or_else(|| firefox_default_path())
    }

    pub fn set_prefs(&self, port: u16, profile: &mut Profile, custom_profile: bool,
                     extra_prefs: Vec<(String, Pref)>)
                 -> WebDriverResult<()> {
        let prefs = try!(profile.user_prefs()
                         .map_err(|_| WebDriverError::new(ErrorStatus::UnknownError,
                                                          "Unable to read profile preferences file")));

        prefs.insert("marionette.defaultPrefs.port", Pref::new(port as i64));

        if !custom_profile {
            prefs.insert_slice(&FIREFOX_DEFAULT_PREFERENCES[..]);
        };
        prefs.insert_slice(&extra_prefs[..]);

        prefs.insert_slice(&FIREFOX_REQUIRED_PREFERENCES[..]);

        if let Some(ref level) = self.current_log_level {
            prefs.insert("marionette.logging", Pref::new(level.to_string()));
        };

        prefs.write().map_err(|_| WebDriverError::new(ErrorStatus::UnknownError,
                                                      "Unable to write Firefox profile"))
    }
}

fn pref_from_json(value: &Json) -> WebDriverResult<Pref> {
    match value {
        &Json::String(ref x) => Ok(Pref::new(x.clone())),
        &Json::I64(x) => Ok(Pref::new(x)),
        &Json::U64(x) => Ok(Pref::new(x as i64)),
        &Json::Boolean(x) => Ok(Pref::new(x)),
        _ => Err(WebDriverError::new(ErrorStatus::UnknownError,
                                     "Could not convert pref value to string, boolean, or integer"))
    }
}

fn unzip_buffer(buf: &[u8], dest_dir: &Path) -> WebDriverResult<()> {
    let reader = Cursor::new(buf);
    let mut zip = try!(zip::ZipArchive::new(reader).map_err(|_| {
        WebDriverError::new(ErrorStatus::UnknownError, "Failed to unzip profile")
    }));

    for i in 0..zip.len() {
        let mut file = try!(zip.by_index(i).map_err(|_| {
            WebDriverError::new(ErrorStatus::UnknownError, "Processing profile zip file failed")
        }));
        let unzip_path = {
            let name = file.name();
            let is_dir = name.ends_with("/");
            let rel_path = Path::new(name);
            let dest_path = dest_dir.join(rel_path);

            {
                let create_dir = if is_dir {
                    Some(dest_path.as_path())
                } else {
                    dest_path.parent()
                };
                if let Some(dir) = create_dir {
                    if !dir.exists() {
                        debug!("Creating profile directory tree {}", dir.to_string_lossy());
                        try!(fs::create_dir_all(dir));
                    }
                }
            }

            if is_dir {
                None
            } else {
                Some(dest_path)
            }
        };

        if let Some(unzip_path) = unzip_path {
            debug!("Extracting profile to {}", unzip_path.to_string_lossy());
            let dest = try!(fs::File::create(unzip_path));
            let mut writer = BufWriter::new(dest);
            try!(io::copy(&mut file, &mut writer));
        }
    }

    Ok(())
}

impl WebDriverHandler<GeckoExtensionRoute> for MarionetteHandler {
    fn handle_command(&mut self, _: &Option<Session>, mut msg: WebDriverMessage<GeckoExtensionRoute>) -> WebDriverResult<WebDriverResponse> {
        {
            let mut new_capabilities = None;
            match self.connection.lock() {
                Ok(ref connection) => {
                    if connection.is_none() {
                        match msg.command {
                            NewSession(ref mut capabilities) => {
                                new_capabilities = Some(capabilities);
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
                        "Failed to aquire Marionette connection"))
                }
            }
            if let Some(capabilities) = new_capabilities {
                try!(self.create_connection(&msg.session_id, capabilities));
            }
        }

        match self.connection.lock() {
            Ok(ref mut connection) => {
                match connection.as_mut() {
                    Some(conn) => conn.send_command(&msg),
                    None => panic!()
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
            IsSelected(_) | GetElementAttribute(_, _) | GetElementProperty(_, _) |
            GetCSSValue(_, _) | GetElementText(_) | SetWindowPosition(_) |
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
            GetWindowPosition => {
                let x = try_opt!(
                    try_opt!(resp.result.find("x"),
                             ErrorStatus::UnknownError,
                             "Failed to find x field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as integer");

                let y = try_opt!(
                    try_opt!(resp.result.find("y"),
                             ErrorStatus::UnknownError,
                             "Failed to find y field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret y as integer");

                WebDriverResponse::WindowPosition(WindowPositionResponse::new(x, y))
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
                    }
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
            params: params,
        }
    }

    fn from_webdriver_message(id: u64, msg: &WebDriverMessage<GeckoExtensionRoute>) -> WebDriverResult<MarionetteCommand> {
        let (opt_name, opt_parameters) = match msg.command {
            NewSession(ref x) => {
                let mut data = BTreeMap::new();
                data.insert("sessionId".to_string(), Json::Null);
                data.insert("capabilities".to_string(), x.to_json());
                (Some("newSession"), Some(Ok(data)))
            },
            DeleteSession => {
                let mut body = BTreeMap::new();
                body.insert("flags".to_owned(), vec!["eForceQuit".to_json()].to_json());
                (Some("quitApplication"), Some(Ok(body)))
            },
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
            SetWindowPosition(ref x) => (Some("setWindowPosition"), Some(x.to_marionette())),
            GetWindowPosition => (Some("getWindowPosition"), None),
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
                let json_value: Vec<String> = x.value.iter().map(|x| {
                    x.to_string()
                }).collect();
                data.insert("value".to_string(), json_value.to_json());
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

        info!("Connecting to Marionette on {}:{}", DEFAULT_HOST, self.port);
        loop {
            match TcpStream::connect(&(DEFAULT_HOST, self.port)) {
                Ok(stream) => {
                    self.stream = Some(stream);
                    break
                },
                Err(e) => {
                    debug!("  connection attempt {}/{}", poll_attempt, poll_attempts);
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

        debug!("TCP connection established");

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

    pub fn send_command(&mut self, msg: &WebDriverMessage<GeckoExtensionRoute>) -> WebDriverResult<WebDriverResponse>  {
        let command = try!(MarionetteCommand::from_webdriver_message(
            self.session.next_command_id(), msg));

        let resp_data = try!(self.send(command.to_json()));
        let json_data: Json = try!(Json::from_str(&*resp_data));

        self.session.response(msg, try!(MarionetteResponse::from_json(&json_data)))
    }

    fn send(&mut self, msg: Json) -> WebDriverResult<String> {
        let data = self.encode_msg(msg);
        debug!("→ {}", data);

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
            Ok(resp) => Ok(resp),
            Err(_) => {
                let mut err = WebDriverError::new(
                    ErrorStatus::UnknownError, "Failed to decode response from marionette");
                err.set_delete_session();
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
        debug!("← {}", data);

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

impl ToMarionette for WindowSizeParameters {
    fn to_marionette(&self) -> WebDriverResult<BTreeMap<String, Json>> {
        Ok(try_opt!(self.to_json().as_object(), ErrorStatus::UnknownError, "Expected an object").clone())
    }
}

impl ToMarionette for WindowPositionParameters {
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
