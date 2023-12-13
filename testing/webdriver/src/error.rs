/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use base64::DecodeError;
use http::StatusCode;
use serde::de::{Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use std::borrow::Cow;
use std::convert::From;
use std::error;
use std::io;
use thiserror::Error;

#[derive(Debug, PartialEq)]
pub enum ErrorStatus {
    /// The [element]'s [ShadowRoot] is not attached to the active document,
    /// or the reference is stale
    /// [element]: ../common/struct.WebElement.html
    /// [ShadowRoot]: ../common/struct.ShadowRoot.html
    DetachedShadowRoot,

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

    /// An [element]'s [ShadowRoot] was not found attached to the element.
    ///
    /// [element]: ../common/struct.WebElement.html
    /// [ShadowRoot]: ../common/struct.ShadowRoot.html
    NoSuchShadowRoot,

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

    /// Indicates that a [command] that should have executed properly is not
    /// currently supported.
    UnsupportedOperation,
}

impl Serialize for ErrorStatus {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        self.error_code().serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for ErrorStatus {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let error_string = String::deserialize(deserializer)?;
        Ok(ErrorStatus::from(error_string))
    }
}

impl ErrorStatus {
    /// Returns the string serialisation of the error type.
    pub fn error_code(&self) -> &'static str {
        use self::ErrorStatus::*;
        match *self {
            DetachedShadowRoot => "detached shadow root",
            ElementClickIntercepted => "element click intercepted",
            ElementNotInteractable => "element not interactable",
            ElementNotSelectable => "element not selectable",
            InsecureCertificate => "insecure certificate",
            InvalidArgument => "invalid argument",
            InvalidCookieDomain => "invalid cookie domain",
            InvalidCoordinates => "invalid coordinates",
            InvalidElementState => "invalid element state",
            InvalidSelector => "invalid selector",
            InvalidSessionId => "invalid session id",
            JavascriptError => "javascript error",
            MoveTargetOutOfBounds => "move target out of bounds",
            NoSuchAlert => "no such alert",
            NoSuchCookie => "no such cookie",
            NoSuchElement => "no such element",
            NoSuchFrame => "no such frame",
            NoSuchShadowRoot => "no such shadow root",
            NoSuchWindow => "no such window",
            ScriptTimeout => "script timeout",
            SessionNotCreated => "session not created",
            StaleElementReference => "stale element reference",
            Timeout => "timeout",
            UnableToCaptureScreen => "unable to capture screen",
            UnableToSetCookie => "unable to set cookie",
            UnexpectedAlertOpen => "unexpected alert open",
            UnknownError => "unknown error",
            UnknownMethod => "unknown method",
            UnknownCommand => "unknown command",
            UnsupportedOperation => "unsupported operation",
        }
    }

    /// Returns the correct HTTP status code associated with the error type.
    pub fn http_status(&self) -> StatusCode {
        use self::ErrorStatus::*;
        match *self {
            DetachedShadowRoot => StatusCode::NOT_FOUND,
            ElementClickIntercepted => StatusCode::BAD_REQUEST,
            ElementNotInteractable => StatusCode::BAD_REQUEST,
            ElementNotSelectable => StatusCode::BAD_REQUEST,
            InsecureCertificate => StatusCode::BAD_REQUEST,
            InvalidArgument => StatusCode::BAD_REQUEST,
            InvalidCookieDomain => StatusCode::BAD_REQUEST,
            InvalidCoordinates => StatusCode::BAD_REQUEST,
            InvalidElementState => StatusCode::BAD_REQUEST,
            InvalidSelector => StatusCode::BAD_REQUEST,
            InvalidSessionId => StatusCode::NOT_FOUND,
            JavascriptError => StatusCode::INTERNAL_SERVER_ERROR,
            MoveTargetOutOfBounds => StatusCode::INTERNAL_SERVER_ERROR,
            NoSuchAlert => StatusCode::NOT_FOUND,
            NoSuchCookie => StatusCode::NOT_FOUND,
            NoSuchElement => StatusCode::NOT_FOUND,
            NoSuchFrame => StatusCode::NOT_FOUND,
            NoSuchShadowRoot => StatusCode::NOT_FOUND,
            NoSuchWindow => StatusCode::NOT_FOUND,
            ScriptTimeout => StatusCode::INTERNAL_SERVER_ERROR,
            SessionNotCreated => StatusCode::INTERNAL_SERVER_ERROR,
            StaleElementReference => StatusCode::NOT_FOUND,
            Timeout => StatusCode::INTERNAL_SERVER_ERROR,
            UnableToCaptureScreen => StatusCode::BAD_REQUEST,
            UnableToSetCookie => StatusCode::INTERNAL_SERVER_ERROR,
            UnexpectedAlertOpen => StatusCode::INTERNAL_SERVER_ERROR,
            UnknownCommand => StatusCode::NOT_FOUND,
            UnknownError => StatusCode::INTERNAL_SERVER_ERROR,
            UnknownMethod => StatusCode::METHOD_NOT_ALLOWED,
            UnsupportedOperation => StatusCode::INTERNAL_SERVER_ERROR,
        }
    }
}

