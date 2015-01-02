use core::u16;
use std::collections::TreeMap;
use serialize::json;
use serialize::json::{ToJson, Json};
use regex::Captures;

use common::{WebDriverResult, WebDriverError, ErrorStatus};
use messagebuilder::MatchType;


#[deriving(PartialEq)]
pub enum WebDriverCommand {
    GetMarionetteId, //TODO: move this
    NewSession,
    DeleteSession,
    Get(GetParameters),
    GetCurrentUrl,
    GoBack,
    GoForward,
    Refresh,
    GetTitle,
    GetWindowHandle,
    GetWindowHandles,
    Close,
    Timeouts(TimeoutsParameters),
    SetWindowSize(WindowSizeParameters),
    GetWindowSize,
    MaximizeWindow,
//    FullscreenWindow // Not supported in marionette
    SwitchToWindow(SwitchToWindowParameters),
    SwitchToFrame(SwitchToFrameParameters),
    SwitchToParentFrame,
    IsDisplayed(WebElement),
    IsSelected(WebElement),
    GetElementAttribute(WebElement, String),
    GetCSSValue(WebElement, String),
    GetElementText(WebElement),
    GetElementTagName(WebElement),
    GetElementRect(WebElement),
    IsEnabled(WebElement),
    ExecuteScript(JavascriptCommandParameters),
    ExecuteAsyncScript(JavascriptCommandParameters)
}

#[deriving(PartialEq)]
pub struct WebDriverMessage {
    pub session_id: Option<String>,
    pub command: WebDriverCommand
}

impl WebDriverMessage {
    pub fn new(session_id: Option<String>, command: WebDriverCommand) -> WebDriverMessage {
        WebDriverMessage {
            session_id: session_id,
            command: command
        }
    }

    pub fn from_http(match_type: MatchType, params: &Captures, body: &str) -> WebDriverResult<WebDriverMessage> {
        let session_id = WebDriverMessage::get_session_id(params);
        let body_data = if body != "" {
            debug!("Got request body {}", body);
            match json::from_str(body) {
                Ok(x) => x,
                Err(_) => return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                         format!("Failed to decode request body as json: {}", body).as_slice()))
            }
        } else {
            json::Null
        };
        let command = match match_type {
            MatchType::NewSession => WebDriverCommand::NewSession,
            MatchType::DeleteSession => WebDriverCommand::DeleteSession,
            MatchType::Get => {
                let parameters: GetParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::Get(parameters)
            },
            MatchType::GetCurrentUrl => WebDriverCommand::GetCurrentUrl,
            MatchType::GoBack => WebDriverCommand::GoBack,
            MatchType::GoForward => WebDriverCommand::GoForward,
            MatchType::Refresh => WebDriverCommand::Refresh,
            MatchType::GetTitle => WebDriverCommand::GetTitle,
            MatchType::GetWindowHandle => WebDriverCommand::GetWindowHandle,
            MatchType::GetWindowHandles => WebDriverCommand::GetWindowHandles,
            MatchType::Close => WebDriverCommand::Close,
            MatchType::Timeouts => {
                let parameters: TimeoutsParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::Timeouts(parameters)
            },
            MatchType::SetWindowSize => {
                let parameters: WindowSizeParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SetWindowSize(parameters)
            },
            MatchType::GetWindowSize => WebDriverCommand::GetWindowSize,
            MatchType::MaximizeWindow => WebDriverCommand::MaximizeWindow,
            MatchType::SwitchToWindow => {
                let parameters: SwitchToWindowParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SwitchToWindow(parameters)
            }
            MatchType::SwitchToFrame => {
                let parameters: SwitchToFrameParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SwitchToFrame(parameters)
            },
            MatchType::SwitchToParentFrame => WebDriverCommand::SwitchToParentFrame,
            MatchType::IsDisplayed => {
                let element = WebElement::new(params.name("elementId").to_string());
                WebDriverCommand::IsDisplayed(element)
            },
            MatchType::IsSelected => {
                let element = WebElement::new(params.name("elementId").to_string());
                WebDriverCommand::IsSelected(element)
            },
            MatchType::GetElementAttribute => {
                let element = WebElement::new(params.name("elementId").to_string());
                let attr = params.name("name").to_string();
                WebDriverCommand::GetElementAttribute(element, attr)
            },
            MatchType::GetCSSValue => {
                let element = WebElement::new(params.name("elementId").to_string());
                let property = params.name("propertyName").to_string();
                WebDriverCommand::GetCSSValue(element, property)
            },
            MatchType::GetElementText => {
                let element = WebElement::new(params.name("elementId").to_string());
                WebDriverCommand::GetElementText(element)
            },
            MatchType::GetElementTagName => {
                let element = WebElement::new(params.name("elementId").to_string());
                WebDriverCommand::GetElementTagName(element)
            },
            MatchType::GetElementRect => {
                let element = WebElement::new(params.name("elementId").to_string());
                WebDriverCommand::GetElementText(element)
            },
            MatchType::IsEnabled => {
                let element = WebElement::new(params.name("elementId").to_string());
                WebDriverCommand::IsEnabled(element)
            },
            MatchType::ExecuteScript => {
                let parameters: JavascriptCommandParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::ExecuteScript(parameters)
            }
            MatchType::ExecuteAsyncScript => {
                let parameters: JavascriptCommandParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::ExecuteAsyncScript(parameters)
            }
        };
        Ok(WebDriverMessage::new(session_id, command))
    }

    fn get_session_id(params: &Captures) -> Option<String> {
        match params.name("sessionId") {
            "" => None,
            x => Some(x.to_string())
        }
    }
}

