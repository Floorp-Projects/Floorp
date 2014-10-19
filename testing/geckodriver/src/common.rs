use std::collections::TreeMap;
use serialize::json;
use serialize::json::ToJson;

#[deriving(PartialEq)]
pub enum Status {
    Success,
    Timeout,
    UnknownError,
    UnknownCommand,
}

pub type WebDriverResult<T> = Result<T, WebDriverError>;

pub struct WebDriverError {
    pub session_id: Option<String>,
    pub status: Status,
    pub message: String
}

impl WebDriverError {
    pub fn new(session_id: Option<String>, status: Status, message: &str) -> WebDriverError {
        WebDriverError {
            session_id: session_id,
            status: status,
            message: message.to_string().clone()
        }
    }

    pub fn http_status(&self) -> int {
        match self.status {
            UnknownCommand => 404,
            _ => 200
        }
    }
}

impl ToJson for WebDriverError {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("error".to_string(), self.message.to_json());
        json::Object(data)
    }
}
