use std::collections::TreeMap;
use serialize::json;
use serialize::json::{ToJson};

use command::WebDriverMessage;
use command::WebDriverCommand::{GetMarionetteId, NewSession, DeleteSession, Get, GetCurrentUrl,
                                GoBack, GoForward, Refresh, GetTitle,
                                GetWindowHandle, GetWindowHandles, Close, Timeouts};
use marionette::{MarionetteSession};

use common::{ErrorStatus, WebDriverError, WebDriverResult};

pub struct WebDriverResponse {
    value: json::Json
}

impl WebDriverResponse {
    pub fn new(value: json::Json) -> WebDriverResponse {
        WebDriverResponse {
            value: value
        }
    }

    pub fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("value".to_string(), self.value.to_json());
        json::Object(data)
    }
}