impl ToJson for WebDriverMessage {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        let parameters = match self.command {
            WebDriverCommand::GetMarionetteId | WebDriverCommand::NewSession |
            WebDriverCommand::DeleteSession | WebDriverCommand::GetCurrentUrl |
            WebDriverCommand::GoBack | WebDriverCommand::GoForward | WebDriverCommand::Refresh |
            WebDriverCommand::GetTitle | WebDriverCommand::GetWindowHandle |
            WebDriverCommand::GetWindowHandles | WebDriverCommand::Close |
            WebDriverCommand::GetWindowSize | WebDriverCommand::MaximizeWindow |
            WebDriverCommand::SwitchToParentFrame | WebDriverCommand::IsDisplayed(_) |
            WebDriverCommand::IsSelected(_) | WebDriverCommand::GetElementAttribute(_, _) |
            WebDriverCommand::GetCSSValue(_, _) | WebDriverCommand::GetElementText(_) |
            WebDriverCommand::GetElementTagName(_) | WebDriverCommand::GetElementRect(_) |
            WebDriverCommand::IsEnabled(_) => {
                None
            },
            WebDriverCommand::Get(ref x) => Some(x.to_json()),
            WebDriverCommand::Timeouts(ref x) => Some(x.to_json()),
            WebDriverCommand::SetWindowSize(ref x) => Some(x.to_json()),
            WebDriverCommand::SwitchToWindow(ref x) => Some(x.to_json()),
            WebDriverCommand::SwitchToFrame(ref x) => Some(x.to_json()),
            WebDriverCommand::ExecuteScript(ref x) |
            WebDriverCommand::ExecuteAsyncScript(ref x) => Some(x.to_json())
        };
        if parameters.is_some() {
            data.insert("parameters".to_string(), parameters.unwrap());
        }
        json::Object(data)
    }
}

#[deriving(PartialEq)]
struct WebElement {
    id: String
}

impl WebElement {
    fn new(id: String) -> WebElement {
        WebElement {
            id: id
        }
    }
}

impl ToJson for WebElement {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("element-6066-11e4-a52e-4f735466cecf".to_string(), self.id.to_json());
        json::Object(data)
    }
}

#[deriving(PartialEq)]
enum FrameId {
    Short(u16),
    Element(WebElement),
    Null
}

impl ToJson for FrameId {
    fn to_json(&self) -> json::Json {
        match *self {
            FrameId::Short(x) => {
                json::Json::U64(x as u64)
            },
            FrameId::Element(ref x) => {
                json::Json::String(x.id.clone())
            },
            FrameId::Null => {
                json::Json::Null
            }
        }
    }
}

#[deriving(PartialEq, Clone)]
enum Nullable<T: ToJson> {
    Value(T),
    Null
}

impl<T: ToJson> Nullable<T> {
    //This is not very pretty
    fn from_json<F: FnOnce(&json::Json) -> WebDriverResult<T>>(value: &json::Json, f: F) -> WebDriverResult<Nullable<T>> {
        if value.is_null() {
            Ok(Nullable::Null)
        } else {
            Ok(Nullable::Value(try!(f(value))))
        }
    }
}

impl<T:ToJson> ToJson for Nullable<T> {
    fn to_json(&self) -> json::Json {
        match *self {
            Nullable::Value(ref x) => x.to_json(),
            Nullable::Null => json::Json::Null
        }
    }
}

trait Parameters {
    fn from_json(body: &json::Json) -> WebDriverResult<Self>;
}

#[deriving(PartialEq)]
struct GetParameters {
    url: String
}

impl Parameters for GetParameters {
    fn from_json(body: &json::Json) -> WebDriverResult<GetParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let url = try_opt!(
            try_opt!(data.get("url"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'url' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'url' not a string");
        return Ok(GetParameters {
            url: url.to_string()
        })
    }
}

impl ToJson for GetParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("url".to_string(), self.url.to_json());
        json::Object(data)
    }
}

#[deriving(PartialEq)]
struct TimeoutsParameters {
    type_: String,
    ms: u64
}

