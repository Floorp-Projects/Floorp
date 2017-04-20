use backtrace::Backtrace;
use hyper::status::StatusCode;
use rustc_serialize::base64::FromBase64Error;
use rustc_serialize::json::{DecoderError, Json, ParserError, ToJson};
use std::borrow::Cow;
use std::collections::BTreeMap;
use std::convert::From;
use std::error::Error;
use std::fmt;
use std::io;

#[derive(PartialEq, Debug)]
pub enum ErrorStatus {
    /// The [`ElementClick`] command could not be completed because the
    /// [element] receiving the events is obscuring the element that was
    /// requested clicked.
    ///
    /// [`ElementClick`]:
    /// ../command/enum.WebDriverCommand.html#variant.ElementClick
    /// [element]: ../common/struct.WebElement.html
    ElementClickIntercepted,

    /// A [command] could not be completed because the element is not pointer-
    /// or keyboard interactable.
    ///
    /// [command]: ../command/index.html
    ElementNotInteractable,

    /// An attempt was made to select an [element] that cannot be selected.
    ///
    /// [element]: ../common/struct.WebElement.html
    ElementNotSelectable,

    /// Navigation caused the user agent to hit a certificate warning, which is
    /// usually the result of an expired or invalid TLS certificate.
    InsecureCertificate,

    /// The arguments passed to a [command] are either invalid or malformed.
    ///
    /// [command]: ../command/index.html
    InvalidArgument,

    /// An illegal attempt was made to set a cookie under a different domain
    /// than the current page.
    InvalidCookieDomain,

    /// The coordinates provided to an interactions operation are invalid.
    InvalidCoordinates,

    /// A [command] could not be completed because the element is an invalid
    /// state, e.g. attempting to click an element that is no longer attached
    /// to the document.
    ///
    /// [command]: ../command/index.html
    InvalidElementState,

    /// Argument was an invalid selector.
    InvalidSelector,

    /// Occurs if the given session ID is not in the list of active sessions,
    /// meaning the session either does not exist or that it’s not active.
    InvalidSessionId,

    /// An error occurred while executing JavaScript supplied by the user.
    JavascriptError,

    /// The target for mouse interaction is not in the browser’s viewport and
    /// cannot be brought into that viewport.
    MoveTargetOutOfBounds,

    /// An attempt was made to operate on a modal dialogue when one was not
    /// open.
    NoSuchAlert,

    /// No cookie matching the given path name was found amongst the associated
    /// cookies of the current browsing context’s active document.
    NoSuchCookie,

    /// An [element] could not be located on the page using the given search
    /// parameters.
    ///
    /// [element]: ../common/struct.WebElement.html
    NoSuchElement,

    /// A [command] to switch to a frame could not be satisfied because the
    /// frame could not be found.
    ///
    /// [command]: ../command/index.html
    NoSuchFrame,

    /// A [command] to switch to a window could not be satisfied because the
    /// window could not be found.
    ///
    /// [command]: ../command/index.html
    NoSuchWindow,

    /// A script did not complete before its timeout expired.
    ScriptTimeout,

    /// A new session could not be created.
    SessionNotCreated,

    /// A [command] failed because the referenced [element] is no longer
    /// attached to the DOM.
    ///
    /// [command]: ../command/index.html
    /// [element]: ../common/struct.WebElement.html
    StaleElementReference,

    /// An operation did not complete before its timeout expired.
    Timeout,

    /// A screen capture was made impossible.
    UnableToCaptureScreen,

    /// Setting the cookie’s value could not be done.
    UnableToSetCookie,

    /// A modal dialogue was open, blocking this operation.
    UnexpectedAlertOpen,

    /// The requested command could not be executed because it does not exist.
    UnknownCommand,

    /// An unknown error occurred in the remote end whilst processing the
    /// [command].
    ///
    /// [command]: ../command/index.html
    UnknownError,

    /// The requested [command] matched a known endpoint, but did not match a
    /// method for that endpoint.
    ///
    /// [command]: ../command/index.html
    UnknownMethod,

    UnknownPath,

    /// Indicates that a [command] that should have executed properly is not
    /// currently supported.
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
    pub stack: Cow<'static, str>,
    pub delete_session: bool,
}

impl WebDriverError {
    pub fn new<S>(error: ErrorStatus, message: S) -> WebDriverError
        where S: Into<Cow<'static, str>>
    {
        WebDriverError {
            error: error,
            message: message.into(),
            stack: format!("{:?}", Backtrace::new()).into(),
            delete_session: false,
        }
    }

    pub fn new_with_stack<S>(error: ErrorStatus, message: S, stack: S) -> WebDriverError
        where S: Into<Cow<'static, str>>
    {
        WebDriverError {
            error: error,
            message: message.into(),
            stack: stack.into(),
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
        data.insert("stacktrace".into(), self.stack.to_json());

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

impl fmt::Display for WebDriverError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.message.fmt(f)
    }
}

impl From<ParserError> for WebDriverError {
    fn from(err: ParserError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.description().to_string())
    }
}

impl From<io::Error> for WebDriverError {
    fn from(err: io::Error) -> WebDriverError {
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
