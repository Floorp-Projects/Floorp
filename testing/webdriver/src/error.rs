use backtrace::Backtrace;
use hyper::status::StatusCode;
use rustc_serialize::base64::FromBase64Error;
use rustc_serialize::json::{DecoderError, Json, ParserError, ToJson};
use std::borrow::Cow;
use std::collections::BTreeMap;
use std::convert::From;
use std::error::Error;
use std::fmt;
use std::io::Error as IoError;

#[derive(PartialEq, Debug)]
pub enum ErrorStatus {
    ElementClickIntercepted,
    ElementNotInteractable,
    ElementNotSelectable,
    InsecureCertificate,
    InvalidArgument,
    InvalidCookieDomain,
    InvalidCoordinates,
    InvalidElementState,
    InvalidSelector,
    InvalidSessionId,
    JavascriptError,
    MoveTargetOutOfBounds,
    NoSuchAlert,
    NoSuchCookie,
    NoSuchElement,
    NoSuchFrame,
    NoSuchWindow,
    ScriptTimeout,
    SessionNotCreated,
    StaleElementReference,
    Timeout,
    UnableToCaptureScreen,
    UnableToSetCookie,
    UnexpectedAlertOpen,
    UnknownCommand,
    UnknownError,
    UnknownMethod,
    UnknownPath,
    UnsupportedOperation,
}

impl ErrorStatus {
    pub fn error_code(&self) -> &'static str {
        match *self {
            ErrorStatus::ElementClickIntercepted => "element click intercepted",
            ErrorStatus::ElementNotInteractable => "element not interactable",
            ErrorStatus::ElementNotSelectable => "element not selectable",
            ErrorStatus::InsecureCertificate => "insecure certificate",
            ErrorStatus::InvalidArgument => "invalid argument",
            ErrorStatus::InvalidCookieDomain => "invalid cookie domain",
            ErrorStatus::InvalidCoordinates => "invalid coordinates",
            ErrorStatus::InvalidElementState => "invalid element state",
            ErrorStatus::InvalidSelector => "invalid selector",
            ErrorStatus::InvalidSessionId => "invalid session id",
            ErrorStatus::JavascriptError => "javascript error",
            ErrorStatus::MoveTargetOutOfBounds => "move target out of bounds",
            ErrorStatus::NoSuchAlert => "no such alert",
            ErrorStatus::NoSuchCookie => "no such cookie",
            ErrorStatus::NoSuchElement => "no such element",
            ErrorStatus::NoSuchFrame => "no such frame",
            ErrorStatus::NoSuchWindow => "no such window",
            ErrorStatus::ScriptTimeout => "script timeout",
            ErrorStatus::SessionNotCreated => "session not created",
            ErrorStatus::StaleElementReference => "stale element reference",
            ErrorStatus::Timeout => "timeout",
            ErrorStatus::UnableToCaptureScreen => "unable to capture screen",
            ErrorStatus::UnableToSetCookie => "unable to set cookie",
            ErrorStatus::UnexpectedAlertOpen => "unexpected alert open",
            ErrorStatus::UnknownCommand |
            ErrorStatus::UnknownError => "unknown error",
            ErrorStatus::UnknownMethod => "unknown method",
            ErrorStatus::UnknownPath => "unknown command",
            ErrorStatus::UnsupportedOperation => "unsupported operation",
        }
    }

    pub fn http_status(&self) -> StatusCode {
        match *self {
            ErrorStatus::ElementClickIntercepted => StatusCode::BadRequest,
            ErrorStatus::ElementNotInteractable => StatusCode::BadRequest,
            ErrorStatus::ElementNotSelectable => StatusCode::BadRequest,
            ErrorStatus::InsecureCertificate => StatusCode::BadRequest,
            ErrorStatus::InvalidArgument => StatusCode::BadRequest,
            ErrorStatus::InvalidCookieDomain => StatusCode::BadRequest,
            ErrorStatus::InvalidCoordinates => StatusCode::BadRequest,
            ErrorStatus::InvalidElementState => StatusCode::BadRequest,
            ErrorStatus::InvalidSelector => StatusCode::BadRequest,
            ErrorStatus::InvalidSessionId => StatusCode::NotFound,
            ErrorStatus::JavascriptError => StatusCode::InternalServerError,
            ErrorStatus::MoveTargetOutOfBounds => StatusCode::InternalServerError,
            ErrorStatus::NoSuchAlert => StatusCode::BadRequest,
            ErrorStatus::NoSuchCookie => StatusCode::NotFound,
            ErrorStatus::NoSuchElement => StatusCode::NotFound,
            ErrorStatus::NoSuchFrame => StatusCode::BadRequest,
            ErrorStatus::NoSuchWindow => StatusCode::BadRequest,
            ErrorStatus::ScriptTimeout => StatusCode::RequestTimeout,
            ErrorStatus::SessionNotCreated => StatusCode::InternalServerError,
            ErrorStatus::StaleElementReference => StatusCode::BadRequest,
            ErrorStatus::Timeout => StatusCode::RequestTimeout,
            ErrorStatus::UnableToCaptureScreen => StatusCode::BadRequest,
            ErrorStatus::UnableToSetCookie => StatusCode::InternalServerError,
            ErrorStatus::UnexpectedAlertOpen => StatusCode::InternalServerError,
            ErrorStatus::UnknownCommand => StatusCode::NotFound,
            ErrorStatus::UnknownError => StatusCode::InternalServerError,
            ErrorStatus::UnknownMethod => StatusCode::MethodNotAllowed,
            ErrorStatus::UnknownPath => StatusCode::NotFound,
            ErrorStatus::UnsupportedOperation => StatusCode::InternalServerError,
        }
    }
}

pub type WebDriverResult<T> = Result<T, WebDriverError>;

#[derive(Debug)]
pub struct WebDriverError {
    pub error: ErrorStatus,
    pub message: Cow<'static, str>,
    pub backtrace: Backtrace,
    pub delete_session: bool,
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
            backtrace: Backtrace::new(),
            delete_session: false,
        }
    }

    pub fn error_code(&self) -> &'static str {
        self.error.error_code()
    }

    pub fn http_status(&self) -> StatusCode {
        self.error.http_status()
    }

    pub fn to_json_string(&self) -> String {
        self.to_json().to_string()
    }
}

impl ToJson for WebDriverError {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("error".into(), self.error_code().to_json());
        data.insert("message".into(), self.message.to_json());
        data.insert("stacktrace".into(),
                    format!("{:?}", self.backtrace).to_json());
        let mut wrapper = BTreeMap::new();
        wrapper.insert("value".into(), Json::Object(data));
        Json::Object(wrapper)
    }
}

impl Error for WebDriverError {
    fn description(&self) -> &str {
        self.error_code()
    }

    fn cause(&self) -> Option<&Error> {
        None
    }
}

impl From<ParserError> for WebDriverError {
    fn from(err: ParserError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.description().to_string())
    }
}

impl From<IoError> for WebDriverError {
    fn from(err: IoError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.description().to_string())
    }
}

impl From<DecoderError> for WebDriverError {
    fn from(err: DecoderError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.description().to_string())
    }
}

impl From<FromBase64Error> for WebDriverError {
    fn from(err: FromBase64Error) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.description().to_string())
    }
}

impl From<Box<Error>> for WebDriverError {
    fn from(err: Box<Error>) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.description().to_string())
    }
}
