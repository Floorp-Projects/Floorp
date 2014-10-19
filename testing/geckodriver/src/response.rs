use std::collections::TreeMap;
use serialize::json;
use serialize::json::{decode, ToJson, Builder};

use command::{WebDriverMessage, GetMarionetteId, NewSession, Get, GetCurrentUrl};
use marionette::{MarionetteSession};

use common::{Status, Success, Timeout, UnknownError, UnknownCommand, WebDriverError};

pub struct WebDriverResponse {
    session_id: Option<String>,
    status: Status,
    value: json::Json
}

impl WebDriverResponse {
    pub fn new(session_id: Option<String>, status: Status, value: json::Json) -> WebDriverResponse {
        WebDriverResponse {
            session_id: session_id,
            status: status,
            value: value
        }
    }

    pub fn from_json(session: &mut MarionetteSession,
                     message: &WebDriverMessage,
                     data: &str) -> Option<WebDriverResponse> {
        let decoded = match json::from_str(data) {
            Ok(data) => data,
            Err(msg) => {
                let error = WebDriverError::new(Some(session.id()),
                                                UnknownError,
                                                "Failed to decode marionette data as json");
                return Some(WebDriverResponse::from_err(&error));
            }
        };
        let json_data = match decoded {
            json::Object(x) => x,
            _ => {
                let error = WebDriverError::new(Some(session.id()),
                                                UnknownError,
                                                "Expected a json object");
                return Some(WebDriverResponse::from_err(&error));
            }
        };
        let status = if json_data.contains_key(&"error".to_string()) {
            UnknownError
        } else {
            Success
        };
        match message.command {
            GetMarionetteId => None,
            NewSession => {
                if status == Success {
                    session.update(message,
                                   json_data.find(&"from".to_string()).unwrap(),
                                   json_data.find(&"value".to_string()).unwrap());
                };
                Some(WebDriverResponse::new(Some(session.session_id.clone()), status,
                                            json::Null))
            },
            Get(_) => {
                Some(WebDriverResponse::new(Some(session.session_id.clone()), status,
                                            json::Null))
            },
            GetCurrentUrl => {
                let value = match json_data.find(&"value".to_string()) {
                    Some(ref data) => {
                        data.clone()
                    },
                    None => {
                        let error = WebDriverError::new(Some(session.session_id.clone()),
                                                        UnknownError,
                                                        "Failed to find value field");
                        return Some(WebDriverResponse::from_err(&error));
                    }
                };
                Some(WebDriverResponse::new(Some(session.id()),
                                       status,
                                       value.clone()))
            }
        }
    }

    pub fn from_err(error_data: &WebDriverError) -> WebDriverResponse {
        WebDriverResponse::new(error_data.session_id.clone(),
                               error_data.status,
                               error_data.to_json())
    }

    fn status_string(&self) -> String {
        match self.status {
            Success => "success".to_string(),
            Timeout => "timeout".to_string(),
            UnknownError => "unknown error".to_string(),
            UnknownCommand => "unknown command".to_string()
        }
    }

    pub fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("sessionId".to_string(), self.session_id.to_json());
        data.insert("status".to_string(), self.status_string().to_json());
        data.insert("value".to_string(), self.value.to_json());
        json::Object(data)
    }
}

