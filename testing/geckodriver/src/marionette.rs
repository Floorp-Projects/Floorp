use serialize::json::{Json, ToJson};
use serialize::json;
use std::collections::TreeMap;
use std::io::{IoResult, TcpStream, IoError};

use command::{WebDriverMessage};
use command::WebDriverCommand::{NewSession, DeleteSession, Get, GetCurrentUrl,
                                GoBack, GoForward, Refresh, GetTitle, GetWindowHandle,
                                GetWindowHandles, Close, SetWindowSize,
                                GetWindowSize, MaximizeWindow, SwitchToWindow, SwitchToFrame,
                                SwitchToParentFrame, FindElement, FindElements, IsDisplayed,
                                IsSelected, GetElementAttribute, GetCSSValue, GetElementText,
                                GetElementTagName, GetElementRect, IsEnabled, ElementClick,
                                ElementTap, ElementClear, ElementSendKeys, ExecuteScript,
                                ExecuteAsyncScript, GetCookie, AddCookie, SetTimeouts,
                                DismissAlert, AcceptAlert, GetAlertText, SendAlertText,
                                TakeScreenshot};
use command::{GetParameters, WindowSizeParameters, SwitchToWindowParameters,
              SwitchToFrameParameters, LocatorParameters, JavascriptCommandParameters,
              GetCookieParameters, AddCookieParameters, TimeoutsParameters,
              TakeScreenshotParameters};
use response::{WebDriverResponse, NewSessionResponse, ValueResponse, WindowSizeResponse,
               ElementRectResponse, CookieResponse, Date, Cookie};
use common::{WebDriverResult, WebDriverError, ErrorStatus, Nullable, WebElement, FrameId};

pub struct MarionetteSession {
    pub session_id: String,
    pub to: String
}

fn object_from_json(data: &str) -> WebDriverResult<TreeMap<String, Json>> {
    Ok(try_opt!(try!(json::from_str(data)).as_object(),
                ErrorStatus::UnknownError,
                "Expected a json object").clone())
}

