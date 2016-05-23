use hyper::status::StatusCode;
use rustc_serialize::base64::FromBase64Error;
use rustc_serialize::json::{Json, ToJson, ParserError, DecoderError};
use std::borrow::Cow;
use std::collections::BTreeMap;
use std::convert::From;
use std::error::Error;
use std::fmt;
use std::io::Error as IoError;

#[derive(PartialEq, Debug)]
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
    UnsupportedOperation
}

impl ErrorStatus {
    pub fn status_code(&self) -> &'static str {
        match self {
            &ErrorStatus::ElementNotSelectable => "element not selectable",
            &ErrorStatus::ElementNotVisible => "element not visible",
            &ErrorStatus::InvalidArgument => "invalid argument",
            &ErrorStatus::InvalidCookieDomain => "invalid cookie domain",
            &ErrorStatus::InvalidElementCoordinates => "invalid element coordinates",
            &ErrorStatus::InvalidElementState => "invalid element state",
            &ErrorStatus::InvalidSelector => "invalid selector",
            &ErrorStatus::InvalidSessionId => "invalid session id",
            &ErrorStatus::JavascriptError => "javascript error",
            &ErrorStatus::MoveTargetOutOfBounds => "move target out of bounds",
            &ErrorStatus::NoSuchAlert => "no such alert",
            &ErrorStatus::NoSuchElement => "no such element",
            &ErrorStatus::NoSuchFrame => "no such frame",
            &ErrorStatus::NoSuchWindow => "no such window",
            &ErrorStatus::ScriptTimeout => "script timeout",
            &ErrorStatus::SessionNotCreated => "session not created",
            &ErrorStatus::StaleElementReference => "stale element reference",
            &ErrorStatus::Timeout => "timeout",
            &ErrorStatus::UnableToSetCookie => "unable to set cookie",
            &ErrorStatus::UnexpectedAlertOpen => "unexpected alert open",
            &ErrorStatus::UnknownError => "unknown error",
            &ErrorStatus::UnknownPath => "unknown command",
            &ErrorStatus::UnknownMethod => "unknown command",
            &ErrorStatus::UnsupportedOperation => "unsupported operation"
        }
    }

    pub fn http_status(&self) -> StatusCode {
        match self {
            &ErrorStatus::ElementNotSelectable => StatusCode::BadRequest,
            &ErrorStatus::ElementNotVisible => StatusCode::BadRequest,
            &ErrorStatus::InvalidArgument => StatusCode::BadRequest,
            &ErrorStatus::InvalidCookieDomain => StatusCode::BadRequest,
            &ErrorStatus::InvalidElementCoordinates => StatusCode::BadRequest,
            &ErrorStatus::InvalidElementState => StatusCode::BadRequest,
            &ErrorStatus::InvalidSelector => StatusCode::BadRequest,
            &ErrorStatus::InvalidSessionId => StatusCode::NotFound,
            &ErrorStatus::JavascriptError => StatusCode::InternalServerError,
            &ErrorStatus::MoveTargetOutOfBounds => StatusCode::InternalServerError,
            &ErrorStatus::NoSuchAlert => StatusCode::BadRequest,
            &ErrorStatus::NoSuchElement => StatusCode::NotFound,
            &ErrorStatus::NoSuchFrame => StatusCode::BadRequest,
            &ErrorStatus::NoSuchWindow => StatusCode::BadRequest,
            &ErrorStatus::ScriptTimeout => StatusCode::RequestTimeout,
            &ErrorStatus::SessionNotCreated => StatusCode::InternalServerError,
            &ErrorStatus::StaleElementReference => StatusCode::BadRequest,
            &ErrorStatus::Timeout => StatusCode::RequestTimeout,
            &ErrorStatus::UnableToSetCookie => StatusCode::InternalServerError,
            &ErrorStatus::UnexpectedAlertOpen => StatusCode::InternalServerError,
            &ErrorStatus::UnknownError => StatusCode::InternalServerError,
            &ErrorStatus::UnknownPath => StatusCode::NotFound,
            &ErrorStatus::UnknownMethod => StatusCode::MethodNotAllowed,
            &ErrorStatus::UnsupportedOperation => StatusCode::InternalServerError
        }
    }
}

pub type WebDriverResult<T> = Result<T, WebDriverError>;

#[derive(Debug)]
pub struct WebDriverError {
    pub error: ErrorStatus,
    pub message: Cow<'static, str>,
    delete_session: bool
}

impl fmt::Display for WebDriverError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.message.fmt(f)
    }
}

impl WebDriverError {
    pub fn new<S>(error: ErrorStatus, message: S) -> WebDriverError
        where S: Into<Cow<'static, str>>
    {
        WebDriverError {
            error: error,
            message: message.into(),
            delete_session: false
        }
    }

    pub fn status_code(&self) -> &'static str {
        self.error.status_code()
    }

    pub fn http_status(&self) -> StatusCode {
        self.error.http_status()
    }

    pub fn to_json_string(&self) -> String {
        self.to_json().to_string()
    }

    pub fn set_delete_session(&mut self) {
        self.delete_session = true
    }

    pub fn delete_session(&self) -> bool {
        self.delete_session
    }
}

impl ToJson for WebDriverError {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("error".to_string(), self.status_code().to_json());
        data.insert("message".to_string(), self.message.to_json());
        Json::Object(data)
    }
}

impl Error for WebDriverError {
    fn description(&self) -> &str {
        self.status_code()
    }

    fn cause(&self) -> Option<&Error> {
        None
    }
}

impl From<ParserError> for WebDriverError {
    fn from(err: ParserError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError,
            err.description().to_string())
    }
}

impl From<IoError> for WebDriverError {
    fn from(err: IoError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError,
            err.description().to_string())
    }
}

impl From<DecoderError> for WebDriverError {
    fn from(err: DecoderError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError,
                            err.description().to_string())
    }
}

impl From<FromBase64Error> for WebDriverError {
    fn from(err: FromBase64Error) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError,
                            err.description().to_string())
    }
}

impl From<Box<Error>> for WebDriverError {
    fn from(err: Box<Error>) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError,
                            err.description().to_string())
    }
}
