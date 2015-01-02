use serialize::json::{Json, ToJson};
use serialize::json;
use std::collections::TreeMap;
use std::io::{IoResult, TcpStream, IoError};

use command::{WebDriverMessage};
use command::WebDriverCommand::{GetMarionetteId, NewSession, DeleteSession, Get, GetCurrentUrl,
                                GoBack, GoForward, Refresh, GetTitle, GetWindowHandle,
                                GetWindowHandles, Close, Timeouts, SetWindowSize,
                                GetWindowSize, MaximizeWindow, SwitchToWindow, SwitchToFrame,
                                SwitchToParentFrame, IsDisplayed, IsSelected,
                                GetElementAttribute, GetCSSValue, GetElementText,
                                GetElementTagName, GetElementRect, IsEnabled, ExecuteScript,
                                ExecuteAsyncScript};
use response::{WebDriverResponse, NewSessionResponse, ValueResponse, WindowSizeResponse, ElementRectResponse};
use common::{WebDriverResult, WebDriverError, ErrorStatus};

pub struct MarionetteSession {
    pub session_id: String,
    pub to: String
}

fn object_from_json(data: &str) -> WebDriverResult<TreeMap<String, json::Json>> {
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

    pub fn update(&mut self, msg: &WebDriverMessage, resp: &TreeMap<String, json::Json>) -> WebDriverResult<()> {
        match msg.command {
            GetMarionetteId => {
                let to = try_opt!(
                    try_opt!(resp.get("to"),
                             ErrorStatus::UnknownError,
                             "Unable to get to value").as_string(),
                    ErrorStatus::UnknownError,
                    "Unable to convert 'to' to a string");

                self.to = to.to_string();
            },
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

    fn command_name(msg:&WebDriverMessage) -> String {
        match msg.command {
            GetMarionetteId => "getMarionetteID",
            NewSession => "newSession",
            DeleteSession => "deleteSession",
            Get(_) => "get",
            GetCurrentUrl => "getCurrentUrl",
            GoBack => "goBack",
            GoForward => "goForward",
            Refresh => "refresh",
            GetTitle => "getTitle",
            GetWindowHandle => "getWindowHandle",
            GetWindowHandles => "getWindowHandles",
            Close => "close",
            Timeouts(_) => "timeouts",
            SetWindowSize(_) => "setWindowSize",
            GetWindowSize => "getWindowSize",
            MaximizeWindow => "maximizeWindow",
            SwitchToWindow(_) => "switchToWindow",
            SwitchToFrame(_) => "switchToFrame",
            SwitchToParentFrame => "switchToParentFrame",
            IsDisplayed(_) => "isElementDisplayed",
            IsSelected(_) => "isElementSelected",
            GetElementAttribute(_, _) => "getElementAttribute",
            GetCSSValue(_, _) => "getElementValueOfCssProperty",
            GetElementText(_) => "getElementText",
            GetElementTagName(_) => "getElementTagName",
            GetElementRect(_) => "getElementRect",
            IsEnabled(_) => "isElementEnabled",
            ExecuteScript(_) => "executeScript",
            ExecuteAsyncScript(_) => "executeAsyncScript"
        }.to_string()
    }

    pub fn msg_to_marionette(&self, msg: &WebDriverMessage) -> json::Json {
        let mut data = msg.to_json().as_object().unwrap().clone();
        match msg.session_id {
            Some(ref x) => data.insert("sessionId".to_string(), x.to_json()),
            None => None
        };
        data.insert("to".to_string(), self.to.to_json());
        data.insert("name".to_string(), MarionetteSession::command_name(msg).to_json());
        json::Object(data)
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

        self.update(message, &json_data);

        match message.command {
            //Everything that doesn't have a response value
            GetMarionetteId => Ok(None),
            Get(_) | GoBack | GoForward | Refresh | Close | Timeouts(_) |
            SetWindowSize(_) | MaximizeWindow | SwitchToWindow(_) | SwitchToFrame(_) |
            SwitchToParentFrame => {
                Ok(Some(WebDriverResponse::Void))
            },
            //Things that simply return the contents of the marionette "value" property
            GetCurrentUrl | GetTitle | GetWindowHandle | GetWindowHandles | IsDisplayed(_) |
            IsSelected(_) | GetElementAttribute(_, _) | GetCSSValue(_, _) | GetElementText(_) |
            GetElementTagName(_) | IsEnabled(_) | ExecuteScript(_) | ExecuteAsyncScript(_) => {
                let value = try_opt!(json_data.get("value"),
                                     ErrorStatus::UnknownError,
                                     "Failed to find value field");
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
            }
            NewSession => {
                let session_id = try_opt!(
                    try_opt!(json_data.get("sessionId"),
                             ErrorStatus::InvalidSessionId,
                             "Failed to find sessionId field").as_string(),
                    ErrorStatus::InvalidSessionId,
                    "sessionId was not a string");

                let value = try_opt!(
                    try_opt!(json_data.get("value"),
                             ErrorStatus::SessionNotCreated,
                             "Failed to find value field").as_object(),
                    ErrorStatus::SessionNotCreated,
                    "value field was not an Object");

                Ok(Some(WebDriverResponse::NewSession(NewSessionResponse::new(
                    session_id.to_string(), json::Object(value.clone())))))
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

    fn encode_msg(&self, msg:&json::Json) -> String {
        let data = json::encode(msg);
        let len = data.len().to_string();
        let mut message = len;
        message.push_str(":");
        message.push_str(data.as_slice());
        message
    }

    pub fn send_message(&mut self, msg: &WebDriverMessage) -> WebDriverResult<Option<WebDriverResponse>>  {
        let resp = {
            self.session.msg_to_marionette(msg)
        };
        let resp = match self.send(&resp) {
            Ok(resp_data) => self.session.response_from_json(msg, resp_data[]),
            Err(x) => Err(x)
        };
        resp
    }

    fn send(&mut self, msg: &json::Json) -> WebDriverResult<String> {
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
