use rustc_serialize::json::{Json, ToJson, ParserError};
use std::collections::BTreeMap;
use std::error::{Error, FromError};

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
            message: message.to_string()
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
