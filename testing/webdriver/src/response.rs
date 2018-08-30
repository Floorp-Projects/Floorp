use common::Cookie;
use serde::ser::{Serialize, Serializer};
use serde_json::Value;

#[derive(Debug, PartialEq, Serialize)]
#[serde(untagged, remote = "Self")]
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

impl Serialize for WebDriverResponse {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        #[derive(Serialize)]
        struct Wrapper<'a> {
            #[serde(with = "WebDriverResponse")]
            value: &'a WebDriverResponse,
        }

        Wrapper { value: self }.serialize(serializer)
    }
}

#[derive(Debug, PartialEq, Serialize)]
pub struct CloseWindowResponse(pub Vec<String>);

#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct CookieResponse(pub Cookie);

#[derive(Debug, PartialEq, Serialize)]
pub struct CookiesResponse(pub Vec<Cookie>);

#[derive(Debug, PartialEq, Serialize)]
pub struct ElementRectResponse {
    /// X axis position of the top-left corner of the element relative
    /// to the current browsing context’s document element in CSS reference
    /// pixels.
    pub x: f64,

    /// Y axis position of the top-left corner of the element relative
    /// to the current browsing context’s document element in CSS reference
    /// pixels.
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

#[derive(Debug, PartialEq, Serialize)]
pub struct NewSessionResponse {
    #[serde(rename = "sessionId")]
    pub session_id: String,
    pub capabilities: Value,
}

impl NewSessionResponse {
    pub fn new(session_id: String, capabilities: Value) -> NewSessionResponse {
        NewSessionResponse {
            capabilities,
            session_id,
        }
    }
}

#[derive(Debug, PartialEq, Serialize)]
pub struct TimeoutsResponse {
    pub script: u64,
    #[serde(rename = "pageLoad")]
    pub page_load: u64,
    pub implicit: u64,
}

impl TimeoutsResponse {
    pub fn new(script: u64, page_load: u64, implicit: u64) -> TimeoutsResponse {
        TimeoutsResponse {
            script,
            page_load,
            implicit,
        }
    }
}

#[derive(Debug, PartialEq, Serialize)]
pub struct ValueResponse(pub Value);

#[derive(Debug, PartialEq, Serialize)]
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

#[cfg(test)]
mod tests {
    use super::*;
    use common::Date;
    use serde_json;
    use test::check_serialize;

    #[test]
    fn test_json_close_window_response() {
        let json = r#"{"value":["1234"]}"#;
        let data = WebDriverResponse::CloseWindow(CloseWindowResponse(vec!["1234".into()]));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_cookie_response_with_optional() {
        let json = r#"{"value":{
            "name":"foo",
            "value":"bar",
            "path":"/",
            "domain":"foo.bar",
            "secure":true,
            "httpOnly":false,
            "expiry":123
        }}"#;
        let data = WebDriverResponse::Cookie(CookieResponse(Cookie {
            name: "foo".into(),
            value: "bar".into(),
            path: Some("/".into()),
            domain: Some("foo.bar".into()),
            expiry: Some(Date(123)),
            secure: true,
            httpOnly: false,
        }));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_cookie_response_without_optional() {
        let json = r#"{"value":{
            "name":"foo",
            "value":"bar",
            "path":"/",
            "domain":null,
            "secure":true,
            "httpOnly":false
        }}"#;
        let data = WebDriverResponse::Cookie(CookieResponse(Cookie {
            name: "foo".into(),
            value: "bar".into(),
            path: Some("/".into()),
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
        }));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_cookies_response() {
        let json = r#"{"value":[{
            "name":"name",
            "value":"value",
            "path":"/",
            "domain":null,
            "secure":true,
            "httpOnly":false
        }]}"#;
        let data = WebDriverResponse::Cookies(CookiesResponse(vec![Cookie {
            name: "name".into(),
            value: "value".into(),
            path: Some("/".into()),
            domain: None,
            expiry: None,
            secure: true,
            httpOnly: false,
        }]));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_delete_session_response() {
        let json = r#"{"value":null}"#;
        let data = WebDriverResponse::DeleteSession;

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_element_rect_response() {
        let json = r#"{"value":{"x":0.0,"y":1.0,"width":2.0,"height":3.0}}"#;
        let data = WebDriverResponse::ElementRect(ElementRectResponse {
            x: 0f64,
            y: 1f64,
            width: 2f64,
            height: 3f64,
        });

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_generic_value_response() {
        let json = r#"{"value":{"example":["test"]}}"#;
        let mut value = serde_json::Map::new();
        value.insert(
            "example".into(),
            Value::Array(vec![Value::String("test".into())]),
        );

        let data = WebDriverResponse::Generic(ValueResponse(Value::Object(value)));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_new_session_response() {
        let json = r#"{"value":{"sessionId":"id","capabilities":{}}}"#;
        let data = WebDriverResponse::NewSession(NewSessionResponse::new(
            "id".into(),
            Value::Object(serde_json::Map::new()),
        ));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_timeouts_response() {
        let json = r#"{"value":{"script":1,"pageLoad":2,"implicit":3}}"#;
        let data = WebDriverResponse::Timeouts(TimeoutsResponse::new(1, 2, 3));

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_void_response() {
        let json = r#"{"value":null}"#;
        let data = WebDriverResponse::Void;

        check_serialize(&json, &data);
    }

    #[test]
    fn test_json_window_rect_response() {
        let json = r#"{"value":{"x":0,"y":1,"width":2,"height":3}}"#;
        let data = WebDriverResponse::WindowRect(WindowRectResponse {
            x: 0i32,
            y: 1i32,
            width: 2i32,
            height: 3i32,
        });

        check_serialize(&json, &data);
    }
}
