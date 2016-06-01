use rustc_serialize::json;

use common::{Nullable, Date};
use cookie;
use time;
use std::collections::BTreeMap;

#[derive(Debug)]
pub enum WebDriverResponse {
    NewSession(NewSessionResponse),
    DeleteSession,
    WindowSize(WindowSizeResponse),
    ElementRect(ElementRectResponse),
    Cookie(CookieResponse),
    Generic(ValueResponse),
    Void
}

impl WebDriverResponse {
    pub fn to_json_string(self) -> String {
        (match self {
            WebDriverResponse::NewSession(x) => json::encode(&x),
            WebDriverResponse::DeleteSession => Ok("{}".to_string()),
            WebDriverResponse::WindowSize(x) => json::encode(&x),
            WebDriverResponse::ElementRect(x) => json::encode(&x),
            WebDriverResponse::Cookie(x) => json::encode(&x),
            WebDriverResponse::Generic(x) => json::encode(&x),
            WebDriverResponse::Void => Ok("{}".to_string())
        }).unwrap()
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

impl Into<cookie::Cookie> for Cookie {
    fn into(self) -> cookie::Cookie {
        cookie::Cookie {
            name: self.name,
            value: self.value,
            expires: match self.expiry {
                Nullable::Value(Date(expiry)) => {
                    Some(time::at(time::Timespec::new(expiry as i64, 0)))
                },
                Nullable::Null => None
            },
            max_age: None,
            domain: self.domain.into(),
            path: self.path.into(),
            secure: self.secure,
            httponly: self.httpOnly,
            custom: BTreeMap::new()
        }
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
