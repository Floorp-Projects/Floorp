use rustc_serialize::json::{self, Json, ToJson};

use common::{Nullable, Date};
use cookie;
use time;

#[derive(Debug)]
pub enum WebDriverResponse {
    CloseWindow(CloseWindowResponse),
    Cookie(CookieResponse),
    DeleteSession,
    ElementRect(ElementRectResponse),
    Generic(ValueResponse),
    NewSession(NewSessionResponse),
    Timeouts(TimeoutsResponse),
    Void,
    WindowRect(WindowRectResponse),
}

impl WebDriverResponse {
    pub fn to_json_string(self) -> String {
        let obj = match self {
            WebDriverResponse::CloseWindow(ref x) => json::encode(&x.to_json()),
            WebDriverResponse::Cookie(ref x) => json::encode(x),
            WebDriverResponse::DeleteSession => Ok("{}".to_string()),
            WebDriverResponse::ElementRect(ref x) => json::encode(x),
            WebDriverResponse::Generic(ref x) => json::encode(x),
            WebDriverResponse::NewSession(ref x) => json::encode(x),
            WebDriverResponse::Timeouts(ref x) => json::encode(x),
            WebDriverResponse::Void => Ok("{}".to_string()),
            WebDriverResponse::WindowRect(ref x) => json::encode(x),
        }.unwrap();

        match self {
            WebDriverResponse::Generic(_) |
            WebDriverResponse::Cookie(_) => obj,
            _ => {
                let mut data = String::with_capacity(11 + obj.len());
                data.push_str("{\"value\": ");
                data.push_str(&*obj);
                data.push_str("}");
                data
            }
        }
    }
}

#[derive(RustcEncodable, Debug)]
pub struct CloseWindowResponse {
    pub window_handles: Vec<String>,
}

impl CloseWindowResponse {
    pub fn new(handles: Vec<String>) -> CloseWindowResponse {
        CloseWindowResponse { window_handles: handles }
    }
}

impl ToJson for CloseWindowResponse {
    fn to_json(&self) -> Json {
        Json::Array(self.window_handles
                    .iter()
                    .map(|x| Json::String(x.clone()))
                    .collect::<Vec<Json>>())
    }
}

#[derive(RustcEncodable, Debug)]
pub struct NewSessionResponse {
    pub sessionId: String,
    pub capabilities: json::Json
}

impl NewSessionResponse {
    pub fn new(session_id: String, capabilities: json::Json) -> NewSessionResponse {
        NewSessionResponse {
            capabilities: capabilities,
            sessionId: session_id
        }
    }
}

#[derive(RustcEncodable, Debug)]
pub struct TimeoutsResponse {
    pub script: u64,
    pub pageLoad: u64,
    pub implicit: u64,
}

impl TimeoutsResponse {
    pub fn new(script: u64, page_load: u64, implicit: u64) -> TimeoutsResponse {
        TimeoutsResponse {
            script: script,
            pageLoad: page_load,
            implicit: implicit,
        }
    }
}

#[derive(RustcEncodable, Debug)]
pub struct ValueResponse {
    pub value: json::Json
}

impl ValueResponse {
    pub fn new(value: json::Json) -> ValueResponse {
        ValueResponse {
            value: value
        }
    }
}

#[derive(RustcEncodable, Debug)]
pub struct WindowRectResponse {
    pub x: i64,
    pub y: i64,
    pub width: u64,
    pub height: u64,
}

#[derive(RustcEncodable, Debug)]
pub struct ElementRectResponse {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64
}

impl ElementRectResponse {
    pub fn new(x: f64, y: f64, width: f64, height: f64) -> ElementRectResponse {
        ElementRectResponse {
            x: x,
            y: y,
            width: width,
            height: height
        }
    }
}

//TODO: some of these fields are probably supposed to be optional
#[derive(RustcEncodable, PartialEq, Debug, Clone)]
pub struct Cookie {
    pub name: String,
    pub value: String,
    pub path: Nullable<String>,
    pub domain: Nullable<String>,
    pub expiry: Nullable<Date>,
    pub secure: bool,
    pub httpOnly: bool
}

