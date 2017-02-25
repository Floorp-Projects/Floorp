use rustc_serialize::json;

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
    WindowPosition(WindowPositionResponse),
    WindowSize(WindowSizeResponse),
}

impl WebDriverResponse {
    pub fn to_json_string(self) -> String {
        let obj = match self {
            WebDriverResponse::CloseWindow(ref x) => json::encode(x),
            WebDriverResponse::Cookie(ref x) => json::encode(x),
            WebDriverResponse::DeleteSession => Ok("{}".to_string()),
            WebDriverResponse::ElementRect(ref x) => json::encode(x),
            WebDriverResponse::Generic(ref x) => json::encode(x),
            WebDriverResponse::NewSession(ref x) => json::encode(x),
            WebDriverResponse::Timeouts(ref x) => json::encode(x),
            WebDriverResponse::Void => Ok("{}".to_string()),
            WebDriverResponse::WindowPosition(ref x) => json::encode(x),
            WebDriverResponse::WindowSize(ref x) => json::encode(x),
        }.unwrap();

        match self {
            WebDriverResponse::Generic(_) => obj,
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

#[derive(RustcEncodable, Debug)]
pub struct NewSessionResponse {
    pub sessionId: String,
    pub value: json::Json
}

impl NewSessionResponse {
    pub fn new(session_id: String, value: json::Json) -> NewSessionResponse {
        NewSessionResponse {
            value: value,
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
pub struct WindowSizeResponse {
    pub width: u64,
    pub height: u64
}

impl WindowSizeResponse {
    pub fn new(width: u64, height: u64) -> WindowSizeResponse {
        WindowSizeResponse {
            width: width,
            height: height
        }
    }
}

#[derive(RustcEncodable, Debug)]
pub struct WindowPositionResponse {
    pub x: i64,
    pub y: i64,
}

impl WindowPositionResponse {
    pub fn new(x: i64, y: i64) -> WindowPositionResponse {
        WindowPositionResponse { x: x, y: y }
    }
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