impl Parameters for TimeoutsParameters {
    fn from_json(body: &json::Json) -> WebDriverResult<TimeoutsParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let type_ = try_opt!(
            try_opt!(data.get("type"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'type' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'type' not a string");

        let ms = try_opt!(
            try_opt!(data.get("ms"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'ms' parameter").as_u64(),
            ErrorStatus::InvalidArgument,
            "'ms' not an integer");
        return Ok(TimeoutsParameters {
            type_: type_.to_string(),
            ms: ms
        })
    }
}

impl ToJson for TimeoutsParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("type".to_string(), self.type_.to_json());
        data.insert("ms".to_string(), self.ms.to_json());
        json::Object(data)
    }
}

#[deriving(PartialEq)]
struct WindowSizeParameters {
    width: u64,
    height: u64
}

impl Parameters for WindowSizeParameters {
    fn from_json(body: &json::Json) -> WebDriverResult<WindowSizeParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let height = try_opt!(
            try_opt!(data.get("height"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'height' parameter").as_u64(),
            ErrorStatus::InvalidArgument,
            "'height' is not a positive integer");
        let width = try_opt!(
            try_opt!(data.get("width"),
                     ErrorStatus::InvalidArgument,
                     "Missing width parameter").as_u64(),
            ErrorStatus::InvalidArgument,
            "'width' is not a positive integer");
        return Ok(WindowSizeParameters {
            height: height,
            width: width
        })
    }
}

impl ToJson for WindowSizeParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("width".to_string(), self.width.to_json());
        data.insert("height".to_string(), self.height.to_json());
        json::Object(data)
    }
}

#[deriving(PartialEq)]
struct SwitchToWindowParameters {
    handle: String
}

impl Parameters for SwitchToWindowParameters {
    fn from_json(body: &json::Json) -> WebDriverResult<SwitchToWindowParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let handle = try_opt!(
            try_opt!(data.get("handle"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'handle' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'handle' not a string");
        return Ok(SwitchToWindowParameters {
            handle: handle.to_string()
        })
    }
}

impl ToJson for SwitchToWindowParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("handle".to_string(), self.handle.to_json());
        json::Object(data)
    }
}

#[deriving(PartialEq)]
struct SwitchToFrameParameters {
    id: FrameId
}

impl Parameters for SwitchToFrameParameters {
    fn from_json(body: &json::Json) -> WebDriverResult<SwitchToFrameParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let id_json = try_opt!(data.get("id"),
                               ErrorStatus::UnknownError,
                               "Missing 'id' parameter");
        let id = if id_json.is_u64() {
            let value = id_json.as_u64().unwrap();
            if value <= u16::MAX as u64 {
                FrameId::Short(value as u16)
            } else {
                return Err(WebDriverError::new(ErrorStatus::NoSuchFrame,
                                               "frame id out of range"))
            }
        } else if id_json.is_null() {
            FrameId::Null
        } else if id_json.is_string() {
            let value = id_json.as_string().unwrap();
            FrameId::Element(WebElement::new(value.to_string()))
        } else {
            return Err(WebDriverError::new(ErrorStatus::NoSuchFrame,
                                           "frame id has unexpected type"))
        };
        Ok(SwitchToFrameParameters {
            id: id
        })
    }
}

impl ToJson for SwitchToFrameParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        json::Object(data)
    }
}

#[deriving(PartialEq)]
struct JavascriptCommandParameters {
    script: String,
    args: Nullable<Vec<json::Json>>
}

impl Parameters for JavascriptCommandParameters {
    fn from_json(body: &json::Json) -> WebDriverResult<JavascriptCommandParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::UnknownError,
                            "Message body was not an object");

        let args_json = try_opt!(data.get("args"),
                                 ErrorStatus::UnknownError,
                                 "Missing args parameter");

        let args = try!(Nullable::from_json(
            args_json,
            |x| {
                Ok((try_opt!(x.as_array(),
                             ErrorStatus::UnknownError,
                             "Failed to convert args to Array")).clone())
            }));

         //TODO: Look for WebElements in args?
        let script = try_opt!(
            try_opt!(data.get("script"),
                     ErrorStatus::UnknownError,
                     "Missing script parameter").as_string(),
            ErrorStatus::UnknownError,
            "Failed to convert script to String");
        Ok(JavascriptCommandParameters {
            script: script.to_string(),
            args: args.clone()
        })
    }
}

impl ToJson for JavascriptCommandParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        //TODO: Wrap script so that it becomes marionette-compatible
        data.insert("script".to_string(), self.script.to_json());
        data.insert("args".to_string(), self.args.to_json());
        data.insert("newSandbox".to_string(), false.to_json());
        data.insert("specialPowers".to_string(), false.to_json());
        data.insert("scriptTimeout".to_string(), json::Json::Null);
        json::Object(data)
    }
}