/// Deserialises error type from string.
impl From<String> for ErrorStatus {
    fn from(s: String) -> ErrorStatus {
        use self::ErrorStatus::*;
        match &*s {
            "detached shadow root" => DetachedShadowRoot,
            "element click intercepted" => ElementClickIntercepted,
            "element not interactable" | "element not visible" => ElementNotInteractable,
            "element not selectable" => ElementNotSelectable,
            "insecure certificate" => InsecureCertificate,
            "invalid argument" => InvalidArgument,
            "invalid cookie domain" => InvalidCookieDomain,
            "invalid coordinates" | "invalid element coordinates" => InvalidCoordinates,
            "invalid element state" => InvalidElementState,
            "invalid selector" => InvalidSelector,
            "invalid session id" => InvalidSessionId,
            "javascript error" => JavascriptError,
            "move target out of bounds" => MoveTargetOutOfBounds,
            "no such alert" => NoSuchAlert,
            "no such element" => NoSuchElement,
            "no such frame" => NoSuchFrame,
            "no such shadow root" => NoSuchShadowRoot,
            "no such window" => NoSuchWindow,
            "script timeout" => ScriptTimeout,
            "session not created" => SessionNotCreated,
            "stale element reference" => StaleElementReference,
            "timeout" => Timeout,
            "unable to capture screen" => UnableToCaptureScreen,
            "unable to set cookie" => UnableToSetCookie,
            "unexpected alert open" => UnexpectedAlertOpen,
            "unknown command" => UnknownCommand,
            "unknown error" => UnknownError,
            "unsupported operation" => UnsupportedOperation,
            _ => UnknownError,
        }
    }
}

pub type WebDriverResult<T> = Result<T, WebDriverError>;

#[derive(Debug, PartialEq, Serialize, Error)]
#[serde(remote = "Self")]
#[error("{}", .error.error_code())]
pub struct WebDriverError {
    pub error: ErrorStatus,
    pub message: Cow<'static, str>,
    #[serde(rename = "stacktrace")]
    pub stack: Cow<'static, str>,
    #[serde(skip)]
    pub delete_session: bool,
}

impl Serialize for WebDriverError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        #[derive(Serialize)]
        struct Wrapper<'a> {
            #[serde(with = "WebDriverError")]
            value: &'a WebDriverError,
        }

        Wrapper { value: self }.serialize(serializer)
    }
}

impl WebDriverError {
    pub fn new<S>(error: ErrorStatus, message: S) -> WebDriverError
    where
        S: Into<Cow<'static, str>>,
    {
        WebDriverError {
            error,
            message: message.into(),
            stack: "".into(),
            delete_session: false,
        }
    }

    pub fn new_with_stack<S>(error: ErrorStatus, message: S, stack: S) -> WebDriverError
    where
        S: Into<Cow<'static, str>>,
    {
        WebDriverError {
            error,
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
}

impl From<serde_json::Error> for WebDriverError {
    fn from(err: serde_json::Error) -> WebDriverError {
        WebDriverError::new(ErrorStatus::InvalidArgument, err.to_string())
    }
}

impl From<io::Error> for WebDriverError {
    fn from(err: io::Error) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.to_string())
    }
}

impl From<DecodeError> for WebDriverError {
    fn from(err: DecodeError) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.to_string())
    }
}

impl From<Box<dyn error::Error>> for WebDriverError {
    fn from(err: Box<dyn error::Error>) -> WebDriverError {
        WebDriverError::new(ErrorStatus::UnknownError, err.to_string())
    }
}

#[cfg(test)]
mod tests {
    use serde_json::json;

    use super::*;
    use crate::test::assert_ser;

    #[test]
    fn test_json_webdriver_error() {
        let json = json!({"value": {
            "error": "unknown error",
            "message": "foo bar",
            "stacktrace": "foo\nbar",
        }});
        let error = WebDriverError {
            error: ErrorStatus::UnknownError,
            message: "foo bar".into(),
            stack: "foo\nbar".into(),
            delete_session: true,
        };

        assert_ser(&error, json);
    }

    #[test]
    fn test_json_error_status() {
        assert_ser(&ErrorStatus::UnknownError, json!("unknown error"));
    }
}
