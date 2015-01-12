use core::num::ToPrimitive;
use serialize::json::{Json, ToJson, ParserError};
use serialize::{json, Encodable, Encoder};
use std::collections::BTreeMap;
use std::error::{Error, FromError};

static ELEMENT_KEY: &'static str = "element-6066-11e4-a52e-4f735466cecf";

#[derive(PartialEq, Show)]
pub enum ErrorStatus {
    ElementNotSelectable,
    ElementNotVisible,
    InvalidArgument,
    InvalidCookieDomain,
    InvalidElementCoordinates,
    InvalidElementState,
    InvalidSelector,
    InvalidSessionId,
    JavascriptError,
    MoveTargetOutOfBounds,
    NoSuchAlert,
    NoSuchElement,
    NoSuchFrame,
    NoSuchWindow,
    ScriptTimeout,
    SessionNotCreated,
    StaleElementReference,
    Timeout,
    UnableToSetCookie,
    UnexpectedAlertOpen,
    UnknownError,
    UnknownPath,
    UnknownMethod,
    UnsupportedOperation,
}

pub type WebDriverResult<T> = Result<T, WebDriverError>;

#[derive(Show)]
pub struct WebDriverError {
    pub status: ErrorStatus,
    pub message: String
}

impl WebDriverError {
    pub fn new(status: ErrorStatus, message: &str) -> WebDriverError {
        WebDriverError {
            status: status,
            message: message.to_string().clone()
        }
    }

    pub fn status_code(&self) -> &'static str {
        match self.status {
            ErrorStatus::ElementNotSelectable => "element not selectable",
            ErrorStatus::ElementNotVisible => "element not visible",
            ErrorStatus::InvalidArgument => "invalid argument",
            ErrorStatus::InvalidCookieDomain => "invalid cookie domain",
            ErrorStatus::InvalidElementCoordinates => "invalid element coordinates",
            ErrorStatus::InvalidElementState => "invalid element state",
            ErrorStatus::InvalidSelector => "invalid selector",
            ErrorStatus::InvalidSessionId => "invalid session id",
            ErrorStatus::JavascriptError => "javascript error",
            ErrorStatus::MoveTargetOutOfBounds => "move target out of bounds",
            ErrorStatus::NoSuchAlert => "no such alert",
            ErrorStatus::NoSuchElement => "no such element",
            ErrorStatus::NoSuchFrame => "no such frame",
            ErrorStatus::NoSuchWindow => "no such window",
            ErrorStatus::ScriptTimeout => "script timeout",
            ErrorStatus::SessionNotCreated => "session not created",
            ErrorStatus::StaleElementReference => "stale element reference",
            ErrorStatus::Timeout => "timeout",
            ErrorStatus::UnableToSetCookie => "unable to set cookie",
            ErrorStatus::UnexpectedAlertOpen => "unexpected alert open",
            ErrorStatus::UnknownError => "unknown error",
            ErrorStatus::UnknownPath => "unknown command",
            ErrorStatus::UnknownMethod => "unknown command",
            ErrorStatus::UnsupportedOperation => "unsupported operation",
        }
    }

    pub fn http_status(&self) -> u32 {
        match self.status {
            ErrorStatus::UnknownPath => 404u32,
            ErrorStatus::UnknownMethod => 405u32,
            _ => 500u32
        }
    }

    pub fn to_json_string(&self) -> String {
        self.to_json().to_string()
    }
}

impl ToJson for WebDriverError {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("status".to_string(), self.status_code().to_json());
        data.insert("error".to_string(), self.message.to_json());
        Json::Object(data)
    }
}

impl Error for WebDriverError {
    fn description(&self) -> &str {
        self.status_code()
    }

    fn detail(&self) -> Option<String> {
        Some(self.message.clone())
    }

    fn cause(&self) -> Option<&Error> {
        None
    }
}

impl FromError<ParserError> for WebDriverError {
    fn from_error(err: ParserError) -> WebDriverError {
        let msg = format!("{:?}", err);
        WebDriverError::new(ErrorStatus::UnknownError, msg.as_slice())
    }
}

#[derive(PartialEq, Clone, Show)]
pub enum Nullable<T: ToJson> {
    Value(T),
    Null
}

