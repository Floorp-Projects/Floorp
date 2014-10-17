use std::collections::TreeMap;
use serialize::json;
use serialize::json::{decode, ToJson, Builder};

use command::{WebDriverMessage, GetMarionetteId, NewSession, Get, GetCurrentUrl};
use marionette::{MarionetteSession};

pub struct WebDriverResponse {
    session_id: String,
    status: Status,
    value: TreeMap<String,String>
}

#[deriving(PartialEq)]
enum Status {
    Success,
    Timeout,
    UnknownError
}

impl WebDriverResponse {
    pub fn from_json(session: &mut MarionetteSession,
                     message: &WebDriverMessage,
                     data: &str) -> Option<WebDriverResponse> {
        println!("Decoding json data");
        let decoded = json::from_str(data).unwrap();
        println!("Decoded json data");
        let json_data = match decoded {
            json::Object(x) => x,
            _ => fail!("Expected an object")
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
                Some(WebDriverResponse {status: status,
                                        session_id: session.session_id.clone(),
                                        value: TreeMap::new()})
            },
            Get(_) => {
                Some(WebDriverResponse {status: status,
                                        session_id: session.session_id.clone(),
                                        value: TreeMap::new()})
            },
            GetCurrentUrl => {
                Some(WebDriverResponse {status: status,
                                        session_id: session.session_id.clone(),
                                        value: TreeMap::new()})
            }
        }
    }

    fn status_string(&self) -> String {
        match self.status {
            Success => "success".to_string(),
            Timeout => "timeout".to_string(),
            UnknownError => "unknown error".to_string()
        }
    }

    pub fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("sessionId".to_string(), self.session_id.to_json());
        data.insert("status".to_string(), self.status_string().to_json());
        data.insert("capabilties".to_string(), self.value.to_json());
        json::Object(data)
    }
}
