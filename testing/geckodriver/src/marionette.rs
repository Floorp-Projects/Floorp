use rustc_serialize::json;
use rustc_serialize::json::{Json, ToJson};
use std::collections::BTreeMap;
use std::old_io::{IoResult, TcpStream, IoError};
use std::old_io::timer::Timer;
use std::old_path::Path;
use std::sync::Mutex;
use std::time::duration::Duration;

use mozrunner::runner::{Runner, FirefoxRunner};
use mozprofile::preferences::{PrefValue};

use webdriver::command::{WebDriverMessage};
use webdriver::command::WebDriverCommand::{
    NewSession, DeleteSession, Get, GetCurrentUrl,
    GoBack, GoForward, Refresh, GetTitle, GetWindowHandle,
    GetWindowHandles, Close, SetWindowSize,
    GetWindowSize, MaximizeWindow, SwitchToWindow, SwitchToFrame,
    SwitchToParentFrame, FindElement, FindElements,
    FindElementElement, FindElementElements, IsDisplayed,
    IsSelected, GetElementAttribute, GetCSSValue, GetElementText,
    GetElementTagName, GetElementRect, IsEnabled, ElementClick,
    ElementTap, ElementClear, ElementSendKeys, ExecuteScript,
    ExecuteAsyncScript, GetCookie, AddCookie, SetTimeouts,
    DismissAlert, AcceptAlert, GetAlertText, SendAlertText,
    TakeScreenshot};
use webdriver::command::{
    GetParameters, WindowSizeParameters, SwitchToWindowParameters,
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

pub enum BrowserLauncher {
    None,
    BinaryLauncher(Path)
}

pub struct MarionetteSettings {
    port: u16,
    launcher: BrowserLauncher
}

impl MarionetteSettings {
    pub fn new(port: u16, launcher: BrowserLauncher) -> MarionetteSettings {
        MarionetteSettings {
            port: port,
            launcher: launcher
        }
    }
}

pub struct MarionetteHandler {
    connection: Mutex<Option<MarionetteConnection>>,
    launcher: BrowserLauncher,
    browser: Option<FirefoxRunner>,
    port: u16,
}

impl MarionetteHandler {
    pub fn new(settings: MarionetteSettings) -> MarionetteHandler {
        MarionetteHandler {
            connection: Mutex::new(None),
            launcher: settings.launcher,
            browser: None,
            port: settings.port
        }
    }

    fn create_connection(&mut self, session_id: &Option<String>) -> WebDriverResult<()> {
        if let Err(e) = self.start_browser() {
            error!("Error starting browser: {}", e.desc);
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                "Failed to start browser")
            )
        };
        debug!("Creating connection");
        let mut connection = MarionetteConnection::new(self.port, session_id.clone());
        debug!("Starting marionette connection");
        if let Err(e) = connection.connect() {
            return Err(WebDriverError::new(
                ErrorStatus::UnknownError,
                &format!("Failed to start marionette connection:\n{}", e.desc)[..]));
        }
        debug!("Marionette connection started");
        self.connection = Mutex::new(Some(connection));
        Ok(())
    }

    fn start_browser(&mut self) -> IoResult<()> {
        match self.launcher {
            BrowserLauncher::BinaryLauncher(ref binary) => {
                let mut runner = try!(FirefoxRunner::new(binary.clone(), None));
                runner.profile.preferences.insert("marionette.defaultPrefs.enabled",
                                                  PrefValue::PrefBool(true));
                runner.profile.preferences.insert("marionette.defaultPrefs.port",
                                                  PrefValue::PrefInt(self.port as isize));
                try!(runner.start());

                self.browser = Some(runner);

                debug!("Browser started");
            },
            BrowserLauncher::None => {}
        }

        Ok(())
    }
}

