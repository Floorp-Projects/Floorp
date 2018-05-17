use common::{Date, Nullable};
use cookie;
use rustc_serialize::json::{self, Json, ToJson};
use std::collections::BTreeMap;
use time;

#[derive(Debug)]
pub enum WebDriverResponse {
    CloseWindow(CloseWindowResponse),
    Cookie(CookieResponse),
    Cookies(CookiesResponse),
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
        use response::WebDriverResponse::*;

        let obj = match self {
            CloseWindow(ref x) => json::encode(&x.to_json()),
            Cookie(ref x) => json::encode(x),
            Cookies(ref x) => json::encode(x),
            DeleteSession => Ok("null".to_string()),
            ElementRect(ref x) => json::encode(x),
            Generic(ref x) => json::encode(x),
            NewSession(ref x) => json::encode(x),
            Timeouts(ref x) => json::encode(x),
            Void => Ok("null".to_string()),
            WindowRect(ref x) => json::encode(&x.to_json()),
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

#[derive(Debug, RustcEncodable)]
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

#[derive(Debug, RustcEncodable)]
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

#[derive(Debug, RustcEncodable)]
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

#[derive(Debug, RustcEncodable)]
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

#[derive(Debug, RustcEncodable)]
pub struct ElementRectResponse {
    /// X axis position of the top-left corner of the element relative
    // to the current browsing context’s document element in CSS reference
    // pixels.
    pub x: f64,

    /// Y axis position of the top-left corner of the element relative
    // to the current browsing context’s document element in CSS reference
    // pixels.
    pub y: f64,

    /// Height of the element’s [bounding rectangle] in CSS reference
    /// pixels.
    ///
    /// [bounding rectangle]: https://drafts.fxtf.org/geometry/#rectangle
    pub width: f64,

    /// Width of the element’s [bounding rectangle] in CSS reference
    /// pixels.
    ///
    /// [bounding rectangle]: https://drafts.fxtf.org/geometry/#rectangle
    pub height: f64,
}

#[derive(Debug)]
pub struct WindowRectResponse {
    /// `WindowProxy`’s [screenX] attribute.
    ///
    /// [screenX]: https://drafts.csswg.org/cssom-view/#dom-window-screenx
    pub x: i32,

    /// `WindowProxy`’s [screenY] attribute.
    ///
    /// [screenY]: https://drafts.csswg.org/cssom-view/#dom-window-screeny
    pub y: i32,

    /// Width of the top-level browsing context’s outer dimensions, including
    /// any browser chrome and externally drawn window decorations in CSS
    /// reference pixels.
    pub width: i32,

    /// Height of the top-level browsing context’s outer dimensions, including
    /// any browser chrome and externally drawn window decorations in CSS
    /// reference pixels.
    pub height: i32,
}

impl ToJson for WindowRectResponse {
    fn to_json(&self) -> Json {
        let mut body = BTreeMap::new();
        body.insert("x".to_owned(), self.x.to_json());
        body.insert("y".to_owned(), self.y.to_json());
        body.insert("width".to_owned(), self.width.to_json());
        body.insert("height".to_owned(), self.height.to_json());
        Json::Object(body)
    }
}

#[derive(Clone, Debug, PartialEq, RustcEncodable)]
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

#[derive(Debug, RustcEncodable)]
pub struct CookieResponse {
    pub value: Cookie,
}

#[derive(Debug, RustcEncodable)]
pub struct CookiesResponse {
    pub value: Vec<Cookie>,
}

#[cfg(test)]
mod tests {
    use super::{CloseWindowResponse, Cookie, CookieResponse, CookiesResponse, ElementRectResponse,
                NewSessionResponse, Nullable, TimeoutsResponse, ValueResponse, WebDriverResponse,
                WindowRectResponse};
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
        let rect = ElementRectResponse {
            x: 0f64,
            y: 1f64,
            width: 2f64,
            height: 3f64,
        };
        let resp = WebDriverResponse::ElementRect(rect);
        let expected = r#"{"value": {"x": 0.0, "y": 1.0, "width": 2.0, "height": 3.0}}"#;
        test(resp, expected);
    }

    #[test]
    fn test_window_rect() {
        let rect = WindowRectResponse {
            x: 0i32,
            y: 1i32,
            width: 2i32,
            height: 3i32,
        };
        let resp = WebDriverResponse::WindowRect(rect);
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