impl MarionetteSession {
    pub fn new(session_id: Option<String>) -> MarionetteSession {
        let initital_id = session_id.unwrap_or("".to_string());
        MarionetteSession {
            session_id: initital_id,
            to: String::from_str("root")
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

    pub fn update(&mut self, msg: &WebDriverMessage, resp: &TreeMap<String, Json>) -> WebDriverResult<()> {
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

    pub fn response_from_json(&mut self, message: &WebDriverMessage,
                              data: &str) -> WebDriverResult<Option<WebDriverResponse>> {
        let json_data = try!(object_from_json(data));
        if json_data.contains_key(&"error".to_string()) {
            //TODO: convert the marionette error into the right webdriver error
            let error = try_opt!(json_data.get("error").unwrap().as_object(),
                                 ErrorStatus::UnknownError,
                                 "Marionette error field was not an object");
            let status_code = try_opt!(
                try_opt!(error.get("status"),
                         ErrorStatus::UnknownError,
                         "Error dict doesn't have a status field").as_u64(),
                ErrorStatus::UnknownError,
                "Error status isn't an integer");
            let status = self.error_from_code(status_code);
            let default_msg = Json::String("Unknown error".into_string());
            let err_msg = try_opt!(
                error.get("message").unwrap_or(&default_msg).as_string(),
                ErrorStatus::UnknownError,
                "Error message was not a string");
            return Err(WebDriverError::new(status, err_msg));
        }

        try!(self.update(message, &json_data));

        match message.command {
            //Everything that doesn't have a response value
            Get(_) | GoBack | GoForward | Refresh | Close | SetTimeouts(_) |
            SetWindowSize(_) | MaximizeWindow | SwitchToWindow(_) | SwitchToFrame(_) |
            SwitchToParentFrame | AddCookie(_) | DismissAlert | AcceptAlert |
            SendAlertText(_) | ElementClick(_) | ElementTap(_) | ElementClear(_) |
            ElementSendKeys(_, _) => {
                Ok(Some(WebDriverResponse::Void))
            },
            //Things that simply return the contents of the marionette "value" property
            GetCurrentUrl | GetTitle | GetWindowHandle | GetWindowHandles |
            FindElement(_) | FindElements(_) | IsDisplayed(_) | IsSelected(_) |
            GetElementAttribute(_, _) | GetCSSValue(_, _) | GetElementText(_) |
            GetElementTagName(_) | IsEnabled(_) | ExecuteScript(_) | ExecuteAsyncScript(_) |
            GetAlertText | TakeScreenshot(_) => {
                let value = try_opt!(json_data.get("value"),
                                     ErrorStatus::UnknownError,
                                     "Failed to find value field");
                //TODO: Convert webelement keys
                Ok(Some(WebDriverResponse::Generic(ValueResponse::new(value.clone()))))
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

                Ok(Some(WebDriverResponse::WindowSize(WindowSizeResponse::new(width, height))))
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

                Ok(Some(WebDriverResponse::ElementRect(ElementRectResponse::new(x, y, width, height))))
            },
            GetCookie(_) => {
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
                        "Failed to interpret name as string").into_string();
                    let value = try_opt!(
                        try_opt!(x.find("value"),
                                 ErrorStatus::UnknownError,
                                 "Failed to find value field").as_string(),
                        ErrorStatus::UnknownError,
                        "Failed to interpret value as string").into_string();
                    let path = try!(
                        Nullable::from_json(try_opt!(x.find("path"),
                                                     ErrorStatus::UnknownError,
                                                     "Failed to find path field"),
                                            |x| {
                                                Ok((try_opt!(x.as_string(),
                                                             ErrorStatus::UnknownError,
                                                             "Failed to interpret path as String")).into_string())
                                            }));
                    let domain = try!(
                        Nullable::from_json(try_opt!(x.find("domain"),
                                                     ErrorStatus::UnknownError,
                                                     "Failed to find domain field"),
                                            |x| {
                                                Ok((try_opt!(x.as_string(),
                                                             ErrorStatus::UnknownError,
                                                             "Failed to interpret domain as String")).into_string())
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
                Ok(Some(WebDriverResponse::Cookie(CookieResponse::new(cookies))))
            },
            NewSession => {
                let session_id = try_opt!(
                    try_opt!(json_data.get("sessionId"),
                             ErrorStatus::InvalidSessionId,
                             "Failed to find sessionId field").as_string(),
                    ErrorStatus::InvalidSessionId,
                    "sessionId was not a string").into_string();

                let value = try_opt!(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::SessionNotCreated,
                             "Failed to find value field").as_object(),
                    ErrorStatus::SessionNotCreated,
                    "value field was not an Object");

                Ok(Some(WebDriverResponse::NewSession(NewSessionResponse::new(
                    session_id, json::Object(value.clone())))))
            }
            DeleteSession => {
                Ok(Some(WebDriverResponse::DeleteSession))
            }
        }
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
            13 | 19 | 51 | 52 | 53 | 54 | 55 | 56 | 500 | _ => ErrorStatus::UnknownError

        }
    }
}

pub struct MarionetteConnection {
    stream: IoResult<TcpStream>,
    pub session: MarionetteSession
}

impl MarionetteConnection {
    pub fn new(session_id: Option<String>) -> MarionetteConnection {
        let stream = TcpStream::connect("127.0.0.1:2828");
        MarionetteConnection {
            stream: stream,
            session: MarionetteSession::new(session_id)
        }
    }

    pub fn connect(&mut self) -> Result<(), IoError> {
        try!(self.read_resp());
        //Would get traits and application type here
        let mut msg = TreeMap::new();
        msg.insert("name".to_string(), "getMarionetteID".to_json());
        msg.insert("to".to_string(), "root".to_json());
        match self.send(&msg.to_json()) {
            Ok(resp) => {
                let json_data = match object_from_json(resp.as_slice()) {
                    Ok(x) => x,
                    Err(_) => panic!("Failed to connect to marionette")
                };
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

    fn encode_msg(&self, msg:&Json) -> String {
        let data = json::encode(msg);
        let len = data.len().to_string();
        let mut message = len;
        message.push_str(":");
        message.push_str(data.as_slice());
        message
    }

    pub fn send_message(&mut self, msg: &WebDriverMessage) -> WebDriverResult<Option<WebDriverResponse>>  {
        let resp = try!(self.session.msg_to_marionette(msg));
        let resp = match self.send(&resp) {
            Ok(resp_data) => self.session.response_from_json(msg, resp_data[]),
            Err(x) => Err(x)
        };
        resp
    }

    fn send(&mut self, msg: &Json) -> WebDriverResult<String> {
        let data = self.encode_msg(msg);
        debug!("Sending {}", data);
        match self.stream.write_str(data.as_slice()) {
            Ok(_) => {},
            Err(_) => {
                return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                               "Failed to write response to stream"))
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
        let mut bytes = 0 as uint;
        loop {
            let byte = try!(self.stream.read_byte()) as char;
            match byte {
                '0'...'9' => {
                    bytes = bytes * 10;
                    bytes += byte as uint - '0' as uint;
                },
                ':' => {
                    break
                }
                _ => {}
            }
        }
        let data = try!(self.stream.read_exact(bytes));
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
            IsDisplayed(ref x) => (Some("isElementDisplayed"), Some(x.to_marionette())),
            IsSelected(ref x) => (Some("isElementSelected"), Some(x.to_marionette())),
            GetElementAttribute(ref e, ref x) => {
                let mut data = TreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("name".to_string(), x.to_json());
                (Some("getElementAttribute"), Some(Ok(Json::Object(data))))
            },
            GetCSSValue(ref e, ref x) => {
                let mut data = TreeMap::new();
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
                let mut data = TreeMap::new();
                data.insert("id".to_string(), e.id.to_json());
                data.insert("value".to_string(), x.value.to_json());
                (Some("sendKeysToElement"), Some(Ok(Json::Object(data))))
            },
            ExecuteScript(ref x) => (Some("executeScript"), Some(x.to_marionette())),
            ExecuteAsyncScript(ref x) => (Some("executeAsyncScript"), Some(x.to_marionette())),
            GetCookie(ref x) => (Some("getCookies"), Some(x.to_marionette())),
            AddCookie(ref x) => (Some("addCookie"), Some(x.to_marionette())),
            DismissAlert => (None, None), //Unsupported
            AcceptAlert => (None, None), //Unsupported
            GetAlertText => (None, None), //Unsupported
            SendAlertText(ref x) => (None, None), //Unsupported
            TakeScreenshot(ref x) => (Some("takeScreenshot"), Some(x.to_marionette())),
        };

        let name = try_opt!(opt_name,
                            ErrorStatus::UnsupportedOperation,
                            "Operation not supported");

        let parameters = try!(opt_parameters.unwrap_or(Ok(Json::Object(TreeMap::new()))));

        let mut data = TreeMap::new();
        data.insert("name".to_string(), name.to_json());
        data.insert("parameters".to_string(), parameters.to_json());
        match self.session_id {
            Some(ref x) => data.insert("sessionId".to_string(), x.to_json()),
            None => None
        };
        Ok(json::Object(data))
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
        Ok(self.to_json())
    }
}

impl ToMarionette for LocatorParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for SwitchToFrameParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = TreeMap::new();
        data.insert("id".to_string(), try!(self.id.to_marionette()));
        Ok(json::Object(data))
    }
}

impl ToMarionette for JavascriptCommandParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for GetCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        Ok(self.to_json())
    }
}

impl ToMarionette for AddCookieParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut cookie = TreeMap::new();
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
        let mut data = TreeMap::new();
        data.insert("cookie".into_string(), Json::Object(cookie));
        Ok(json::Object(data))
    }
}

impl ToMarionette for TakeScreenshotParameters {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = TreeMap::new();
        let element = match self.element {
            Nullable::Null => Json::Null,
            Nullable::Value(ref x) => try!(x.to_marionette())
        };
        data.insert("element".into_string(), element);
        Ok(Json::Object(data))
    }
}

impl ToMarionette for WebElement {
    fn to_marionette(&self) -> WebDriverResult<Json> {
        let mut data = TreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        Ok(json::Object(data))
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
