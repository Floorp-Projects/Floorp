use std::error;
use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Clone, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize, Deserialize)]
#[serde(untagged)]
pub(crate) enum Error {
    Marionette(MarionetteError),
}

impl Error {
    pub fn kind(&self) -> ErrorKind {
        match *self {
            Error::Marionette(ref err) => err.kind,
        }
    }
}

impl fmt::Debug for Error {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::Marionette(ref err) => fmt
                .debug_struct("Marionette")
                .field("kind", &err.kind)
                .field("message", &err.message)
                .field("stacktrace", &err.stack.clone())
                .finish(),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::Marionette(ref err) => write!(fmt, "{}: {}", err.kind, err.message),
        }
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        match self {
            Error::Marionette(_) => self.kind().as_str(),
        }
    }
}

#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize, Deserialize)]
pub struct MarionetteError {
    #[serde(rename = "error")]
    pub kind: ErrorKind,
    #[serde(default = "empty_string")]
    pub message: String,
    #[serde(rename = "stacktrace", default = "empty_string")]
    pub stack: String,
}

fn empty_string() -> String {
    "".to_owned()
}

impl Into<Error> for MarionetteError {
    fn into(self) -> Error {
        Error::Marionette(self)
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize, Deserialize)]
pub enum ErrorKind {
    #[serde(rename = "element click intercepted")]
    ElementClickIntercepted,
    #[serde(rename = "element not accessible")]
    ElementNotAccessible,
    #[serde(rename = "element not interactable")]
    ElementNotInteractable,
    #[serde(rename = "insecure certificate")]
    InsecureCertificate,
    #[serde(rename = "invalid argument")]
    InvalidArgument,
    #[serde(rename = "invalid cookie")]
    InvalidCookieDomain,
    #[serde(rename = "invalid element state")]
    InvalidElementState,
    #[serde(rename = "invalid selector")]
    InvalidSelector,
    #[serde(rename = "invalid session id")]
    InvalidSessionId,
    #[serde(rename = "javascript error")]
    JavaScript,
    #[serde(rename = "move target out of bounds")]
    MoveTargetOutOfBounds,
    #[serde(rename = "no such alert")]
    NoSuchAlert,
    #[serde(rename = "no such element")]
    NoSuchElement,
    #[serde(rename = "no such frame")]
    NoSuchFrame,
    #[serde(rename = "no such window")]
    NoSuchWindow,
    #[serde(rename = "script timeout")]
    ScriptTimeout,
    #[serde(rename = "session not created")]
    SessionNotCreated,
    #[serde(rename = "stale element reference")]
    StaleElementReference,
    #[serde(rename = "timeout")]
    Timeout,
    #[serde(rename = "unable to set cookie")]
    UnableToSetCookie,
    #[serde(rename = "unexpected alert open")]
    UnexpectedAlertOpen,
    #[serde(rename = "unknown command")]
    UnknownCommand,
    #[serde(rename = "unknown error")]
    Unknown,
    #[serde(rename = "unsupported operation")]
    UnsupportedOperation,
    #[serde(rename = "webdriver error")]
    WebDriver,
}

impl ErrorKind {
    pub(crate) fn as_str(self) -> &'static str {
        use ErrorKind::*;
        match self {
            ElementClickIntercepted => "element click intercepted",
            ElementNotAccessible => "element not accessible",
            ElementNotInteractable => "element not interactable",
            InsecureCertificate => "insecure certificate",
            InvalidArgument => "invalid argument",
            InvalidCookieDomain => "invalid cookie",
            InvalidElementState => "invalid element state",
            InvalidSelector => "invalid selector",
            InvalidSessionId => "invalid session id",
            JavaScript => "javascript error",
            MoveTargetOutOfBounds => "move target out of bounds",
            NoSuchAlert => "no such alert",
            NoSuchElement => "no such element",
            NoSuchFrame => "no such frame",
            NoSuchWindow => "no such window",
            ScriptTimeout => "script timeout",
            SessionNotCreated => "session not created",
            StaleElementReference => "stale eelement referencee",
            Timeout => "timeout",
            UnableToSetCookie => "unable to set cookie",
            UnexpectedAlertOpen => "unexpected alert open",
            UnknownCommand => "unknown command",
            Unknown => "unknown error",
            UnsupportedOperation => "unsupported operation",
            WebDriver => "webdriver error",
        }
    }
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{}", self.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::assert_ser_de;
    use serde_json::json;

    #[test]
    fn test_json_error() {
        let err = MarionetteError {
            kind: ErrorKind::Timeout,
            message: "".into(),
            stack: "".into(),
        };
        assert_ser_de(
            &err,
            json!({"error": "timeout", "message": "", "stacktrace": ""}),
        );
    }
}