impl Cookie {
    pub fn new(name: String, value: String, path: Nullable<String>, domain: Nullable<String>,
               expiry: Nullable<Date>, secure: bool, http_only: bool) -> Cookie {
        Cookie {
            name: name,
            value: value,
            path: path,
            domain: domain,
            expiry: expiry,
            secure: secure,
            httpOnly: http_only
        }
    }
}

impl Into<cookie::Cookie<'static>> for Cookie {
    fn into(self) -> cookie::Cookie<'static> {
        let cookie = cookie::Cookie::build(self.name, self.value)
            .secure(self.secure)
            .http_only(self.httpOnly);
        let cookie = match self.domain {
            Nullable::Value(domain) => cookie.domain(domain),
            Nullable::Null => cookie,
        };
        let cookie = match self.path {
            Nullable::Value(path) => cookie.path(path),
            Nullable::Null => cookie,
        };
        let cookie = match self.expiry {
            Nullable::Value(Date(expiry)) => {
                cookie.expires(time::at(time::Timespec::new(expiry as i64, 0)))
            },
            Nullable::Null => cookie,
        };
        cookie.finish()
    }
}

#[derive(RustcEncodable, Debug)]
pub struct CookieResponse {
    pub value: Vec<Cookie>
}

impl CookieResponse {
    pub fn new(value: Vec<Cookie>) -> CookieResponse {
        CookieResponse {
            value: value
        }
    }
}


#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;
    use rustc_serialize::json::Json;
    use super::{WebDriverResponse,
                CloseWindowResponse,
                CookieResponse,
                ElementRectResponse,
                NewSessionResponse,
                ValueResponse,
                TimeoutsResponse,
                WindowRectResponse,
                Cookie,
                Nullable};

    fn test(resp: WebDriverResponse, expected_str: &str) {
        let data = resp.to_json_string();
        let actual = Json::from_str(&*data).unwrap();
        let expected = Json::from_str(expected_str).unwrap();
        assert_eq!(actual, expected);
    }

    #[test]
    fn test_close_window() {
        let resp = WebDriverResponse::CloseWindow(
            CloseWindowResponse::new(vec!["test".into()]));
        let expected = r#"{"value": ["test"]}"#;
        test(resp, expected);
    }

    #[test]
    fn test_cookie() {
        let resp = WebDriverResponse::Cookie(CookieResponse::new(
            vec![
                Cookie::new("test".into(),
                            "test_value".into(),
                            Nullable::Value("/".into()),
                            Nullable::Null,
                            Nullable::Null,
                            true,
                            false)
            ]));
        let expected = r#"{"value": [{"name": "test", "value": "test_value", "path": "/",
"domain": null, "expiry": null, "secure": true, "httpOnly": false}]}"#;
        test(resp, expected);
    }

    #[test]
    fn test_element_rect() {
        let resp = WebDriverResponse::ElementRect(ElementRectResponse::new(
            0f64, 1f64, 2f64, 3f64));
        let expected = r#"{"value": {"x": 0.0, "y": 1.0, "width": 2.0, "height": 3.0}}"#;
        test(resp, expected);
    }

    #[test]
    fn test_window_rect() {
        let resp = WebDriverResponse::WindowRect(WindowRectResponse {
            x: 0i64,
            y: 1i64,
            width: 2u64,
            height: 3u64,
        });
        let expected = r#"{"value": {"x": 0, "y": 1, "width": 2, "height": 3}}"#;
        test(resp, expected);
    }

    #[test]
    fn test_new_session() {
        let resp = WebDriverResponse::NewSession(
            NewSessionResponse::new("test".into(),
                                    Json::Object(BTreeMap::new())));
        let expected = r#"{"value": {"sessionId": "test", "capabilities": {}}}"#;
        test(resp, expected);
    }

    #[test]
    fn test_timeouts() {
         let resp = WebDriverResponse::Timeouts(TimeoutsResponse::new(
            1, 2, 3));
        let expected = r#"{"value": {"script": 1, "pageLoad": 2, "implicit": 3}}"#;
        test(resp, expected);
    }

    #[test]
    fn test_value() {
        let mut value = BTreeMap::new();
        value.insert("example".into(), Json::Array(vec![Json::String("test".into())]));
        let resp = WebDriverResponse::Generic(ValueResponse::new(
            Json::Object(value)));
        let expected = r#"{"value": {"example": ["test"]}}"#;
        test(resp, expected);
    }
}