impl<T: ToJson> Nullable<T> {
     pub fn is_null(&self) -> bool {
        match *self {
            Nullable::Value(_) => false,
            Nullable::Null => true
        }
    }

     pub fn is_value(&self) -> bool {
        match *self {
            Nullable::Value(_) => true,
            Nullable::Null => false
        }
    }
}

impl<T: ToJson> Nullable<T> {
    //This is not very pretty
    pub fn from_json<F: FnOnce(&Json) -> WebDriverResult<T>>(value: &Json, f: F) -> WebDriverResult<Nullable<T>> {
        if value.is_null() {
            Ok(Nullable::Null)
        } else {
            Ok(Nullable::Value(try!(f(value))))
        }
    }
}

impl<T: ToJson> ToJson for Nullable<T> {
    fn to_json(&self) -> Json {
        match *self {
            Nullable::Value(ref x) => x.to_json(),
            Nullable::Null => Json::Null
        }
    }
}

impl<T: ToJson> Encodable for Nullable<T> {
    fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
        match *self {
            Nullable::Value(ref x) => x.to_json().encode(s),
            Nullable::Null => s.emit_option_none()
        }
    }
}

#[derive(PartialEq)]
pub struct WebElement {
    pub id: String
}

impl WebElement {
    pub fn new(id: String) -> WebElement {
        WebElement {
            id: id
        }
    }

    pub fn from_json(data: &Json) -> WebDriverResult<WebElement> {
        let object = try_opt!(data.as_object(),
                              ErrorStatus::InvalidArgument,
                              "Could not convert webelement to object");
        let id_value = try_opt!(object.get(ELEMENT_KEY),
                                ErrorStatus::InvalidArgument,
                                "Could not find webelement key");

        let id = try_opt!(id_value.as_string(),
                          ErrorStatus::InvalidArgument,
                          "Could not convert web element to string").to_string();

        Ok(WebElement::new(id))
    }
}

impl ToJson for WebElement {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert(ELEMENT_KEY.to_string(), self.id.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub enum FrameId {
    Short(u16),
    Element(WebElement),
    Null
}

impl FrameId {
    pub fn from_json(data: &Json) -> WebDriverResult<FrameId> {
        match data {
            &Json::U64(x) => {
                let id = try_opt!(x.to_u16(),
                                  ErrorStatus::NoSuchFrame,
                                  "frame id out of range");
                Ok(FrameId::Short(id))
            },
            &Json::Null => Ok(FrameId::Null),
            &Json::String(ref x) => Ok(FrameId::Element(WebElement::new(x.clone()))),
            _ => Err(WebDriverError::new(ErrorStatus::NoSuchFrame,
                                         "frame id has unexpected type"))
        }
    }
}

impl ToJson for FrameId {
    fn to_json(&self) -> Json {
        match *self {
            FrameId::Short(x) => {
                Json::U64(x as u64)
            },
            FrameId::Element(ref x) => {
                Json::String(x.id.clone())
            },
            FrameId::Null => {
                Json::Null
            }
        }
    }
}

#[derive(PartialEq)]
pub enum LocatorStrategy {
    CSSSelector,
    LinkText,
    PartialLinkText,
    XPath
}

impl LocatorStrategy {
    pub fn from_json(body: &Json) -> WebDriverResult<LocatorStrategy> {
        match try_opt!(body.as_string(),
                       ErrorStatus::InvalidArgument,
                       "Cound not convert strategy to string") {
            "css selector" => Ok(LocatorStrategy::CSSSelector),
            "link text" => Ok(LocatorStrategy::LinkText),
            "partial link text" => Ok(LocatorStrategy::PartialLinkText),
            "xpath" => Ok(LocatorStrategy::XPath),
            _ => Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                         "Unknown locator strategy"))
        }
    }
}

impl ToJson for LocatorStrategy {
    fn to_json(&self) -> Json {
        Json::String(match *self {
            LocatorStrategy::CSSSelector => "css selector",
            LocatorStrategy::LinkText => "link text",
            LocatorStrategy::PartialLinkText => "partial link text",
            LocatorStrategy::XPath => "xpath"
        }.to_string())
    }
}
