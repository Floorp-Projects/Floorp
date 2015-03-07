use rustc_serialize::json;

use common::{Nullable, Date};

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
    value: json::Json
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
    width: u64,
    height: u64
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
    x: f64,
    y: f64,
    width: f64,
    height: f64
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
#[derive(RustcEncodable, PartialEq, Debug)]
pub struct Cookie {
    name: String,
    value: String,
    path: Nullable<String>,
    domain: Nullable<String>,
    expiry: Nullable<Date>,
    maxAge: Date,
    secure: bool,
    httpOnly: bool
}

impl Cookie {
    pub fn new(name: String, value: String, path: Nullable<String>, domain: Nullable<String>,
               expiry: Nullable<Date>, max_age: Date, secure: bool, http_only: bool) -> Cookie {
        Cookie {
            name: name,
            value: value,
            path: path,
            domain: domain,
            expiry: expiry,
            maxAge: max_age,
            secure: secure,
            httpOnly: http_only
        }
    }
}

#[derive(RustcEncodable, Debug)]
pub struct CookieResponse {
    value: Vec<Cookie>
}

impl CookieResponse {
    pub fn new(value: Vec<Cookie>) -> CookieResponse {
        CookieResponse {
            value: value
        }
    }
}