impl WebDriverHandler for MarionetteHandler {
    fn handle_command(&mut self, _: &Option<Session>, msg: &WebDriverMessage) -> WebDriverResult<WebDriverResponse> {
        let mut create_connection = false;
        match self.connection.lock() {
            Ok(ref mut connection) => {
                if connection.is_none() {
                    match msg.command {
                        NewSession => {
                            create_connection = true;
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
        if create_connection {
            try!(self.create_connection(&msg.session_id));
        }
        match self.connection.lock() {
            Ok(ref mut connection) => {
                match connection.as_mut() {
                    Some(conn) => conn.send_message(msg),
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
        if let Ok(connection) = self.connection.lock() {
            if let Some(ref conn) = *connection {
                conn.close();
            }
        }
        if let Some(ref mut runner) = self.browser {
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
    pub to: String
}

fn object_from_json(data: &str) -> WebDriverResult<BTreeMap<String, Json>> {
    Ok(try_opt!(try!(Json::from_str(data)).as_object(),
                ErrorStatus::UnknownError,
                "Expected a json object").clone())
}

impl MarionetteSession {
    pub fn new(session_id: Option<String>) -> MarionetteSession {
        let initital_id = session_id.unwrap_or("".to_string());
        MarionetteSession {
            session_id: initital_id,
            to: "root".to_string()
        }
    }

    pub fn msg_to_marionette(&self, msg: &WebDriverMessage) -> WebDriverResult<Json> {
        let x = try!(msg.to_marionette());
        let mut data = try_opt!(x.as_object(),
                                ErrorStatus::UnknownError,
                                "Message was not a JSON Object").clone();
        data.insert("to".to_string(), self.to.to_json());
        Ok(Json::Object(data))
    }

    pub fn update(&mut self, msg: &WebDriverMessage, resp: &BTreeMap<String, Json>) -> WebDriverResult<()> {
        match msg.command {
            NewSession => {
                let session_id = try_opt!(
                    try_opt!(resp.get("sessionId"),
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

    pub fn response_from_json(&mut self, message: &WebDriverMessage,
                              data: &str) -> WebDriverResult<WebDriverResponse> {
        let json_data = try!(object_from_json(data));

        if let Some(error) = json_data.get("error") {
            let error = try_opt!(error.as_object(),
                                 ErrorStatus::UnknownError,
                                 "Marionette error field was not an object");
            let status_code = try_opt!(
                try_opt!(error.get("status"),
                         ErrorStatus::UnknownError,
                         "Error dict doesn't have a status field").as_u64(),
                ErrorStatus::UnknownError,
                "Error status isn't an integer");
            let status = self.error_from_code(status_code);
            let default_msg = Json::String("Unknown error".to_string());
            let err_msg = try_opt!(
                error.get("message").unwrap_or(&default_msg).as_string(),
                ErrorStatus::UnknownError,
                "Error message was not a string");
            return Err(WebDriverError::new(status,
                                           &format!("Marionette Error: {}", err_msg)[..]));
        }

        try!(self.update(message, &json_data));

        Ok(match message.command {
            //Everything that doesn't have a response value
            Get(_) | GoBack | GoForward | Refresh | Close | SetTimeouts(_) |
            SetWindowSize(_) | MaximizeWindow | SwitchToWindow(_) | SwitchToFrame(_) |
            SwitchToParentFrame | AddCookie(_) | DismissAlert | AcceptAlert |
            SendAlertText(_) | ElementClick(_) | ElementTap(_) | ElementClear(_) |
            ElementSendKeys(_, _) => {
                WebDriverResponse::Void
            },
            //Things that simply return the contents of the marionette "value" property
            GetCurrentUrl | GetTitle | GetWindowHandle | GetWindowHandles |
            IsDisplayed(_) | IsSelected(_) |
            GetElementAttribute(_, _) | GetCSSValue(_, _) | GetElementText(_) |
            GetElementTagName(_) | IsEnabled(_) | ExecuteScript(_) | ExecuteAsyncScript(_) |
            GetAlertText | TakeScreenshot(_) => {
                let value = try_opt!(json_data.get("value"),
                                     ErrorStatus::UnknownError,
                                     "Failed to find value field");
                //TODO: Convert webelement keys
                WebDriverResponse::Generic(ValueResponse::new(value.clone()))
            },
            GetWindowSize => {
                let value = try_opt!(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::UnknownError,
                             "Failed to find value field").as_object(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret value as object");

                let width = try_opt!(
                    try_opt!(value.get("width"),
                             ErrorStatus::UnknownError,
                             "Failed to find width field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as integer");

                let height = try_opt!(
                    try_opt!(value.get("height"),
                             ErrorStatus::UnknownError,
                             "Failed to find height field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as integer");

                WebDriverResponse::WindowSize(WindowSizeResponse::new(width, height))
            },
            GetElementRect(_) => {
                let value = try_opt!(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::UnknownError,
                             "Failed to find value field").as_object(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret value as object");

                let x = try_opt!(
                    try_opt!(value.get("x"),
                             ErrorStatus::UnknownError,
                             "Failed to find x field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret x as integer");

                let y = try_opt!(
                    try_opt!(value.get("y"),
                             ErrorStatus::UnknownError,
                             "Failed to find y field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret y as integer");

                let width = try_opt!(
                    try_opt!(value.get("width"),
                             ErrorStatus::UnknownError,
                             "Failed to find width field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as integer");

                let height = try_opt!(
                    try_opt!(value.get("height"),
                             ErrorStatus::UnknownError,
                             "Failed to find height field").as_u64(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret width as integer");

                WebDriverResponse::ElementRect(ElementRectResponse::new(x, y, width, height))
            },
            GetCookie => {
                let value = try_opt!(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::UnknownError,
                             "Failed to find value field").as_array(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret value as array");
                let cookies = try!(value.iter().map(|x| {
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
                        Nullable::from_json(try_opt!(x.find("path"),
                                                     ErrorStatus::UnknownError,
                                                     "Failed to find path field"),
                                            |x| {
                                                Ok((try_opt!(x.as_string(),
                                                             ErrorStatus::UnknownError,
                                                             "Failed to interpret path as String")).to_string())
                                            }));
                    let domain = try!(
                        Nullable::from_json(try_opt!(x.find("domain"),
                                                     ErrorStatus::UnknownError,
                                                     "Failed to find domain field"),
                                            |x| {
                                                Ok((try_opt!(x.as_string(),
                                                             ErrorStatus::UnknownError,
                                                             "Failed to interpret domain as String")).to_string())
                                            }));
                    let expiry = try!(
                        Nullable::from_json(try_opt!(x.find("expiry"),
                                                     ErrorStatus::UnknownError,
                                                     "Failed to find expiry field"),
                                            |x| {
                                                Ok(Date::new((try_opt!(
                                                    x.as_u64(),
                                                    ErrorStatus::UnknownError,
                                                    "Failed to interpret domain as String"))))
                                            }));
                    let max_age = Date::new(try_opt!(
                        try_opt!(x.find("maxAge"),
                                 ErrorStatus::UnknownError,
                                 "Failed to find maxAge field").as_u64(),
                        ErrorStatus::UnknownError,
                        "Failed to interpret maxAge as u64"));
                    let secure = match x.find("secure") {
                        Some(x) => try_opt!(x.as_boolean(),
                                            ErrorStatus::UnknownError,
                                            "Failed to interpret secure as boolean"),
                        None => false
                    };
                    let http_only = match x.find("httpOnly") {
                        Some(x) => try_opt!(x.as_boolean(),
                                            ErrorStatus::UnknownError,
                                            "Failed to interpret http_only as boolean"),
                        None => false
                    };
                    Ok(Cookie::new(name, value, path, domain, expiry, max_age, secure, http_only))
                }).collect::<Result<Vec<_>, _>>());
                WebDriverResponse::Cookie(CookieResponse::new(cookies))
            },
            FindElement(_) | FindElementElement(_, _) => {
                let element = try!(self.to_web_element(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::UnknownError,
                             "Failed to find value field")));
                WebDriverResponse::Generic(ValueResponse::new(element.to_json()))
            },
            FindElements(_) | FindElementElements(_, _) => {
                let element_vec = try_opt!(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::UnknownError,
                             "Failed to find value field").as_array(),
                    ErrorStatus::UnknownError,
                    "Failed to interpret value as array");
                let elements = try!(element_vec.iter().map(
                    |x| {
                        self.to_web_element(x)
                    }).collect::<Result<Vec<_>, _>>());
                WebDriverResponse::Generic(ValueResponse::new(
                    Json::Array(elements.iter().map(|x| {x.to_json()}).collect())))
            },
            NewSession => {
                let session_id = try_opt!(
                    try_opt!(json_data.get("sessionId"),
                             ErrorStatus::InvalidSessionId,
                             "Failed to find sessionId field").as_string(),
                    ErrorStatus::InvalidSessionId,
                    "sessionId was not a string").to_string();

                let value = try_opt!(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::SessionNotCreated,
                             "Failed to find value field").as_object(),
                    ErrorStatus::SessionNotCreated,
                    "value field was not an Object");

                WebDriverResponse::NewSession(NewSessionResponse::new(
                    session_id, Json::Object(value.clone())))
            }
            DeleteSession => {
                WebDriverResponse::DeleteSession
            }
        })
    }

    pub fn error_from_code(&self, error_code: u64) -> ErrorStatus {
        match error_code {
            7 => ErrorStatus::NoSuchElement,
            8 => ErrorStatus::NoSuchFrame,
            9 => ErrorStatus::UnsupportedOperation,
            10 => ErrorStatus::StaleElementReference,
            11 => ErrorStatus::ElementNotVisible,
            12 => ErrorStatus::InvalidElementState,
            15 => ErrorStatus::ElementNotSelectable,
            17 => ErrorStatus::JavascriptError,
            21 => ErrorStatus::Timeout,
            23 => ErrorStatus::NoSuchWindow,
            24 => ErrorStatus::InvalidCookieDomain,
            25 => ErrorStatus::UnableToSetCookie,
            26 => ErrorStatus::UnexpectedAlertOpen,
            27 => ErrorStatus::NoSuchAlert,
            28 => ErrorStatus::ScriptTimeout,
            29 => ErrorStatus::InvalidElementCoordinates,
            32 => ErrorStatus::InvalidSelector,
            34 => ErrorStatus::MoveTargetOutOfBounds,
            405 => ErrorStatus::UnsupportedOperation,
            // Explicitly list all known marionette error codes
            13 | 19 | 51 | 52 | 53 | 54 | 55 | 56 | 500 | _ => ErrorStatus::UnknownError

        }
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

    pub fn connect(&mut self) -> Result<(), IoError> {
        let mut timer = try!(Timer::new());
        let timeout = 60 * 1000;
        let poll_interval = Duration::milliseconds(100);
        let poll_attempts = timeout / poll_interval.num_milliseconds();
        let mut poll_attempt = 0;
        loop {
            match TcpStream::connect(("127.0.0.1", self.port)) {
                Ok(stream) => {
                    self.stream = Some(stream);
                    break
                },
                Err(e) => {
                    debug!("{}/{}", poll_attempt, poll_attempts);
                    if poll_attempt <= poll_attempts {
                        poll_attempt += 1;
                        timer.sleep(poll_interval);
                    } else {
                        return Err(e);
                    }
                }
            }
        };

        debug!("TCP stream open");

        self.handshake()
    }

    fn handshake(&mut self) -> Result<(), IoError> {
        try!(self.read_resp());

        //Would get traits and application type here
        let mut msg = BTreeMap::new();
        msg.insert("name".to_string(), "getMarionetteID".to_json());
        msg.insert("to".to_string(), "root".to_json());
        match self.send(&msg.to_json()) {
            Ok(resp) => {
                let json_data = object_from_json(&resp[..]).ok().expect("Failed to connect to marionette");
                match json_data.get(&"id".to_string()) {
                    Some(x) => match x.as_string() {
                        Some(id) => self.session.to = id.to_string(),
                        None => panic!("Failed to connect to marionette")
                    },
                    None => panic!("Failed to connect to marionette")
                };
                Ok(())
            }
            Err(_) => panic!("Failed to connect to marionette")
        }
    }

    pub fn close(&self) {
    }

    fn encode_msg(&self, msg:&Json) -> String {
        let data = json::encode(msg).unwrap();
        format!("{}:{}", data.len(), data)
    }

    pub fn send_message(&mut self, msg: &WebDriverMessage) -> WebDriverResult<WebDriverResponse>  {
        let resp = try!(self.session.msg_to_marionette(msg));
        let resp_data = try!(self.send(&resp));
        self.session.response_from_json(msg, &resp_data[..])
    }

    fn send(&mut self, msg: &Json) -> WebDriverResult<String> {
        let data = self.encode_msg(msg);
        debug!("Sending {}", data);
        match self.stream {
            Some(ref mut stream) => {
                if stream.write_str(&data[..]).is_err() {
                    return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                   "Failed to write response to stream"))
                }
            },
            None => {
                return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                               "Tried to write before opening stream"))
            }
        }
        match self.read_resp() {
            Ok(resp) => {
                debug!("Marionette response {}", resp);
                Ok(resp)
            },
            Err(_) => Err(WebDriverError::new(ErrorStatus::UnknownError,
                                              "Failed to decode response from marionette"))
        }
    }

    fn read_resp(&mut self) -> Result<String, IoError> {
        let mut bytes = 0usize;
        //TODO: Check before we unwrap?
        let mut stream = self.stream.as_mut().unwrap();
        loop {
            let byte = try!(stream.read_byte()) as char;
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
        let data = try!(stream.read_exact(bytes));
        //Need to handle the error here
        Ok(String::from_utf8(data).unwrap())
    }
}

trait ToMarionette {
    fn to_marionette(&self) -> WebDriverResult<Json>;
}

impl ToMarionette for WebDriverMessage {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let (opt_name, opt_parameters) = match self.command {
            NewSession => (Some("newSession"), None),
            DeleteSession => (Some("deleteSession"), None),
            Get(ref x) => (Some("get"), Some(x.to_marionette())),
            GetCurrentUrl => (Some("getCurrentUrl"), None),
            GoBack => (Some("goBack"), None),
            GoForward => (Some("goForward"), None),
            Refresh => (Some("refresh"), None),
            GetTitle => (Some("getTitle"), None),
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
                let body = x.to_marionette();
                if body.is_err() {
                    return body;
                }

                let mut data = try_opt!(body.unwrap().as_object(),
                                        ErrorStatus::UnknownError,
                                        "Marionette response was not an object").clone();
                data.insert("element".to_string(), e.id.to_json());
                (Some("findElement"), Some(Ok(Json::Object(data.clone()))))
            },
            FindElementElements(ref e, ref x) => {
                let body = x.to_marionette();
                if body.is_err() {
                    return body;
                }

                let mut data = try_opt!(body.unwrap().as_object(),
                                        ErrorStatus::UnknownError,
                                        "Marionette response was not an object").clone();
                data.insert("element".to_string(), e.id.to_json());
                (Some("findElements"), Some(Ok(Json::Object(data.clone()))))
            },
            IsDisplayed(ref x) => (Some("isElementDisplayed"), Some(x.to_marionette())),
            IsSelected(ref x) => (Some("isElementSelected"), Some(x.to_marionette())),
            GetElementAttribute(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("name".to_string(), x.to_json());
                (Some("getElementAttribute"), Some(Ok(Json::Object(data))))
            },
            GetCSSValue(ref e, ref x) => {
                let mut data = BTreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("name".to_string(), x.to_json());
                (Some("getElementValueOfCSSProperty"), Some(Ok(Json::Object(data))))
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
                (Some("sendKeysToElement"), Some(Ok(Json::Object(data))))
            },
            ExecuteScript(ref x) => (Some("executeScript"), Some(x.to_marionette())),
            ExecuteAsyncScript(ref x) => (Some("executeAsyncScript"), Some(x.to_marionette())),
            GetCookie => (Some("getCookies"), None),
            AddCookie(ref x) => (Some("addCookie"), Some(x.to_marionette())),
            DismissAlert => (Some("dismissDialog"), None),
            AcceptAlert => (Some("acceptDialog"), None),
            GetAlertText => (Some("getTextFromDialog"), None),
            SendAlertText(ref x) => {
                let mut data = BTreeMap::new();
                data.insert("value".to_string(), x.to_json());
                (Some("sendKeysToDialog"), Some(Ok(Json::Object(data))))
            },
            TakeScreenshot(ref x) => (Some("takeScreenshot"), Some(x.to_marionette())),
        };

        let name = try_opt!(opt_name,
                            ErrorStatus::UnsupportedOperation,
                            "Operation not supported");

        let parameters = try!(opt_parameters.unwrap_or(Ok(Json::Object(BTreeMap::new()))));

        let mut data = BTreeMap::new();
        data.insert("name".to_string(), name.to_json());
        data.insert("parameters".to_string(), parameters.to_json());
        match self.session_id {
            Some(ref x) => data.insert("sessionId".to_string(), x.to_json()),
            None => None
        };
        Ok(Json::Object(data))
    }
}

impl ToMarionette for GetParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for TimeoutsParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for WindowSizeParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for SwitchToWindowParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = BTreeMap::new();
        data.insert("name".to_string(), self.handle.to_json());
        Ok(Json::Object(data))
    }
}

impl ToMarionette for LocatorParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for SwitchToFrameParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = BTreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        Ok(Json::Object(data))
    }
}

impl ToMarionette for JavascriptCommandParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = self.to_json().as_object().unwrap().clone();
        data.insert("newSandbox".to_string(), false.to_json());
        data.insert("specialPowers".to_string(), false.to_json());
        data.insert("scriptTimeout".to_string(), Json::Null);
        Ok(Json::Object(data))
    }
}

impl ToMarionette for GetCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for AddCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
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
        if self.maxAge.is_value() {
            cookie.insert("maxAge".to_string(), self.maxAge.to_json());
        }
        cookie.insert("secure".to_string(), self.secure.to_json());
        cookie.insert("httpOnly".to_string(), self.httpOnly.to_json());
        let mut data = BTreeMap::new();
        data.insert("cookie".to_string(), Json::Object(cookie));
        Ok(Json::Object(data))
    }
}

impl ToMarionette for TakeScreenshotParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = BTreeMap::new();
        let element = match self.element {
            Nullable::Null => Json::Null,
            Nullable::Value(ref x) => try!(x.to_marionette())
        };
        data.insert("element".to_string(), element);
        Ok(Json::Object(data))
    }
}

impl ToMarionette for WebElement {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = BTreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        Ok(Json::Object(data))
    }
}

impl<T: ToJson> ToMarionette for Nullable<T> {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        //Note this is a terrible hack. We don't want Nullable<T: ToJson+ToMarionette>
        //so in cases where ToJson != ToMarionette you have to deal with the Nullable
        //explicitly. This kind of suggests that the whole design is wrong.
        Ok(self.to_json())
    }
}

impl ToMarionette for FrameId {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        match *self {
            FrameId::Short(x) => Ok(x.to_json()),
            FrameId::Element(ref x) => Ok(try!(x.to_marionette())),
            FrameId::Null => Ok(Json::Null)
        }
    }
}
