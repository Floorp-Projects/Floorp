use rustc_serialize::json::{self, Json, ToJson};

use common::{Nullable, Date};
use cookie;
use time;

#[derive(Debug)]
pub enum WebDriverResponse {
    CloseWindow(CloseWindowResponse),
    Cookie(CookieResponse),
    Cookies(CookiesResponse),
    DeleteSession,
    ElementRect(RectResponse),
    Generic(ValueResponse),
    NewSession(NewSessionResponse),
    Timeouts(TimeoutsResponse),
    Void,
    WindowRect(RectResponse),
}

impl WebDriverResponse {
    pub fn to_json_string(self) -> String {
        use response::WebDriverResponse::*;

        let obj = match self {
            CloseWindow(ref x) => json::encode(&x.to_json()),
            Cookie(ref x) => json::encode(x),
            Cookies(ref x) => json::encode(x),
            DeleteSession => Ok("{}".to_string()),
            ElementRect(ref x) => json::encode(x),
            Generic(ref x) => json::encode(x),
            NewSession(ref x) => json::encode(x),
            Timeouts(ref x) => json::encode(x),
            Void => Ok("{}".to_string()),
            WindowRect(ref x) => json::encode(x),
        }.unwrap();

        match self {
            Generic(_) | Cookie(_) | Cookies(_) => obj,
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
pub struct RectResponse {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64
}

impl RectResponse {
    pub fn new(x: f64, y: f64, width: f64, height: f64) -> RectResponse {
        RectResponse {
            x: x,
            y: y,
            width: width,
            height: height
        }
    }
}

#[derive(RustcEncodable, PartialEq, Debug, Clone)]
pub struct Cookie {
    pub name: String,
    pub value: String,
    pub path: Nullable<String>,
    pub domain: Nullable<String>,
    pub expiry: Nullable<Date>,
    pub secure: bool,
    pub httpOnly: bool,
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
            }
            Nullable::Null => cookie,
        };
        cookie.finish()
    }
}

#[derive(RustcEncodable, Debug)]
pub struct CookieResponse {
    pub value: Cookie,
}

#[derive(RustcEncodable, Debug)]
pub struct CookiesResponse {
    pub value: Vec<Cookie>,
}

#[cfg(test)]
mod tests {
    use super::{CloseWindowResponse, Cookie, CookieResponse, CookiesResponse, NewSessionResponse,
                Nullable, RectResponse, TimeoutsResponse, ValueResponse, WebDriverResponse};
    use rustc_serialize::json::Json;
    use std::collections::BTreeMap;

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
        let cookie = Cookie {
            name: "name".into(),
            value: "value".into(),
            path: Nullable::Value("/".into()),
            domain: Nullable::Null,
            expiry: Nullable::Null,
            secure: true,
            httpOnly: false,
        };
        let resp = WebDriverResponse::Cookie(CookieResponse { value: cookie });
        let expected = r#"{"value": {"name": "name", "expiry": null, "value": "value",
"path": "/", "domain": null, "secure": true, "httpOnly": false}}"#;
        test(resp, expected);
    }

    #[test]
    fn test_cookies() {
        let resp = WebDriverResponse::Cookies(CookiesResponse {
            value: vec![
                Cookie {
                    name: "name".into(),
                    value: "value".into(),
                    path: Nullable::Value("/".into()),
                    domain: Nullable::Null,
                    expiry: Nullable::Null,
                    secure: true,
                    httpOnly: false,
                }
            ]});
        let expected = r#"{"value": [{"name": "name", "value": "value", "path": "/",
"domain": null, "expiry": null, "secure": true, "httpOnly": false}]}"#;
        test(resp, expected);
    }

    #[test]
    fn test_element_rect() {
        let resp = WebDriverResponse::ElementRect(RectResponse::new(
            0f64, 1f64, 2f64, 3f64));
        let expected = r#"{"value": {"x": 0.0, "y": 1.0, "width": 2.0, "height": 3.0}}"#;
        test(resp, expected);
    }

    #[test]
    fn test_window_rect() {
        let resp = WebDriverResponse::WindowRect(RectResponse {
            x: 0f64,
            y: 1f64,
            width: 2f64,
            height: 3f64,
        });
        let expected = r#"{"value": {"x": 0.0, "y": 1.0, "width": 2.0, "height": 3.0}}"#;
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
