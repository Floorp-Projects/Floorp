use crate::common::Cookie;
use serde::ser::{Serialize, Serializer};
use serde_json::Value;

#[derive(Debug, PartialEq, Serialize)]
#[serde(untagged, remote = "Self")]
pub enum WebDriverResponse {
    NewWindow(NewWindowResponse),
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
pub struct NewWindowResponse {
    pub handle: String,
    #[serde(rename = "type")]
    pub typ: String,
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
    pub script: Option<u64>,
    #[serde(rename = "pageLoad")]
    pub page_load: u64,
    pub implicit: u64,
}

impl TimeoutsResponse {
    pub fn new(script: Option<u64>, page_load: u64, implicit: u64) -> TimeoutsResponse {
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
    use serde_json::{json, Map};

    use super::*;
    use crate::common::Date;
    use crate::test::assert_ser;

    #[test]
    fn test_json_new_window_response() {
        let json = json!({"value": {"handle": "42", "type": "window"}});
        let response = WebDriverResponse::NewWindow(NewWindowResponse {
            handle: "42".into(),
            typ: "window".into(),
        });

        assert_ser(&response, json);
    }

    #[test]
    fn test_json_close_window_response() {
        assert_ser(
            &WebDriverResponse::CloseWindow(CloseWindowResponse(vec!["1234".into()])),
            json!({"value": ["1234"]}),
        );
    }

    #[test]
    fn test_json_cookie_response_with_optional() {
        let json = json!({"value": {
            "name": "foo",
            "value": "bar",
            "path": "/",
            "domain": "foo.bar",
            "secure": true,
            "httpOnly": false,
            "expiry": 123,
            "sameSite": "Strict",
        }});
        let response = WebDriverResponse::Cookie(CookieResponse(Cookie {
            name: "foo".into(),
            value: "bar".into(),
            path: Some("/".into()),
            domain: Some("foo.bar".into()),
            expiry: Some(Date(123)),
            secure: true,
            http_only: false,
            same_site: Some("Strict".into()),
        }));

        assert_ser(&response, json);
    }

    #[test]
    fn test_json_cookie_response_without_optional() {
        let json = json!({"value": {
            "name": "foo",
            "value": "bar",
            "path": "/",
            "domain": null,
            "secure": true,
            "httpOnly": false,
        }});
        let response = WebDriverResponse::Cookie(CookieResponse(Cookie {
            name: "foo".into(),
            value: "bar".into(),
            path: Some("/".into()),
            domain: None,
            expiry: None,
            secure: true,
            http_only: false,
            same_site: None,
        }));

        assert_ser(&response, json);
    }

    #[test]
    fn test_json_cookies_response() {
        let json = json!({"value": [{
            "name": "name",
            "value": "value",
            "path": "/",
            "domain": null,
            "secure": true,
            "httpOnly": false,
            "sameSite": "None",
        }]});
        let response = WebDriverResponse::Cookies(CookiesResponse(vec![Cookie {
            name: "name".into(),
            value: "value".into(),
            path: Some("/".into()),
            domain: None,
            expiry: None,
            secure: true,
            http_only: false,
            same_site: Some("None".into()),
        }]));

        assert_ser(&response, json);
    }

    #[test]
    fn test_json_delete_session_response() {
        assert_ser(&WebDriverResponse::DeleteSession, json!({ "value": null }));
    }

    #[test]
    fn test_json_element_rect_response() {
        let json = json!({"value": {
            "x": 0.0,
            "y": 1.0,
            "width": 2.0,
            "height": 3.0,
        }});
        let response = WebDriverResponse::ElementRect(ElementRectResponse {
            x: 0f64,
            y: 1f64,
            width: 2f64,
            height: 3f64,
        });

        assert_ser(&response, json);
    }

    #[test]
    fn test_json_generic_value_response() {
        let response = {
            let mut value = Map::new();
            value.insert(
                "example".into(),
                Value::Array(vec![Value::String("test".into())]),
            );
            WebDriverResponse::Generic(ValueResponse(Value::Object(value)))
        };
        assert_ser(&response, json!({"value": {"example": ["test"]}}));
    }

    #[test]
    fn test_json_new_session_response() {
        let response =
            WebDriverResponse::NewSession(NewSessionResponse::new("id".into(), json!({})));
        assert_ser(
            &response,
            json!({"value": {"sessionId": "id", "capabilities": {}}}),
        );
    }

    #[test]
    fn test_json_timeouts_response() {
        assert_ser(
            &WebDriverResponse::Timeouts(TimeoutsResponse::new(Some(1), 2, 3)),
            json!({"value": {"script": 1, "pageLoad": 2, "implicit": 3}}),
        );
    }

    #[test]
    fn test_json_timeouts_response_with_null_script_timeout() {
        assert_ser(
            &WebDriverResponse::Timeouts(TimeoutsResponse::new(None, 2, 3)),
            json!({"value": {"script": null, "pageLoad": 2, "implicit": 3}}),
        );
    }

    #[test]
    fn test_json_void_response() {
        assert_ser(&WebDriverResponse::Void, json!({ "value": null }));
    }

    #[test]
    fn test_json_window_rect_response() {
        let json = json!({"value": {
            "x": 0,
            "y": 1,
            "width": 2,
            "height": 3,
        }});
        let response = WebDriverResponse::WindowRect(WindowRectResponse {
            x: 0i32,
            y: 1i32,
            width: 2i32,
            height: 3i32,
        });

        assert_ser(&response, json);
    }
}
